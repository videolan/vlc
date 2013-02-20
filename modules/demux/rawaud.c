/*****************************************************************************
 * rawaud.c : raw audio input module for vlc
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jarmo Torvinen <jarmo.torvinen@jutel.fi>
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
#include <vlc_demux.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );


#define SAMPLERATE_TEXT N_("Audio samplerate (Hz)")
#define SAMPLERATE_LONGTEXT N_("Audio sample rate in Hertz. Default is 48000 Hz.")

#define CHANNELS_TEXT N_("Audio channels")
#define CHANNELS_LONGTEXT N_("Audio channels in input stream. Numeric value >0. Default is 2.")

#define FOURCC_TEXT N_("FOURCC code of raw input format")
#define FOURCC_LONGTEXT N_( \
    "FOURCC code of the raw input format. This is a four character string." )

#define LANG_TEXT N_("Forces the audio language")
#define LANG_LONGTEXT N_("Forces the audio language for the output mux. Three letter ISO639 code. Default is 'eng'. ")

#ifdef WORDS_BIGENDIAN
# define FOURCC_DEFAULT "s16b"
#else
# define FOURCC_DEFAULT "s16l"
#endif

vlc_module_begin();
    set_shortname( "Raw Audio" );
    set_description( N_("Raw audio demuxer") );
    set_capability( "demux", 0 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_callbacks( Open, Close );
    add_shortcut( "rawaud" );
    add_integer( "rawaud-channels", 2, CHANNELS_TEXT, CHANNELS_LONGTEXT, false );
    add_integer( "rawaud-samplerate", 48000, SAMPLERATE_TEXT, SAMPLERATE_LONGTEXT, false );
    add_string( "rawaud-fourcc", FOURCC_DEFAULT,
                FOURCC_TEXT, FOURCC_LONGTEXT, false );
    add_string( "rawaud-lang", "eng", LANG_TEXT, LANG_LONGTEXT, false);
vlc_module_end();

/*****************************************************************************
 * Definitions of structures used by this plugin
 *****************************************************************************/
struct demux_sys_t
{
    es_out_id_t *p_es;
    es_format_t  fmt;
    unsigned int i_frame_size;
    unsigned int i_frame_samples;
    unsigned int i_seek_step;
    date_t       pts;
};


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t * );
static int Control( demux_t *, int i_query, va_list args );


/*****************************************************************************
 * Open: initializes raw audio demuxer
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    p_demux->p_sys      = p_sys = malloc( sizeof( demux_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    es_format_Init( &p_sys->fmt, AUDIO_ES, 0 );

    char *psz_fourcc = var_CreateGetString( p_demux, "rawaud-fourcc" );
    p_sys->fmt.i_codec = vlc_fourcc_GetCodecFromString( AUDIO_ES, psz_fourcc );
    free( psz_fourcc );

    if( !p_sys->fmt.i_codec )
    {
        msg_Err( p_demux, "rawaud-fourcc must be a 4 character string");
        es_format_Clean( &p_sys->fmt );
        free( p_sys );
        return VLC_EGENERIC;
    }

    // get the bits per sample ratio based on codec
    switch( p_sys->fmt.i_codec )
    {

        case VLC_CODEC_FL64:
            p_sys->fmt.audio.i_bitspersample = 64;
            break;

        case VLC_CODEC_FL32:
        case VLC_CODEC_S32L:
        case VLC_CODEC_S32B:
            p_sys->fmt.audio.i_bitspersample = 32;
            break;

        case VLC_CODEC_S24L:
        case VLC_CODEC_S24B:
            p_sys->fmt.audio.i_bitspersample = 24;
            break;

        case VLC_CODEC_S16L:
        case VLC_CODEC_S16B:
            p_sys->fmt.audio.i_bitspersample = 16;
            break;

        case VLC_CODEC_S8:
        case VLC_CODEC_U8:
            p_sys->fmt.audio.i_bitspersample = 8;
            break;

        default:
            msg_Err( p_demux, "unknown fourcc format %4.4s",
                     (char *)&p_sys->fmt.i_codec);
            es_format_Clean( &p_sys->fmt );
            free( p_sys );
            return VLC_EGENERIC;

    }


    p_sys->fmt.psz_language = var_CreateGetString( p_demux, "rawaud-lang" );
    p_sys->fmt.audio.i_channels = var_CreateGetInteger( p_demux, "rawaud-channels" );
    p_sys->fmt.audio.i_rate = var_CreateGetInteger( p_demux, "rawaud-samplerate" );

    if( p_sys->fmt.audio.i_rate <= 0 )
    {
      msg_Err( p_demux, "invalid sample rate");
      es_format_Clean( &p_sys->fmt );
      free( p_sys );
      return VLC_EGENERIC;
    }

    if( p_sys->fmt.audio.i_channels <= 0 )
    {
      msg_Err( p_demux, "invalid number of channels");
      es_format_Clean( &p_sys->fmt );
      free( p_sys );
      return VLC_EGENERIC;
    }

    p_sys->fmt.i_bitrate = p_sys->fmt.audio.i_rate *
                           p_sys->fmt.audio.i_channels *
                           p_sys->fmt.audio.i_bitspersample;

    msg_Dbg( p_demux,
     "format initialized: channels=%d , samplerate=%d Hz, fourcc=%4.4s, bits per sample = %d, bitrate = %d bit/s",
     p_sys->fmt.audio.i_channels,
     p_sys->fmt.audio.i_rate,
     (char*)&p_sys->fmt.i_codec,
     p_sys->fmt.audio.i_bitspersample,
     p_sys->fmt.i_bitrate);

    /* add the es */
    p_sys->p_es = es_out_Add( p_demux->out, &p_sys->fmt );
    msg_Dbg( p_demux, "elementary stream added");

    /* initialize timing */
    date_Init( &p_sys->pts, p_sys->fmt.audio.i_rate, 1 );
    date_Set( &p_sys->pts, 0 );

    /* calculate 50ms frame size/time */
    p_sys->i_frame_samples = __MAX( p_sys->fmt.audio.i_rate / 20, 1 );
    p_sys->i_seek_step = p_sys->fmt.audio.i_channels *
                          ( (p_sys->fmt.audio.i_bitspersample + 7) / 8 );
    p_sys->i_frame_size = p_sys->i_frame_samples * p_sys->i_seek_step;
    msg_Dbg( p_demux, "frame size is %d bytes ", p_sys->i_frame_size);

    p_demux->pf_demux   = Demux;
    p_demux->pf_control = Control;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys  = p_demux->p_sys;

    es_format_Clean( &p_sys->fmt );
    free( p_sys );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys  = p_demux->p_sys;
    block_t     *p_block;

    if( ( p_block = stream_Block( p_demux->s, p_sys->i_frame_size ) ) == NULL )
    {
        /* EOF */
        return 0;
    }

    p_block->i_dts =
    p_block->i_pts = VLC_TS_0 + date_Get( &p_sys->pts );

    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block->i_pts );
    es_out_Send( p_demux->out, p_sys->p_es, p_block );

    date_Increment( &p_sys->pts, p_sys->i_frame_samples );

    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys  = p_demux->p_sys;

    return demux_vaControlHelper( p_demux->s, 0, -1,
                                   p_sys->fmt.i_bitrate, p_sys->i_seek_step, i_query, args );
}
