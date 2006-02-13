/*****************************************************************************
 * motiondetect.c : Motion detect video effect plugin for vlc
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Jérôme Decoodt <djc@videolan.org>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/intf.h>

#include "filter_common.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );
static void MotionDetect( vout_thread_t *p_vout, picture_t *p_inpic,
                                                 picture_t *p_outpic );

static int  SendEvents   ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DESC_TEXT N_("Description file")
#define DESC_LONGTEXT N_("Description file, file containing simple playlist")
#define HISTORY_TEXT N_("History parameter")
#define HISTORY_LONGTEXT N_("History parameter, number of frames used for detection")

vlc_module_begin();
    set_description( _("Motion detect video filter") );
    set_shortname( N_( "Motion detect" ));
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER );
    set_capability( "video filter", 0 );

    add_integer( "motiondetect-history", 1, NULL, HISTORY_TEXT,
                                HISTORY_LONGTEXT, VLC_FALSE );
    add_string( "motiondetect-description", "motiondetect", NULL, DESC_TEXT,
                                DESC_LONGTEXT, VLC_FALSE );

    set_callbacks( Create, Destroy );
vlc_module_end();

/*****************************************************************************
 * vout_sys_t: Motion detect video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Motion detect specific properties of an output thread.
 *****************************************************************************/
typedef struct area_t
{
    int i_x1, i_y1;
    int i_x2, i_y2;
    int i_matches;
    int i_level;
    int i_downspeed, i_upspeed;
    char *psz_mrl;
} area_t;

struct vout_sys_t
{
    vout_thread_t *p_vout;
    playlist_t *p_playlist;

    uint8_t *p_bufferY;
    int i_stack;
    area_t** pp_areas;
    int i_areas;
    int i_history;
};

