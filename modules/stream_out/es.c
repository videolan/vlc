/*****************************************************************************
 * es.c: Elementary stream output module
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc_sout.h>
#include <vlc_dialog.h>
#include <vlc_memstream.h>

typedef struct
{
    int  i_count_audio;
    int  i_count_video;
    int  i_count;

    char *psz_mux;
    char *psz_mux_audio;
    char *psz_mux_video;

    char *psz_access;
    char *psz_access_audio;
    char *psz_access_video;

    char *psz_dst;
    char *psz_dst_audio;
    char *psz_dst_video;
} sout_stream_sys_t;

typedef struct
{
    sout_input_t *p_input;
    sout_mux_t   *p_mux;
} sout_stream_id_sys_t;

static char * es_print_url( const char *psz_fmt, vlc_fourcc_t i_fourcc, int i_count,
                            const char *psz_access, const char *psz_mux )
{
    struct vlc_memstream stream;
    unsigned char c;

    if (vlc_memstream_open(&stream))
        return NULL;

    if( psz_fmt == NULL || !*psz_fmt )
        psz_fmt = "stream-%n-%c.%m";

    while ((c = *(psz_fmt++)) != '\0')
    {
        if (c != '%')
        {
            vlc_memstream_putc(&stream, c);
            continue;
        }

        switch (c = *(psz_fmt++))
        {
            case 'n':
                vlc_memstream_printf(&stream, "%d", i_count);
                break;
            case 'c':
                vlc_memstream_printf(&stream, "%4.4s", (char *)&i_fourcc);
                break;
            case 'm':
                vlc_memstream_puts(&stream, psz_mux);
                break;
            case 'a':
                vlc_memstream_puts(&stream, psz_access);
                break;
            case '\0':
                vlc_memstream_putc(&stream, '%');
                goto out;
            default:
                vlc_memstream_printf(&stream, "%%%c", (int) c);
                break;
        }
    }
out:
    if (vlc_memstream_close(&stream))
        return NULL;
    return stream.ptr;
}

static void *Add( sout_stream_t *p_stream, const es_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_sys_t  *id;

    const char        *psz_access;
    const char        *psz_mux;
    char              *psz_dst;

    sout_access_out_t *p_access;
    sout_mux_t        *p_mux;

    /* *** get access name *** */
    if( p_fmt->i_cat == AUDIO_ES && p_sys->psz_access_audio && *p_sys->psz_access_audio )
    {
        psz_access = p_sys->psz_access_audio;
    }
    else if( p_fmt->i_cat == VIDEO_ES && p_sys->psz_access_video && *p_sys->psz_access_video )
    {
        psz_access = p_sys->psz_access_video;
    }
    else
    {
        psz_access = p_sys->psz_access;
    }

    /* *** get mux name *** */
    if( p_fmt->i_cat == AUDIO_ES && p_sys->psz_mux_audio && *p_sys->psz_mux_audio )
    {
        psz_mux = p_sys->psz_mux_audio;
    }
    else if( p_fmt->i_cat == VIDEO_ES && p_sys->psz_mux_video && *p_sys->psz_mux_video )
    {
        psz_mux = p_sys->psz_mux_video;
    }
    else
    {
        psz_mux = p_sys->psz_mux;
    }

    /* Get url (%d expanded as a codec count, %c expanded as codec fcc ) */
    if( p_fmt->i_cat == AUDIO_ES && p_sys->psz_dst_audio && *p_sys->psz_dst_audio )
    {
        psz_dst = es_print_url( p_sys->psz_dst_audio, p_fmt->i_codec,
                                p_sys->i_count_audio, psz_access, psz_mux );
    }
    else if( p_fmt->i_cat == VIDEO_ES && p_sys->psz_dst_video && *p_sys->psz_dst_video )
    {
        psz_dst = es_print_url( p_sys->psz_dst_video, p_fmt->i_codec,
                                p_sys->i_count_video, psz_access, psz_mux );
    }
    else
    {
        int i_count;
        if( p_fmt->i_cat == VIDEO_ES )
        {
            i_count = p_sys->i_count_video;
        }
        else if( p_fmt->i_cat == AUDIO_ES )
        {
            i_count = p_sys->i_count_audio;
        }
        else
        {
            i_count = p_sys->i_count;
        }

        psz_dst = es_print_url( p_sys->psz_dst, p_fmt->i_codec,
                                i_count, psz_access, psz_mux );
    }

    p_sys->i_count++;
    if( p_fmt->i_cat == VIDEO_ES )
    {
        p_sys->i_count_video++;
    }
    else if( p_fmt->i_cat == AUDIO_ES )
    {
        p_sys->i_count_audio++;
    }
    msg_Dbg( p_stream, "creating `%s/%s://%s'",
             psz_access, psz_mux, psz_dst );

    /* *** find and open appropriate access module *** */
    p_access = sout_AccessOutNew( p_stream, psz_access, psz_dst );
    if( p_access == NULL )
    {
        msg_Err( p_stream, "no suitable sout access module for `%s/%s://%s'",
                 psz_access, psz_mux, psz_dst );
        vlc_dialog_display_error( p_stream, _("Streaming / Transcoding failed"),
            _("There is no suitable stream-output access module for \"%s/%s://%s\"."),
            psz_access, psz_mux, psz_dst );
        free( psz_dst );
        return( NULL );
    }

    bool pace_control = sout_AccessOutCanControlPace( p_access );

    /* *** find and open appropriate mux module *** */
    p_mux = sout_MuxNew( p_access, psz_mux );
    if( p_mux == NULL )
    {
        msg_Err( p_stream, "no suitable sout mux module for `%s/%s://%s'",
                 psz_access, psz_mux, psz_dst );
        vlc_dialog_display_error( p_stream, _("Streaming / Transcoding failed"),
            _("There is no suitable stream-output access module "\
            "for \"%s/%s://%s\"."), psz_access, psz_mux, psz_dst );
        sout_AccessOutDelete( p_access );
        free( psz_dst );
        return( NULL );
    }
    free( psz_dst );

    id = malloc( sizeof( sout_stream_id_sys_t ) );
    if( !id )
    {
        sout_MuxDelete( p_mux );
        sout_AccessOutDelete( p_access );
        return NULL;
    }
    id->p_mux = p_mux;
    id->p_input = sout_MuxAddStream( p_mux, p_fmt );

    if( id->p_input == NULL )
    {
        sout_MuxDelete( p_mux );
        sout_AccessOutDelete( p_access );
        free( id );
        return NULL;
    }

    if( !pace_control )
        p_stream->p_sout->i_out_pace_nocontrol++;

    return id;
}

