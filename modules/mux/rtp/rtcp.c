/*****************************************************************************
 * rtcp.c: RTP/RTCP source file
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

#include <netinet/in.h>
#include <sys/time.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <vlc_bits.h>
#include <vlc_block.h>

#include "rtp.h"
#include "rtcp.h"

void send_RTCP( vlc_object_t *p_this, rtcp_event_t );
void rtcp_schedule( vlc_object_t *p_this, mtime_t, rtcp_event_t );

/* SDES support functions */
static int SDES_client_item_add( rtcp_client_t *p_client, int i_item, char *psz_name )
{
    rtcp_SDES_item_t *p_item = NULL;
    
    p_item = (rtcp_SDES_item_t *) malloc( sizeof( rtcp_SDES_item_t ) );
    if( !p_item )
        return VLC_ENOMEM;
    p_item->u_type = i_item;
    p_item->psz_data = strdup( psz_name );
    p_item->i_index = p_client->i_items + 1;;
    INSERT_ELEM( p_client->pp_sdes, p_client->i_items,
                 p_item->i_index, p_item );
    return VLC_EGENERIC;
}

static int SDES_client_item_del( rtcp_client_t *p_client )
{
    uint32_t i = 0;

    for( i=0; i < p_client->i_items; i++ )
    {
        rtcp_SDES_item_t *p_old = p_client->pp_sdes[i];
        REMOVE_ELEM( p_client->pp_sdes, p_client->i_items, i );
        p_client->i_items--;
        if( p_old->psz_data)
            free( p_old->psz_data );
        free( p_old );
    }
    return VLC_SUCCESS;
}

int rtcp_add_client( vlc_object_t *p_this, uint32_t u_ssrc, uint32_t *i_pos )
{
    rtcp_t *p_rtcp = (rtcp_t *) p_this;
    rtcp_client_t *p_client = NULL;

    vlc_mutex_lock( &p_rtcp->object_lock );
    p_client = (rtcp_client_t*) malloc( sizeof(rtcp_client_t) );
    if( !p_client )
        return VLC_ENOMEM;
    p_client->i_index = p_rtcp->i_clients + 1;
    p_client->b_deleted = false;
    *i_pos = p_client->i_index ;
    INSERT_ELEM( p_rtcp->pp_clients, p_rtcp->i_clients,
                 p_client->i_index, p_client );
    p_rtcp->i_clients++;
    p_rtcp->u_clients++;
    vlc_mutex_unlock( &p_rtcp->object_lock );
    return VLC_SUCCESS;
}

int rtcp_del_client( vlc_object_t *p_this, uint32_t u_ssrc )
{
    rtcp_t *p_rtcp = (rtcp_t *) p_this;
    uint32_t i_pos = 0;

    vlc_mutex_lock( &p_rtcp->object_lock );
    if( p_rtcp->pf_find_client( p_this, u_ssrc, &i_pos ) == VLC_SUCCESS )
    {
        rtcp_client_t *p_old = p_rtcp->pp_clients[i_pos];

        p_old->b_deleted = true;
        p_old->i_timeout = 5 * (p_rtcp->i_date - p_rtcp->i_last_date) +
                           p_rtcp->i_next_date;
        p_rtcp->u_clients--;
        /* BYE message is sent by rtcp_destroy_client() */
    }
    vlc_mutex_unlock( &p_rtcp->object_lock );
    return VLC_SUCCESS;
}

/* rtcp_cleanup_clients should not be called too often */
int rtcp_cleanup_clients( vlc_object_t *p_this )
{
    rtcp_t *p_rtcp = (rtcp_t *) p_this;
    uint32_t i = 0;

    vlc_mutex_lock( &p_rtcp->object_lock );
    for( i=0; i < p_rtcp->i_clients; i++ )
    {
        rtcp_client_t *p_old = p_rtcp->pp_clients[i];

        if( p_old->b_deleted &&
           (p_old->i_timeout > mdate()) )
        {
            REMOVE_ELEM( p_rtcp->pp_clients, p_rtcp->i_clients, i );
            p_rtcp->i_clients--;
            SDES_client_item_del( p_old );
            free( p_old );
        }
    }
    vlc_mutex_unlock( &p_rtcp->object_lock );
    return VLC_SUCCESS;
}

/* Close communication with clients and release allocated objects */
int rtcp_destroy_clients( vlc_object_t *p_this )
{
    rtcp_t *p_rtcp = (rtcp_t *) p_this;
    uint32_t i = 0;

    for( i=0; i < p_rtcp->i_clients; i++ )
    {
        rtcp_pkt_t *pkt = NULL;
        rtcp_client_t *p_old = p_rtcp->pp_clients[i];

        p_rtcp->pf_del_client( p_this, p_old->u_ssrc );
    pkt = rtcp_pkt_new( p_this, RTCP_BYE );
    if( pkt )
    {
        block_t *p_block = NULL;
        p_block = rtcp_encode_BYE( p_this, pkt, strdup("server is leaving") );
            /* FIXME:
             * if( p_block )
         *    send_RTCP( p_this, p_block );
             */
    }
            
    }
    /* wait till all clients have been signalled */
    while( p_rtcp->i_clients != 0 )
    {
        p_rtcp->pf_cleanup_clients( p_this );
        msleep( 500 );
    }
    return VLC_SUCCESS;
}

