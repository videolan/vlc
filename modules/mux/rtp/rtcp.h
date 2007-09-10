/*****************************************************************************
 * rtcp.h: RTP/RTCP headerfile
 *****************************************************************************
 * Copyright (C) 2005-2007 M2X
 *
 * $Id$
 *
 * Authors: Jean-Paul Saman <jpsaman #_at_# videolan dot org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef _RTCP_H
#define _RTCP_H 1

/* RTCP packet types */
#define RTCP_SR     200
#define RTCP_RR     201
#define RTCP_SDES   202
#define RTCP_BYE    203
#define RTCP_APP    204
/* End of RTCP packet types */

/* SDES type */
#define RTCP_SDES_CNAME 1
#define RTCP_SDES_NAME  2
#define RTCP_SDES_EMAIL 3
#define RTCP_SDES_PHONE 4
#define RTCP_SDES_LOC   5
#define RTCP_SDES_TOOL  6
#define RTCP_SDES_NOTE  7
#define RTCP_SDES_PRIV  8
/* End of SDES type */

#define RTCP_HEADER_LEN 3

typedef enum rtcp_event_enum
{
    EVENT_BYE,
    EVENT_REPORT

} rtcp_event_t;

typedef struct
{
    uint32_t ntp_timestampH;
    uint32_t ntp_timestampL;
    uint32_t rtp_timestamp;
    uint32_t u_pkt_count;
    uint32_t u_octet_count;

} rtcp_SR_t;

typedef struct
{

} rtcp_RR_t;

typedef struct SDES_item_t
{
    uint32_t       i_index;     /*< index */
    uint8_t        u_type;      /*< type field */
    char           *psz_data;    /*< null terminated string */

} rtcp_SDES_item_t;

typedef struct
{
    uint32_t    u_length;        /*< length of packet */
    uint32_t    u_items;         /*< number of SDES_items */
    rtcp_SDES_item_t **pp_items; /*< list of SDES_items */

} rtcp_SDES_t;

typedef struct
{
    uint32_t u_length;        /*< length of packet */

} rtcp_BYE_t;

typedef struct
{
    uint32_t u_length;        /*< length of packet */
    uint32_t u_prefix_len;    /*< length of prefix data */
    unsigned char *psz_prefix;/*< prefix string (null terminated) */
    unsigned char *psz_data;  /*< data string (null terminated) */

} rtcp_APP_t;

/**
 * structure rtcp_stats_t
 */
typedef struct
{
    uint32_t  u_RR_received;   /*< RR records received */
    uint32_t  u_SR_received;   /*< SR records received */
    uint64_t  l_dest_SSRC;     /*< SSRC of first source */
    uint32_t  u_pkt_count;     /*< count of packets received */
    uint32_t  u_octet_count;   /*< ? */
    uint32_t  u_pkt_lost;      /*< cumulative count of packets lost */
    uint8_t   u_fraction_lost; /*< count of fractional packets lost */
    uint32_t  u_highest_seq_no;/*< highest sequence number found */
    uint32_t  u_jitter;        /*< inter arrival jitter calculated */
    uint32_t  u_last_SR;       /*< last SR record received */
    uint32_t  u_last_RR;       /*< last RR record received */
    mtime_t   u_delay_since_last_SR; /*< delay since last SR record received */
    mtime_t   u_delay_since_last_RR; /*< delay since last RR record received */

    uint64_t  u_avg_pkt_size;  /*< average RTCP packet size */
    uint64_t  u_sent_pkt_size; /*< RTCP packet size sent */

} rtcp_stats_t;

typedef struct
{
    uint32_t u_version;        /*< RTCP version number */
    vlc_bool_t b_padding;      /*< indicates if packets has padding */
    uint32_t u_report;         /*< reception packet count */
    uint32_t u_payload_type;   /*< type of RTCP payload */
    uint32_t u_length;         /*< length of packet */
    uint32_t u_ssrc;           /*< channel name this packet belongs to */

    union {
        rtcp_SR_t sr;                /*< SR record */
        rtcp_RR_t rr;                /*< RR record */
        rtcp_SDES_t sdes;            /*< SDES record */
        rtcp_BYE_t bye;              /*< BYE record */
        rtcp_APP_t app;              /*< APP record */
    } report;

} rtcp_pkt_t;

typedef struct rtcp_client_t
{
    int fd;                        /*< socket descriptor of rtcp stream */

    uint32_t    i_index;
    uint32_t    u_ssrc;            /*< channel name */
    vlc_bool_t  b_deleted;         /*< channel deleted ? */
    mtime_t     i_timeout;         /*< remove timeout before real deletion,
                                    * this is recommended by RFC 3550 at
                                    * page 27 to ignore out-of-order packets.
                                    */
    rtcp_stats_t *p_stats;         /*< RTCP statistics */

    uint32_t         i_items;      /*< count of SDES item structures */
    rtcp_SDES_item_t **pp_sdes;    /*< SDES client items */
} rtcp_client_t;

/**
 * structure rtcp_t
 * This structure is a VLC_OBJECT and holds RTCP statistical information.
 */