static void Del( sout_stream_t *p_stream, void *_id )
{
    VLC_UNUSED(p_stream);
    sout_stream_id_sys_t *id = (sout_stream_id_sys_t *)_id;
    sout_access_out_t *p_access = id->p_mux->p_access;

    sout_MuxDeleteStream( id->p_mux, id->p_input );
    sout_MuxDelete( id->p_mux );
    if( !sout_AccessOutCanControlPace( p_access ) )
        p_stream->p_sout->i_out_pace_nocontrol--;
    sout_AccessOutDelete( p_access );

    free( id );
}

static int Send( sout_stream_t *p_stream, void *_id, block_t *p_buffer )
{
    VLC_UNUSED(p_stream);
    sout_stream_id_sys_t *id = (sout_stream_id_sys_t *)_id;
    return sout_MuxSendBuffer( id->p_mux, id->p_input, p_buffer );
}

#define SOUT_CFG_PREFIX "sout-es-"

static const char *const ppsz_sout_options[] = {
    "access", "access-audio", "access-video",
    "mux", "mux-audio", "mux-video",
    "dst", "dst-audio", "dst-video",
    NULL
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t       *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = vlc_obj_malloc(p_this, sizeof (*p_sys));

    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    config_ChainParse( p_stream, SOUT_CFG_PREFIX, ppsz_sout_options, p_stream->p_cfg );

    p_sys->i_count          = 0;
    p_sys->i_count_audio    = 0;
    p_sys->i_count_video    = 0;

    p_sys->psz_access = var_GetString( p_stream, SOUT_CFG_PREFIX "access" );
    p_sys->psz_access_audio = var_GetString( p_stream, SOUT_CFG_PREFIX "access-audio" );
    p_sys->psz_access_video = var_GetString( p_stream, SOUT_CFG_PREFIX "access-video" );

    p_sys->psz_mux = var_GetString( p_stream, SOUT_CFG_PREFIX "mux" );
    p_sys->psz_mux_audio = var_GetString( p_stream, SOUT_CFG_PREFIX "mux-audio" );
    p_sys->psz_mux_video = var_GetString( p_stream, SOUT_CFG_PREFIX "mux-video" );

    p_sys->psz_dst       = var_GetString( p_stream, SOUT_CFG_PREFIX "dst" );
    p_sys->psz_dst_audio = var_GetString( p_stream, SOUT_CFG_PREFIX "dst-audio" );
    p_sys->psz_dst_video = var_GetString( p_stream, SOUT_CFG_PREFIX "dst-video" );

    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    p_stream->p_sys     = p_sys;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/

static void Close( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    free( p_sys->psz_access );
    free( p_sys->psz_access_audio );
    free( p_sys->psz_access_video );

    free( p_sys->psz_mux );
    free( p_sys->psz_mux_audio );
    free( p_sys->psz_mux_video );

    free( p_sys->psz_dst );
    free( p_sys->psz_dst_audio );
    free( p_sys->psz_dst_video );
}

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ACCESS_TEXT N_("Output access method")
#define ACCESS_LONGTEXT N_( \
    "This is the default output access method that will be used." )

