/*****************************************************************************
 * cast.cpp: Chromecast sout module for vlc
 *****************************************************************************
 * Copyright © 2014-2015 VideoLAN
 *
 * Authors: Adrien Maglo <magsoft@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Steve Lhomme <robux4@videolabs.io>
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

#include "chromecast.h"
#include <vlc_dialog.h>

#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_modules.h>
#include <vlc_httpd.h>

#include <cassert>

struct sout_access_out_sys_t
{
    intf_sys_t * const m_intf;
    httpd_url_t       *m_url;
    vlc_fifo_t        *m_fifo;
    block_t           *m_header;
    bool               m_eof;
    bool               m_flushing;
    bool               m_seeking;
    std::string        m_mime;

    sout_access_out_sys_t(httpd_host_t *httpd_host, intf_sys_t * const intf,
                          const char *psz_url);
    ~sout_access_out_sys_t();

    void clearUnlocked();
    void clear();
    void flush();
    void prepare(sout_stream_t *p_stream, const std::string &mime);
    int url_cb(httpd_client_t *cl, httpd_message_t *answer, const httpd_message_t *query);
    void seek_done_cb();
};

struct sout_stream_sys_t
{
    sout_stream_sys_t(httpd_host_t *httpd_host, intf_sys_t * const intf, bool has_video, int port,
                      const char *psz_default_muxer, const char *psz_default_mime)
        : httpd_host(httpd_host)
        , access_out_live(httpd_host, intf, "/stream")
        , p_out(NULL)
        , default_muxer(psz_default_muxer)
        , default_mime(psz_default_mime)
        , mime(psz_default_mime)
        , p_intf(intf)
        , b_supports_video(has_video)
        , i_port(port)
        , es_changed( true )
        , cc_has_input( false )
        , out_force_reload( false )
        , drained( false )
        , out_streams_added( 0 )
        , transcode_attempt_idx( 0 )
        , previous_state( Authenticating )
    {
        assert(p_intf != NULL);

    }

    ~sout_stream_sys_t()
    {
    }

    bool canDecodeVideo( vlc_fourcc_t i_codec ) const;
    bool canDecodeAudio( sout_stream_t* p_stream, vlc_fourcc_t i_codec,
                         const audio_format_t* p_fmt ) const;
    bool startSoutChain(sout_stream_t* p_stream,
                        const std::vector<sout_stream_id_sys_t*> &new_streams,
                        const std::string &sout);
    void stopSoutChain(sout_stream_t* p_stream);
    int  handleChromecastState(sout_stream_t* p_stream);

    httpd_host_t      *httpd_host;
    sout_access_out_sys_t access_out_live;

    sout_stream_t     *p_out;
    const std::string  default_muxer;
    const std::string  default_mime;
    std::string        mime;

    intf_sys_t * const p_intf;
    const bool b_supports_video;
    const int i_port;

    sout_stream_id_sys_t *GetSubId( sout_stream_t*, sout_stream_id_sys_t* );

    bool                               es_changed;
    bool                               cc_has_input;
    bool                               out_force_reload;
    bool                               drained;
    std::vector<sout_stream_id_sys_t*> streams;
    std::vector<sout_stream_id_sys_t*> out_streams;
    unsigned int                       out_streams_added;
    unsigned int                       transcode_attempt_idx;
    States                             previous_state;

private:
    bool UpdateOutput( sout_stream_t * );
};

struct sout_stream_id_sys_t
{
    es_format_t           fmt;
    sout_stream_id_sys_t  *p_sub_id;
};

#define SOUT_CFG_PREFIX "sout-chromecast-"

static const vlc_fourcc_t DEFAULT_TRANSCODE_VIDEO = VLC_CODEC_H264;
static const unsigned int MAX_TRANSCODE_PASS = 3;
static const char DEFAULT_MUXER[] = "avformat{mux=matroska,options={live=1}}";


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Open(vlc_object_t *);
static void Close(vlc_object_t *);
static int ProxyOpen(vlc_object_t *);
static int AccessOpen(vlc_object_t *);
static void AccessClose(vlc_object_t *);

static const char *const ppsz_sout_options[] = {
    "ip", "port",  "http-port", "mux", "mime", "video", NULL
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define HTTP_PORT_TEXT N_("HTTP port")
#define HTTP_PORT_LONGTEXT N_("This sets the HTTP port of the local server " \
                              "used to stream the media to the Chromecast.")
#define HAS_VIDEO_TEXT N_("Video")
#define HAS_VIDEO_LONGTEXT N_("The Chromecast receiver can receive video.")
#define MUX_TEXT N_("Muxer")
#define MUX_LONGTEXT N_("This sets the muxer used to stream to the Chromecast.")
#define MIME_TEXT N_("MIME content type")
#define MIME_LONGTEXT N_("This sets the media MIME content type sent to the Chromecast.")
#define PERF_TEXT N_( "Performance warning" )
#define PERF_LONGTEXT N_( "Display a performance warning when transcoding" )
#define AUDIO_PASSTHROUGH_TEXT N_( "Enable Audio passthrough" )
#define AUDIO_PASSTHROUGH_LONGTEXT N_( "Disable if your receiver does not support Dolby®." )

enum {
    CONVERSION_QUALITY_HIGH = 0,
    CONVERSION_QUALITY_MEDIUM = 1,
    CONVERSION_QUALITY_LOW = 2,
    CONVERSION_QUALITY_LOWCPU = 3,
};

#if defined (__ANDROID__) || defined (__arm__) || (defined (TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
# define CONVERSION_QUALITY_DEFAULT CONVERSION_QUALITY_LOW
#else
# define CONVERSION_QUALITY_DEFAULT CONVERSION_QUALITY_MEDIUM
#endif

static const int conversion_quality_list[] = {
    CONVERSION_QUALITY_HIGH,
    CONVERSION_QUALITY_MEDIUM,
    CONVERSION_QUALITY_LOW,
    CONVERSION_QUALITY_LOWCPU,
};
static const char *const conversion_quality_list_text[] = {
    N_( "High (high quality and high bandwidth)" ),
    N_( "Medium (medium quality and medium bandwidth)" ),
    N_( "Low (low quality and low bandwidth)" ),
    N_( "Low CPU (low quality but high bandwidth)" ),
};

#define CONVERSION_QUALITY_TEXT N_( "Conversion quality" )
#define CONVERSION_QUALITY_LONGTEXT N_( "Change this option to increase conversion speed or quality." )

#define IP_ADDR_TEXT N_("IP Address")
#define IP_ADDR_LONGTEXT N_("IP Address of the Chromecast.")
#define PORT_TEXT N_("Chromecast port")
#define PORT_LONGTEXT N_("The port used to talk to the Chromecast.")

#define HTTPD_BUFFER_MAX INT64_C(16 * 1024 * 1024) /* 16 MB */

