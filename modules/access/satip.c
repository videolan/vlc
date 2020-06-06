/*****************************************************************************
 * satip.c: SAT>IP input module
 *****************************************************************************
 * Copyright © 2016 VLC authors and VideoLAN
 * Copyright © 2016 jusst technologies GmbH
 * Copyright © 2016 Videolabs SAS
 * Copyright © 2016 Julian Scheel
 *
 * Authors: Julian Scheel <julian@jusst.de>
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

#include "config.h"

#include <unistd.h>
#include <ctype.h>
#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_network.h>
#include <vlc_block.h>
#include <vlc_queue.h>
#include <vlc_rand.h>
#include <vlc_url.h>
#include <vlc_interrupt.h>

#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#define RTSP_DEFAULT_PORT 554
#define RTSP_RECEIVE_BUFFER 2048
#define RTP_HEADER_SIZE 12
#define VLEN 100
#define KEEPALIVE_INTERVAL 60
#define KEEPALIVE_MARGIN 5

static int satip_open(vlc_object_t *);
static void satip_close(vlc_object_t *);

#define MULTICAST_TEXT N_("Request multicast stream")
#define MULTICAST_LONGTEXT N_("Request server to send stream as multicast")

#define SATIP_HOST_TEXT N_("Host")

vlc_module_begin()
    set_shortname("satip")
    set_description( N_("SAT>IP Receiver Plugin") )
    set_capability("access", 201)
    set_callbacks(satip_open, satip_close)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACCESS)
    add_obsolete_integer("satip-buffer") /* obsolete since 4.0.0 */
    add_bool("satip-multicast", false, MULTICAST_TEXT, MULTICAST_LONGTEXT, true)
    add_string("satip-host", "", SATIP_HOST_TEXT, SATIP_HOST_TEXT, true)
    change_safe()
    add_shortcut("rtsp", "satip")
vlc_module_end()

enum rtsp_state {
    RTSP_IDLE,
    RTSP_DESCRIBE,
    RTSP_SETUP,
    RTSP_PLAY,
    RTSP_RUNNING
};

enum rtsp_result {
    RTSP_RESULT_OK = 200,
};

#define UDP_ADDRESS_LEN 16
typedef struct
{
    char *content_base;
    char *control;
    char session_id[64];
    uint16_t stream_id;
    int keepalive_interval;

    char udp_address[UDP_ADDRESS_LEN];
    uint16_t udp_port;

    int tcp_sock;
    int udp_sock;
    int rtcp_sock;

    enum rtsp_state state;
    int cseq;

    vlc_queue_t queue;
    vlc_thread_t thread;
    uint16_t last_seq_nr;

    bool woken;
} access_sys_t;

VLC_FORMAT(3, 4)
static void net_Printf(stream_t *access, int fd, const char *fmt, ...)
{
    va_list ap;
    char *str;
    int val;

    va_start(ap, fmt);
    val = vasprintf(&str, fmt, ap);
    va_end(ap);

    if (val >= 0) {
        net_Write(access, fd, str, val);
        free(str);
    }
}

static void parse_session(char *request_line, char *session, unsigned max, int *timeout) {
    char *state;
    char *tok;

    tok = strtok_r(request_line, ";", &state);
    if (tok == NULL)
        return;
    memcpy(session, tok, __MIN(strlen(tok), max - 1));

    while ((tok = strtok_r(NULL, ";", &state)) != NULL) {
        if (strncmp(tok, "timeout=", 8) == 0) {
            *timeout = atoi(tok + 8);
            if (*timeout > 5)
                *timeout -= KEEPALIVE_MARGIN;
            else if (*timeout > 0)
                *timeout = 1;
        }
    }
}

static int parse_port(char *str, uint16_t *port)
{
    int p = atoi(str);
    if (p < 0 || p > UINT16_MAX)
        return VLC_EBADVAR;

    *port = p;

    return 0;
}

