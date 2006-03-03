/*****************************************************************************
 * rtcp.c: RTP/RTCP source file
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

#include <netinet/in.h>
#include <sys/time.h>
#include <sys/time.h>

#include <vlc/vlc.h>
#include <vlc_bits.h>
#include <vlc_block.h>

#include "rtp.h"
#include "rtcp.h"

static int rtcp_decode_SR( vlc_object_t *p_this, rtcp_t *p_rtcp, uint8_t *p_block );
static int rtcp_decode_RR( vlc_object_t *p_this, rtcp_t *p_rtcp, uint8_t *p_buffer );
static int rtcp_decode_SR( vlc_object_t *p_this, rtcp_t *p_rtcp, uint8_t *p_buffer );
static int rtcp_decode_SDES( vlc_object_t *p_this, rtcp_t *p_rtcp, uint8_t *p_buffer );
static int rtcp_decode_BYE( vlc_object_t *p_this, rtcp_t *p_rtcp, uint8_t *p_buffer );
static int rtcp_decode_APP( vlc_object_t *p_this, rtcp_t *p_rtcp, uint8_t *p_buffer );

static block_t *rtcp_encode_SR( vlc_object_t *p_this, rtcp_t *p_rtcp );
static block_t *rtcp_encode_RR( vlc_object_t *p_this, rtcp_t *p_rtcp );
static block_t *rtcp_encode_SDES( vlc_object_t *p_this, rtcp_t *p_rtcp );
static block_t *rtcp_encode_BYE( vlc_object_t *p_this, rtcp_t *p_rtcp );

static int rtcp_decode_SR( vlc_object_t *p_this, rtcp_t *p_rtcp, uint8_t *p_buffer )
{
    unsigned int u_ssrc_count;
    unsigned int i = 0;

    if( !p_rtcp && !p_buffer )
        return VLC_EGENERIC;

    msg_Dbg( p_this, "decoding record: SR" );
    p_rtcp->stats.u_SR_received++;
    p_rtcp->stats.u_pkt_count   = p_buffer[20+RTCP_HEADER_LEN];
    p_rtcp->stats.u_octet_count = p_buffer[24+RTCP_HEADER_LEN];
    u_ssrc_count = p_buffer[RTCP_HEADER_LEN] & 0x1f;
    msg_Dbg( p_this, "SR received %d, packet count %d, octect count %d, SSRC count %d",
        p_rtcp->stats.u_SR_received,
        p_rtcp->stats.u_pkt_count,
        p_rtcp->stats.u_octet_count,
        u_ssrc_count );

    for( i=0; i < u_ssrc_count; i++ )
    {
        unsigned char count[4];

        p_rtcp->stats.u_fract_lost = p_buffer[32+RTCP_HEADER_LEN];

        count[0] = 0;
        count[1] = p_buffer[33+RTCP_HEADER_LEN];
        count[2] = p_buffer[34+RTCP_HEADER_LEN];
        count[3] = p_buffer[35+RTCP_HEADER_LEN];

        /* FIXME: I don't like the sight of this */
        p_rtcp->stats.u_pkt_lost = ntohl((int)count);

        p_rtcp->stats.u_highest_seq_no = ntohl( p_buffer[36+RTCP_HEADER_LEN] );
        p_rtcp->stats.u_jitter  = ntohl( p_buffer[40+RTCP_HEADER_LEN] );
        p_rtcp->stats.u_last_SR = ntohl( p_buffer[44+RTCP_HEADER_LEN] );
        p_rtcp->stats.u_delay_since_last_SR = (mtime_t) ntohl( p_buffer[48+RTCP_HEADER_LEN] );

        msg_Dbg( p_this, "fract lost %d, packet lost %d, highest seqno %d, jitter %d, last SR %d, delay %lld",
            p_rtcp->stats.u_fract_lost,
            p_rtcp->stats.u_pkt_lost,
            p_rtcp->stats.u_highest_seq_no,
            p_rtcp->stats.u_jitter,
            p_rtcp->stats.u_last_SR,
            p_rtcp->stats.u_delay_since_last_SR );
    }
    return VLC_SUCCESS;
}

