/*****************************************************************************
 * ts.c: Transport Stream input module for VLC.
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id$
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
static int  Open  ( vlc_object_t * );
static void Close ( vlc_object_t * );

#define PMT_TEXT N_("Extra PMT")
#define PMT_LONGTEXT N_( \
  "Allows a user to specify an extra pmt (pmt_pid=pid:stream_type[,...])" )

#define PID_TEXT N_("Set id of ES to PID")
#define PID_LONGTEXT N_("set id of es to pid")

#define TSOUT_TEXT N_("Fast udp streaming")
#define TSOUT_LONGTEXT N_( \
  "Sends TS to specific ip:port by udp (you must know what you are doing)")

#define MTUOUT_TEXT N_("MTU for out mode")
#define MTUOUT_LONGTEXT N_("MTU for out mode")

#define CSA_TEXT N_("CSA ck")
#define CSA_LONGTEXT N_("CSA ck")

#define SILENT_TEXT N_("Silent mode")
#define SILENT_LONGTEXT N_("do not complain on encrypted PES")

#define CAPMT_SYSID_TEXT N_("CAPMT System ID")
#define CAPMT_SYSID_LONGTEXT N_("only forward descriptors from this SysID to the CAM")

vlc_module_begin();
    set_description( _("MPEG Transport Stream demuxer") );
    set_shortname ( _("MPEG-TS") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );

    add_string( "ts-extra-pmt", NULL, NULL, PMT_TEXT, PMT_LONGTEXT, VLC_TRUE );
    add_bool( "ts-es-id-pid", 0, NULL, PID_TEXT, PID_LONGTEXT, VLC_TRUE );
    add_string( "ts-out", NULL, NULL, TSOUT_TEXT, TSOUT_LONGTEXT, VLC_TRUE );
    add_integer( "ts-out-mtu", 1500, NULL, MTUOUT_TEXT,
                 MTUOUT_LONGTEXT, VLC_TRUE );
    add_string( "ts-csa-ck", NULL, NULL, CSA_TEXT, CSA_LONGTEXT, VLC_TRUE );
    add_bool( "ts-silent", 0, NULL, SILENT_TEXT, SILENT_LONGTEXT, VLC_TRUE );
    add_integer( "ts-capmt-sysid", 0, NULL, CAPMT_SYSID_TEXT,
                 CAPMT_SYSID_LONGTEXT, VLC_TRUE );

    set_capability( "demux2", 10 );
    set_callbacks( Open, Close );
    add_shortcut( "ts" );
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
    dvbpsi_handle   handle;

    int             i_version;
    int             i_number;
    int             i_pid_pcr;
    int             i_pid_pmt;
    /* IOD stuff (mpeg4) */
    iod_descriptor_t *iod;

} ts_prg_psi_t;

typedef struct
{
    /* for special PAT case */
    dvbpsi_handle   handle;
    int             i_pat_version;

    /* For PMT */
    int             i_prg;
    ts_prg_psi_t    **prg;

} ts_psi_t;

typedef struct
{
    es_format_t  fmt;
    es_out_id_t *id;
    int         i_pes_size;
    int         i_pes_gathered;
    block_t     *p_pes;
    block_t     **pp_last;

    es_mpeg4_descriptor_t *p_mpeg4desc;
    int         b_gather;

} ts_es_t;

typedef struct
{
    int         i_pid;

    vlc_bool_t  b_seen;
    vlc_bool_t  b_valid;
    int         i_cc;   /* countinuity counter */

    /* PSI owner (ie PMT -> PAT, ES -> PMT */
    ts_psi_t   *p_owner;
    int         i_owner_number;

    /* */
    ts_psi_t    *psi;
    ts_es_t     *es;

    /* Some private streams encapsulate several ES (eg. DVB subtitles)*/
    ts_es_t     **extra_es;
    int         i_extra_es;

} ts_pid_t;

struct demux_sys_t
{
    /* TS packet size (188, 192, 204) */
    int         i_packet_size;

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
    vlc_bool_t  b_silent;
    uint16_t    i_capmt_sysid;

    vlc_bool_t  b_udp_out;
    int         fd; /* udp socket */
    uint8_t     *buffer;

    vlc_bool_t  b_dvb_control;
    int         i_dvb_program;
    vlc_list_t  *p_programs_list;
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

static void PCRHandle( demux_t *p_demux, ts_pid_t *, block_t * );

static iod_descriptor_t *IODNew( int , uint8_t * );
static void              IODFree( iod_descriptor_t * );

#define TS_PACKET_SIZE_188 188
#define TS_PACKET_SIZE_192 192
#define TS_PACKET_SIZE_204 204
#define TS_PACKET_SIZE_MAX 204

/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    uint8_t     *p_peek;
    int          i_sync, i_peek, i;
    int          i_packet_size;

    ts_pid_t     *pat;

    vlc_value_t  val;

    if( stream_Peek( p_demux->s, &p_peek, TS_PACKET_SIZE_MAX ) <
        TS_PACKET_SIZE_MAX ) return VLC_EGENERIC;

    /* Search first sync byte */
    for( i_sync = 0; i_sync < TS_PACKET_SIZE_MAX; i_sync++ )
    {
        if( p_peek[i_sync] == 0x47 ) break;
    }
    if( i_sync >= TS_PACKET_SIZE_MAX )
    {
        if( strcmp( p_demux->psz_demux, "ts" ) ) return VLC_EGENERIC;
        msg_Warn( p_demux, "this does not look like a TS stream, continuing" );
    }

    /* Check next 3 sync bytes */
    i_peek = TS_PACKET_SIZE_MAX * 3 + i_sync + 1;
    if( ( stream_Peek( p_demux->s, &p_peek, i_peek ) ) < i_peek )
    {
        msg_Err( p_demux, "cannot peek" );
        return VLC_EGENERIC;
    }
    if( p_peek[i_sync + TS_PACKET_SIZE_188] == 0x47 &&
        p_peek[i_sync + 2 * TS_PACKET_SIZE_188] == 0x47 &&
        p_peek[i_sync + 3 * TS_PACKET_SIZE_188] == 0x47 )
    {
        i_packet_size = TS_PACKET_SIZE_188;
    }
    else if( p_peek[i_sync + TS_PACKET_SIZE_192] == 0x47 &&
             p_peek[i_sync + 2 * TS_PACKET_SIZE_192] == 0x47 &&
             p_peek[i_sync + 3 * TS_PACKET_SIZE_192] == 0x47 )
    {
        i_packet_size = TS_PACKET_SIZE_192;
    }
    else if( p_peek[i_sync + TS_PACKET_SIZE_204] == 0x47 &&
             p_peek[i_sync + 2 * TS_PACKET_SIZE_204] == 0x47 &&
             p_peek[i_sync + 3 * TS_PACKET_SIZE_204] == 0x47 )
    {
        i_packet_size = TS_PACKET_SIZE_204;
    }
    else if( !strcmp( p_demux->psz_demux, "ts" ) )
    {
        i_packet_size = TS_PACKET_SIZE_188;
    }
    else
    {
        msg_Warn( p_demux, "TS module discarded (lost sync)" );
        return VLC_EGENERIC;
    }

    /* Fill p_demux field */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    memset( p_sys, 0, sizeof( demux_sys_t ) );

    /* Init p_sys field */
    p_sys->b_dvb_control = VLC_TRUE;
    p_sys->i_dvb_program = 0;
    for( i = 0; i < 8192; i++ )
    {
        ts_pid_t *pid = &p_sys->pid[i];

        pid->i_pid      = i;
        pid->b_seen     = VLC_FALSE;
        pid->b_valid    = VLC_FALSE;
    }
    p_sys->i_packet_size = i_packet_size;
    p_sys->b_udp_out = VLC_FALSE;
    p_sys->i_ts_read = 50;
    p_sys->csa = NULL;

