/*****************************************************************************
 *  * rist.c: RIST (Reliable Internet Stream Transport) output module
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
#include <vlc_fs.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_network.h>
#include <vlc_queue.h>
#include <vlc_threads.h>
#include <vlc_rand.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#include <sys/time.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <bitstream/ietf/rtcp_rr.h>
#include <bitstream/ietf/rtcp_sr.h>
#include <bitstream/ietf/rtcp_fb.h>
#include <bitstream/ietf/rtcp_sdes.h>
#include <bitstream/ietf/rtp.h>

#include "../access/rist.h"

/* Uncomment the following to introduce induced packet loss for TESTING purposes only */
/*#define TEST_PACKET_LOSS*/

/* The default target packet size */
#define RIST_TARGET_PACKET_SIZE 1328
/* The default caching delay for output data */
#define DEFAULT_CACHING_DELAY 50
/* The default buffer size in ms */
#define DEFAULT_BUFFER_SIZE 0
/* Calculate and print stats once per second */
#define STATS_INTERVAL 1000 /*ms*/

#define MPEG_II_TRANSPORT_STREAM (0x21)
#define RIST_DEFAULT_PORT 1968

#define SOUT_CFG_PREFIX "sout-rist-"

static const char *const ppsz_sout_options[] = {
    "packet-size",
    "caching",
    "buffer-size",
    "ssrc",
    "stream-name",
    NULL
};

typedef struct
{
    struct       rist_flow *flow;
    uint16_t     rtp_counter;
    char         receiver_name[MAX_CNAME];
    uint64_t     last_rtcp_tx;
    vlc_thread_t ristthread;
    vlc_thread_t senderthread;
    size_t       i_packet_size;
    bool         b_mtu_warning;
    bool         b_ismulticast;
    vlc_mutex_t  lock;
    vlc_mutex_t  fd_lock;
    block_t      *p_pktbuffer;
    uint64_t     i_ticks_caching;
    uint32_t     ssrc;
    bool         dead;
    vlc_queue_t  queue;
    /* stats variables */
    uint64_t     i_last_stat;
    uint32_t     i_retransmit_packets;
    uint32_t     i_total_packets;
} sout_access_out_sys_t;

static struct rist_flow *rist_init_tx()
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
    flow->fd_out = -1;
    flow->fd_rtcp = -1;
    flow->fd_rtcp_m = -1;

    return flow;
}

static struct rist_flow *rist_udp_transmitter(sout_access_out_t *p_access, char *psz_dst_server, 
    int i_dst_port, bool b_ismulticast)
{
    struct rist_flow *flow;
    flow = rist_init_tx();
    if (!flow)
        return NULL;

    flow->fd_out = net_ConnectDgram(p_access, psz_dst_server, i_dst_port, -1, IPPROTO_UDP );
    if (flow->fd_out < 0)
    {
        msg_Err( p_access, "cannot open output socket" );
        goto fail;
    }

    if (b_ismulticast) {
        flow->fd_rtcp_m = net_OpenDgram(p_access, psz_dst_server, i_dst_port + 1,
            NULL, 0, IPPROTO_UDP);
        if (flow->fd_rtcp_m < 0)
        {
            msg_Err( p_access, "cannot open multicast nack socket" );
            goto fail;
        }
    }

    flow->fd_rtcp = net_ConnectDgram(p_access, psz_dst_server, i_dst_port + 1, -1, IPPROTO_UDP );
    if (flow->fd_rtcp < 0)
    {
        msg_Err( p_access, "cannot open nack socket" );
        goto fail;
    }

    char *psz_streamname = NULL;
    psz_streamname = var_InheritString( p_access, SOUT_CFG_PREFIX "stream-name" );
    if ( psz_streamname != NULL && psz_streamname[0] != '\0')
    {
        int name_length = snprintf(flow->cname, MAX_CNAME, "%s", psz_streamname);
        if (name_length >= MAX_CNAME)
            flow->cname[MAX_CNAME-1] = 0;
        free( psz_streamname );
    }
    else
        populate_cname(flow->fd_rtcp, flow->cname);

    msg_Info(p_access, "our cname is %s", flow->cname);

    return flow;

fail:
    if (flow->fd_out != -1)
        vlc_close(flow->fd_out);
    if (flow->fd_rtcp != -1)
        vlc_close(flow->fd_rtcp);
    if (flow->fd_rtcp_m != -1)
        vlc_close(flow->fd_rtcp_m);
    free(flow->buffer);
    free(flow);
    return NULL;
}

