/*****************************************************************************
 * coreaudio.c resampler based on CoreAudio's AudioConverter
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
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

#include <AudioToolbox/AudioConverter.h>

#include <vlc/vlc.h>
#include "audio_output.h"
#include "aout_internal.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Close     ( vlc_object_t * );

static void DoWork    ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                        aout_buffer_t * );

/*****************************************************************************
 * Local structures
 *****************************************************************************/
struct aout_filter_sys_t
{
    aout_filter_t * p_secondary_resampler;
    aout_alloc_t alloc;

    AudioStreamBasicDescription s_src_stream_format;
    AudioStreamBasicDescription s_dst_stream_format;
    AudioConverterRef   s_converter;
    unsigned int i_remainder;
    unsigned int i_first_rate;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("audio filter using CoreAudio for resampling") );
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_MISC );
    set_capability( "audio filter", 40 );
    set_callbacks( Create, Close );
vlc_module_end();

/*****************************************************************************
 * Create: allocate resampler
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;
    struct aout_filter_sys_t * p_sys = p_filter->p_sys;
    unsigned int i_nb_channels;
    OSStatus err;
    uint32_t i_prop;
    unsigned int i_first_rate;

    if ( p_filter->input.i_rate == p_filter->output.i_rate
          || p_filter->input.i_format != p_filter->output.i_format
          || p_filter->input.i_physical_channels
              != p_filter->output.i_physical_channels
          || p_filter->input.i_original_channels
              != p_filter->output.i_original_channels
          || p_filter->input.i_format != VLC_FOURCC('f','l','3','2') )
    {
        return VLC_EGENERIC;
    }

    if ( p_filter->input.i_rate >= 48000 * (100 + AOUT_MAX_RESAMPLING) / 100 )
        i_first_rate = 48000;
    else
        i_first_rate = 44100;
    if ( p_filter->output.i_rate == i_first_rate )
    {
        return VLC_EGENERIC;
    }

    i_nb_channels = aout_FormatNbChannels( &p_filter->input );

    /* Allocate the memory needed to store the module's structure */
    p_sys = p_filter->p_sys = malloc( sizeof(struct aout_filter_sys_t) );
    if( p_filter->p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }
    memset( p_filter->p_sys, 0, sizeof(struct aout_filter_sys_t) );
    p_sys->i_first_rate = i_first_rate;
    p_sys->i_remainder = 0;

    p_sys->s_src_stream_format.mFormatID = kAudioFormatLinearPCM;
    p_sys->s_src_stream_format.mFormatFlags
        = kLinearPCMFormatFlagIsFloat | kAudioFormatFlagsNativeEndian
          | kAudioFormatFlagIsPacked;
    p_sys->s_src_stream_format.mBytesPerPacket = i_nb_channels * 4;
    p_sys->s_src_stream_format.mFramesPerPacket = 1;
    p_sys->s_src_stream_format.mBytesPerFrame = i_nb_channels * 4;
    p_sys->s_src_stream_format.mChannelsPerFrame = i_nb_channels;
    p_sys->s_src_stream_format.mBitsPerChannel = 32;

    memcpy( &p_sys->s_dst_stream_format, &p_sys->s_src_stream_format,
            sizeof(AudioStreamBasicDescription) );

    p_sys->s_src_stream_format.mSampleRate = p_sys->i_first_rate;
    p_sys->s_dst_stream_format.mSampleRate = p_filter->output.i_rate;

    err = AudioConverterNew( &p_sys->s_src_stream_format,
                             &p_sys->s_dst_stream_format,
                             &p_sys->s_converter );

    if( err != noErr )
    {
        msg_Err( p_filter, "AudioConverterNew failed: [%4.4s]",
                 (char *)&err );
        free(p_sys);
        return VLC_EGENERIC;
    }

    i_prop = kConverterPrimeMethod_None;
    err = AudioConverterSetProperty( p_sys->s_converter,
            kAudioConverterPrimeMethod, sizeof(i_prop), &i_prop );

    if( err != noErr )
    {
        msg_Err( p_filter, "AudioConverterSetProperty failed: [%4.4s]",
                 (char *)&err );
        free(p_sys);
        return VLC_EGENERIC;
    }

    /* Allocate a secondary resampler for the remainder. */
    p_sys->p_secondary_resampler = vlc_object_create( p_filter,
                                                  sizeof(aout_filter_t) );     
    if ( p_sys->p_secondary_resampler == NULL )
    {
        free(p_sys);
        return VLC_EGENERIC;
    }
    vlc_object_attach( p_sys->p_secondary_resampler, p_filter );

    memcpy( &p_sys->p_secondary_resampler->input, &p_filter->input, 
            sizeof(audio_sample_format_t) );
    memcpy( &p_sys->p_secondary_resampler->output, &p_filter->output, 
            sizeof(audio_sample_format_t) );
    p_sys->p_secondary_resampler->p_module
        = module_Need( p_sys->p_secondary_resampler, "audio filter",
                       "ugly_resampler", VLC_TRUE );
    if ( p_sys->p_secondary_resampler->p_module == NULL )
    {
        vlc_object_detach( p_sys->p_secondary_resampler );
        vlc_object_destroy( p_sys->p_secondary_resampler );
        free(p_sys);
        return VLC_EGENERIC;
    }
    p_sys->p_secondary_resampler->b_continuity = VLC_FALSE;
    p_sys->alloc.i_alloc_type = AOUT_ALLOC_STACK;
    p_sys->alloc.i_bytes_per_sec = p_filter->output.i_bytes_per_frame
                             * p_filter->output.i_rate
                             / p_filter->output.i_frame_length;

    p_filter->pf_do_work = DoWork;

    /* We don't want a new buffer to be created because we're not sure we'll
     * actually need to resample anything. */
    p_filter->b_in_place = VLC_FALSE;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free our resources
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;
    struct aout_filter_sys_t * p_sys = p_filter->p_sys;
    OSErr err;

    module_Unneed( p_sys->p_secondary_resampler,
                   p_sys->p_secondary_resampler->p_module );
    vlc_object_detach( p_sys->p_secondary_resampler );
    vlc_object_destroy( p_sys->p_secondary_resampler );

    /* Destroy the AudioConverter */
    err = AudioConverterDispose( p_sys->s_converter );

    if( err != noErr )
    {
        msg_Err( p_this, "AudioConverterDispose failed: %u", err );
    }

    free( p_filter->p_sys );
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    struct aout_filter_sys_t * p_sys = p_filter->p_sys;
    int32_t *p_in = (int32_t *)p_in_buf->p_buffer;
    int32_t *p_out;
    UInt32 i_output_size;
    unsigned int i_out_nb, i_wanted_nb, i_new_rate;
    OSErr err;
    aout_buffer_t * p_middle_buf;

    unsigned int i_nb_channels = aout_FormatNbChannels( &p_filter->input );

