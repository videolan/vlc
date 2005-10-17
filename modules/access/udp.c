/*****************************************************************************
 * udp.c: raw UDP & RTP input module
 *****************************************************************************
 * Copyright (C) 2001-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Tristan Leteurtre <tooney@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
 *
 * Reviewed: 23 October 2003, Jean-Paul Saman <jpsaman@wxs.nl>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "network.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for UDP streams. This " \
    "value should be set in millisecond units." )

#define AUTO_MTU_TEXT N_("Autodetection of MTU")
#define AUTO_MTU_LONGTEXT N_( \
    "Allows growing the MTU if truncated packets are found" )

#define RTP_LATE_TEXT N_("Reorder timeout in ms for late RTP packets")
#define RTP_LATE_LONGTEXT N_( \
    "Allows you to modify the RTP packets reorder and late behaviour. " \
    "If enabled (value>0) then out-of-order packets will be held for the " \
    "specified timeout in ms. " \
    "The default behaviour is not to reorder." )

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_shortname( _("UDP/RTP" ) );
    set_description( _("UDP/RTP input") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );

    add_integer( "udp-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT,
                 CACHING_LONGTEXT, VLC_TRUE );
    add_bool( "udp-auto-mtu", 1, NULL,
              AUTO_MTU_TEXT, AUTO_MTU_LONGTEXT, VLC_TRUE );

    add_integer( "rtp-late", 0, NULL, RTP_LATE_TEXT, RTP_LATE_LONGTEXT, VLC_TRUE );

    set_capability( "access2", 0 );
    add_shortcut( "udp" );
    add_shortcut( "udpstream" );
    add_shortcut( "udp4" );
    add_shortcut( "udp6" );
    add_shortcut( "rtp" );
    add_shortcut( "rtp4" );
    add_shortcut( "rtp6" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#define RTP_HEADER_LEN 12
#define RTP_SEQ_NUM_SIZE 65536

static block_t *BlockUDP( access_t * );
static block_t *BlockRTP( access_t * );
static block_t *BlockChoose( access_t * );
static int Control( access_t *, int, va_list );

struct access_sys_t
{
    int fd;

    int i_mtu;
    vlc_bool_t b_auto_mtu;

    /* rtp only */
    uint16_t i_sequence_number;
    vlc_bool_t b_first_seqno;

    /* reorder rtp packets when out-of-bounds 
     * the packets hold queue is one level deep
     */
    uint32_t i_rtp_late; /* number of ms an RTP packet may be too late*/
    uint32_t i_last_pcr; /* last known good PCR */
    block_t *p_list;     /* list of packets to rearrange */
    block_t *p_end;      /* last packet in p_list */
    block_t *p_next;     /* p_next ?? */
};