static void rist_retransmit(sout_access_out_t *p_access, struct rist_flow *flow, uint16_t seq)
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    struct rtp_pkt *pkt = &(flow->buffer[seq]);
    if (pkt->buffer == NULL)
    {
        msg_Err(p_access, "RIST recovery: missing requested packet %d, buffer not yet full", seq);
        return;
    }

    /* Mark SSID for retransmission (change the last bit of the ssrc to 1) */
    pkt->buffer->p_buffer[11] |= (1 << 0);
#ifdef TEST_PACKET_LOSS
#   warning COMPILED WITH SELF INFLICTED PACKET LOSS
        if ((flow->packets_count % 14) == 0) {
            return;
        }
#endif
    uint32_t rtp_age = flow->hi_timestamp - pkt->rtp_ts;
    uint64_t age = ts_get_from_rtp(rtp_age)/1000;
    if (flow->rtp_latency > 0 && rtp_age > flow->rtp_latency)
    {
        msg_Err(p_access, "   Not Sending Nack #%d, too old (age %"PRId64" ms), current seq is:" \
            " [%d]. Perhaps you should increase the buffer-size ...", seq, age, flow->wi);
    }
    else
    {
        msg_Dbg(p_access, "   Sending Nack #%d (age %"PRId64" ms), current seq is: [%d]", 
            seq, age, flow->wi);
        p_sys->i_retransmit_packets++;
        vlc_mutex_lock( &p_sys->fd_lock );
        if (rist_Write(flow->fd_out, pkt->buffer->p_buffer, pkt->buffer->i_buffer) 
                != (ssize_t)pkt->buffer->i_buffer) {
            msg_Err(p_access, "Error sending retransmitted packet after 2 tries ...");
        }

        vlc_mutex_unlock( &p_sys->fd_lock );
    }
}

static void process_nack(sout_access_out_t *p_access, uint8_t  ptype, uint16_t nrecords, 
    struct rist_flow *flow, uint8_t *pkt)
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    int i,j;

    /*msg_Info(p_access, "   Nack (BbRR), %d record(s), Window: [%d:%d-->%d]", nrecords, 
        flow->ri, flow->wi, flow->wi-flow->ri);*/

    if (ptype == RTCP_PT_RTPFR)
    {
        uint8_t pi_ssrc[4];
        rtcp_fb_get_ssrc_media_src(pkt, pi_ssrc);
        if (memcmp(pi_ssrc, "RIST", 4) != 0)
        {
            msg_Info(p_access, "   Ignoring Nack with name %s", pi_ssrc);
            return; /* Ignore app-type not RIST */
        }

        for (i = 0; i < (nrecords-2); i++) {
            uint16_t missing;
            uint16_t additional;
            uint8_t *rtp_nack_record = (pkt + 12 + i * 4);
            missing = rtcp_fb_nack_get_range_start(rtp_nack_record);
            additional = rtcp_fb_nack_get_range_extra(rtp_nack_record);
            /*msg_Info(p_access, "   Nack (Range), %d, current seq is: [%d]", missing, flow->wi);*/
            vlc_mutex_lock( &p_sys->lock );
            rist_retransmit(p_access, flow, missing);
            for (j = 0; j < additional; j++) {
                rist_retransmit(p_access, flow, missing + j + 1);
            }
            vlc_mutex_unlock( &p_sys->lock );
        }
    }
    else if (ptype == RTCP_PT_RTPFB)
    {
        for (i = 0; i < (nrecords-2); i++) {
            uint16_t missing;
            uint16_t bitmask;
            uint8_t *rtp_nack_record = (pkt + 12 + i * 4);
            missing = rtcp_fb_nack_get_packet_id(rtp_nack_record);
            bitmask = rtcp_fb_nack_get_bitmask_lost(rtp_nack_record);
            /*msg_Info(p_access, "  Nack (Bitmask), %d, current seq is: [%d]", missing, flow->wi);*/
            vlc_mutex_lock( &p_sys->lock );
            rist_retransmit(p_access, flow, missing);
            for (j = 0; j < 16; j++) {
                if ((bitmask & (1 << j)) == (1 << j)) {
                    rist_retransmit(p_access, flow, missing + j + 1);
                }
            }
            vlc_mutex_unlock( &p_sys->lock );
        }
    }
    else
    {
        msg_Err(p_access, "   !!! Wrong feedback. Ptype is %02x!=%02x, FMT: %02x", ptype, 
            RTCP_PT_RTPFR, rtcp_fb_get_fmt(pkt));
    }
}