vlc_module_begin ()

    set_shortname(N_("Chromecast"))
    set_description(N_("Chromecast stream output"))
    set_capability("sout stream", 0)
    add_shortcut("chromecast")
    set_category(CAT_SOUT)
    set_subcategory(SUBCAT_SOUT_STREAM)
    set_callbacks(Open, Close)

    add_string(SOUT_CFG_PREFIX "ip", NULL, IP_ADDR_TEXT, IP_ADDR_LONGTEXT, false)
        change_private()
    add_integer(SOUT_CFG_PREFIX "port", CHROMECAST_CONTROL_PORT, PORT_TEXT, PORT_LONGTEXT, false)
        change_private()
    add_integer(SOUT_CFG_PREFIX "http-port", HTTP_PORT, HTTP_PORT_TEXT, HTTP_PORT_LONGTEXT, false)
    add_bool(SOUT_CFG_PREFIX "video", true, HAS_VIDEO_TEXT, HAS_VIDEO_LONGTEXT, false)
    add_string(SOUT_CFG_PREFIX "mux", DEFAULT_MUXER, MUX_TEXT, MUX_LONGTEXT, false)
    add_string(SOUT_CFG_PREFIX "mime", "video/x-matroska", MIME_TEXT, MIME_LONGTEXT, false)
    add_integer(SOUT_CFG_PREFIX "show-perf-warning", 1, PERF_TEXT, PERF_LONGTEXT, true )
        change_private()
    add_bool(SOUT_CFG_PREFIX "audio-passthrough", false, AUDIO_PASSTHROUGH_TEXT, AUDIO_PASSTHROUGH_LONGTEXT, false )
    add_integer(SOUT_CFG_PREFIX "conversion-quality", CONVERSION_QUALITY_DEFAULT,
                CONVERSION_QUALITY_TEXT, CONVERSION_QUALITY_LONGTEXT, false );
        change_integer_list(conversion_quality_list, conversion_quality_list_text)

    add_submodule()
        /* sout proxy that start the cc input when all streams are loaded */
        add_shortcut("chromecast-proxy")
        set_capability("sout stream", 0)
        set_callbacks(ProxyOpen, NULL)
    add_submodule()
        set_subcategory(SUBCAT_SOUT_ACO)
        add_shortcut("chromecast-http")
        set_capability("sout access", 0)
        set_callbacks(AccessOpen, AccessClose)
vlc_module_end ()

static sout_stream_id_sys_t *ProxyAdd(sout_stream_t *p_stream, const es_format_t *p_fmt)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_sys_t *id = sout_StreamIdAdd(p_stream->p_next, p_fmt);
    if (id)
        p_sys->out_streams_added++;
    return id;
}

static void ProxyDel(sout_stream_t *p_stream, sout_stream_id_sys_t *id)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    p_sys->out_streams_added--;
    return sout_StreamIdDel(p_stream->p_next, id);
}

static int ProxySend(sout_stream_t *p_stream, sout_stream_id_sys_t *id,
                     block_t *p_buffer)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    if (p_sys->cc_has_input || p_sys->out_streams_added >= p_sys->out_streams.size())
    {
        int ret = sout_StreamIdSend(p_stream->p_next, id, p_buffer);
        if (ret == VLC_SUCCESS && !p_sys->cc_has_input)
        {
            /* Start the chromecast only when all streams are added into the
             * last sout (the http one) */
            p_sys->p_intf->setHasInput(p_sys->mime);
            p_sys->cc_has_input = true;
        }
        return ret;
    }
    else
    {
        block_Release(p_buffer);
        return VLC_SUCCESS;
    }
}

static void ProxyFlush(sout_stream_t *p_stream, sout_stream_id_sys_t *id)
{
    return sout_StreamFlush(p_stream->p_next, id);
}

static int ProxyControl(sout_stream_t *p_stream, int i_query, va_list args)
{
    if (!p_stream->p_next->pf_control)
        return VLC_EGENERIC;
    return p_stream->p_next->pf_control(p_stream->p_next, i_query, args);
}

