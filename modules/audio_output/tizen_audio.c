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
#include <vlc_atomic.h>

#include "audio_io.h"
#include "sound_manager.h"

static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );

struct aout_sys_t {
    /* sw gain */
    float               soft_gain;
    bool                soft_mute;

    audio_out_h         out;
    bool                b_prepared;
    bool                b_error;
    atomic_bool         interrupted_completed;

    unsigned int        i_rate;
    audio_sample_type_e i_sample_type;
    audio_channel_e     i_channel;

    int (*pf_audio_out_drain)( audio_out_h output );
    int (*pf_audio_out_flush)( audio_out_h output );
};

/* Soft volume helper */
#include "audio_output/volume.h"

vlc_module_begin ()
    set_shortname( "Tizen audio" )
    set_description( "Tizen audio output" )
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

        /* Error could be recoverable if audio_out was interrupted. */
        p_sys->b_error = true;
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

    /* if no more interrupted, cancel error and try again */
    if( atomic_exchange( &p_sys->interrupted_completed, false ) )
        p_sys->b_error = false;

    if( p_sys->b_error )
        return VLC_EGENERIC;

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

static void
AudioIO_InterruptedCb(audio_io_interrupted_code_e code, void *p_user_data)
{
    audio_output_t *p_aout = p_user_data;
    aout_sys_t *p_sys = p_aout->sys;

    if( code == AUDIO_IO_INTERRUPTED_COMPLETED
     || code ==  AUDIO_IO_INTERRUPTED_BY_EARJACK_UNPLUG )
    {
        msg_Warn( p_aout, "audio_out interrupted completed by %d", code);
        atomic_store( &p_sys->interrupted_completed, true );
    }
    else
    {
        msg_Warn( p_aout, "audio_out interrupted by %d", code);
        atomic_store( &p_sys->interrupted_completed, false );
    }
}

static int
AudioIO_Start( audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;

    /* Out create */
    if( VLCRET( audio_out_create( p_sys->i_rate, p_sys->i_channel,
                                  p_sys->i_sample_type, SOUND_TYPE_MEDIA,
                                  &p_sys->out ) ) )
        return VLC_EGENERIC;
    return VLCRET( audio_out_set_interrupted_cb( p_sys->out,
                                                 AudioIO_InterruptedCb,
                                                 p_aout ) );
}

static int
Start( audio_output_t *p_aout, audio_sample_format_t *restrict p_fmt )
{
    aout_sys_t *p_sys = p_aout->sys;

    if( aout_FormatNbChannels( p_fmt ) == 0 )
        return VLC_EGENERIC;

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

    p_fmt->channel_type = AUDIO_CHANNEL_TYPE_BITMAP;

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
        audio_out_unset_interrupted_cb( p_sys->out );
        audio_out_destroy( p_sys->out );
        p_sys->out = NULL;
    }
    p_sys->b_error = false;
    atomic_store( &p_sys->interrupted_completed, false );

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

    if( p_sys->pf_audio_out_drain || p_sys->pf_audio_out_flush )
    {
        if( b_wait )
            VLCRET( p_sys->pf_audio_out_drain( p_sys->out ) );
        else
            VLCRET( p_sys->pf_audio_out_flush( p_sys->out ) );
    }
    else
    {
        (void) b_wait;
        if( AudioIO_Unprepare( p_aout ) )
            return;
        audio_out_unset_interrupted_cb( p_sys->out );
        audio_out_destroy( p_sys->out );
        p_sys->out = NULL;
        AudioIO_Start( p_aout );
    }
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

    /* Available only on 2.4 */
    p_sys->pf_audio_out_drain = dlsym( RTLD_DEFAULT, "audio_out_drain" );
    p_sys->pf_audio_out_flush = dlsym( RTLD_DEFAULT, "audio_out_flush" );

    aout_SoftVolumeInit( p_aout );

    atomic_init( &p_sys->interrupted_completed, false );

    return VLC_SUCCESS;
}

static void
Close( vlc_object_t *obj )
{
    audio_output_t *p_aout = (audio_output_t *) obj;
    aout_sys_t *p_sys = p_aout->sys;

    free( p_sys );
}