static int parse_transport(stream_t *access, char *request_line) {
    access_sys_t *sys = access->p_sys;
    char *state;
    char *tok;
    int err;

    tok = strtok_r(request_line, ";", &state);
    if (tok == NULL || strncmp(tok, "RTP/AVP", 7) != 0)
        return VLC_EGENERIC;

    tok = strtok_r(NULL, ";", &state);
    if (tok == NULL || strncmp(tok, "multicast", 9) != 0)
        return 0;

    while ((tok = strtok_r(NULL, ";", &state)) != NULL) {
        if (strncmp(tok, "destination=", 12) == 0) {
            memcpy(sys->udp_address, tok + 12, __MIN(strlen(tok + 12), UDP_ADDRESS_LEN - 1));
        } else if (strncmp(tok, "port=", 5) == 0) {
            char port[6];
            char *end;

            memset(port, 0x00, 6);
            memcpy(port, tok + 5, __MIN(strlen(tok + 5), 5));
            if ((end = strstr(port, "-")) != NULL)
                *end = '\0';
            err = parse_port(port, &sys->udp_port);
            if (err)
                return err;
        }
    }

    return 0;
}

/*
 * Semi-interruptible net_Gets replacement.
 * If an interruption is occuring it will fallback to non-interruptible read
 * with a given timeout before it returns.
 *
 * interrupted: Informs the caller whether an interrupt occured or not
 */
static char *net_readln_timeout(vlc_object_t *obj, int fd, int timeout, bool *interrupted)
{
    char *buf = NULL;
    size_t size = 0, len = 0;
    bool intr = false;

    for (;;)
    {
        if (len == size)
        {
            if (unlikely(size >= (1 << 16)))
            {
                errno = EMSGSIZE;
                goto error; /* put sane buffer size limit */
            }

            char *newbuf = realloc(buf, size + 1024);
            if (unlikely(newbuf == NULL))
                goto error;
            buf = newbuf;
            size += 1024;
        }
        assert(len < size);

        ssize_t val = 0;
        if (!intr) {
            val = vlc_recv_i11e(fd, buf + len, size - len, MSG_PEEK);
            if (val <= 0 && errno == EINTR) {
                intr = true;
                if (interrupted)
                    *interrupted = true;
                continue;
            }
            if (val <= 0)
                goto error;
        } else {
            struct pollfd pfd = {
                .fd = fd,
                .events = POLLIN,
            };
            int ret;

            while((ret = poll(&pfd, 1, timeout)) < 0)
                ;

            val = recv(fd, buf + len, size - len, MSG_PEEK);
            if (val <= 0)
                goto error;
        }

        char *end = memchr(buf + len, '\n', val);
        if (end != NULL)
            val = (end + 1) - (buf + len);
        if (recv(fd, buf + len, val, 0) != val)
            goto error;
        len += val;
        if (end != NULL)
            break;
    }

    assert(len > 0);
    buf[--len] = '\0';
    if (len > 0 && buf[--len] == '\r')
        buf[len] = '\0';
    return buf;
error:
    msg_Err(obj, "read error: %s", vlc_strerror_c(errno));
    free(buf);
    return NULL;
}

#define skip_whitespace(x) while(*x == ' ') x++
static enum rtsp_result rtsp_handle(stream_t *access, bool *interrupted) {
    access_sys_t *sys = access->p_sys;
    uint8_t buffer[512];
    int rtsp_result = 0;
    bool have_header = false;
    size_t content_length = 0;
    size_t read = 0;
    char *in, *val;

    /* Parse header */
    while (!have_header) {
        in = net_readln_timeout((vlc_object_t*)access, sys->tcp_sock, 5000,
                interrupted);
        if (in == NULL)
            break;

        if (strncmp(in, "RTSP/1.0 ", 9) == 0) {
            rtsp_result = atoi(in + 9);
        } else if (strncmp(in, "Content-Base:", 13) == 0) {
            free(sys->content_base);

            val = in + 13;
            skip_whitespace(val);

            sys->content_base = strdup(val);
        } else if (strncmp(in, "Content-Length:", 15) == 0) {
            val = in + 16;
            skip_whitespace(val);

            content_length = atoi(val);
        } else if (strncmp("Session:", in, 8) == 0) {
            val = in + 8;
            skip_whitespace(val);

            parse_session(val, sys->session_id, 64, &sys->keepalive_interval);
        } else if (strncmp("Transport:", in, 10) == 0) {
            val = in + 10;
            skip_whitespace(val);

            if (parse_transport(access, val) != 0) {
                rtsp_result = VLC_EGENERIC;
                break;
            }
        } else if (strncmp("com.ses.streamID:", in, 17) == 0) {
            val = in + 17;
            skip_whitespace(val);

            sys->stream_id = atoi(val);
        } else if (in[0] == '\0') {
            have_header = true;
        }

        free(in);
    }