static int ProxyOpen(vlc_object_t *p_this)
{
    sout_stream_t *p_stream = reinterpret_cast<sout_stream_t*>(p_this);
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *) var_InheritAddress(p_this, SOUT_CFG_PREFIX "sys");
    if (p_sys == NULL || p_stream->p_next == NULL)
        return VLC_EGENERIC;

    p_stream->p_sys = (sout_stream_sys_t *) p_sys;
    p_sys->out_streams_added = 0;

    p_stream->pf_add     = ProxyAdd;
    p_stream->pf_del     = ProxyDel;
    p_stream->pf_send    = ProxySend;
    p_stream->pf_flush   = ProxyFlush;
    p_stream->pf_control = ProxyControl;
    return VLC_SUCCESS;
}

static int httpd_url_cb(httpd_callback_sys_t *data, httpd_client_t *cl,
                        httpd_message_t *answer, const httpd_message_t *query)
{
    sout_access_out_sys_t *p_sys = reinterpret_cast<sout_access_out_sys_t *>(data);
    return p_sys->url_cb(cl, answer, query);
}

static void on_seek_done_cb(void *data)
{
    sout_access_out_sys_t *p_sys = reinterpret_cast<sout_access_out_sys_t *>(data);
    p_sys->seek_done_cb();
}

sout_access_out_sys_t::sout_access_out_sys_t(httpd_host_t *httpd_host,
                                             intf_sys_t * const intf,
                                             const char *psz_url)
    : m_intf(intf)
    , m_header(NULL)
    , m_eof(true)
    , m_flushing(false)
    , m_seeking(false)
{
    m_fifo = block_FifoNew();
    if (!m_fifo)
        throw std::runtime_error( "block_FifoNew failed" );
    m_url = httpd_UrlNew(httpd_host, psz_url, NULL, NULL);
    if (m_url == NULL)
    {
        block_FifoRelease(m_fifo);
        throw std::runtime_error( "httpd_UrlNew failed" );
    }
    httpd_UrlCatch(m_url, HTTPD_MSG_GET, httpd_url_cb,
                   (httpd_callback_sys_t*)this);

}

sout_access_out_sys_t::~sout_access_out_sys_t()
{
    block_FifoRelease(m_fifo);
    httpd_UrlDelete(m_url);
}

void sout_access_out_sys_t::clearUnlocked()
{
    block_ChainRelease(vlc_fifo_DequeueAllUnlocked(m_fifo));
    if (m_header)
    {
        block_Release(m_header);
        m_header = NULL;
    }
    m_eof = true;
}

void sout_access_out_sys_t::clear()
{
    vlc_fifo_Lock(m_fifo);
    clearUnlocked();
    vlc_fifo_Unlock(m_fifo);
    vlc_fifo_Signal(m_fifo);
}

void sout_access_out_sys_t::flush()
{
    vlc_fifo_Lock(m_fifo);
    block_ChainRelease(vlc_fifo_DequeueAllUnlocked(m_fifo));
    vlc_fifo_Unlock(m_fifo);

    m_intf->setPacing(false);

    /* Don't seek from here since flush can be called several time (one time
     * per id). */
    m_flushing = true;
}

void sout_access_out_sys_t::prepare(sout_stream_t *p_stream, const std::string &mime)
{
    var_SetAddress(p_stream->p_sout, SOUT_CFG_PREFIX "access-out-sys", this);

    vlc_fifo_Lock(m_fifo);
    clearUnlocked();
    m_mime = mime;
    m_eof = false;
    m_flushing = false;
    m_seeking = false;
    vlc_fifo_Unlock(m_fifo);
}

void sout_access_out_sys_t::seek_done_cb()
{
    vlc_fifo_Lock(m_fifo);
    m_seeking = false;
    vlc_fifo_Unlock(m_fifo);
    vlc_fifo_Signal(m_fifo);
}

int sout_access_out_sys_t::url_cb(httpd_client_t *cl, httpd_message_t *answer,
                                  const httpd_message_t *query)
{
    if (!answer || !query || !cl)
        return VLC_SUCCESS;

    vlc_fifo_Lock(m_fifo);

    block_t *p_block = NULL;
    do
    {
        while ((p_block = vlc_fifo_DequeueUnlocked(m_fifo)) == NULL
           && !m_eof)
            vlc_fifo_Wait(m_fifo);

        /* Wait for the seek to be done. This will prevent the CC to flush this
         * buffer that came after a flush. */
        while (m_seeking && !m_eof)
            vlc_fifo_Wait(m_fifo);

        if (m_flushing && p_block)
        {
            block_Release(p_block);
            p_block = NULL;
        }

    } while (m_flushing);

    /* Handle block headers */
    if (p_block && answer->i_body_offset == 0 && m_header != NULL)
    {
        /* Using header block. Re-insert current block into fifo */
        block_t *p_fifo = vlc_fifo_DequeueAllUnlocked(m_fifo);
        vlc_fifo_QueueUnlocked(m_fifo, p_block);
        vlc_fifo_QueueUnlocked(m_fifo, p_fifo);
        p_block = m_header;
    }
    bool do_unpace = vlc_fifo_GetBytes(m_fifo) < HTTPD_BUFFER_MAX;
    vlc_fifo_Unlock(m_fifo);

    if (do_unpace)
        m_intf->setPacing(false);

    answer->i_proto  = HTTPD_PROTO_HTTP;
    answer->i_version= 0;
    answer->i_type   = HTTPD_MSG_ANSWER;
    answer->i_status = 200;

    bool b_close = false;
    if (p_block)
    {
        if (answer->i_body_offset == 0)
        {
            httpd_MsgAdd(answer, "Content-type", "%s", m_mime.c_str());
            httpd_MsgAdd(answer, "Cache-Control", "no-cache");
            b_close = true;
        }
        answer->p_body = (uint8_t *) malloc(p_block->i_buffer);
        if (answer->p_body)
        {
            answer->i_body = p_block->i_buffer;
            answer->i_body_offset += answer->i_body ;
            memcpy(answer->p_body, p_block->p_buffer, p_block->i_buffer);
        }
        else
            b_close = true;

        if (p_block != m_header)
            block_Release(p_block);
    }
    else
        b_close = true;

    if (b_close)
        httpd_MsgAdd(answer, "Connection", "close");

    return VLC_SUCCESS;
}


