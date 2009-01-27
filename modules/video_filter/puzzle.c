/*****************************************************************************
 * puzzle.c : Puzzle game
 *****************************************************************************
 * Copyright (C) 2005-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>

#include <math.h>

#include "filter_common.h"
#include "vlc_image.h"
#include "vlc_input.h"
#include "vlc_playlist.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );

static int  SendEvents   ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  MouseEvent   ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );

static int PuzzleCallback( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define ROWS_TEXT N_("Number of puzzle rows")
#define ROWS_LONGTEXT N_("Number of puzzle rows")
#define COLS_TEXT N_("Number of puzzle columns")
#define COLS_LONGTEXT N_("Number of puzzle columns")
#define BLACKSLOT_TEXT N_("Make one tile a black slot")
#define BLACKSLOT_LONGTEXT N_("Make one slot black. Other tiles can only be swapped with the black slot.")

#define CFG_PREFIX "puzzle-"

vlc_module_begin ()
    set_description( N_("Puzzle interactive game video filter") )
    set_shortname( N_( "Puzzle" ))
    set_capability( "video filter", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    add_integer_with_range( CFG_PREFIX "rows", 4, 1, 128, NULL,
                            ROWS_TEXT, ROWS_LONGTEXT, false )
    add_integer_with_range( CFG_PREFIX "cols", 4, 1, 128, NULL,
                            COLS_TEXT, COLS_LONGTEXT, false )
    add_bool( CFG_PREFIX "black-slot", 0, NULL,
              BLACKSLOT_TEXT, BLACKSLOT_LONGTEXT, false )

    set_callbacks( Create, Destroy )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "rows", "cols", "black-slot", NULL
};

/*****************************************************************************
 * vout_sys_t: Magnify video output method descriptor
 *****************************************************************************/
struct vout_sys_t
{
    vout_thread_t *p_vout;

    image_handler_t *p_image;

    int i_cols;
    int i_rows;
    int *pi_order;
    int i_selected;
    bool b_finished;

    bool b_blackslot;
};

/*****************************************************************************
 * Control: control facility for the vout (forwards to child vout)
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    return vout_vaControl( p_vout->p_sys->p_vout, i_query, args );
}

/*****************************************************************************
 * Misc stuff...
 *****************************************************************************/
static bool finished( vout_sys_t *p_sys )
{
    int i;
    for( i = 0; i < p_sys->i_cols * p_sys->i_rows; i++ )
    {
        if( i != p_sys->pi_order[i] ) return false;
    }
    return true;
}
static bool is_valid( vout_sys_t *p_sys )
{
    int i, j, d=0;
    if( p_sys->b_blackslot == false ) return true;
    for( i = 0; i < p_sys->i_cols * p_sys->i_rows; i++ )
    {
        if( p_sys->pi_order[i] == p_sys->i_cols * p_sys->i_rows - 1 )
        {
            d += i / p_sys->i_cols + 1;
            continue;
        }
        for( j = i+1; j < p_sys->i_cols * p_sys->i_rows; j++ )
        {
            if( p_sys->pi_order[j] == p_sys->i_cols * p_sys->i_rows - 1 )
                continue;
            if( p_sys->pi_order[i] > p_sys->pi_order[j] ) d++;
        }
    }
    if( d%2!=0 ) return false;
    else return true;
}
static void shuffle( vout_sys_t *p_sys )
{
    int i, c;
    free( p_sys->pi_order );
    p_sys->pi_order = malloc( p_sys->i_cols * p_sys->i_rows * sizeof( int ) );
    do
    {
        for( i = 0; i < p_sys->i_cols * p_sys->i_rows; i++ )
        {
            p_sys->pi_order[i] = -1;
        }
        i = 0;
        for( c = 0; c < p_sys->i_cols * p_sys->i_rows; )
        {
            i = rand()%( p_sys->i_cols * p_sys->i_rows );
            if( p_sys->pi_order[i] == -1 )
            {
                p_sys->pi_order[i] = c;
                c++;
            }
        }
        p_sys->b_finished = finished( p_sys );
    } while(    p_sys->b_finished == true
             || is_valid( p_sys ) == false );

    if( p_sys->b_blackslot == true )
    {
        for( i = 0; i < p_sys->i_cols * p_sys->i_rows; i++ )
        {
            if( p_sys->pi_order[i] ==
                p_sys->i_cols * p_sys->i_rows - 1 )
            {
                p_sys->i_selected = i;
                break;
            }
        }
    }
    else
    {
        p_sys->i_selected = -1;
    }
}