static void rist_rtcp_recv(sout_access_out_t *p_access, struct rist_flow *flow, uint8_t *pkt_raw, 
    size_t len)
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    uint8_t *pkt = pkt_raw;
    uint8_t  ptype;
    uint16_t processed_bytes = 0;
    uint16_t records;

    while (processed_bytes < len) {
        pkt = pkt_raw + processed_bytes;
        /* safety checks */
        uint16_t bytes_left = len - processed_bytes + 1;
        if ( bytes_left < 4 )
        {
            /* we must have at least 4 bytes */
            msg_Err(p_access, "Rist rtcp packet must have at least 4 bytes, we have %d", 
                bytes_left);
            return; 
        }
        else if (!rtp_check_hdr(pkt))
        {
            /* check for a valid rtp header */
            msg_Err(p_access, "Malformed feedback packet starting with %02x, ignoring.", pkt[0]);
            return;
        }

        ptype =  rtcp_get_pt(pkt);
        records = rtcp_get_length(pkt);
        uint16_t bytes = (uint16_t)(4 * (1 + records));
        if (bytes > bytes_left)
        {
            /* check for a sane number of bytes */
            msg_Err(p_access, "Malformed feedback packet, wrong len %d, expecting %u bytes in the" \
                " packet, got a buffer of %u bytes. ptype = %d", rtcp_get_length(pkt), bytes, 
                bytes_left, ptype);
            return;
        }

        switch(ptype) {
            case RTCP_PT_RTPFR:
            case RTCP_PT_RTPFB:
                process_nack(p_access, ptype, records, flow, pkt);
                break;

            case RTCP_PT_RR:
                /*
                if (p_sys->b_ismulticast == false)
                    process_rr(f, pkt, len);
                */
                break;

            case RTCP_PT_SDES:
                {
                    if (p_sys->b_ismulticast == false)
                    {
                        int8_t name_length = rtcp_sdes_get_name_length(pkt);
                        if (name_length > bytes_left)
                        {
                            /* check for a sane number of bytes */
                            msg_Err(p_access, "Malformed SDES packet, wrong cname len %u, got a " \
                                "buffer of %u bytes.", name_length, bytes_left);
                            return;
                        }
                        if (memcmp(pkt + RTCP_SDES_SIZE, p_sys->receiver_name, name_length) != 0)
                        {
                            memcpy(p_sys->receiver_name, pkt + RTCP_SDES_SIZE, name_length);
                            msg_Info(p_access, "Receiver name: %s", p_sys->receiver_name);
                        }
                    }
                }
                break;

            case RTCP_PT_SR:
                break;

            default:
                msg_Err(p_access, "   Unrecognized RTCP packet with PTYPE=%02x!!", ptype);
        }
        processed_bytes += bytes;
    }
}

