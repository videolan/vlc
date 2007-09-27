/*****************************************************************************
 * udp.c: raw UDP & RTP input module
 *****************************************************************************
 * Copyright (C) 2001-2005 the VideoLAN team
 * Copyright (C) 2007 Remi Denis-Courmont
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Tristan Leteurtre <tooney@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
 *          Remi Denis-Courmont
 *
 * Reviewed: 23 October 2003, Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <vlc/vlc.h>
#include <vlc_access.h>
#include <vlc_network.h>

#ifndef SOCK_DCCP /* provisional API */
# ifdef __linux__
#  define SOCK_DCCP 6
# endif
#endif

#ifndef IPPROTO_DCCP
# define IPPROTO_DCCP 33 /* IANA */
#endif

#ifndef IPPROTO_UDPLITE
# define IPPROTO_UDPLITE 136 /* from IANA */
#endif

#define MTU 65535

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Caching value for UDP streams. This " \
    "value should be set in milliseconds." )

#define RTP_LATE_TEXT N_("RTP reordering timeout in ms")
#define RTP_LATE_LONGTEXT N_( \
    "VLC reorders RTP packets. The input will wait for late packets at most "\
    "the time specified here (in milliseconds)." )

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_shortname( _("UDP/RTP" ) );
    set_description( _("UDP/RTP input") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );

    add_integer( "udp-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT,
                 CACHING_LONGTEXT, VLC_TRUE );
    add_integer( "rtp-late", 100, NULL, RTP_LATE_TEXT, RTP_LATE_LONGTEXT, VLC_TRUE );
    add_obsolete_bool( "udp-auto-mtu" );

    set_capability( "access2", 0 );
    add_shortcut( "udp" );
    add_shortcut( "udpstream" );
    add_shortcut( "udp4" );
    add_shortcut( "udp6" );
    add_shortcut( "rtp" );
    add_shortcut( "rtp4" );
    add_shortcut( "rtp6" );
    add_shortcut( "udplite" );
    add_shortcut( "rtptcp" ); /* tcp name is already taken */
    add_shortcut( "dccp" );

    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#define RTP_HEADER_LEN 12

static block_t *BlockUDP( access_t * );
static block_t *BlockStartRTP( access_t * );
static block_t *BlockRTP( access_t * );
static block_t *BlockChoose( access_t * );
static int Control( access_t *, int, va_list );

struct access_sys_t
{
    int fd;

    vlc_bool_t b_framed_rtp, b_comedia;

    /* reorder rtp packets when out-of-sequence */
    uint16_t i_last_seqno;
    mtime_t i_rtp_late;
    block_t *p_list;
    block_t *p_end;
    block_t *p_partial_frame; /* Partial Framed RTP packet */
};