/*****************************************************************************
 * Control: control facility for the vout (forwards to child vout)
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    return vout_vaControl( p_vout->p_sys->p_vout, i_query, args );
}

/*****************************************************************************
 * Create: allocates Distort video thread output method
 *****************************************************************************
 * This function allocates and initializes a Distort vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    char *psz_descfilename;
    char buffer[256];
    int x1, x2, y1, y2, i_level, i_downspeed, i_upspeed, i;
    area_t *p_area;
    FILE * p_file;

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

    memset( p_vout->p_sys, 0, sizeof( vout_sys_t ) );

    p_vout->p_sys->i_history = config_GetInt( p_vout,
                                              "motiondetect-history" );

    if( !(psz_descfilename = config_GetPsz( p_vout,
                                            "motiondetect-description" ) ) )
    {
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    p_vout->p_sys->p_playlist = vlc_object_find( p_this, VLC_OBJECT_PLAYLIST,
                                                 FIND_ANYWHERE );
    if( !p_vout->p_sys->p_playlist )
    {
         msg_Err( p_vout, "playlist not found" );
         free( p_vout->p_sys );
         return VLC_EGENERIC;
    }

    /* Parse description file and allocate areas */
    p_file = utf8_fopen( psz_descfilename, "r" );
    if( !p_file )
    {
        msg_Err( p_this, "Failed to open descritpion file %s",
                            psz_descfilename );
        free( psz_descfilename );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }
    p_vout->p_sys->i_areas = 0;
    while( fscanf( p_file, "%d,%d,%d,%d,%d,%d,%d,",
                            &x1, &y1, &x2, &y2, &i_level,
                            &i_downspeed, &i_upspeed ) == 7 )
    {
        for( i = 0 ; i < 255 ; i++ )
        {
            fread( buffer + i, 1, 1, p_file );
            if( *( buffer + i ) == '\n' )
                break;
        }
        *( buffer + i )  = 0;
        p_vout->p_sys->i_areas++;
        p_vout->p_sys->pp_areas = realloc( p_vout->p_sys->pp_areas,
                                        p_vout->p_sys->i_areas * 
                                                    sizeof( area_t ) );
        if( !p_vout->p_sys->pp_areas )
            /*FIXME: clean this... */
            return VLC_ENOMEM;
        p_area = malloc( sizeof( area_t ) );
        if( !p_area )
            break;

        p_area->i_x1 = x1;
        p_area->i_x2 = x2;
        p_area->i_y1 = y1;
        p_area->i_y2 = y2;
        p_area->i_matches = 0;
        p_area->i_level = i_level;
        p_area->i_downspeed = i_downspeed;
        p_area->i_upspeed = i_upspeed;

        p_area->psz_mrl = strdup(buffer);
        p_vout->p_sys->pp_areas[p_vout->p_sys->i_areas-1] = p_area;
    }
    fclose( p_file );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize Motion detect video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;
    video_format_t fmt = {0};

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

    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );

    ADD_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );

    ADD_PARENT_CALLBACKS( SendEventsToChild );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate Motion detect video thread output method
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
 * Destroy: destroy Motion detect video thread output method
 *****************************************************************************
 * Terminate an output method created by DistortCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    int i;

    if( p_vout->p_sys->p_vout )
    {
        DEL_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );
        vlc_object_detach( p_vout->p_sys->p_vout );
        vout_Destroy( p_vout->p_sys->p_vout );
    }

    DEL_PARENT_CALLBACKS( SendEventsToChild );

    for( i = 0 ; i < p_vout->p_sys->i_areas ; i++ )
    {
        free( p_vout->p_sys->pp_areas[i]->psz_mrl );
        free( p_vout->p_sys->pp_areas[i] );
    }

    free( p_vout->p_sys->pp_areas );
    free( p_vout->p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Distort image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *p_outpic;

    /* This is a new frame. Get a structure from the video_output. */
    while( ( p_outpic = vout_CreatePicture( p_vout->p_sys->p_vout, 0, 0, 0 ) )
              == NULL )
    {
        if( p_vout->b_die || p_vout->b_error )
        {
            return;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }

    vout_DatePicture( p_vout->p_sys->p_vout, p_outpic, p_pic->date );

    MotionDetect( p_vout, p_pic, p_outpic );

    vout_DisplayPicture( p_vout->p_sys->p_vout, p_outpic );
}

/*****************************************************************************
 * MotionDetect: calculates new matches
 *****************************************************************************/
static void MotionDetect( vout_thread_t *p_vout, picture_t *p_inpic,
                                                 picture_t *p_outpic )
{
#define pp_curent_area p_vout->p_sys->pp_areas[i_area]

    int i_index, i_index_col, i_area;
    for( i_index = 0 ; i_index < p_inpic->i_planes ; i_index++ )
    {
        int i_line, i_num_lines, i_offset, i_size, i_diff;
        uint8_t *p_in, *p_out, *p_last_in, *p_buffer;

        p_in = p_inpic->p[i_index].p_pixels;
        p_out = p_outpic->p[i_index].p_pixels;

        i_num_lines = p_inpic->p[i_index].i_visible_lines;
        i_size = p_inpic->p[i_index].i_lines * p_inpic->p[i_index].i_pitch;

        p_vout->p_vlc->pf_memcpy( p_out, p_in, i_size );
        switch( i_index )
        {
        case Y_PLANE:
            p_buffer = p_vout->p_sys->p_bufferY;

            if( p_buffer == NULL)
            {
                p_buffer = malloc( p_vout->p_sys->i_history * i_size);
                memset( p_buffer, 0, p_vout->p_sys->i_history * i_size );

                p_vout->p_sys->p_bufferY = p_buffer;
                p_vout->p_sys->i_stack = 0;
            }

            i_offset = i_size * p_vout->p_sys->i_stack;
            p_last_in = p_buffer + i_offset;
            for( i_area = 0 ; i_area < p_vout->p_sys->i_areas ; i_area++)
            {
                int i_tmp = 0, i_nb_pixels;
                p_last_in = p_buffer + i_offset;
                p_in = p_inpic->p[i_index].p_pixels;
                p_out = p_outpic->p[i_index].p_pixels;
                if( ( pp_curent_area->i_y1 > p_inpic->p[i_index].i_lines ) ||
                    ( pp_curent_area->i_x1 > p_inpic->p[i_index].i_pitch ) )
                    continue;
                if( ( pp_curent_area->i_y2 > p_inpic->p[i_index].i_lines ) )
                    pp_curent_area->i_y2 = p_inpic->p[i_index].i_lines;
                if( ( pp_curent_area->i_x2 > p_inpic->p[i_index].i_pitch ) )
                    pp_curent_area->i_x2 = p_inpic->p[i_index].i_pitch;

                for( i_line = pp_curent_area->i_y1 ;
                     i_line < pp_curent_area->i_y2 ; i_line++ )
                {
                    for( i_index_col = pp_curent_area->i_x1 ;
                         i_index_col < pp_curent_area->i_x2 ; i_index_col++ )
                    {
                        i_diff = (*(p_last_in + i_index_col +
                                        i_line * p_inpic->p[i_index].i_pitch)
                                - *(p_in      + i_index_col +
                                        i_line * p_inpic->p[i_index].i_pitch));
                        if( i_diff < 0)
                            i_diff = -i_diff;

                        if( i_diff > pp_curent_area->i_level )
                            i_tmp += pp_curent_area->i_upspeed;

                        *( p_out + i_index_col + i_line *
                                        p_inpic->p[i_index].i_pitch ) =
                                                pp_curent_area->i_matches;
                    }
                }
                i_nb_pixels = ( pp_curent_area->i_y2 - pp_curent_area->i_y1 ) *
                              ( pp_curent_area->i_x2 - pp_curent_area->i_x1 );
                pp_curent_area->i_matches += i_tmp / i_nb_pixels -
                                    pp_curent_area->i_downspeed;
                if( pp_curent_area->i_matches < 0)
                    pp_curent_area->i_matches = 0;
                if( pp_curent_area->i_matches > 255)
                {
                    playlist_item_t *p_item = playlist_ItemNew( p_vout,
                                        (const char*)pp_curent_area->psz_mrl,
                                        pp_curent_area->psz_mrl );
                    msg_Dbg( p_vout, "Area(%d) matched, going to %s\n", i_area,
                                        pp_curent_area->psz_mrl );
                    playlist_Control( p_vout->p_sys->p_playlist,
                                        PLAYLIST_ITEMPLAY, p_item );
                    pp_curent_area->i_matches = 0;
                }
            }
            p_last_in = p_buffer + i_offset;
            p_in = p_inpic->p[i_index].p_pixels;
            p_out = p_outpic->p[i_index].p_pixels;

            p_vout->p_vlc->pf_memcpy( p_last_in, p_in, i_size );
            break;
        default:
            break;
        }
    }
    p_vout->p_sys->i_stack++;
    if( p_vout->p_sys->i_stack >= p_vout->p_sys->i_history )
        p_vout->p_sys->i_stack = 0;
#undef pp_curent_area
}

/*****************************************************************************
 * SendEvents: forward mouse and keyboard events to the parent p_vout
 *****************************************************************************/
static int SendEvents( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    var_Set( (vlc_object_t *)p_data, psz_var, newval );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SendEventsToChild: forward events to the child/children vout
 *****************************************************************************/
static int SendEventsToChild( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    var_Set( p_vout->p_sys->p_vout, psz_var, newval );
    return VLC_SUCCESS;
}