/*  rtcp_find_client should be called with the object lock held.
 *  vlc_mutex_lock( &p_rtcp->obj_lock );
 */
int rtcp_find_client( vlc_object_t *p_this, uint32_t u_ssrc, uint32_t *i_pos )
{
    rtcp_t *p_rtcp = (rtcp_t *) p_this;
    uint32_t i = 0;

    for( i=0; i < p_rtcp->i_clients; i++ )
    {
        if( p_rtcp->pp_clients[i]->u_ssrc == u_ssrc )
        {
            *i_pos = i;
            return VLC_SUCCESS;
        }
    }
    *i_pos = -1;
    return VLC_EGENERIC;
}

/*--------------------------------------------------------------------------
 * rtcp_interval - Calculate the interval in seconds for sending RTCP packets.
 *--------------------------------------------------------------------------
 */
uint64_t rtcp_interval( vlc_object_t *p_this, uint64_t u_bandwidth, uint32_t u_ssrc,
                        bool b_sender, bool b_first )
{
    rtcp_t *p_rtcp = (rtcp_t *) p_this;
    rtcp_client_t *p_client = NULL;
    uint32_t i_rtcp_min = 5; /* seconds */
    uint32_t i_pos = 0;
    double i_bandwidth = u_bandwidth;
    const double i_compensation = 2.71828 - 1.5;
    double i_interval = 0;
    int n = p_rtcp->i_clients;

    if( b_first )
        i_rtcp_min = (i_rtcp_min >> 1);

    if( (double)(p_rtcp->u_active) <= (double)(p_rtcp->u_clients * 0.25) )
    {
        if( b_sender )
        {
            i_bandwidth = i_bandwidth * 0.25;
            n = p_rtcp->u_active;
        }
        else
        {
            i_bandwidth = i_bandwidth * ( 1 - 0.25 );
            n = n - p_rtcp->u_active;
        }
    }
    /* calculate average time between reports */
    p_client = p_rtcp->pf_find_client( p_this, u_ssrc, &i_pos );
    if( !p_client )
        return -1;
        
    i_interval = p_client->p_stats->u_avg_pkt_size * ( n / i_bandwidth );
    if( i_interval < i_rtcp_min )
        i_interval = i_rtcp_min;
    i_interval = i_interval * ( drand48() + 0.5 );
    i_interval = (double) (i_interval / i_compensation);

    return (uint64_t)i_interval;
}

/*--------------------------------------------------------------------------
 * rtcp_expire - decides to sent an RTCP report or a BYE record
 *--------------------------------------------------------------------------
 */
void rtcp_expire( vlc_object_t *p_this, rtcp_event_t rtcp_event, uint64_t u_bandwidth,
          uint32_t u_ssrc, bool b_sender, bool *b_first )
{
    rtcp_t *p_rtcp = (rtcp_t *) p_this;
    rtcp_client_t *p_client = NULL;
    rtcp_stats_t *p_stats = NULL;
    mtime_t i_interval = 0;
    uint32_t i_pos = 0;

    p_client = p_rtcp->pf_find_client( p_this, u_ssrc, &i_pos );
    if( !p_client )
        return;
    p_stats = (rtcp_stats_t*) p_client->p_stats;
    i_interval = (mtime_t) rtcp_interval( p_this, u_bandwidth,
                                          u_ssrc, b_sender, *b_first );
    p_rtcp->i_next_date = p_rtcp->i_last_date + i_interval;

    switch( rtcp_event )
    {
        case EVENT_BYE:
            if( p_rtcp->i_next_date <= p_rtcp->i_date )
                send_RTCP( p_this, rtcp_event );
            else
                rtcp_schedule( p_this, p_rtcp->i_next_date, rtcp_event );
            break;

        case EVENT_REPORT:
            if( p_rtcp->i_next_date <= p_rtcp->i_date )
            {
                send_RTCP( p_this, rtcp_event );

                /* Magic numbers are from RFC 3550 page 92
                 * 1.0/16.0 = 0.0625 and
                 * 15.0/16.0 = 0.9375
                 */
                p_stats->u_avg_pkt_size = (uint64_t)
                      ( (double) ( (double)p_stats->u_sent_pkt_size / ((double)0.0625) ) +
                      ( ((double)0.9357) * p_stats->u_avg_pkt_size ) );

                /* recalculate */
                p_rtcp->i_last_date = p_rtcp->i_date;
                i_interval = rtcp_interval( p_this, u_bandwidth,
                                            u_ssrc, b_sender, *b_first );
                rtcp_schedule( p_this, p_rtcp->i_next_date + i_interval, rtcp_event );
                *b_first = false;
            }
            else
            {
                rtcp_schedule( p_this, p_rtcp->i_next_date, rtcp_event );
            }
            break;
    }
    p_rtcp->i_date = p_rtcp->i_next_date;
}

