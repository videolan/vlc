/*****************************************************************************
 * ts.c: Transport Stream input module for VLC.
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id: ts.c,v 1.10 2004/02/01 04:50:13 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "iso_lang.h"
#include "network.h"

#include "../mux/mpeg/csa.h"

/* Include dvbpsi headers */
#ifdef HAVE_DVBPSI_DR_H
#   include <dvbpsi/dvbpsi.h>
#   include <dvbpsi/descriptor.h>
#   include <dvbpsi/pat.h>
#   include <dvbpsi/pmt.h>
#   include <dvbpsi/dr.h>
#   include <dvbpsi/psi.h>
#else
#   include "dvbpsi.h"
#   include "descriptor.h"
#   include "tables/pat.h"
#   include "tables/pmt.h"
#   include "descriptors/dr.h"
#   include "psi.h"
#endif

/* TODO:
 *  - XXX: do not mark options message to be translated, they are too osbcure for now ...
 *  - test it
 *  - ...
 */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close  ( vlc_object_t * );

vlc_module_begin();
    set_description( _("ISO 13818-1 MPEG Transport Stream input - new" ) );
    add_string( "ts-extra-pmt", NULL, NULL, "extra PMT", "allow user to specify an extra pmt (pmt_pid=pid:stream_type[,...])", VLC_TRUE );
    add_bool( "ts-es-id-pid", 0, NULL, "set id of es to pid", "set id of es to pid", VLC_TRUE );
    add_string( "ts-out", NULL, NULL, "fast udp streaming", "send TS to specific ip:port by udp (you must know what you are doing)", VLC_TRUE );
    add_integer( "ts-out-mtu", 1500, NULL, "MTU for out mode", "MTU for out mode", VLC_TRUE );
    add_string( "ts-csa-ck", NULL, NULL, "CSA ck", "CSA ck", VLC_TRUE );
    set_capability( "demux2", 10 );
    set_callbacks( Open, Close );
    add_shortcut( "ts2" );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct
{
    uint8_t                 i_objectTypeIndication;
    uint8_t                 i_streamType;
    vlc_bool_t              b_upStream;
    uint32_t                i_bufferSizeDB;
    uint32_t                i_maxBitrate;
    uint32_t                i_avgBitrate;

    int                     i_decoder_specific_info_len;
    uint8_t                 *p_decoder_specific_info;

} decoder_config_descriptor_t;

typedef struct
{
    vlc_bool_t              b_useAccessUnitStartFlag;
    vlc_bool_t              b_useAccessUnitEndFlag;
    vlc_bool_t              b_useRandomAccessPointFlag;
    vlc_bool_t              b_useRandomAccessUnitsOnlyFlag;
    vlc_bool_t              b_usePaddingFlag;
    vlc_bool_t              b_useTimeStampsFlags;
    vlc_bool_t              b_useIdleFlag;
    vlc_bool_t              b_durationFlag;
    uint32_t                i_timeStampResolution;
    uint32_t                i_OCRResolution;
    uint8_t                 i_timeStampLength;
    uint8_t                 i_OCRLength;
    uint8_t                 i_AU_Length;
    uint8_t                 i_instantBitrateLength;
    uint8_t                 i_degradationPriorityLength;
    uint8_t                 i_AU_seqNumLength;
    uint8_t                 i_packetSeqNumLength;

    uint32_t                i_timeScale;
    uint16_t                i_accessUnitDuration;
    uint16_t                i_compositionUnitDuration;

    uint64_t                i_startDecodingTimeStamp;
    uint64_t                i_startCompositionTimeStamp;

} sl_config_descriptor_t;

typedef struct
{
    vlc_bool_t              b_ok;
    uint16_t                i_es_id;

    vlc_bool_t              b_streamDependenceFlag;
    vlc_bool_t              b_OCRStreamFlag;
    uint8_t                 i_streamPriority;

    char                    *psz_url;

    uint16_t                i_dependOn_es_id;
    uint16_t                i_OCR_es_id;

    decoder_config_descriptor_t    dec_descr;
    sl_config_descriptor_t         sl_descr;
} es_mpeg4_descriptor_t;

typedef struct
{
    uint8_t                i_iod_label;

    /* IOD */
    uint16_t                i_od_id;
    char                    *psz_url;

    uint8_t                 i_ODProfileLevelIndication;
    uint8_t                 i_sceneProfileLevelIndication;
    uint8_t                 i_audioProfileLevelIndication;
    uint8_t                 i_visualProfileLevelIndication;
    uint8_t                 i_graphicsProfileLevelIndication;

    es_mpeg4_descriptor_t   es_descr[255];

} iod_descriptor_t;

typedef struct
{
    int             i_version;
    int             i_number;
    int             i_pid_pcr;
    dvbpsi_handle   handle;

    /* IOD stuff (mpeg4) */
    iod_descriptor_t *iod;

} ts_psi_t;

typedef struct
{
    es_format_t  fmt;
    es_out_id_t *id;
    block_t     *p_pes;

    es_mpeg4_descriptor_t *p_mpeg4desc;
} ts_es_t;

typedef struct
{
    int         i_pid;

    vlc_bool_t  b_seen;
    vlc_bool_t  b_valid;
    int         i_cc;   /* countinuity counter */

    /* PSI owner (ie PMT -> PAT, ES -> PMT */
    ts_psi_t   *p_owner;

    /* */
    ts_psi_t    *psi;
    ts_es_t     *es;

} ts_pid_t;

struct demux_sys_t
{
    /* how many TS packet we read at once */
    int         i_ts_read;

    /* All pid */
    ts_pid_t    pid[8192];

    /* All PMT */
    int         i_pmt;
    ts_pid_t    **pmt;

    /* */
    vlc_bool_t  b_es_id_pid;
    csa_t       *csa;

    vlc_bool_t  b_udp_out;
    int         fd; /* udp socket */
    uint8_t     *buffer;
};

static int Demux  ( demux_t *p_demux );
static int Control( demux_t *p_demux, int i_query, va_list args );


static void PIDInit ( ts_pid_t *pid, vlc_bool_t b_psi, ts_psi_t *p_owner );
static void PIDClean( es_out_t *out, ts_pid_t *pid );
static int  PIDFillFormat( ts_pid_t *pid, int i_stream_type );

static void PATCallBack( demux_t *, dvbpsi_pat_t * );
static void PMTCallBack( demux_t *p_demux, dvbpsi_pmt_t *p_pmt );

static inline int PIDGet( block_t *p )
{
    return ( (p->p_buffer[1]&0x1f)<<8 )|p->p_buffer[2];
}

static vlc_bool_t GatherPES( demux_t *p_demux, ts_pid_t *pid, block_t *p_bk );

static iod_descriptor_t *IODNew( int , uint8_t * );
static void              IODFree( iod_descriptor_t * );