static void rist_rtcp_send(sout_access_out_t *p_access)
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    struct rist_flow *flow = p_sys->flow;
    uint8_t rtcp_buf[RTCP_SR_SIZE + RTCP_SDES_SIZE + MAX_CNAME] = { };
    struct timeval tv;
    int r;
    uint64_t fractions;
    uint16_t namelen = strlen(flow->cname) + 1;
    gettimeofday(&tv, NULL);

    /* Populate SR for sender report */
    uint8_t *p_sr = rtcp_buf;
    rtp_set_hdr(p_sr);
    rtcp_sr_set_pt(p_sr);
    rtcp_sr_set_length(p_sr, 6);
    rtcp_fb_set_int_ssrc_pkt_sender(p_sr, p_sys->ssrc);
    rtcp_sr_set_ntp_time_msw(p_sr, tv.tv_sec + SEVENTY_YEARS_OFFSET);
    fractions = (uint64_t)tv.tv_usec;
    fractions <<= 32ULL;
    fractions /= 1000000ULL;
    rtcp_sr_set_ntp_time_lsw(p_sr, (uint32_t)fractions);
    rtcp_sr_set_rtp_time(p_sr, rtp_get_ts(vlc_tick_now()));
    vlc_mutex_lock( &p_sys->lock );
    rtcp_sr_set_packet_count(p_sr, flow->packets_count);
    rtcp_sr_set_octet_count(p_sr, flow->bytes_count);
    vlc_mutex_unlock( &p_sys->lock );

    /* Populate SDES for sender description */
    uint8_t *p_sdes = (rtcp_buf + RTCP_SR_SIZE);
    /* we need to make sure it is a multiple of 4, pad if necessary */
    if ((namelen - 2) & 0x3)
        namelen = ((((namelen - 2) >> 2) + 1) << 2) + 2;
    rtp_set_hdr(p_sdes);
    rtp_set_cc(p_sdes, 1); /* Actually it is source count in this case */
    rtcp_sdes_set_pt(p_sdes);
    rtcp_set_length(p_sdes, (namelen >> 2) + 2);
    rtcp_sdes_set_cname(p_sdes, 1);
    rtcp_sdes_set_name_length(p_sdes, strlen(flow->cname));
    p_sdes += RTCP_SDES_SIZE;
    strlcpy((char *)p_sdes, flow->cname, namelen);

    /* Send the rtcp message */
    r = send(flow->fd_rtcp, rtcp_buf, RTCP_SR_SIZE + RTCP_SDES_SIZE + namelen, 0);
    (void)r;
}

static void *rist_thread(void *data)
{
    sout_access_out_t *p_access = data;
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    uint64_t now;
    uint8_t pkt[RTP_PKT_SIZE];
    struct pollfd pfd[2];
    int ret;
    ssize_t r;

    int poll_sockets = 1;
    pfd[0].fd = p_sys->flow->fd_rtcp;
    pfd[0].events = POLLIN;
    if (p_sys->b_ismulticast)
    {
        pfd[1].fd = p_sys->flow->fd_rtcp_m;
        pfd[1].events = POLLIN;
        poll_sockets++;
    }

    for (;;) {
        ret = poll(pfd, poll_sockets, RTCP_INTERVAL >> 1);
        int canc = vlc_savecancel();
        if (ret > 0)
        {
            if (pfd[0].revents & POLLIN)
            {
                r = rist_Read(p_sys->flow->fd_rtcp, pkt, RTP_PKT_SIZE);
                if (r == RTP_PKT_SIZE) {
                    msg_Err(p_access, "Rist RTCP messsage is too big (%zd bytes) and was probably " \
                        "cut, please keep it under %d bytes", r, RTP_PKT_SIZE);
                }
                if (unlikely(r == -1)) {
                    msg_Err(p_access, "socket %d error: %s\n", p_sys->flow->fd_rtcp, 
                        gai_strerror(errno));
                }
                else {
                    rist_rtcp_recv(p_access, p_sys->flow, pkt, r);
                }
            }
            if (p_sys->b_ismulticast && (pfd[1].revents & POLLIN))
            {
                r = rist_Read(p_sys->flow->fd_rtcp_m, pkt, RTP_PKT_SIZE);
                if (r == RTP_PKT_SIZE) {
                    msg_Err(p_access, "Rist RTCP messsage is too big (%zd bytes) and was " \
                        "probably cut, please keep it under %d bytes", r, RTP_PKT_SIZE);
                }
                if (unlikely(r == -1)) {
                    msg_Err(p_access, "mcast socket %d error: %s\n", p_sys->flow->fd_rtcp_m,
                        gai_strerror(errno));
                }
                else {
                    rist_rtcp_recv(p_access, p_sys->flow, pkt, r);
                }
            }
        }

        /* And, in any case: */
        now = vlc_tick_now();
        if ((now - p_sys->last_rtcp_tx) > VLC_TICK_FROM_MS(RTCP_INTERVAL))
        {
            rist_rtcp_send(p_access);
            p_sys->last_rtcp_tx = now;
        }
        vlc_restorecancel (canc);
    }

    return NULL;
}