static ssize_t AccessWrite(sout_access_out_t *p_access, block_t *p_block)
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    size_t i_len = p_block->i_buffer;

    vlc_fifo_Lock(p_sys->m_fifo);

    if (p_sys->m_flushing)
    {
        p_sys->m_flushing = false;
        p_sys->m_seeking = true;

        vlc_fifo_Unlock(p_sys->m_fifo);
        /* TODO: put better timestamp */
        bool success = p_sys->m_intf->requestPlayerSeek(mdate() + INT64_C(1000000));
        vlc_fifo_Lock(p_sys->m_fifo);
        if (!success)
            p_sys->m_seeking = false;
    }

    bool do_pace = false;
    if (p_block->i_flags & BLOCK_FLAG_HEADER)
    {
        if (p_sys->m_header)
            block_Release(p_sys->m_header);
        p_sys->m_header = p_block;
    }
    else
    {
        /* Tell the demux filter to pace when the fifo starts to be full */
        do_pace = vlc_fifo_GetBytes(p_sys->m_fifo) >= HTTPD_BUFFER_MAX;

        /* Drop buffer is the fifo is really full */
        while (vlc_fifo_GetBytes(p_sys->m_fifo) >= (HTTPD_BUFFER_MAX * 2))
        {
            block_t *p_drop = vlc_fifo_DequeueUnlocked(p_sys->m_fifo);
            msg_Warn(p_access, "httpd buffer full: dropping %zuB", p_drop->i_buffer);
            block_Release(p_drop);
        }
        vlc_fifo_QueueUnlocked(p_sys->m_fifo, p_block);
    }

    vlc_fifo_Unlock(p_sys->m_fifo);
    vlc_fifo_Signal(p_sys->m_fifo);

    if (do_pace)
        p_sys->m_intf->setPacing(true);

    return i_len;
}

static int AccessControl(sout_access_out_t *p_access, int i_query, va_list args)
{
    (void) p_access;

    switch (i_query)
    {
        case ACCESS_OUT_CONTROLS_PACE:
            *va_arg(args, bool *) = false;
            break;
        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int AccessOpen(vlc_object_t *p_this)
{
    sout_access_out_t *p_access = (sout_access_out_t*)p_this;

    sout_access_out_sys_t *p_sys = (sout_access_out_sys_t *)
        var_InheritAddress(p_access, SOUT_CFG_PREFIX "access-out-sys");
    if (p_sys == NULL)
        return VLC_EGENERIC;

    assert(!p_sys->m_eof);

    p_access->pf_write       = AccessWrite;
    p_access->pf_control     = AccessControl;
    p_access->p_sys          = p_sys;

    return VLC_SUCCESS;
}

static void AccessClose(vlc_object_t *p_this)
{
    sout_access_out_t *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t *p_sys = p_access->p_sys;

    vlc_fifo_Lock(p_sys->m_fifo);
    p_sys->m_eof = true;
    p_sys->m_flushing = false;
    vlc_fifo_Unlock(p_sys->m_fifo);
    vlc_fifo_Signal(p_sys->m_fifo);

    p_sys->m_intf->setPacing(false);
}

/*****************************************************************************
 * Sout callbacks
 *****************************************************************************/
static sout_stream_id_sys_t *Add(sout_stream_t *p_stream, const es_format_t *p_fmt)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if (!p_sys->b_supports_video)
    {
        if (p_fmt->i_cat != AUDIO_ES)
            return NULL;
    }

    sout_stream_id_sys_t *p_sys_id = (sout_stream_id_sys_t *)malloc( sizeof(sout_stream_id_sys_t) );
    if (p_sys_id != NULL)
    {
        es_format_Copy( &p_sys_id->fmt, p_fmt );
        p_sys_id->p_sub_id = NULL;

        p_sys->streams.push_back( p_sys_id );
        p_sys->es_changed = true;
    }
    return p_sys_id;
}


static void DelInternal(sout_stream_t *p_stream, sout_stream_id_sys_t *id,
                        bool reset_config)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    for (std::vector<sout_stream_id_sys_t*>::iterator it = p_sys->streams.begin();
         it != p_sys->streams.end(); )
    {
        sout_stream_id_sys_t *p_sys_id = *it;
        if ( p_sys_id == id )
        {
            if ( p_sys_id->p_sub_id != NULL )
            {
                sout_StreamIdDel( p_sys->p_out, p_sys_id->p_sub_id );
                for (std::vector<sout_stream_id_sys_t*>::iterator out_it = p_sys->out_streams.begin();
                     out_it != p_sys->out_streams.end(); )
                {
                    if (*out_it == id)
                    {
                        p_sys->out_streams.erase(out_it);
                        p_sys->es_changed = reset_config;
                        p_sys->out_force_reload = reset_config;
                        break;
                    }
                    out_it++;
                }
            }

            es_format_Clean( &p_sys_id->fmt );
            free( p_sys_id );
            p_sys->streams.erase( it );
            break;
        }
        it++;
    }

    if ( p_sys->out_streams.empty() )
    {
        p_sys->stopSoutChain(p_stream);
        p_sys->p_intf->requestPlayerStop();
        p_sys->access_out_live.clear();
        p_sys->transcode_attempt_idx = 0;
        p_sys->drained = false;
    }
}