/*--------------------------------------------------------------------------
 * Local functions prototoypes
 *--------------------------------------------------------------------------
 */
static int rtcp_decode_SR( vlc_object_t *p_this, rtcp_pkt_t *p_pkt );
static int rtcp_decode_RR( vlc_object_t *p_this, rtcp_pkt_t *p_pkt );
static int rtcp_decode_SR( vlc_object_t *p_this, rtcp_pkt_t *p_pkt );
static int rtcp_decode_SDES( vlc_object_t *p_this, rtcp_pkt_t *p_pkt );
static int rtcp_decode_BYE( vlc_object_t *p_this, rtcp_pkt_t *p_pkt );
static int rtcp_decode_APP( vlc_object_t *p_this, rtcp_pkt_t *p_pkt );

/*--------------------------------------------------------------------------
 * Local functions
 *--------------------------------------------------------------------------
 */
static int rtcp_decode_SR( vlc_object_t *p_this, rtcp_pkt_t *p_pkt )
{
    rtcp_t *p_rtcp = (rtcp_t *) p_this;
    unsigned int i = 0;

    if( !p_pkt )
        return VLC_EGENERIC;

    msg_Dbg( p_this, "decoding record: SR" );

    /* parse sender info */
    p_pkt->u_payload_type = RTCP_SR;
    p_pkt->report.sr.ntp_timestampH = bs_read( p_rtcp->bs, 32 );
    p_pkt->report.sr.ntp_timestampL = bs_read( p_rtcp->bs, 32 );
    p_pkt->report.sr.rtp_timestamp  = bs_read( p_rtcp->bs, 32 );
    p_pkt->report.sr.u_pkt_count    = bs_read( p_rtcp->bs, 32 ); /*sender*/
    p_pkt->report.sr.u_octet_count  = bs_read( p_rtcp->bs, 32 ); /*sender*/

    /* parse report block */
    for( i=0; i < p_pkt->u_report; i++ )
    {
        rtcp_client_t *p_client = NULL;
        uint32_t i_pos = 0;
        uint32_t u_ssrc = 0;
        int   result = 0;

        u_ssrc = bs_read( p_rtcp->bs, 32 );

        result = p_rtcp->pf_find_client( p_this, u_ssrc, &i_pos );
        if( result == VLC_EGENERIC )
        {
            result = p_rtcp->pf_add_client( p_this, p_pkt->u_ssrc, &i_pos );
            if( result == VLC_EGENERIC )
                return VLC_ENOMEM;
        }
        vlc_mutex_lock( &p_rtcp->object_lock );
        p_client = p_rtcp->pp_clients[i_pos];

        p_client->p_stats->u_SR_received++;
        p_client->p_stats->u_pkt_count++;
        p_client->p_stats->u_octet_count++;

        msg_Dbg( p_this, "SR received %d, packet count %d, octect count %d, SSRC count %d",
            p_client->p_stats->u_SR_received,
            p_client->p_stats->u_pkt_count,
            p_client->p_stats->u_octet_count,
            p_pkt->u_ssrc );
        
        p_client->p_stats->u_fraction_lost = bs_read( p_rtcp->bs, 8 );
        p_client->p_stats->u_pkt_lost = bs_read( p_rtcp->bs, 24 );
        p_client->p_stats->u_highest_seq_no = bs_read( p_rtcp->bs, 32 );
        p_client->p_stats->u_jitter  = bs_read( p_rtcp->bs, 32 );
        p_client->p_stats->u_last_SR = bs_read( p_rtcp->bs, 32 );
        p_client->p_stats->u_delay_since_last_SR = (mtime_t) bs_read( p_rtcp->bs, 32 );

        /* Magic numbers are from RFC 3550 page 92
         * 1.0/16.0 = 0.0625 and
         * 15.0/16.0 = 0.9375
         */
        p_client->p_stats->u_avg_pkt_size = (uint64_t)
            ( (double)((double)p_client->p_stats->u_sent_pkt_size * (double)(0.0625)) +
              ((double)(0.9375) * p_client->p_stats->u_avg_pkt_size) );

        msg_Dbg( p_this, "fract lost %d, packet lost %d, highest seqno %d, "
                         "jitter %d, last SR %d, delay %lld",
            p_client->p_stats->u_fraction_lost,
            p_client->p_stats->u_pkt_lost,
            p_client->p_stats->u_highest_seq_no,
            p_client->p_stats->u_jitter,
            p_client->p_stats->u_last_SR,
            p_client->p_stats->u_delay_since_last_SR );
        p_client = NULL;
        vlc_mutex_unlock( &p_rtcp->object_lock );
    }
    return VLC_SUCCESS;
}