/****************************************************************************
 * RTP send
 ****************************************************************************/
static void* ThreadSend( void *data )
{
    sout_access_out_t *p_access = data;
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    vlc_tick_t i_caching = p_sys->i_ticks_caching;
    struct rist_flow *flow = p_sys->flow;
    block_t *out;

    while ((out = vlc_queue_DequeueKillable(&p_sys->queue,
                                            &p_sys->dead)) != NULL)
    {
        ssize_t len = 0;
        uint16_t seq = 0;
        uint32_t pkt_ts = 0;

        vlc_tick_wait (out->i_dts + i_caching);

        len = out->i_buffer;

        seq = rtp_get_seqnum(out->p_buffer);
        pkt_ts = rtp_get_timestamp(out->p_buffer);

        vlc_mutex_lock( &p_sys->fd_lock );
#ifdef TEST_PACKET_LOSS
#   warning COMPILED WITH SELF INFLICTED PACKET LOSS
        if ((seq % 14) == 0) {
            /*msg_Err(p_access, "Dropped packet with seq number %d ...", seq);*/
        }
        else
        {
            if (rist_Write(flow->fd_out, out->p_buffer, len) != len) {
                msg_Err(p_access, "Error sending data packet after 2 tries ...");
            }
        }
#else
        if (rist_Write(flow->fd_out, out->p_buffer, len) != len) {
            msg_Err(p_access, "Error sending data packet after 2 tries ...");
        }
#endif
        vlc_mutex_unlock( &p_sys->fd_lock );

        /* Insert Into Queue */
        vlc_mutex_lock( &p_sys->lock );
        /* Always replace the existing one with the new one */
        struct rtp_pkt *pkt;
        pkt = &(flow->buffer[seq]);
        if (pkt->buffer)
        {
            block_Release(pkt->buffer);
            pkt->buffer = NULL;
        }
        pkt->rtp_ts = pkt_ts;
        pkt->buffer = out;

        if (flow->reset == 1)
        {
            msg_Info(p_access, "Traffic detected");
            /* First packet in the queue */
            flow->reset = 0;
        }
        flow->wi = seq;
        flow->hi_timestamp = pkt_ts;
        /* Stats for RTCP feedback packets */
        flow->packets_count++;
        flow->bytes_count += len;
        flow->last_out = seq;
        vlc_mutex_unlock( &p_sys->lock );

        /* We print out the stats once per second */
        uint64_t now = vlc_tick_now();
        uint64_t interval = (now - p_sys->i_last_stat);
        if ( interval > VLC_TICK_FROM_MS(STATS_INTERVAL) )
        {
            if (p_sys->i_retransmit_packets > 0)
            {
                float quality = 100;
                if (p_sys->i_total_packets > 0)
                    quality = (float)100 - (float)100*(float)(p_sys->i_retransmit_packets)
                        /(float)p_sys->i_total_packets;
                msg_Info(p_access, "STATS: Total %u, Retransmitted %u, Link Quality %.2f%%", 
                    p_sys->i_total_packets, p_sys->i_retransmit_packets, quality);
            }
            p_sys->i_last_stat = now;
            p_sys->i_retransmit_packets = 0;
            p_sys->i_total_packets = 0;
        }
        p_sys->i_total_packets++;
    }
    return NULL;
}