/*****************************************************************************
 * Open: open the socket
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;

    char *psz_name = strdup( p_access->psz_path );
    char *psz_parser, *psz_server_addr, *psz_bind_addr = "";
    int  i_bind_port, i_server_port = 0;

    /* First set ipv4/ipv6 */
    var_Create( p_access, "ipv4", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_access, "ipv6", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    if( *p_access->psz_access )
    {
        vlc_value_t val;
        /* Find out which shortcut was used */
        if( !strncmp( p_access->psz_access, "udp4", 6 ) ||
            !strncmp( p_access->psz_access, "rtp4", 6 ))
        {
            val.b_bool = VLC_TRUE;
            var_Set( p_access, "ipv4", val );

            val.b_bool = VLC_FALSE;
            var_Set( p_access, "ipv6", val );
        }
        else if( !strncmp( p_access->psz_access, "udp6", 6 ) ||
                 !strncmp( p_access->psz_access, "rtp6", 6 ) )
        {
            val.b_bool = VLC_TRUE;
            var_Set( p_access, "ipv6", val );

            val.b_bool = VLC_FALSE;
            var_Set( p_access, "ipv4", val );
        }
    }

    i_bind_port = var_CreateGetInteger( p_access, "server-port" );

    /* Parse psz_name syntax :
     * [serveraddr[:serverport]][@[bindaddr]:[bindport]] */
    psz_parser = strchr( psz_name, '@' );
    if( psz_parser != NULL )
    {
        /* Found bind address and/or bind port */
        *psz_parser++ = '\0';
        psz_bind_addr = psz_parser;

        if( *psz_parser == '[' )
            /* skips bracket'd IPv6 address */
            psz_parser = strchr( psz_parser, ']' );

        if( psz_parser != NULL )
        {
            psz_parser = strchr( psz_parser, ':' );
            if( psz_parser != NULL )
            {
                *psz_parser++ = '\0';
                i_bind_port = atoi( psz_parser );
            }
        }
    }

    psz_server_addr = psz_name;
    if( *psz_server_addr == '[' )
        /* skips bracket'd IPv6 address */
        psz_parser = strchr( psz_name, ']' );

    if( psz_parser != NULL )
    {
        psz_parser = strchr( psz_parser, ':' );
        if( psz_parser != NULL )
        {
            *psz_parser++ = '\0';
            i_server_port = atoi( psz_parser );
        }
    }

    msg_Dbg( p_access, "opening server=%s:%d local=%s:%d",
             psz_server_addr, i_server_port, psz_bind_addr, i_bind_port );

    /* Set up p_access */
    p_access->pf_read = NULL;
    if( !strcasecmp( p_access->psz_access, "rtp" )
          || !strcasecmp( p_access->psz_access, "rtp4" )
          || !strcasecmp( p_access->psz_access, "rtp6" ) )
    {
        p_access->pf_block = BlockRTP;
    }
    else
    {
        p_access->pf_block = BlockChoose;
    }
    p_access->pf_control = Control;
    p_access->pf_seek = NULL;
    p_access->info.i_update = 0;
    p_access->info.i_size = 0;
    p_access->info.i_pos = 0;
    p_access->info.b_eof = VLC_FALSE;
    p_access->info.i_title = 0;
    p_access->info.i_seekpoint = 0;

    p_access->p_sys = p_sys = malloc( sizeof( access_sys_t ) );
    p_sys->fd = net_OpenUDP( p_access, psz_bind_addr, i_bind_port,
                                      psz_server_addr, i_server_port );
    if( p_sys->fd < 0 )
    {
        msg_Err( p_access, "cannot open socket" );
        free( psz_name );
        free( p_sys );
        return VLC_EGENERIC;
    }
    free( psz_name );

    net_StopSend( p_sys->fd );

    /* FIXME */
    p_sys->i_mtu = var_CreateGetInteger( p_access, "mtu" );
    if( p_sys->i_mtu <= 1 )
        p_sys->i_mtu  = 1500;   /* Avoid problem */

    p_sys->b_auto_mtu = var_CreateGetBool( p_access, "udp-auto-mtu" );;

    /* Update default_pts to a suitable value for udp access */
    var_Create( p_access, "udp-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    /* Keep track of RTP sequence number */
    p_sys->i_sequence_number = 0;
    p_sys->b_first_seqno = VLC_TRUE;

    /* RTP reordering out-of-bound packets */
    p_sys->i_last_pcr = 0;
    p_sys->i_rtp_late = var_CreateGetInteger( p_access, "rtp-late" );
    p_sys->p_list = NULL;
    p_sys->p_end = NULL;
    p_sys->p_next = NULL;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    block_ChainRelease( p_sys->p_list );
    net_Close( p_sys->fd );
    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    access_sys_t *p_sys = p_access->p_sys;
    vlc_bool_t   *pb_bool;
    int          *pi_int;
    int64_t      *pi_64;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_FALSE;
            break;
        /* */
        case ACCESS_GET_MTU:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = p_sys->i_mtu;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = var_GetInteger( p_access, "udp-caching" ) * 1000;
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * BlockUDP:
 *****************************************************************************/
static block_t *BlockUDP( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    block_t      *p_block;

    /* Read data */
    p_block = block_New( p_access, p_sys->i_mtu );
    p_block->i_buffer = net_Read( p_access, p_sys->fd, NULL,
                                  p_block->p_buffer, p_sys->i_mtu,
                                  VLC_FALSE );
    if( p_block->i_buffer <= 0 )
    {
        block_Release( p_block );
        return NULL;
    }

    if( p_block->i_buffer >= p_sys->i_mtu && p_sys->b_auto_mtu &&
        p_sys->i_mtu < 32767 )
    {
        /* Increase by 100% */
        p_sys->i_mtu *= 2;
        msg_Dbg( p_access, "increasing MTU to %d", p_sys->i_mtu );
    }

    return p_block;
}

/*
 * rtp_ChainInsert - insert a p_block in the chain and
 * look at the sequence numbers.
 */
static inline void rtp_ChainInsert( access_t *p_access, block_t **pp_list, block_t **pp_end, block_t *p_block )
{
    block_t *p_tmp = NULL;
    block_t *p = NULL;
    uint16_t i_new = 0;
    uint16_t i_cur = 0;
    uint16_t i_expected = 0;
    uint32_t i_pcr_new = 0;

    if( *pp_list == NULL )
    {
        *pp_list = p_block;
        *pp_end  = p_block;
        return;
    }
    /* Appending packets at the end of the chain is the normal case */
    i_pcr_new = ( (p_block->p_buffer[4] << 24) +
                  (p_block->p_buffer[5] << 16) +
                  (p_block->p_buffer[6] << 8) +
                   p_block->p_buffer[7] );
    i_new = ( (p_block->p_buffer[2] << 8 ) + p_block->p_buffer[3] );

    p = *pp_end;
    i_cur = ( (p->p_buffer[2] << 8 ) + p->p_buffer[3] );
    i_expected = ((i_cur+1) % RTP_SEQ_NUM_SIZE);
    if( (i_new - i_expected) >= 0 ) /* Append at the end? */
    {
        msg_Dbg( p_access, ">> append %p(%u)==%p(%u)\n", p_block, i_cur, p, i_new );
        p->p_next = *pp_end = p_block;
        return;
    }
    /* Add to the front fo the chain? */
    p = *pp_list;
    i_new = ( (p_block->p_buffer[2] << 8 ) + p_block->p_buffer[3] );
    i_cur = ( (p->p_buffer[2] << 8 ) + p->p_buffer[3] );
    if( (i_cur - i_new) > 0 )
    {
        msg_Dbg( p_access, ">> prepend %p(%u)==%p(%u)\n", p_block, i_cur, p, i_new );
        p_block->p_next = p;
        *pp_list = p_block;
        return;
    }
    /* The packet can't be added to the front or the end of the chain,
     * thus walk the chain from the start.
     */
    while( p )
    {
        i_cur = (p->p_buffer[2] << 8 ) + p->p_buffer[3];
        i_expected = (i_cur+1) % RTP_SEQ_NUM_SIZE;

        msg_Dbg( p_access,  "i_cur: %u, i_new: %u", i_cur, i_new);
        if( i_cur == i_new )
        {
            uint32_t i_pcr_cur = ( (p->p_buffer[4] << 24) +
                                   (p->p_buffer[5] << 16) +
                                   (p->p_buffer[6] << 8) +
                                    p->p_buffer[7] );
            /* This packet might be a duplicate, so check PCR's */
            if( i_pcr_cur >= i_pcr_new )
            {
                /* packet way too late drop it. */
                block_Release( p_block );
                return;
            }
            /* Add it to list later on
             * else if( i_pcr_cur < i_pcr_new ) */
            break;
        }
        else if( (i_expected - i_new) >= 0 ) /* insert in chain */
        {
            p_tmp = p->p_next;
            msg_Dbg( p_access, ">> insert between %p(%u)==%p(%u)", p, i_cur, p_tmp, i_new );
            p->p_next = p_block;
            p_block->p_next = p_tmp;
            return;
        }
        if( !p->p_next ) break;
        p = p->p_next;
    }
}

/*
 * rtp_ChainSend - look which packets are ready for sending.
 */
static inline block_t *rtp_ChainSend( access_t *p_access, block_t **pp_list, uint16_t i_seq )
{
    access_sys_t *p_sys = (access_sys_t *) p_access->p_sys;
    uint16_t i_cur = 0;

    if( *pp_list )
    {
        /* Parse RTP header */
        int i_skip = 0;
        int i_extension_bit = 0;
        int i_extension_length = 0;
        int i_CSRC_count = 0;
        int i_payload_type = 0;
        uint32_t i_pcr_prev = 0;
        uint16_t i_seq_prev = 0;
        /* Data pointers */
        block_t *p_prev = NULL;
        block_t *p_send = *pp_list;
        block_t *p = *pp_list;

        while( p )
        {
            i_cur = ( (p->p_buffer[2] << 8 ) + p->p_buffer[3] );
            msg_Dbg( p_access, "rtp_ChainSend: i_cur %u, i_seq %u", i_cur, i_seq );
            if( (i_cur - i_seq) == 0 )
            {
                i_seq++; /* sent all packets that are received in order */

                /* Remember PCR and sequence number of packet
                 * for next iteration */
                i_pcr_prev = ( (p->p_buffer[4] << 24) +
                               (p->p_buffer[5] << 16) +
                               (p->p_buffer[6] << 8) +
                                p->p_buffer[7] );
                i_seq_prev = ( (p->p_buffer[2] << 8 ) +
                                p->p_buffer[3] );

                /* Parse headerfields we need */
                i_CSRC_count = p->p_buffer[0] & 0x0F;
                i_payload_type = (p->p_buffer[1] & 0x7F);
                i_extension_bit  = ( p->p_buffer[0] & 0x10 ) >> 4;
                if ( i_extension_bit == 1)
                    i_extension_length = ( (p->p_buffer[14] << 8 ) +
                                            p->p_buffer[15] );

                /* Skip header + CSRC extension field n*(32 bits) + extention */
                i_skip = RTP_HEADER_LEN + 4*i_CSRC_count + i_extension_length;
                if( i_payload_type == 14 ) i_skip += 4;

                /* Return the packet without the RTP header. */
                p->i_buffer -= i_skip;
                p->p_buffer += i_skip;
            }
            else if( (i_cur - i_seq) > 0)
            {
                if( p_prev )
                {
                    *pp_list = p;
                    p_prev->p_next = NULL;
                    p_sys->i_last_pcr = i_pcr_prev;
                    p_sys->i_sequence_number = i_seq_prev;
                    return p_send;
                }
                /* FiXME: or should we return NULL here? */
                return NULL;
            }
            p_prev = p;
            if (!p->p_next) break;
            p = p->p_next;
        }
        /* We have walked through the complete chain and all packets are
         * in sequence - so send the whole chain
         */
        i_payload_type = (p->p_buffer[1] & 0x7F);
        i_CSRC_count = p->p_buffer[0] & 0x0F;
        i_extension_bit  = ( p->p_buffer[0] & 0x10 ) >> 4;
        if( i_extension_bit == 1)
            i_extension_length = ( (p->p_buffer[14] << 8 ) + p->p_buffer[15] );

        /* Skip header + CSRC extension field n*(32 bits) + extention */
        i_skip = RTP_HEADER_LEN + 4*i_CSRC_count + i_extension_length;
        if( i_payload_type == 14 ) i_skip += 4;

        /* Update the list pointers */
        *pp_list = NULL;
        p_sys->p_next = NULL;
        p_sys->p_end = NULL;
        p_sys->i_sequence_number = ( (p->p_buffer[2] << 8 ) +
                                      p->p_buffer[3] );
        p_sys->i_last_pcr = ( (p->p_buffer[4] << 24) +
                              (p->p_buffer[5] << 16) +
                              (p->p_buffer[6] << 8) +
                               p->p_buffer[7] );
        /* Return the packet without the RTP header. */
        p->i_buffer -= i_skip;
        p->p_buffer += i_skip;
        return p_send;
    }
    return NULL;
}

/*****************************************************************************
 * BlockParseRTP/BlockRTP:
 *****************************************************************************/
static block_t *BlockParseRTP( access_t *p_access, block_t *p_block )
{
    access_sys_t *p_sys = (access_sys_t *) p_access->p_sys;
    int      i_rtp_version;
    int      i_CSRC_count;
    int      i_payload_type;
    int      i_skip = 0;
    uint16_t i_sequence_number = 0;
    uint16_t i_sequence_expected = 0;
    int      i_extension_bit = 0;
    int      i_extension_length = 0;
    uint32_t i_pcr = 0;

    if( p_block == NULL )
        return NULL;

    if( p_block->i_buffer < RTP_HEADER_LEN )
    {
        msg_Warn( p_access, "received a too short packet for RTP" );
        block_Release( p_block );
        return NULL;
    }
    /* Parse the header and make some verifications.
     * See RFC 3550. */
    i_rtp_version     = ( p_block->p_buffer[0] & 0xC0 ) >> 6;
    i_CSRC_count      = p_block->p_buffer[0] & 0x0F;
    i_payload_type    = p_block->p_buffer[1] & 0x7F;
    i_sequence_number = (p_block->p_buffer[2] << 8) +
                         p_block->p_buffer[3];
    i_pcr = ( (p_block->p_buffer[4] << 24) +
              (p_block->p_buffer[5] << 16) +
              (p_block->p_buffer[6] << 8) +
               p_block->p_buffer[7] );
    i_extension_bit  = ( p_block->p_buffer[0] & 0x10 ) >> 4;

    if( i_rtp_version != 2 )
        msg_Dbg( p_access, "RTP version is %u, should be 2", i_rtp_version );

    if( i_payload_type == 14 )
        i_skip = 4;
    else if( i_payload_type !=  33 && i_payload_type != 32 )
        msg_Dbg( p_access, "unsupported RTP payload type (%u)", i_payload_type );

    if( i_extension_bit == 1)
        i_extension_length = 4 +
            4 * ( (p_block->p_buffer[14] << 8) + p_block->p_buffer[15] );

    /* Skip header + CSRC extension field n*(32 bits) + extention */
    i_skip += RTP_HEADER_LEN + 4*i_CSRC_count + i_extension_length;
    if( i_skip >= p_block->i_buffer )
    {
        msg_Warn( p_access, "received a too short packet for RTP" );
        block_Release( p_block );
        return NULL;
    }

    /* Detect RTP packet loss through tracking sequence numbers,
     * and take RTP PCR into account.
     * See RFC 3550.
     */
    if( p_sys->b_first_seqno )
    {
        p_sys->i_sequence_number = i_sequence_number-1;
        p_sys->i_last_pcr = i_pcr;
        p_sys->b_first_seqno = VLC_FALSE;
    }
#if 0
    /* Emulate packet loss */
    if ( (i_sequence_number % 4000) == 0)
    {
        msg_Warn( p_access, "Emulating packet drop" );
        block_Release( p_block );
        return NULL;
    }
#endif
    i_sequence_expected = ((p_sys->i_sequence_number + 1) % RTP_SEQ_NUM_SIZE);
    if( (i_sequence_expected - i_sequence_number) != 0 )
    {
        /* Handle out of order packets */
        if( p_sys->i_rtp_late > 0 )
        {
            if( (i_sequence_number - i_sequence_expected) > 0 )
            {
                msg_Warn( p_access,
                    "RTP packet out of order (too early) expected %u, current %u",
                    i_sequence_expected, i_sequence_number );
                if( (i_pcr - p_sys->i_last_pcr) > (p_sys->i_rtp_late*90) )
                {
                    block_t *p_start = p_sys->p_list;
                    uint16_t i_start = (p_start->p_buffer[2] << 8) +
                                        p_start->p_buffer[3];
                    /* Gap too big, we have been holding this data for too long,
                     * send what we have.
                     */
                    msg_Warn( p_access,
                        "Gap too big resyncing: delta %u, held for %d ms",
                        (i_pcr - p_sys->i_last_pcr), p_sys->i_rtp_late );
                    rtp_ChainInsert( p_access, &p_sys->p_list, &p_sys->p_end, p_block );
                    return rtp_ChainSend( p_access, &p_sys->p_list, i_start );
                }
                /* hold packets that arrive too early. */
                rtp_ChainInsert( p_access, &p_sys->p_list, &p_sys->p_end, p_block );
                return rtp_ChainSend( p_access, &p_sys->p_list, i_sequence_expected );
            }
            else if( /* ((i_sequence_expected - i_sequence_number ) > 0) && */
                     (i_pcr <= p_sys->i_last_pcr) )
            {
                msg_Warn( p_access,
                    "RTP packet out of order (duplicate or too late) expected %u, current %u .. trashing it",
                    i_sequence_expected, i_sequence_number );
                block_Release( p_block );
                p_sys->i_sequence_number = i_sequence_number;
                p_sys->i_last_pcr = i_pcr;
                return NULL;
            }

            if( p_sys->p_list )
            {
                block_t *p = NULL;
                block_t **p_send = &p_sys->p_list;

                msg_Warn( p_access,
                    "RTP packet (unexpected condition) expected %u, current %u",
                    i_sequence_expected, i_sequence_number );

                /* Append block to the end of chain and send whole chain */
                block_ChainLastAppend( &p_send, p_block );
                p_sys->p_list = p_sys->p_end = NULL;
                p_sys->i_sequence_number = i_sequence_number;
                p_sys->i_last_pcr = i_pcr;

                /* Return the packet without the RTP header. */
                p = *p_send;
                while( p )
                {
                    p->i_buffer -= i_skip;
                    p->p_buffer += i_skip;
                    if( !p->p_next ) break;
                    p = p->p_next;
                }
                return *p_send;
            }
            /* This code should never be reached !! */
            msg_Err( p_access,
                "Bug in algorithme: (unexpected condition) expected %u (pcr=%u), current %u (pcr=%u)",
                i_sequence_expected, i_sequence_number, p_sys->i_last_pcr, i_pcr );
        }
        msg_Warn( p_access,
                  "RTP packet(s) lost, expected sequence number %d got %d",
                  i_sequence_expected, i_sequence_number );

        /* Mark transport error in the first TS packet in the RTP stream. */
        if( (i_payload_type == 33) && (p_block->p_buffer[0] == 0x47) )
            p_block->p_buffer[1] |= 0x80;
    }
    else if( (p_sys->i_rtp_late > 0) && p_sys->p_list )
    {
        if( i_pcr <= p_sys->i_last_pcr )
        {
            msg_Warn( p_access,
                "RTP packet out of order (duplicate) expected %u, current %u .. trashing it",
                i_sequence_expected, i_sequence_number );
            block_Release( p_block );
            p_sys->i_sequence_number = i_sequence_number;
            p_sys->i_last_pcr = i_pcr;
            return NULL;
        }
        rtp_ChainInsert( p_access, &p_sys->p_list, &p_sys->p_end, p_block );
        return rtp_ChainSend( p_access, &p_sys->p_list, i_sequence_expected );
    }

    /* This is the normal case when no packet reordering is effective */
    p_sys->i_sequence_number = i_sequence_number;
    p_sys->i_last_pcr = i_pcr;

    /* Return the packet without the RTP header. */
    p_block->i_buffer -= i_skip;
    p_block->p_buffer += i_skip;
    return p_block;
}

static block_t *BlockRTP( access_t *p_access )
{
    block_t *p_block = BlockUDP( p_access );

    if ( p_block != NULL )
        return BlockParseRTP( p_access, p_block );
    else
        return NULL;
}

/*****************************************************************************
 * BlockChoose: decide between RTP and UDP
 *****************************************************************************/
static block_t *BlockChoose( access_t *p_access )
{
    block_t *p_block;
    int     i_rtp_version;
    int     i_CSRC_count;
    int     i_payload_type;

    if( ( p_block = BlockUDP( p_access ) ) == NULL )
        return NULL;

    if( p_block->p_buffer[0] == 0x47 )
    {
        msg_Dbg( p_access, "detected TS over raw UDP" );
        p_access->pf_block = BlockUDP;
        return p_block;
    }

    if( p_block->i_buffer < RTP_HEADER_LEN )
        return p_block;

    /* Parse the header and make some verifications.
     * See RFC 3550. */

    i_rtp_version  = ( p_block->p_buffer[0] & 0xC0 ) >> 6;
    i_CSRC_count   = ( p_block->p_buffer[0] & 0x0F );
    i_payload_type = ( p_block->p_buffer[1] & 0x7F );

    if( i_rtp_version != 2 )
    {
        msg_Dbg( p_access, "no supported RTP header detected" );
        p_access->pf_block = BlockUDP;
        return p_block;
    }

    switch( i_payload_type )
    {
        case 33:
            msg_Dbg( p_access, "detected TS over RTP" );
            p_access->psz_demux = strdup( "ts" );
            break;

        case 14:
            msg_Dbg( p_access, "detected MPEG audio over RTP" );
            p_access->psz_demux = strdup( "mpga" );
            break;

        case 32:
            msg_Dbg( p_access, "detected MPEG video over RTP" );
            p_access->psz_demux = strdup( "mpgv" );
            break;

        default:
            msg_Dbg( p_access, "no RTP header detected" );
            p_access->pf_block = BlockUDP;
            return p_block;
    }

    p_access->pf_block = BlockRTP;

    return BlockParseRTP( p_access, p_block );
}
