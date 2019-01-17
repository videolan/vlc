/*****************************************************************************
 * dolby.c : simple decoder for dolby surround encoded streams
 *****************************************************************************
 * Copyright (C) 2005-2009 the VideoLAN team
 *
 * Authors: Boris Dorès <babal@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_filter.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static block_t *DoWork( filter_t *, block_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Simple decoder for Dolby Surround encoded streams") )
    set_shortname( N_("Dolby Surround decoder") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )
    set_capability( "audio converter", 5 )
    set_callbacks( Create, Destroy )
vlc_module_end ()

/*****************************************************************************
 * Internal data structures
 *****************************************************************************/
typedef struct
{
    int i_left;
    int i_center;
    int i_right;
    int i_rear_left;
    int i_rear_center;
    int i_rear_right;
} filter_sys_t;

/*****************************************************************************
 * Create: allocate headphone downmixer
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    int i = 0;
    int i_offset = 0;
    filter_t * p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    /* Validate audio filter format */
    if ( p_filter->fmt_in.audio.i_physical_channels != (AOUT_CHAN_LEFT|AOUT_CHAN_RIGHT)
       || ! ( p_filter->fmt_in.audio.i_chan_mode & AOUT_CHANMODE_DOLBYSTEREO )
       || p_filter->fmt_out.audio.i_channels <= 2
       || ( p_filter->fmt_in.audio.i_chan_mode & ~AOUT_CHANMODE_DOLBYSTEREO )
          != ( p_filter->fmt_out.audio.i_chan_mode & ~AOUT_CHANMODE_DOLBYSTEREO ) )
    {
        return VLC_EGENERIC;
    }

    if ( p_filter->fmt_in.audio.i_rate != p_filter->fmt_out.audio.i_rate )
    {
        return VLC_EGENERIC;
    }

    if ( p_filter->fmt_in.audio.i_format != VLC_CODEC_FL32
          || p_filter->fmt_out.audio.i_format != VLC_CODEC_FL32 )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the module's structure */
    p_sys = p_filter->p_sys = malloc( sizeof(*p_sys) );
    if( p_sys == NULL )
        return VLC_ENOMEM;
    p_sys->i_left = -1;
    p_sys->i_center = -1;
    p_sys->i_right = -1;
    p_sys->i_rear_left = -1;
    p_sys->i_rear_center = -1;
    p_sys->i_rear_right = -1;

    while ( pi_vlc_chan_order_wg4[i] )
    {
        if ( p_filter->fmt_out.audio.i_physical_channels & pi_vlc_chan_order_wg4[i] )
        {
            switch ( pi_vlc_chan_order_wg4[i] )
            {
                case AOUT_CHAN_LEFT:
                    p_sys->i_left = i_offset;
                    break;
                case AOUT_CHAN_CENTER:
                    p_sys->i_center = i_offset;
                    break;
                case AOUT_CHAN_RIGHT:
                    p_sys->i_right = i_offset;
                    break;
                case AOUT_CHAN_REARLEFT:
                    p_sys->i_rear_left = i_offset;
                    break;
                case AOUT_CHAN_REARCENTER:
                    p_sys->i_rear_center = i_offset;
                    break;
                case AOUT_CHAN_REARRIGHT:
                    p_sys->i_rear_right = i_offset;
                    break;
            }
            ++i_offset;
        }
        ++i;
    }

    p_filter->pf_audio_filter = DoWork;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: deallocate resources associated with headphone downmixer
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t * p_filter = (filter_t *)p_this;
    free( p_filter->p_sys );
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static block_t *DoWork( filter_t * p_filter, block_t * p_in_buf )
{
    filter_sys_t * p_sys = p_filter->p_sys;
    float * p_in = (float*) p_in_buf->p_buffer;
    size_t i_nb_samples = p_in_buf->i_nb_samples;
    size_t i_nb_channels = aout_FormatNbChannels( &p_filter->fmt_out.audio );
    size_t i_nb_rear = 0;
    size_t i;
    block_t *p_out_buf = block_Alloc(
                                sizeof(float) * i_nb_samples * i_nb_channels );
    if( !p_out_buf )
        goto out;

    float * p_out = (float*) p_out_buf->p_buffer;
    p_out_buf->i_nb_samples = i_nb_samples;
    p_out_buf->i_dts        = p_in_buf->i_dts;
    p_out_buf->i_pts        = p_in_buf->i_pts;
    p_out_buf->i_length     = p_in_buf->i_length;

    memset( p_out, 0, p_out_buf->i_buffer );

    if( p_sys->i_rear_left >= 0 )
    {
        ++i_nb_rear;
    }
    if( p_sys->i_rear_center >= 0 )
    {
        ++i_nb_rear;
    }
    if( p_sys->i_rear_right >= 0 )
    {
        ++i_nb_rear;
    }

    for( i = 0; i < i_nb_samples; ++i )
    {
        float f_left = p_in[ i * 2 ];
        float f_right = p_in[ i * 2 + 1 ];
        float f_rear = ( f_left - f_right ) / i_nb_rear;

        if( p_sys->i_center >= 0 )
        {
            float f_center = f_left + f_right;
            f_left -= f_center / 2;
            f_right -= f_center / 2;

            p_out[ i * i_nb_channels + p_sys->i_center ] = f_center;
        }

        if( p_sys->i_left >= 0 )
        {
            p_out[ i * i_nb_channels + p_sys->i_left ] = f_left;
        }
        if( p_sys->i_right >= 0 )
        {
            p_out[ i * i_nb_channels + p_sys->i_right ] = f_right;
        }
        if( p_sys->i_rear_left >= 0 )
        {
            p_out[ i * i_nb_channels + p_sys->i_rear_left ] = f_rear;
        }
        if( p_sys->i_rear_center >= 0 )
        {
            p_out[ i * i_nb_channels + p_sys->i_rear_center ] = f_rear;
        }
        if( p_sys->i_rear_right >= 0 )
        {
            p_out[ i * i_nb_channels + p_sys->i_rear_right ] = f_rear;
        }
    }
out:
    block_Release( p_in_buf );
    return p_out_buf;
}