static void SendtoFIFO( sout_access_out_t *p_access, block_t *buffer )
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    uint16_t seq = p_sys->rtp_counter++;

    /* Set fresh rtp header data */
    uint8_t *bufhdr = buffer->p_buffer;
    rtp_set_hdr(bufhdr);
    rtp_set_type(bufhdr, MPEG_II_TRANSPORT_STREAM);
    rtp_set_seqnum(bufhdr, seq);
    rtp_set_int_ssrc(bufhdr, p_sys->ssrc);
    uint32_t pkt_ts = rtp_get_ts(buffer->i_dts);
    rtp_set_timestamp(bufhdr, pkt_ts);

    vlc_queue_Enqueue(&p_sys->queue, block_Duplicate(buffer));
}

static ssize_t Write( sout_access_out_t *p_access, block_t *p_buffer )
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    int i_len = 0;

    while( p_buffer )
    {
        block_t *p_next;
        int i_block_split = 0;

        if( !p_sys->b_mtu_warning && p_buffer->i_buffer > p_sys->i_packet_size )
        {
            msg_Warn( p_access, "Buffer data size (%zu) > configured packet size (%zu), you " \
                "should probably increase the configured packet size", p_buffer->i_buffer, 
                p_sys->i_packet_size );
            p_sys->b_mtu_warning = true;
        }

        /* Temp buffer is already too large, flush */
        if( p_sys->p_pktbuffer->i_buffer + p_buffer->i_buffer > p_sys->i_packet_size )
        {
            SendtoFIFO(p_access, p_sys->p_pktbuffer);
            p_sys->p_pktbuffer->i_buffer = RTP_HEADER_SIZE;
        }

        i_len += p_buffer->i_buffer;

        while( p_buffer->i_buffer )
        {

            size_t i_write = __MIN( p_buffer->i_buffer, p_sys->i_packet_size );

            i_block_split++;

            if( p_sys->p_pktbuffer->i_buffer == RTP_HEADER_SIZE )
            {
                p_sys->p_pktbuffer->i_dts = p_buffer->i_dts;
            }

            memcpy( p_sys->p_pktbuffer->p_buffer + p_sys->p_pktbuffer->i_buffer,
                    p_buffer->p_buffer, i_write );

            p_sys->p_pktbuffer->i_buffer += i_write;
            p_buffer->p_buffer += i_write;
            p_buffer->i_buffer -= i_write;

            /*  Flush if we reached the target size for the case of block size < target packet size.
             *  Also flush when we are in block_split > 1 for the case when the block_size is
             *  larger than the packet-size because we need to continue the inner loop */
            if( p_sys->p_pktbuffer->i_buffer == p_sys->i_packet_size || i_block_split > 1 )
            {
                SendtoFIFO(p_access, p_sys->p_pktbuffer);
                p_sys->p_pktbuffer->i_buffer = RTP_HEADER_SIZE;
            }

        }

        p_next = p_buffer->p_next;
        block_Release( p_buffer );
        p_buffer = p_next;

    }

    if ( i_len <= 0 ) {
        block_ChainRelease( p_buffer );
    }
    return i_len;
}

static int Control( sout_access_out_t *p_access, int i_query, va_list args )
{
    VLC_UNUSED( p_access );

    int i_ret = VLC_SUCCESS;

    switch( i_query )
    {
        case ACCESS_OUT_CONTROLS_PACE:
            *va_arg( args, bool * ) = false;
            break;

        default:
            i_ret = VLC_EGENERIC;
            break;
    }

    return i_ret;
}