static int rtcp_decode_RR( vlc_object_t *p_this, rtcp_pkt_t *p_pkt )
{
    rtcp_t *p_rtcp = (rtcp_t *) p_this;
    unsigned int i = 0;

    if( !p_pkt )
        return VLC_EGENERIC;

    msg_Dbg( p_this, "decoding record: RR" );

    for( i=0; i < p_pkt->u_report; i++ )
    {
        rtcp_client_t *p_client = NULL;
        uint32_t i_pos = 0;
        uint32_t u_ssrc = 0;
        int   result = 0;

        u_ssrc = bs_read( p_rtcp->bs, 32 );

        result = p_rtcp->pf_find_client( p_this, u_ssrc, &i_pos );
        if( result == VLC_EGENERIC )
        {
            result = p_rtcp->pf_add_client( p_this, p_pkt->u_ssrc, &i_pos );
            if( result == VLC_EGENERIC )
                return VLC_ENOMEM;
        }

        vlc_mutex_lock( &p_rtcp->object_lock );
        p_client = p_rtcp->pp_clients[i_pos];

        p_client->p_stats->u_RR_received++;
        msg_Dbg( p_this, "RR received %d, SSRC %d",
                 p_client->p_stats->u_RR_received, u_ssrc );

        p_client->p_stats->u_fraction_lost = bs_read( p_rtcp->bs, 8 );
        p_client->p_stats->u_pkt_lost = bs_read( p_rtcp->bs, 24 );
        p_client->p_stats->u_highest_seq_no = bs_read( p_rtcp->bs, 32 );
        p_client->p_stats->u_jitter  = bs_read( p_rtcp->bs, 32 );
        p_client->p_stats->u_last_SR = bs_read( p_rtcp->bs, 32 );
        p_client->p_stats->u_delay_since_last_SR = (mtime_t) bs_read( p_rtcp->bs, 32 );

        /* Magic numbers are from RFC 3550 page 92
         * 1.0/16.0 = 0.0625 and
         * 15.0/16.0 = 0.9375
         */
        p_client->p_stats->u_avg_pkt_size = (uint64_t)
            ( (double)((double)p_client->p_stats->u_sent_pkt_size * (double)(0.0625)) +
              ((double)(0.9375) * p_client->p_stats->u_avg_pkt_size) );

        msg_Dbg( p_this, "fract lost %d, packet lost %d, highest seqno %d, "
                         "jitter %d, last SR %d, delay %lld",
            p_client->p_stats->u_fraction_lost,
            p_client->p_stats->u_pkt_lost,
            p_client->p_stats->u_highest_seq_no,
            p_client->p_stats->u_jitter,
            p_client->p_stats->u_last_SR,
            p_client->p_stats->u_delay_since_last_SR );
        p_client = NULL;
        vlc_mutex_unlock( &p_rtcp->object_lock );
    }
    return VLC_SUCCESS;
}

static int rtcp_decode_SDES( vlc_object_t *p_this, rtcp_pkt_t *p_pkt )
{
    rtcp_t *p_rtcp = (rtcp_t *) p_this;
    unsigned int i = 0;

    if( !p_pkt )
        return VLC_EGENERIC;

    msg_Dbg( p_this, "decoding record: SDES" );

    for( i = 0; i < p_pkt->u_report; i++ )
    {
        rtcp_client_t *p_client = NULL;
        uint32_t i_pos = 0;
        uint32_t u_ssrc = 0;
        uint8_t  u_item = 0;
        uint8_t  u_length = 0;
        int   i = 0;
        int   result = 0;

        u_ssrc = bs_read( p_rtcp->bs, 32 );

        result = p_rtcp->pf_find_client( p_this, u_ssrc, &i_pos );
        if( result == VLC_EGENERIC )
        {
            result = p_rtcp->pf_add_client( p_this, p_pkt->u_ssrc, &i_pos );
            if( result == VLC_EGENERIC )
                return VLC_ENOMEM;
        }

        vlc_mutex_lock( &p_rtcp->object_lock );
        p_client = p_rtcp->pp_clients[i_pos];

        u_item = bs_read( p_rtcp->bs, 8 );
        switch( u_item )
        {
            case RTCP_SDES_CNAME:
            case RTCP_SDES_NAME:
            case RTCP_SDES_EMAIL:
            case RTCP_SDES_PHONE:
            case RTCP_SDES_LOC:
            case RTCP_SDES_TOOL:
            case RTCP_SDES_NOTE:
            {
                char psz_name[255];

                u_length = bs_read( p_rtcp->bs, 8 );
                for( i = 0 ; i < u_length; i++ )
                {
                    psz_name[i] = bs_read( p_rtcp->bs, 8 );
                }
                SDES_client_item_add( p_client, u_item, psz_name );
            }
            break;

            case RTCP_SDES_PRIV: /* ignoring these */
            {
                uint8_t u_prefix_len = 0;
                uint8_t u_length = 0;
                char psz_prefix_name[255];
                char psz_name[255];

                u_length = bs_read( p_rtcp->bs, 8 );
                u_prefix_len = bs_read( p_rtcp->bs, 8 );
                if( u_prefix_len > 254 )
                    u_prefix_len = 254;

                for( i=0 ; i < u_prefix_len; i++ )
                {
                    psz_prefix_name[i] = bs_read( p_rtcp->bs, 8 );
                }
                psz_prefix_name[255] = '\0';
                SDES_client_item_add( p_client, u_item, psz_prefix_name );

                for( i=0 ; i < u_length; i++ )
                {
                    psz_name[i] = bs_read( p_rtcp->bs, 8 );
                }
                psz_name[255] = '\0';
                SDES_client_item_add( p_client, u_item, psz_name );
            }
            break;

            default:
                return VLC_EGENERIC;
        }
        /* Magic numbers are from RFC 3550 page 92
         * 1.0/16.0 = 0.0625 and
         * 15.0/16.0 = 0.9375
         */
        p_client->p_stats->u_avg_pkt_size = (uint64_t)
            ( (double)((double)p_client->p_stats->u_sent_pkt_size * (double)(0.0625)) +
              ((double)(0.9375) * p_client->p_stats->u_avg_pkt_size) );

        p_client = NULL;
        vlc_mutex_unlock( &p_rtcp->object_lock );
    }
    return VLC_SUCCESS;
}