/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    uint8_t     *p_peek;
    int          i_peek;
    int          i_sync;
    int          i;

    ts_pid_t     *pat;

    vlc_value_t  val;

    if( ( i_peek = stream_Peek( p_demux->s, &p_peek, 189 ) ) < 1 )
    {
        msg_Err( p_demux, "cannot peek" );
        return VLC_EGENERIC;
    }

    /* Search first synch */
    for( i_sync = 0; i_sync < i_peek; i_sync++ )
    {
        if( p_peek[i_sync] == 0x47 ) break;
    }
    if( i_sync >= i_peek )
    {
        if( strcmp( p_demux->psz_demux, "ts2" ) )
        {
            msg_Warn( p_demux, "TS module discarded" );
            return VLC_EGENERIC;
        }
        msg_Warn( p_demux, "this does not look like a TS stream, continuing" );
    }
    if( strcmp( p_demux->psz_demux, "ts2" ) )
    {
        /* Check next 3 sync points */
        i_peek = 188*3 + 1 + i_sync;
        if( ( stream_Peek( p_demux->s, &p_peek, i_peek ) ) < i_peek )
        {
            msg_Err( p_demux, "cannot peek" );
            return VLC_EGENERIC;
        }
        if( p_peek[i_sync+  188] != 0x47 || p_peek[i_sync+2*188] != 0x47 ||
            p_peek[i_sync+3*188] != 0x47 )
        {
            msg_Warn( p_demux, "TS module discarded (lost sync)" );
            return VLC_EGENERIC;
        }
    }

    /* Fill p_demux field */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    memset( p_sys, 0, sizeof( demux_sys_t ) );

    /* Init p_sys field */
    for( i = 0; i < 8192; i++ )
    {
        ts_pid_t *pid = &p_sys->pid[i];

        pid->i_pid      = i;
        pid->b_seen     = VLC_FALSE;
        pid->b_valid    = VLC_FALSE;
    }
    p_sys->b_udp_out = VLC_FALSE;
    p_sys->i_ts_read = 50;
    p_sys->csa = NULL;

    /* Init PAT handler */
    pat = &p_sys->pid[0];
    PIDInit( pat, VLC_TRUE, NULL );
    pat->psi->handle = dvbpsi_AttachPAT( (dvbpsi_pat_callback)PATCallBack, p_demux );

    /* Init PMT array */
    p_sys->i_pmt = 0;
    p_sys->pmt   = NULL;

    /* Read config */
    var_Create( p_demux, "ts-es-id-pid", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "ts-es-id-pid", &val );
    p_sys->b_es_id_pid = val.b_bool,

    var_Create( p_demux, "ts-out", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "ts-out", &val );
    if( val.psz_string && *val.psz_string )
    {
        vlc_value_t mtu;
        char *psz = strchr( val.psz_string, ':' );
        int   i_port = 0;

        p_sys->b_udp_out = VLC_TRUE;

        if( psz )
        {
            *psz++ = '\0';
            i_port = atoi( psz );
        }
        if( i_port <= 0 ) i_port  = 1234;
        msg_Dbg( p_demux, "resend ts to '%s:%d'", val.psz_string, i_port );

        p_sys->fd = net_OpenUDP( p_demux, "", 0, val.psz_string, i_port );
        if( p_sys->fd < 0 )
        {
            msg_Err( p_demux, "failed to open udp socket, send disabled" );
            p_sys->b_udp_out = VLC_FALSE;
        }
        else
        {
            var_Create( p_demux, "ts-out-mtu", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
            var_Get( p_demux, "ts-out-mtu", &mtu );
            p_sys->i_ts_read = mtu.i_int / 188;
            if( p_sys->i_ts_read <= 0 )
            {
                p_sys->i_ts_read = 1500 / 188;
            }
            p_sys->buffer = malloc( 188 * p_sys->i_ts_read );
        }
    }
    if( val.psz_string )
    {
        free( val.psz_string );
    }


    /* We handle description of an extra PMT */
    var_Create( p_demux, "ts-extra-pmt", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "ts-extra-pmt", &val );
    if( val.psz_string && strchr( val.psz_string, '=' ) != NULL )
    {
        char *psz = val.psz_string;
        int  i_pid = strtol( psz, &psz, 0 );

        if( i_pid >= 2 && i_pid < 8192 )
        {
            ts_pid_t *pmt = &p_sys->pid[i_pid];

            msg_Dbg( p_demux, "extra pmt specified (pid=0x%x)", i_pid );
            PIDInit( pmt, VLC_TRUE, NULL );
            /* FIXME we should also ask for a number */
            pmt->psi->handle = dvbpsi_AttachPMT( 1, (dvbpsi_pmt_callback)PMTCallBack, p_demux );
            pmt->psi->i_number = 0; /* special one */

            psz = strchr( psz, '=' ) + 1;   /* can't failed */
            while( psz && *psz )
            {
                char *psz_next = strchr( psz, ',' );
                int i_pid, i_stream_type;

                if( psz_next )
                {
                    *psz_next++ = '\0';
                }

                i_pid = strtol( psz, &psz, 0 );
                if( *psz == ':' )
                {
                    i_stream_type = strtol( psz + 1, &psz, 0 );
                    if( i_pid >= 2 && i_pid < 8192 && !p_sys->pid[i_pid].b_valid )
                    {
                        ts_pid_t *pid = &p_sys->pid[i_pid];

                        PIDInit( pid, VLC_FALSE, pmt->psi);
                        if( pmt->psi->i_pid_pcr <= 0 )
                        {
                            pmt->psi->i_pid_pcr = i_pid;
                        }
                        PIDFillFormat( pid, i_stream_type);
                        if( pid->es->fmt.i_cat != UNKNOWN_ES )
                        {
                            if( p_sys->b_es_id_pid )
                            {
                                pid->es->fmt.i_id = i_pid;
                            }
                            msg_Dbg( p_demux, "  * es pid=0x%x type=0x%x fcc=%4.4s", i_pid, i_stream_type, (char*)&pid->es->fmt.i_codec );
                            pid->es->id = es_out_Add( p_demux->out, &pid->es->fmt );
                        }
                    }
                }
                psz = psz_next;
            }
        }
    }
    if( val.psz_string )
    {
        free( val.psz_string );
    }

    var_Create( p_demux, "ts-csa-ck", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "ts-csa-ck", &val );
    if( val.psz_string && *val.psz_string )
    {
        char *psz = val.psz_string;
        if( psz[0] == '0' && ( psz[1] == 'x' || psz[1] == 'X' ) )
        {
            psz += 2;
        }
        if( strlen( psz ) != 16 )
        {
            msg_Warn( p_demux, "invalid csa ck (it must be 16 chars long)" );
        }
        else
        {
            uint64_t i_ck = strtoll( psz, NULL, 16 );
            uint8_t ck[8];
            int     i;
            for( i = 0; i < 8; i++ )
            {
                ck[i] = ( i_ck >> ( 56 - 8*i) )&0xff;
            }

            msg_Dbg( p_demux, "using CSA scrambling with ck=%x:%x:%x:%x:%x:%x:%x:%x",
                     ck[0], ck[1], ck[2], ck[3], ck[4], ck[5], ck[6], ck[7] );

            p_sys->csa = csa_New();
            csa_SetCW( p_sys->csa, ck, ck );
        }
    }
    if( val.psz_string )
    {
        free( val.psz_string );
    }


    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    int          i;

    msg_Dbg( p_demux, "pid list:" );
    for( i = 0; i < 8192; i++ )
    {
        ts_pid_t *pid = &p_sys->pid[i];

        if( pid->b_valid && pid->psi )
        {
            switch( pid->i_pid )
            {
                case 0: /* PAT */
                    dvbpsi_DetachPAT( pid->psi->handle );
                    break;
                case 1: /* CAT */
                    break;
                default:
                    dvbpsi_DetachPMT( pid->psi->handle );
                    if( pid->psi->iod )
                    {
                        IODFree( pid->psi->iod );
                    }
                    break;
            }
            free( pid->psi );
        }
        else if( pid->b_valid && pid->es )
        {
            if( pid->es->p_pes )
            {
                block_ChainRelease( pid->es->p_pes );
            }
            free( pid->es );
        }
        if( pid->b_seen )
        {
            msg_Dbg( p_demux, "  - pid[0x%x] seen", pid->i_pid );
        }
    }

    if( p_sys->b_udp_out )
    {
        net_Close( p_sys->fd );
        free( p_sys->buffer );
    }
    if( p_sys->csa )
    {
        csa_Delete( p_sys->csa );
    }

    free( p_sys );
}


