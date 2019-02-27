/*****************************************************************************
 * normvol.c: volume normalizer
 *****************************************************************************
 * Copyright (C) 2001, 2006 VLC authors and VideoLAN
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

/*
 * TODO:
 *
 * We should detect fast power increases and react faster to these
 * This way, we can increase the buffer size to get a more stable filter */


/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <vlc_aout.h>
#include <vlc_filter.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int  Open     ( vlc_object_t * );
static void Close    ( vlc_object_t * );
static block_t *DoWork( filter_t *, block_t * );

typedef struct
{
    int i_nb;
    float *p_last;
    float f_max;
} filter_sys_t;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define BUFF_TEXT N_("Number of audio buffers" )
#define BUFF_LONGTEXT N_("This is the number of audio buffers on which the " \
                "power measurement is made. A higher number of buffers will " \
                "increase the response time of the filter to a spike " \
                "but will make it less sensitive to short variations." )

#define LEVEL_TEXT N_("Maximal volume level" )
#define LEVEL_LONGTEXT N_("If the average power over the last N buffers " \
               "is higher than this value, the volume will be normalized. " \
               "This value is a positive floating point number. A value " \
               "between 0.5 and 10 seems sensible." )

vlc_module_begin ()
    set_description( N_("Volume normalizer") )
    set_shortname( N_("Volume normalizer") )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    add_shortcut( "volnorm" )
    add_integer( "norm-buff-size", 20  ,BUFF_TEXT, BUFF_LONGTEXT,
                 true )
    add_float( "norm-max-level", 2.0, LEVEL_TEXT,
               LEVEL_LONGTEXT, true )
    set_capability( "audio filter", 0 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    unsigned i_channels;
    filter_sys_t *p_sys;

    i_channels = aout_FormatNbChannels( &p_filter->fmt_in.audio );

    p_sys = p_filter->p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;
    p_sys->i_nb = var_CreateGetInteger( vlc_object_parent(p_filter),
                                        "norm-buff-size" );
    p_sys->f_max = var_CreateGetFloat( vlc_object_parent(p_filter),
                                       "norm-max-level" );

    if( p_sys->f_max <= 0 ) p_sys->f_max = 0.01;

    /* We need to store (nb_buffers+1)*nb_channels floats */
    p_sys->p_last = calloc( i_channels * (p_sys->i_nb + 2), sizeof(float) );
    if( !p_sys->p_last )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }

    p_filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
    aout_FormatPrepare(&p_filter->fmt_in.audio);
    p_filter->fmt_out.audio = p_filter->fmt_in.audio;
    p_filter->pf_audio_filter = DoWork;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DoWork : normalizes and sends a buffer
 *****************************************************************************/
static block_t *DoWork( filter_t *p_filter, block_t *p_in_buf )
{
    float *pf_sum;
    float *pf_gain;
    float f_average = 0;
    int i, i_chan;

    int i_samples = p_in_buf->i_nb_samples;
    int i_channels = aout_FormatNbChannels( &p_filter->fmt_in.audio );
    float *p_out = (float*)p_in_buf->p_buffer;
    float *p_in =  (float*)p_in_buf->p_buffer;

    filter_sys_t *p_sys = p_filter->p_sys;

    pf_sum = calloc( i_channels, sizeof(float) );
    if( !pf_sum )
        goto out;

    pf_gain = vlc_alloc( i_channels, sizeof(float) );
    if( !pf_gain )
    {
        free( pf_sum );
        goto out;
    }

    /* Calculate the average power level on this buffer */
    for( i = 0 ; i < i_samples; i++ )
    {
        for( i_chan = 0; i_chan < i_channels; i_chan++ )
        {
            float f_sample = p_in[i_chan];
            pf_sum[i_chan] += f_sample * f_sample;
        }
        p_in += i_channels;
    }

    /* sum now contains for each channel the sigma(value²) */
    for( i_chan = 0; i_chan < i_channels; i_chan++ )
    {
        /* Shift our lastbuff */
        memmove( &p_sys->p_last[ i_chan * p_sys->i_nb],
                        &p_sys->p_last[i_chan * p_sys->i_nb + 1],
                 (p_sys->i_nb-1) * sizeof( float ) );

        /* Insert the new average : sqrt(sigma(value²)) */
        p_sys->p_last[ i_chan * p_sys->i_nb + p_sys->i_nb - 1] =
                sqrt( pf_sum[i_chan] );

        pf_sum[i_chan] = 0;

        /* Get the average power on the lastbuff */
        f_average = 0;
        for( i = 0; i < p_sys->i_nb ; i++)
        {
            f_average += p_sys->p_last[ i_chan * p_sys->i_nb + i ];
        }
        f_average = f_average / p_sys->i_nb;

        /* Seuil arbitraire */
        p_sys->f_max = var_GetFloat( vlc_object_parent(p_filter),
                                     "norm-max-level" );

        //fprintf(stderr,"Average %f, max %f\n", f_average, p_sys->f_max );
        if( f_average > p_sys->f_max )
        {
             pf_gain[i_chan] = f_average / p_sys->f_max;
        }
        else
        {
           pf_gain[i_chan] = 1;
        }
    }

    /* Apply gain */
    for( i = 0; i < i_samples; i++)
    {
        for( i_chan = 0; i_chan < i_channels; i_chan++ )
        {
            p_out[i_chan] /= pf_gain[i_chan];
        }
        p_out += i_channels;
    }

    free( pf_sum );
    free( pf_gain );

    return p_in_buf;
out:
    block_Release( p_in_buf );
    return NULL;
}

/**********************************************************************
 * Close
 **********************************************************************/
static void Close( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    free( p_sys->p_last );
    free( p_sys );
}
