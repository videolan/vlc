/*****************************************************************************
 * rist.h: RIST (Reliable Internet Stream Transport) helper
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

#include <stdint.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <errno.h>
#include <assert.h>

/*****************************************************************************
 * Public API
 *****************************************************************************/

/* RIST */

/* RTP header format (RFC 3550) */
/*
   0                   1                   2                   3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           timestamp                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           synchronization source (SSRC) identifier            |
   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   |            contributing source (CSRC) identifiers             |
   |                             ....                              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

#define RIST_QUEUE_SIZE 65536
#define RTP_PKT_SIZE (1472)

#define RTCP_INTERVAL 75 /*ms*/

#define SEVENTY_YEARS_OFFSET (2208988800ULL)
#define MAX_NACKS 32
#define MAX_CNAME 128
#define RTCP_EMPTY_RR_SIZE 8

#define RTCP_PT_RTPFR             204

#define RIST_TICK_FROM_MS(ms) ((CLOCK_FREQ / INT64_C(1000)) * (ms))

struct rtp_pkt {
    uint32_t rtp_ts;
    struct block_t *buffer;
};

/* RIST NACK header format  */
/*
   0                   1                   2                   3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       SNBase low bits         |        Length recovery        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |E| PT recovery |                    Mask                       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                          TS recovery                          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |N|D|type |index|    Offset     |      NA       |SNBase ext bits|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

struct rist_flow {
    uint8_t reset;
    struct rtp_pkt *buffer;
    uint32_t qsize;
    uint32_t last_out;
    uint32_t ssrc;
    char cname[MAX_CNAME];
    struct sockaddr_storage peer_sockaddr;
    socklen_t peer_socklen;
    uint16_t ri, wi;
    int fd_in;
    int fd_out;
    int fd_rtcp;
    int fd_rtcp_m;
    int fd_nack;
    uint8_t nacks_retries[RIST_QUEUE_SIZE];
    uint32_t hi_timestamp;
    uint64_t feedback_time;
    uint32_t latency;
    uint32_t rtp_latency;
    uint32_t retry_interval;
    uint32_t reorder_buffer;
    uint8_t max_retries;
    uint32_t packets_count;
    uint32_t bytes_count;
};

static inline uint16_t rtcp_fb_nack_get_range_start(const uint8_t *p_rtcp_fb_nack)
{
    return (p_rtcp_fb_nack[0] << 8) | p_rtcp_fb_nack[1];
}

static inline uint16_t rtcp_fb_nack_get_range_extra(const uint8_t *p_rtcp_fb_nack)
{
    return (p_rtcp_fb_nack[2] << 8) | p_rtcp_fb_nack[3];
}

static inline void rtcp_fb_nack_set_range_start(uint8_t *p_rtcp_fb_nack,
                                              uint16_t start)
{
    p_rtcp_fb_nack[0] = (start >> 8) & 0xff;
    p_rtcp_fb_nack[1] = start & 0xff;
}

static inline void rtcp_fb_nack_set_range_extra(uint8_t *p_rtcp_fb_nack,
                                                 uint16_t extra)
{
    p_rtcp_fb_nack[2] = (extra >> 8) & 0xff;
    p_rtcp_fb_nack[3] = extra & 0xff;
}

static inline void populate_cname(int fd, char *identifier)
{
    /* Set the CNAME Identifier as host@ip:port and fallback to hostname if needed */
    char hostname[MAX_CNAME];
    struct sockaddr_storage peer_sockaddr;
    int name_length = 0;
    socklen_t peer_socklen = 0;
    int ret_hostname = gethostname(hostname, MAX_CNAME);
    if (ret_hostname == -1)
        snprintf(hostname, MAX_CNAME, "UnknownHost");
    int ret_sockname = getsockname(fd, (struct sockaddr *)&peer_sockaddr, &peer_socklen);
    if (ret_sockname == 0)
    {
        struct sockaddr *peer = (struct sockaddr *)&peer_sockaddr;
        if (peer->sa_family == AF_INET) {
            struct sockaddr_in *xin = (void*)peer;
            name_length = snprintf(identifier, MAX_CNAME, "%s@%s:%u", hostname,
                            inet_ntoa(xin->sin_addr), ntohs(xin->sin_port));
            if (name_length >= MAX_CNAME)
                identifier[MAX_CNAME-1] = 0;
        } else if (peer->sa_family == AF_INET6) {
            struct sockaddr_in6 *xin6 = (void*)peer;
            char str[INET6_ADDRSTRLEN];
            inet_ntop(xin6->sin6_family, &xin6->sin6_addr, str, sizeof(struct in6_addr));
            name_length = snprintf(identifier, MAX_CNAME, "%s@%s:%u", hostname,
                            str, ntohs(xin6->sin6_port));
            if (name_length >= MAX_CNAME)
                identifier[MAX_CNAME-1] = 0;
        }
    }
    if (name_length == 0)
    {
        name_length = snprintf(identifier, MAX_CNAME, "%s", hostname);
        if (name_length >= MAX_CNAME)
            identifier[MAX_CNAME-1] = 0;
    }
}