/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int          i_pkt;

    /* We read at most 100 TS packet or until a frame is completed */
    for( i_pkt = 0; i_pkt < p_sys->i_ts_read; i_pkt++ )
    {
        vlc_bool_t  b_frame = VLC_FALSE;
        block_t     *p_pkt;
        ts_pid_t    *p_pid;

        /* Get a new TS packet */
        if( ( p_pkt = stream_Block( p_demux->s, 188 ) ) == NULL )
        {
            msg_Dbg( p_demux, "eof ?" );
            return 0;
        }
        if( p_pkt->p_buffer[0] != 0x47 )
        {
            msg_Warn( p_demux, "lost synchro" );
            block_Release( p_pkt );

            /* Resynch */
            while( !p_demux->b_die )
            {
                uint8_t    *p_peek;
                int         i_peek = stream_Peek( p_demux->s, &p_peek, 1880 );
                int         i_skip = 0;
                vlc_bool_t  b_ok = VLC_FALSE;

                if( i_peek < 189 )
                {
                    msg_Dbg( p_demux, "eof ?" );
                    return 0;
                }

                while( i_skip < i_peek - 188 )
                {
                    if( p_peek[i_skip] == 0x47 && p_peek[i_skip+188] == 0x47 )
                    {
                        b_ok = VLC_TRUE;
                        break;
                    }
                    i_skip++;
                }
                stream_Read( p_demux->s, NULL, i_skip );
                msg_Dbg( p_demux, "%d bytes of garbage", i_skip );
                if( b_ok )
                {
                    break;
                }
            }
            if( ( p_pkt = stream_Block( p_demux->s, 188 ) ) == NULL )
            {
                msg_Dbg( p_demux, "eof ?" );
                return 0;
            }
        }

        if( p_sys->b_udp_out )
        {
            memcpy( &p_sys->buffer[i_pkt*188], p_pkt->p_buffer, 188 );
        }

        /* Parse the TS packet */
        p_pid = &p_sys->pid[PIDGet( p_pkt )];

        if( p_pid->b_valid )
        {
            if( p_pid->psi )
            {
                dvbpsi_PushPacket( p_pid->psi->handle, p_pkt->p_buffer );
                block_Release( p_pkt );
            }
            else if( !p_sys->b_udp_out )
            {
                b_frame = GatherPES( p_demux, p_pid, p_pkt );
            }
            else
            {
                block_Release( p_pkt );
            }
        }
        else
        {
            if( !p_pid->b_seen )
            {
                msg_Dbg( p_demux, "pid[0x%x] unknown", p_pid->i_pid );
            }
            block_Release( p_pkt );
        }
        p_pid->b_seen = VLC_TRUE;

        if( b_frame )
        {
            break;
        }
    }

    if( p_sys->b_udp_out )
    {
        /* Send the complete block */
        net_Write( p_demux, p_sys->fd, p_sys->buffer, p_sys->i_ts_read * 188 );
    }

    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    /* demux_sys_t *p_sys = p_demux->p_sys; */
    double f, *pf;
    int64_t i64;

    switch( i_query )
    {
        case DEMUX_GET_POSITION:
            pf = (double*) va_arg( args, double* );
            i64 = stream_Size( p_demux->s );
            if( i64 > 0 )
            {
                *pf = (double)stream_Tell( p_demux->s ) / (double)i64;
            }
            else
            {
                *pf = 0.0;
            }
            return VLC_SUCCESS;
        case DEMUX_SET_POSITION:
            f = (double) va_arg( args, double );
            i64 = stream_Size( p_demux->s );

            es_out_Control( p_demux->out, ES_OUT_RESET_PCR );
            if( stream_Seek( p_demux->s, (int64_t)(i64 * f) ) )
            {
                return VLC_EGENERIC;
            }
            return VLC_SUCCESS;
#if 0

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->i_time < 0 )
            {
                *pi64 = 0;
                return VLC_EGENERIC;
            }
            *pi64 = p_sys->i_time;
            return VLC_SUCCESS;

#if 0
        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->i_mux_rate > 0 )
            {
                *pi64 = (int64_t)1000000 * ( stream_Size( p_demux->s ) / 50 ) / p_sys->i_mux_rate;
                return VLC_SUCCESS;
            }
            *pi64 = 0;
            return VLC_EGENERIC;

        case DEMUX_SET_TIME:
#endif
        case DEMUX_GET_FPS:
            pf = (double*)va_arg( args, double * );
            *pf = (double)1000000.0 / (double)p_sys->i_pcr_inc;
            return VLC_SUCCESS;
#endif
        default:
            return VLC_EGENERIC;
    }
}

static void PIDInit( ts_pid_t *pid, vlc_bool_t b_psi, ts_psi_t *p_owner )
{
    pid->b_valid    = VLC_TRUE;
    pid->i_cc       = 0xff;
    pid->p_owner    = p_owner;

    if( b_psi )
    {
        pid->psi = malloc( sizeof( ts_psi_t ) );
        pid->es  = NULL;

        pid->psi->i_version  = -1;
        pid->psi->i_number   = -1;
        pid->psi->i_pid_pcr  = -1;
        pid->psi->handle     = NULL;
        pid->psi->iod        = NULL;
    }
    else
    {
        pid->psi = NULL;
        pid->es  = malloc( sizeof( ts_es_t ) );

        es_format_Init( &pid->es->fmt, UNKNOWN_ES, 0 );
        pid->es->id      = NULL;
        pid->es->p_pes   = NULL;
        pid->es->p_mpeg4desc = NULL;
    }
}

static void PIDClean( es_out_t *out, ts_pid_t *pid )
{
    if( pid->psi )
    {
        if( pid->psi->handle )
        {
            dvbpsi_DetachPMT( pid->psi->handle );
        }
        if( pid->psi->iod )
        {
            IODFree( pid->psi->iod );
        }
        free( pid->psi );
    }
    else
    {
        if( pid->es->id )
        {
            es_out_Del( out, pid->es->id );
        }
        if( pid->es->p_pes )
        {
            block_ChainRelease( pid->es->p_pes );
        }
        free( pid->es );
    }
    pid->b_valid = VLC_FALSE;
}

/****************************************************************************
 * gathering stuff
 ****************************************************************************/