static void Clean( sout_access_out_t *p_access )
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;

    if ( p_sys->flow )
    {
        if (p_sys->flow->fd_out >= 0) {
            net_Close (p_sys->flow->fd_out);
        }
        if (p_sys->flow->fd_rtcp >= 0) {
            net_Close (p_sys->flow->fd_rtcp);
        }
        if (p_sys->flow->fd_rtcp_m >= 0) {
            net_Close (p_sys->flow->fd_rtcp_m);
        }
        for (int i=0; i<RIST_QUEUE_SIZE; i++) {
            struct rtp_pkt *pkt = &(p_sys->flow->buffer[i]);
            if (pkt->buffer)
            {
                block_Release(pkt->buffer);
                pkt->buffer = NULL;
            }
        }
        free(p_sys->flow->buffer);
        free(p_sys->flow);
    }

    if (p_sys->p_pktbuffer)
        block_Release(p_sys->p_pktbuffer);
}

static void Close( vlc_object_t * p_this )
{
    sout_access_out_t     *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t *p_sys = p_access->p_sys;

    vlc_cancel(p_sys->ristthread);
    vlc_queue_Kill(&p_sys->queue, &p_sys->dead);

    vlc_join(p_sys->ristthread, NULL);
    vlc_join(p_sys->senderthread, NULL);

    Clean( p_access );
}

static int Open( vlc_object_t *p_this )
{
    sout_access_out_t       *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t   *p_sys = NULL;

    if (var_Create ( p_access, "dst-port", VLC_VAR_INTEGER )
     || var_Create ( p_access, "src-port", VLC_VAR_INTEGER )
     || var_Create ( p_access, "dst-addr", VLC_VAR_STRING )
     || var_Create ( p_access, "src-addr", VLC_VAR_STRING ) )
    {
        msg_Err( p_access, "Valid network information is required." );
        return VLC_ENOMEM;
    }

    config_ChainParse( p_access, SOUT_CFG_PREFIX, ppsz_sout_options, p_access->p_cfg );

    p_sys = vlc_obj_calloc( p_this, 1, sizeof( *p_sys ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    int i_dst_port = RIST_DEFAULT_PORT;
    char *psz_dst_addr;
    char *psz_parser = psz_dst_addr = strdup( p_access->psz_path );
    if( !psz_dst_addr )
        return VLC_ENOMEM;

    if ( psz_parser[0] == '[' )
        psz_parser = strchr( psz_parser, ']' );

    psz_parser = strchr( psz_parser ? psz_parser : psz_dst_addr, ':' );
    if ( psz_parser != NULL )
    {
        *psz_parser++ = '\0';
        i_dst_port = atoi( psz_parser );
    }

    vlc_mutex_init( &p_sys->lock );
    vlc_mutex_init( &p_sys->fd_lock );

    msg_Info(p_access, "Connecting RIST output to %s:%d and %s:%d", psz_dst_addr, i_dst_port, 
        psz_dst_addr, i_dst_port+1);
    p_sys->b_ismulticast = is_multicast_address(psz_dst_addr);
    struct rist_flow *flow = rist_udp_transmitter(p_access, psz_dst_addr, i_dst_port,
        p_sys->b_ismulticast);
    free (psz_dst_addr);
    if (!flow)
        goto failed;

    p_sys->flow = flow;
    flow->latency = var_InheritInteger(p_access, SOUT_CFG_PREFIX "buffer-size");
    flow->rtp_latency = rtp_get_ts(VLC_TICK_FROM_MS(flow->latency));
    p_sys->ssrc = var_InheritInteger(p_access, SOUT_CFG_PREFIX "ssrc");
    if (p_sys->ssrc == 0) {
        vlc_rand_bytes(&p_sys->ssrc, 4);
    }
    /* Last bit of ssrc must be 0 for normal data and 1 for retries */
    p_sys->ssrc &= ~(1 << 0);

    msg_Info(p_access, "SSRC: 0x%08X", p_sys->ssrc);
    p_sys->i_ticks_caching = VLC_TICK_FROM_MS(var_InheritInteger( p_access, 
        SOUT_CFG_PREFIX "caching"));
    p_sys->i_packet_size = var_InheritInteger(p_access, SOUT_CFG_PREFIX "packet-size" );
    p_sys->dead = false;
    vlc_queue_Init(&p_sys->queue, offsetof (block_t, p_next));
    p_sys->p_pktbuffer = block_Alloc( p_sys->i_packet_size );
    if( unlikely(p_sys->p_pktbuffer == NULL) )
        goto failed;

    p_sys->p_pktbuffer->i_buffer = RTP_HEADER_SIZE;

    p_access->p_sys = p_sys;

    if( vlc_clone(&p_sys->senderthread, ThreadSend, p_access, VLC_THREAD_PRIORITY_HIGHEST ) )
    {
        msg_Err(p_access, "Failed to create sender thread.");
        goto failed;
    }

    if (vlc_clone(&p_sys->ristthread, rist_thread, p_access, VLC_THREAD_PRIORITY_INPUT))
    {
        msg_Err(p_access, "Failed to create worker thread.");
        vlc_queue_Kill(&p_sys->queue, &p_sys->dead);
        vlc_join(p_sys->senderthread, NULL);
        goto failed;
    }

    p_access->pf_write = Write;
    p_access->pf_control = Control;

    return VLC_SUCCESS;

failed:
    Clean( p_access );
    return VLC_EGENERIC;
}

#define CACHING_TEXT N_("RIST data output caching size (ms)")
#define CACHING_LONGTEXT N_( \
    "Having this cache will guarantee that the packets going out are " \
    "delivered at a spacing determined by the chain timestamps thus ensuring " \
    "a near jitter free output. Be aware that this setting will also add to " \
    "the overall latency of the stream." )

#define BUFFER_TEXT N_("RIST retry-buffer queue size (ms)")
#define BUFFER_LONGTEXT N_( \
    "This must match the buffer size (latency) configured on the server side. If you " \
    "are not sure, leave the default of 0 which will set it the maximum " \
    "value and will use about 100MB of RAM" )

