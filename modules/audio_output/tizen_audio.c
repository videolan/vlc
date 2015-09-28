/*****************************************************************************
 * tizen_audio.c: Tizen audio output module
 *****************************************************************************
 * Copyright © 2015 VLC authors, VideoLAN and VideoLabs
 *
 * Authors: Thomas Guillem <thomas@gllm.fr>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <dlfcn.h>
#include <stdbool.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>

#include "audio_io.h"
#include "sound_manager.h"

#if TIZEN_SDK_MAJOR >= 3 || (TIZEN_SDK_MAJOR >= 2 && TIZEN_SDK_MINOR >= 4)
#define AUDIO_IO_HAS_FLUSH
#endif

static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );

struct aout_sys_t {
    /* sw gain */
    float               soft_gain;
    bool                soft_mute;

    audio_out_h         out;
    bool                b_prepared;

    unsigned int        i_rate;
    audio_sample_type_e i_sample_type;
    audio_channel_e     i_channel;
};

/* Soft volume helper */
#include "audio_output/volume.h"

vlc_module_begin ()
    set_shortname( "Tizen audio" )
    set_description( N_( "Tizen audio output" ) )
    set_capability( "audio output", 180 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )
    add_sw_gain()
    add_shortcut( "tizen" )
    set_callbacks( Open, Close )
vlc_module_end ()

static const char *
AudioIO_Err2Str( audio_io_error_e e )
{
    switch( e )
    {
    case AUDIO_IO_ERROR_NONE:
        return "AUDIO_IO_ERROR_NONE";
    case AUDIO_IO_ERROR_OUT_OF_MEMORY:
        return "AUDIO_IO_ERROR_OUT_OF_MEMORY";
    case AUDIO_IO_ERROR_INVALID_PARAMETER:
        return "AUDIO_IO_ERROR_INVALID_PARAMETER";
    case AUDIO_IO_ERROR_INVALID_OPERATION:
        return "AUDIO_IO_ERROR_INVALID_OPERATION";
    case AUDIO_IO_ERROR_PERMISSION_DENIED:
        return "AUDIO_IO_ERROR_PERMISSION_DENIED";
    case AUDIO_IO_ERROR_NOT_SUPPORTED:
        return "AUDIO_IO_ERROR_NOT_SUPPORTED";
    case AUDIO_IO_ERROR_DEVICE_NOT_OPENED:
        return "AUDIO_IO_ERROR_DEVICE_NOT_OPENED";
    case AUDIO_IO_ERROR_DEVICE_NOT_CLOSED:
        return "AUDIO_IO_ERROR_DEVICE_NOT_CLOSED";
    case AUDIO_IO_ERROR_INVALID_BUFFER:
        return "AUDIO_IO_ERROR_INVALID_BUFFER";
    case AUDIO_IO_ERROR_SOUND_POLICY:
        return "AUDIO_IO_ERROR_SOUND_POLICY";
    default:
        return "UNKNOWN_ERROR";
    }
}

static int
AudioIO_VlcRet( audio_output_t *p_aout, const char *p_func, int i_ret )
{
    if( i_ret != AUDIO_IO_ERROR_NONE )
    {
        aout_sys_t *p_sys = p_aout->sys;

        msg_Err( p_aout, "%s failed: 0x%X, %s", p_func,
                 i_ret, AudioIO_Err2Str( i_ret ) );
        audio_out_destroy( p_sys->out );
        p_sys->out = NULL;
        return VLC_EGENERIC;
    }
    else
        return VLC_SUCCESS;
}
#define VLCRET( func ) AudioIO_VlcRet( p_aout, #func, func )

static int
AudioIO_Prepare( audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;

    if( !p_sys->b_prepared )
    {
        if( VLCRET( audio_out_prepare( p_sys->out ) ) )
            return VLC_EGENERIC;
        p_sys->b_prepared = true;
    }
    return VLC_SUCCESS;
}