static void ParsePES ( demux_t *p_demux, ts_pid_t *pid )
{
    block_t *p_pes = pid->es->p_pes;
    uint8_t header[30];
    int     i_pes_size;
    int     i_skip = 0;
    mtime_t i_dts = -1;
    mtime_t i_pts = -1;
    int i_max;

    /* remove the pes from pid */
    pid->es->p_pes = NULL;

    /* FIXME find real max size */
    i_max = block_ChainExtract( p_pes, header, 30 );

    if( header[0] != 0 || header[1] != 0 || header[2] != 1 )
    {
        msg_Warn( p_demux, "invalid header [0x%x:%x:%x:%x]", header[0], header[1],header[2],header[3] );
        block_ChainRelease( p_pes );
        return;
    }

    i_pes_size = (header[4] << 8)|header[5];

    /* TODO check size */

    switch( header[3] )
    {
        case 0xBC:  /* Program stream map */
        case 0xBE:  /* Padding */
        case 0xBF:  /* Private stream 2 */
        case 0xB0:  /* ECM */
        case 0xB1:  /* EMM */
        case 0xFF:  /* Program stream directory */
        case 0xF2:  /* DSMCC stream */
        case 0xF8:  /* ITU-T H.222.1 type E stream */
            i_skip = 6;
            break;
        default:
            if( ( header[6]&0xC0 ) == 0x80 )
            {
                /* mpeg2 PES */
                i_skip = header[8] + 9;

                if( header[7]&0x80 )    /* has pts */
                {
                    i_pts = ((mtime_t)(header[ 9]&0x0e ) << 29)|
                             (mtime_t)(header[10] << 22)|
                            ((mtime_t)(header[11]&0xfe) << 14)|
                             (mtime_t)(header[12] << 7)|
                             (mtime_t)(header[12] >> 1);

                    if( header[7]&0x40 )    /* has dts */
                    {
                         i_dts = ((mtime_t)(header[14]&0x0e ) << 29)|
                                 (mtime_t)(header[15] << 22)|
                                ((mtime_t)(header[16]&0xfe) << 14)|
                                 (mtime_t)(header[17] << 7)|
                                 (mtime_t)(header[18] >> 1);
                    }
                }
            }
            else
            {
                i_skip = 6;
                while( i_skip < 23 && header[i_skip] == 0xff )
                {
                    i_skip++;
                }
                if( i_skip == 23 )
                {
                    msg_Err( p_demux, "too much MPEG-1 stuffing" );
                    block_ChainRelease( p_pes );
                    return;
                }
                if( ( header[i_skip] & 0xC0 ) == 0x40 )
                {
                    i_skip += 2;
                }

                if(  header[i_skip]&0x20 )
                {
                     p_pes->i_pts = ((mtime_t)(header[i_skip]&0x0e ) << 29)|
                                     (mtime_t)(header[i_skip+1] << 22)|
                                    ((mtime_t)(header[i_skip+2]&0xfe) << 14)|
                                     (mtime_t)(header[i_skip+3] << 7)|
                                     (mtime_t)(header[i_skip+4] >> 1);

                    if( header[i_skip]&0x10 )    /* has dts */
                    {
                         p_pes->i_dts = ((mtime_t)(header[i_skip+5]&0x0e ) << 29)|
                                         (mtime_t)(header[i_skip+6] << 22)|
                                        ((mtime_t)(header[i_skip+7]&0xfe) << 14)|
                                         (mtime_t)(header[i_skip+8] << 7)|
                                         (mtime_t)(header[i_skip+9] >> 1);
                         i_skip += 10;
                    }
                    else
                    {
                        i_skip += 5;
                    }
                }
                else
                {
                    i_skip += 1;
                }
            }
            break;
    }

    if( pid->es->fmt.i_codec == VLC_FOURCC( 'a', '5', '2', 'b' ) ||
        pid->es->fmt.i_codec == VLC_FOURCC( 'd', 't', 's', 'b' ) )
    {
        i_skip += 4;
    }
    else if( pid->es->fmt.i_codec == VLC_FOURCC( 'l', 'p', 'c', 'b' ) ||
             pid->es->fmt.i_codec == VLC_FOURCC( 's', 'p', 'u', 'b' ) ||
             pid->es->fmt.i_codec == VLC_FOURCC( 's', 'd', 'd', 'b' ) )
    {
        i_skip += 1;
    }

    /* skip header */
    while( p_pes && i_skip > 0 )
    {
        if( p_pes->i_buffer <= i_skip )
        {
            block_t *p_next = p_pes->p_next;

            i_skip -= p_pes->i_buffer;
            block_Release( p_pes );
            p_pes = p_next;
        }
        else
        {
            p_pes->i_buffer -= i_skip;
            p_pes->p_buffer += i_skip;
            break;
        }
    }
    if( p_pes )
    {
        if( i_dts >= 0 )
        {
            p_pes->i_dts = i_dts * 100 / 9;
        }
        if( i_pts >= 0 )
        {
            p_pes->i_pts = i_pts * 100 / 9;
        }

        /* For mpeg4/mscodec we first gather the packet -> will make ffmpeg happier */
        es_out_Send( p_demux->out, pid->es->id, block_ChainGather( p_pes ) );
    }
    else
    {
        msg_Warn( p_demux, "empty pes" );
    }
};

static vlc_bool_t GatherPES( demux_t *p_demux, ts_pid_t *pid, block_t *p_bk )
{
    const uint8_t    *p = p_bk->p_buffer;
    const vlc_bool_t  b_adaptation= p[3]&0x20;
    const vlc_bool_t  b_payload   = p[3]&0x10;
    const int         i_cc        = p[3]&0x0f;   /* continuity counter */
    /* transport_scrambling_control is ignored */

    int         i_skip = 0;
    vlc_bool_t  i_ret   = VLC_FALSE;

    int         i_diff;

    //msg_Dbg( p_demux, "pid=0x%x unit_start=%d adaptation=%d payload=%d cc=0x%x", i_pid, b_unit_start, b_adaptation, b_payload, i_continuity_counter);
    if( p[1]&0x80 )
    {
        msg_Dbg( p_demux, "transport_error_indicator set (pid=0x%x)", pid->i_pid );
    }

    if( p_demux->p_sys->csa )
    {
        csa_Decrypt( p_demux->p_sys->csa, p_bk->p_buffer );
    }

    if( !b_adaptation )
    {
        i_skip = 4;
    }
    else
    {
        /* p[4] is adaptation length */
        i_skip = 5 + p[4];
        if( p[4] > 0 )
        {
            if( p[5]&0x80 )
            {
                msg_Warn( p_demux, "discontinuity_indicator (pid=0x%x) ignored", pid->i_pid );
            }
        }
    }
    /* test continuity counter */
    /* continuous when (one of this):
        * diff == 1
        * diff == 0 and payload == 0
        * diff == 0 and duplicate packet (playload != 0) <- do we should test the content ?
     */

    i_diff = ( i_cc - pid->i_cc )&0x0f;
    if( b_payload && i_diff == 1 )
    {
        pid->i_cc++;
    }
    else
    {
        if( pid->i_cc == 0xff )
        {
            msg_Warn( p_demux, "first packet for pid=0x%x cc=0x%x", pid->i_pid, i_cc );
            pid->i_cc = i_cc;
        }
        else if( i_diff != 0 )
        {
            /* FIXME what to do when discontinuity_indicator is set ? */
            msg_Warn( p_demux, "discontinuity received 0x%x instead of 0x%x",
                      i_cc, ( pid->i_cc + 1 )&0x0f );

            pid->i_cc = i_cc;

            if( pid->es->p_pes )
            {
                block_ChainRelease( pid->es->p_pes );
                pid->es->p_pes = NULL;
            }
        }
    }

    if( b_adaptation &&
        (p[5] & 0x10) && p[4]>=7 && pid->p_owner && pid->p_owner->i_pid_pcr == pid->i_pid )
    {
        mtime_t i_pcr;  /* 33 bits */

        i_pcr = ( (mtime_t)p[6] << 25 ) |
                ( (mtime_t)p[7] << 17 ) |
                ( (mtime_t)p[8] << 9 ) |
                ( (mtime_t)p[9] << 1 ) |
                ( (mtime_t)p[10] >> 7 );

        es_out_Control( p_demux->out, ES_OUT_SET_GROUP_PCR, (int)pid->p_owner->i_number, (int64_t)(i_pcr * 100 / 9) );
    }


    if( i_skip >= 188 || pid->es->id == NULL || p_demux->p_sys->b_udp_out )
    {
        block_Release( p_bk );
    }
    else
    {
        const vlc_bool_t b_unit_start= p[1]&0x40;

        /* we have to gather it */
        p_bk->p_buffer += i_skip;
        p_bk->i_buffer -= i_skip;

        if( b_unit_start )
        {
            if( pid->es->p_pes )
            {
                ParsePES( p_demux, pid );
                i_ret = VLC_TRUE;
            }

            pid->es->p_pes = p_bk;
        }
        else
        {
            if( pid->es->p_pes == NULL )
            {
                /* msg_Dbg( p_demux, "broken packet" ); */
                block_Release( p_bk );
            }
            else
            {
                /* TODO check if when have gather enough packet to form a PES (ie read PES size)*/
                block_ChainAppend( &pid->es->p_pes, p_bk );
            }
        }
    }

    return i_ret;
}

