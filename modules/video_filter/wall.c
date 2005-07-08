/*****************************************************************************
 * wall.c : Wall video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001, 2002, 2003 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include "filter_common.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );

static void RemoveAllVout  ( vout_thread_t *p_vout );

static int  SendEvents( vlc_object_t *, char const *,
                        vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define COLS_TEXT N_("Number of columns")
#define COLS_LONGTEXT N_("Select the number of horizontal video windows in " \
    "which to split the video.")

#define ROWS_TEXT N_("Number of rows")
#define ROWS_LONGTEXT N_("Select the number of vertical video windows in " \
    "which to split the video.")

#define ACTIVE_TEXT N_("Active windows")
#define ACTIVE_LONGTEXT N_("Comma separated list of active windows, " \
    "defaults to all")

#define ASPECT_TEXT N_("Element aspect ratio")
#define ASPECT_LONGTEXT N_("The aspect ratio of the individual displays building the display wall")

vlc_module_begin();
    set_description( _("Wall video filter") );
    set_shortname( N_("Image wall" ));
    set_capability( "video filter", 0 );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER );

    add_integer( "wall-cols", 3, NULL, COLS_TEXT, COLS_LONGTEXT, VLC_FALSE );
    add_integer( "wall-rows", 3, NULL, ROWS_TEXT, ROWS_LONGTEXT, VLC_FALSE );
    add_string( "wall-active", NULL, NULL, ACTIVE_TEXT, ACTIVE_LONGTEXT, VLC_FALSE );
    add_string( "wall-element-aspect", "4:3", NULL, ASPECT_TEXT, ASPECT_LONGTEXT, VLC_FALSE );

    add_shortcut( "wall" );
    set_callbacks( Create, Destroy );
vlc_module_end();

/*****************************************************************************
 * vout_sys_t: Wall video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Wall specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    int    i_col;
    int    i_row;
    int    i_vout;
    struct vout_list_t
    {
        vlc_bool_t b_active;
        int i_width;
        int i_height;
        int i_left;
        int i_top;
        vout_thread_t *p_vout;
    } *pp_vout;
};

/*****************************************************************************
 * Control: control facility for the vout (forwards to child vout)
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    int i_row, i_col, i_vout = 0;

    for( i_row = 0; i_row < p_vout->p_sys->i_row; i_row++ )
    {
        for( i_col = 0; i_col < p_vout->p_sys->i_col; i_col++ )
        {
            vout_vaControl( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                            i_query, args );
            i_vout++;
        }
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Create: allocates Wall video thread output method
 *****************************************************************************
 * This function allocates and initializes a Wall vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    char *psz_method, *psz_tmp, *psz_method_tmp;
    int i_vout;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return VLC_ENOMEM;
    }

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = Render;
    p_vout->pf_display = NULL;
    p_vout->pf_control = Control;

    /* Look what method was requested */
    p_vout->p_sys->i_col = config_GetInt( p_vout, "wall-cols" );
    p_vout->p_sys->i_row = config_GetInt( p_vout, "wall-rows" );

    p_vout->p_sys->i_col = __MAX( 1, __MIN( 15, p_vout->p_sys->i_col ) );
    p_vout->p_sys->i_row = __MAX( 1, __MIN( 15, p_vout->p_sys->i_row ) );

    msg_Dbg( p_vout, "opening a %i x %i wall",
             p_vout->p_sys->i_col, p_vout->p_sys->i_row );

    p_vout->p_sys->pp_vout = malloc( p_vout->p_sys->i_row *
                                     p_vout->p_sys->i_col *
                                     sizeof(struct vout_list_t) );
    if( p_vout->p_sys->pp_vout == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        free( p_vout->p_sys );
        return VLC_ENOMEM;
    }

    psz_method_tmp = psz_method = config_GetPsz( p_vout, "wall-active" );

    /* If no trailing vout are specified, take them all */
    if( psz_method == NULL )
    {
        for( i_vout = p_vout->p_sys->i_row * p_vout->p_sys->i_col;
             i_vout--; )
        {
            p_vout->p_sys->pp_vout[i_vout].b_active = 1;
        }
    }
    /* If trailing vout are specified, activate only the requested ones */
    else
    {
        for( i_vout = p_vout->p_sys->i_row * p_vout->p_sys->i_col;
             i_vout--; )
        {
            p_vout->p_sys->pp_vout[i_vout].b_active = 0;
        }

        while( *psz_method )
        {
            psz_tmp = psz_method;
            while( *psz_tmp && *psz_tmp != ',' )
            {
                psz_tmp++;
            }

            if( *psz_tmp )
            {
                *psz_tmp = '\0';
                i_vout = atoi( psz_method );
                psz_method = psz_tmp + 1;
            }
            else
            {
                i_vout = atoi( psz_method );
                psz_method = psz_tmp;
            }

            if( i_vout >= 0 &&
                i_vout < p_vout->p_sys->i_row * p_vout->p_sys->i_col )
            {
                p_vout->p_sys->pp_vout[i_vout].b_active = 1;
            }
        }
    }

    free( psz_method_tmp );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize Wall video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int i_index, i_row, i_col, i_width, i_height, i_left, i_top;
    unsigned int i_target_width,i_target_height;
    picture_t *p_pic;
    video_format_t fmt = {0};
    int i_aspect = 4*VOUT_ASPECT_FACTOR/3;
    int i_align = 0;
    unsigned int i_hstart, i_hend, i_vstart, i_vend;
    unsigned int w1,h1,w2,h2;
    int i_xpos, i_ypos;
    int i_vstart_rounded = 0, i_hstart_rounded = 0;
    char *psz_aspect;

    psz_aspect = config_GetPsz( p_vout, "wall-element-aspect" );
    if( psz_aspect && *psz_aspect )
    {
        char *psz_parser = strchr( psz_aspect, ':' );
        if( psz_parser )
        {
            *psz_parser++ = '\0';
            i_aspect = atoi( psz_aspect ) * VOUT_ASPECT_FACTOR
                / atoi( psz_parser );
        }
        else
        {
            msg_Warn( p_vout, "invalid aspect ratio specification" );
        }
        free( psz_aspect );
    }
    

    i_xpos = var_CreateGetInteger( p_vout, "video-x" );
    i_ypos = var_CreateGetInteger( p_vout, "video-y" );
    if( i_xpos < 0 ) i_xpos = 0;
    if( i_ypos < 0 ) i_ypos = 0;

    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure */
    p_vout->output.i_chroma = p_vout->render.i_chroma;
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;
    var_Create( p_vout, "align", VLC_VAR_INTEGER );

    fmt.i_width = fmt.i_visible_width = p_vout->render.i_width;
    fmt.i_height = fmt.i_visible_height = p_vout->render.i_height;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    fmt.i_chroma = p_vout->render.i_chroma;
    fmt.i_aspect = p_vout->render.i_aspect;
    fmt.i_sar_num = p_vout->render.i_aspect * fmt.i_height / fmt.i_width;
    fmt.i_sar_den = VOUT_ASPECT_FACTOR;

    w1 = p_vout->output.i_width / p_vout->p_sys->i_col;
    w1 &= ~1;
    h1 = w1 * VOUT_ASPECT_FACTOR / i_aspect&~1;
    h1 &= ~1;
    
    h2 = p_vout->output.i_height / p_vout->p_sys->i_row&~1;
    h2 &= ~1;
    w2 = h2 * i_aspect / VOUT_ASPECT_FACTOR&~1;
    w2 &= ~1;
    
    if ( h1 * p_vout->p_sys->i_row < p_vout->output.i_height )
    {
        unsigned int i_tmp;
        i_target_width = w2;        
        i_target_height = h2;
        i_vstart = 0;
        i_vend = p_vout->output.i_height;
        i_tmp = i_target_width * p_vout->p_sys->i_col;
        while( i_tmp < p_vout->output.i_width ) i_tmp += p_vout->p_sys->i_col;
        i_hstart = (( i_tmp - p_vout->output.i_width ) / 2)&~1;
        i_hstart_rounded  = ( ( i_tmp - p_vout->output.i_width ) % 2 ) ||
            ( ( ( i_tmp - p_vout->output.i_width ) / 2 ) & 1 );
        i_hend = i_hstart + p_vout->output.i_width;
    }
    else
    {
        unsigned int i_tmp;
        i_target_height = h1;
        i_target_width = w1;
        i_hstart = 0;
        i_hend = p_vout->output.i_width;
        i_tmp = i_target_height * p_vout->p_sys->i_row;
        while( i_tmp < p_vout->output.i_height ) i_tmp += p_vout->p_sys->i_row;
        i_vstart = ( ( i_tmp - p_vout->output.i_height ) / 2 ) & ~1;
        i_vstart_rounded  = ( ( i_tmp - p_vout->output.i_height ) % 2 ) ||
            ( ( ( i_tmp - p_vout->output.i_height ) / 2 ) & 1 );
        i_vend = i_vstart + p_vout->output.i_height;
    }
    msg_Dbg( p_vout, "target resolution %dx%d", i_target_width, i_target_height );

    /* Try to open the real video output */
    msg_Dbg( p_vout, "spawning the real video outputs" );

    p_vout->p_sys->i_vout = 0;
    msg_Dbg( p_vout, "target window (%d,%d)-(%d,%d)", i_hstart,i_vstart,i_hend,i_vend );
    

    i_top = 0;
    i_height = 0;
    for( i_row = 0; i_row < p_vout->p_sys->i_row; i_row++ )
    {
        i_left = 0;
        i_top += i_height;
        for( i_col = 0; i_col < p_vout->p_sys->i_col; i_col++ )
        {
            i_align = 0;

            if( i_col*i_target_width >= i_hstart &&
                (i_col+1)*i_target_width <= i_hend )
            {
                i_width = i_target_width;
            }
            else if( ( i_col + 1 ) * i_target_width < i_hstart ||
                     ( i_col * i_target_width ) > i_hend )
            {
                i_width = 0;                
            }
            else
            {
                i_width = ( i_target_width - i_hstart % i_target_width );
                if( i_col >= ( p_vout->p_sys->i_col / 2 ) )
                {
                    i_align |= VOUT_ALIGN_LEFT;
                    i_width -= i_hstart_rounded ? 2: 0;
                }
                else
                {
                    i_align |= VOUT_ALIGN_RIGHT;
                }
            }
            
            if( i_row * i_target_height >= i_vstart &&
                ( i_row + 1 ) * i_target_height <= i_vend )
            {
                i_height = i_target_height;
            }
            else if( ( i_row + 1 ) * i_target_height < i_vstart ||
                     ( i_row * i_target_height ) > i_vend )
            {
                i_height = 0;
            }
            else
            {
                i_height = ( i_target_height -
                             i_vstart%i_target_height );
                if(  i_row >= ( p_vout->p_sys->i_row / 2 ) )
                {
                    i_align |= VOUT_ALIGN_TOP;
                    i_height -= i_vstart_rounded ? 2: 0;
                }
                else
                {
                    i_align |= VOUT_ALIGN_BOTTOM;
                }
            }
            if( i_height == 0 || i_width == 0 )
            {
                p_vout->p_sys->pp_vout[ p_vout->p_sys->i_vout ].b_active = VLC_FALSE;
            }

            p_vout->p_sys->pp_vout[ p_vout->p_sys->i_vout ].i_width = i_width;
            p_vout->p_sys->pp_vout[ p_vout->p_sys->i_vout ].i_height = i_height;
            p_vout->p_sys->pp_vout[ p_vout->p_sys->i_vout ].i_left = i_left;
            p_vout->p_sys->pp_vout[ p_vout->p_sys->i_vout ].i_top = i_top;
            i_left += i_width;

            if( !p_vout->p_sys->pp_vout[ p_vout->p_sys->i_vout ].b_active )
            {
                p_vout->p_sys->i_vout++;
                continue;
            }
            var_SetInteger( p_vout, "align", i_align );
            var_SetInteger( p_vout, "video-x", i_left + i_xpos - i_width);
            var_SetInteger( p_vout, "video-y", i_top + i_ypos );

            fmt.i_width = fmt.i_visible_width = i_width;
            fmt.i_height = fmt.i_visible_height = i_height;
            fmt.i_aspect = i_aspect * i_target_height / i_height *
                i_width / i_target_width;

            p_vout->p_sys->pp_vout[ p_vout->p_sys->i_vout ].p_vout =
                vout_Create( p_vout, &fmt );
            if( !p_vout->p_sys->pp_vout[ p_vout->p_sys->i_vout ].p_vout )
            {
                msg_Err( p_vout, "failed to get %ix%i vout threads",
                                 p_vout->p_sys->i_col, p_vout->p_sys->i_row );
                RemoveAllVout( p_vout );
                return VLC_EGENERIC;
            }
            ADD_CALLBACKS(
                p_vout->p_sys->pp_vout[ p_vout->p_sys->i_vout ].p_vout,
                SendEvents );
            p_vout->p_sys->i_vout++;
        }
    }

    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );

    ADD_PARENT_CALLBACKS( SendEventsToChild );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate Wall video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    int i_index;

    /* Free the fake output buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        free( PP_OUTPUTPICTURE[ i_index ]->p_data_orig );
    }
}

/*****************************************************************************
 * Destroy: destroy Wall video thread output method
 *****************************************************************************
 * Terminate an output method created by WallCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    RemoveAllVout( p_vout );

    DEL_PARENT_CALLBACKS( SendEventsToChild );

    free( p_vout->p_sys->pp_vout );
    free( p_vout->p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Wall image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *p_outpic = NULL;
    int i_col, i_row, i_vout, i_plane;
    int pi_left_skip[VOUT_MAX_PLANES], pi_top_skip[VOUT_MAX_PLANES];

    i_vout = 0;

    for( i_row = 0; i_row < p_vout->p_sys->i_row; i_row++ )
    {
        for( i_col = 0; i_col < p_vout->p_sys->i_col; i_col++ )
        {
            for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
            {
                pi_left_skip[i_plane] = p_vout->p_sys->pp_vout[ i_vout ].i_left * p_pic->p[ i_plane ].i_pitch / p_vout->output.i_width;
                pi_top_skip[i_plane] = (p_vout->p_sys->pp_vout[ i_vout ].i_top * p_pic->p[ i_plane ].i_lines / p_vout->output.i_height)*p_pic->p[i_plane].i_pitch;
            }

            if( !p_vout->p_sys->pp_vout[ i_vout ].b_active )
            {
                i_vout++;
                continue;
            }

            while( ( p_outpic =
                vout_CreatePicture( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                                    0, 0, 0 )
                   ) == NULL )
            {
                if( p_vout->b_die || p_vout->b_error )
                {
                    vout_DestroyPicture(
                        p_vout->p_sys->pp_vout[ i_vout ].p_vout, p_outpic );
                    return;
                }

                msleep( VOUT_OUTMEM_SLEEP );
            }

            vout_DatePicture( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                              p_outpic, p_pic->date );
            vout_LinkPicture( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                              p_outpic );

            for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
            {
                uint8_t *p_in, *p_in_end, *p_out;
                int i_in_pitch = p_pic->p[i_plane].i_pitch;
                int i_out_pitch = p_outpic->p[i_plane].i_pitch;
                int i_copy_pitch = p_outpic->p[i_plane].i_visible_pitch;

                p_in = p_pic->p[i_plane].p_pixels
                        + pi_top_skip[i_plane] + pi_left_skip[i_plane];

                p_in_end = p_in + p_outpic->p[i_plane].i_visible_lines
                                   * p_pic->p[i_plane].i_pitch;

                p_out = p_outpic->p[i_plane].p_pixels;

                while( p_in < p_in_end )
                {
                    p_vout->p_vlc->pf_memcpy( p_out, p_in, i_copy_pitch );
                    p_in += i_in_pitch;
                    p_out += i_out_pitch;
                }

//                pi_left_skip[i_plane] += i_copy_pitch;
            }

            vout_UnlinkPicture( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                                p_outpic );
            vout_DisplayPicture( p_vout->p_sys->pp_vout[ i_vout ].p_vout,
                                 p_outpic );

            i_vout++;
        }

/*         for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ ) */
/*         { */
/*             pi_top_skip[i_plane] += p_vout->p_sys->pp_vout[ i_vout ].i_height */
/*                                      * p_pic->p[i_plane].i_visible_lines */
/*                                      / p_vout->output.i_height */
/*                                      * p_pic->p[i_plane].i_pitch; */
/*         } */
    }
}