static inline uint32_t rtp_get_ts( int64_t i_pts )
{
    unsigned i_clock_rate = 90000;
    /* This is an overflow-proof way of doing:
     * return i_pts * (int64_t)i_clock_rate / CLOCK_FREQ;
     *
     * NOTE: this plays nice with offsets because the (equivalent)
     * calculations are linear. */
    lldiv_t q = lldiv(i_pts, CLOCK_FREQ);
    return q.quot * (int64_t)i_clock_rate
          + q.rem * (int64_t)i_clock_rate / CLOCK_FREQ;
}

static inline int64_t ts_get_from_rtp( uint32_t i_rtp_ts )
{
    unsigned i_clock_rate = 90000;
    return (int64_t)i_rtp_ts * (int64_t)(CLOCK_FREQ/i_clock_rate);
}

static inline ssize_t rist_ReadFrom_i11e(int fd, void *buf, size_t len, struct sockaddr *peer, 
    socklen_t *slen)
{
    ssize_t ret = -1;

    if (peer == NULL)
        ret = vlc_recv_i11e(fd, buf, len, 0);
    else
        ret = vlc_recvfrom_i11e(fd, buf, len, 0, peer, slen);

    if (ret == -1)
    {
        switch (errno)
        {
        case EAGAIN:
        case EINTR:
            if (vlc_killed())
                return ret;

            /* retry one time */
            if (peer == NULL)
                ret = vlc_recv_i11e(fd, buf, len, 0);
            else
                ret = vlc_recvfrom_i11e(fd, buf, len, 0, peer, slen);
        default:
            break;
        }
    }
    return ret;
}

static inline ssize_t rist_Read_i11e(int fd, void *buf, size_t len)
{
    return rist_ReadFrom_i11e(fd, buf, len, NULL, NULL);
}

static inline ssize_t rist_ReadFrom(int fd, void *buf, size_t len, struct sockaddr *peer, 
    socklen_t *slen)
{
    ssize_t ret = -1;

    if (peer == NULL)
        ret = recv(fd, buf, len, 0);
    else
        ret = recvfrom(fd, buf, len, 0, peer, slen);

    if (ret == -1)
    {
        switch (errno)
        {
        case EAGAIN:
        case EINTR:
            /* retry one time */
            if (peer == NULL)
                ret = recv(fd, buf, len, 0);
            else
                ret = recvfrom(fd, buf, len, 0, peer, slen);
        default:
            break;
        }
    }
    return ret;
}

static inline ssize_t rist_Read(int fd, void *buf, size_t len)
{
    return rist_ReadFrom(fd, buf, len, NULL, NULL);
}