static int PIDFillFormat( ts_pid_t *pid, int i_stream_type )
{
    es_format_t *fmt = &pid->es->fmt;

    switch( i_stream_type )
    {
        case 0x01:  /* MPEG-1 video */
        case 0x02:  /* MPEG-2 video */
        case 0x80:  /* MPEG-2 MOTO video */
            es_format_Init( fmt, VIDEO_ES, VLC_FOURCC( 'm', 'p', 'g', 'v' ) );
            break;
        case 0x03:  /* MPEG-1 audio */
        case 0x04:  /* MPEG-2 audio */
            es_format_Init( fmt, AUDIO_ES, VLC_FOURCC( 'm', 'p', 'g', 'a' ) );
            break;
        case 0x11:  /* MPEG4 (audio) */
        case 0x0f:  /* ISO/IEC 13818-7 Audio with ADTS transport syntax */
            es_format_Init( fmt, AUDIO_ES, VLC_FOURCC( 'm', 'p', '4', 'a' ) );
            break;
        case 0x10:  /* MPEG4 (video) */
            es_format_Init( fmt, VIDEO_ES, VLC_FOURCC( 'm', 'p', '4', 'v' ) );
            break;
        case 0x1B:  /* H264 <- check transport syntax/needed descriptor */
            es_format_Init( fmt, VIDEO_ES, VLC_FOURCC( 'h', '2', '6', '4' ) );
            break;

        case 0x81:  /* A52 (audio) */
            es_format_Init( fmt, AUDIO_ES, VLC_FOURCC( 'a', '5', '2', ' ' ) );
            break;
        case 0x82:  /* DVD_SPU (sub) */
            es_format_Init( fmt, SPU_ES, VLC_FOURCC( 's', 'p', 'u', ' ' ) );
            break;
        case 0x83:  /* LPCM (audio) */
            es_format_Init( fmt, AUDIO_ES, VLC_FOURCC( 'l', 'p', 'c', 'm' ) );
            break;
        case 0x84:  /* SDDS (audio) */
            es_format_Init( fmt, AUDIO_ES, VLC_FOURCC( 's', 'd', 'd', 's' ) );
            break;
        case 0x85:  /* DTS (audio) */
            es_format_Init( fmt, AUDIO_ES, VLC_FOURCC( 'd', 't', 's', ' ' ) );
            break;

        case 0x91:  /* A52 vls (audio) */
            es_format_Init( fmt, AUDIO_ES, VLC_FOURCC( 'a', '5', '2', 'b' ) );
            break;
        case 0x92:  /* DVD_SPU vls (sub) */
            es_format_Init( fmt, SPU_ES, VLC_FOURCC( 's', 'p', 'u', 'b' ) );
            break;
        case 0x93:  /* LPCM vls (audio) */
            es_format_Init( fmt, AUDIO_ES, VLC_FOURCC( 'l', 'p', 'c', 'b' ) );
            break;
        case 0x94:  /* SDDS (audio) */
            es_format_Init( fmt, AUDIO_ES, VLC_FOURCC( 's', 'd', 'd', 'b' ) );
            break;

        case 0x06:  /* PES_PRIVATE  (fixed later) */
        case 0xa0:  /* MSCODEC vlc (video) (fixed later) */
        default:
            es_format_Init( fmt, UNKNOWN_ES, 0 );
            break;
    }

    return fmt->i_cat == UNKNOWN_ES ? VLC_EGENERIC : VLC_SUCCESS ;
}

/*****************************************************************************
 * MP4 specific functions (IOD parser)
 *****************************************************************************/
static int  IODDescriptorLength( int *pi_data, uint8_t **pp_data )
{
    unsigned int i_b;
    unsigned int i_len = 0;
    do
    {
        i_b = **pp_data;
        (*pp_data)++;
        (*pi_data)--;
        i_len = ( i_len << 7 ) + ( i_b&0x7f );

    } while( i_b&0x80 );

    return( i_len );
}
static int IODGetByte( int *pi_data, uint8_t **pp_data )
{
    if( *pi_data > 0 )
    {
        const int i_b = **pp_data;
        (*pp_data)++;
        (*pi_data)--;
        return( i_b );
    }
    return( 0 );
}
static int IODGetWord( int *pi_data, uint8_t **pp_data )
{
    const int i1 = IODGetByte( pi_data, pp_data );
    const int i2 = IODGetByte( pi_data, pp_data );
    return( ( i1 << 8 ) | i2 );
}
static int IODGet3Bytes( int *pi_data, uint8_t **pp_data )
{
    const int i1 = IODGetByte( pi_data, pp_data );
    const int i2 = IODGetByte( pi_data, pp_data );
    const int i3 = IODGetByte( pi_data, pp_data );

    return( ( i1 << 16 ) | ( i2 << 8) | i3 );
}

static uint32_t IODGetDWord( int *pi_data, uint8_t **pp_data )
{
    const uint32_t i1 = IODGetWord( pi_data, pp_data );
    const uint32_t i2 = IODGetWord( pi_data, pp_data );
    return( ( i1 << 16 ) | i2 );
}

static char* IODGetURL( int *pi_data, uint8_t **pp_data )
{
    char *url;
    int i_url_len, i;

    i_url_len = IODGetByte( pi_data, pp_data );
    url = malloc( i_url_len + 1 );
    for( i = 0; i < i_url_len; i++ )
    {
        url[i] = IODGetByte( pi_data, pp_data );
    }
    url[i_url_len] = '\0';
    return( url );
}