/*****************************************************************************
 * Open: open the socket
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;

    char *psz_name = strdup( p_access->psz_path );
    char *psz_parser;
    const char *psz_server_addr, *psz_bind_addr = "";
    int  i_bind_port, i_server_port = 0;
    int fam = AF_UNSPEC, proto = IPPROTO_UDP;

    /* Set up p_access */
    access_InitFields( p_access );
    ACCESS_SET_CALLBACKS( NULL, BlockStartRTP, Control, NULL );
    p_access->info.b_prebuffered = VLC_FALSE;
    MALLOC_ERR( p_access->p_sys, access_sys_t ); p_sys = p_access->p_sys;
    memset (p_sys, 0, sizeof (*p_sys));

    if (strlen (p_access->psz_access) > 0)
    {
        switch (p_access->psz_access[strlen (p_access->psz_access) - 1])
        {
            case '4':
                fam = AF_INET;
                break;

            case '6':
                fam = AF_INET6;
                break;
        }

        if (strcmp (p_access->psz_access, "udplite") == 0)
            proto = IPPROTO_UDPLITE;
        else
        if (strncmp (p_access->psz_access, "udp", 3 ) == 0 )
            p_access->pf_block = BlockChoose;
        else
        if (strcmp (p_access->psz_access, "rtptcp") == 0)
            proto = IPPROTO_TCP;
        else
        if (strcmp (p_access->psz_access, "dccp") == 0)
            proto = IPPROTO_DCCP;
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

        if( psz_bind_addr[0] == '[' )
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
    psz_parser = ( psz_server_addr[0] == '[' )
        ? strchr( psz_name, ']' ) /* skips bracket'd IPv6 address */
        : psz_name;

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

    /* Hmm, the net_* connection functions may need to be unified... */
    switch (proto)
    {
        case IPPROTO_UDP:
        case IPPROTO_UDPLITE:
            p_sys->fd = net_OpenDgram( p_access, psz_bind_addr, i_bind_port,
                                       psz_server_addr, i_server_port, fam,
                                       proto );
            break;

        case IPPROTO_TCP:
            p_sys->fd = net_ConnectTCP( p_access, psz_server_addr, i_server_port );
            p_access->pf_block = BlockRTP;
            p_sys->b_comedia = p_sys->b_framed_rtp = VLC_TRUE;
            break;

        case IPPROTO_DCCP:
#ifdef SOCK_DCCP
            p_sys->fd = net_Connect( p_access, psz_server_addr, i_server_port,
                                     SOCK_DCCP, IPPROTO_DCCP );
#else
            p_sys->fd = -1;
            msg_Err( p_access, "DCCP support not compiled-in!" );
#endif
            p_sys->b_comedia = VLC_TRUE;
            break;
    }
    free (psz_name);
    if( p_sys->fd == -1 )
    {
        msg_Err( p_access, "cannot open socket" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    shutdown( p_sys->fd, SHUT_WR );
    net_SetCSCov (p_sys->fd, -1, 12);

    /* Update default_pts to a suitable value for udp access */
    var_Create( p_access, "udp-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    /* RTP reordering for out-of-sequence packets */
    p_sys->i_rtp_late = var_CreateGetInteger( p_access, "rtp-late" ) * 1000;
    p_sys->i_last_seqno = 0;
    p_sys->p_list = NULL;
    p_sys->p_end = NULL;
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
            *pi_int = MTU;
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

    if( p_access->info.b_eof )
        return NULL;

    /* Read data */
    p_block = block_New( p_access, MTU );
    p_block->i_buffer = net_Read( p_access, p_sys->fd, NULL,
                                  p_block->p_buffer, MTU, VLC_FALSE );
    if( ( p_block->i_buffer < 0 )
     || ( p_sys->b_comedia && ( p_block->i_buffer == 0 ) ) )
    {
        if( p_sys->b_comedia )
        {
            p_access->info.b_eof = VLC_TRUE;
            msg_Dbg( p_access, "connection-oriented media hangup" );
        }
        block_Release( p_block );
        return NULL;
    }

    return block_Realloc( p_block, 0, p_block->i_buffer );
}

/*****************************************************************************
 * BlockTCP: Framed RTP/AVP packet reception for COMEDIA (see RFC4571)
 *****************************************************************************/
static block_t *BlockTCP( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    block_t      *p_block = p_sys->p_partial_frame;

    if( p_access->info.b_eof )
        return NULL;

    if( p_block == NULL )
    {
        /* MTU should always be 65535 in this case */
        p_sys->p_partial_frame = p_block = block_New( p_access, 2 + MTU );
        if (p_block == NULL)
            return NULL;
    }

    /* Read RTP framing */
    if (p_block->i_buffer < 2)
    {
        int i_read = net_Read( p_access, p_sys->fd, NULL,
                               p_block->p_buffer + p_block->i_buffer,
                               2 - p_block->i_buffer, VLC_FALSE );
        if( i_read <= 0 )
            goto error;

        p_block->i_buffer += i_read;
        if (p_block->i_buffer < 2)
            return NULL;
    }

    uint16_t framelen = GetWLE( p_block->p_buffer );
    /* Read RTP frame */
    if( framelen > 0 )
    {
        int i_read = net_Read( p_access, p_sys->fd, NULL,
                               p_block->p_buffer + p_block->i_buffer,
                               2 + framelen - p_block->i_buffer, VLC_FALSE );
        if( i_read <= 0 )
            goto error;

        p_block->i_buffer += i_read;
    }

    if( p_block->i_buffer < (2 + framelen) )
        return NULL; // incomplete frame

    /* Hide framing from RTP layer */
    p_block->p_buffer += 2;
    p_block->i_buffer -= 2;
    p_sys->p_partial_frame = NULL;
    return p_block;

error:
    p_access->info.b_eof = VLC_TRUE;
    block_Release( p_block );
    p_sys->p_partial_frame = NULL;
    return NULL;
}


/*
 * rtp_ChainInsert - insert a p_block in the chain and
 * look at the sequence numbers.
 */
static inline vlc_bool_t rtp_ChainInsert( access_t *p_access, block_t *p_block )
{
    access_sys_t *p_sys = (access_sys_t *) p_access->p_sys;
    block_t *p_prev = NULL;
    block_t *p = p_sys->p_end;
    uint16_t i_new = (uint16_t) p_block->i_dts;
    uint16_t i_tmp = 0;

    if( !p_sys->p_list )
    {
        p_sys->p_list = p_block;
        p_sys->p_end = p_block;
        return VLC_TRUE;
    }
    /* walk through the queue from top down since the new packet is in
    most cases just appended to the end */

    for( ;; )
    {
        i_tmp = i_new - (uint16_t) p->i_dts;

        if( !i_tmp )   /* trash duplicate */
            break;

        if ( i_tmp < 32768 )
        {   /* insert after this block ( i_new > p->i_dts ) */
            p_block->p_next = p->p_next;
            p->p_next = p_block;
            p_block->p_prev = p;
            if (p_prev)
            {
                p_prev->p_prev = p_block;
                msg_Dbg(p_access, "RTP reordering: insert after %d, new %d",
                        (uint16_t) p->i_dts, i_new );
            }
            else
            {
                p_sys->p_end = p_block;
            }
            return VLC_TRUE;
        }
        if( p == p_sys->p_list )
        {   /* we've reached bottom of chain */
            i_tmp = p_sys->i_last_seqno - i_new;
            if( !p_access->info.b_prebuffered || (i_tmp > 32767) )
            {
                msg_Dbg(p_access, "RTP reordering: prepend %d before %d",
                        i_new, (uint16_t) p->i_dts );
                p_block->p_next = p;
                p->p_prev = p_block;
                p_sys->p_list = p_block;
                return VLC_TRUE;
            }

            if( !i_tmp )   /* trash duplicate */
                break;

            /* reordering failed - append the packet to the end of queue */
            msg_Dbg(p_access, "RTP: sequence changed (or buffer too small) "
                    "new: %d, buffer %d...%d", i_new, (uint16_t) p->i_dts,
                (uint16_t) p_sys->p_end->i_dts);
            p_sys->p_end->p_next = p_block;
            p_block->p_prev = p_sys->p_end;
            p_sys->p_end = p_block;
            return VLC_TRUE;
        }
        p_prev = p;
        p = p->p_prev;
    }
    block_Release( p_block );
    return VLC_FALSE;
}

/*****************************************************************************
 * BlockParseRTP: decapsulate the RTP packet and return it
 *****************************************************************************/
static block_t *BlockParseRTP( access_t *p_access, block_t *p_block )
{
    int      i_payload_type;
    size_t   i_skip = RTP_HEADER_LEN;

    if( p_block == NULL )
        return NULL;

    if( p_block->i_buffer < RTP_HEADER_LEN )
    {
        msg_Dbg( p_access, "short RTP packet received" );
        goto trash;
    }

    /* Parse the header and make some verifications.
     * See RFC 3550. */
    // Version number:
    if( ( p_block->p_buffer[0] >> 6 ) != 2)
    {
        msg_Dbg( p_access, "RTP version is %u instead of 2",
                 p_block->p_buffer[0] >> 6 );
        goto trash;
    }
    // Padding bit:
    uint8_t pad = (p_block->p_buffer[0] & 0x20)
                    ? p_block->p_buffer[p_block->i_buffer - 1] : 0;
    // CSRC count:
    i_skip += (p_block->p_buffer[0] & 0x0F) * 4;
    // Extension header:
    if (p_block->p_buffer[0] & 0x10) /* Extension header */
    {
        i_skip += 4;
        if ((size_t)p_block->i_buffer < i_skip)
            goto trash;

        i_skip += 4 * GetWBE( p_block->p_buffer + i_skip - 2 );
    }

    i_payload_type    = p_block->p_buffer[1] & 0x7F;

    /* Remember sequence number in i_dts */
    p_block->i_pts = mdate();
    p_block->i_dts = (mtime_t) GetWBE( p_block->p_buffer + 2 );

    /* FIXME: use rtpmap */
    const char *psz_demux = NULL;

    switch( i_payload_type )
    {
        case 14: // MPA: MPEG Audio (RFC2250, §3.4)
            i_skip += 4; // 32 bits RTP/MPA header
            psz_demux = "mpga";
            break;

        case 32: // MPV: MPEG Video (RFC2250, §3.5)
            i_skip += 4; // 32 bits RTP/MPV header
            if( (size_t)p_block->i_buffer < i_skip )
                goto trash;
            if( p_block->p_buffer[i_skip - 3] & 0x4 )
            {
                /* MPEG2 Video extension header */
                /* TODO: shouldn't we skip this too ? */
            }
            psz_demux = "mpgv";
            break;

        case 33: // MP2: MPEG TS (RFC2250, §2)
            /* plain TS over RTP */
            psz_demux = "ts";
            break;

        case 72: /* muxed SR */
        case 73: /* muxed RR */
        case 74: /* muxed SDES */
        case 75: /* muxed BYE */
        case 76: /* muxed APP */
            goto trash; /* ooh! ignoring RTCP is evil! */

        default:
            msg_Dbg( p_access, "unsupported RTP payload type: %u", i_payload_type );
            goto trash;
    }

    if( (size_t)p_block->i_buffer < (i_skip + pad) )
        goto trash;

    /* Remove the RTP header */
    p_block->i_buffer -= i_skip;
    p_block->p_buffer += i_skip;

    /* This is the place for deciphering and authentication */

    /* Remove padding (at the end) */
    p_block->i_buffer -= pad;

#if 0
    /* Emulate packet loss */
    if ( (i_sequence_number % 4000) == 0)
    {
        msg_Warn( p_access, "Emulating packet drop" );
        block_Release( p_block );
        return NULL;
    }
#endif

    if( !p_access->psz_demux || !*p_access->psz_demux )
    {
        free( p_access->psz_demux );
        p_access->psz_demux = strdup( psz_demux );
    }

    return p_block;

trash:
    block_Release( p_block );
    return NULL;
}

/*****************************************************************************
 * BlockRTP: receives an RTP packet, parses it, queues it queue,
 * then dequeues the oldest packet and returns it to input/demux.
 ****************************************************************************/
static block_t *BlockRTP( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    block_t *p;

    while ( !p_sys->p_list ||
             ( mdate() - p_sys->p_list->i_pts ) < p_sys->i_rtp_late )
    {
        p = BlockParseRTP( p_access,
                           p_sys->b_framed_rtp ? BlockTCP( p_access )
                                               : BlockUDP( p_access ) );
        if ( !p )
            return NULL;

        rtp_ChainInsert( p_access, p );
    }

    p = p_sys->p_list;
    p_sys->p_list = p_sys->p_list->p_next;
    p_sys->i_last_seqno++;
    if( p_sys->i_last_seqno != (uint16_t) p->i_dts )
    {
        msg_Dbg( p_access, "RTP: packet(s) lost, expected %d, got %d",
                 p_sys->i_last_seqno, (uint16_t) p->i_dts );
        p_sys->i_last_seqno = (uint16_t) p->i_dts;
    }
    p->p_next = NULL;
    return p;
}

/*****************************************************************************
 * BlockPrebufferRTP: waits until we have at least two RTP datagrams,
 * so that we can synchronize the RTP sequence number.
 * This is only useful for non-reliable transport protocols.
 ****************************************************************************/
static block_t *BlockPrebufferRTP( access_t *p_access, block_t *p_block )
{
    access_sys_t *p_sys = p_access->p_sys;
    mtime_t   i_first = mdate();
    int       i_count = 0;
    block_t   *p = p_block;

    if( BlockParseRTP( p_access, p_block ) == NULL )
        return NULL;

    for( ;; )
    {
        mtime_t i_date = mdate();

        if( p && rtp_ChainInsert( p_access, p ))
            i_count++;

        /* Require at least 2 packets in the buffer */
        if( i_count > 2 && (i_date - i_first) > p_sys->i_rtp_late )
            break;

        p = BlockParseRTP( p_access, BlockUDP( p_access ) );
        if( !p && (i_date - i_first) > p_sys->i_rtp_late )
        {
            msg_Err( p_access, "error in RTP prebuffering!" );
            return NULL;
        }
    }

    msg_Dbg( p_access, "RTP: prebuffered %d packets", i_count - 1 );
    p_access->info.b_prebuffered = VLC_TRUE;
    p = p_sys->p_list;
    p_sys->p_list = p_sys->p_list->p_next;
    p_sys->i_last_seqno = (uint16_t) p->i_dts;
    p->p_next = NULL;
    return p;
}

static block_t *BlockStartRTP( access_t *p_access )
{
    p_access->pf_block = BlockRTP;
    return BlockPrebufferRTP( p_access, BlockUDP( p_access ) );
}


/*****************************************************************************
 * BlockChoose: decide between RTP and UDP
 *****************************************************************************/
static block_t *BlockChoose( access_t *p_access )
{
    block_t *p_block;
    int     i_rtp_version;
    int     i_payload_type;

    if( ( p_block = BlockUDP( p_access ) ) == NULL )
        return NULL;

    if( p_block->p_buffer[0] == 0x47 )
    {
        msg_Dbg( p_access, "detected TS over raw UDP" );
        p_access->pf_block = BlockUDP;
        p_access->info.b_prebuffered = VLC_TRUE;
        return p_block;
    }

    if( p_block->i_buffer < RTP_HEADER_LEN )
        return p_block;

    /* Parse the header and make some verifications.
     * See RFC 3550. */

    i_rtp_version  = p_block->p_buffer[0] >> 6;
    i_payload_type = ( p_block->p_buffer[1] & 0x7F );

    if( i_rtp_version != 2 )
    {
        msg_Dbg( p_access, "no supported RTP header detected" );
        p_access->pf_block = BlockUDP;
        p_access->info.b_prebuffered = VLC_TRUE;
        return p_block;
    }

    switch( i_payload_type )
    {
        case 33:
            msg_Dbg( p_access, "detected MPEG2 TS over RTP" );
            p_access->psz_demux = strdup( "ts" );
            break;

        case 14:
            msg_Dbg( p_access, "detected MPEG Audio over RTP" );
            p_access->psz_demux = strdup( "mpga" );
            break;

        case 32:
            msg_Dbg( p_access, "detected MPEG Video over RTP" );
            p_access->psz_demux = strdup( "mpgv" );
            break;

        default:
            msg_Dbg( p_access, "no RTP header detected" );
            p_access->pf_block = BlockUDP;
            p_access->info.b_prebuffered = VLC_TRUE;
            return p_block;
    }

    p_access->pf_block = BlockRTP;
    return BlockPrebufferRTP( p_access, p_block );
}