static inline ssize_t rist_WriteTo_i11e(int fd, const void *buf, size_t len, 
    const struct sockaddr *peer, socklen_t slen)
{
#ifdef _WIN32
    # define ENOBUFS      WSAENOBUFS
    # define EAGAIN       WSAEWOULDBLOCK
    # define EWOULDBLOCK  WSAEWOULDBLOCK
#endif
    ssize_t r = -1;
    if (slen == 0)
        r = vlc_send_i11e( fd, buf, len, 0 );
    else
        r = vlc_sendto_i11e( fd, buf, len, 0, peer, slen );
    if( r == -1
        && net_errno != EAGAIN && net_errno != EWOULDBLOCK
        && net_errno != ENOBUFS && net_errno != ENOMEM )
    {
        int type;
        if (!getsockopt( fd, SOL_SOCKET, SO_TYPE,
                    &type, &(socklen_t){ sizeof(type) }))
        {
            if( type == SOCK_DGRAM )
            {
                /* ICMP soft error: ignore and retry */
                if (slen == 0)
                    r = vlc_send_i11e( fd, buf, len, 0 );
                else
                    r = vlc_sendto_i11e( fd, buf, len, 0, peer, slen );
            }
        }
    }
    return r;
}

static inline ssize_t rist_Write_i11e(int fd, const void *buf, size_t len)
{
    return rist_WriteTo_i11e(fd, buf, len, NULL, 0);
}

static inline ssize_t rist_WriteTo(int fd, const void *buf, size_t len, const struct sockaddr *peer, 
    socklen_t slen)
{
#ifdef _WIN32
    # define ENOBUFS      WSAENOBUFS
    # define EAGAIN       WSAEWOULDBLOCK
    # define EWOULDBLOCK  WSAEWOULDBLOCK
#endif
    ssize_t r = -1;
    if (slen == 0)
        r = send( fd, buf, len, 0 );
    else
        r = sendto( fd, buf, len, 0, peer, slen );
    if( r == -1
        && net_errno != EAGAIN && net_errno != EWOULDBLOCK
        && net_errno != ENOBUFS && net_errno != ENOMEM )
    {
        int type;
        if (!getsockopt( fd, SOL_SOCKET, SO_TYPE,
                    &type, &(socklen_t){ sizeof(type) }))
        {
            if( type == SOCK_DGRAM )
            {
                /* ICMP soft error: ignore and retry */
                if (slen == 0)
                    r = send( fd, buf, len, 0 );
                else
                    r = sendto( fd, buf, len, 0, peer, slen );
            }
        }
    }
    return r;
}

static inline ssize_t rist_Write(int fd, const void *buf, size_t len)
{
    return rist_WriteTo(fd, buf, len, NULL, 0);
}

static bool is_multicast_address(char *psz_dst_server)
{
    int ret;
    int ismulticast = 0;

    struct addrinfo hint = {
        .ai_socktype = SOCK_DGRAM,
        .ai_protocol = IPPROTO_UDP,
        .ai_flags = AI_NUMERICSERV | AI_IDN | AI_PASSIVE,
    }, *res;

    ret = vlc_getaddrinfo(psz_dst_server, 0, &hint, &res);
    if (ret) {
        return 0;
    } else if(res->ai_family == AF_INET) {
        unsigned long addr = ntohl(inet_addr(psz_dst_server));
        ismulticast =  IN_MULTICAST(addr);
    } else if (res->ai_family == AF_INET6) {
        if (strlen(psz_dst_server) >= 5 && (strncmp("[ff00", psz_dst_server, 5) == 0 ||
                    strncmp("[FF00", psz_dst_server, 5) == 0))
            ismulticast = 1;
    }

    freeaddrinfo(res);

    return ismulticast;
}

// imported from bitstream
#define RTP_HEADER_SIZE               12
#define RTCP_SR_SIZE                  28
#define RTCP_SDES_SIZE                10
#define RTCP_FB_HEADER_SIZE           12
#define RTCP_FB_FCI_GENERIC_NACK_SIZE 4
#define RTCP_PT_SR                    200
#define RTCP_PT_RR                    201
#define RTCP_PT_SDES                  202
#define RTCP_PT_RTPFB                 205

static inline uint32_t rtp_get_timestamp(const uint8_t *p_rtp)
{
    return (p_rtp[4] << 24) | (p_rtp[5] << 16) | (p_rtp[6] << 8) | p_rtp[7];
}

static inline void rtp_set_timestamp(uint8_t *p_rtp, uint32_t i_timestamp)
{
    p_rtp[4] = (i_timestamp >> 24) & 0xff;
    p_rtp[5] = (i_timestamp >> 16) & 0xff;
    p_rtp[6] = (i_timestamp >> 8) & 0xff;
    p_rtp[7] = i_timestamp & 0xff;
}

