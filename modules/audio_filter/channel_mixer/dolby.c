/*****************************************************************************
 * dolby.c : simple decoder for dolby surround encoded streams
 *****************************************************************************
 * Copyright (C) 2005, 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Boris Dorès <babal@via.ecp.fr>
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
#include <vlc_aout.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static void DoWork    ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                        aout_buffer_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( N_("Simple decoder for Dolby Surround encoded streams") );
    set_shortname( N_("Dolby Surround decoder") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACODEC );
    set_capability( "audio filter", 5 );
    set_callbacks( Create, Destroy );
vlc_module_end();

/*****************************************************************************
 * Internal data structures
 *****************************************************************************/
struct aout_filter_sys_t
{
    int i_left;
    int i_center;
    int i_right;
    int i_rear_left;
    int i_rear_center;
    int i_rear_right;
};

/* our internal channel order (WG-4 order) */
static const uint32_t pi_channels[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT, AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT,
  AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT, AOUT_CHAN_REARCENTER,
  AOUT_CHAN_CENTER, AOUT_CHAN_LFE, 0 };

/*****************************************************************************
 * Create: allocate headphone downmixer
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    int i = 0;
    int i_offset = 0;
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    /* Validate audio filter format */
    if ( p_filter->input.i_physical_channels != (AOUT_CHAN_LEFT|AOUT_CHAN_RIGHT)
       || ! ( p_filter->input.i_original_channels & AOUT_CHAN_DOLBYSTEREO )
       || aout_FormatNbChannels( &p_filter->output ) <= 2
       || ( p_filter->input.i_original_channels & ~AOUT_CHAN_DOLBYSTEREO )
          != ( p_filter->output.i_original_channels & ~AOUT_CHAN_DOLBYSTEREO ) )
    {
        return VLC_EGENERIC;
    }

    if ( p_filter->input.i_rate != p_filter->output.i_rate )
    {
        return VLC_EGENERIC;
    }

    if ( p_filter->input.i_format != VLC_FOURCC('f','l','3','2')
          || p_filter->output.i_format != VLC_FOURCC('f','l','3','2') )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the module's structure */
    p_filter->p_sys = malloc( sizeof(struct aout_filter_sys_t) );
    if ( p_filter->p_sys == NULL )
        return VLC_ENOMEM;
    p_filter->p_sys->i_left = -1;
    p_filter->p_sys->i_center = -1;
    p_filter->p_sys->i_right = -1;
    p_filter->p_sys->i_rear_left = -1;
    p_filter->p_sys->i_rear_center = -1;
    p_filter->p_sys->i_rear_right = -1;

    while ( pi_channels[i] )
    {
        if ( p_filter->output.i_physical_channels & pi_channels[i] )
        {
            switch ( pi_channels[i] )
            {
                case AOUT_CHAN_LEFT:
                    p_filter->p_sys->i_left = i_offset;
                    break;
                case AOUT_CHAN_CENTER:
                    p_filter->p_sys->i_center = i_offset;
                    break;
                case AOUT_CHAN_RIGHT:
                    p_filter->p_sys->i_right = i_offset;
                    break;
                case AOUT_CHAN_REARLEFT:
                    p_filter->p_sys->i_rear_left = i_offset;
                    break;
                case AOUT_CHAN_REARCENTER:
                    p_filter->p_sys->i_rear_center = i_offset;
                    break;
                case AOUT_CHAN_REARRIGHT:
                    p_filter->p_sys->i_rear_right = i_offset;
                    break;
            }
            ++i_offset;
        }
        ++i;
    }

    p_filter->pf_do_work = DoWork;
    p_filter->b_in_place = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: deallocate resources associated with headphone downmixer
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( p_filter->p_sys != NULL )
    {
        free ( p_filter->p_sys );
        p_filter->p_sys = NULL;
    }
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
    float * p_in = (float*) p_in_buf->p_buffer;
    float * p_out = (float*) p_out_buf->p_buffer;
    size_t i_nb_samples = p_in_buf->i_nb_samples;
    size_t i_nb_channels = aout_FormatNbChannels( &p_filter->output );

    p_out_buf->i_nb_samples = i_nb_samples;
    p_out_buf->i_nb_bytes = sizeof(float) * i_nb_samples
                            * aout_FormatNbChannels( &p_filter->output );
    memset ( p_out , 0 , p_out_buf->i_nb_bytes );

    if ( p_filter->p_sys != NULL )
    {
        struct aout_filter_sys_t * p_sys = p_filter->p_sys;
        size_t i_nb_rear = 0;
        size_t i;

        if ( p_sys->i_rear_left >= 0 )
        {
            ++i_nb_rear;
        }
        if ( p_sys->i_rear_center >= 0 )
        {
            ++i_nb_rear;
        }
        if ( p_sys->i_rear_right >= 0 )
        {
            ++i_nb_rear;
        }

        for ( i = 0; i < i_nb_samples; ++i )
        {
            float f_left = p_in[ i * 2 ];
            float f_right = p_in[ i * 2 + 1 ];
            float f_rear = ( f_left - f_right ) / i_nb_rear;

            if ( p_sys->i_center >= 0 )
            {
                float f_center = f_left + f_right;
                f_left -= f_center / 2;
                f_right -= f_center / 2;

                p_out[ i * i_nb_channels + p_sys->i_center ] = f_center;
            }

            if ( p_sys->i_left >= 0 )
            {
                p_out[ i * i_nb_channels + p_sys->i_left ] = f_left;
            }
            if ( p_sys->i_right >= 0 )
            {
                p_out[ i * i_nb_channels + p_sys->i_right ] = f_right;
            }
            if ( p_sys->i_rear_left >= 0 )
            {
                p_out[ i * i_nb_channels + p_sys->i_rear_left ] = f_rear;
            }
            if ( p_sys->i_rear_center >= 0 )
            {
                p_out[ i * i_nb_channels + p_sys->i_rear_center ] = f_rear;
            }
            if ( p_sys->i_rear_right >= 0 )
            {
                p_out[ i * i_nb_channels + p_sys->i_rear_right ] = f_rear;
            }
        }
    }
}