/*****************************************************************************
 * Create: allocates Magnify video thread output method
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
        return VLC_ENOMEM;

    p_vout->p_sys->p_image = image_HandlerCreate( p_vout );

    config_ChainParse( p_vout, CFG_PREFIX, ppsz_filter_options,
                       p_vout->p_cfg );

    p_vout->p_sys->i_rows =
        var_CreateGetIntegerCommand( p_vout, CFG_PREFIX "rows" );
    p_vout->p_sys->i_cols =
        var_CreateGetIntegerCommand( p_vout, CFG_PREFIX "cols" );
    p_vout->p_sys->b_blackslot =
        var_CreateGetBoolCommand( p_vout, CFG_PREFIX "black-slot" );
    var_AddCallback( p_vout, CFG_PREFIX "rows",
                     PuzzleCallback, p_vout->p_sys );
    var_AddCallback( p_vout, CFG_PREFIX "cols",
                     PuzzleCallback, p_vout->p_sys );
    var_AddCallback( p_vout, CFG_PREFIX "black-slot",
                     PuzzleCallback, p_vout->p_sys );

    p_vout->p_sys->pi_order = NULL;
    shuffle( p_vout->p_sys );

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = Render;
    p_vout->pf_display = NULL;
    p_vout->pf_control = Control;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize Magnify video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;
    video_format_t fmt;
    memset( &fmt, 0, sizeof( video_format_t ) );

    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure */
    p_vout->output.i_chroma = p_vout->render.i_chroma;
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    p_vout->fmt_out = p_vout->fmt_in;
    fmt = p_vout->fmt_out;

    /* Try to open the real video output */
    msg_Dbg( p_vout, "spawning the real video output" );

    p_vout->p_sys->p_vout = vout_Create( p_vout, &fmt );

    /* Everything failed */
    if( p_vout->p_sys->p_vout == NULL )
    {
        msg_Err( p_vout, "cannot open vout, aborting" );
        return VLC_EGENERIC;
    }

    var_AddCallback( p_vout->p_sys->p_vout, "mouse-x", MouseEvent, p_vout );
    var_AddCallback( p_vout->p_sys->p_vout, "mouse-y", MouseEvent, p_vout );
    var_AddCallback( p_vout->p_sys->p_vout, "mouse-clicked",
                     MouseEvent, p_vout);

    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );
    ADD_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );
    ADD_PARENT_CALLBACKS( SendEventsToChild );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate Magnify video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    int i_index;

    DEL_PARENT_CALLBACKS( SendEventsToChild );

    DEL_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );

    /* Free the fake output buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        free( PP_OUTPUTPICTURE[ i_index ]->p_data_orig );
    }

    var_DelCallback( p_vout->p_sys->p_vout, "mouse-x", MouseEvent, p_vout);
    var_DelCallback( p_vout->p_sys->p_vout, "mouse-y", MouseEvent, p_vout);
    var_DelCallback( p_vout->p_sys->p_vout, "mouse-clicked", MouseEvent, p_vout);

    vout_CloseAndRelease( p_vout->p_sys->p_vout );
}

#define SHUFFLE_WIDTH 81
#define SHUFFLE_HEIGHT 13
static const char *shuffle_button[] =
{
".................................................................................",
"..............  ............................   ........   ......  ...............",
"..............  ...........................  .........  ........  ...............",
"..............  ...........................  .........  ........  ...............",
"..     .......  .    .......  ....  ......     ......     ......  ........    ...",
".  .... ......   ...  ......  ....  .......  .........  ........  .......  ..  ..",
".  ...........  ....  ......  ....  .......  .........  ........  ......  ....  .",
".      .......  ....  ......  ....  .......  .........  ........  ......        .",
"..      ......  ....  ......  ....  .......  .........  ........  ......  .......",
"......  ......  ....  ......  ....  .......  .........  ........  ......  .......",
". ....  ......  ....  ......  ...   .......  .........  ........  .......  .... .",
"..     .......  ....  .......    .  .......  .........  ........  ........     ..",
"................................................................................."};


/*****************************************************************************
 * Destroy: destroy Magnify video thread output method
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    image_HandlerDelete( p_vout->p_sys->p_image );
    free( p_vout->p_sys->pi_order );

    free( p_vout->p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *p_outpic;

    //video_format_t fmt_out;
    // memset( &fmt_out, 0, sizeof(video_format_t) );
    //picture_t *p_converted;

    int i_plane;

    int i_rows = p_vout->p_sys->i_rows;
    int i_cols = p_vout->p_sys->i_cols;

    /* This is a new frame. Get a structure from the video_output. */
    while( ( p_outpic = vout_CreatePicture( p_vout->p_sys->p_vout, 0, 0, 0 ) )
              == NULL )
    {
        if( !vlc_object_alive (p_vout) || p_vout->b_error )
        {
            return;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }

    p_outpic->date = p_pic->date;

    for( i_plane = 0; i_plane < p_outpic->i_planes; i_plane++ )
    {
        plane_t *p_in = p_pic->p+i_plane;
        plane_t *p_out = p_outpic->p+i_plane;
        int i_pitch = p_in->i_pitch;
        int i;

        for( i = 0; i < i_cols * i_rows; i++ )
        {
            int i_col = i % i_cols;
            int i_row = i / i_cols;
            int i_ocol = p_vout->p_sys->pi_order[i] % i_cols;
            int i_orow = p_vout->p_sys->pi_order[i] / i_cols;
            int i_last_row = i_row + 1;
            i_orow *= p_in->i_lines / i_rows;
            i_row *= p_in->i_lines / i_rows;
            i_last_row *= p_in->i_lines / i_rows;

            if( p_vout->p_sys->b_blackslot == true
                && p_vout->p_sys->b_finished == false
                && i == p_vout->p_sys->i_selected )
            {
                uint8_t color = ( i_plane == Y_PLANE ? 0x0 : 0x80 );
                for( ; i_row < i_last_row; i_row++, i_orow++ )
                {
                    vlc_memset( p_out->p_pixels + i_row * i_pitch
                                               + i_col * i_pitch / i_cols,
                               color, i_pitch / i_cols );
                }
            }
            else
            {
                for( ; i_row < i_last_row; i_row++, i_orow++ )
                {
                    vlc_memcpy( p_out->p_pixels + i_row * i_pitch
                                               + i_col * i_pitch / i_cols,
                               p_in->p_pixels + i_orow * i_pitch
                                              + i_ocol * i_pitch / i_cols,
                               i_pitch / i_cols );
                }
            }
        }
    }

    if(    p_vout->p_sys->i_selected != -1
        && p_vout->p_sys->b_blackslot == false )
    {
        plane_t *p_in = p_pic->p+Y_PLANE;
        plane_t *p_out = p_outpic->p+Y_PLANE;
        int i_pitch = p_in->i_pitch;
        int i_col = p_vout->p_sys->i_selected % i_cols;
        int i_row = p_vout->p_sys->i_selected / i_cols;
        int i_last_row = i_row + 1;
        i_row *= p_in->i_lines / i_rows;
        i_last_row *= p_in->i_lines / i_rows;
        vlc_memset( p_out->p_pixels + i_row * i_pitch
                                   + i_col * i_pitch / i_cols,
                   0xff, i_pitch / i_cols );
        for( ; i_row < i_last_row; i_row++ )
        {
            p_out->p_pixels[   i_row * i_pitch
                             + i_col * i_pitch / i_cols ] = 0xff;
            p_out->p_pixels[ i_row * i_pitch
                             + (i_col+1) * i_pitch / i_cols - 1 ] = 0xff;
        }
        i_row--;
        vlc_memset( p_out->p_pixels + i_row * i_pitch
                                   + i_col * i_pitch / i_cols,
                   0xff, i_pitch / i_cols );
    }

    if( p_vout->p_sys->b_finished == true )
    {
        int i, j;
        plane_t *p_out = p_outpic->p+Y_PLANE;
        int i_pitch = p_out->i_pitch;
        for( i = 0; i < SHUFFLE_HEIGHT; i++ )
        {
            for( j = 0; j < SHUFFLE_WIDTH; j++ )
            {
                if( shuffle_button[i][j] == '.' )
                   p_out->p_pixels[ i * i_pitch + j ] = 0xff;
            }
        }
    }

    vout_DisplayPicture( p_vout->p_sys->p_vout, p_outpic );
}