static int rtcp_decode_BYE( vlc_object_t *p_this, rtcp_pkt_t *p_pkt )
{
    rtcp_t    *p_rtcp = (rtcp_t *) p_this;
    uint32_t  u_ssrc = 0;
    uint8_t   u_length = 0;
    int       i = 0;

    if( !p_pkt )
        return VLC_EGENERIC;

    msg_Dbg( p_this, "decoding record: BYE" );

    u_ssrc = bs_read( p_rtcp->bs, 32 );
    p_rtcp->pf_del_client( p_this, u_ssrc );
    u_length = p_pkt->u_length-1;
    for( i = 0 ; i < u_length; i++ )
    {
        u_ssrc = bs_read( p_rtcp->bs, 8 );
        p_rtcp->pf_del_client( p_this, u_ssrc );
    }
    return VLC_SUCCESS;
}

static int rtcp_decode_APP( vlc_object_t *p_this, rtcp_pkt_t *p_pkt )
{
    rtcp_t        *p_rtcp = (rtcp_t *) p_this;
    rtcp_client_t *p_client = NULL;
    char  psz_name[4];
    char* psz_data = NULL;
    uint32_t u_ssrc = 0;
    uint32_t i_pos = 0;
    uint32_t i = 0;
    int   result = 0;

    if( !p_pkt )
        return VLC_EGENERIC;

    msg_Dbg( p_this, "decoding record: APP" );

    u_ssrc = bs_read( p_rtcp->bs, 32 );

    result = p_rtcp->pf_find_client( p_this, u_ssrc, &i_pos );
    if( result == VLC_EGENERIC )
    {
        result = p_rtcp->pf_add_client( p_this, p_pkt->u_ssrc, &i_pos );
        if( result == VLC_EGENERIC )
            return VLC_ENOMEM;
    }

    vlc_mutex_lock( &p_rtcp->object_lock );
    p_client = p_rtcp->pp_clients[i_pos];

    for( i = 0 ; i < 4; i++ )
    {
        psz_name[i] = bs_read( p_rtcp->bs, 8 );
    }
    psz_name[4] = '\0';
    
    p_pkt->u_payload_type = RTCP_APP;
    p_pkt->report.app.psz_prefix = strdup( psz_name );
    p_pkt->report.app.u_prefix_len = 4;
    p_pkt->u_length -= 4;

    psz_data = (char *) malloc( p_pkt->u_length );
    if( !psz_data ) {
        vlc_mutex_unlock( &p_rtcp->object_lock );
        return VLC_EGENERIC;
    }

    for( i = 0; i < p_pkt->u_length; i-- )
    {
        psz_data[i] = bs_read( p_rtcp->bs, 8 );
    }
    psz_data[p_pkt->u_length] = '\0';

    p_pkt->report.app.psz_data = strdup( psz_data );
    p_pkt->report.app.u_length = p_pkt->u_length;

    p_client = NULL;
    vlc_mutex_unlock( &p_rtcp->object_lock );

    /* Just ignore this packet */
    return VLC_SUCCESS;
}

/* Decode RTCP packet
 * Decode incoming RTCP packet and inspect the records types.
 */
