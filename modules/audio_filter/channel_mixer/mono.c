/*****************************************************************************
 * mono.c : stereo2mono downmixsimple channel mixer plug-in
 *****************************************************************************
 * Copyright (C) 2006 M2X
 *
 * Authors: Jean-Paul Saman <jpsaman at m2x dot nl>
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

#include <math.h>                                        /* sqrt */
#include <stdint.h>                                         /* int16_t .. */

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_block.h>
#include <vlc_filter.h>
#include <vlc_aout.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenFilter    ( vlc_object_t * );
static void CloseFilter   ( vlc_object_t * );

static block_t *Convert( filter_t *p_filter, block_t *p_block );

static unsigned int stereo_to_mono( filter_t *, block_t *, block_t * );
static unsigned int mono( filter_t *, block_t *, block_t * );
static void stereo2mono_downmix( filter_t *, block_t *, block_t * );

/*****************************************************************************
 * Local structures
 *****************************************************************************/
struct atomic_operation_t
{
    int i_source_channel_offset;
    int i_dest_channel_offset;
    unsigned int i_delay;/* in sample unit */
    double d_amplitude_factor;
};

typedef struct
{
    bool b_downmix;

    unsigned int i_nb_channels; /* number of int16_t per sample */
    int i_channel_selected;
    int i_bitspersample;

    size_t i_overflow_buffer_size;/* in bytes */
    uint8_t * p_overflow_buffer;
    unsigned int i_nb_atomic_operations;
    struct atomic_operation_t * p_atomic_operations;
} filter_sys_t;

#define MONO_DOWNMIX_TEXT N_("Use downmix algorithm")
#define MONO_DOWNMIX_LONGTEXT N_("This option selects a stereo to mono " \
    "downmix algorithm that is used in the headphone channel mixer. It " \
    "gives the effect of standing in a room full of speakers." )

#define MONO_CHANNEL_TEXT N_("Select channel to keep")
#define MONO_CHANNEL_LONGTEXT N_("This option silences all other channels " \
    "except the selected channel.")

static const int pi_pos_values[] = { 0, 1, 4, 5, 7, 8, 2, 3, 6 };
static const char *const ppsz_pos_descriptions[] =
{ N_("Left"), N_("Right"),
  N_("Rear left"), N_("Rear right"),
  N_("Center"), N_("Low-frequency effects"),
  N_("Side left"), N_("Side right"), N_("Rear center") };