/*****************************************************************************
 * SendEvents: forward mouse and keyboard events to the parent p_vout
 *****************************************************************************/
static int SendEvents( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);

    var_Set( (vlc_object_t *)p_data, psz_var, newval );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SendEventsToChild: forward events to the child/children vout
 *****************************************************************************/
static int SendEventsToChild( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(p_data); VLC_UNUSED(oldval);
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    var_Set( p_vout->p_sys->p_vout, psz_var, newval );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * MouseEvent: callback for mouse events
 *****************************************************************************/
static int MouseEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval); VLC_UNUSED(newval);
    vout_thread_t *p_vout = (vout_thread_t*)p_data;
    int i_x, i_y;
    int i_v;

#define MOUSE_DOWN    1
#define MOUSE_CLICKED 2
#define MOUSE_MOVE_X  4
#define MOUSE_MOVE_Y  8
#define MOUSE_MOVE    12
    uint8_t mouse= 0;

    int v_h = p_vout->output.i_height;
    int v_w = p_vout->output.i_width;
    int i_pos;

    if( psz_var[6] == 'x' ) mouse |= MOUSE_MOVE_X;
    if( psz_var[6] == 'y' ) mouse |= MOUSE_MOVE_Y;
    if( psz_var[6] == 'c' ) mouse |= MOUSE_CLICKED;

    i_v = var_GetInteger( p_vout->p_sys->p_vout, "mouse-button-down" );
    if( i_v & 0x1 ) mouse |= MOUSE_DOWN;
    i_y = var_GetInteger( p_vout->p_sys->p_vout, "mouse-y" );
    i_x = var_GetInteger( p_vout->p_sys->p_vout, "mouse-x" );

    if( i_y < 0 || i_x < 0 || i_y >= v_h || i_x >= v_w )
        return VLC_SUCCESS;

    if( mouse & MOUSE_CLICKED )
    {
        i_pos = p_vout->p_sys->i_cols * ( ( p_vout->p_sys->i_rows * i_y ) / v_h ) + (p_vout->p_sys->i_cols * i_x ) / v_w;
        if( p_vout->p_sys->b_finished == true
            && i_x < SHUFFLE_WIDTH && i_y < SHUFFLE_HEIGHT )
        {
            shuffle( p_vout->p_sys );
        }
        else if( p_vout->p_sys->i_selected == -1 )
        {
            p_vout->p_sys->i_selected = i_pos;
        }
        else if( p_vout->p_sys->i_selected == i_pos
                 && p_vout->p_sys->b_blackslot == false )
        {
            p_vout->p_sys->i_selected = -1;
        }
        else if(    ( p_vout->p_sys->i_selected == i_pos + 1
                      && p_vout->p_sys->i_selected%p_vout->p_sys->i_cols != 0 )
                 || ( p_vout->p_sys->i_selected == i_pos - 1
                      && i_pos % p_vout->p_sys->i_cols != 0 )
                 || p_vout->p_sys->i_selected == i_pos + p_vout->p_sys->i_cols
                 || p_vout->p_sys->i_selected == i_pos - p_vout->p_sys->i_cols )
        {
            int a = p_vout->p_sys->pi_order[ p_vout->p_sys->i_selected ];
            p_vout->p_sys->pi_order[ p_vout->p_sys->i_selected ] =
                p_vout->p_sys->pi_order[ i_pos ];
            p_vout->p_sys->pi_order[ i_pos ] = a;
            if( p_vout->p_sys->b_blackslot == true )
                p_vout->p_sys->i_selected = i_pos;
            else
                p_vout->p_sys->i_selected = -1;

            p_vout->p_sys->b_finished = finished( p_vout->p_sys );
        }
    }
    return VLC_SUCCESS;
}

static int PuzzleCallback( vlc_object_t *p_this, char const *psz_var,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);
    vout_sys_t *p_sys = (vout_sys_t *)p_data;
    if( !strcmp( psz_var, CFG_PREFIX "rows" ) )
    {
        p_sys->i_rows = __MAX( 1, newval.i_int );
    }
    else if( !strcmp( psz_var, CFG_PREFIX "cols" ) )
    {
        p_sys->i_cols = __MAX( 1, newval.i_int );
    }
    else if( !strcmp( psz_var, CFG_PREFIX "black-slot" ) )
    {
        p_sys->b_blackslot = newval.b_bool;
    }
    shuffle( p_sys );
    return VLC_SUCCESS;
}