#define SSRC_TEXT N_("SSRC used in RTP output (default is random, i.e. 0)")
#define SSRC_LONGTEXT N_( \
    "Use this setting to specify a known SSRC for the RTP header. This is only useful " \
    "if your receiver acts on it. When using VLC as receiver, it is not." )

#define NAME_TEXT N_("Stream name")
#define NAME_LONGTEXT N_( \
    "This Stream name will be sent to the receiver using the rist RTCP channel" )

/* Module descriptor */
vlc_module_begin()

    set_shortname( N_("RIST") )
    set_description( N_("RIST stream output") )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_ACO )

    add_integer( SOUT_CFG_PREFIX "packet-size", RIST_TARGET_PACKET_SIZE,
            N_("RIST target packet size (bytes)"), NULL, true )
    add_integer( SOUT_CFG_PREFIX "caching", DEFAULT_CACHING_DELAY,
            CACHING_TEXT, CACHING_LONGTEXT, true )
    add_integer( SOUT_CFG_PREFIX "buffer-size", DEFAULT_BUFFER_SIZE,
            BUFFER_TEXT, BUFFER_LONGTEXT, true )
    add_integer( SOUT_CFG_PREFIX "ssrc", 0,
            SSRC_TEXT, SSRC_LONGTEXT, true )
    add_string( SOUT_CFG_PREFIX "stream-name", NULL, NAME_TEXT, NAME_LONGTEXT, true )

    set_capability( "sout access", 0 )
    add_shortcut( "rist", "tr06" )

    set_callbacks( Open, Close )

vlc_module_end ()