static void Del(sout_stream_t *p_stream, sout_stream_id_sys_t *id)
{
    DelInternal(p_stream, id, true);
}

/**
 * Transcode steps:
 * 0: Accept HEVC/VP9 & all supported audio formats
 * 1: Transcode to h264 & accept all supported audio formats if the video codec
 *    was HEVC/VP9
 * 2: Transcode to H264 & MP3
 *
 * Additionally:
 * - Allow (E)AC3 passthrough depending on the audio-passthrough
 *   config value, except for the final step, where we just give up and transcode
 *   everything.
 * - Disallow multichannel AAC
 *
 * Supported formats: https://developers.google.com/cast/docs/media
 */

bool sout_stream_sys_t::canDecodeVideo( vlc_fourcc_t i_codec ) const
{
    if ( transcode_attempt_idx != 0 )
        return false;
    if ( i_codec == VLC_CODEC_HEVC || i_codec == VLC_CODEC_VP9 )
        return transcode_attempt_idx == 0;
    return i_codec == VLC_CODEC_H264 || i_codec == VLC_CODEC_VP8;
}

bool sout_stream_sys_t::canDecodeAudio( sout_stream_t *p_stream,
                                        vlc_fourcc_t i_codec,
                                        const audio_format_t* p_fmt ) const
{
    if ( transcode_attempt_idx == MAX_TRANSCODE_PASS - 1 )
        return false;
    if ( i_codec == VLC_CODEC_A52 || i_codec == VLC_CODEC_EAC3 )
    {
        return var_InheritBool( p_stream, SOUT_CFG_PREFIX "audio-passthrough" );
    }
    if ( i_codec == VLC_FOURCC('h', 'a', 'a', 'c') ||
            i_codec == VLC_FOURCC('l', 'a', 'a', 'c') ||
            i_codec == VLC_FOURCC('s', 'a', 'a', 'c') ||
            i_codec == VLC_CODEC_MPGA ||
            i_codec == VLC_CODEC_MP4A )
    {
        return p_fmt->i_channels <= 2;
    }
    return i_codec == VLC_CODEC_VORBIS || i_codec == VLC_CODEC_OPUS ||
           i_codec == VLC_CODEC_MP3;
}

void sout_stream_sys_t::stopSoutChain(sout_stream_t *p_stream)
{
    (void) p_stream;

    if ( unlikely( p_out != NULL ) )
    {
        for ( size_t i = 0; i < out_streams.size(); i++ )
        {
            if ( out_streams[i]->p_sub_id != NULL )
            {
                sout_StreamIdDel( p_out, out_streams[i]->p_sub_id );
                out_streams[i]->p_sub_id = NULL;
            }
        }
        out_streams.clear();
        sout_StreamChainDelete( p_out, NULL );
        p_out = NULL;
    }
}

bool sout_stream_sys_t::startSoutChain(sout_stream_t *p_stream,
                                       const std::vector<sout_stream_id_sys_t*> &new_streams,
                                       const std::string &sout)
{
    stopSoutChain( p_stream );

    msg_Dbg( p_stream, "Creating chain %s", sout.c_str() );
    previous_state = Authenticating;
    cc_has_input = false;
    out_streams = new_streams;

    access_out_live.prepare( p_stream, mime );

    p_out = sout_StreamChainNew( p_stream->p_sout, sout.c_str(), NULL, NULL);
    if (p_out == NULL) {
        msg_Dbg(p_stream, "could not create sout chain:%s", sout.c_str());
        out_streams.clear();
        access_out_live.clear();
        return false;
    }

    /* check the streams we can actually add */
    for (std::vector<sout_stream_id_sys_t*>::iterator it = out_streams.begin();
         it != out_streams.end(); )
    {
        sout_stream_id_sys_t *p_sys_id = *it;
        p_sys_id->p_sub_id = sout_StreamIdAdd( p_out, &p_sys_id->fmt );
        if ( p_sys_id->p_sub_id == NULL )
        {
            msg_Err( p_stream, "can't handle %4.4s stream", (char *)&p_sys_id->fmt.i_codec );
            es_format_Clean( &p_sys_id->fmt );
            it = out_streams.erase( it );
        }
        else
            ++it;
    }

    if (out_streams.empty())
    {
        stopSoutChain( p_stream );
        access_out_live.clear();
        return false;
    }
    return true;
}

