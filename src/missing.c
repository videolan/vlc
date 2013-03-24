/*****************************************************************************
 * missing.c: missing libvlccore symbols
 *****************************************************************************
 * Copyright (C) 2008 Rémi Denis-Courmont
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

/** \file
 * This file contains dummy replacement API for disabled features
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <assert.h>

#ifndef ENABLE_HTTPD
# include <vlc_httpd.h>

char *httpd_ClientIP (const httpd_client_t *cl, char *psz_ip, int *port)
{
    (void) cl; (void) psz_ip; (void) port;
    assert (0);
}

void httpd_ClientModeBidir (httpd_client_t *cl)
{
    (void) cl;
    assert (0);
}

void httpd_ClientModeStream (httpd_client_t *cl)
{
    (void) cl;
    assert (0);
}

httpd_file_sys_t *httpd_FileDelete (httpd_file_t *file)
{
    (void) file;
    assert (0);
}

httpd_file_t *httpd_FileNew (httpd_host_t *host,
                             const char *url, const char *content_type,
                             const char *login, const char *password,
                             httpd_file_callback_t cb, httpd_file_sys_t *data)
{
    (void) host;
    (void) url; (void) content_type;
    (void) login; (void) password;
    (void) cb; (void) data;
    assert (0);
}

httpd_handler_sys_t *httpd_HandlerDelete (httpd_handler_t *handler)
{
    (void) handler;
    assert (0);
}

httpd_handler_t *httpd_HandlerNew (httpd_host_t *host, const char *url,
                                   const char *login, const char *password,
                                   httpd_handler_callback_t cb,
                                   httpd_handler_sys_t *data)
{
    (void) host; (void) url;
    (void) login; (void) password;
    (void) cb; (void) data;
    assert (0);
}

void httpd_HostDelete (httpd_host_t *h)
{
    (void) h;
    assert (0);
}

httpd_host_t *vlc_http_HostNew (vlc_object_t *obj)
{
    msg_Err (obj, "HTTP server not compiled-in!");
    return NULL;
}

httpd_host_t *vlc_https_HostNew (vlc_object_t *obj)
{
    msg_Err (obj, "HTTPS server not compiled-in!");
    return NULL;
}

httpd_host_t *vlc_rtsp_HostNew (vlc_object_t *obj)
{
    msg_Err (obj, "RTSP server not compiled-in!");
    return NULL;
}

void httpd_MsgAdd (httpd_message_t *m, const char *name, const char *fmt, ...)
{
    (void) m; (void) name; (void) fmt;
    assert (0);
}

const char *httpd_MsgGet (const httpd_message_t *m, const char *name)
{
    (void) m; (void) name;
    assert (0);
}

void httpd_RedirectDelete (httpd_redirect_t *r)
{
    (void) r;
    assert (0);
}

httpd_redirect_t *httpd_RedirectNew (httpd_host_t *host,
                                     const char *dst, const char *src)
{
    (void) host; (void) dst; (void) src;
    assert (0);
}

char *httpd_ServerIP (const httpd_client_t *client, char *ip, int *port)
{
    (void) client; (void) ip; (void) port;
    assert (0);
}

void httpd_StreamDelete (httpd_stream_t *stream)
{
    (void) stream;
    assert (0);
}

int httpd_StreamHeader (httpd_stream_t *stream, uint8_t *data, int count)
{
    (void) stream; (void) data; (void) count;
    assert (0);
}

httpd_stream_t *httpd_StreamNew (httpd_host_t *host,
                                 const char *url, const char *content_type,
                                 const char *login, const char *password)
{
    (void) host; (void) url; (void) content_type;
    (void) login; (void) password;
    assert (0);
}

int httpd_StreamSend (httpd_stream_t *stream, uint8_t *data, int count)
{
    (void) stream; (void) data; (void) count;
    assert (0);
}

int httpd_UrlCatch (httpd_url_t *url, int request, httpd_callback_t cb,
                    httpd_callback_sys_t *data)
{
    (void) url; (void) request; (void) cb; (void) data;
    assert (0);
}

void httpd_UrlDelete (httpd_url_t *url)
{
    (void) url;
    assert (0);
}

httpd_url_t *httpd_UrlNew (httpd_host_t *host, const char *url,
                           const char *login, const char *password)
{
    (void) host; (void) url; (void) login; (void) password;
    assert (0);
}
#endif /* !ENABLE_HTTPD */

#ifndef ENABLE_SOUT
# include <vlc_sout.h>