#define MONO_CFG "sout-mono-"
/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Stereo to mono downmixer") )
    set_capability( "audio filter", 0 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_callbacks( OpenFilter, CloseFilter )
    set_shortname( "Mono" )

    add_bool( MONO_CFG "downmix", true, MONO_DOWNMIX_TEXT,
              MONO_DOWNMIX_LONGTEXT, false )
    add_integer( MONO_CFG "channel", -1, MONO_CHANNEL_TEXT,
        MONO_CHANNEL_LONGTEXT, false )
        change_integer_list( pi_pos_values, ppsz_pos_descriptions )

vlc_module_end ()

/* Init() and ComputeChannelOperations() -
 * Code taken from modules/audio_filter/channel_mixer/headphone.c
 * converted from float into int16_t based downmix
 * Written by Boris Dorès <babal@via.ecp.fr>
 */

/*****************************************************************************
 * Init: initialize internal data structures
 * and computes the needed atomic operations
 *****************************************************************************/
/* x and z represent the coordinates of the virtual speaker
 *  relatively to the center of the listener's head, measured in meters :
 *
 *  left              right
 *Z
 *-
 *a          head
 *x
 *i
 *s
 *  rear left    rear right
 *
 *          x-axis
 *  */
static void ComputeChannelOperations( filter_sys_t * p_data,
        unsigned int i_rate, unsigned int i_next_atomic_operation,
        int i_source_channel_offset, double d_x, double d_z,
        double d_compensation_length, double d_channel_amplitude_factor )
{
    double d_c = 340; /*sound celerity (unit: m/s)*/
    double d_compensation_delay = (d_compensation_length-0.1) / d_c * i_rate;

    /* Left ear */
    p_data->p_atomic_operations[i_next_atomic_operation]
        .i_source_channel_offset = i_source_channel_offset;
    p_data->p_atomic_operations[i_next_atomic_operation]
        .i_dest_channel_offset = 0;/* left */
    p_data->p_atomic_operations[i_next_atomic_operation]
        .i_delay = (int)( sqrt( (-0.1-d_x)*(-0.1-d_x) + (0-d_z)*(0-d_z) )
                          / d_c * i_rate - d_compensation_delay );
    if( d_x < 0 )
    {
        p_data->p_atomic_operations[i_next_atomic_operation]
            .d_amplitude_factor = d_channel_amplitude_factor * 1.1 / 2;
    }
    else if( d_x > 0 )
    {
        p_data->p_atomic_operations[i_next_atomic_operation]
            .d_amplitude_factor = d_channel_amplitude_factor * 0.9 / 2;
    }
    else
    {
        p_data->p_atomic_operations[i_next_atomic_operation]
            .d_amplitude_factor = d_channel_amplitude_factor / 2;
    }

    /* Right ear */
    p_data->p_atomic_operations[i_next_atomic_operation + 1]
        .i_source_channel_offset = i_source_channel_offset;
    p_data->p_atomic_operations[i_next_atomic_operation + 1]
        .i_dest_channel_offset = 1;/* right */
    p_data->p_atomic_operations[i_next_atomic_operation + 1]
        .i_delay = (int)( sqrt( (0.1-d_x)*(0.1-d_x) + (0-d_z)*(0-d_z) )
                          / d_c * i_rate - d_compensation_delay );
    if( d_x < 0 )
    {
        p_data->p_atomic_operations[i_next_atomic_operation + 1]
            .d_amplitude_factor = d_channel_amplitude_factor * 0.9 / 2;
    }
    else if( d_x > 0 )
    {
        p_data->p_atomic_operations[i_next_atomic_operation + 1]
            .d_amplitude_factor = d_channel_amplitude_factor * 1.1 / 2;
    }
    else
    {
        p_data->p_atomic_operations[i_next_atomic_operation + 1]
            .d_amplitude_factor = d_channel_amplitude_factor / 2;
    }
}

static int Init( vlc_object_t *p_this, filter_sys_t * p_data,
                 unsigned int i_nb_channels, uint32_t i_physical_channels,
                 unsigned int i_rate )
{
    double d_x = var_InheritInteger( p_this, "headphone-dim" );
    double d_z = d_x;
    double d_z_rear = -d_x/3;
    double d_min = 0;
    unsigned int i_next_atomic_operation;
    int i_source_channel_offset;
    unsigned int i;

    if( var_InheritBool( p_this, "headphone-compensate" ) )
    {
        /* minimal distance to any speaker */
        if( i_physical_channels & AOUT_CHAN_REARCENTER )
        {
            d_min = d_z_rear;
        }
        else
        {
            d_min = d_z;
        }
    }

    /* Number of elementary operations */
    p_data->i_nb_atomic_operations = i_nb_channels * 2;
    if( i_physical_channels & AOUT_CHAN_CENTER )
    {
        p_data->i_nb_atomic_operations += 2;
    }
    p_data->p_atomic_operations = malloc( sizeof(struct atomic_operation_t)
            * p_data->i_nb_atomic_operations );
    if( p_data->p_atomic_operations == NULL )
        return -1;

    /* For each virtual speaker, computes elementary wave propagation time
     * to each ear */
    i_next_atomic_operation = 0;
    i_source_channel_offset = 0;
    if( i_physical_channels & AOUT_CHAN_LEFT )
    {
        ComputeChannelOperations( p_data , i_rate
                , i_next_atomic_operation , i_source_channel_offset
                , -d_x , d_z , d_min , 2.0 / i_nb_channels );
        i_next_atomic_operation += 2;
        i_source_channel_offset++;
    }
    if( i_physical_channels & AOUT_CHAN_RIGHT )
    {
        ComputeChannelOperations( p_data , i_rate
                , i_next_atomic_operation , i_source_channel_offset
                , d_x , d_z , d_min , 2.0 / i_nb_channels );
        i_next_atomic_operation += 2;
        i_source_channel_offset++;
    }
    if( i_physical_channels & AOUT_CHAN_MIDDLELEFT )
    {
        ComputeChannelOperations( p_data , i_rate
                , i_next_atomic_operation , i_source_channel_offset
                , -d_x , 0 , d_min , 1.5 / i_nb_channels );
        i_next_atomic_operation += 2;
        i_source_channel_offset++;
    }
    if( i_physical_channels & AOUT_CHAN_MIDDLERIGHT )
    {
        ComputeChannelOperations( p_data , i_rate
                , i_next_atomic_operation , i_source_channel_offset
                , d_x , 0 , d_min , 1.5 / i_nb_channels );
        i_next_atomic_operation += 2;
        i_source_channel_offset++;
    }
    if( i_physical_channels & AOUT_CHAN_REARLEFT )
    {
        ComputeChannelOperations( p_data , i_rate
                , i_next_atomic_operation , i_source_channel_offset
                , -d_x , d_z_rear , d_min , 1.5 / i_nb_channels );
        i_next_atomic_operation += 2;
        i_source_channel_offset++;
    }
    if( i_physical_channels & AOUT_CHAN_REARRIGHT )
    {
        ComputeChannelOperations( p_data , i_rate
                , i_next_atomic_operation , i_source_channel_offset
                , d_x , d_z_rear , d_min , 1.5 / i_nb_channels );
        i_next_atomic_operation += 2;
        i_source_channel_offset++;
    }
    if( i_physical_channels & AOUT_CHAN_REARCENTER )
    {
        ComputeChannelOperations( p_data , i_rate
                , i_next_atomic_operation , i_source_channel_offset
                , 0 , -d_z , d_min , 1.5 / i_nb_channels );
        i_next_atomic_operation += 2;
        i_source_channel_offset++;
    }
    if( i_physical_channels & AOUT_CHAN_CENTER )
    {
        /* having two center channels increases the spatialization effect */
        ComputeChannelOperations( p_data , i_rate
                , i_next_atomic_operation , i_source_channel_offset
                , d_x / 5.0 , d_z , d_min , 0.75 / i_nb_channels );
        i_next_atomic_operation += 2;
        ComputeChannelOperations( p_data , i_rate
                , i_next_atomic_operation , i_source_channel_offset
                , -d_x / 5.0 , d_z , d_min , 0.75 / i_nb_channels );
        i_next_atomic_operation += 2;
        i_source_channel_offset++;
    }
    if( i_physical_channels & AOUT_CHAN_LFE )
    {
        ComputeChannelOperations( p_data , i_rate
                , i_next_atomic_operation , i_source_channel_offset
                , 0 , d_z_rear , d_min , 5.0 / i_nb_channels );
        i_next_atomic_operation += 2;
        i_source_channel_offset++;
    }

    /* Initialize the overflow buffer
     * we need it because the process induce a delay in the samples */
    p_data->i_overflow_buffer_size = 0;
    for( i = 0 ; i < p_data->i_nb_atomic_operations ; i++ )
    {
        if( p_data->i_overflow_buffer_size
                < p_data->p_atomic_operations[i].i_delay * 2 * sizeof (int16_t) )
        {
            p_data->i_overflow_buffer_size
                = p_data->p_atomic_operations[i].i_delay * 2 * sizeof (int16_t);
        }
    }
    p_data->p_overflow_buffer = malloc( p_data->i_overflow_buffer_size );
    if( p_data->p_overflow_buffer == NULL )
    {
        free( p_data->p_atomic_operations );
        return -1;
    }
    memset( p_data->p_overflow_buffer, 0, p_data->i_overflow_buffer_size );

    /* end */
    return 0;
}

/*****************************************************************************
 * OpenFilter
 *****************************************************************************/
static int OpenFilter( vlc_object_t *p_this )
{
    filter_t * p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = NULL;

    if( aout_FormatNbChannels( &(p_filter->fmt_in.audio) ) == 1 )
    {
        /*msg_Dbg( p_filter, "filter discarded (incompatible format)" );*/
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the module's structure */
    p_sys = p_filter->p_sys = malloc( sizeof(filter_sys_t) );
    if( p_sys == NULL )
        return VLC_EGENERIC;

    p_sys->b_downmix = var_InheritBool( p_this, MONO_CFG "downmix" );
    p_sys->i_channel_selected = var_InheritInteger( p_this, MONO_CFG "channel" );

    p_sys->i_nb_channels = aout_FormatNbChannels( &(p_filter->fmt_in.audio) );
    p_sys->i_bitspersample = p_filter->fmt_out.audio.i_bitspersample;

    p_sys->i_overflow_buffer_size = 0;
    p_sys->p_overflow_buffer = NULL;
    p_sys->i_nb_atomic_operations = 0;
    p_sys->p_atomic_operations = NULL;

    if( Init( VLC_OBJECT(p_filter), p_filter->p_sys,
              aout_FormatNbChannels( &p_filter->fmt_in.audio ),
              p_filter->fmt_in.audio.i_physical_channels,
              p_filter->fmt_in.audio.i_rate ) < 0 )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( p_sys->b_downmix )
    {
        msg_Dbg( p_this, "using stereo to mono downmix" );
        p_filter->fmt_out.audio.i_physical_channels = AOUT_CHAN_CENTER;
        p_filter->fmt_out.audio.i_channels = 1;
    }
    else
    {
        msg_Dbg( p_this, "using pseudo mono" );
        p_filter->fmt_out.audio.i_physical_channels = AOUT_CHANS_STEREO;
        p_filter->fmt_out.audio.i_channels = 2;
    }
    p_filter->fmt_out.audio.i_rate = p_filter->fmt_in.audio.i_rate;
    p_filter->pf_audio_filter = Convert;

    msg_Dbg( p_this, "%4.4s->%4.4s, channels %d->%d, bits per sample: %i->%i",
             (char *)&p_filter->fmt_in.i_codec,
             (char *)&p_filter->fmt_out.i_codec,
             p_filter->fmt_in.audio.i_physical_channels,
             p_filter->fmt_out.audio.i_physical_channels,
             p_filter->fmt_in.audio.i_bitspersample,
             p_filter->fmt_out.audio.i_bitspersample );

    p_filter->fmt_in.audio.i_format = VLC_CODEC_S16N;
    aout_FormatPrepare(&p_filter->fmt_in.audio);
    p_filter->fmt_out.audio.i_format = VLC_CODEC_S16N;
    aout_FormatPrepare(&p_filter->fmt_out.audio);

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseFilter
 *****************************************************************************/
static void CloseFilter( vlc_object_t *p_this)
{
    filter_t *p_filter = (filter_t *) p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    free( p_sys->p_atomic_operations );
    free( p_sys->p_overflow_buffer );
    free( p_sys );
}

/*****************************************************************************
 * Convert
 *****************************************************************************/
static block_t *Convert( filter_t *p_filter, block_t *p_block )
{
    block_t *p_out;
    int i_out_size;

    if( !p_block || !p_block->i_nb_samples )
    {
        if( p_block )
            block_Release( p_block );
        return NULL;
    }

    filter_sys_t *p_sys = p_filter->p_sys;
    i_out_size = p_block->i_nb_samples * p_sys->i_bitspersample/8 *
                 aout_FormatNbChannels( &(p_filter->fmt_out.audio) );

    p_out = block_Alloc( i_out_size );
    if( !p_out )
    {
        msg_Warn( p_filter, "can't get output buffer" );
        block_Release( p_block );
        return NULL;
    }
    p_out->i_nb_samples =
                  (p_block->i_nb_samples / p_sys->i_nb_channels) *
                       aout_FormatNbChannels( &(p_filter->fmt_out.audio) );

#if 0
    unsigned int i_in_size = in_buf.i_nb_samples  * (p_sys->i_bitspersample/8) *
                             aout_FormatNbChannels( &(p_filter->fmt_in.audio) );
    if( (in_buf.i_buffer != i_in_size) && ((i_in_size % 32) != 0) ) /* is it word aligned?? */
    {
        msg_Err( p_filter, "input buffer is not word aligned" );
        /* Fix output buffer to be word aligned */
    }
#endif

    memset( p_out->p_buffer, 0, i_out_size );
    if( p_sys->b_downmix )
    {
        stereo2mono_downmix( p_filter, p_block, p_out );
        mono( p_filter, p_out, p_block );
    }
    else
    {
        stereo_to_mono( p_filter, p_out, p_block );
    }

    block_Release( p_block );
    return p_out;
}

/* stereo2mono_downmix - stereo channels into one mono channel.
 * Code taken from modules/audio_filter/channel_mixer/headphone.c
 * converted from float into int16_t based downmix
 * Written by Boris Dorès <babal@via.ecp.fr>
 */
static void stereo2mono_downmix( filter_t * p_filter,
                                 block_t * p_in_buf, block_t * p_out_buf )
{
    filter_sys_t *p_sys = (filter_sys_t *)p_filter->p_sys;

    int i_input_nb = aout_FormatNbChannels( &p_filter->fmt_in.audio );
    int i_output_nb = aout_FormatNbChannels( &p_filter->fmt_out.audio );

    int16_t * p_in = (int16_t*) p_in_buf->p_buffer;
    uint8_t * p_out;
    uint8_t * p_overflow;
    uint8_t * p_slide;

    size_t i_overflow_size;     /* in bytes */
    size_t i_out_size;          /* in bytes */

    unsigned int i, j;

    int i_source_channel_offset;
    int i_dest_channel_offset;
    unsigned int i_delay;
    double d_amplitude_factor;

    /* out buffer characterisitcs */
    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_buffer = p_in_buf->i_buffer * i_output_nb / i_input_nb;
    p_out = p_out_buf->p_buffer;
    i_out_size = p_out_buf->i_buffer;

    /* Slide the overflow buffer */
    p_overflow = p_sys->p_overflow_buffer;
    i_overflow_size = p_sys->i_overflow_buffer_size;

    if ( i_out_size > i_overflow_size )
        memcpy( p_out, p_overflow, i_overflow_size );
    else
        memcpy( p_out, p_overflow, i_out_size );

    p_slide = p_sys->p_overflow_buffer;
    while( p_slide < p_overflow + i_overflow_size )
    {
        if( p_slide + i_out_size < p_overflow + i_overflow_size )
        {
            memset( p_slide, 0, i_out_size );
            if( p_slide + 2 * i_out_size < p_overflow + i_overflow_size )
                memcpy( p_slide, p_slide + i_out_size, i_out_size );
            else
                memcpy( p_slide, p_slide + i_out_size,
                        p_overflow + i_overflow_size - ( p_slide + i_out_size ) );
        }
        else
        {
            memset( p_slide, 0, p_overflow + i_overflow_size - p_slide );
        }
        p_slide += i_out_size;
    }

    /* apply the atomic operations */
    for( i = 0; i < p_sys->i_nb_atomic_operations; i++ )
    {
        /* shorter variable names */
        i_source_channel_offset
            = p_sys->p_atomic_operations[i].i_source_channel_offset;
        i_dest_channel_offset
            = p_sys->p_atomic_operations[i].i_dest_channel_offset;
        i_delay = p_sys->p_atomic_operations[i].i_delay;
        d_amplitude_factor
            = p_sys->p_atomic_operations[i].d_amplitude_factor;

        if( p_out_buf->i_nb_samples > i_delay )
        {
            /* current buffer coefficients */
            for( j = 0; j < p_out_buf->i_nb_samples - i_delay; j++ )
            {
                ((int16_t*)p_out)[ (i_delay+j)*i_output_nb + i_dest_channel_offset ]
                    += p_in[ j * i_input_nb + i_source_channel_offset ]
                       * d_amplitude_factor;
            }

            /* overflow buffer coefficients */
            for( j = 0; j < i_delay; j++ )
            {
                ((int16_t*)p_overflow)[ j*i_output_nb + i_dest_channel_offset ]
                    += p_in[ (p_out_buf->i_nb_samples - i_delay + j)
                       * i_input_nb + i_source_channel_offset ]
                       * d_amplitude_factor;
            }
        }
        else
        {
            /* overflow buffer coefficients only */
            for( j = 0; j < p_out_buf->i_nb_samples; j++ )
            {
                ((int16_t*)p_overflow)[ (i_delay - p_out_buf->i_nb_samples + j)
                                        * i_output_nb + i_dest_channel_offset ]
                    += p_in[ j * i_input_nb + i_source_channel_offset ]
                       * d_amplitude_factor;
            }
        }
    }
}

/* Simple stereo to mono mixing. */
static unsigned int mono( filter_t *p_filter,
                          block_t *p_output, block_t *p_input )
{
    filter_sys_t *p_sys = (filter_sys_t *)p_filter->p_sys;
    int16_t *p_in, *p_out;
    unsigned int n = 0, r = 0;

    p_in = (int16_t *) p_input->p_buffer;
    p_out = (int16_t *) p_output->p_buffer;

    while( n < (p_input->i_nb_samples * p_sys->i_nb_channels) )
    {
        p_out[r] = (p_in[n] + p_in[n+1]) >> 1;
        r++;
        n += 2;
    }
    return r;
}

/* Simple stereo to mono mixing. */
static unsigned int stereo_to_mono( filter_t *p_filter,
                                    block_t *p_output, block_t *p_input )
{
    filter_sys_t *p_sys = (filter_sys_t *)p_filter->p_sys;
    int16_t *p_in, *p_out;
    unsigned int n;

    p_in = (int16_t *) p_input->p_buffer;
    p_out = (int16_t *) p_output->p_buffer;

    for( n = 0; n < (p_input->i_nb_samples * p_sys->i_nb_channels); n++ )
    {
        /* Fake real mono. */
        if( p_sys->i_channel_selected == -1)
        {
            p_out[n] = p_out[n+1] = (p_in[n] + p_in[n+1]) >> 1;
            n++;
        }
        else if( (n % p_sys->i_nb_channels) == (unsigned int) p_sys->i_channel_selected )
        {
            p_out[n] = p_out[n+1] = p_in[n];
        }
    }
    return n;
}