bool sout_stream_sys_t::UpdateOutput( sout_stream_t *p_stream )
{
    assert( p_stream->p_sys == this );

    if ( !es_changed )
        return true;

    es_changed = false;

    bool canRemux = true;
    vlc_fourcc_t i_codec_video = 0, i_codec_audio = 0;
    const es_format_t *p_original_audio = NULL;
    const es_format_t *p_original_video = NULL;
    bool b_out_streams_changed = false;
    std::vector<sout_stream_id_sys_t*> new_streams;

    for (std::vector<sout_stream_id_sys_t*>::iterator it = streams.begin(); it != streams.end(); ++it)
    {
        const es_format_t *p_es = &(*it)->fmt;
        if (p_es->i_cat == AUDIO_ES && p_original_audio == NULL)
        {
            if ( !canDecodeAudio( p_stream, p_es->i_codec, &p_es->audio ) )
            {
                msg_Dbg( p_stream, "can't remux audio track %d codec %4.4s", p_es->i_id, (const char*)&p_es->i_codec );
                canRemux = false;
            }
            else if (i_codec_audio == 0)
                i_codec_audio = p_es->i_codec;
            p_original_audio = p_es;
            new_streams.push_back(*it);
        }
        else if (b_supports_video)
        {
            if (p_es->i_cat == VIDEO_ES && p_original_video == NULL)
            {
                if (!canDecodeVideo( p_es->i_codec ))
                {
                    msg_Dbg( p_stream, "can't remux video track %d codec %4.4s",
                             p_es->i_id, (const char*)&p_es->i_codec );
                    canRemux = false;
                }
                else if (i_codec_video == 0)
                    i_codec_video = p_es->i_codec;
                p_original_video = p_es;
                new_streams.push_back(*it);
            }
            else
                continue;
            /* TODO: else handle ttml/webvtt */
        }
        else
            continue;

        bool b_found = out_force_reload;
        for (std::vector<sout_stream_id_sys_t*>::iterator out_it = out_streams.begin();
             out_it != out_streams.end() && !b_found; ++out_it)
        {
            if (*out_it == *it)
                b_found = true;
        }
        if (!b_found)
            b_out_streams_changed = true;
    }

    if (new_streams.empty())
    {
        p_intf->requestPlayerStop();
        return true;
    }

    /* Don't restart sout and CC session if streams didn't change */
    if (!out_force_reload && new_streams.size() == out_streams.size() && !b_out_streams_changed)
        return true;

    out_force_reload = false;

    std::stringstream ssout;
    if ( !canRemux )
    {
        if ( i_codec_video == 0 && p_original_video
          && var_InheritInteger( p_stream, SOUT_CFG_PREFIX "show-perf-warning" ) )
        {
            int res = vlc_dialog_wait_question( p_stream,
                          VLC_DIALOG_QUESTION_WARNING,
                         _("Cancel"), _("OK"), _("Ok, Don't warn me again"),
                         _("Performance warning"),
                         _("Casting this video requires conversion. "
                           "This conversion can use all the available power and "
                           "could quickly drain your battery." ) );
            if ( res <= 0 )
                 return false;
            if ( res == 2 )
                config_PutInt(p_stream, SOUT_CFG_PREFIX "show-perf-warning", 0 );
        }

        static const char video_maxres_hd[] = "maxwidth=1920,maxheight=1080";
        static const char video_maxres_720p[] = "maxwidth=1280,maxheight=720";
        static const char video_x264_preset_veryfast[] = "veryfast";
        static const char video_x264_preset_ultrafast[] = "ultrafast";

        const int i_quality = var_InheritInteger( p_stream, SOUT_CFG_PREFIX "conversion-quality" );
        const char *psz_video_maxres;
        const char *psz_video_x264_preset;
        unsigned i_video_x264_crf_hd, i_video_x264_crf_720p;
        bool b_audio_mp3;

        switch ( i_quality )
        {
            case CONVERSION_QUALITY_HIGH:
                psz_video_maxres = video_maxres_hd;
                i_video_x264_crf_hd = i_video_x264_crf_720p = 21;
                psz_video_x264_preset = video_x264_preset_veryfast;
                b_audio_mp3 = false;
                break;
            case CONVERSION_QUALITY_MEDIUM:
                psz_video_maxres = video_maxres_hd;
                i_video_x264_crf_hd = 23;
                i_video_x264_crf_720p = 21;
                psz_video_x264_preset = video_x264_preset_veryfast;
                b_audio_mp3 = false;
                break;
            case CONVERSION_QUALITY_LOW:
                psz_video_maxres = video_maxres_720p;
                i_video_x264_crf_hd = i_video_x264_crf_720p = 23;
                psz_video_x264_preset = video_x264_preset_veryfast;
                b_audio_mp3 = true;
                break;
            default:
            case CONVERSION_QUALITY_LOWCPU:
                psz_video_maxres = video_maxres_720p;
                i_video_x264_crf_hd = i_video_x264_crf_720p = 23;
                psz_video_x264_preset = video_x264_preset_ultrafast;
                b_audio_mp3 = true;
                break;
        }

        /* TODO: provide audio samplerate and channels */
        ssout << "transcode{";
        char s_fourcc[5];
        if ( i_codec_audio == 0 && p_original_audio )
        {
            if ( !b_audio_mp3
              && p_original_audio->audio.i_channels > 2 && module_exists( "vorbis" ) )
                i_codec_audio = VLC_CODEC_VORBIS;
            else
                i_codec_audio = VLC_CODEC_MP3;

            msg_Dbg( p_stream, "Converting audio to %.4s", (const char*)&i_codec_audio );
            ssout << "acodec=";
            vlc_fourcc_to_char( i_codec_audio, s_fourcc );
            s_fourcc[4] = '\0';
            ssout << s_fourcc << ',';
            if( i_codec_audio == VLC_CODEC_VORBIS )
                ssout << "aenc=vorbis{quality=6},";
        }
        if ( i_codec_video == 0 && p_original_video )
        {
            i_codec_video = DEFAULT_TRANSCODE_VIDEO;
            msg_Dbg( p_stream, "Converting video to %.4s", (const char*)&i_codec_video );
            ssout << "vcodec=";
            vlc_fourcc_to_char( i_codec_video, s_fourcc );
            s_fourcc[4] = '\0';
            ssout << s_fourcc << ',' << psz_video_maxres << ',';

            const video_format_t *p_vid = &p_original_video->video;
            const bool b_hdres = p_vid == NULL || p_vid->i_height == 0 || p_vid->i_height >= 800;
            unsigned i_video_x264_crf = b_hdres ? i_video_x264_crf_hd : i_video_x264_crf_720p;

            if( p_vid == NULL
             || p_vid->i_frame_rate == 0 || p_vid->i_frame_rate_base == 0
             || ( p_vid->i_frame_rate / p_vid->i_frame_rate_base ) > 30 )
            {
                /* Even force 24fps if the frame rate is unknown */
                msg_Warn( p_stream, "lowering frame rate to 24fps" );
                ssout << "fps=24,";
            }

            if( i_codec_video == VLC_CODEC_H264 )
            {
                if ( module_exists("x264") )
                    ssout << "venc=x264{preset=" << psz_video_x264_preset
                          << ",crf=" << i_video_x264_crf << "},";
            }
        }
        ssout << "}:";
    }
    if ( !p_original_video && default_muxer == DEFAULT_MUXER )
        mime = "audio/x-matroska";
    else if ( i_codec_audio == VLC_CODEC_VORBIS &&
              i_codec_video == VLC_CODEC_VP8 &&
              default_muxer == DEFAULT_MUXER )
        mime = "video/webm";
    else
        mime = default_mime;

    ssout << "chromecast-proxy:"
          << "http{mux=" << default_muxer
          << ",access=chromecast-http}";

    if ( !startSoutChain( p_stream, new_streams, ssout.str() ) )
        p_intf->requestPlayerStop();

    return true;
}