/*****************************************************************************
 * RemoveAllVout: destroy all the child video output threads
 *****************************************************************************/
static void RemoveAllVout( vout_thread_t *p_vout )
{
    while( p_vout->p_sys->i_vout )
    {
         --p_vout->p_sys->i_vout;
         if( p_vout->p_sys->pp_vout[ p_vout->p_sys->i_vout ].b_active )
         {
             DEL_CALLBACKS(
                 p_vout->p_sys->pp_vout[ p_vout->p_sys->i_vout ].p_vout,
                 SendEvents );
             vlc_object_detach(
                 p_vout->p_sys->pp_vout[ p_vout->p_sys->i_vout ].p_vout );
             vout_Destroy(
                 p_vout->p_sys->pp_vout[ p_vout->p_sys->i_vout ].p_vout );
         }
    }
}

/*****************************************************************************
 * SendEvents: forward mouse and keyboard events to the parent p_vout
 *****************************************************************************/
static int SendEvents( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *_p_vout )
{
    vout_thread_t *p_vout = (vout_thread_t *)_p_vout;
    int i_vout;
    vlc_value_t sentval = newval;

    /* Find the video output index */
    for( i_vout = 0; i_vout < p_vout->p_sys->i_vout; i_vout++ )
    {
        if( p_this == (vlc_object_t *)p_vout->p_sys->pp_vout[ i_vout ].p_vout )
        {
            break;
        }
    }

    if( i_vout == p_vout->p_sys->i_vout )
    {
        return VLC_EGENERIC;
    }

    /* Translate the mouse coordinates */
    if( !strcmp( psz_var, "mouse-x" ) )
    {
        sentval.i_int += p_vout->output.i_width
                          * (i_vout % p_vout->p_sys->i_col)
                          / p_vout->p_sys->i_col;
    }
    else if( !strcmp( psz_var, "mouse-y" ) )
    {
        sentval.i_int += p_vout->output.i_height
                          * (i_vout / p_vout->p_sys->i_row)
                          / p_vout->p_sys->i_row;
    }

    var_Set( p_vout, psz_var, sentval );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SendEventsToChild: forward events to the child/children vout
 *****************************************************************************/
static int SendEventsToChild( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    int i_row, i_col, i_vout = 0;

    for( i_row = 0; i_row < p_vout->p_sys->i_row; i_row++ )
    {
        for( i_col = 0; i_col < p_vout->p_sys->i_col; i_col++ )
        {
            var_Set( p_vout->p_sys->pp_vout[ i_vout ].p_vout, psz_var, newval);
            if( !strcmp( psz_var, "fullscreen" ) ) break;
            i_vout++;
        }
    }

    return VLC_SUCCESS;
}
