/*****************************************************************************
 * udp.c: standard stream output module
 *****************************************************************************
 * Copyright (C) 2003-2011 VLC authors and VideoLAN
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>

#include <vlc_network.h>
#include <vlc_memstream.h>
#include "sdp_helper.h"

struct sout_stream_udp
{
    sout_access_out_t *access;
    sout_mux_t *mux;
    session_descriptor_t *sap;
    int fd;
    uint_fast16_t mtu;
};

static void *Add(sout_stream_t *stream, const es_format_t *fmt)
{
    struct sout_stream_udp *sys = stream->p_sys;

    return sout_MuxAddStream(sys->mux, fmt);
}

static void Del(sout_stream_t *stream, void *id)
{
    struct sout_stream_udp *sys = stream->p_sys;

    sout_MuxDeleteStream(sys->mux, id);
}

static int Send(sout_stream_t *stream, void *id, block_t *block)
{
    struct sout_stream_udp *sys = stream->p_sys;

    return sout_MuxSendBuffer(sys->mux, id, block);
}

static void Flush(sout_stream_t *stream, void *id)
{
    struct sout_stream_udp *sys = stream->p_sys;

    sout_MuxFlush(sys->mux, id);
}

#define SOUT_CFG_PREFIX "sout-udp-"

static session_descriptor_t *CreateSDP(vlc_object_t *obj, int fd)
{
    union {
        struct sockaddr addr;
        struct sockaddr_in in;
        struct sockaddr_in6 in6;
    } src, dst;
    socklen_t srclen = sizeof (srclen), dstlen = sizeof (dst);
    char dhost[INET6_ADDRSTRLEN];
    unsigned short dport;

    if (getsockname(fd, &src.addr, &srclen)
     || getpeername(fd, &dst.addr, &dstlen)) {
        int val = errno;

        msg_Err(obj, "cannot format SDP: %s", vlc_strerror_c(val));
        return NULL;
    }

    switch (dst.addr.sa_family) {
        case AF_INET:
            inet_ntop(AF_INET, &dst.in.sin_addr, dhost, sizeof (dhost));
            dport = dst.in.sin_port;
            break;
        case AF_INET6:
            inet_ntop(AF_INET6, &dst.in6.sin6_addr, dhost, sizeof (dhost));
            dport = dst.in6.sin6_port;
            break;
        default:
            return NULL;
    }

    struct vlc_memstream sdp;

    if (vlc_sdp_Start(&sdp, obj, SOUT_CFG_PREFIX,
                      &src.addr, srclen, &dst.addr, dstlen))
        return NULL;

    vlc_memstream_printf(&sdp, "m=video %d udp mpeg\r\n", ntohs(dport));

    if (vlc_memstream_close(&sdp) != 0)
        return NULL;

    /* Register the SDP with the SAP thread */
    session_descriptor_t *session;

    msg_Dbg(obj, "Generated SDP:\n%s", sdp.ptr);
    session = sout_AnnounceRegisterSDP(obj, sdp.ptr, dhost);
    free(sdp.ptr);
    return session;
}

static int Control(sout_stream_t *stream, int query, va_list args)
{
    switch (query) {
        case SOUT_STREAM_IS_SYNCHRONOUS:
            *va_arg(args, bool *) = true;
            break;

        default:
            return VLC_EGENERIC;
    }

    (void) stream;
    return VLC_SUCCESS;
}

static ssize_t AccessOutWrite(sout_access_out_t *access, block_t *block)
{
    struct sout_stream_udp *sys = access->p_sys;
    ssize_t total = 0;

    while (block != NULL) {
        struct iovec iov[16];
        block_t *unsent = block;
        unsigned iovlen = 0;
        size_t tosend = 0;

        /* Count how many blocks to gather */
        do {
            if (iovlen >= ARRAY_SIZE(iov))
                break;
            if (unsent->i_buffer + tosend > sys->mtu && likely(iovlen > 0))
                break;

            iov[iovlen].iov_base = unsent->p_buffer;
            iov[iovlen].iov_len = unsent->i_buffer;
            iovlen++;
            tosend += unsent->i_buffer;
            unsent = unsent->p_next;
        } while (unsent != NULL);

        /* Send */
        struct msghdr hdr = { .msg_iov = iov, .msg_iovlen = iovlen };
        ssize_t val = sendmsg(sys->fd, &hdr, 0);

        if (val < 0)
            msg_Err(access, "send error: %s", vlc_strerror_c(errno));
        else
            total += val;

        /* Free */
        do {
            block_t *next = block->p_next;

            block_Release(block);
            block = next;
        } while (block != unsent);
    }

    return total;
}

static void Close(vlc_object_t *obj)
{
    sout_stream_t *stream = (sout_stream_t *)obj;
    struct sout_stream_udp *sys = stream->p_sys;

    if (sys->sap != NULL)
        sout_AnnounceUnRegister(stream, sys->sap);

    sout_MuxDelete(sys->mux);
    sout_AccessOutDelete(sys->access);
    net_Close(sys->fd);
    free(sys);
}

static const struct sout_stream_operations ops = {
    Add, Del, Send, Control, Flush,
};

static const char *const chain_options[] = {
    "avformat", "dst", "sap", "name", "description", NULL
};

#define DEFAULT_PORT 1234

