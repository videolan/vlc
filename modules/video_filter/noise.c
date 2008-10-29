/*****************************************************************************
 * noise.c : "add noise to image" video filter
 *****************************************************************************
 * Copyright (C) 2000-2006 the VideoLAN team
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

#include "vlc_filter.h"
#include "filter_picture.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );

#define FILTER_PREFIX "noise-"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Noise video filter") )
    set_shortname( N_( "Noise" ))
    set_capability( "video filter2", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    add_shortcut( "noise" )
    set_callbacks( Create, Destroy )
vlc_module_end ()

/*****************************************************************************
 * vout_sys_t: Distort video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Distort specific properties of an output thread.
 *****************************************************************************/
struct filter_sys_t
{
    mtime_t last_date;
};

/*****************************************************************************
 * Create: allocates Distort video thread output method
 *****************************************************************************
 * This function allocates and initializes a Distort vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    /* Allocate structure */
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;

    p_filter->pf_video_filter = Filter;

    p_filter->p_sys->last_date = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy Distort video thread output method
 *****************************************************************************
 * Terminate an output method created by DistortCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    free( p_filter->p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Distort image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;
    filter_sys_t *p_sys = p_filter->p_sys;
    int i_index;
    mtime_t new_date = mdate();

    if( !p_pic ) return NULL;

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        msg_Warn( p_filter, "can't get output picture" );
        picture_Release( p_pic );
        return NULL;
    }

    p_sys->last_date = new_date;

    for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
    {
        uint8_t *p_in = p_pic->p[i_index].p_pixels;
        uint8_t *p_out = p_outpic->p[i_index].p_pixels;

        const int i_num_lines = p_pic->p[i_index].i_visible_lines;
        const int i_num_cols = p_pic->p[i_index].i_visible_pitch;
        const int i_pitch = p_pic->p[i_index].i_pitch;

        int i_line, i_col;

        for( i_line = 0 ; i_line < i_num_lines ; i_line++ )
        {
            if( rand()%8 )
            {
                /* line isn't noisy */
                vlc_memcpy( p_out+i_line*i_pitch, p_in+i_line*i_pitch,
                            i_num_cols );
            }
            else
            {
                /* this line is noisy */
                int noise_level = rand()%8+2;
                for( i_col = 0; i_col < i_num_cols ; i_col++ )
                {
                    if( rand()%noise_level )
                    {
                        p_out[i_line*i_pitch+i_col] =
                            p_in[i_line*i_pitch+i_col];
                    }
                    else
                    {
                        p_out[i_line*i_pitch+i_col] = (rand()%3)*0x7f;
                    }
                }
            }
        }
    }

    return CopyInfoAndRelease( p_outpic, p_pic );
}
