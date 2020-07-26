/*****************************************************************************
 * rist.c: RIST (Reliable Internet Stream Transport) input module
 *****************************************************************************
 * Copyright (C) 2018, DVEO, the Broadcast Division of Computer Modules, Inc.
 * Copyright (C) 2018, SipRadius LLC
 *
 * Authors: Sergio Ammirata <sergio@ammirata.net>
 *          Daniele Lacamera <root@danielinux.net>
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

#include <vlc_common.h>
#include <vlc_interrupt.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_queue.h>
#include <vlc_threads.h>
#include <vlc_network.h>
#include <vlc_block.h>
#include <vlc_url.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#include <bitstream/ietf/rtcp_rr.h>
#include <bitstream/ietf/rtcp_sdes.h>
#include <bitstream/ietf/rtcp_fb.h>
#include <bitstream/ietf/rtp.h>

#include "rist.h"

/* The default latency is 1000 ms */
#define RIST_DEFAULT_LATENCY 1000
/* The default nack retry interval */
#define RIST_DEFAULT_RETRY_INTERVAL 132
/* The default packet re-ordering buffer */
#define RIST_DEFAULT_REORDER_BUFFER 70
/* The default max packet size */
#define RIST_MAX_PACKET_SIZE 1472
/* The default timeout is 5 ms */
#define RIST_DEFAULT_POLL_TIMEOUT 5
/* The max retry count for nacks */
#define RIST_MAX_RETRIES 10
/* The rate at which we process and send nack requests */
#define NACK_INTERVAL 5 /*ms*/
/* Calculate and print stats once per second */
#define STATS_INTERVAL 1000 /*ms*/

static const int nack_type[] = {
    0, 1,
};

static const char *const nack_type_names[] = {
    N_("Range"), N_("Bitmask"),
};

enum NACK_TYPE {
    NACK_FMT_RANGE = 0,
    NACK_FMT_BITMASK
};

typedef struct
{
    struct rist_flow *flow;
    char             sender_name[MAX_CNAME];
    enum NACK_TYPE   nack_type;
    uint64_t         last_data_rx;
    uint64_t         last_nack_tx;
    vlc_thread_t     thread;
    int              i_max_packet_size;
    int              i_poll_timeout;
    int              i_poll_timeout_current;
    bool             b_ismulticast;
    bool             b_sendnacks;
    bool             b_sendblindnacks;
    bool             b_disablenacks;
    bool             b_flag_discontinuity;
    bool             dead;
    vlc_queue_t      queue;
    vlc_mutex_t      lock;
    uint64_t         last_message;
    uint64_t         last_reset;
    /* stat variables */
    uint32_t         i_poll_timeout_zero_count;
    uint32_t         i_poll_timeout_nonzero_count;
    uint64_t         i_last_stat;
    float            vbr_ratio;
    uint16_t         vbr_ratio_count;
    uint32_t         i_lost_packets;
    uint32_t         i_nack_packets;
    uint32_t         i_recovered_packets;
    uint32_t         i_reordered_packets;
    uint32_t         i_total_packets;
} stream_sys_t;