static int rtcp_decode_RR( vlc_object_t *p_this, rtcp_t *p_rtcp, uint8_t *p_buffer )
{
    unsigned int u_ssrc_count;
    unsigned int i = 0;

    if( !p_rtcp && !p_buffer )
        return VLC_EGENERIC;

    msg_Dbg( p_this, "decoding record: RR" );

    p_rtcp->stats.u_RR_received++;
    u_ssrc_count = (p_buffer[RTCP_HEADER_LEN] & 0x1f);
    msg_Dbg( p_this, "RR received %d, SSRC count %d", p_rtcp->stats.u_RR_received, u_ssrc_count );

    for( i=0; i < u_ssrc_count; i++ )
    {
        unsigned char count[4];

        p_rtcp->stats.u_fract_lost = p_buffer[12+RTCP_HEADER_LEN];

        count[0] = 0;
        count[1] = p_buffer[13+RTCP_HEADER_LEN];
        count[2] = p_buffer[14+RTCP_HEADER_LEN];
        count[3] = p_buffer[15+RTCP_HEADER_LEN];

        /* FIXME: I don't like the sight of this */
        p_rtcp->stats.u_pkt_lost = ntohl((int)count);

        p_rtcp->stats.u_highest_seq_no = ntohl( p_buffer[16+RTCP_HEADER_LEN] );
        p_rtcp->stats.u_jitter  = ntohl( p_buffer[20+RTCP_HEADER_LEN] );
        p_rtcp->stats.u_last_RR = ntohl( p_buffer[24+RTCP_HEADER_LEN] );
        p_rtcp->stats.u_delay_since_last_RR = (mtime_t) ntohl( p_buffer[28+RTCP_HEADER_LEN] );

        msg_Dbg( p_this, "fract lost %d, packet lost %d, highest seqno %d, jitter %d, last RR %d, delay %lld",
            p_rtcp->stats.u_fract_lost,
            p_rtcp->stats.u_pkt_lost,
            p_rtcp->stats.u_highest_seq_no,
            p_rtcp->stats.u_jitter,
            p_rtcp->stats.u_last_RR,
            p_rtcp->stats.u_delay_since_last_RR );
    }
    return VLC_SUCCESS;
}