sout_stream_id_sys_t *sout_stream_sys_t::GetSubId( sout_stream_t *p_stream,
                                                   sout_stream_id_sys_t *id )
{
    size_t i;

    assert( p_stream->p_sys == this );

    if ( UpdateOutput( p_stream ) == false )
        return NULL;

    for (i = 0; i < out_streams.size(); ++i)
    {
        if ( id == (sout_stream_id_sys_t*) out_streams[i] )
            return out_streams[i]->p_sub_id;
    }

    msg_Err( p_stream, "unknown stream ID" );
    return NULL;
}

int sout_stream_sys_t::handleChromecastState(sout_stream_t* p_stream)
{
    States s = p_intf->state();
    if (cc_has_input && previous_state != s)
    {
        if (!drained && s == LoadFailed && es_changed == false)
        {
            if (transcode_attempt_idx > MAX_TRANSCODE_PASS - 1)
            {
                msg_Err(p_stream, "All attempts failed. Giving up.");
                return VLC_EGENERIC;
            }
            transcode_attempt_idx++;
            es_changed = true;
            stopSoutChain(p_stream);
            msg_Warn(p_stream, "Load failed detected. Switching to next "
                     "configuration index: %u", transcode_attempt_idx);
            return VLC_ETIMEOUT;
        }
        else if (s == Playing || s == Paused)
        {
            msg_Dbg( p_stream, "Playback started: Current configuration (%u) "
                     "accepted", transcode_attempt_idx );
        }
        else if (s == Connected || s == TakenOver)
        {
            msg_Warn(p_stream, "chromecast %s, aborting...\n",
                     s == Connected ? "exited" : "was taken over");
            stopSoutChain(p_stream);
            return VLC_EGENERIC;
        }
        previous_state = s;
    }
    return VLC_SUCCESS;
}

static int Send(sout_stream_t *p_stream, sout_stream_id_sys_t *id,
                block_t *p_buffer)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    sout_stream_id_sys_t *next_id = p_sys->GetSubId( p_stream, id );
    if ( next_id == NULL )
    {
        block_Release( p_buffer );
        return VLC_EGENERIC;
    }

    int ret = p_sys->handleChromecastState(p_stream);
    switch (ret)
    {
        case VLC_SUCCESS:
            break;
        case VLC_ETIMEOUT:
            next_id = p_sys->GetSubId( p_stream, id );
            if (next_id != NULL)
                break;
            /* fallthrough */
        default:
            block_Release( p_buffer );
            return VLC_EGENERIC;
    }

    ret = sout_StreamIdSend(p_sys->p_out, next_id, p_buffer);
    if (ret != VLC_SUCCESS)
        DelInternal(p_stream, id, false);

    p_sys->drained = false;

    return ret;
}

static void Flush( sout_stream_t *p_stream, sout_stream_id_sys_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if( p_sys->out_streams.empty() && p_sys->drained )
    {
        /* Worst case scenario that won't happen often: we terminated
         * everything since we were draining but the user requested a seek.
         * Change the es_changed value in order to re-create everything again.
         * */

        p_sys->es_changed = true;
    }

    id = p_sys->GetSubId( p_stream, id );
    if ( id == NULL || p_sys->drained )
        return;

    sout_StreamFlush( p_sys->p_out, id );
    p_sys->access_out_live.flush();
}

static int Control(sout_stream_t *p_stream, int i_query, va_list args)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if (i_query == SOUT_STREAM_EMPTY)
    {
        bool *b = va_arg( args, bool * );

        /* Close the whole sout chain. This will drain every streams, and send
         * the last data to the chromecast-http sout_access. This sout_access
         * won't close the http connection but will send an EOF in order to let
         * the CC download the last remaining data. */
        if( !p_sys->drained )
        {
            p_sys->stopSoutChain( p_stream );
            p_sys->drained = true;
        }

        /* check if the Chromecast to be done playing */
        *b = p_sys->p_intf->isFinishedPlaying()
          || p_sys->handleChromecastState(p_stream) != VLC_SUCCESS;
        return VLC_SUCCESS;
    }

    if ( !p_sys->p_out->pf_control )
        return VLC_EGENERIC;

    return p_sys->p_out->pf_control( p_sys->p_out, i_query, args );
}

