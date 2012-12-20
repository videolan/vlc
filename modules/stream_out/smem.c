/*****************************************************************************
 * smem.c: stream output to memory buffer module
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Courtaut <christophe.courtaut@gmail.com>
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
 * How to use it
 *****************************************************************************
 *
 * You should use this module in combination with the transcode module, to get
 * raw datas from it. This module does not make any conversion at all, so you
 * need to use the transcode module for this purpose.
 *
 * For example, you can use smem as it :
 * --sout="#transcode{vcodec=RV24,acodec=s16l}:smem{smem-options}"
 *
 * Into each lock function (audio and video), you will have all the information
 * you need to allocate a buffer, so that this module will copy data in it.
 *
 * the video-data and audio-data pointers will be passed to lock/unlock function
 *
 ******************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_codec.h>
#include <vlc_aout.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define T_VIDEO_PRERENDER_CALLBACK N_( "Video prerender callback" )
#define LT_VIDEO_PRERENDER_CALLBACK N_( "Address of the video prerender callback function. " \
                                "This function will set the buffer where render will be done." )

#define T_AUDIO_PRERENDER_CALLBACK N_( "Audio prerender callback" )
#define LT_AUDIO_PRERENDER_CALLBACK N_( "Address of the audio prerender callback function. " \
                                        "This function will set the buffer where render will be done." )

#define T_VIDEO_POSTRENDER_CALLBACK N_( "Video postrender callback" )
#define LT_VIDEO_POSTRENDER_CALLBACK N_( "Address of the video postrender callback function. " \
                                        "This function will be called when the render is into the buffer." )

#define T_AUDIO_POSTRENDER_CALLBACK N_( "Audio postrender callback" )
#define LT_AUDIO_POSTRENDER_CALLBACK N_( "Address of the audio postrender callback function. " \
                                        "This function will be called when the render is into the buffer." )

#define T_VIDEO_DATA N_( "Video Callback data" )
#define LT_VIDEO_DATA N_( "Data for the video callback function." )

#define T_AUDIO_DATA N_( "Audio callback data" )
#define LT_AUDIO_DATA N_( "Data for the audio callback function." )

#define T_TIME_SYNC N_( "Time Synchronized output" )
#define LT_TIME_SYNC N_( "Time Synchronisation option for output. " \
                        "If true, stream will render as usual, else " \
                        "it will be rendered as fast as possible.")

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-smem-"
#define SOUT_PREFIX_VIDEO SOUT_CFG_PREFIX"video-"
#define SOUT_PREFIX_AUDIO SOUT_CFG_PREFIX"audio-"

vlc_module_begin ()
    set_shortname( N_("Smem"))
    set_description( N_("Stream output to memory buffer") )
    set_capability( "sout stream", 0 )
    add_shortcut( "smem" )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_STREAM )
    add_string( SOUT_PREFIX_VIDEO "prerender-callback", "0", T_VIDEO_PRERENDER_CALLBACK, LT_VIDEO_PRERENDER_CALLBACK, true )
        change_volatile()
    add_string( SOUT_PREFIX_AUDIO "prerender-callback", "0", T_AUDIO_PRERENDER_CALLBACK, LT_AUDIO_PRERENDER_CALLBACK, true )
        change_volatile()
    add_string( SOUT_PREFIX_VIDEO "postrender-callback", "0", T_VIDEO_POSTRENDER_CALLBACK, LT_VIDEO_POSTRENDER_CALLBACK, true )
        change_volatile()
    add_string( SOUT_PREFIX_AUDIO "postrender-callback", "0", T_AUDIO_POSTRENDER_CALLBACK, LT_AUDIO_POSTRENDER_CALLBACK, true )
        change_volatile()
    add_string( SOUT_PREFIX_VIDEO "data", "0", T_VIDEO_DATA, LT_VIDEO_DATA, true )
        change_volatile()
    add_string( SOUT_PREFIX_AUDIO "data", "0", T_AUDIO_DATA, LT_VIDEO_DATA, true )
        change_volatile()
    add_bool( SOUT_CFG_PREFIX "time-sync", true, T_TIME_SYNC, LT_TIME_SYNC, true )
        change_private()
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *const ppsz_sout_options[] = {
    "video-prerender-callback", "audio-prerender-callback",
    "video-postrender-callback", "audio-postrender-callback", "video-data", "audio-data", "time-sync", NULL
};

static sout_stream_id_t *Add ( sout_stream_t *, es_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *, block_t* );

static sout_stream_id_t *AddVideo( sout_stream_t *p_stream, es_format_t *p_fmt );
static sout_stream_id_t *AddAudio( sout_stream_t *p_stream, es_format_t *p_fmt );

static int SendVideo( sout_stream_t *p_stream, sout_stream_id_t *id,
                      block_t *p_buffer );
static int SendAudio( sout_stream_t *p_stream, sout_stream_id_t *id,
                      block_t *p_buffer );

struct sout_stream_id_t
{
    es_format_t* format;
    void *p_data;
};

struct sout_stream_sys_t
{
    vlc_mutex_t *p_lock;
    void ( *pf_video_prerender_callback ) ( void* p_video_data, uint8_t** pp_pixel_buffer , int size );
    void ( *pf_audio_prerender_callback ) ( void* p_audio_data, uint8_t** pp_pcm_buffer , unsigned int size );
    void ( *pf_video_postrender_callback ) ( void* p_video_data, uint8_t* p_pixel_buffer, int width, int height, int pixel_pitch, int size, mtime_t pts );
    void ( *pf_audio_postrender_callback ) ( void* p_audio_data, uint8_t* p_pcm_buffer, unsigned int channels, unsigned int rate, unsigned int nb_samples, unsigned int bits_per_sample, unsigned int size, mtime_t pts );
    bool time_sync;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    char* psz_tmp;
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;

    p_sys = calloc( 1, sizeof( sout_stream_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;
    p_stream->p_sys = p_sys;

    config_ChainParse( p_stream, SOUT_CFG_PREFIX, ppsz_sout_options,
                       p_stream->p_cfg );

    p_sys->time_sync = var_GetBool( p_stream, SOUT_CFG_PREFIX "time-sync" );

    psz_tmp = var_GetString( p_stream, SOUT_PREFIX_VIDEO "prerender-callback" );
    p_sys->pf_video_prerender_callback = (void (*) (void *, uint8_t**, int))(intptr_t)atoll( psz_tmp );
    free( psz_tmp );

    psz_tmp = var_GetString( p_stream, SOUT_PREFIX_AUDIO "prerender-callback" );
    p_sys->pf_audio_prerender_callback = (void (*) (void* , uint8_t**, unsigned int))(intptr_t)atoll( psz_tmp );
    free( psz_tmp );

    psz_tmp = var_GetString( p_stream, SOUT_PREFIX_VIDEO "postrender-callback" );
    p_sys->pf_video_postrender_callback = (void (*) (void*, uint8_t*, int, int, int, int, mtime_t))(intptr_t)atoll( psz_tmp );
    free( psz_tmp );

    psz_tmp = var_GetString( p_stream, SOUT_PREFIX_AUDIO "postrender-callback" );
    p_sys->pf_audio_postrender_callback = (void (*) (void*, uint8_t*, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, mtime_t))(intptr_t)atoll( psz_tmp );
    free( psz_tmp );

    /* Setting stream out module callbacks */
    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;
    p_stream->pace_nocontrol = p_sys->time_sync;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t *p_stream = (sout_stream_t*)p_this;
    free( p_stream->p_sys );
}