static iod_descriptor_t *IODNew( int i_data, uint8_t *p_data )
{
    iod_descriptor_t *p_iod;
    int i;
    int i_es_index;
    uint8_t     i_flags;
    vlc_bool_t  b_url;
    int         i_iod_length;

    p_iod = malloc( sizeof( iod_descriptor_t ) );
    memset( p_iod, 0, sizeof( iod_descriptor_t ) );

    fprintf( stderr, "\n************ IOD ************" );
    for( i = 0; i < 255; i++ )
    {
        p_iod->es_descr[i].b_ok = 0;
    }
    i_es_index = 0;

    if( i_data < 3 )
    {
        return p_iod;
    }

    p_iod->i_iod_label = IODGetByte( &i_data, &p_data );
    fprintf( stderr, "\n* iod_label:%d", p_iod->i_iod_label );
    fprintf( stderr, "\n* ===========" );
    fprintf( stderr, "\n* tag:0x%x", p_data[0] );

    if( IODGetByte( &i_data, &p_data ) != 0x02 )
    {
        fprintf( stderr, "\n ERR: tag != 0x02" );
        return p_iod;
    }

    i_iod_length = IODDescriptorLength( &i_data, &p_data );
    fprintf( stderr, "\n* length:%d", i_iod_length );
    if( i_iod_length > i_data )
    {
        i_iod_length = i_data;
    }

    p_iod->i_od_id = ( IODGetByte( &i_data, &p_data ) << 2 );
    i_flags = IODGetByte( &i_data, &p_data );
    p_iod->i_od_id |= i_flags >> 6;
    b_url = ( i_flags >> 5  )&0x01;

    fprintf( stderr, "\n* od_id:%d", p_iod->i_od_id );
    fprintf( stderr, "\n* url flag:%d", b_url );
    fprintf( stderr, "\n* includeInlineProfileLevel flag:%d", ( i_flags >> 4 )&0x01 );

    if( b_url )
    {
        p_iod->psz_url = IODGetURL( &i_data, &p_data );
        fprintf( stderr, "\n* url string:%s", p_iod->psz_url );
        fprintf( stderr, "\n*****************************\n" );
        return p_iod;
    }
    else
    {
        p_iod->psz_url = NULL;
    }

    p_iod->i_ODProfileLevelIndication = IODGetByte( &i_data, &p_data );
    p_iod->i_sceneProfileLevelIndication = IODGetByte( &i_data, &p_data );
    p_iod->i_audioProfileLevelIndication = IODGetByte( &i_data, &p_data );
    p_iod->i_visualProfileLevelIndication = IODGetByte( &i_data, &p_data );
    p_iod->i_graphicsProfileLevelIndication = IODGetByte( &i_data, &p_data );

    fprintf( stderr, "\n* ODProfileLevelIndication:%d", p_iod->i_ODProfileLevelIndication );
    fprintf( stderr, "\n* sceneProfileLevelIndication:%d", p_iod->i_sceneProfileLevelIndication );
    fprintf( stderr, "\n* audioProfileLevelIndication:%d", p_iod->i_audioProfileLevelIndication );
    fprintf( stderr, "\n* visualProfileLevelIndication:%d", p_iod->i_visualProfileLevelIndication );
    fprintf( stderr, "\n* graphicsProfileLevelIndication:%d", p_iod->i_graphicsProfileLevelIndication );


    while( i_data > 0 && i_es_index < 255)
    {
        int i_tag, i_length;
        int     i_data_sav;
        uint8_t *p_data_sav;

        i_tag = IODGetByte( &i_data, &p_data );
        i_length = IODDescriptorLength( &i_data, &p_data );

        i_data_sav = i_data;
        p_data_sav = p_data;

        i_data = i_length;

        switch( i_tag )
        {
            case 0x03:
                {
#define es_descr    p_iod->es_descr[i_es_index]
                    int i_decoderConfigDescr_length;
                    fprintf( stderr, "\n* - ES_Descriptor length:%d", i_length );
                    es_descr.b_ok = 1;

                    es_descr.i_es_id = IODGetWord( &i_data, &p_data );
                    i_flags = IODGetByte( &i_data, &p_data );
                    es_descr.b_streamDependenceFlag = ( i_flags >> 7 )&0x01;
                    b_url = ( i_flags >> 6 )&0x01;
                    es_descr.b_OCRStreamFlag = ( i_flags >> 5 )&0x01;
                    es_descr.i_streamPriority = i_flags & 0x1f;
                    fprintf( stderr, "\n*   * streamDependenceFlag:%d", es_descr.b_streamDependenceFlag );
                    fprintf( stderr, "\n*   * OCRStreamFlag:%d", es_descr.b_OCRStreamFlag );
                    fprintf( stderr, "\n*   * streamPriority:%d", es_descr.i_streamPriority );

                    if( es_descr.b_streamDependenceFlag )
                    {
                        es_descr.i_dependOn_es_id = IODGetWord( &i_data, &p_data );
                        fprintf( stderr, "\n*   * dependOn_es_id:%d", es_descr.i_dependOn_es_id );
                    }

                    if( b_url )
                    {
                        es_descr.psz_url = IODGetURL( &i_data, &p_data );
                        fprintf( stderr, "\n* url string:%s", es_descr.psz_url );
                    }
                    else
                    {
                        es_descr.psz_url = NULL;
                    }

                    if( es_descr.b_OCRStreamFlag )
                    {
                        es_descr.i_OCR_es_id = IODGetWord( &i_data, &p_data );
                        fprintf( stderr, "\n*   * OCR_es_id:%d", es_descr.i_OCR_es_id );
                    }

                    if( IODGetByte( &i_data, &p_data ) != 0x04 )
                    {
                        fprintf( stderr, "\n* ERR missing DecoderConfigDescr" );
                        es_descr.b_ok = 0;
                        break;
                    }
                    i_decoderConfigDescr_length = IODDescriptorLength( &i_data, &p_data );

                    fprintf( stderr, "\n*   - DecoderConfigDesc length:%d", i_decoderConfigDescr_length );
#define dec_descr   es_descr.dec_descr
                    dec_descr.i_objectTypeIndication = IODGetByte( &i_data, &p_data );
                    i_flags = IODGetByte( &i_data, &p_data );
                    dec_descr.i_streamType = i_flags >> 2;
                    dec_descr.b_upStream = ( i_flags >> 1 )&0x01;
                    dec_descr.i_bufferSizeDB = IODGet3Bytes( &i_data, &p_data );
                    dec_descr.i_maxBitrate = IODGetDWord( &i_data, &p_data );
                    dec_descr.i_avgBitrate = IODGetDWord( &i_data, &p_data );
                    fprintf( stderr, "\n*     * objectTypeIndication:0x%x", dec_descr.i_objectTypeIndication  );
                    fprintf( stderr, "\n*     * streamType:0x%x", dec_descr.i_streamType );
                    fprintf( stderr, "\n*     * upStream:%d", dec_descr.b_upStream );
                    fprintf( stderr, "\n*     * bufferSizeDB:%d", dec_descr.i_bufferSizeDB );
                    fprintf( stderr, "\n*     * maxBitrate:%d", dec_descr.i_maxBitrate );
                    fprintf( stderr, "\n*     * avgBitrate:%d", dec_descr.i_avgBitrate );
                    if( i_decoderConfigDescr_length > 13 && IODGetByte( &i_data, &p_data ) == 0x05 )
                    {
                        int i;
                        dec_descr.i_decoder_specific_info_len =
                            IODDescriptorLength( &i_data, &p_data );
                        if( dec_descr.i_decoder_specific_info_len > 0 )
                        {
                            dec_descr.p_decoder_specific_info =
                                malloc( dec_descr.i_decoder_specific_info_len );
                        }
                        for( i = 0; i < dec_descr.i_decoder_specific_info_len; i++ )
                        {
                            dec_descr.p_decoder_specific_info[i] = IODGetByte( &i_data, &p_data );
                        }
                    }
                    else
                    {
                        dec_descr.i_decoder_specific_info_len = 0;
                        dec_descr.p_decoder_specific_info = NULL;
                    }
                }
#undef  dec_descr
#define sl_descr    es_descr.sl_descr
                {
                    int i_SLConfigDescr_length;
                    int i_predefined;

                    if( IODGetByte( &i_data, &p_data ) != 0x06 )
                    {
                        fprintf( stderr, "\n* ERR missing SLConfigDescr" );
                        es_descr.b_ok = 0;
                        break;
                    }
                    i_SLConfigDescr_length = IODDescriptorLength( &i_data, &p_data );

                    fprintf( stderr, "\n*   - SLConfigDescr length:%d", i_SLConfigDescr_length );
                    i_predefined = IODGetByte( &i_data, &p_data );
                    fprintf( stderr, "\n*     * i_predefined:0x%x", i_predefined  );
                    switch( i_predefined )
                    {
                        case 0x01:
                            {
                                sl_descr.b_useAccessUnitStartFlag   = 0;
                                sl_descr.b_useAccessUnitEndFlag     = 0;
                                sl_descr.b_useRandomAccessPointFlag = 0;
                                //sl_descr.b_useRandomAccessUnitsOnlyFlag = 0;
                                sl_descr.b_usePaddingFlag           = 0;
                                sl_descr.b_useTimeStampsFlags       = 0;
                                sl_descr.b_useIdleFlag              = 0;
                                sl_descr.b_durationFlag     = 0;    // FIXME FIXME
                                sl_descr.i_timeStampResolution      = 1000;
                                sl_descr.i_OCRResolution    = 0;    // FIXME FIXME
                                sl_descr.i_timeStampLength          = 32;
                                sl_descr.i_OCRLength        = 0;    // FIXME FIXME
                                sl_descr.i_AU_Length                = 0;
                                sl_descr.i_instantBitrateLength= 0; // FIXME FIXME
                                sl_descr.i_degradationPriorityLength= 0;
                                sl_descr.i_AU_seqNumLength          = 0;
                                sl_descr.i_packetSeqNumLength       = 0;
                                if( sl_descr.b_durationFlag )
                                {
                                    sl_descr.i_timeScale            = 0;    // FIXME FIXME
                                    sl_descr.i_accessUnitDuration   = 0;    // FIXME FIXME
                                    sl_descr.i_compositionUnitDuration= 0;    // FIXME FIXME
                                }
                                if( !sl_descr.b_useTimeStampsFlags )
                                {
                                    sl_descr.i_startDecodingTimeStamp   = 0;    // FIXME FIXME
                                    sl_descr.i_startCompositionTimeStamp= 0;    // FIXME FIXME
                                }
                            }
                            break;
                        default:
                            fprintf( stderr, "\n* ERR unsupported SLConfigDescr predefined" );
                            es_descr.b_ok = 0;
                            break;
                    }
                }
                break;
#undef  sl_descr
#undef  es_descr
            default:
                fprintf( stderr, "\n* - OD tag:0x%x length:%d (Unsupported)", i_tag, i_length );
                break;
        }

        p_data = p_data_sav + i_length;
        i_data = i_data_sav - i_length;
        i_es_index++;
    }


    fprintf( stderr, "\n*****************************\n" );
    return p_iod;
}