static int Control(stream_t *p_access, int i_query, va_list args)
{
    switch( i_query )
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            *va_arg( args, bool * ) = false;
            break;

        case STREAM_GET_PTS_DELAY:
            *va_arg( args, vlc_tick_t * ) = VLC_TICK_FROM_MS(
                   var_InheritInteger(p_access, "network-caching") );
            break;

        default:
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static struct rist_flow *rist_init_rx(void)
{
    struct rist_flow *flow = calloc(1, sizeof(struct rist_flow));
    if (!flow)
        return NULL;

    flow->reset = 1;
    flow->buffer = calloc(RIST_QUEUE_SIZE, sizeof(struct rtp_pkt));

    if ( unlikely( flow->buffer == NULL ) )
    {
        free(flow);
        return NULL;
    }
    flow->fd_in = -1;
    flow->fd_nack = -1;
    flow->fd_rtcp_m = -1;

    return flow;
}

static void rist_WriteTo_i11e_Locked(vlc_mutex_t lock, int fd, const void *buf, size_t len,
    const struct sockaddr *peer, socklen_t slen)
{
    vlc_mutex_lock( &lock );
    rist_WriteTo_i11e(fd, buf, len, peer, slen);
    vlc_mutex_unlock( &lock );
}

static struct rist_flow *rist_udp_receiver(stream_t *p_access, vlc_url_t *parsed_url, bool b_ismulticast)
{
    stream_sys_t *p_sys = p_access->p_sys;
    msg_Info( p_access, "Opening Rist Flow Receiver at %s:%d and %s:%d",
             parsed_url->psz_host, parsed_url->i_port,
             parsed_url->psz_host, parsed_url->i_port+1);

    p_sys->flow = rist_init_rx();
    if (!p_sys->flow)
        return NULL;

    p_sys->flow->fd_in = net_OpenDgram(p_access, parsed_url->psz_host, parsed_url->i_port, NULL,
                0, IPPROTO_UDP);
    if (p_sys->flow->fd_in < 0)
    {
        msg_Err( p_access, "cannot open input socket" );
        goto fail;
    }

    if (b_ismulticast)
    {
        p_sys->flow->fd_rtcp_m = net_OpenDgram(p_access, parsed_url->psz_host, parsed_url->i_port + 1,
            NULL, 0, IPPROTO_UDP);
        if (p_sys->flow->fd_rtcp_m < 0)
        {
            msg_Err( p_access, "cannot open multicast nack socket" );
            goto fail;
        }
        p_sys->flow->fd_nack = net_ConnectDgram(p_access, parsed_url->psz_host,
            parsed_url->i_port + 1, -1, IPPROTO_UDP );
    }
    else
    {
        p_sys->flow->fd_nack = net_OpenDgram(p_access, parsed_url->psz_host, parsed_url->i_port + 1,
            NULL, 0, IPPROTO_UDP);
    }
    if (p_sys->flow->fd_nack < 0)
    {
        msg_Err( p_access, "cannot open nack socket" );
        goto fail;
    }

    populate_cname(p_sys->flow->fd_nack, p_sys->flow->cname);
    msg_Info(p_access, "our cname is %s", p_sys->flow->cname);

    return  p_sys->flow;

fail:
    if (p_sys->flow->fd_in != -1)
        vlc_close(p_sys->flow->fd_in);
    if (p_sys->flow->fd_nack != -1)
        vlc_close(p_sys->flow->fd_nack);
    if (p_sys->flow->fd_rtcp_m != -1)
        vlc_close(p_sys->flow->fd_rtcp_m);
    free(p_sys->flow->buffer);
    free(p_sys->flow);
    return NULL;
}

static int is_index_in_range(struct rist_flow *flow, uint16_t idx)
{
    if (flow->ri <= flow->wi) {
        return ((idx > flow->ri) && (idx <= flow->wi));
    } else {
        return ((idx > flow->ri) || (idx <= flow->wi));
    }
}

static void send_rtcp_feedback(stream_t *p_access, struct rist_flow *flow)
{
    stream_sys_t *p_sys = p_access->p_sys;
    int namelen = strlen(flow->cname) + 1;

    /* we need to make sure it is a multiple of 4, pad if necessary */
    if ((namelen - 2) & 0x3)
        namelen = ((((namelen - 2) >> 2) + 1) << 2) + 2;

    int rtcp_feedback_size = RTCP_EMPTY_RR_SIZE + RTCP_SDES_SIZE + namelen;
    uint8_t *buf = malloc(rtcp_feedback_size);
    if ( unlikely( buf == NULL ) )
        return;

    /* Populate RR */
    uint8_t *rr = buf;
    rtp_set_hdr(rr);
    rtcp_rr_set_pt(rr);
    rtcp_set_length(rr, 1);
    rtcp_fb_set_int_ssrc_pkt_sender(rr, 0);

    /* Populate SDES */
    uint8_t *p_sdes = (buf + RTCP_EMPTY_RR_SIZE);
    rtp_set_hdr(p_sdes);
    rtp_set_cc(p_sdes, 1); /* Actually it is source count in this case */
    rtcp_sdes_set_pt(p_sdes);
    rtcp_set_length(p_sdes, (namelen >> 2) + 2);
    rtcp_sdes_set_cname(p_sdes, 1);
    rtcp_sdes_set_name_length(p_sdes, strlen(flow->cname));
    uint8_t *p_sdes_name = (buf + RTCP_EMPTY_RR_SIZE + RTCP_SDES_SIZE);
    strlcpy((char *)p_sdes_name, flow->cname, namelen);

    /* Write to Socket */
    rist_WriteTo_i11e_Locked(p_sys->lock, flow->fd_nack, buf, rtcp_feedback_size,
        (struct sockaddr *)&flow->peer_sockaddr, flow->peer_socklen);
    free(buf);
    buf = NULL;
}

static void send_bbnack(stream_t *p_access, int fd_nack, block_t *pkt_nacks, uint16_t nack_count)
{
    stream_sys_t *p_sys = p_access->p_sys;
    struct rist_flow *flow = p_sys->flow;
    int len = 0;

    int bbnack_bufsize = RTCP_FB_HEADER_SIZE +
        RTCP_FB_FCI_GENERIC_NACK_SIZE * nack_count;
    uint8_t *buf = malloc(bbnack_bufsize);
    if ( unlikely( buf == NULL ) )
        return;

    /* Populate NACKS */
    uint8_t *nack = buf;
    rtp_set_hdr(nack);
    rtcp_fb_set_fmt(nack, NACK_FMT_BITMASK);
    rtcp_set_pt(nack, RTCP_PT_RTPFB);
    rtcp_set_length(nack, 2 + nack_count);
    /*uint8_t name[4] = "RIST";*/
    /*rtcp_fb_set_ssrc_media_src(nack, name);*/
    len += RTCP_FB_HEADER_SIZE;
    /* TODO : group together */
    uint16_t nacks[MAX_NACKS];
    memcpy(nacks, pkt_nacks->p_buffer, pkt_nacks->i_buffer);
    for (int i = 0; i < nack_count; i++) {
        uint8_t *nack_record = buf + len + RTCP_FB_FCI_GENERIC_NACK_SIZE*i;
        rtcp_fb_nack_set_packet_id(nack_record, nacks[i]);
        rtcp_fb_nack_set_bitmask_lost(nack_record, 0);
    }
    len += RTCP_FB_FCI_GENERIC_NACK_SIZE * nack_count;

    /* Write to Socket */
    if (p_sys->b_sendnacks && p_sys->b_disablenacks == false)
        rist_WriteTo_i11e_Locked(p_sys->lock, fd_nack, buf, len,
            (struct sockaddr *)&flow->peer_sockaddr, flow->peer_socklen);
    free(buf);
    buf = NULL;
}

static void send_rbnack(stream_t *p_access, int fd_nack, block_t *pkt_nacks, uint16_t nack_count)
{
    stream_sys_t *p_sys = p_access->p_sys;
    struct rist_flow *flow = p_sys->flow;
    int len = 0;

    int rbnack_bufsize = RTCP_FB_HEADER_SIZE +
        RTCP_FB_FCI_GENERIC_NACK_SIZE * nack_count;
    uint8_t *buf = malloc(rbnack_bufsize);
    if ( unlikely( buf == NULL ) )
        return;

    /* Populate NACKS */
    uint8_t *nack = buf;
    rtp_set_hdr(nack);
    rtcp_fb_set_fmt(nack, NACK_FMT_RANGE);
    rtcp_set_pt(nack, RTCP_PT_RTPFR);
    rtcp_set_length(nack, 2 + nack_count);
    uint8_t name[4] = "RIST";
    rtcp_fb_set_ssrc_media_src(nack, name);
    len += RTCP_FB_HEADER_SIZE;
    /* TODO : group together */
    uint16_t nacks[MAX_NACKS];
    memcpy(nacks, pkt_nacks->p_buffer, pkt_nacks->i_buffer);
    for (int i = 0; i < nack_count; i++)
    {
        uint8_t *nack_record = buf + len + RTCP_FB_FCI_GENERIC_NACK_SIZE*i;
        rtcp_fb_nack_set_range_start(nack_record, nacks[i]);
        rtcp_fb_nack_set_range_extra(nack_record, 0);
    }
    len += RTCP_FB_FCI_GENERIC_NACK_SIZE * nack_count;

    /* Write to Socket */
    if (p_sys->b_sendnacks && p_sys->b_disablenacks == false)
        rist_WriteTo_i11e_Locked(p_sys->lock, fd_nack, buf, len,
            (struct sockaddr *)&flow->peer_sockaddr, flow->peer_socklen);
    free(buf);
    buf = NULL;
}

static void send_nacks(stream_t *p_access, struct rist_flow *flow)
{
    stream_sys_t *p_sys = p_access->p_sys;
    struct rtp_pkt *pkt;
    uint16_t idx;
    uint64_t last_ts = 0;
    uint16_t null_count = 0;
    int nacks_len = 0;
    uint16_t nacks[MAX_NACKS];

    idx = flow->ri;
    while(idx++ != flow->wi)
    {
        pkt = &(flow->buffer[idx]);
        if (pkt->buffer == NULL)
        {
            if (nacks_len + 1 >= MAX_NACKS)
            {
                break;
            }
            else
            {
                null_count++;
                /* TODO: after adding average spacing calculation, change this formula
                   to extrapolated_ts = last_ts + null_count * avg_delta_ts; */
                uint64_t extrapolated_ts = last_ts;
                /* Find out the age and add it only if necessary */
                int retry_count = flow->nacks_retries[idx];
                uint64_t age = flow->hi_timestamp - extrapolated_ts;
                uint64_t expiration;
                if (retry_count == 0){
                    expiration = flow->reorder_buffer;
                } else {
                    expiration = (uint64_t)flow->nacks_retries[idx] * (uint64_t)flow->retry_interval;
                }
                if (age > expiration && retry_count <= flow->max_retries)
                {
                    flow->nacks_retries[idx]++;
                    nacks[nacks_len++] = idx;
                    msg_Dbg(p_access, "Sending NACK for seq %d, age %"PRId64" ms, retry %u, " \
                        "expiration %"PRId64" ms", idx, ts_get_from_rtp(age)/1000,
                        flow->nacks_retries[idx], ts_get_from_rtp(expiration)/1000);
                }
            }
        }
        else
        {
            last_ts = pkt->rtp_ts;
            null_count = 0;
        }
    }
    if (nacks_len > 0)
    {
        p_sys->i_nack_packets += nacks_len;
        block_t *pkt_nacks = block_Alloc(nacks_len * 2);
        if (pkt_nacks)
        {
            memcpy(pkt_nacks->p_buffer, nacks, nacks_len * 2);
            pkt_nacks->i_buffer = nacks_len * 2;
            vlc_queue_Enqueue(&p_sys->queue, pkt_nacks);
        }
    }
}

static int sockaddr_cmp(struct sockaddr *x, struct sockaddr *y)
{
#define CMP(a, b) if (a != b) return a < b ? -1 : 1

    CMP(x->sa_family, y->sa_family);

    if (x->sa_family == AF_INET)
    {
        struct sockaddr_in *xin = (void*)x, *yin = (void*)y;
        CMP(ntohl(xin->sin_addr.s_addr), ntohl(yin->sin_addr.s_addr));
        CMP(ntohs(xin->sin_port), ntohs(yin->sin_port));
    }
    else if (x->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *xin6 = (void*)x, *yin6 = (void*)y;
        int r = memcmp(xin6->sin6_addr.s6_addr, yin6->sin6_addr.s6_addr,
            sizeof(xin6->sin6_addr.s6_addr));
        if (r != 0)
            return r;
        CMP(ntohs(xin6->sin6_port), ntohs(yin6->sin6_port));
        CMP(xin6->sin6_flowinfo, yin6->sin6_flowinfo);
        CMP(xin6->sin6_scope_id, yin6->sin6_scope_id);
    }

#undef CMP
    return 0;
}

static void print_sockaddr_info_change(stream_t *p_access, struct sockaddr *x, struct sockaddr *y)
{
    if (x->sa_family == AF_INET)
    {
        struct sockaddr_in *xin = (void*)x, *yin = (void*)y;
        msg_Info(p_access, "Peer IP:Port change detected: old IP:Port %s:%d, new IP:Port %s:%d",
            inet_ntoa(xin->sin_addr), ntohs(xin->sin_port), inet_ntoa(yin->sin_addr),
            ntohs(yin->sin_port));
    }
    else if (x->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *xin6 = (void*)x, *yin6 = (void*)y;
        char oldstr[INET6_ADDRSTRLEN];
        char newstr[INET6_ADDRSTRLEN];
        inet_ntop(xin6->sin6_family, &xin6->sin6_addr, oldstr, sizeof(struct in6_addr));
        inet_ntop(yin6->sin6_family, &yin6->sin6_addr, newstr, sizeof(struct in6_addr));
        msg_Info(p_access, "Peer IP:Port change detected: old IP:Port %s:%d, new IP:Port %s:%d",
            oldstr, ntohs(xin6->sin6_port), newstr, ntohs(yin6->sin6_port));
    }
}

static void print_sockaddr_info(stream_t *p_access, struct sockaddr *x)
{
    if (x->sa_family == AF_INET)
    {
        struct sockaddr_in *xin = (void*)x;
        msg_Info(p_access, "Peer IP:Port %s:%d", inet_ntoa(xin->sin_addr), ntohs(xin->sin_port));
    }
    else if (x->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *xin6 = (void*)x;
        char str[INET6_ADDRSTRLEN];
        inet_ntop(xin6->sin6_family, &xin6->sin6_addr, str, sizeof(struct in6_addr));
        msg_Info(p_access, "Peer IP:Port %s:%d", str, ntohs(xin6->sin6_port));
    }
}

static void rtcp_input(stream_t *p_access, struct rist_flow *flow, uint8_t *buf_in, size_t len,
    struct sockaddr *peer, socklen_t slen)
{
    stream_sys_t *p_sys = p_access->p_sys;
    uint8_t  ptype;
    uint16_t processed_bytes = 0;
    uint16_t records;
    char new_sender_name[MAX_CNAME];
    uint8_t *buf;

    while (processed_bytes < len) {
        buf = buf_in + processed_bytes;
        /* safety checks */
        uint16_t bytes_left = len - processed_bytes + 1;
        if ( bytes_left < 4 )
        {
            /* we must have at least 4 bytes */
            msg_Err(p_access, "Rist rtcp packet must have at least 4 bytes, we have %d",
                bytes_left);
            return;
        }
        else if (!rtp_check_hdr(buf))
        {
            /* check for a valid rtp header */
            msg_Err(p_access, "Malformed rtcp packet starting with %02x, ignoring.", buf[0]);
            return;
        }

        ptype =  rtcp_get_pt(buf);
        records = rtcp_get_length(buf);
        uint16_t bytes = (uint16_t)(4 * (1 + records));
        if (bytes > bytes_left)
        {
            /* check for a sane number of bytes */
            msg_Err(p_access, "Malformed rtcp packet, wrong len %d, expecting %u bytes in the " \
                "packet, got a buffer of %u bytes.", rtcp_get_length(buf), bytes, bytes_left);
            return;
        }

        switch(ptype) {
            case RTCP_PT_RTPFR:
            case RTCP_PT_RTPFB:
                break;

            case RTCP_PT_RR:
                break;

            case RTCP_PT_SDES:
                {
                    if (p_sys->b_sendnacks == false)
                        p_sys->b_sendnacks = true;
                    if (p_sys->b_ismulticast)
                        return;
                    /* Check for changes in source IP address or port */
                    int8_t name_length = rtcp_sdes_get_name_length(buf);
                    if (name_length > bytes_left || name_length <= 0 ||
                        (size_t)name_length > sizeof(new_sender_name))
                    {
                        /* check for a sane number of bytes */
                        msg_Err(p_access, "Malformed SDES packet, wrong cname len %d, got a " \
                            "buffer of %u bytes.", name_length, bytes_left);
                        return;
                    }
                    bool ip_port_changed = false;
                    if (sockaddr_cmp((struct sockaddr *)&flow->peer_sockaddr, peer) != 0)
                    {
                        ip_port_changed = true;
                        if(flow->peer_socklen > 0)
                            print_sockaddr_info_change(p_access,
                                (struct sockaddr *)&flow->peer_sockaddr, peer);
                        else
                            print_sockaddr_info(p_access, peer);
                        vlc_mutex_lock( &p_sys->lock );
                        memcpy(&flow->peer_sockaddr, peer, sizeof(struct sockaddr_storage));
                        flow->peer_socklen = slen;
                        vlc_mutex_unlock( &p_sys->lock );
                    }

                    /* Check for changes in cname */
                    bool peer_name_changed = false;
                    memset(new_sender_name, 0, MAX_CNAME);
                    memcpy(new_sender_name, buf + RTCP_SDES_SIZE, name_length);
                    if (memcmp(new_sender_name, p_sys->sender_name, name_length) != 0)
                    {
                        peer_name_changed = true;
                        if (strcmp(p_sys->sender_name, "") == 0)
                            msg_Info(p_access, "Peer Name: %s", new_sender_name);
                        else
                            msg_Info(p_access, "Peer Name change detected: old Name: %s, new " \
                                "Name: %s", p_sys->sender_name, new_sender_name);
                        memset(p_sys->sender_name, 0, MAX_CNAME);
                        memcpy(p_sys->sender_name, buf + RTCP_SDES_SIZE, name_length);
                    }

                    /* Reset the buffer as the source must have been restarted */
                    if (peer_name_changed || ip_port_changed)
                    {
                        /* reset the buffer */
                        flow->reset = 1;
                    }
                }
                break;

            case RTCP_PT_SR:
                if (p_sys->b_sendnacks == false)
                    p_sys->b_sendnacks = true;
                if (p_sys->b_ismulticast)
                        return;
                break;

            default:
                msg_Err(p_access, "   Unrecognized RTCP packet with PTYPE=%02x!!", ptype);
        }
        processed_bytes += (4 * (1 + records));
    }
}

static bool rist_input(stream_t *p_access, struct rist_flow *flow, uint8_t *buf, size_t len)
{
    stream_sys_t *p_sys = p_access->p_sys;

    /* safety checks */
    if ( len < RTP_HEADER_SIZE )
    {
        /* check if packet size >= rtp header size */
        msg_Err(p_access, "Rist rtp packet must have at least 12 bytes, we have %zu", len);
        return false;
    }
    else if (!rtp_check_hdr(buf))
    {
        /* check for a valid rtp header */
        msg_Err(p_access, "Malformed rtp packet header starting with %02x, ignoring.", buf[0]);
        return false;
    }

    uint16_t idx = rtp_get_seqnum(buf);
    uint32_t pkt_ts = rtp_get_timestamp(buf);
    bool retrasnmitted = false;
    bool success = true;

    if (flow->reset == 1)
    {
        msg_Info(p_access, "Traffic detected after buffer reset");
        /* First packet in the queue */
        flow->hi_timestamp = pkt_ts;
        msg_Info(p_access, "ts@%u", flow->hi_timestamp);
        flow->wi = idx;
        flow->ri = idx;
        flow->reset = 0;
        p_sys->b_flag_discontinuity = true;
    }

    /* Check to see if this is a retransmission or a regular packet */
    if (buf[11] & (1 << 0))
    {
        msg_Dbg(p_access, "Packet %d RECOVERED, Window: [%d:%d-->%d]", idx, flow->ri, flow->wi,
            flow->wi-flow->ri);
        p_sys->i_recovered_packets++;
        retrasnmitted = true;
    }
    else if (flow->wi != flow->ri)
    {
        /* Reset counter to 0 on incoming holes */
        /* Regular packets only as retransmits are expected to come in out of order */
        uint16_t idxnext = (uint16_t)(flow->wi + 1);
        if (idx != idxnext)
        {
            if (idx > idxnext) {
                msg_Dbg(p_access, "Gap, got %d, expected %d, %d packet gap, Window: [%d:%d-->%d]",
                    idx, idxnext, idx - idxnext, flow->ri, flow->wi, (uint16_t)(flow->wi-flow->ri));
            } else {
                p_sys->i_reordered_packets++;
                msg_Dbg(p_access, "Out of order, got %d, expected %d, Window: [%d:%d-->%d]", idx,
                    idxnext, flow->ri, flow->wi, (uint16_t)(flow->wi-flow->ri));
            }
            uint16_t zero_counter = (uint16_t)(flow->wi + 1);
            while(zero_counter++ != idx) {
                flow->nacks_retries[zero_counter] = 0;
            }
            /*msg_Dbg(p_access, "Gap, reseting %d packets as zero nacks %d to %d",
                idx - flow->wi - 1, (uint16_t)(flow->wi + 1), idx);*/
        }
    }

    /* Always replace the existing one with the new one */
    struct rtp_pkt *pkt;
    pkt = &(flow->buffer[idx]);
    if (pkt->buffer && pkt->buffer->i_buffer > 0)
    {
        block_Release(pkt->buffer);
        pkt->buffer = NULL;
    }
    pkt->buffer = block_Alloc(len);
    if (!pkt->buffer)
        return false;

    pkt->buffer->i_buffer = len;
    memcpy(pkt->buffer->p_buffer, buf, len);
    pkt->rtp_ts = pkt_ts;
    p_sys->last_data_rx = vlc_tick_now();
    /* Reset the try counter regardless of wether it was a retransmit or not */
    flow->nacks_retries[idx] = 0;

    if (retrasnmitted)
        return success;

    p_sys->i_total_packets++;
    /* Perform discontinuity checks and udpdate counters */
    if (!is_index_in_range(flow, idx) && pkt_ts >= flow->hi_timestamp)
    {
        if ((pkt_ts - flow->hi_timestamp) > flow->hi_timestamp/10)
        {
            msg_Info(p_access, "Forward stream discontinuity idx@%d/%d/%d ts@%u/%u", flow->ri, idx,
                flow->wi, pkt_ts, flow->hi_timestamp);
            flow->reset = 1;
            success = false;
        }
        else
        {
            flow->wi = idx;
            flow->hi_timestamp = pkt_ts;
        }
    }
    else if (!is_index_in_range(flow, idx))
    {
        /* incoming timestamp just jumped back in time or index is outside of scope */
        msg_Info(p_access, "Backwards stream discontinuity idx@%d/%d/%d ts@%u/%u", flow->ri, idx,
            flow->wi, pkt_ts, flow->hi_timestamp);
        flow->reset = 1;
        success = false;
    }

    return success;
}

static block_t *rist_dequeue(stream_t *p_access, struct rist_flow *flow)
{
    stream_sys_t *p_sys = p_access->p_sys;
    block_t *pktout = NULL;
    struct rtp_pkt *pkt;
    uint16_t idx;
    if (flow->ri == flow->wi || flow->reset > 0)
        return NULL;

    idx = flow->ri;
    bool found_data = false;
    uint16_t loss_amount = 0;
    while(idx++ != flow->wi) {

        pkt = &(flow->buffer[idx]);
        if (!pkt->buffer)
        {
            /*msg_Info(p_access, "Possible packet loss on index #%d", idx);*/
            loss_amount++;
            /* We move ahead until we find a timestamp but we do not move the cursor.
             * None of them are guaranteed packet loss because we do not really
             * know their timestamps. They might still arrive on the next loop.
             * We can confirm the loss only if we get a valid packet in the loop below. */
            continue;
        }

        /*printf("IDX=%d, flow->hi_timestamp: %u, (ts + flow->rtp_latency): %u\n", idx,
            flow->hi_timestamp, (ts - 100 * flow->qdelay));*/
        if (flow->hi_timestamp > (uint32_t)(pkt->rtp_ts + flow->rtp_latency))
        {
            /* Populate output packet now but remove rtp header from source */
            int newSize = pkt->buffer->i_buffer - RTP_HEADER_SIZE;
            pktout = block_Alloc(newSize);
            if (pktout)
            {
                pktout->i_buffer = newSize;
                memcpy(pktout->p_buffer, pkt->buffer->p_buffer + RTP_HEADER_SIZE, newSize);
                /* free the buffer and increase the read index */
                flow->ri = idx;
                /* TODO: calculate average duration using buffer average (bring from sender) */
                found_data = true;
            }
            block_Release(pkt->buffer);
            pkt->buffer = NULL;
            break;
        }

    }

    if (loss_amount > 0 && found_data == true)
    {
        /* Packet loss confirmed, we found valid data after the holes */
        msg_Dbg(p_access, "Packet NOT RECOVERED, %d packet(s), Window: [%d:%d]", loss_amount,
            flow->ri, flow->wi);
        p_sys->i_lost_packets += loss_amount;
        p_sys->b_flag_discontinuity = true;
    }

    return pktout;
}

static void *rist_thread(void *data)
{
    stream_t *p_access = data;
    stream_sys_t *p_sys = p_access->p_sys;
    block_t *pkt_nacks;

    /* Process nacks every 5ms */
    /* We only ask for the relevant ones */
    while ((pkt_nacks = vlc_queue_DequeueKillable(&p_sys->queue,
                                                  &p_sys->dead)) != NULL) {
        /* there are two bytes per nack */
        uint16_t nack_count = (uint16_t)pkt_nacks->i_buffer/2;
        switch(p_sys->nack_type) {
            case NACK_FMT_BITMASK:
                send_bbnack(p_access, p_sys->flow->fd_nack, pkt_nacks, nack_count);
                break;

            default:
                send_rbnack(p_access, p_sys->flow->fd_nack, pkt_nacks, nack_count);
        }

        if (nack_count > 1)
            msg_Dbg(p_access, "Sent %u NACKs !!!", nack_count);
        block_Release(pkt_nacks);
    }

    return NULL;
}

static block_t *BlockRIST(stream_t *p_access, bool *restrict eof)
{
    stream_sys_t *p_sys = p_access->p_sys;
    uint64_t now;
    *eof = false;
    block_t *pktout = NULL;
    struct pollfd pfd[3];
    int ret;
    ssize_t r;
    struct sockaddr_storage peer;
    socklen_t slen = sizeof(struct sockaddr_storage);
    struct rist_flow *flow = p_sys->flow;

    if (vlc_killed())
    {
        *eof = true;
        return NULL;
    }

    int poll_sockets = 2;
    pfd[0].fd = flow->fd_in;
    pfd[0].events = POLLIN;
    pfd[1].fd = flow->fd_nack;
    pfd[1].events = POLLIN;
    if (p_sys->b_ismulticast)
    {
        pfd[2].fd = flow->fd_rtcp_m;
        pfd[2].events = POLLIN;
        poll_sockets++;
    }

    /* The protocol uses a fifo buffer with a fixed time delay.
     * That buffer needs to be emptied at a rate that is determined by the rtp timestamps of the
     * packets. If I waited indefinitely for data coming in, the rate and delay of output packets
     * would be wrong. I am calling the rist_dequeue function every time a data packet comes in
     * and also every time we get a poll timeout. The configurable poll timeout is for controling
     * the maximum jitter of output data coming out of the buffer. The default 5ms timeout covers
     * most cases. */

    ret = vlc_poll_i11e(pfd, poll_sockets, p_sys->i_poll_timeout_current);
    if (unlikely(ret < 0))
        return NULL;
    else if (ret == 0)
    {
        /* Poll timeout, check the queue for the next packet that needs to be delivered */
        pktout = rist_dequeue(p_access, flow);
        /* if there is data, we need to come back faster to finish emptying it */
        if (pktout) {
            p_sys->i_poll_timeout_current = 0;
            p_sys->i_poll_timeout_zero_count++;
        } else {
            p_sys->i_poll_timeout_current = p_sys->i_poll_timeout;
            p_sys->i_poll_timeout_nonzero_count++;
        }
    }
    else
    {

        uint8_t *buf = malloc(p_sys->i_max_packet_size);
        if ( unlikely( buf == NULL ) )
            return NULL;

        /* Process rctp incoming data */
        if (pfd[1].revents & POLLIN)
        {
            r = rist_ReadFrom_i11e(flow->fd_nack, buf, p_sys->i_max_packet_size,
                (struct sockaddr *)&peer, &slen);
            if (unlikely(r == -1)) {
                msg_Err(p_access, "socket %d error: %s\n", flow->fd_nack, gai_strerror(errno));
            }
            else {
                if (p_sys->b_ismulticast == false)
                    rtcp_input(p_access, flow, buf, r, (struct sockaddr *)&peer, slen);
            }
        }
        if (p_sys->b_ismulticast && pfd[2].revents & POLLIN)
        {
            r = rist_ReadFrom_i11e(flow->fd_rtcp_m, buf, p_sys->i_max_packet_size,
                (struct sockaddr *)&peer, &slen);
            if (unlikely(r == -1)) {
                msg_Err(p_access, "mcast socket %d error: %s\n",flow->fd_rtcp_m, gai_strerror(errno));
            }
            else {
                rtcp_input(p_access, flow, buf, r, (struct sockaddr *)&peer, slen);
            }
        }

        /* Process regular incoming data */
        if (pfd[0].revents & POLLIN)
        {
            r = rist_Read_i11e(flow->fd_in, buf, p_sys->i_max_packet_size);
            if (unlikely(r == -1)) {
                msg_Err(p_access, "socket %d error: %s\n", flow->fd_in, gai_strerror(errno));
            }
            else
            {
                /* rist_input will process and queue the pkt */
                if (rist_input(p_access, flow, buf, r))
                {
                    /* Check the queue for the next packet that needs to be delivered */
                    pktout = rist_dequeue(p_access, flow);
                    if (pktout) {
                        p_sys->i_poll_timeout_current = 0;
                        p_sys->i_poll_timeout_zero_count++;
                    } else {
                        p_sys->i_poll_timeout_current = p_sys->i_poll_timeout;
                        p_sys->i_poll_timeout_nonzero_count++;
                    }
                }
            }
        }

        free(buf);
        buf = NULL;
    }

    now = vlc_tick_now();

    /* Process stats and print them out */
    /* We need to measure some items every 70ms */
    uint64_t interval = (now - flow->feedback_time);
    if ( interval > VLC_TICK_FROM_MS(RTCP_INTERVAL) )
    {
        if (p_sys->i_poll_timeout_nonzero_count > 0)
        {
            float ratio = (float)p_sys->i_poll_timeout_zero_count
                / (float)p_sys->i_poll_timeout_nonzero_count;
            if (ratio <= 1)
                p_sys->vbr_ratio += 1 - ratio;
            else
                p_sys->vbr_ratio += ratio - 1;
            p_sys->vbr_ratio_count++;
            /*msg_Dbg(p_access, "zero poll %u, non-zero poll %u, ratio %.2f",
                p_sys->i_poll_timeout_zero_count, p_sys->i_poll_timeout_nonzero_count, ratio);*/
            p_sys->i_poll_timeout_zero_count = 0;
            p_sys->i_poll_timeout_nonzero_count =  0;
        }
    }
    /* We print out the stats once per second */
    interval = (now - p_sys->i_last_stat);
    if ( interval > VLC_TICK_FROM_MS(STATS_INTERVAL) )
    {
        if ( p_sys->i_lost_packets > 0)
            msg_Err(p_access, "We have %d lost packets", p_sys->i_lost_packets);
        float ratio = 1;
        if (p_sys->vbr_ratio_count > 0)
            ratio = p_sys->vbr_ratio / (float)p_sys->vbr_ratio_count;
        float quality = 100;
        if (p_sys->i_total_packets > 0)
            quality -= (float)100*(float)(p_sys->i_lost_packets + p_sys->i_recovered_packets +
                p_sys->i_reordered_packets)/(float)p_sys->i_total_packets;
        if (quality != 100)
            msg_Info(p_access, "STATS: Total %u, Recovered %u/%u, Reordered %u, Lost %u, VBR " \
                "Score %.2f, Link Quality %.2f%%", p_sys->i_total_packets,
                p_sys->i_recovered_packets, p_sys->i_nack_packets, p_sys->i_reordered_packets,
                p_sys->i_lost_packets, ratio, quality);
        p_sys->i_last_stat = now;
        p_sys->vbr_ratio = 0;
        p_sys->vbr_ratio_count = 0;
        p_sys->i_lost_packets = 0;
        p_sys->i_nack_packets = 0;
        p_sys->i_recovered_packets = 0;
        p_sys->i_reordered_packets = 0;
        p_sys->i_total_packets = 0;
    }

    /* Send rtcp feedback every RTCP_INTERVAL */
    interval = (now - flow->feedback_time);
    if ( interval > VLC_TICK_FROM_MS(RTCP_INTERVAL) )
    {
        /* msg_Dbg(p_access, "Calling RTCP Feedback %lu<%d ms using timer", interval,
        VLC_TICK_FROM_MS(RTCP_INTERVAL)); */
        send_rtcp_feedback(p_access, flow);
        flow->feedback_time = now;
    }

    /* Send nacks every NACK_INTERVAL (only the ones that have matured, if any) */
    interval = (now - p_sys->last_nack_tx);
    if ( interval > VLC_TICK_FROM_MS(NACK_INTERVAL) )
    {
        send_nacks(p_access, p_sys->flow);
        p_sys->last_nack_tx = now;
    }

    /* Safety check for when the input stream stalls */
    if ( p_sys->last_data_rx > 0 && now > p_sys->last_data_rx &&
        (uint64_t)(now - p_sys->last_data_rx) >  (uint64_t)VLC_TICK_FROM_MS(flow->latency) &&
        (uint64_t)(now - p_sys->last_reset) > (uint64_t)VLC_TICK_FROM_MS(flow->latency) )
    {
        msg_Err(p_access, "No data received for %"PRId64" ms, resetting buffers",
            (int64_t)(now - p_sys->last_data_rx)/1000);
        p_sys->last_reset = now;
        flow->reset = 1;
    }

    if (pktout)
    {
        if (p_sys->b_flag_discontinuity) {
            pktout->i_flags |= BLOCK_FLAG_DISCONTINUITY;
            p_sys->b_flag_discontinuity = false;
        }
        return pktout;
    }
    else
        return NULL;
}

static void Clean( stream_t *p_access )
{
    stream_sys_t *p_sys = p_access->p_sys;

    if (p_sys->flow)
    {
        if (p_sys->flow->fd_in >= 0)
            net_Close (p_sys->flow->fd_in);
        if (p_sys->flow->fd_nack >= 0)
            net_Close (p_sys->flow->fd_nack);
        if (p_sys->flow->fd_rtcp_m >= 0)
            net_Close (p_sys->flow->fd_rtcp_m);
        for (int i=0; i<RIST_QUEUE_SIZE; i++) {
            struct rtp_pkt *pkt = &(p_sys->flow->buffer[i]);
            if (pkt->buffer && pkt->buffer->i_buffer > 0) {
                block_Release(pkt->buffer);
                pkt->buffer = NULL;
            }
        }
        free(p_sys->flow->buffer);
        free(p_sys->flow);
    }
}

static void Close(vlc_object_t *p_this)
{
    stream_t     *p_access = (stream_t*)p_this;
    stream_sys_t *p_sys = p_access->p_sys;

    vlc_queue_Kill(&p_sys->queue, &p_sys->dead);
    vlc_join(p_sys->thread, NULL);

    Clean( p_access );
}

static int Open(vlc_object_t *p_this)
{
    stream_t     *p_access = (stream_t*)p_this;
    stream_sys_t *p_sys = NULL;
    vlc_url_t     parsed_url = { 0 };

    p_sys = vlc_obj_calloc( p_this, 1, sizeof( *p_sys ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    p_access->p_sys = p_sys;

    vlc_mutex_init( &p_sys->lock );

    if ( vlc_UrlParse( &parsed_url, p_access->psz_url ) == -1 )
    {
        msg_Err( p_access, "Failed to parse input URL (%s)",
            p_access->psz_url );
        goto failed;
    }

    /* Initialize rist flow */
    p_sys->b_ismulticast = is_multicast_address(parsed_url.psz_host);
    p_sys->flow = rist_udp_receiver(p_access, &parsed_url, p_sys->b_ismulticast);
    vlc_UrlClean( &parsed_url );
    if (!p_sys->flow)
    {
        msg_Err( p_access, "Failed to open rist flow (%s)",
            p_access->psz_url );
        goto failed;
    }

    p_sys->b_flag_discontinuity = false;
    p_sys->b_disablenacks = var_InheritBool( p_access, "disable-nacks" );
    p_sys->b_sendblindnacks = var_InheritBool( p_access, "mcast-blind-nacks" );
    if (p_sys->b_sendblindnacks && p_sys->b_disablenacks == false)
        p_sys->b_sendnacks = true;
    else
        p_sys->b_sendnacks = false;
    p_sys->nack_type = var_InheritInteger( p_access, "nack-type" );
    p_sys->i_max_packet_size = var_InheritInteger( p_access, "packet-size" );
    p_sys->i_poll_timeout = var_InheritInteger( p_access, "maximum-jitter" );
    p_sys->flow->retry_interval = var_InheritInteger( p_access, "retry-interval" );
    p_sys->flow->max_retries = var_InheritInteger( p_access, "max-retries" );
    p_sys->flow->latency = var_InheritInteger( p_access, "latency" );
    if (p_sys->b_disablenacks)
        p_sys->flow->reorder_buffer = p_sys->flow->latency;
    else
        p_sys->flow->reorder_buffer = var_InheritInteger( p_access, "reorder-buffer" );
    msg_Info(p_access, "Setting queue latency to %d ms", p_sys->flow->latency);

    /* Convert to rtp times */
    p_sys->flow->rtp_latency = rtp_get_ts(VLC_TICK_FROM_MS(p_sys->flow->latency));
    p_sys->flow->retry_interval = rtp_get_ts(VLC_TICK_FROM_MS(p_sys->flow->retry_interval));
    p_sys->flow->reorder_buffer = rtp_get_ts(VLC_TICK_FROM_MS(p_sys->flow->reorder_buffer));

    p_sys->dead = false;
    vlc_queue_Init(&p_sys->queue, offsetof (block_t, p_next));

    /* This extra thread is for sending feedback/nack packets even when no data comes in */
    if (vlc_clone(&p_sys->thread, rist_thread, p_access, VLC_THREAD_PRIORITY_INPUT))
    {
        msg_Err(p_access, "Failed to create worker thread.");
        goto failed;
    }

    p_access->pf_block = BlockRIST;
    p_access->pf_control = Control;

    return VLC_SUCCESS;

failed:
    Clean( p_access );
    return VLC_EGENERIC;
}

/* Module descriptor */
vlc_module_begin ()

    set_shortname( N_("RIST") )
    set_description( N_("RIST input") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )

    add_integer( "packet-size", RIST_MAX_PACKET_SIZE,
        N_("RIST maximum packet size (bytes)"), NULL, true )
    add_integer( "maximum-jitter", RIST_DEFAULT_POLL_TIMEOUT,
        N_("RIST demux/decode maximum jitter (default is 5ms)"),
        N_("This controls the maximum jitter that will be passed to the demux/decode chain. "
            "The lower the value, the more CPU cycles the algorithm will consume"), true )
    add_integer( "latency", RIST_DEFAULT_LATENCY, N_("RIST latency (ms)"), NULL, true )
    add_integer( "retry-interval", RIST_DEFAULT_RETRY_INTERVAL, N_("RIST nack retry interval (ms)"),
        NULL, true )
    add_integer( "reorder-buffer", RIST_DEFAULT_REORDER_BUFFER, N_("RIST reorder buffer (ms)"),
        NULL, true )
    add_integer( "max-retries", RIST_MAX_RETRIES, N_("RIST maximum retry count"), NULL, true )
    add_integer( "nack-type", NACK_FMT_RANGE,
            N_("RIST nack type, 0 = range, 1 = bitmask. Default is range"), NULL, true )
        change_integer_list( nack_type, nack_type_names )
    add_bool( "disable-nacks", false, "Disable NACK output packets",
        "Use this to disable packet recovery", true )
    add_bool( "mcast-blind-nacks", false, "Do not check for a valid rtcp message from the encoder",
        "Send nack messages even when we have not confirmed that the encoder is on our local " \
        "network.", true )

    set_capability( "access", 0 )
    add_shortcut( "rist", "tr06" )

    set_callbacks( Open, Close )

vlc_module_end ()