static inline uint16_t rtp_get_seqnum(const uint8_t *p_rtp)
{
    return (p_rtp[2] << 8) | p_rtp[3];
}

static inline void rtp_set_seqnum(uint8_t *p_rtp, uint16_t i_seqnum)
{
    p_rtp[2] = i_seqnum >> 8;
    p_rtp[3] = i_seqnum & 0xff;
}

static inline int8_t rtcp_sdes_get_name_length(const uint8_t *p_rtcp_sdes)
{
    return p_rtcp_sdes[9];
}

static inline uint16_t rtcp_get_length(const uint8_t *p_rtcp)
{
    return (p_rtcp[2] << 8) | p_rtcp[3];
}

static inline void rtcp_set_length(uint8_t *p_rtcp,
                                      uint16_t length)
{
    p_rtcp[2] = length >> 8;
    p_rtcp[3] = length & 0xff;
}

static inline uint8_t rtcp_get_pt(const uint8_t *p_rtcp)
{
    return p_rtcp[1];
}

static inline void rtcp_set_pt(uint8_t *p_rtcp, uint8_t pt)
{
    p_rtcp[1] = pt;
}

static inline bool rtp_check_hdr(const uint8_t *p_rtp)
{
    return (p_rtp[0] & 0xc0) == 0x80;
}

static inline void rtp_set_hdr(uint8_t *p_rtp)
{
    p_rtp[0] = 0x80;
}

static inline void rtcp_rr_set_pt(uint8_t *p_rtcp_rr)
{
    rtcp_set_pt(p_rtcp_rr, RTCP_PT_RR);
}

static inline void rtcp_fb_set_int_ssrc_pkt_sender(uint8_t *p_rtcp_fb, uint32_t i_ssrc)
{
    p_rtcp_fb[4] = (i_ssrc >> 24) & 0xff;
    p_rtcp_fb[5] = (i_ssrc >> 16) & 0xff;
    p_rtcp_fb[6] = (i_ssrc >> 8) & 0xff;
    p_rtcp_fb[7] = i_ssrc & 0xff;
}

static inline void rtp_set_cc(uint8_t *p_rtp, uint8_t i_cc)
{
    p_rtp[0] &= 0xf0;
    p_rtp[0] |= i_cc & 0xf;
}

static inline void rtcp_sdes_set_pt(uint8_t *p_rtcp_rr)
{
    rtcp_set_pt(p_rtcp_rr, RTCP_PT_SDES);
}

static inline void rtcp_sdes_set_cname(uint8_t *p_rtcp_sdes, uint8_t cname)
{
    p_rtcp_sdes[8] = cname;
}

static inline void rtcp_sdes_set_name_length(uint8_t *p_rtcp_sdes,
        int8_t name_length)
{
    p_rtcp_sdes[9] = name_length;
}

static inline uint8_t rtcp_fb_get_fmt(const uint8_t *p_rtcp)
{
    return p_rtcp[0] & 0x1f;
}

static inline void rtcp_fb_set_fmt(uint8_t *p_rtcp, uint8_t fmt)
{
    p_rtcp[0] |= fmt & 0x1f;
}

static inline uint16_t rtcp_fb_nack_get_packet_id(const uint8_t *p_rtcp_fb_nack)
{
    return (p_rtcp_fb_nack[0] << 8) | p_rtcp_fb_nack[1];
}

static inline void rtcp_fb_nack_set_packet_id(uint8_t *p_rtcp_fb_nack,
                                              uint16_t packet_id)
{
    p_rtcp_fb_nack[0] = (packet_id >> 8) & 0xff;
    p_rtcp_fb_nack[1] = packet_id & 0xff;
}

static inline uint16_t rtcp_fb_nack_get_bitmask_lost(const uint8_t *p_rtcp_fb_nack)
{
    return (p_rtcp_fb_nack[2] << 8) | p_rtcp_fb_nack[3];
}

static inline void rtcp_fb_nack_set_bitmask_lost(uint8_t *p_rtcp_fb_nack,
                                                 uint16_t bitmask)
{
    p_rtcp_fb_nack[2] = (bitmask >> 8) & 0xff;
    p_rtcp_fb_nack[3] = bitmask & 0xff;
}