static int rtcp_decode_SDES( vlc_object_t *p_this, rtcp_t *p_rtcp, uint8_t *p_buffer )
{
    if( !p_rtcp && !p_buffer )
        return VLC_EGENERIC;

    msg_Dbg( p_this, "decoding record: SDES" );

    switch( p_buffer[8] )
    {
        case RTCP_INFO_CNAME:
            p_rtcp->stats.l_dest_SSRC = ntohs( (int)(p_buffer[4+RTCP_HEADER_LEN]) );
            break;
        case RTCP_INFO_NAME:
        case RTCP_INFO_EMAIL:
        case RTCP_INFO_PHONE:
        case RTCP_INFO_LOC:
        case RTCP_INFO_TOOL:
        case RTCP_INFO_NOTE:
        case RTCP_INFO_PRIV: /* ignoring these */
            break;
        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int rtcp_decode_BYE( vlc_object_t *p_this, rtcp_t *p_rtcp, uint8_t *p_buffer )
{
    if( !p_rtcp && !p_buffer )
        return VLC_EGENERIC;

    msg_Dbg( p_this, "decoding record: BYE" );
    return VLC_SUCCESS;
}

static int rtcp_decode_APP( vlc_object_t *p_this, rtcp_t *p_rtcp, uint8_t *p_buffer )
{
    if( !p_rtcp && !p_buffer )
        return VLC_EGENERIC;

    msg_Dbg( p_this, "decoding record: APP" );
    /* Just ignore this packet */
    return VLC_SUCCESS;
}

/* Decode RTCP packet
 * Decode incoming RTCP packet and inspect the records types.
 */
int rtcp_decode( vlc_object_t *p_this, rtcp_t *p_rtcp, block_t *p_block )
{
    uint8_t *p_buffer = NULL;
    unsigned int i_length = 0;
    unsigned int i;

    if( !p_rtcp && !p_block )
        return VLC_EGENERIC;

    i_length = p_block->i_buffer;
    p_buffer = p_block->p_buffer;

    for( i=0; i<i_length; ++i )
    {
        p_rtcp->u_count = p_buffer[i] & 0xF8;
        p_rtcp->u_version = p_buffer[i] & 0x03;
        p_rtcp->u_payload_type = p_buffer[i+1];
        p_rtcp->u_length = (p_buffer[i+2]<<8) + p_buffer[i+3];
        msg_Dbg( p_this, "New RTCP packet: count %d, version %d, type %d, lenght %d",
            p_rtcp->u_count,
            p_rtcp->u_version,
            p_rtcp->u_payload_type,
            p_rtcp->u_length );

        switch( p_rtcp->u_payload_type )
        {
            case RTCP_SR:
                rtcp_decode_SR( p_this, p_rtcp, p_buffer );
                break;
            case RTCP_RR:
                rtcp_decode_RR( p_this, p_rtcp, p_buffer );
                break;
            case RTCP_SDES:
                rtcp_decode_SDES( p_this, p_rtcp, p_buffer );
                break;
            case RTCP_BYE:
                rtcp_decode_BYE( p_this, p_rtcp, p_buffer );
                break;
            case RTCP_APP:
                rtcp_decode_APP( p_this, p_rtcp, p_buffer );
                break;
            default:
                return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
}

/*
 * Create RTCP records for reporting to server.
 */
block_t *rtcp_encode( vlc_object_t *p_this, int type )
{
    rtcp_t  *p_rtcp = NULL;
    block_t *p_block = NULL;

    p_rtcp = (rtcp_t *) malloc( sizeof( rtcp_t ) );
    if( !p_rtcp )
        return NULL;
    memset( p_rtcp, 0 , sizeof( rtcp_t ) );
    p_rtcp->u_version = 2;
    p_rtcp->u_payload_type = type;
    p_rtcp->u_length = RTCP_HEADER_LEN;

    switch( type )
    {
        case RTCP_SR: p_rtcp->u_length += sizeof(rtcp_SR); break;
        case RTCP_RR: p_rtcp->u_length += sizeof(rtcp_RR); break;
        case RTCP_SDES:
            p_rtcp->u_length += sizeof(rtcp_SDES) + strlen(p_rtcp->report.sdes.p_data);
            break;
        case RTCP_BYE:  p_rtcp->u_length += sizeof(rtcp_BYE);  break;
        case RTCP_APP:  /* ignore: p_rtcp->u_length += sizeof(rtcp_APP); */ break;
    }

    /* FIXME: Maybe this should be a buffer pool instead */
    p_block = block_New( p_this, p_rtcp->u_length );
    return p_block;
}

static block_t *rtcp_encode_SR( vlc_object_t *p_this, rtcp_t *p_rtcp )
{
    mtime_t ntp_time;
    bs_t bits, *s = &bits;
    block_t *p_block = NULL;

    /* FIXME: Maybe this should be a buffer pool instead */
    p_block = block_New( p_this, p_rtcp->u_length );
    if( !p_block )
        return NULL;

    bs_init( s, p_block->p_buffer, p_block->i_buffer );

    /* Fill in header */
    bs_write( s, 5, p_rtcp->u_count );
    bs_write( s, 1, 0 ); /* padding */
    bs_write( s, 2, p_rtcp->u_version );
    bs_write( s, 8, p_rtcp->u_payload_type );
    bs_write( s, 16, p_rtcp->u_length );

    /* fill in record */
    bs_write( s, 32, htonl( p_rtcp->report.sr.u_ssrc ) );

    ntp_time = mdate();
    bs_write( s, 32, htonl( ((unsigned int)(ntp_time>>32)) ) ); /* ntp_timestampH */
    bs_write( s, 32, htonl( ((unsigned int)ntp_time)) );/* ntp_timestampL */

    /* FIXME: Make sure to generate a good RTP server timestamp.
        p_rtcp->report.sr.rtp_timestamp = htonl(
        (unsigned int) ((double)ntp_time.tv_sec +
        (double)ntp_time.tv_usec/1000000.) * p_mux->rate
        + session->start_rtptime ); */
    bs_write( s, 32, htonl( p_rtcp->report.sr.rtp_timestamp ) );
    bs_write( s, 32, htonl( p_rtcp->report.sr.u_pkt_count ) );
    bs_write( s, 32, htonl( p_rtcp->report.sr.u_octet_count ) );

    return p_block;
}

static block_t *rtcp_encode_RR( vlc_object_t *p_this, rtcp_t *p_rtcp )
{
    bs_t bits, *s = &bits;
    block_t *p_block = NULL;

    /* FIXME: Maybe this should be a buffer pool instead */
    p_block = block_New( p_this, p_rtcp->u_length );
    if( !p_block )
        return NULL;

    bs_init( s, p_block->p_buffer, p_block->i_buffer );

    /* Fill in header */
    bs_write( s, 5, p_rtcp->u_count );
    bs_write( s, 1, 0 ); /* padding */
    bs_write( s, 2, p_rtcp->u_version );
    bs_write( s, 8, p_rtcp->u_payload_type );
    bs_write( s, 16, (p_rtcp->u_length >> 2) -1 );

    /* fill in record */
    bs_write( s, 32, htonl( p_rtcp->report.rr.u_ssrc ) );

    return p_block;
}

static block_t *rtcp_encode_SDES( vlc_object_t *p_this, rtcp_t *p_rtcp )
{
    bs_t bits, *s = &bits;
    block_t *p_block = NULL;
    char *p_hostname = strdup("hostname");
    int i_length;
    int i;

    /* FIXME: Maybe this should be a buffer pool instead */
    p_block = block_New( p_this, p_rtcp->u_length );
    if( !p_block )
        return NULL;

    bs_init( s, p_block->p_buffer, p_block->i_buffer );

    /* Fill in header */
    bs_write( s, 5, p_rtcp->u_count );
    bs_write( s, 1, 0 ); /* padding */
    bs_write( s, 2, p_rtcp->u_version );
    bs_write( s, 8, p_rtcp->u_payload_type );
    bs_write( s, 16, (p_rtcp->u_length >> 2) -1 );

    /* fill in record */
    bs_write( s, 32,htonl( p_rtcp->report.sdes.u_ssrc ) );
    bs_write( s, 8, htonl( p_rtcp->report.sdes.u_attr_name ) );
    bs_write( s, 8, htonl( p_rtcp->report.sdes.u_length ) );

    i_length = strlen(p_hostname);
    bs_write( s, 8, i_length );
    for( i=0; i<i_length; i++ )
    {
        bs_write( s, 8, p_hostname[i] );
    }
    free(p_hostname);
    return p_block;
}

static block_t *rtcp_encode_BYE( vlc_object_t *p_this, rtcp_t *p_rtcp )
{
    bs_t bits, *s = &bits;
    block_t *p_block = NULL;
    char *p_reason = strdup( "Stream ended." );
    int i_length;
    int i;

    /* FIXME: Maybe this should be a buffer pool instead */
    p_block = block_New( p_this, p_rtcp->u_length );
    if( !p_block )
        return NULL;

    bs_init( s, p_block->p_buffer, p_block->i_buffer );

    /* Fill in header */
    bs_write( s, 5, p_rtcp->u_count );
    bs_write( s, 1, 0 ); /* padding */
    bs_write( s, 2, p_rtcp->u_version );
    bs_write( s, 8, p_rtcp->u_payload_type );
    bs_write( s, 16, (p_rtcp->u_length >> 2) -1 );

    /* fill in record */
    bs_write( s, 32, htonl( p_rtcp->report.sdes.u_ssrc ) );

    i_length = strlen(p_reason);
    bs_write( s, 8, i_length );
    for( i=0; i<i_length; i++ )
    {
        bs_write( s, 8, p_reason[i] );
    }
    free(p_reason);
    return p_block;
}