    /* Init PAT handler */
    pat = &p_sys->pid[0];
    PIDInit( pat, VLC_TRUE, NULL );
    pat->psi->handle = dvbpsi_AttachPAT( (dvbpsi_pat_callback)PATCallBack,
                                         p_demux );

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
            var_Create( p_demux, "ts-out-mtu",
                        VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
            var_Get( p_demux, "ts-out-mtu", &mtu );
            p_sys->i_ts_read = mtu.i_int / p_sys->i_packet_size;
            if( p_sys->i_ts_read <= 0 )
            {
                p_sys->i_ts_read = 1500 / p_sys->i_packet_size;
            }
            p_sys->buffer = malloc( p_sys->i_packet_size * p_sys->i_ts_read );
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

            msg_Dbg( p_demux, "extra pmt specified (pid=%d)", i_pid );
            PIDInit( pmt, VLC_TRUE, NULL );
            pmt->psi->i_prg = 1;
            pmt->psi->prg = malloc( sizeof(ts_prg_psi_t) );
            /* FIXME we should also ask for a number */
            pmt->psi->prg[0]->handle =
                dvbpsi_AttachPMT( 1, (dvbpsi_pmt_callback)PMTCallBack,
                                  p_demux );
            pmt->psi->prg[0]->i_number = 0; /* special one */

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
                    if( i_pid >= 2 && i_pid < 8192 &&
                        !p_sys->pid[i_pid].b_valid )
                    {
                        ts_pid_t *pid = &p_sys->pid[i_pid];

                        PIDInit( pid, VLC_FALSE, pmt->psi);
                        if( pmt->psi->prg[0]->i_pid_pcr <= 0 )
                        {
                            pmt->psi->prg[0]->i_pid_pcr = i_pid;
                        }
                        PIDFillFormat( pid, i_stream_type);
                        if( pid->es->fmt.i_cat != UNKNOWN_ES )
                        {
                            if( p_sys->b_es_id_pid )
                            {
                                pid->es->fmt.i_id = i_pid;
                            }
                            msg_Dbg( p_demux, "  * es pid=%d type=%d "
                                     "fcc=%4.4s", i_pid, i_stream_type,
                                     (char*)&pid->es->fmt.i_codec );
                            pid->es->id = es_out_Add( p_demux->out,
                                                      &pid->es->fmt );
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
#ifndef UNDER_CE
            uint64_t i_ck = strtoull( psz, NULL, 16 );
#else
            uint64_t i_ck = strtoll( psz, NULL, 16 );
#endif
            uint8_t ck[8];
            int     i;
            for( i = 0; i < 8; i++ )
            {
                ck[i] = ( i_ck >> ( 56 - 8*i) )&0xff;
            }

            msg_Dbg( p_demux, "using CSA scrambling with "
                     "ck=%x:%x:%x:%x:%x:%x:%x:%x",
                     ck[0], ck[1], ck[2], ck[3], ck[4], ck[5], ck[6], ck[7] );

            p_sys->csa = csa_New();
            csa_SetCW( p_sys->csa, ck, ck );
        }
    }
    if( val.psz_string )
    {
        free( val.psz_string );
    }

    var_Create( p_demux, "ts-silent", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "ts-silent", &val );
    p_sys->b_silent = val.b_bool;

    var_Create( p_demux, "ts-capmt-sysid", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "ts-capmt-sysid", &val );
    p_sys->i_capmt_sysid = val.i_int;

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
                    free( pid->psi );
                    break;
                case 1: /* CAT */
                    free( pid->psi );
                    break;
                default:
                    PIDClean( p_demux->out, pid );
                    break;
            }
        }
        else if( pid->b_valid && pid->es )
        {
            PIDClean( p_demux->out, pid );
        }

        if( pid->b_seen )
        {
            msg_Dbg( p_demux, "  - pid[%d] seen", pid->i_pid );
        }

        if( p_sys->b_dvb_control && pid->i_pid > 0 )
        {
            /* too much */
            stream_Control( p_demux->s, STREAM_CONTROL_ACCESS, ACCESS_SET_PRIVATE_ID_STATE, pid->i_pid, VLC_FALSE );
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

    if( p_sys->i_pmt ) free( p_sys->pmt );

    if ( p_sys->p_programs_list )
    {
        vlc_value_t val;
        val.p_list = p_sys->p_programs_list;
        var_Change( p_demux, "programs", VLC_VAR_FREELIST, &val, NULL );
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
        if( !( p_pkt = stream_Block( p_demux->s, p_sys->i_packet_size ) ) )
        {
            msg_Dbg( p_demux, "eof ?" );
            return 0;
        }

        /* Check sync byte and re-sync if needed */
        if( p_pkt->p_buffer[0] != 0x47 )
        {
            msg_Warn( p_demux, "lost synchro" );
            block_Release( p_pkt );

            while( !p_demux->b_die )
            {
                uint8_t *p_peek;
                int i_peek, i_skip = 0;

                i_peek = stream_Peek( p_demux->s, &p_peek,
                                      p_sys->i_packet_size * 10 );
                if( i_peek < p_sys->i_packet_size + 1 )
                {
                    msg_Dbg( p_demux, "eof ?" );
                    return 0;
                }

                while( i_skip < i_peek - p_sys->i_packet_size )
                {
                    if( p_peek[i_skip] == 0x47 &&
                        p_peek[i_skip + p_sys->i_packet_size] == 0x47 )
                    {
                        break;
                    }
                    i_skip++;
                }

                msg_Dbg( p_demux, "skipping %d bytes of garbage", i_skip );
                stream_Read( p_demux->s, NULL, i_skip );

                if( i_skip < i_peek - p_sys->i_packet_size )
                {
                    break;
                }
            }

            if( !( p_pkt = stream_Block( p_demux->s, p_sys->i_packet_size ) ) )
            {
                msg_Dbg( p_demux, "eof ?" );
                return 0;
            }
        }

        if( p_sys->b_udp_out )
        {
            memcpy( &p_sys->buffer[i_pkt * p_sys->i_packet_size],
                    p_pkt->p_buffer, p_sys->i_packet_size );
        }

        /* Parse the TS packet */
        p_pid = &p_sys->pid[PIDGet( p_pkt )];

        if( p_pid->b_valid )
        {
            if( p_pid->psi )
            {
                if( p_pid->i_pid == 0 )
                {
                    dvbpsi_PushPacket( p_pid->psi->handle, p_pkt->p_buffer );
                }
                else
                {
                    int i_prg;
                    for( i_prg = 0; i_prg < p_pid->psi->i_prg; i_prg++ )
                    {
                        dvbpsi_PushPacket( p_pid->psi->prg[i_prg]->handle,
                                           p_pkt->p_buffer );
                    }
                }
                block_Release( p_pkt );
            }
            else if( !p_sys->b_udp_out )
            {
                b_frame = GatherPES( p_demux, p_pid, p_pkt );
            }
            else
            {
                PCRHandle( p_demux, p_pid, p_pkt );
                block_Release( p_pkt );
            }
        }
        else
        {
            if( !p_pid->b_seen )
            {
                msg_Dbg( p_demux, "pid[%d] unknown", p_pid->i_pid );
            }
            /* We have to handle PCR if present */
            PCRHandle( p_demux, p_pid, p_pkt );
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
        net_Write( p_demux, p_sys->fd, NULL, p_sys->buffer,
                   p_sys->i_ts_read * p_sys->i_packet_size );
    }

    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double f, *pf;
    int64_t i64;
    int i_int;

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

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->i_mux_rate > 0 )
            {
                *pi64 = I64C(1000000) * ( stream_Size( p_demux->s ) / 50 ) /
                        p_sys->i_mux_rate;
                return VLC_SUCCESS;
            }
            *pi64 = 0;
            return VLC_EGENERIC;
#endif
        case DEMUX_SET_GROUP:
        {
            uint16_t i_vpid = 0, i_apid1 = 0, i_apid2 = 0, i_apid3 = 0;
            ts_prg_psi_t *p_prg = NULL;
            vlc_list_t *p_list;

            i_int = (int)va_arg( args, int );
            p_list = (vlc_list_t *)va_arg( args, vlc_list_t * );
            msg_Dbg( p_demux, "DEMUX_SET_GROUP %d %p", i_int, p_list );

            if( p_sys->b_dvb_control && i_int > 0 && i_int != p_sys->i_dvb_program )
            {
                int i_pmt_pid = -1;
                int i;

                /* Search pmt to be unselected */
                for( i = 0; i < p_sys->i_pmt; i++ )
                {
                    ts_pid_t *pmt = p_sys->pmt[i];
                    int i_prg;

                    for( i_prg = 0; i_prg < pmt->psi->i_prg; i_prg++ )
                    {
                        if( pmt->psi->prg[i_prg]->i_number == p_sys->i_dvb_program )
                        {
                            i_pmt_pid = p_sys->pmt[i]->i_pid;
                            break;
                        }
                    }
                    if( i_pmt_pid > 0 ) break;
                }

                if( i_pmt_pid > 0 )
                {
                    stream_Control( p_demux->s, STREAM_CONTROL_ACCESS,
                                    ACCESS_SET_PRIVATE_ID_STATE, i_pmt_pid,
                                    VLC_FALSE );
                    /* All ES */
                    for( i = 2; i < 8192; i++ )
                    {
                        ts_pid_t *pid = &p_sys->pid[i];
                        int i_prg;

                        if( !pid->b_valid || pid->psi ) continue;

                        for( i_prg = 0; i_prg < pid->p_owner->i_prg; i_prg++ )
                        {
                            if( pid->p_owner->prg[i_prg]->i_pid_pmt == i_pmt_pid && pid->es->id )
                            {
                                /* We only remove es that aren't defined by extra pmt */
                                stream_Control( p_demux->s,
                                                STREAM_CONTROL_ACCESS,
                                                ACCESS_SET_PRIVATE_ID_STATE,
                                                i, VLC_FALSE );
                                break;
                            }
                        }
                    }
                }

                /* select new program */
                p_sys->i_dvb_program = i_int;
                i_pmt_pid = -1;
                for( i = 0; i < p_sys->i_pmt; i++ )
                {
                    ts_pid_t *pmt = p_sys->pmt[i];
                    int i_prg;

                    for( i_prg = 0; i_prg < pmt->psi->i_prg; i_prg++ )
                    {
                        if( pmt->psi->prg[i_prg]->i_number == i_int )
                        {
                            i_pmt_pid = p_sys->pmt[i]->i_pid;
                            p_prg = p_sys->pmt[i]->psi->prg[i_prg];
                            break;
                        }
                    }
                    if( i_pmt_pid > 0 ) break;
                }
                if( i_pmt_pid > 0 )
                {
                    stream_Control( p_demux->s, STREAM_CONTROL_ACCESS,
                                    ACCESS_SET_PRIVATE_ID_STATE, i_pmt_pid,
                                    VLC_TRUE );
                    stream_Control( p_demux->s, STREAM_CONTROL_ACCESS,
                                    ACCESS_SET_PRIVATE_ID_STATE, p_prg->i_pid_pcr,
                                    VLC_TRUE );

                    for( i = 2; i < 8192; i++ )
                    {
                        ts_pid_t *pid = &p_sys->pid[i];
                        int i_prg;

                        if( !pid->b_valid || pid->psi ) continue;

                        for( i_prg = 0; i_prg < pid->p_owner->i_prg; i_prg++ )
                        {
                            if( pid->p_owner->prg[i_prg]->i_pid_pmt == i_pmt_pid && pid->es->id )
                            {
                                if ( pid->es->fmt.i_cat == VIDEO_ES && !i_vpid )
                                    i_vpid = i;
                                if ( pid->es->fmt.i_cat == AUDIO_ES && !i_apid1 )
                                    i_apid1 = i;
                                else if ( pid->es->fmt.i_cat == AUDIO_ES && !i_apid2 )
                                    i_apid2 = i;
                                else if ( pid->es->fmt.i_cat == AUDIO_ES && !i_apid3 )
                                    i_apid3 = i;

                                stream_Control( p_demux->s,
                                                STREAM_CONTROL_ACCESS,
                                                ACCESS_SET_PRIVATE_ID_STATE,
                                                i, VLC_TRUE );
                                break;
                            }
                        }
                    }
                }
            }
            else
            {
                p_sys->i_dvb_program = -1;
                p_sys->p_programs_list = p_list;
            }
            return VLC_SUCCESS;
        }

        case DEMUX_GET_FPS:
        case DEMUX_SET_TIME:
        default:
            return VLC_EGENERIC;
    }
}