static void IODFree( iod_descriptor_t *p_iod )
{
    int i;

    if( p_iod->psz_url )
    {
        free( p_iod->psz_url );
        p_iod->psz_url = NULL;
        free( p_iod );
        return;
    }

    for( i = 0; i < 255; i++ )
    {
#define es_descr p_iod->es_descr[i]
        if( es_descr.b_ok )
        {
            if( es_descr.psz_url )
            {
                free( es_descr.psz_url );
                es_descr.psz_url = NULL;
            }
            else
            {
                if( es_descr.dec_descr.p_decoder_specific_info != NULL )
                {
                    free( es_descr.dec_descr.p_decoder_specific_info );
                    es_descr.dec_descr.p_decoder_specific_info = NULL;
                    es_descr.dec_descr.i_decoder_specific_info_len = 0;
                }
            }
        }
        es_descr.b_ok = 0;
#undef  es_descr
    }
    free( p_iod );
}

/****************************************************************************
 ****************************************************************************
 ** libdvbpsi callbacks
 ****************************************************************************
 ****************************************************************************/
static void PMTCallBack( demux_t *p_demux, dvbpsi_pmt_t *p_pmt )
{
    demux_sys_t          *p_sys = p_demux->p_sys;
    dvbpsi_descriptor_t  *p_dr;
    dvbpsi_pmt_es_t      *p_es;

    ts_pid_t             *pmt = NULL;
    int                  i;

    msg_Dbg( p_demux, "PMTCallBack called" );
    /* First find this PMT declared in PAT */
    for( i = 0; i < p_sys->i_pmt; i++ )
    {
        if( p_sys->pmt[i]->psi->i_number == p_pmt->i_program_number )
        {
            pmt = p_sys->pmt[i];
        }
    }
    if( pmt == NULL )
    {
        msg_Warn( p_demux, "unreferenced program (broken stream)" );
        dvbpsi_DeletePMT(p_pmt);
        return;
    }

    if( pmt->psi->i_version != -1 && ( !p_pmt->b_current_next || pmt->psi->i_version == p_pmt->i_version ) )
    {
        dvbpsi_DeletePMT( p_pmt );
        return;
    }

    /* Clean this program (remove all es) */
    for( i = 0; i < 8192; i++ )
    {
        ts_pid_t *pid = &p_sys->pid[i];

        if( pid->b_valid && pid->p_owner == pmt->psi && pid->psi == NULL )
        {
            PIDClean( p_demux->out, pid );
        }
    }
    if( pmt->psi->iod )
    {
        IODFree( pmt->psi->iod );
        pmt->psi->iod = NULL;
    }

    msg_Dbg( p_demux, "new PMT program number=%d version=%d pid_pcr=0x%x", p_pmt->i_program_number, p_pmt->i_version, p_pmt->i_pcr_pid );
    pmt->psi->i_pid_pcr = p_pmt->i_pcr_pid;
    pmt->psi->i_version = p_pmt->i_version;

    /* Parse descriptor */
    for( p_dr = p_pmt->p_first_descriptor; p_dr != NULL; p_dr = p_dr->p_next )
    {
        if( p_dr->i_tag == 0x1d )
        {
            /* We have found an IOD descriptor */
            msg_Warn( p_demux, "found IOD descriptor" );

            pmt->psi->iod = IODNew( p_dr->i_length, p_dr->p_data );
        }
    }

    for( p_es = p_pmt->p_first_es; p_es != NULL; p_es = p_es->p_next )
    {
        ts_pid_t *pid = &p_sys->pid[p_es->i_pid];

        if( pid->b_valid )
        {
            msg_Warn( p_demux, "pmt error: pid=0x%x already defined", p_es->i_pid );
            continue;
        }

        PIDInit( pid, VLC_FALSE, pmt->psi );
        PIDFillFormat( pid, p_es->i_type );

        if( p_es->i_type == 0x10 || p_es->i_type == 0x11 )
        {
            /* MPEG-4 stream: search SL_DESCRIPTOR */
            dvbpsi_descriptor_t *p_dr = p_es->p_first_descriptor;;

            while( p_dr && ( p_dr->i_tag != 0x1f ) ) p_dr = p_dr->p_next;

            if( p_dr && p_dr->i_length == 2 )
            {
                int i;
                int i_es_id = ( p_dr->p_data[0] << 8 ) | p_dr->p_data[1];

                msg_Warn( p_demux, "found SL_descriptor es_id=%d", i_es_id );

                pid->es->p_mpeg4desc = NULL;

                for( i = 0; i < 255; i++ )
                {
                    iod_descriptor_t *iod = pmt->psi->iod;

                    if( iod->es_descr[i].b_ok &&
                        iod->es_descr[i].i_es_id == i_es_id )
                    {
                        pid->es->p_mpeg4desc = &iod->es_descr[i];
                        break;
                    }
                }
            }

            if( pid->es->p_mpeg4desc != NULL )
            {
                decoder_config_descriptor_t *dcd = &pid->es->p_mpeg4desc->dec_descr;

                if( dcd->i_streamType == 0x04 )    /* VisualStream */
                {
                    pid->es->fmt.i_cat = VIDEO_ES;
                    switch( dcd->i_objectTypeIndication )
                    {
                        case 0x20:
                            pid->es->fmt.i_codec = VLC_FOURCC('m','p','4','v');    // mpeg4
                            break;
                        case 0x60:
                        case 0x61:
                        case 0x62:
                        case 0x63:
                        case 0x64:
                        case 0x65:
                            pid->es->fmt.i_codec = VLC_FOURCC( 'm','p','g','v' );  // mpeg2
                            break;
                        case 0x6a:
                            pid->es->fmt.i_codec = VLC_FOURCC( 'm','p','g','v' );  // mpeg1
                            break;
                        case 0x6c:
                            pid->es->fmt.i_codec = VLC_FOURCC( 'j','p','e','g' );  // mpeg1
                            break;

                        default:
                            pid->es->fmt.i_cat = UNKNOWN_ES;
                            break;
                    }
                }
                else if( dcd->i_streamType == 0x05 )    /* AudioStream */
                {
                    pid->es->fmt.i_cat = AUDIO_ES;
                    switch( dcd->i_objectTypeIndication )
                    {
                        case 0x40:
                            pid->es->fmt.i_codec = VLC_FOURCC('m','p','4','a');    // mpeg4
                            break;
                        case 0x66:
                        case 0x67:
                        case 0x68:
                            pid->es->fmt.i_codec = VLC_FOURCC('m','p','4','a');// mpeg2 aac
                            break;
                        case 0x69:
                            pid->es->fmt.i_codec = VLC_FOURCC('m','p','g','a');    // mpeg2
                            break;
                        case 0x6b:
                            pid->es->fmt.i_codec = VLC_FOURCC('m','p','g','a');    // mpeg1
                            break;
                        default:
                            pid->es->fmt.i_cat = UNKNOWN_ES;
                            break;
                    }
                }
                else
                {
                    pid->es->fmt.i_cat = UNKNOWN_ES;
                }

                if( pid->es->fmt.i_cat != UNKNOWN_ES )
                {
                    pid->es->fmt.i_extra = dcd->i_decoder_specific_info_len;
                    if( pid->es->fmt.i_extra > 0 )
                    {
                        pid->es->fmt.p_extra = malloc( pid->es->fmt.i_extra );
                        memcpy( pid->es->fmt.p_extra,
                                dcd->p_decoder_specific_info,
                                pid->es->fmt.i_extra );
                    }
                }
            }
        }
        else if( p_es->i_type == 0x06 )
        {
            dvbpsi_descriptor_t *p_dr;

            for( p_dr = p_es->p_first_descriptor; p_dr != NULL; p_dr = p_dr->p_next )
            {
                if( p_dr->i_tag == 0x6a )
                {
                    pid->es->fmt.i_cat = AUDIO_ES;
                    pid->es->fmt.i_codec = VLC_FOURCC( 'a', '5', '2', ' ' );
                }
#ifdef _DVBPSI_DR_59_H_
                else if( p_dr->i_tag == 0x59 )
                {
                    /* DVB subtitle */
                    /* TODO */
                }
#endif
            }
        }
        else if( p_es->i_type == 0xa0 )
        {
            /* MSCODEC send by vlc */
            dvbpsi_descriptor_t *p_dr = p_es->p_first_descriptor;;

            while( p_dr && ( p_dr->i_tag != 0xa0 ) ) p_dr = p_dr->p_next;

            if( p_dr && p_dr->i_length >= 8 )
            {
                pid->es->fmt.i_cat = VIDEO_ES;
                pid->es->fmt.i_codec = VLC_FOURCC( p_dr->p_data[0], p_dr->p_data[1],
                                                   p_dr->p_data[2], p_dr->p_data[3] );
                pid->es->fmt.video.i_width = ( p_dr->p_data[4] << 8 )|p_dr->p_data[5];
                pid->es->fmt.video.i_height= ( p_dr->p_data[6] << 8 )|p_dr->p_data[7];
                pid->es->fmt.i_extra = (p_dr->p_data[8] << 8) | p_dr->p_data[9];

                if( pid->es->fmt.i_extra > 0 )
                {
                    pid->es->fmt.p_extra = malloc( pid->es->fmt.i_extra );
                    memcpy( pid->es->fmt.p_extra, &p_dr->p_data[10], pid->es->fmt.i_extra );
                }
            }
            else
            {
                msg_Warn( p_demux, "private MSCODEC (vlc) without bih private descriptor" );
            }
        }

        if( pid->es->fmt.i_cat == AUDIO_ES || pid->es->fmt.i_cat == SPU_ES )
        {
            /* get language descriptor */
            dvbpsi_descriptor_t *p_dr = p_es->p_first_descriptor;
            while( p_dr && ( p_dr->i_tag != 0x0a ) ) p_dr = p_dr->p_next;

            if( p_dr )
            {
                dvbpsi_iso639_dr_t *p_decoded = dvbpsi_DecodeISO639Dr( p_dr );

                if( p_decoded )
                {
                    pid->es->fmt.psz_language = malloc( 4 );
                    memcpy( pid->es->fmt.psz_language, p_decoded->i_iso_639_code, 3 );
                    pid->es->fmt.psz_language[3] = 0;
                }
            }
        }

        pid->es->fmt.i_group = p_pmt->i_program_number;
        if( pid->es->fmt.i_cat == UNKNOWN_ES )
        {
            msg_Dbg( p_demux, "  * es pid=0x%x type=0x%x *unknown*", p_es->i_pid, p_es->i_type );
        }
        else if( !p_sys->b_udp_out )
        {
            msg_Dbg( p_demux, "  * es pid=0x%x type=0x%x fcc=%4.4s", p_es->i_pid, p_es->i_type, (char*)&pid->es->fmt.i_codec );
            if( p_sys->b_es_id_pid )
            {
                pid->es->fmt.i_id = p_es->i_pid;
            }
            pid->es->id = es_out_Add( p_demux->out, &pid->es->fmt );
        }
    }
    dvbpsi_DeletePMT(p_pmt);
}