char *sdp_AddMedia (char **sdp, const char *type, const char *protocol,
                    int dport, unsigned pt, bool bw_indep, unsigned bw,
                    const char *ptname, unsigned clockrate, unsigned channels,
                    const char *fmtp)
{
    assert (*sdp == NULL);
    return NULL;
}

char *sdp_AddAttribute (char **sdp, const char *name, const char *fmt, ...)
{
    assert (*sdp == NULL);
    return NULL;
}

int sout_AccessOutControl (sout_access_out_t *out, int query, ...)
{
    assert (0);
}

void sout_AccessOutDelete (sout_access_out_t *out)
{
    assert (0);
}

#undef sout_AccessOutNew
sout_access_out_t *sout_AccessOutNew (vlc_object_t *obj,
                                      const char *access, const char *name)
{
    msg_Err (obj, "Output support not compiled-in!");
    return NULL;
}

ssize_t sout_AccessOutRead (sout_access_out_t *out, block_t *block)
{
    assert (0);
}

int sout_AccessOutSeek (sout_access_out_t *out, off_t offset)
{
    assert (0);
}

ssize_t sout_AccessOutWrite (sout_access_out_t *out, block_t *block)
{
    assert (0);
}

#undef sout_AnnounceRegisterSDP
session_descriptor_t *sout_AnnounceRegisterSDP (vlc_object_t *obj,
                                                const char *sdp,
                                                const char *dst)
{
    msg_Err (obj, "SDP export not compiled-in!");
    return NULL;
}

#undef sout_AnnounceUnRegister
sout_AnnounceUnRegister (vlc_object_t *obj, session_descriptor_t *d)
{
    assert (0);
}

#undef sout_EncoderCreate
encoder_t *sout_EncoderCreate( vlc_object_t *p_this )
{
    msg_Err (p_this, "Encoding support not compiled-in!");
    return NULL;
}

sout_input_t *sout_MuxAddStream (sout_mux_t *mux, es_format_t *fmt)
{
    assert (0);
}

void sout_MuxDelete (sout_mux_t *mux)
{
    assert (0);
}

void sout_MuxDeleteStream (sout_mux_t *mux, sout_input_t *input)
{
    assert (0);
}

int sout_MuxGetStream (sout_mux_t *p_mux, int i_blocks, mtime_t *pi_dts)
{
    assert (0);
}

sout_mux_t *sout_MuxNew (sout_instance_t *instance, const char *mux,
                         sout_access_out_t *out)
{
    assert (0);
}

void sout_MuxSendBuffer (sout_mux_t *mux, sout_input_t *input, block_t *block)
{
    assert (0);
}

void sout_StreamChainDelete (sout_stream_t *p_first, sout_stream_t *p_last)
{
    assert (0);
}

sout_stream_t *sout_StreamChainNew (sout_instance_t *p_sout, char *psz_chain,
                                    sout_stream_t *p_next,
                                    sout_stream_t **pp_last)
{
    assert (0);
}

char *vlc_sdp_Start (vlc_object_t *obj, const char *cfg,
                     const struct sockaddr *src, size_t srclen,
                     const struct sockaddr *addr, size_t addrlen)
{
    return NULL;
}
#endif /* !ENABLE_SOUT */

#ifndef ENABLE_VLM
# include <vlc_vlm.h>

int vlm_Control (vlm_t *vlm, int query, ...)
{
    VLC_UNUSED (vlm);
    assert (0);
}

void vlm_Delete (vlm_t *vlm)
{
    VLC_UNUSED (vlm);
    assert (0);
}

int vlm_ExecuteCommand (vlm_t *vlm, const char *cmd, vlm_message_t **pm)
{
    VLC_UNUSED (vlm);
    VLC_UNUSED (cmd);
    VLC_UNUSED (pm);
    assert (0);
}

vlm_message_t *vlm_MessageAdd (vlm_message_t *a, vlm_message_t *b)
{
    VLC_UNUSED (a);
    VLC_UNUSED (b);
    assert (0);
}

void vlm_MessageDelete (vlm_message_t *m)
{
    VLC_UNUSED (m);
    assert (0);
}

vlm_message_t *vlm_MessageSimpleNew (const char *a)
{
    VLC_UNUSED (a);
    return NULL;
}

vlm_message_t *vlm_MessageNew (const char *a, const char *fmt, ...)
{
    VLC_UNUSED (a);
    VLC_UNUSED (fmt);
    return vlm_MessageSimpleNew (a);
}

#undef vlm_New
vlm_t *vlm_New (vlc_object_t *obj)
{
     msg_Err (obj, "VLM not compiled-in!");
     return NULL;
}
#endif /* !ENABLE_VLM */