static void PIDInit( ts_pid_t *pid, vlc_bool_t b_psi, ts_psi_t *p_owner )
{
    vlc_bool_t b_old_valid = pid->b_valid;

    pid->b_valid    = VLC_TRUE;
    pid->i_cc       = 0xff;
    pid->p_owner    = p_owner;
    pid->i_owner_number = 0;

    pid->extra_es   = NULL;
    pid->i_extra_es = 0;

    if( b_psi )
    {
        pid->es  = NULL;

        if( !b_old_valid )
        {
            pid->psi = malloc( sizeof( ts_psi_t ) );
            pid->psi->i_prg = 0;
            pid->psi->prg   = NULL;
            pid->psi->handle= NULL;
        }
        pid->psi->i_pat_version  = -1;
        if( p_owner )
        {
            ts_prg_psi_t *prg = malloc( sizeof( ts_prg_psi_t ) );
            /* PMT */
            prg->i_version  = -1;
            prg->i_number   = -1;
            prg->i_pid_pcr  = -1;
            prg->i_pid_pmt  = -1;
            prg->iod        = NULL;
            prg->handle     = NULL;

            TAB_APPEND( pid->psi->i_prg, pid->psi->prg, prg );
        }
    }
    else
    {
        pid->psi = NULL;
        pid->es  = malloc( sizeof( ts_es_t ) );

        es_format_Init( &pid->es->fmt, UNKNOWN_ES, 0 );
        pid->es->id      = NULL;
        pid->es->p_pes   = NULL;
        pid->es->i_pes_size= 0;
        pid->es->i_pes_gathered= 0;
        pid->es->pp_last = &pid->es->p_pes;
        pid->es->p_mpeg4desc = NULL;
        pid->es->b_gather = VLC_FALSE;
    }
}