    /* Discard further content */
    while (content_length > 0 &&
            (read = net_Read(access, sys->tcp_sock, buffer, __MIN(sizeof(buffer), content_length))))
        content_length -= read;

    return rtsp_result;
}

#ifdef HAVE_RECVMMSG
static void satip_cleanup_blocks(void *data)
{
    block_t **input_blocks = data;

    for (size_t i = 0; i < VLEN; i++)
        if (input_blocks[i] != NULL)
            block_Release(input_blocks[i]);
}
#endif

static int check_rtp_seq(stream_t *access, block_t *block)
{
    access_sys_t *sys = access->p_sys;
    uint16_t seq_nr = block->p_buffer[2] << 8 | block->p_buffer[3];

    if (seq_nr == sys->last_seq_nr) {
        msg_Warn(access, "Received duplicate packet (seq_nr=%"PRIu16")", seq_nr);
        return VLC_EGENERIC;
    } else if (seq_nr < (uint16_t)(sys->last_seq_nr + 1)) {
        msg_Warn(access, "Received out of order packet (seq_nr=%"PRIu16" < %"PRIu16")",
                seq_nr, sys->last_seq_nr);
        return VLC_EGENERIC;
    } else if (++sys->last_seq_nr > 1 && seq_nr > sys->last_seq_nr) {
        msg_Warn(access, "Gap in seq_nr (%"PRIu16" > %"PRIu16"), probably lost a packet",
                seq_nr, sys->last_seq_nr);
    }

    sys->last_seq_nr = seq_nr;
    return 0;
}

static void satip_teardown(void *data) {
    stream_t *access = data;
    access_sys_t *sys = access->p_sys;
    int ret;

    if (sys->tcp_sock > 0) {
        if (sys->session_id[0] > 0) {
            char discard_buf[32];
            struct pollfd pfd = {
                .fd = sys->tcp_sock,
                .events = POLLOUT,
            };
            char *msg;

            int len = asprintf(&msg, "TEARDOWN %s RTSP/1.0\r\n"
                    "CSeq: %d\r\n"
                    "Session: %s\r\n\r\n",
                    sys->control, sys->cseq++, sys->session_id);
            if (len < 0)
                return;

            /* make socket non-blocking, to avoid blocking when output buffer
             * has not enough space */
#ifndef _WIN32
            fcntl(sys->tcp_sock, F_SETFL, fcntl(sys->tcp_sock, F_GETFL) | O_NONBLOCK);
#else
            ioctlsocket(sys->tcp_sock, FIONBIO, &(unsigned long){ 1 });
#endif

            for (int sent = 0; sent < len;) {
                ret = poll(&pfd, 1, 5000);
                if (ret == 0) {
                    msg_Err(access, "Timed out sending RTSP teardown\n");
                    free(msg);
                    return;
                }

                ret = vlc_send(sys->tcp_sock, msg + sent, len, 0);
                if (ret < 0) {
                    msg_Err(access, "Failed to send RTSP teardown: %d\n", ret);
                    free(msg);
                    return;
                }
                sent += ret;
            }
            free(msg);

            if (rtsp_handle(access, NULL) != RTSP_RESULT_OK) {
                msg_Err(access, "Failed to teardown RTSP session");
                return;
            }

            /* Some SATIP servers send a few empty extra bytes after TEARDOWN.
             * Try to read them, to avoid a TCP socket reset */
            while (recv(sys->tcp_sock, discard_buf, sizeof(discard_buf), 0) > 0);

            /* Extra sleep for compatibility with some satip servers, that
             * can't handle new sessions right after teardown */
            vlc_tick_sleep(VLC_TICK_FROM_MS(150));
        }
    }
}

