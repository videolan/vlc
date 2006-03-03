/*****************************************************************************
 * rtcp.h: RTP/RTCP headerfile
 *****************************************************************************
 * Copyright (C) 2005 M2X
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

/* RTCP Info */
#define RTCP_INFO_CNAME 1
#define RTCP_INFO_NAME  2
#define RTCP_INFO_EMAIL 3
#define RTCP_INFO_PHONE 4
#define RTCP_INFO_LOC   5
#define RTCP_INFO_TOOL  6
#define RTCP_INFO_NOTE  7
#define RTCP_INFO_PRIV  8
/* End of RTCP Info */

#define RTCP_HEADER_LEN 3

typedef struct
{
    unsigned int u_ssrc;
    unsigned int ntp_timestampH;
    unsigned int ntp_timestampL;
    unsigned int rtp_timestamp;
    unsigned int u_pkt_count;
    unsigned int u_octet_count;
} rtcp_SR;

typedef struct _RTCP_header_RR
{
    unsigned int u_ssrc;
} rtcp_RR;

typedef struct
{
    unsigned long u_ssrc;
    unsigned char u_fract_lost;
    unsigned char u_pck_lost[3];
    unsigned int  u_highest_seq_no;
    unsigned int  u_jitter;
    unsigned int  u_last_SR;
    unsigned int  u_delay_last_SR;
} rtcp_report_block_SR;

typedef struct
{
    unsigned int  u_ssrc;
    unsigned char u_attr_name;
    unsigned char u_length;
    char          *p_data;
} rtcp_SDES;

typedef struct
{
    unsigned int  u_ssrc;
    unsigned char u_length;
} rtcp_BYE;

typedef struct
{
    unsigned char u_length;
    char          *p_data;
} rtcp_APP;

/**
 * structure rtcp_stats_t
 */
typedef struct
{
    unsigned int  u_RR_received;   /*< RR records received */
    unsigned int  u_SR_received;   /*< SR records received */
    unsigned long l_dest_SSRC;     /*< SSRC */
    unsigned int  u_pkt_count;     /*< count of packets received */
    unsigned int  u_octet_count;   /*< ? */
    unsigned int  u_pkt_lost;      /*< count of packets lost */
    unsigned int  u_fract_lost;    /*< count of fractional packets lost */
    unsigned int  u_highest_seq_no;/*< highest sequence number found */
    unsigned int  u_jitter;        /*< jitter calculated */
    unsigned int  u_last_SR;       /*< last SR record received */
    unsigned int  u_last_RR;       /*< last RR record received */
    mtime_t u_delay_since_last_SR; /*< delay since last SR record received */
    mtime_t u_delay_since_last_RR; /*< delay since last RR record received */
} rtcp_stats_t;

typedef struct {
    int fd;                 /*< socket descriptor of rtcp stream */

    unsigned int u_count;        /*< packet count */
    unsigned int u_version;      /*< RTCP version number */
    unsigned int u_length;       /*< lenght of packet */
    unsigned int u_payload_type; /*< type of RTCP payload */
    rtcp_stats_t stats;          /*< RTCP statistics */

    union {
        rtcp_SR sr;         /*< SR record */
        rtcp_RR rr;         /*< RR record */
        rtcp_SDES sdes;     /*< SDES record */
        rtcp_BYE bye;       /*< BYE record */
        rtcp_APP app;       /*< APP record */
    } report;

    /*int (*pf_connect)( void *p_userdata, char *p_server, int i_port );
    int (*pf_disconnect)( void *p_userdata );
    int (*pf_read)( void *p_userdata, uint8_t *p_buffer, int i_buffer );
    int (*pf_write)( void *p_userdata, uint8_t *p_buffer, int i_buffer );*/
} rtcp_t;


/**
 * Decode RTCP packet
 * Decode incoming RTCP packet and inspect the records types.
 */
int rtcp_decode( vlc_object_t *p_this, rtcp_t *p_rtcp, block_t *p_block );
//VLC_EXPORT( int, rtcp_decode, ( rtcp_t *p_rtcp, block_t *p_block ) );
block_t *rtcp_encode( vlc_object_t *p_this, int type );

#endif /* RTCP_H */