static int Open(vlc_object_t *obj)
{
    sout_stream_t *stream = (sout_stream_t *)obj;
    sout_access_out_t *access = NULL;
    const char *muxmod = "ts";
    int ret;

    /* --sout '#std{...}' option backward compatibility */
    if (strcmp(stream->psz_name, "std") == 0
     || strcmp(stream->psz_name, "standard") == 0) {
        config_chain_t *c = NULL;

        for (c = stream->p_cfg; c != NULL; c = c->p_next)
             if (strcmp(c->psz_name, "access") == 0)
                 break;

        if (c == NULL || c->psz_value == NULL) /* default is file, not for us */
            return VLC_ENOTSUP;
        if (strcmp(c->psz_value, "udp"))
            return VLC_ENOTSUP;

        msg_Info(stream, "\"#standard{access=udp,mux=ts,...}\" is deprecated. "
                 "Use \"#udp{...}\" instead.");
    }

    config_ChainParse(stream, SOUT_CFG_PREFIX, chain_options, stream->p_cfg);

    char *dst = var_GetNonEmptyString(stream, SOUT_CFG_PREFIX "dst");
    if (dst == NULL) {
        msg_Err(stream, "missing required destination");
        return VLC_EINVAL;
    }

    const char *dhost;
    char *end;
    int dport = DEFAULT_PORT;

    if (dst[0] == '[') {
        dhost = dst;
        end = strchr(dst, ']');

        if (end != NULL)
            *(end++) = '\0';
    } else {
        dhost = dst;
        end = strchr(dst, ':');
    }

    if (end != NULL && *end == ':') {
        *(end++) = '\0';
        dport = atoi(&end[1]);
    }

    int fd = net_ConnectDgram(stream, dhost, dport, -1, IPPROTO_UDP);
    free(dst);
    if (fd == -1) {
        int val = errno;

        msg_Err(stream, "cannot reach destination: %s", vlc_strerror_c(val));
        return val ? -val : VLC_EGENERIC;
    }

    if (var_GetBool(stream, SOUT_CFG_PREFIX "avformat")
     && var_Create(stream, "sout-avformat-mux",
                   VLC_VAR_STRING) == VLC_SUCCESS) {
        var_SetString(stream, "sout-avformat-mux", "mpegts");
        muxmod = "avformat";
    }

    struct sout_stream_udp *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL)) {
        ret = VLC_ENOMEM;
        goto error;
    }

    access = vlc_object_create(stream, sizeof (*access));
    if (unlikely(access == NULL)) {
        ret = VLC_ENOMEM;
        goto error;
    }

    access->p_module = NULL;
    access->psz_access = strdup("udp");
    access->psz_path = NULL;
    access->p_sys = sys;
    access->pf_seek = NULL;
    access->pf_read = NULL;
    access->pf_write = AccessOutWrite;
    access->pf_control = NULL;
    access->p_cfg = NULL;
    sys->access = access;
    sys->fd = fd;
    sys->mtu = var_InheritInteger(stream, "mtu");

    sout_mux_t *mux = sout_MuxNew(access, muxmod);
    if (mux == NULL) {
        ret = VLC_ENOTSUP;
        goto error;
    }
    sys->mux = mux;

    if (var_GetBool(stream, SOUT_CFG_PREFIX "sap"))
        sys->sap = CreateSDP(VLC_OBJECT(stream), fd);
    else
        sys->sap = NULL;

    stream->p_sys = sys;
    stream->ops = &ops;
    return VLC_SUCCESS;

error:
    if (access != NULL)
        sout_AccessOutDelete(access);
    free(sys);
    net_Close(fd);
    return ret;
}

#define AVF_TEXT N_("Use libavformat")
#define AVF_LONGTEXT N_("Use libavformat instead of dvbpsi to mux MPEG TS.")
#define DEST_TEXT N_("Destination")
#define DEST_LONGTEXT N_( \
    "Destination address and port (colon-separated) for the stream.")
#define SAP_TEXT N_("SAP announcement")
#define SAP_LONGTEXT N_("Announce this stream as a session with SAP.")
#define NAME_TEXT N_("SAP name")
#define NAME_LONGTEXT N_( \
    "Name of the stream that will be announced with SAP.")
#define DESC_TEXT N_("SAP description")
#define DESC_LONGTEXT N_( \
    "Short description of the stream that will be announced with SAP.")

vlc_module_begin()
    set_shortname(N_("UDP"))
    set_description(N_("UDP stream output"))
    set_capability("sout output", 40)
    add_shortcut("standard", "std", "udp")
    set_subcategory(SUBCAT_SOUT_STREAM)

    add_bool(SOUT_CFG_PREFIX "avformat", false, AVF_TEXT, AVF_LONGTEXT)
    add_string(SOUT_CFG_PREFIX "dst", "", DEST_TEXT, DEST_LONGTEXT)
    add_bool(SOUT_CFG_PREFIX "sap", false, SAP_TEXT, SAP_LONGTEXT)
    add_string(SOUT_CFG_PREFIX "name", "", NAME_TEXT, NAME_LONGTEXT)
    add_string(SOUT_CFG_PREFIX "description", "", DESC_TEXT, DESC_LONGTEXT)

    set_callbacks(Open, Close)
vlc_module_end()