/*****************************************************************************
 * Open: connect to the Chromecast and initialize the sout
 *****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    sout_stream_t *p_stream = reinterpret_cast<sout_stream_t*>(p_this);
    sout_stream_sys_t *p_sys = NULL;
    intf_sys_t *p_intf = NULL;
    char *psz_ip = NULL;
    char *psz_mux = NULL;
    char *psz_var_mime = NULL;
    sout_stream_t *p_sout = NULL;
    httpd_host_t *httpd_host = NULL;
    bool b_supports_video = true;
    int i_local_server_port;
    int i_device_port;
    std::stringstream ss;

    vlc_interrupt_t *p_interrupt = vlc_interrupt_create();
    if (unlikely(p_interrupt == NULL))
        goto error;

    config_ChainParse(p_stream, SOUT_CFG_PREFIX, ppsz_sout_options, p_stream->p_cfg);

    psz_ip = var_GetNonEmptyString( p_stream, SOUT_CFG_PREFIX "ip");
    if ( psz_ip == NULL )
    {
        msg_Err( p_this, "missing Chromecast IP address" );
        goto error;
    }

    i_device_port = var_InheritInteger(p_stream, SOUT_CFG_PREFIX "port");
    i_local_server_port = var_InheritInteger(p_stream, SOUT_CFG_PREFIX "http-port");

    var_Create(p_stream, "http-port", VLC_VAR_INTEGER);
    var_SetInteger(p_stream, "http-port", i_local_server_port);
    var_Create(p_stream, "http-host", VLC_VAR_STRING);
    var_SetString(p_stream, "http-host", "");
    httpd_host = vlc_http_HostNew(VLC_OBJECT(p_stream));
    if (httpd_host == NULL)
        goto error;

    try
    {
        p_intf = new intf_sys_t( p_this, i_local_server_port, psz_ip, i_device_port,
                                 p_interrupt, httpd_host );
    }
    catch (const std::runtime_error& err )
    {
        msg_Err( p_this, "cannot load the Chromecast controller (%s)", err.what() );
        goto error;
    }
    catch (const std::bad_alloc& )
    {
        p_intf = NULL;
        goto error;
    }

    p_interrupt = NULL;

    psz_mux = var_GetNonEmptyString(p_stream, SOUT_CFG_PREFIX "mux");
    if (psz_mux == NULL)
    {
        goto error;
    }
    psz_var_mime = var_GetNonEmptyString(p_stream, SOUT_CFG_PREFIX "mime");
    if (psz_var_mime == NULL)
        goto error;

    /* check if we can open the proper sout */
    ss << "http{mux=" << psz_mux << "}";
    p_sout = sout_StreamChainNew( p_stream->p_sout, ss.str().c_str(), NULL, NULL);
    if (p_sout == NULL) {
        msg_Dbg(p_stream, "could not create sout chain:%s", ss.str().c_str());
        goto error;
    }
    sout_StreamChainDelete( p_sout, NULL );


    b_supports_video = var_GetBool(p_stream, SOUT_CFG_PREFIX "video");

    p_sys = new(std::nothrow) sout_stream_sys_t( httpd_host, p_intf, b_supports_video,
                                                 i_local_server_port, psz_mux, psz_var_mime );
    if (unlikely(p_sys == NULL))
        goto error;

    p_intf->setOnSeekDoneCb(on_seek_done_cb, &p_sys->access_out_live);

    /* prevent sout-mux-caching since chromecast-proxy is already doing it */
    var_Create( p_stream->p_sout, "sout-mux-caching", VLC_VAR_INTEGER );
    var_SetInteger( p_stream->p_sout, "sout-mux-caching", 0 );

    var_Create( p_stream->p_sout, SOUT_CFG_PREFIX "sys", VLC_VAR_ADDRESS );
    var_SetAddress( p_stream->p_sout, SOUT_CFG_PREFIX "sys", p_sys );

    var_Create( p_stream->p_sout, SOUT_CFG_PREFIX "access-out-sys", VLC_VAR_ADDRESS );

    // Set the sout callbacks.
    p_stream->pf_add     = Add;
    p_stream->pf_del     = Del;
    p_stream->pf_send    = Send;
    p_stream->pf_flush   = Flush;
    p_stream->pf_control = Control;

    p_stream->p_sys = p_sys;
    free(psz_ip);
    free(psz_mux);
    free(psz_var_mime);
    return VLC_SUCCESS;

error:
    if (p_interrupt)
        vlc_interrupt_destroy(p_interrupt);
    delete p_intf;
    if (httpd_host)
        httpd_HostDelete(httpd_host);
    free(psz_ip);
    free(psz_mux);
    free(psz_var_mime);
    delete p_sys;
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
static void Close(vlc_object_t *p_this)
{
    sout_stream_t *p_stream = reinterpret_cast<sout_stream_t*>(p_this);
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    assert(p_sys->out_streams.empty() && p_sys->streams.empty());
    var_Destroy( p_stream->p_sout, SOUT_CFG_PREFIX "sys" );
    var_Destroy( p_stream->p_sout, SOUT_CFG_PREFIX "sout-mux-caching" );

    assert(p_sys->streams.empty() && p_sys->out_streams.empty());

    httpd_host_t *httpd_host = p_sys->httpd_host;
    intf_sys_t *p_intf = p_sys->p_intf;
    p_intf->setOnSeekDoneCb(NULL, NULL);
    delete p_sys;
    delete p_intf;
    /* Delete last since p_intf and p_sys depends on httpd_host */
    httpd_HostDelete(httpd_host);
}