static void PATCallBack( demux_t *p_demux, dvbpsi_pat_t *p_pat )
{
    demux_sys_t          *p_sys = p_demux->p_sys;
    dvbpsi_pat_program_t *p_program;
    ts_pid_t             *pat = &p_sys->pid[0];
    int                  i;

    msg_Dbg( p_demux, "PATCallBack called" );

    if( pat->psi->i_version != -1 && ( !p_pat->b_current_next || p_pat->i_version == pat->psi->i_version ) )
    {
        dvbpsi_DeletePAT( p_pat );
        return;
    }

    msg_Dbg( p_demux, "new PAT ts_id=0x%x version=%d current_next=%d", p_pat->i_ts_id, p_pat->i_version, p_pat->b_current_next );

    /* Clean old */
    for( i = 2; i < 8192; i++ )
    {
        ts_pid_t *pid = &p_sys->pid[i];

        if( pid->b_valid )
        {
            if( pid->psi )
            {
                if( pid->p_owner == pat->psi )
                {
                    PIDClean( p_demux->out, pid );
                    TAB_REMOVE( p_sys->i_pmt, p_sys->pmt, pid );
                }
            }
            else if( pid->p_owner && pid->p_owner->i_number != 0 && pid->es->id )
            {
                /* We only remove es that aren't defined by extra pmt */
                PIDClean( p_demux->out, pid );
            }
        }
    }

    /* now create programs */
    for( p_program = p_pat->p_first_program; p_program != NULL; p_program = p_program->p_next )
    {
        msg_Dbg( p_demux, "  * number=%d pid=0x%x", p_program->i_number, p_program->i_pid );
        if( p_program->i_number != 0 )
        {
            ts_pid_t *pmt = &p_sys->pid[p_program->i_pid];

            PIDInit( pmt, VLC_TRUE, pat->psi );
            pmt->psi->handle = dvbpsi_AttachPMT( p_program->i_number, (dvbpsi_pmt_callback)PMTCallBack, p_demux );
            pmt->psi->i_number = p_program->i_number;

            TAB_APPEND( p_sys->i_pmt, p_sys->pmt, pmt );
        }
    }
    pat->psi->i_version = p_pat->i_version;

    dvbpsi_DeletePAT( p_pat );
}