typedef struct rtcp_t
{
    VLC_COMMON_MEMBERS

    uint32_t u_clients;            /*< number of clients connected */
    uint32_t u_active;             /*< number of active senders */
    uint32_t u_members;            /*< number of clients in previous interval */

    mtime_t i_date;                 /*< current RTCP packet interval */
    mtime_t i_last_date;            /*< previous RTCP packet interval */
    mtime_t i_next_date;            /*< next scheduled transmision time
                                        of RTCP packet */

    uint32_t i_clients;            /*< count of clients structures */
    rtcp_client_t **pp_clients;    /*< RTCP clients */

    /* bitstream data pointer used for decoding */
    bs_t    *bs;                   /*< bitstream decoding data pointer */

    /* functions */
    int (*pf_add_client)( vlc_object_t *p_this, uint32_t u_ssrc, uint32_t *i_pos );
    int (*pf_del_client)( vlc_object_t *p_this, uint32_t u_ssrc );
    int (*pf_find_client)( vlc_object_t *p_this, uint32_t u_ssrc, uint32_t *i_pos );
    int (*pf_cleanup_clients)( vlc_object_t *p_this );
    int (*pf_destroy_clients)( vlc_object_t *p_this );
} rtcp_t;

int rtcp_add_client( vlc_object_t *p_this, uint32_t u_ssrc, uint32_t *i_pos );
int rtcp_del_client( vlc_object_t *p_this, uint32_t u_ssrc );
/* Should be called with vlc_mutex_lock( &p_this->objec_lock ) held */
int rtcp_find_client( vlc_object_t *p_this, uint32_t u_ssrc, uint32_t *i_pos );
int rtcp_cleanup_clients( vlc_object_t *p_this );
int rtcp_destroy_clients( vlc_object_t *p_this );

/**
 * rtcp_cleanup_clients - Permanently remove clients from the list
 * Permanently delete struct rtcp_client_t from member list when it is there
 * time to be removed. RFC 3550 recommends a grace period of five times the
 * RTCP packet send/receive interval before permanent removal of session. This
 * is to ignore out of order packets for the same SSRC that arrive after the
 * BYE report for that SSRC has been received.
 * Arguments:
 * \param p_this    VLC_OBJECT_T of type rtcp_t
 */
int rtcp_cleanup_clients( vlc_object_t *p_this );
//VLC_EXPORT( int, rtcp_cleanup_clients, ( vlc_object_t * ) );

/**
 * rtcp_pkt_decode - Decode RTCP packet
 * Decode incoming RTCP packet and inspect the records types.
 * Arguments:
 * \param p_this
 * \param p_pkt
 * \param p_block
 */
int rtcp_pkt_decode( vlc_object_t *p_this, rtcp_pkt_t *p_pkt, block_t *p_block );
//VLC_EXPORT( int, rtcp_decode, ( rtcp_pkt_t *, block_t * ) );

/**
 * rtcp_pkt_new - Encode RTCP packet
 * Create a new RTCP packet of type 'type'
 * Arguments:
 * \param type  type of RTCP packet @see
 */
rtcp_pkt_t *rtcp_pkt_new( vlc_object_t *p_this, int type );
//VLC_EXPORT( block_t* , rtcp_encode, ( vlc_object_t *, int ) );
void rtcp_pkt_del( vlc_object_t *p_this, rtcp_pkt_t *pkt );

block_t *rtcp_encode_SR( vlc_object_t *p_this, rtcp_pkt_t *p_pkt );
block_t *rtcp_encode_RR( vlc_object_t *p_this, rtcp_pkt_t *p_pkt );
block_t *rtcp_encode_SDES( vlc_object_t *p_this, rtcp_pkt_t *p_pkt );
block_t *rtcp_encode_BYE( vlc_object_t *p_this, rtcp_pkt_t *p_pkt, char *psz_reason );

/**
 * rtcp_interval
 * Calculate the interval at which RTCP compound packets are going to be
 * sent or received.
 * Arguments:
 * \param p_this        VLC_OBJECT of type rtcp_t
 * \param u_bandwith    bandwidth of RTP connection
 * \param u_ssrc        client to sent or receive from
 * \param b_sender      are we the sender or the receiver
 * \param b_first       the first time this function is called use only half
 *                      of the initial waiting time
 */
uint64_t rtcp_interval( vlc_object_t *p_this, uint64_t u_bandwidth, uint32_t u_ssrc,
                        vlc_bool_t b_sender, vlc_bool_t b_first );

/**
 * rtcp_expire
 * Decides to sent an RTCP report or a BYE record
 * Arguments:
 * \param p_this        VLC_OBJECT of type rtcp_t
 * \param u_bandwith    bandwidth of RTP connection
 * \param u_ssrc        client to sent or receive from
 * \param rtcp_event    type of event received
 * \param b_sender      are we the sender or the receiver
 * \param *b_first      the first time this function is called use only half
 *                      of the initial waiting time. If b_first is VLC_TRUE, then
 *                      it will return *b_first = VLC_FALSE;
 */
void rtcp_expire( vlc_object_t *p_this, rtcp_event_t rtcp_event, uint64_t u_bandwidth,
                  uint32_t u_ssrc, vlc_bool_t b_sender, vlc_bool_t *b_first );

/**
 * rtcp_received
 * Determine what to do on the received Sender Report, decode it
 * or leave the channel (BYE record).
 * Arguments:
 * \param p_this        VLC_OBJECT of type rtcp_t
 * \param p_pkt         RTCP packet that was received
 * \param rtcp_event    type of event received
 */
void rtcp_received( vlc_object_t *p_this, rtcp_pkt_t *pkt,
                    rtcp_event_t rtcp_event );

#endif /* RTCP_H */