static void PIDClean( es_out_t *out, ts_pid_t *pid )
{
    if( pid->psi )
    {
        int i;

        if( pid->psi->handle ) dvbpsi_DetachPMT( pid->psi->handle );
        for( i = 0; i < pid->psi->i_prg; i++ )
        {
            if( pid->psi->prg[i]->iod )
                IODFree( pid->psi->prg[i]->iod );
            if( pid->psi->prg[i]->handle )
                dvbpsi_DetachPMT( pid->psi->prg[i]->handle );
            free( pid->psi->prg[i] );
        }
        if( pid->psi->prg ) free( pid->psi->prg );
        free( pid->psi );
    }
    else
    {
        int i;

        if( pid->es->id )
            es_out_Del( out, pid->es->id );

        if( pid->es->p_pes )
            block_ChainRelease( pid->es->p_pes );

        es_format_Clean( &pid->es->fmt );

        free( pid->es );

        for( i = 0; i < pid->i_extra_es; i++ )
        {
            if( pid->extra_es[i]->id )
                es_out_Del( out, pid->extra_es[i]->id );

            if( pid->extra_es[i]->p_pes )
                block_ChainRelease( pid->extra_es[i]->p_pes );

            es_format_Clean( &pid->extra_es[i]->fmt );

            free( pid->extra_es[i] );
        }
        if( pid->i_extra_es ) free( pid->extra_es );
    }

    pid->b_valid = VLC_FALSE;
}

/****************************************************************************
 * gathering stuff
 ****************************************************************************/