#if 1
    if ( !p_filter->b_continuity )
    {
        err = AudioConverterReset( p_sys->s_converter );
        if( err != noErr )
        {
            msg_Err( p_filter, "AudioConverterReset failed: [%4.4s]",
                     (char *)&err );
        }
        p_filter->b_continuity = VLC_TRUE;
        p_sys->i_remainder = 0;
    }
#endif

    i_out_nb = (p_in_buf->i_nb_samples * p_filter->output.i_rate
                 + p_sys->i_remainder) / p_sys->i_first_rate;
    p_sys->i_remainder = (p_in_buf->i_nb_samples * p_filter->output.i_rate
                 + p_sys->i_remainder) % p_sys->i_first_rate;

    i_output_size = i_out_nb * 4 * i_nb_channels;
    if ( i_output_size > p_out_buf->i_size )
    {
        aout_BufferAlloc( &p_sys->alloc,
            i_out_nb * 1000000 / p_filter->output.i_rate,
            NULL, p_middle_buf );
    }
    else
    {
        p_middle_buf = p_out_buf;
    }
    p_out = (int32_t*)p_middle_buf->p_buffer;
    err = AudioConverterConvertBuffer( p_sys->s_converter,
        p_in_buf->i_nb_samples * 4 * i_nb_channels, p_in,
        &i_output_size, p_out );
    if( err != noErr )
    {
        msg_Warn( p_filter, "AudioConverterConvertBuffer failed: [%4.4s] (%u:%u)",
                 (char *)&err, i_out_nb * 4 * i_nb_channels, i_output_size );
        i_output_size = i_out_nb * 4 * i_nb_channels;
        memset( p_out, 0, i_output_size );
    }

    p_middle_buf->i_nb_samples = i_output_size / 4 / i_nb_channels;
    p_middle_buf->i_nb_bytes = i_output_size;
    p_middle_buf->start_date = p_in_buf->start_date;
    p_middle_buf->end_date = p_middle_buf->start_date + p_middle_buf->i_nb_samples *
        1000000 / p_filter->output.i_rate;

    i_wanted_nb = p_in_buf->i_nb_samples * p_filter->output.i_rate
                                        / p_filter->input.i_rate;
    i_new_rate = p_middle_buf->i_nb_samples * p_filter->output.i_rate
                                        / i_wanted_nb;

    p_sys->p_secondary_resampler->input.i_rate = i_new_rate;
    p_sys->p_secondary_resampler->pf_do_work( p_aout,
        p_sys->p_secondary_resampler, p_middle_buf, p_out_buf );

    if ( p_middle_buf != p_out_buf )
    {
        aout_BufferFree( p_middle_buf );
    }
}