static int
AudioIO_Unprepare( audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;

    if( p_sys->b_prepared )
    {
        p_sys->b_prepared = false;

        /* Unlocked to avoid deadlock with AudioIO_StreamCb */
        if( VLCRET( audio_out_unprepare( p_sys->out ) ) )
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int
AudioIO_Start( audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;

    /* Out create */
    return VLCRET( audio_out_create( p_sys->i_rate, p_sys->i_channel,
                                     p_sys->i_sample_type, SOUND_TYPE_MEDIA,
                                     &p_sys->out ) );
}

static int
Start( audio_output_t *p_aout, audio_sample_format_t *restrict p_fmt )
{
    aout_sys_t *p_sys = p_aout->sys;

    aout_FormatPrint( p_aout, "Tizen audio is looking for:", p_fmt );

    /* Sample rate: tizen accept rate between 8000 and 48000 Hz */
    p_sys->i_rate = p_fmt->i_rate = VLC_CLIP( p_fmt->i_rate, 8000, 48000 );

    /* Channel */
    switch( p_fmt->i_physical_channels )
    {
    case AOUT_CHAN_LEFT:
        p_sys->i_channel = AUDIO_CHANNEL_MONO;
        break;
    default:
    case AOUT_CHANS_STEREO:
        p_fmt->i_physical_channels = AOUT_CHANS_STEREO;
        p_sys->i_channel = AUDIO_CHANNEL_STEREO;
        break;
    }

    /* Sample type */
    switch( p_fmt->i_format )
    {
    case VLC_CODEC_U8:
        p_sys->i_sample_type = AUDIO_SAMPLE_TYPE_U8;
        break;
    default:
    case VLC_CODEC_S16N:
        p_fmt->i_format = VLC_CODEC_S16N;
        p_sys->i_sample_type = AUDIO_SAMPLE_TYPE_S16_LE;
        break;
    }

    if( AudioIO_Start( p_aout ) != VLC_SUCCESS )
        return VLC_EGENERIC;

    aout_FormatPrepare( p_fmt );
    aout_SoftVolumeStart( p_aout );

    aout_FormatPrint( p_aout, "Tizen audio will output:", p_fmt );

    return VLC_SUCCESS;
}

static void
Stop( audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;

    if( p_sys->out)
    {
        AudioIO_Unprepare( p_aout );
        audio_out_destroy( p_sys->out );
        p_sys->out = NULL;
    }

    p_sys->i_rate = 0;
    p_sys->i_channel = 0;
    p_sys->i_sample_type = 0;
}

static void
Play( audio_output_t *p_aout, block_t *p_block )
{
    aout_sys_t *p_sys = p_aout->sys;

    if( !p_sys->out || AudioIO_Prepare( p_aout ) )
    {
        block_Release( p_block );
        return;
    }

    while( p_block )
    {
        int i_ret = audio_out_write( p_sys->out, p_block->p_buffer, p_block->i_buffer );
        if( i_ret < 0 )
        {
            AudioIO_VlcRet( p_aout, "audio_out_write", i_ret );
            block_Release( p_block );
            p_block = NULL;
        }
        else
        {
            p_block->i_buffer -= i_ret;
            p_block->p_buffer += i_ret;
            if( !p_block->i_buffer )
            {
                block_Release( p_block );
                p_block = NULL;
            }
        }
    }
}

static void
Pause( audio_output_t *p_aout, bool b_pause, mtime_t i_date )
{
    aout_sys_t *p_sys = p_aout->sys;
    (void) i_date;

    if( !p_sys->out )
        return;

    if( b_pause )
        AudioIO_Unprepare( p_aout );
}

static void
Flush( audio_output_t *p_aout, bool b_wait )
{
    aout_sys_t *p_sys = p_aout->sys;

    if( !p_sys->out )
        return;

#ifdef AUDIO_IO_HAS_FLUSH
    if( b_wait )
        VLCRET( audio_out_drain( p_aout ) );
    else
        VLCRET( audio_out_flush( p_aout ) );
#else
    (void) b_wait;
    if( AudioIO_Unprepare( p_aout ) )
        return;
    audio_out_destroy( p_sys->out );
    p_sys->out = NULL;
    AudioIO_Start( p_aout );
#endif
}

static int
Open( vlc_object_t *obj )
{
    audio_output_t *p_aout = (audio_output_t *) obj;
    aout_sys_t *p_sys;

    p_sys = calloc( 1, sizeof (aout_sys_t) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    p_aout->sys = p_sys;
    p_aout->start = Start;
    p_aout->stop = Stop;
    p_aout->play = Play;
    p_aout->pause = Pause;
    p_aout->flush = Flush;
    /* p_aout->time_get = TimeGet; FIXME */

    aout_SoftVolumeInit( p_aout );

    return VLC_SUCCESS;
}

static void
Close( vlc_object_t *obj )
{
    audio_output_t *p_aout = (audio_output_t *) obj;
    aout_sys_t *p_sys = p_aout->sys;

    free( p_sys );
}