static void ParsePES( demux_t *p_demux, ts_pid_t *pid )
{
    block_t *p_pes = pid->es->p_pes;
    uint8_t header[30];
    int     i_pes_size = 0;
    int     i_skip = 0;
    mtime_t i_dts = -1;
    mtime_t i_pts = -1;
    mtime_t i_length = 0;
    int i_max;

    /* remove the pes from pid */
    pid->es->p_pes = NULL;
    pid->es->i_pes_size= 0;
    pid->es->i_pes_gathered= 0;
    pid->es->pp_last = &pid->es->p_pes;

    /* FIXME find real max size */
    i_max = block_ChainExtract( p_pes, header, 30 );


    if( header[0] != 0 || header[1] != 0 || header[2] != 1 )
    {
        if( !p_demux->p_sys->b_silent )
            msg_Warn( p_demux, "invalid header [0x%x:%x:%x:%x] (pid: %d)",
                      header[0], header[1],header[2],header[3], pid->i_pid );
        block_ChainRelease( p_pes );
        return;
    }

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
                             (mtime_t)(header[13] >> 1);

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
                     i_pts = ((mtime_t)(header[i_skip]&0x0e ) << 29)|
                              (mtime_t)(header[i_skip+1] << 22)|
                             ((mtime_t)(header[i_skip+2]&0xfe) << 14)|
                              (mtime_t)(header[i_skip+3] << 7)|
                              (mtime_t)(header[i_skip+4] >> 1);

                    if( header[i_skip]&0x10 )    /* has dts */
                    {
                         i_dts = ((mtime_t)(header[i_skip+5]&0x0e ) << 29)|
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
    else if( pid->es->fmt.i_codec == VLC_FOURCC( 's', 'u', 'b', 't' ) &&
             pid->es->p_mpeg4desc )
    {
        decoder_config_descriptor_t *dcd = &pid->es->p_mpeg4desc->dec_descr;

        if( dcd->i_decoder_specific_info_len > 2 &&
            dcd->p_decoder_specific_info[0] == 0x10 &&
            ( dcd->p_decoder_specific_info[1]&0x10 ) )
        {
            /* display length */
            if( p_pes->i_buffer + 2 <= i_skip )
            {
                i_length = GetWBE( &p_pes->p_buffer[i_skip] );
            }

            i_skip += 2;
        }
        if( p_pes->i_buffer + 2 <= i_skip )
        {
            i_pes_size = GetWBE( &p_pes->p_buffer[i_skip] );
        }
        /* */
        i_skip += 2;
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

    /* ISO/IEC 13818-1 2.7.5: if no pts and no dts, then dts == pts */
    if( i_pts >= 0 && i_dts < 0 )
        i_dts = i_pts;

    if( p_pes )
    {
        block_t *p_block;
        int i;

        if( i_dts >= 0 )
        {
            p_pes->i_dts = i_dts * 100 / 9;
        }
        if( i_pts >= 0 )
        {
            p_pes->i_pts = i_pts * 100 / 9;
        }
        p_pes->i_length = i_length * 100 / 9;

        p_block = block_ChainGather( p_pes );
        if( pid->es->fmt.i_codec == VLC_FOURCC( 's', 'u', 'b', 't' ) )
        {
            if( i_pes_size > 0 && p_block->i_buffer > i_pes_size )
            {
                p_block->i_buffer = i_pes_size;
            }
            /* Append a \0 */
            p_block = block_Realloc( p_block, 0, p_block->i_buffer + 1 );
            p_block->p_buffer[p_block->i_buffer -1] = '\0';
        }

        for( i = 0; i < pid->i_extra_es; i++ )
        {
            es_out_Send( p_demux->out, pid->extra_es[i]->id,
                         block_Duplicate( p_block ) );
        }

        es_out_Send( p_demux->out, pid->es->id, p_block );
    }
    else
    {
        msg_Warn( p_demux, "empty pes" );
    }
}

static void PCRHandle( demux_t *p_demux, ts_pid_t *pid, block_t *p_bk )
{
    demux_sys_t   *p_sys = p_demux->p_sys;
    const uint8_t *p = p_bk->p_buffer;

    if( ( p[3]&0x20 ) && /* adaptation */
        ( p[5]&0x10 ) &&
        ( p[4] >= 7 ) )
    {
        int i;
        mtime_t i_pcr;  /* 33 bits */

        i_pcr = ( (mtime_t)p[6] << 25 ) |
                ( (mtime_t)p[7] << 17 ) |
                ( (mtime_t)p[8] << 9 ) |
                ( (mtime_t)p[9] << 1 ) |
                ( (mtime_t)p[10] >> 7 );

        /* Search program and set the PCR */
        for( i = 0; i < p_sys->i_pmt; i++ )
        {
            int i_prg;
            for( i_prg = 0; i_prg < p_sys->pmt[i]->psi->i_prg; i_prg++ )
            {
                if( pid->i_pid == p_sys->pmt[i]->psi->prg[i_prg]->i_pid_pcr )
                {
                    es_out_Control( p_demux->out, ES_OUT_SET_GROUP_PCR,
                                    (int)p_sys->pmt[i]->psi->prg[i_prg]->i_number,
                                    (int64_t)(i_pcr * 100 / 9) );
                }
            }
        }
    }
}

static vlc_bool_t GatherPES( demux_t *p_demux, ts_pid_t *pid, block_t *p_bk )
{
    const uint8_t    *p = p_bk->p_buffer;
    const vlc_bool_t b_unit_start = p[1]&0x40;
    const vlc_bool_t b_adaptation = p[3]&0x20;
    const vlc_bool_t b_payload    = p[3]&0x10;
    const int        i_cc         = p[3]&0x0f;   /* continuity counter */

    /* transport_scrambling_control is ignored */

    int         i_skip = 0;
    vlc_bool_t  i_ret  = VLC_FALSE;
    int         i_diff;

#if 0
    msg_Dbg( p_demux, "pid=%d unit_start=%d adaptation=%d payload=%d "
             "cc=0x%x", pid->i_pid, b_unit_start, b_adaptation,
             b_payload, i_cc );
#endif

    /* For now, ignore additional error correction
     * TODO: handle Reed-Solomon 204,188 error correction */
    p_bk->i_buffer = TS_PACKET_SIZE_188;

    if( p[1]&0x80 )
    {
        msg_Dbg( p_demux, "transport_error_indicator set (pid=%d)",
                 pid->i_pid );
    }

    if( p_demux->p_sys->csa )
    {
        csa_Decrypt( p_demux->p_sys->csa, p_bk->p_buffer );
    }

    if( !b_adaptation )
    {
        /* We don't have any adaptation_field, so payload starts
         * immediately after the 4 byte TS header */
        i_skip = 4;
    }
    else
    {
        /* p[4] is adaptation_field_length minus one */
        i_skip = 5 + p[4];
        if( p[4] > 0 )
        {
            if( p[5]&0x80 )
            {
                msg_Warn( p_demux, "discontinuity_indicator (pid=%d) "
                          "ignored", pid->i_pid );
            }
        }
    }

    /* Test continuity counter */
    /* continuous when (one of this):
        * diff == 1
        * diff == 0 and payload == 0
        * diff == 0 and duplicate packet (playload != 0) <- should we
        *   test the content ?
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
            msg_Warn( p_demux, "first packet for pid=%d cc=0x%x",
                      pid->i_pid, i_cc );
            pid->i_cc = i_cc;
        }
        else if( i_diff != 0 )
        {
            /* FIXME what to do when discontinuity_indicator is set ? */
            msg_Warn( p_demux, "discontinuity received 0x%x instead of 0x%x",
                      i_cc, ( pid->i_cc + 1 )&0x0f );

            pid->i_cc = i_cc;

            if( pid->es->p_pes && pid->es->fmt.i_cat != VIDEO_ES )
            {
                /* Small video artifacts are usually better then
                 * dropping full frames */
                pid->es->p_pes->i_flags |= BLOCK_FLAG_CORRUPTED;
            }
        }
    }

    PCRHandle( p_demux, pid, p_bk );

    if( i_skip >= 188 || pid->es->id == NULL || p_demux->p_sys->b_udp_out )
    {
        block_Release( p_bk );
        return i_ret;
    }

    /* We have to gather it */
    p_bk->p_buffer += i_skip;
    p_bk->i_buffer -= i_skip;

    if( b_unit_start )
    {
        if( pid->es->p_pes )
        {
            ParsePES( p_demux, pid );
            i_ret = VLC_TRUE;
        }

        block_ChainLastAppend( &pid->es->pp_last, p_bk );
        if( p_bk->i_buffer > 6 )
        {
            pid->es->i_pes_size = GetWBE( &p_bk->p_buffer[4] );
            if( pid->es->i_pes_size > 0 )
            {
                pid->es->i_pes_size += 6;
            }
        }
        pid->es->i_pes_gathered += p_bk->i_buffer;
        if( pid->es->i_pes_size > 0 &&
            pid->es->i_pes_gathered >= pid->es->i_pes_size )
        {
            ParsePES( p_demux, pid );
            i_ret = VLC_TRUE;
        }
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
            block_ChainLastAppend( &pid->es->pp_last, p_bk );
            pid->es->i_pes_gathered += p_bk->i_buffer;
            if( pid->es->i_pes_size > 0 &&
                pid->es->i_pes_gathered >= pid->es->i_pes_size )
            {
                ParsePES( p_demux, pid );
                i_ret = VLC_TRUE;
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
            pid->es->b_gather = VLC_TRUE;
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

        case 0xa0:  /* MSCODEC vlc (video) (fixed later) */
            es_format_Init( fmt, UNKNOWN_ES, 0 );
            pid->es->b_gather = VLC_TRUE;
            break;

        case 0x06:  /* PES_PRIVATE  (fixed later) */
        case 0x12:  /* MPEG-4 generic (sub/scene/...) (fixed later) */
        default:
            es_format_Init( fmt, UNKNOWN_ES, 0 );
            break;
    }

    /* PES packets usually contain truncated frames */
    fmt->b_packetized = VLC_FALSE;

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
static vlc_bool_t DVBProgramIsSelected( demux_t *p_demux, uint16_t i_pgrm )
{
    demux_sys_t          *p_sys = p_demux->p_sys;

    if ( !p_sys->b_dvb_control ) return VLC_FALSE;
    if ( p_sys->i_dvb_program == -1 && p_sys->p_programs_list == NULL )
        return VLC_TRUE;
    if ( p_sys->i_dvb_program == i_pgrm ) return VLC_TRUE;

    if ( p_sys->p_programs_list != NULL )
    {
        int i;
        for ( i = 0; i < p_sys->p_programs_list->i_count; i++ )
        {
            if ( i_pgrm == p_sys->p_programs_list->p_values[i].i_int )
                return VLC_TRUE;
        }
    }
    return VLC_FALSE;
}

static void PMTCallBack( demux_t *p_demux, dvbpsi_pmt_t *p_pmt )
{
    demux_sys_t          *p_sys = p_demux->p_sys;
    dvbpsi_descriptor_t  *p_dr;
    dvbpsi_pmt_es_t      *p_es;

    ts_pid_t             *pmt = NULL;
    ts_prg_psi_t         *prg = NULL;

    ts_pid_t             **pp_clean = NULL;
    int                  i_clean = 0, i;

    msg_Dbg( p_demux, "PMTCallBack called" );

    /* First find this PMT declared in PAT */
    for( i = 0; i < p_sys->i_pmt; i++ )
    {
        int i_prg;
        for( i_prg = 0; i_prg < p_sys->pmt[i]->psi->i_prg; i_prg++ )
        {
            if( p_sys->pmt[i]->psi->prg[i_prg]->i_number ==
                p_pmt->i_program_number )
            {
                pmt = p_sys->pmt[i];
                prg = p_sys->pmt[i]->psi->prg[i_prg];
                break;
            }
        }
        if( pmt ) break;
    }

    if( pmt == NULL )
    {
        msg_Warn( p_demux, "unreferenced program (broken stream)" );
        dvbpsi_DeletePMT(p_pmt);
        return;
    }

    if( prg->i_version != -1 &&
        ( !p_pmt->b_current_next || prg->i_version == p_pmt->i_version ) )
    {
        dvbpsi_DeletePMT( p_pmt );
        return;
    }

    /* Clean this program (remove all es) */
    for( i = 0; i < 8192; i++ )
    {
        ts_pid_t *pid = &p_sys->pid[i];

        if( pid->b_valid && pid->p_owner == pmt->psi &&
            pid->i_owner_number == prg->i_number && pid->psi == NULL )
        {
            TAB_APPEND( i_clean, pp_clean, pid );
        }
    }
    if( prg->iod )
    {
        IODFree( prg->iod );
        prg->iod = NULL;
    }

    msg_Dbg( p_demux, "new PMT program number=%d version=%d pid_pcr=%d",
             p_pmt->i_program_number, p_pmt->i_version, p_pmt->i_pcr_pid );
    prg->i_pid_pcr = p_pmt->i_pcr_pid;
    prg->i_version = p_pmt->i_version;

    if( DVBProgramIsSelected( p_demux, prg->i_number ) )
    {
        /* Set demux filter */
        stream_Control( p_demux->s, STREAM_CONTROL_ACCESS,
                        ACCESS_SET_PRIVATE_ID_STATE, prg->i_pid_pcr,
                        VLC_TRUE );
    }
    else if ( p_sys->b_dvb_control )
    {
        msg_Warn( p_demux, "skipping program (not selected)" );
        dvbpsi_DeletePMT(p_pmt);
        return;
    }

    /* Parse descriptor */
    for( p_dr = p_pmt->p_first_descriptor; p_dr != NULL; p_dr = p_dr->p_next )
    {
        if( p_dr->i_tag == 0x1d )
        {
            /* We have found an IOD descriptor */
            msg_Dbg( p_demux, " * descriptor : IOD (0x1d)" );

            prg->iod = IODNew( p_dr->i_length, p_dr->p_data );
        }
        else if( p_dr->i_tag == 0x9 )
        {
            uint16_t i_sysid = ((uint16_t)p_dr->p_data[0] << 8)
                                | p_dr->p_data[1];
            msg_Dbg( p_demux, " * descriptor : CA (0x9) SysID 0x%x", i_sysid );
        }
        else
        {
            msg_Dbg( p_demux, " * descriptor : unknown (0x%x)", p_dr->i_tag );
        }
    }

    for( p_es = p_pmt->p_first_es; p_es != NULL; p_es = p_es->p_next )
    {
        ts_pid_t tmp_pid, *old_pid = 0, *pid = &tmp_pid;

        /* Find out if the PID was already declared */
        for( i = 0; i < i_clean; i++ )
        {
            if( pp_clean[i] == &p_sys->pid[p_es->i_pid] )
            {
                old_pid = pp_clean[i];
                break;
            }
        }

        if( !old_pid && p_sys->pid[p_es->i_pid].b_valid )
        {
            msg_Warn( p_demux, "pmt error: pid=%d already defined",
                      p_es->i_pid );
            continue;
        }

        PIDInit( pid, VLC_FALSE, pmt->psi );
        PIDFillFormat( pid, p_es->i_type );
        pid->i_owner_number = prg->i_number;
        pid->i_pid          = p_es->i_pid;
        pid->b_seen         = p_sys->pid[p_es->i_pid].b_seen;

        if( p_es->i_type == 0x10 || p_es->i_type == 0x11 ||
            p_es->i_type == 0x12 )
        {
            /* MPEG-4 stream: search SL_DESCRIPTOR */
            dvbpsi_descriptor_t *p_dr = p_es->p_first_descriptor;;

            while( p_dr && ( p_dr->i_tag != 0x1f ) ) p_dr = p_dr->p_next;

            if( p_dr && p_dr->i_length == 2 )
            {
                int i_es_id = ( p_dr->p_data[0] << 8 ) | p_dr->p_data[1];

                msg_Warn( p_demux, "found SL_descriptor es_id=%d", i_es_id );

                pid->es->p_mpeg4desc = NULL;

                for( i = 0; i < 255; i++ )
                {
                    iod_descriptor_t *iod = prg->iod;

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
                decoder_config_descriptor_t *dcd =
                    &pid->es->p_mpeg4desc->dec_descr;

                if( dcd->i_streamType == 0x04 )    /* VisualStream */
                {
                    pid->es->fmt.i_cat = VIDEO_ES;
                    switch( dcd->i_objectTypeIndication )
                    {
                    case 0x0B: /* mpeg4 sub */
                        pid->es->fmt.i_cat = SPU_ES;
                        pid->es->fmt.i_codec = VLC_FOURCC('s','u','b','t');
                        break;

                    case 0x20: /* mpeg4 */
                        pid->es->fmt.i_codec = VLC_FOURCC('m','p','4','v');
                        break;
                    case 0x60:
                    case 0x61:
                    case 0x62:
                    case 0x63:
                    case 0x64:
                    case 0x65: /* mpeg2 */
                        pid->es->fmt.i_codec = VLC_FOURCC( 'm','p','g','v' );
                        break;
                    case 0x6a: /* mpeg1 */
                        pid->es->fmt.i_codec = VLC_FOURCC( 'm','p','g','v' );
                        break;
                    case 0x6c: /* mpeg1 */
                        pid->es->fmt.i_codec = VLC_FOURCC( 'j','p','e','g' );
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
                    case 0x40: /* mpeg4 */
                        pid->es->fmt.i_codec = VLC_FOURCC('m','p','4','a');
                        break;
                    case 0x66:
                    case 0x67:
                    case 0x68: /* mpeg2 aac */
                        pid->es->fmt.i_codec = VLC_FOURCC('m','p','4','a');
                        break;
                    case 0x69: /* mpeg2 */
                        pid->es->fmt.i_codec = VLC_FOURCC('m','p','g','a');
                        break;
                    case 0x6b: /* mpeg1 */
                        pid->es->fmt.i_codec = VLC_FOURCC('m','p','g','a');
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

            for( p_dr = p_es->p_first_descriptor; p_dr != NULL;
                 p_dr = p_dr->p_next )
            {
                msg_Dbg( p_demux, "  * es pid=%d type=%d dr->i_tag=0x%x",
                         p_es->i_pid, p_es->i_type, p_dr->i_tag );

                if( p_dr->i_tag == 0x05 )
                {
                    /* Registration Descriptor */
                    if( p_dr->i_length != 4 )
                    {
                        msg_Warn( p_demux, "invalid Registration Descriptor" );
                    }
                    else
                    {
                        if( !memcmp( p_dr->p_data, "AC-3", 4 ) )
                        {
                            /* ATSC with stream_type 0x81 (but this descriptor
                             * is then not mandatory */
                            pid->es->fmt.i_cat = AUDIO_ES;
                            pid->es->fmt.i_codec = VLC_FOURCC('a','5','2',' ');
                        }
                        else if( !memcmp( p_dr->p_data, "DTS1", 4 ) ||
                                 !memcmp( p_dr->p_data, "DTS2", 4 ) ||
                                 !memcmp( p_dr->p_data, "DTS3", 4 ) )
                        {
                           /*registration descriptor(ETSI TS 101 154 Annex F)*/
                            pid->es->fmt.i_cat = AUDIO_ES;
                            pid->es->fmt.i_codec = VLC_FOURCC('d','t','s',' ');
                        }
                        else if( !memcmp( p_dr->p_data, "BSSD", 4 ) )
                        {
                            pid->es->fmt.i_cat = AUDIO_ES;
                            pid->es->fmt.i_codec = VLC_FOURCC('l','p','c','m');
                        }
                        else
                        {
                            msg_Warn( p_demux,
                                      "unknown Registration Descriptor (%4.4s)",
                                      p_dr->p_data );
                        }
                    }

                }
                else if( p_dr->i_tag == 0x6a )
                {
                    /* DVB with stream_type 0x06 */
                    pid->es->fmt.i_cat = AUDIO_ES;
                    pid->es->fmt.i_codec = VLC_FOURCC( 'a', '5', '2', ' ' );
                }
                else if( p_dr->i_tag == 0x73 )
                {
                    /* DTS audio descriptor (ETSI TS 101 154 Annex F) */
                    msg_Dbg( p_demux, "   * DTS audio descriptor not decoded" );
                    pid->es->fmt.i_cat = AUDIO_ES;
                    pid->es->fmt.i_codec = VLC_FOURCC( 'd', 't', 's', ' ' );
                }
                else if( p_dr->i_tag == 0x45 )
                {
                    msg_Dbg( p_demux, "   * VBI Data descriptor" );
                    pid->es->fmt.i_cat = SPU_ES;
                    pid->es->fmt.i_codec = VLC_FOURCC( 'v', 'b', 'i', 'd' );
                    pid->es->fmt.psz_description = strdup( "VBI Data" );
                    pid->es->fmt.i_extra = p_dr->i_length;
                    pid->es->fmt.p_extra = malloc( p_dr->i_length );
                    memcpy( pid->es->fmt.p_extra, p_dr->p_data,
                            p_dr->i_length );
                }
                else if( p_dr->i_tag == 0x46 )
                {
                    msg_Dbg( p_demux, "  * VBI Teletext descriptor" );
                    pid->es->fmt.i_cat = SPU_ES;
                    pid->es->fmt.i_codec = VLC_FOURCC( 'v', 'b', 'i', 't' );
                    pid->es->fmt.psz_description = strdup( "VBI Teletext" );
                    pid->es->fmt.i_extra = p_dr->i_length;
                    pid->es->fmt.p_extra = malloc( p_dr->i_length );
                    memcpy( pid->es->fmt.p_extra, p_dr->p_data,
                            p_dr->i_length );
                }
                else if( p_dr->i_tag == 0x56 )
                {
                    msg_Dbg( p_demux, "   * EBU Teletext descriptor" );
                    pid->es->fmt.i_cat = SPU_ES;
                    pid->es->fmt.i_codec = VLC_FOURCC( 't', 'e', 'l', 'x' );
                    pid->es->fmt.psz_description = strdup( "Teletext" );
                    pid->es->fmt.i_extra = p_dr->i_length;
                    pid->es->fmt.p_extra = malloc( p_dr->i_length );
                    memcpy( pid->es->fmt.p_extra, p_dr->p_data,
                            p_dr->i_length );
                }
#ifdef _DVBPSI_DR_59_H_
                else if( p_dr->i_tag == 0x59 )
                {
                    uint16_t n;
                    dvbpsi_subtitling_dr_t *sub;

                    /* DVB subtitles */
                    pid->es->fmt.i_cat = SPU_ES;
                    pid->es->fmt.i_codec = VLC_FOURCC( 'd', 'v', 'b', 's' );
                    pid->es->fmt.i_group = p_pmt->i_program_number;

                    sub = dvbpsi_DecodeSubtitlingDr( p_dr );
                    if( !sub ) continue;

                    /* Each subtitle ES contains n languages,
                     * We are going to create n ES for the n tracks */
                    if( sub->i_subtitles_number > 0 )
                    {
                        pid->es->fmt.psz_language = malloc( 4 );
                        memcpy( pid->es->fmt.psz_language,
                                sub->p_subtitle[0].i_iso6392_language_code, 3);
                        pid->es->fmt.psz_language[3] = 0;

                        pid->es->fmt.subs.dvb.i_id =
                            sub->p_subtitle[0].i_composition_page_id;
                        /* Hack, FIXME */
                        pid->es->fmt.subs.dvb.i_id |=
                          ((int)sub->p_subtitle[0].i_ancillary_page_id << 16);
                    }
                    else pid->es->fmt.i_cat = UNKNOWN_ES;

                    for( n = 1; n < sub->i_subtitles_number; n++ )
                    {
                        ts_es_t *p_es = malloc( sizeof( ts_es_t ) );
                        p_es->fmt = pid->es->fmt;
                        p_es->id = NULL;
                        p_es->p_pes = NULL;
                        p_es->i_pes_size = 0;
                        p_es->i_pes_gathered = 0;
                        p_es->pp_last = &p_es->p_pes;
                        p_es->p_mpeg4desc = NULL;

                        p_es->fmt.psz_language = malloc( 4 );
                        memcpy( p_es->fmt.psz_language,
                                sub->p_subtitle[n].i_iso6392_language_code, 3);
                        p_es->fmt.psz_language[3] = 0;

                        p_es->fmt.subs.dvb.i_id =
                            sub->p_subtitle[n].i_composition_page_id;
                        /* Hack, FIXME */
                        p_es->fmt.subs.dvb.i_id |=
                          ((int)sub->p_subtitle[n].i_ancillary_page_id << 16);

                        TAB_APPEND( pid->i_extra_es, pid->extra_es, p_es );
                    }
                }
#endif /* _DVBPSI_DR_59_H_ */
            }
        }
        else if( p_es->i_type == 0xa0 )
        {
            /* MSCODEC sent by vlc */
            dvbpsi_descriptor_t *p_dr = p_es->p_first_descriptor;

            while( p_dr && ( p_dr->i_tag != 0xa0 ) ) p_dr = p_dr->p_next;

            if( p_dr && p_dr->i_length >= 8 )
            {
                pid->es->fmt.i_cat = VIDEO_ES;
                pid->es->fmt.i_codec =
                    VLC_FOURCC( p_dr->p_data[0], p_dr->p_data[1],
                                p_dr->p_data[2], p_dr->p_data[3] );
                pid->es->fmt.video.i_width =
                    ( p_dr->p_data[4] << 8 ) | p_dr->p_data[5];
                pid->es->fmt.video.i_height =
                    ( p_dr->p_data[6] << 8 ) | p_dr->p_data[7];
                pid->es->fmt.i_extra = 
                    (p_dr->p_data[8] << 8) | p_dr->p_data[9];

                if( pid->es->fmt.i_extra > 0 )
                {
                    pid->es->fmt.p_extra = malloc( pid->es->fmt.i_extra );
                    memcpy( pid->es->fmt.p_extra, &p_dr->p_data[10],
                            pid->es->fmt.i_extra );
                }
            }
            else
            {
                msg_Warn( p_demux, "private MSCODEC (vlc) without bih private "
                          "descriptor" );
            }
            /* For such stream we will gather them ourself and don't launch a
             * packetizer.
             * Yes it's ugly but it's the only way to have DIV3 working */
            pid->es->fmt.b_packetized = VLC_TRUE;
        }

        if( pid->es->fmt.i_cat == AUDIO_ES ||
            ( pid->es->fmt.i_cat == SPU_ES &&
              pid->es->fmt.i_codec != VLC_FOURCC('d','v','b','s') ) )
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
                    memcpy( pid->es->fmt.psz_language,
                            p_decoded->i_iso_639_code, 3 );
                    pid->es->fmt.psz_language[3] = 0;
                }
            }
        }

        pid->es->fmt.i_group = p_pmt->i_program_number;
        if( pid->es->fmt.i_cat == UNKNOWN_ES )
        {
            msg_Dbg( p_demux, "  * es pid=%d type=%d *unknown*",
                     p_es->i_pid, p_es->i_type );
        }
        else if( !p_sys->b_udp_out )
        {
            msg_Dbg( p_demux, "  * es pid=%d type=%d fcc=%4.4s",
                     p_es->i_pid, p_es->i_type, (char*)&pid->es->fmt.i_codec );

            if( p_sys->b_es_id_pid ) pid->es->fmt.i_id = p_es->i_pid;

            /* Check if we can avoid restarting the ES */
            if( old_pid &&
                pid->es->fmt.i_codec == old_pid->es->fmt.i_codec &&
                pid->es->fmt.i_extra == old_pid->es->fmt.i_extra &&
                pid->es->fmt.i_extra == 0 &&
                pid->i_extra_es == old_pid->i_extra_es &&
                ( ( !pid->es->fmt.psz_language &&
                    !old_pid->es->fmt.psz_language ) ||
                  ( pid->es->fmt.psz_language &&
                    old_pid->es->fmt.psz_language &&
                    !strcmp( pid->es->fmt.psz_language,
                             old_pid->es->fmt.psz_language ) ) ) )
            {
                pid->es->id = old_pid->es->id;
                old_pid->es->id = NULL;
                for( i = 0; i < pid->i_extra_es; i++ )
                {
                    pid->extra_es[i]->id = old_pid->extra_es[i]->id;
                    old_pid->extra_es[i]->id = NULL;
                }
            }
            else
            {
                if( old_pid )
                {
                    PIDClean( p_demux->out, old_pid );
                    TAB_REMOVE( i_clean, pp_clean, old_pid );
                    old_pid = 0;
                }

                pid->es->id = es_out_Add( p_demux->out, &pid->es->fmt );
                for( i = 0; i < pid->i_extra_es; i++ )
                {
                    pid->extra_es[i]->id =
                        es_out_Add( p_demux->out, &pid->extra_es[i]->fmt );
                }
            }
        }

        /* Add ES to the list */
        if( old_pid )
        {
            PIDClean( p_demux->out, old_pid );
            TAB_REMOVE( i_clean, pp_clean, old_pid );
        }
        p_sys->pid[p_es->i_pid] = *pid;

        for( p_dr = p_es->p_first_descriptor; p_dr != NULL;
             p_dr = p_dr->p_next )
        {
            if( p_dr->i_tag == 0x9 )
            {
                uint16_t i_sysid = ((uint16_t)p_dr->p_data[0] << 8)
                                    | p_dr->p_data[1];
                msg_Dbg( p_demux, "   * descriptor : CA (0x9) SysID 0x%x",
                         i_sysid );
            }
        }

        if( DVBProgramIsSelected( p_demux, prg->i_number ) )
        {
            /* Set demux filter */
            stream_Control( p_demux->s, STREAM_CONTROL_ACCESS,
                            ACCESS_SET_PRIVATE_ID_STATE, p_es->i_pid,
                            VLC_TRUE );
        }
    }

    if( DVBProgramIsSelected( p_demux, prg->i_number ) )
    {
        /* Set CAM descrambling */
        stream_Control( p_demux->s, STREAM_CONTROL_ACCESS,
                        ACCESS_SET_PRIVATE_ID_CA, p_pmt );
    }
    else
    {
        dvbpsi_DeletePMT( p_pmt );
    }

    for ( i = 0; i < i_clean; i++ )
    {
        if( DVBProgramIsSelected( p_demux, prg->i_number ) )
        {
            stream_Control( p_demux->s, STREAM_CONTROL_ACCESS,
                            ACCESS_SET_PRIVATE_ID_STATE, pp_clean[i]->i_pid,
                            VLC_FALSE );
        }

        PIDClean( p_demux->out, pp_clean[i] );
    }
    if( i_clean ) free( pp_clean );
}

static void PATCallBack( demux_t *p_demux, dvbpsi_pat_t *p_pat )
{
    demux_sys_t          *p_sys = p_demux->p_sys;
    dvbpsi_pat_program_t *p_program;
    ts_pid_t             *pat = &p_sys->pid[0];
    int                  i, j;

    msg_Dbg( p_demux, "PATCallBack called" );

    if( pat->psi->i_pat_version != -1 &&
        ( !p_pat->b_current_next ||
          p_pat->i_version == pat->psi->i_pat_version ) )
    {
        dvbpsi_DeletePAT( p_pat );
        return;
    }

    msg_Dbg( p_demux, "new PAT ts_id=%d version=%d current_next=%d",
             p_pat->i_ts_id, p_pat->i_version, p_pat->b_current_next );

    /* Clean old */
    if( p_sys->i_pmt > 0 )
    {
        int      i_pmt_rm = 0;
        ts_pid_t **pmt_rm = NULL;

        /* Search pmt to be deleted */
        for( i = 0; i < p_sys->i_pmt; i++ )
        {
            ts_pid_t *pmt = p_sys->pmt[i];
            vlc_bool_t b_keep = VLC_FALSE;

            for( p_program = p_pat->p_first_program; p_program != NULL;
                 p_program = p_program->p_next )
            {
                if( p_program->i_pid == pmt->i_pid )
                {
                    int i_prg;
                    for( i_prg = 0; i_prg < pmt->psi->i_prg; i_prg++ )
                    {
                        if( p_program->i_number ==
                            pmt->psi->prg[i_prg]->i_number )
                        {
                            b_keep = VLC_TRUE;
                            break;
                        }
                    }
                    if( b_keep ) break;
                }
            }

            if( !b_keep )
            {
                TAB_APPEND( i_pmt_rm, pmt_rm, pmt );
            }
        }

        /* Delete all ES attached to thoses PMT */
        for( i = 2; i < 8192; i++ )
        {
            ts_pid_t *pid = &p_sys->pid[i];

            if( !pid->b_valid || pid->psi ) continue;

            for( j = 0; j < i_pmt_rm; j++ )
            {
                int i_prg;
                for( i_prg = 0; i_prg < pid->p_owner->i_prg; i_prg++ )
                {
                    /* We only remove es that aren't defined by extra pmt */
                    if( pid->p_owner->prg[i_prg]->i_pid_pmt !=
                        pmt_rm[j]->i_pid ) continue;

                    if( p_sys->b_dvb_control && pid->es->id )
                    {
                        if( stream_Control( p_demux->s, STREAM_CONTROL_ACCESS,
                                            ACCESS_SET_PRIVATE_ID_STATE, i,
                                            VLC_FALSE ) )
                            p_sys->b_dvb_control = VLC_FALSE;
                    }

                    PIDClean( p_demux->out, pid );
                    break;
                }

                if( !pid->b_valid ) break;
            }
        }

        /* Delete PMT pid */
        for( i = 0; i < i_pmt_rm; i++ )
        {
            if( p_sys->b_dvb_control )
            {
                if( stream_Control( p_demux->s, STREAM_CONTROL_ACCESS,
                                    ACCESS_SET_PRIVATE_ID_STATE,
                                    pmt_rm[i]->i_pid, VLC_FALSE ) )
                    p_sys->b_dvb_control = VLC_FALSE;
            }

            PIDClean( p_demux->out, &p_sys->pid[pmt_rm[i]->i_pid] );
            TAB_REMOVE( p_sys->i_pmt, p_sys->pmt, pmt_rm[i] );
        }

        if( pmt_rm ) free( pmt_rm );
    }

    /* now create programs */
    for( p_program = p_pat->p_first_program; p_program != NULL;
         p_program = p_program->p_next )
    {
        msg_Dbg( p_demux, "  * number=%d pid=%d", p_program->i_number,
                 p_program->i_pid );
        if( p_program->i_number != 0 )
        {
            ts_pid_t *pmt = &p_sys->pid[p_program->i_pid];
            vlc_bool_t b_add = VLC_TRUE;

            if( pmt->b_valid )
            {
                int i_prg;
                for( i_prg = 0; i_prg < pmt->psi->i_prg; i_prg++ )
                {
                    if( pmt->psi->prg[i_prg]->i_number == p_program->i_number )
                    {
                        b_add = VLC_FALSE;
                        break;
                    }
                }
            }
            else
            {
                TAB_APPEND( p_sys->i_pmt, p_sys->pmt, pmt );
            }

            if( b_add )
            {
                PIDInit( pmt, VLC_TRUE, pat->psi );
                pmt->psi->prg[pmt->psi->i_prg-1]->handle =
                    dvbpsi_AttachPMT( p_program->i_number,
                                      (dvbpsi_pmt_callback)PMTCallBack,
                                      p_demux );
                pmt->psi->prg[pmt->psi->i_prg-1]->i_number =
                    p_program->i_number;
                pmt->psi->prg[pmt->psi->i_prg-1]->i_pid_pmt =
                    p_program->i_pid;

                /* Now select PID at access level */
                if( p_sys->b_dvb_control )
                {
                    if( DVBProgramIsSelected( p_demux, p_program->i_number ) )
                    {
                        if( p_sys->i_dvb_program == 0 )
                            p_sys->i_dvb_program = p_program->i_number;

                        if( stream_Control( p_demux->s, STREAM_CONTROL_ACCESS, ACCESS_SET_PRIVATE_ID_STATE, p_program->i_pid, VLC_TRUE ) )
                            p_sys->b_dvb_control = VLC_FALSE;
                    }
                    else
                    {
                        if( stream_Control( p_demux->s, STREAM_CONTROL_ACCESS, ACCESS_SET_PRIVATE_ID_STATE, p_program->i_pid, VLC_FALSE ) )
                            p_sys->b_dvb_control = VLC_FALSE;
                    }
                }
            }
        }
    }
    pat->psi->i_pat_version = p_pat->i_version;

    dvbpsi_DeletePAT( p_pat );
}