static sout_stream_id_t *Add( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_stream_id_t *id = NULL;

    if ( p_fmt->i_cat == VIDEO_ES )
        id = AddVideo( p_stream, p_fmt );
    else if ( p_fmt->i_cat == AUDIO_ES )
        id = AddAudio( p_stream, p_fmt );
    return id;
}

static sout_stream_id_t *AddVideo( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    char* psz_tmp;
    sout_stream_id_t    *id;
    int i_bits_per_pixel;

    switch( p_fmt->i_codec )
    {
        case VLC_CODEC_RGB32:
        case VLC_CODEC_RGBA:
            i_bits_per_pixel = 32;
            break;
        case VLC_CODEC_I444:
        case VLC_CODEC_RGB24:
            i_bits_per_pixel = 24;
            break;
        case VLC_CODEC_RGB16:
        case VLC_CODEC_RGB15:
        case VLC_CODEC_RGB8:
        case VLC_CODEC_I422:
            i_bits_per_pixel = 16;
            break;
        case VLC_CODEC_YV12:
        case VLC_CODEC_I420:
            i_bits_per_pixel = 12;
            break;
        case VLC_CODEC_RGBP:
            i_bits_per_pixel = 8;
            break;
        default:
            i_bits_per_pixel = 0;
            msg_Dbg( p_stream, "non raw video format detected (%4.4s), buffers will contain compressed video", (char *)&p_fmt->i_codec );
            break;
    }

    id = calloc( 1, sizeof( sout_stream_id_t ) );
    if( !id )
        return NULL;

    psz_tmp = var_GetString( p_stream, SOUT_PREFIX_VIDEO "data" );
    id->p_data = (void *)( intptr_t )atoll( psz_tmp );
    free( psz_tmp );

    id->format = p_fmt;
    id->format->video.i_bits_per_pixel = i_bits_per_pixel;
    return id;
}