#define ACCESSA_TEXT N_("Audio output access method")
#define ACCESSA_LONGTEXT N_( \
    "This is the output access method that will be used for audio." )
#define ACCESSV_TEXT N_("Video output access method")
#define ACCESSV_LONGTEXT N_( \
    "This is the output access method that will be used for video." )

#define MUX_TEXT N_("Output muxer")
#define MUX_LONGTEXT N_( \
    "This is the default muxer method that will be used." )
#define MUXA_TEXT N_("Audio output muxer")
#define MUXA_LONGTEXT N_( \
    "This is the muxer that will be used for audio." )
#define MUXV_TEXT N_("Video output muxer")
#define MUXV_LONGTEXT N_( \
    "This is the muxer that will be used for video." )

#define DEST_TEXT N_("Output URL")
#define DEST_LONGTEXT N_( \
    "This is the default output URI." )
#define DESTA_TEXT N_("Audio output URL")
#define DESTA_LONGTEXT N_( \
    "This is the output URI that will be used for audio." )
#define DESTV_TEXT N_("Video output URL")
#define DESTV_LONGTEXT N_( \
    "This is the output URI that will be used for video." )

vlc_module_begin()
    set_shortname("ES")
    set_description(N_("Elementary stream output"))
    set_capability("sout output", 50)
    add_shortcut("es")
    set_category(CAT_SOUT)
    set_subcategory(SUBCAT_SOUT_STREAM)

    set_section(N_("Generic"), NULL)
    add_string(SOUT_CFG_PREFIX "access", "",
               ACCESS_TEXT, ACCESS_LONGTEXT, true)
    add_string(SOUT_CFG_PREFIX "mux", "", MUX_TEXT, MUX_LONGTEXT, true)
    add_string(SOUT_CFG_PREFIX "dst", "", DEST_TEXT, DEST_LONGTEXT, true )

    set_section(N_("Audio"), NULL)
    add_string(SOUT_CFG_PREFIX "access-audio", "",
               ACCESSA_TEXT, ACCESSA_LONGTEXT, true )
    add_string(SOUT_CFG_PREFIX "mux-audio", "", MUXA_TEXT, MUXA_LONGTEXT, true)
    add_string(SOUT_CFG_PREFIX "dst-audio", "",
               DESTA_TEXT, DESTA_LONGTEXT, true)

    set_section(N_("Video"), NULL)
    add_string(SOUT_CFG_PREFIX "access-video", "", ACCESSV_TEXT,
               ACCESSV_LONGTEXT, true)
    add_string(SOUT_CFG_PREFIX "mux-video", "", MUXV_TEXT,
               MUXV_LONGTEXT, true)
    add_string(SOUT_CFG_PREFIX "dst-video", "", DESTV_TEXT,
               DESTV_LONGTEXT, true)

    set_callbacks(Open, Close)
vlc_module_end()