#define RECV_TIMEOUT VLC_TICK_FROM_SEC(2)
static void *satip_thread(void *data) {
    stream_t *access = data;
    access_sys_t *sys = access->p_sys;
    int sock = sys->udp_sock;
    vlc_tick_t last_recv = vlc_tick_now();
    ssize_t len;
    vlc_tick_t next_keepalive = vlc_tick_now() + vlc_tick_from_sec(sys->keepalive_interval);
#ifdef HAVE_RECVMMSG
    struct mmsghdr msgs[VLEN];
    struct iovec iovecs[VLEN];
    block_t *input_blocks[VLEN];
    int retval;

    for (size_t i = 0; i < VLEN; i++) {
        memset(&msgs[i], 0, sizeof (msgs[i]));
        msgs[i].msg_hdr.msg_iov = &iovecs[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
        iovecs[i].iov_base = NULL;
        iovecs[i].iov_len = RTSP_RECEIVE_BUFFER;
        input_blocks[i] = NULL;
    }
#else
    struct pollfd ufd;

    ufd.fd = sock;
    ufd.events = POLLIN;
#endif

    while (last_recv > vlc_tick_now() - RECV_TIMEOUT) {
#ifdef HAVE_RECVMMSG
        for (size_t i = 0; i < VLEN; i++) {
            if (input_blocks[i] != NULL)
                continue;

            input_blocks[i] = block_Alloc(RTSP_RECEIVE_BUFFER);
            if (unlikely(input_blocks[i] == NULL))
                break;

            iovecs[i].iov_base = input_blocks[i]->p_buffer;
        }

        vlc_cleanup_push(satip_cleanup_blocks, input_blocks);
        retval = recvmmsg(sock, msgs, VLEN, MSG_WAITFORONE, NULL);
        vlc_cleanup_pop();
        if (retval == -1)
            continue;

        last_recv = vlc_tick_now();
        for (int i = 0; i < retval; ++i) {
            block_t *block = input_blocks[i];

            len = msgs[i].msg_len;
            if (check_rtp_seq(access, block))
                continue;

            block->p_buffer += RTP_HEADER_SIZE;
            block->i_buffer = len - RTP_HEADER_SIZE;
            vlc_queue_Enqueue(&sys->queue, block);
            input_blocks[i] = NULL;
        }
#else
        if (poll(&ufd, 1, 20) == -1)
            continue;

        block_t *block = block_Alloc(RTSP_RECEIVE_BUFFER);
        if (block == NULL) {
            msg_Err(access, "Failed to allocate memory for input buffer");
            break;
        }

        block_cleanup_push(block);
        len = recv(sock, block->p_buffer, RTSP_RECEIVE_BUFFER, 0);
        vlc_cleanup_pop();

        if (len < RTP_HEADER_SIZE) {
            block_Release(block);
            continue;
        }

        if (check_rtp_seq(access, block)) {
            block_Release(block);
            continue;
        }
        last_recv = vlc_tick_now();
        block->p_buffer += RTP_HEADER_SIZE;
        block->i_buffer = len - RTP_HEADER_SIZE;
        vlc_queue_Enqueue(&sys->queue, block);
#endif

        if (sys->keepalive_interval > 0 && vlc_tick_now() > next_keepalive) {
            net_Printf(access, sys->tcp_sock,
                    "OPTIONS %s RTSP/1.0\r\n"
                    "CSeq: %d\r\n"
                    "Session: %s\r\n\r\n",
                    sys->control, sys->cseq++, sys->session_id);
            if (rtsp_handle(access, NULL) != RTSP_RESULT_OK)
                msg_Warn(access, "Failed to keepalive RTSP session");

            next_keepalive = vlc_tick_now() + vlc_tick_from_sec(sys->keepalive_interval);
        }
    }

#ifdef HAVE_RECVMMSG
    satip_cleanup_blocks(input_blocks);
#endif
    msg_Dbg(access, "timed out waiting for data...");
    vlc_queue_Kill(&sys->queue, &sys->woken);
    return NULL;
}

static block_t* satip_block(stream_t *access, bool *restrict eof) {
    access_sys_t *sys = access->p_sys;
    block_t *block = vlc_queue_DequeueKillable(&sys->queue, &sys->woken);

    if (block == NULL)
        *eof = true;

    return block;
}

static int satip_control(stream_t *access, int i_query, va_list args) {
    bool *pb_bool;

    switch(i_query)
    {
        case STREAM_CAN_CONTROL_PACE:
        case STREAM_CAN_SEEK:
        case STREAM_CAN_PAUSE:
            pb_bool = va_arg(args, bool *);
            *pb_bool = false;
            break;

        case STREAM_GET_PTS_DELAY:
            *va_arg(args, vlc_tick_t *) =
                VLC_TICK_FROM_MS(var_InheritInteger(access, "live-caching"));
            break;

        default:
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

/* Bind two adjacent free ports, of which the first one is even (for RTP data)
 * and the second is odd (RTCP). This is a requirement of the satip
 * specification */
static int satip_bind_ports(stream_t *access)
{
    access_sys_t *sys = access->p_sys;
    uint8_t rnd;

    vlc_rand_bytes(&rnd, 1);
    sys->udp_port = 9000 + (rnd * 2); /* randomly chosen, even start point */
    while (sys->udp_sock < 0) {
        sys->udp_sock = net_OpenDgram(access, "0.0.0.0", sys->udp_port, NULL,
                0, IPPROTO_UDP);
        if (sys->udp_sock < 0) {
            if (sys->udp_port == 65534)
                break;

            sys->udp_port += 2;
            continue;
        }

        sys->rtcp_sock = net_OpenDgram(access, "0.0.0.0", sys->udp_port + 1, NULL,
                0, IPPROTO_UDP);
        if (sys->rtcp_sock < 0) {
            close(sys->udp_sock);
            sys->udp_port += 2;
            continue;
        }
    }

    if (sys->udp_sock < 0) {
        msg_Err(access, "Could not open two adjacent ports for RTP and RTCP data");
        return VLC_EGENERIC;
    }

    return 0;
}

static int satip_open(vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;
    access_sys_t *sys;
    vlc_url_t url;

    bool multicast = var_InheritBool(access, "satip-multicast");

    access->p_sys = sys = vlc_obj_calloc(obj, 1, sizeof(*sys));
    if (sys == NULL)
        return VLC_ENOMEM;

    msg_Dbg(access, "try to open '%s'", access->psz_url);

    char *psz_host = var_InheritString(access, "satip-host");

    sys->udp_sock = -1;
    sys->rtcp_sock = -1;
    sys->tcp_sock = -1;

    /* convert url to lowercase, some famous m3u playlists for satip contain
     * uppercase parameters while most (all?) satip servers do only understand
     * parameters matching lowercase spelling as defined in the specification
     * */
    char *psz_lower_url = strdup(access->psz_url);
    if (psz_lower_url == NULL)
    {
        free( psz_host );
        return VLC_ENOMEM;
    }

    for (unsigned i = 0; i < strlen(psz_lower_url); i++)
        psz_lower_url[i] = tolower(psz_lower_url[i]);

    vlc_UrlParse(&url, psz_lower_url);
    if (url.i_port <= 0)
        url.i_port = RTSP_DEFAULT_PORT;
    if (psz_host == NULL && url.psz_host )
        psz_host = strdup(url.psz_host);
    if (psz_host == NULL )
        goto error;

    if (url.psz_host == NULL || url.psz_host[0] == '\0')
    {
        msg_Dbg(access, "malformed URL: %s", psz_lower_url);
        goto error;
    }

    msg_Dbg(access, "connect to host '%s'", psz_host);
    sys->tcp_sock = net_Connect(access, psz_host, url.i_port, SOCK_STREAM, 0);
    if (sys->tcp_sock < 0) {
        msg_Err(access, "Failed to connect to RTSP server %s:%d",
                psz_host, url.i_port);
        goto error;
    }
    setsockopt (sys->tcp_sock, SOL_SOCKET, SO_KEEPALIVE, &(int){ 1 }, sizeof (int));

    if (asprintf(&sys->content_base, "rtsp://%s:%d/", psz_host,
             url.i_port) < 0) {
        sys->content_base = NULL;
        goto error;
    }

    sys->last_seq_nr = 0;
    sys->keepalive_interval = (KEEPALIVE_INTERVAL - KEEPALIVE_MARGIN);

    vlc_url_t setup_url = url;

    // substitute "sat.ip" if present with an the host IP that was fetched during device discovery
    if( !strncasecmp( setup_url.psz_host, "sat.ip", 6 ) ) {
        setup_url.psz_host = psz_host;
    }

    // reverse the satip protocol trick, as SAT>IP believes to be RTSP
    if( setup_url.psz_protocol == NULL ||
        strncasecmp( setup_url.psz_protocol, "satip", 5 ) == 0 )
    {
        setup_url.psz_protocol = (char *)"rtsp";
    }

    char *psz_setup_url = vlc_uri_compose(&setup_url);
    if( psz_setup_url == NULL )
        goto error;

    if (multicast) {
        net_Printf(access, sys->tcp_sock,
                "SETUP %s RTSP/1.0\r\n"
                "CSeq: %d\r\n"
                "Transport: RTP/AVP;multicast\r\n\r\n",
                psz_setup_url, sys->cseq++);
    } else {
        /* open UDP socket to acquire a free port to use */
        if (satip_bind_ports(access)) {
            free(psz_setup_url);
            goto error;
        }

        net_Printf(access, sys->tcp_sock,
                "SETUP %s RTSP/1.0\r\n"
                "CSeq: %d\r\n"
                "Transport: RTP/AVP;unicast;client_port=%d-%d\r\n\r\n",
                psz_setup_url, sys->cseq++, sys->udp_port, sys->udp_port + 1);
    }
    free(psz_setup_url);

    bool interrupted = false;
    if (rtsp_handle(access, &interrupted) != RTSP_RESULT_OK) {
        msg_Err(access, "Failed to setup RTSP session");
        goto error;
    }

    if (asprintf(&sys->control, "%sstream=%d", sys->content_base, sys->stream_id) < 0) {
        sys->control = NULL;
        goto error;
    }

    if (interrupted) {
        msg_Warn(access, "SETUP was interrupted, abort startup");
        goto error;
    }

    /* Extra sleep for compatibility with some satip servers, that
     * can't handle PLAY right after SETUP */
    if (vlc_msleep_i11e(VLC_TICK_FROM_MS(50)) < 0)
        goto error;

    /* Open UDP socket for reading if not done */
    if (multicast) {
        sys->udp_sock = net_OpenDgram(access, sys->udp_address, sys->udp_port, "", sys->udp_port, IPPROTO_UDP);
        if (sys->udp_sock < 0) {
            msg_Err(access, "Failed to open UDP socket for listening.");
            goto error;
        }

        sys->rtcp_sock = net_OpenDgram(access, sys->udp_address, sys->udp_port + 1, "", sys->udp_port + 1, IPPROTO_UDP);
        if (sys->rtcp_sock < 0) {
            msg_Err(access, "Failed to open RTCP socket for listening.");
            goto error;
        }
    }

    net_Printf(access, sys->tcp_sock,
            "PLAY %s RTSP/1.0\r\n"
            "CSeq: %d\r\n"
            "Session: %s\r\n\r\n",
            sys->control, sys->cseq++, sys->session_id);

    if (rtsp_handle(access, NULL) != RTSP_RESULT_OK) {
        msg_Err(access, "Failed to play RTSP session");
        goto error;
    }

    vlc_queue_Init(&sys->queue, offsetof (block_t, p_next));

    if (vlc_clone(&sys->thread, satip_thread, access, VLC_THREAD_PRIORITY_INPUT)) {
        msg_Err(access, "Failed to create worker thread.");
        goto error;
    }

    access->pf_control = satip_control;
    access->pf_block = satip_block;

    free(psz_host);
    free(psz_lower_url);
    return VLC_SUCCESS;

error:
    free(psz_host);
    free(psz_lower_url);
    vlc_UrlClean(&url);

    satip_teardown(access);

    if (sys->udp_sock >= 0)
        net_Close(sys->udp_sock);
    if (sys->rtcp_sock >= 0)
        net_Close(sys->rtcp_sock);
    if (sys->tcp_sock >= 0)
        net_Close(sys->tcp_sock);

    free(sys->content_base);
    free(sys->control);
    return VLC_EGENERIC;
}

static void satip_close(vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;
    access_sys_t *sys = access->p_sys;

    vlc_cancel(sys->thread);
    vlc_join(sys->thread, NULL);

    satip_teardown(access);

    block_ChainRelease(vlc_queue_DequeueAll(&sys->queue));
    net_Close(sys->udp_sock);
    net_Close(sys->rtcp_sock);
    net_Close(sys->tcp_sock);
    free(sys->content_base);
    free(sys->control);
}