int rtcp_pkt_decode( vlc_object_t *p_this, rtcp_pkt_t *p_pkt, block_t *p_block )
{
    rtcp_t *p_rtcp = (rtcp_t *) p_this;

    if( !p_pkt && !p_block )
        return VLC_EGENERIC;

    bs_init( p_rtcp->bs, p_block->p_buffer, p_block->i_buffer );

    p_pkt->u_version = bs_read( p_rtcp->bs, 2 );
    p_pkt->b_padding = bs_read( p_rtcp->bs, 1 ) ? true : false;
    p_pkt->u_report  = bs_read( p_rtcp->bs, 5 );
    p_pkt->u_payload_type = bs_read( p_rtcp->bs, 8 );
    p_pkt->u_length = bs_read( p_rtcp->bs, 16 );

    if( p_pkt->u_payload_type != RTCP_SDES )
        p_pkt->u_ssrc = bs_read( p_rtcp->bs, 32 );

    msg_Dbg( p_this, "New RTCP packet: version %d, padding %s, count %d, "
                     "type %d, length %d, SSRC %d",
        p_pkt->u_version,
        p_pkt->b_padding ? "true" : "false",
        p_pkt->u_report,
        p_pkt->u_payload_type,
        p_pkt->u_length,
        p_pkt->u_ssrc );

    while( !bs_eof( p_rtcp->bs ) )
    {
        uint32_t i_pos = 0;
        
        switch( p_pkt->u_payload_type )
        {
            case RTCP_SR:
                if( p_rtcp->pf_find_client( p_this, p_pkt->u_ssrc, &i_pos ) == VLC_EGENERIC )
                    p_rtcp->pf_add_client( p_this, p_pkt->u_ssrc, &i_pos );
                rtcp_decode_SR( p_this, p_pkt );
                break;

            case RTCP_RR:
                if( p_rtcp->pf_find_client( p_this, p_pkt->u_ssrc, &i_pos ) == VLC_EGENERIC )
                    p_rtcp->pf_add_client( p_this, p_pkt->u_ssrc, &i_pos );
                rtcp_decode_RR( p_this, p_pkt );
                break;

            case RTCP_SDES:
                if( p_rtcp->pf_find_client( p_this, p_pkt->u_ssrc, &i_pos ) == VLC_EGENERIC )
                    p_rtcp->pf_add_client( p_this, p_pkt->u_ssrc, &i_pos );
                rtcp_decode_SDES( p_this, p_pkt );
                break;

            case RTCP_BYE:
                rtcp_decode_BYE( p_this, p_pkt );
#if 0
                 if( p_rtcp->pf_find_sender( p_this, pkt->u_ssrc, &i_pos ) == VLC_EGENERIC )
                     p_rtcp->pf_del_sender( p_this, p_pkt->u_ssrc );
#endif
                if( p_rtcp->pf_find_client( p_this, p_pkt->u_ssrc, &i_pos ) == VLC_EGENERIC )
                    p_rtcp->pf_del_client( p_this, p_pkt->u_ssrc );

                if( p_rtcp->u_active < p_rtcp->u_members )
                {
                    rtcp_event_t event = EVENT_BYE;
                    
                    p_rtcp->i_next_date = p_rtcp->i_date +
                                (mtime_t) ( (p_rtcp->u_active / p_rtcp->u_members) *
                                            (p_rtcp->i_next_date - p_rtcp->i_date) );
                    p_rtcp->i_last_date = p_rtcp->i_date -
                                        (mtime_t)
                                            ( (mtime_t)(p_rtcp->u_active / p_rtcp->u_members) *
                                              (p_rtcp->i_date - p_rtcp->i_last_date) );
                    /* schedule for next period */
                    rtcp_schedule( VLC_OBJECT(p_rtcp), p_rtcp->i_next_date, event );
                    p_rtcp->u_members = p_rtcp->u_active;
                }
                else p_rtcp->u_members++;
                break;

            case RTCP_APP:
                if( p_rtcp->pf_find_client( p_this, p_pkt->u_ssrc, &i_pos ) == VLC_EGENERIC )
                    p_rtcp->pf_add_client( p_this, p_pkt->u_ssrc, &i_pos );
                rtcp_decode_APP( p_this, p_pkt );
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
rtcp_pkt_t *rtcp_pkt_new( vlc_object_t *p_this, int type )
{
    rtcp_pkt_t *p_pkt = NULL;

    p_pkt = (rtcp_pkt_t *) malloc( sizeof( rtcp_pkt_t ) );
    if( !p_pkt )
        return NULL;

    memset( p_pkt, 0 , sizeof( rtcp_pkt_t ) );
    p_pkt->u_version = 2;
    p_pkt->u_payload_type = type;
    p_pkt->u_length = RTCP_HEADER_LEN;

    switch( type )
    {
        case RTCP_SR:
            p_pkt->u_length += sizeof(rtcp_SR_t);
            break;
        case RTCP_RR:
            p_pkt->u_length += sizeof(rtcp_RR_t);
            break;
        case RTCP_SDES:
            p_pkt->u_length += sizeof(rtcp_SDES_t);
            if( p_pkt->report.sdes.pp_items )
                p_pkt->u_length += p_pkt->report.sdes.u_items;
            break;
        case RTCP_BYE:
            p_pkt->u_length += sizeof(rtcp_BYE_t);
            break;
        case RTCP_APP:
            p_pkt->u_length += sizeof(rtcp_APP_t);
            break;
        default:
            free(p_pkt);
            return NULL;
    }
    return p_pkt;
}

void rtcp_pkt_del( vlc_object_t *p_this, rtcp_pkt_t *p_pkt )
{
    if( !p_pkt )
        return;

    switch( p_pkt->u_payload_type )
    {
        case RTCP_SR:
        case RTCP_RR:
            break;
        case RTCP_SDES:
            if( p_pkt->report.sdes.pp_items )
            {
                uint32_t i = 0;

                for( i = 0; i < p_pkt->report.sdes.u_items; i++ )
                {
                    rtcp_SDES_item_t *p_old =
                        p_pkt->report.sdes.pp_items[i];
                    REMOVE_ELEM( p_pkt->report.sdes.pp_items,
                                 p_pkt->report.sdes.u_items, i );
                    p_pkt->report.sdes.u_items--;
                    if( p_old->psz_data )
                        free( p_old->psz_data );
                    free( p_old );
                }
            }
            break;
        case RTCP_BYE:
            break;
        case RTCP_APP:
            if( p_pkt->report.app.psz_prefix )
                free( p_pkt->report.app.psz_prefix );
            if( p_pkt->report.app.psz_data )
                free( p_pkt->report.app.psz_data );
            break;
        default:
            msg_Err( p_this, "unknown RTCP packet type %d: "
                             "possible leaking of memory.",
                            p_pkt->u_payload_type );
            break;
    }
    free( p_pkt );
}

block_t *rtcp_encode_SR( vlc_object_t *p_this, rtcp_pkt_t *p_pkt )
{
    rtcp_t *p_rtcp = (rtcp_t *) p_this;
    mtime_t ntp_time;
    bs_t bits, *s = &bits;
    block_t *p_block = NULL;
    rtcp_stats_t *p_stats = NULL;
    rtcp_client_t *p_client = NULL;
    uint32_t i_pos = 0;
    int result = 0;

    if( p_pkt->u_payload_type != RTCP_SR )
        return NULL;

    /* FIXME: Maybe this should be a buffer pool instead */
    p_block = block_New( p_this, p_pkt->u_length );
    if( !p_block )
        return NULL;

    bs_init( s, p_block->p_buffer, p_block->i_buffer );

    /* Fill in header */
    bs_write( s, 2, p_pkt->u_version );
    bs_write( s, 1, 0 ); /* padding */
    bs_write( s, 5, p_pkt->u_report );
    bs_write( s, 8, p_pkt->u_payload_type );
    bs_write( s, 16, p_pkt->u_length );
    bs_write( s, 32, p_pkt->u_ssrc );

    /* sender info */
    ntp_time = mdate();
    bs_write( s, 32, ((unsigned int)(ntp_time>>32)) ); /* ntp_timestampH */
    bs_write( s, 32, ((unsigned int)ntp_time) );/* ntp_timestampL */

    /* FIXME: Make sure to generate a good RTP server timestamp.
        p_pkt->report.sr.rtp_timestamp = htonl(
        (unsigned int) ((double)ntp_time.tv_sec +
        (double)ntp_time.tv_usec/1000000.) * p_mux->rate
        + session->start_rtptime ); */
    bs_write( s, 32, p_pkt->report.sr.rtp_timestamp );
    bs_write( s, 32, p_pkt->report.sr.u_pkt_count );
    bs_write( s, 32, p_pkt->report.sr.u_octet_count );

    /* report block */
    result = p_rtcp->pf_find_client( p_this, p_pkt->u_ssrc, &i_pos );
    if( result == VLC_EGENERIC )
    {
        msg_Err( p_this, "SR: SSRC identifier doesn't exists", p_pkt->u_ssrc );
        free( p_block );
        return NULL;
    }

    vlc_mutex_lock( &p_rtcp->object_lock );
    p_client = p_rtcp->pp_clients[i_pos];

    p_stats = p_client->p_stats;
    if( !p_stats ) {
        msg_Err( p_this, "SR: SSRC identifier doesn't exists", p_pkt->u_ssrc );
        free( p_block );
        return NULL;
    }
    bs_write( s, 32, p_stats->l_dest_SSRC );
    bs_write( s,  8, p_stats->u_fraction_lost );
    bs_write( s, 24, p_stats->u_pkt_lost );
    bs_write( s, 32, p_stats->u_highest_seq_no );
    bs_write( s, 32, p_stats->u_jitter );
    bs_write( s, 32, p_stats->u_last_SR );
    bs_write( s, 32, p_stats->u_delay_since_last_SR );

    p_client = NULL;
    vlc_mutex_unlock( &p_rtcp->object_lock );

    /* possible SR extension */
    return p_block;
}

block_t *rtcp_encode_RR( vlc_object_t *p_this, rtcp_pkt_t *p_pkt )
{
    rtcp_t *p_rtcp = (rtcp_t *) p_this;
    bs_t bits, *s = &bits;
    block_t *p_block = NULL;
    rtcp_stats_t *p_stats = NULL;
    rtcp_client_t *p_client = NULL;
    uint32_t i_pos = 0;
    int result = 0;

    if( p_pkt->u_payload_type != RTCP_RR )
        return NULL;

    /* FIXME: Maybe this should be a buffer pool instead */
    p_block = block_New( p_this, p_pkt->u_length );
    if( !p_block )
        return NULL;

    bs_init( s, p_block->p_buffer, p_block->i_buffer );

    /* Fill in header */
    bs_write( s, 2, p_pkt->u_version );
    bs_write( s, 1, 0 ); /* padding */
    bs_write( s, 5, p_pkt->u_report );
    bs_write( s, 8, p_pkt->u_payload_type );
    bs_write( s, 16, p_pkt->u_length );
    bs_write( s, 32, p_pkt->u_ssrc );

    /* report block */
    result = p_rtcp->pf_find_client( p_this, p_pkt->u_ssrc, &i_pos );
    if( result == VLC_EGENERIC )
    {
        msg_Err( p_this, "RR: SSRC identifier doesn't exists", p_pkt->u_ssrc );
        free( p_block );
        return NULL;
    }

    vlc_mutex_lock( &p_rtcp->object_lock );
    p_client = p_rtcp->pp_clients[i_pos];

    p_stats = p_client->p_stats;
    if( !p_stats ) {
        msg_Err( p_this, "RR: SSRC identifier doesn't exists", p_pkt->u_ssrc );
        free( p_block );
        return NULL;
    }
    bs_write( s, 32, p_stats->l_dest_SSRC );
    bs_write( s,  8, p_stats->u_fraction_lost );
    bs_write( s, 24, p_stats->u_pkt_lost );
    bs_write( s, 32, p_stats->u_highest_seq_no );
    bs_write( s, 32, p_stats->u_jitter );
    bs_write( s, 32, p_stats->u_last_RR );
    bs_write( s, 32, p_stats->u_delay_since_last_RR );

    p_client = NULL;
    vlc_mutex_unlock( &p_rtcp->object_lock );

    /* possible RR extension */
    return p_block;
}

block_t *rtcp_encode_SDES( vlc_object_t *p_this, rtcp_pkt_t *p_pkt )
{
    bs_t bits, *s = &bits;
    block_t *p_block = NULL;
    uint32_t i_chunks;

    if( p_pkt->u_payload_type != RTCP_SDES )
        return NULL;

    /* FIXME: Maybe this should be a buffer pool instead */
    p_block = block_New( p_this, p_pkt->u_length );
    if( !p_block )
        return NULL;

    bs_init( s, p_block->p_buffer, p_block->i_buffer );

    /* Fill in header */
    bs_write( s, 2, p_pkt->u_version );
    bs_write( s, 1, 0 ); /* padding */
    bs_write( s, 5, p_pkt->u_report ); /* Number of SSRC/CSRC chunks */
    bs_write( s, 8, p_pkt->u_payload_type );
    bs_write( s, 16, p_pkt->u_length );
    bs_write( s, 32, p_pkt->u_ssrc );

    /* fill in record */
    for( i_chunks = 0; i_chunks < p_pkt->u_report; i_chunks++ )
    {
        uint32_t i_item;

        for( i_item = 0 ; i_item < p_pkt->report.sdes.u_items; i_item++ )
        {
            uint32_t i_count = strlen( p_pkt->report.sdes.pp_items[i_item]->psz_data );
            uint8_t  u_octet = i_count / 8;  /* Octect count ??*/
            rtcp_SDES_item_t *p_item = p_pkt->report.sdes.pp_items[i_item];
            uint32_t i_pos, i_pad, i_padding;

            bs_write( s, 8, p_item->u_type );
            bs_write( s, 8, u_octet );

            for( i_pos = 0; i_pos < i_count; i_pos++ )
            {
                /* FIXME: must be UTF 8 encoded */
                bs_write( s, 8, p_item->psz_data[i_pos] );
            }

            /* do we need padding to 32 bit boundary? */
            i_padding = 0;
            if( ((i_count + 2) % 4) != 0 )
                i_padding = (i_count + 2) - (((i_count + 2) % 4) << 2);
            for( i_pad = 0; i_pad < i_padding; i_pad++ )
            {
                bs_write( s, 8, 0 );
            }
        }
    }
    return p_block;
}

block_t *rtcp_encode_BYE( vlc_object_t *p_this, rtcp_pkt_t *p_pkt, char *psz_reason )
{
    bs_t bits, *s = &bits;
    block_t *p_block = NULL;
    uint32_t i_count = strlen( psz_reason );
    uint8_t  u_octet = i_count / 8;  /* Octect count ??*/
    int32_t i_pos, i_pad, i_padding;

    if( p_pkt->u_payload_type != RTCP_BYE )
        return NULL;

    /* FIXME: Maybe this should be a buffer pool instead */
    p_block = block_New( p_this, p_pkt->u_length );
    if( !p_block )
        return NULL;

    bs_init( s, p_block->p_buffer, p_block->i_buffer );

    /* Fill in header */
    bs_write( s, 2, p_pkt->u_version );
    bs_write( s, 1, 0 ); /* padding */
    bs_write( s, 5, p_pkt->u_report ); /* Number of SSRC/CSRC chunks */
    bs_write( s, 8, p_pkt->u_payload_type );
    bs_write( s, 16, p_pkt->u_length );
    bs_write( s, 32, p_pkt->u_ssrc );

    /* Give reason for leaving */
    //FIXME: bs_write( s, 8, p_item->u_type );
    bs_write( s, 8, u_octet );
    
    for( i_pos = 0; i_pos < i_count; i_pos++ )
    {
        /* FIXME: must be UTF 8 encoded */
        bs_write( s, 8, psz_reason[i_pos] );
    }

    /* do we need padding to 32 bit boundary? */
    i_padding = 0;
    if( ((i_count + 2) % 4) != 0 )
        i_padding = (i_count + 2) - (((i_count + 2) % 4) << 2);
    for( i_pad = 0; i_pad < i_padding; i_pad++ )
    {
        bs_write( s, 8, 0 );
    }
    return p_block;
}