static sout_stream_id_t *AddAudio( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    char* psz_tmp;
    sout_stream_id_t* id;
    int i_bits_per_sample = aout_BitsPerSample( p_fmt->i_codec );

    if( !i_bits_per_sample )
    {
        msg_Err( p_stream, "Smem does only support raw audio format" );
        return NULL;
    }

    id = calloc( 1, sizeof( sout_stream_id_t ) );
    if( !id )
        return NULL;

    psz_tmp = var_GetString( p_stream, SOUT_PREFIX_AUDIO "data" );
    id->p_data = (void *)( intptr_t )atoll( psz_tmp );
    free( psz_tmp );

    id->format = p_fmt;
    id->format->audio.i_bitspersample = i_bits_per_sample;
    return id;
}

static int Del( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    VLC_UNUSED( p_stream );
    free( id );
    return VLC_SUCCESS;
}

static int Send( sout_stream_t *p_stream, sout_stream_id_t *id,
                 block_t *p_buffer )
{
    if ( id->format->i_cat == VIDEO_ES )
        return SendVideo( p_stream, id, p_buffer );
    else if ( id->format->i_cat == AUDIO_ES )
        return SendAudio( p_stream, id, p_buffer );
    return VLC_SUCCESS;
}

static int SendVideo( sout_stream_t *p_stream, sout_stream_id_t *id,
                      block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    int i_line, i_line_size, i_size, i_pixel_pitch;
    uint8_t* p_pixels = NULL;

    if( id->format->video.i_bits_per_pixel > 0 )
    {
        i_line = id->format->video.i_height;
        i_pixel_pitch = id->format->video.i_bits_per_pixel / 8;
        i_line_size = i_pixel_pitch * id->format->video.i_width;
        i_size = i_line * i_line_size;
    }
    else
    {
        i_size = p_buffer->i_buffer;
    }
    /* Calling the prerender callback to get user buffer */
    p_sys->pf_video_prerender_callback( id->p_data, &p_pixels , i_size );

    if (!p_pixels)
    {
        msg_Err( p_stream, "No buffer given!" );
        block_ChainRelease( p_buffer );
        return VLC_EGENERIC;
    }

    /* Copying data into user buffer */
    if( id->format->video.i_bits_per_pixel > 0 )
    {
        uint8_t *p_in = p_buffer->p_buffer;
        uint8_t *p_out = p_pixels;

        for ( int line = 0; line < i_line; line++ )
        {
            memcpy( p_out, p_in, i_line_size );
            p_out += i_line_size;
            p_in += i_line_size;
        }
    }
    else
    {
        memcpy( p_pixels, p_buffer->p_buffer, i_size );
    }
    /* Calling the postrender callback to tell the user his buffer is ready */
    p_sys->pf_video_postrender_callback( id->p_data, p_pixels,
                                         id->format->video.i_width, id->format->video.i_height,
                                         id->format->video.i_bits_per_pixel, i_size, p_buffer->i_pts );
    block_ChainRelease( p_buffer );
    return VLC_SUCCESS;
}

static int SendAudio( sout_stream_t *p_stream, sout_stream_id_t *id,
                      block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    int i_size;
    uint8_t* p_pcm_buffer = NULL;
    int i_samples = 0;

    i_size = p_buffer->i_buffer;
    if (id->format->audio.i_channels <= 0)
    {
        msg_Warn( p_stream, "No buffer given!" );
        block_ChainRelease( p_buffer );
        return VLC_EGENERIC;
    }

    i_samples = i_size / ( ( id->format->audio.i_bitspersample / 8 ) * id->format->audio.i_channels );
    /* Calling the prerender callback to get user buffer */
    p_sys->pf_audio_prerender_callback( id->p_data, &p_pcm_buffer, i_size );
    if (!p_pcm_buffer)
    {
        msg_Err( p_stream, "No buffer given!" );
        block_ChainRelease( p_buffer );
        return VLC_EGENERIC;
    }

    /* Copying data into user buffer */
    memcpy( p_pcm_buffer, p_buffer->p_buffer, i_size );
    /* Calling the postrender callback to tell the user his buffer is ready */
    p_sys->pf_audio_postrender_callback( id->p_data, p_pcm_buffer,
                                         id->format->audio.i_channels, id->format->audio.i_rate, i_samples,
                                         id->format->audio.i_bitspersample, i_size, p_buffer->i_pts );
    block_ChainRelease( p_buffer );
    return VLC_SUCCESS;
}