static inline void rtcp_fb_get_ssrc_media_src(const uint8_t *p_rtcp_fb,
                                              uint8_t pi_ssrc[4])
{
    pi_ssrc[0] = p_rtcp_fb[8];
    pi_ssrc[1] = p_rtcp_fb[9];
    pi_ssrc[2] = p_rtcp_fb[10];
    pi_ssrc[3] = p_rtcp_fb[11];
}

static inline void rtcp_fb_set_ssrc_media_src(uint8_t *p_rtcp_fb,
                                              const uint8_t pi_ssrc[4])
{
    p_rtcp_fb[8] = pi_ssrc[0];
    p_rtcp_fb[9] = pi_ssrc[1];
    p_rtcp_fb[10] = pi_ssrc[2];
    p_rtcp_fb[11] = pi_ssrc[3];
}

static inline void rtcp_sr_set_pt(uint8_t *p_rtcp_sr)
{
    rtcp_set_pt(p_rtcp_sr, RTCP_PT_SR);
}

static inline void rtcp_sr_set_length(uint8_t *p_rtcp_sr,
                                      uint16_t length)
{
    rtcp_set_length(p_rtcp_sr, length);
}

static inline void rtcp_sr_set_ntp_time_msw(uint8_t *p_rtcp_sr,
                                            uint32_t ntp_time_msw)
{
    p_rtcp_sr[8] = (ntp_time_msw >> 24) & 0xff;
    p_rtcp_sr[9] = (ntp_time_msw >> 16) & 0xff;
    p_rtcp_sr[10] = (ntp_time_msw >> 8) & 0xff;
    p_rtcp_sr[11] = ntp_time_msw & 0xff;
}

static inline void rtcp_sr_set_ntp_time_lsw(uint8_t *p_rtcp_sr,
                                            uint32_t ntp_time_lsw)
{
    p_rtcp_sr[12] = (ntp_time_lsw >> 24) & 0xff;
    p_rtcp_sr[13] = (ntp_time_lsw >> 16) & 0xff;
    p_rtcp_sr[14] = (ntp_time_lsw >> 8) & 0xff;
    p_rtcp_sr[15] = ntp_time_lsw & 0xff;
}

static inline void rtcp_sr_set_rtp_time(uint8_t *p_rtcp_sr,
                                            uint32_t rtp_time)
{
    p_rtcp_sr[16] = (rtp_time >> 24) & 0xff;
    p_rtcp_sr[17] = (rtp_time >> 16) & 0xff;
    p_rtcp_sr[18] = (rtp_time >> 8) & 0xff;
    p_rtcp_sr[19] = rtp_time & 0xff;
}

static inline void rtcp_sr_set_packet_count(uint8_t *p_rtcp_sr,
                                            uint32_t packet_count)
{
    p_rtcp_sr[20] = (packet_count >> 24) & 0xff;
    p_rtcp_sr[21] = (packet_count >> 16) & 0xff;
    p_rtcp_sr[22] = (packet_count >> 8) & 0xff;
    p_rtcp_sr[23] = packet_count & 0xff;
}

static inline void rtcp_sr_set_octet_count(uint8_t *p_rtcp_sr,
                                            uint32_t octet_count)
{
    p_rtcp_sr[24] = (octet_count >> 24) & 0xff;
    p_rtcp_sr[25] = (octet_count >> 16) & 0xff;
    p_rtcp_sr[26] = (octet_count >> 8) & 0xff;
    p_rtcp_sr[27] = octet_count & 0xff;
}

static inline void rtp_set_type(uint8_t *p_rtp, uint8_t i_type)
{
    p_rtp[1] = i_type & 0x7f;
}

static inline void rtp_set_int_ssrc(uint8_t *p_rtp, uint32_t i_ssrc)
{
    p_rtp[8] = (i_ssrc >> 24) & 0xff;
    p_rtp[9] = (i_ssrc >> 16) & 0xff;
    p_rtp[10] = (i_ssrc >> 8) & 0xff;
    p_rtp[11] = i_ssrc & 0xff;
}
