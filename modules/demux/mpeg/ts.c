/*****************************************************************************
 * ts.c: Transport Stream input module for VLC.
 *****************************************************************************
 * Copyright (C) 2004-2016 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman #_at_# m2x.nl>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>    /* DVB-specific things */
#include <vlc_demux.h>
#include <vlc_input.h>

#include "ts_pid.h"
#include "ts_streams.h"
#include "ts_streams_private.h"
#include "ts_pes.h"
#include "ts_psi.h"
#include "ts_si.h"
#include "ts_psip.h"

#include "ts_hotfixes.h"
#include "ts_sl.h"
#include "ts_metadata.h"
#include "sections.h"
#include "pes.h"
#include "timestamps.h"

#include "ts.h"

#include "../../codec/scte18.h"
#include "../opus.h"
#include "../../mux/mpeg/csa.h"

#ifdef HAVE_ARIBB24
 #include <aribb24/aribb24.h>
#endif

#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open  ( vlc_object_t * );
static void Close ( vlc_object_t * );

/* TODO
 * - Rename "extra pmt" to "user pmt"
 * - Update extra pmt description
 *      pmt_pid[:pmt_number][=pid_description[,pid_description]]
 *      where pid_description could take 3 forms:
 *          1. pid:pcr (to force the pcr pid)
 *          2. pid:stream_type
 *          3. pid:type=fourcc where type=(video|audio|spu)
 */
#define PMT_TEXT N_("Extra PMT")
#define PMT_LONGTEXT N_( \
  "Allows a user to specify an extra pmt (pmt_pid=pid:stream_type[,...])." )

#define PID_TEXT N_("Set id of ES to PID")
#define PID_LONGTEXT N_("Set the internal ID of each elementary stream" \
                       " handled by VLC to the same value as the PID in" \
                       " the TS stream, instead of 1, 2, 3, etc. Useful to" \
                       " do \'#duplicate{..., select=\"es=<pid>\"}\'.")

#define CSA_TEXT N_("CSA Key")
#define CSA_LONGTEXT N_("CSA encryption key. This must be a " \
  "16 char string (8 hexadecimal bytes).")

#define CSA2_TEXT N_("Second CSA Key")
#define CSA2_LONGTEXT N_("The even CSA encryption key. This must be a " \
  "16 char string (8 hexadecimal bytes).")


#define CPKT_TEXT N_("Packet size in bytes to decrypt")
#define CPKT_LONGTEXT N_("Specify the size of the TS packet to decrypt. " \
    "The decryption routines subtract the TS-header from the value before " \
    "decrypting." )

#define SPLIT_ES_TEXT N_("Separate sub-streams")
#define SPLIT_ES_LONGTEXT N_( \
    "Separate teletex/dvbs pages into independent ES. " \
    "It can be useful to turn off this option when using stream output." )

#define SEEK_PERCENT_TEXT N_("Seek based on percent not time")
#define SEEK_PERCENT_LONGTEXT N_( \
    "Seek and position based on a percent byte position, not a PCR generated " \
    "time position. If seeking doesn't work property, turn on this option." )

#define CC_CHECK_TEXT       "Check packets continuity counter"
#define CC_CHECK_LONGTEXT   "Detect discontinuities and drop packet duplicates. " \
                            "(bluRay sources are known broken and have false positives). "

#define TS_PATFIX_TEXT      "Try to generate PAT/PMT if missing"
#define TS_SKIP_GHOST_PROGRAM_TEXT "Only create ES on program sending data"
#define TS_OFFSETFIX_TEXT   "Try to fix too early PCR (or late DTS)"
#define TS_GENERATED_PCR_OFFSET_TEXT "Offset in ms for generated PCR"

#define PCR_TEXT N_("Trust in-stream PCR")
#define PCR_LONGTEXT N_("Use the stream PCR as a reference.")

static const char *const ts_standards_list[] =
    { "auto", "mpeg", "dvb", "arib", "atsc", "tdmb" };
static const char *const ts_standards_list_text[] =
  { N_("Auto"), "MPEG", "DVB", "ARIB", "ATSC", "T-DMB" };

#define STANDARD_TEXT N_("Digital TV Standard")
#define STANDARD_LONGTEXT N_( "Selects mode for digital TV standard. " \
                              "This feature affects EPG information and subtitles." )

vlc_module_begin ()
    set_description( N_("MPEG Transport Stream demuxer") )
    set_shortname ( "MPEG-TS" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )

    add_string( "ts-standard", "auto", STANDARD_TEXT, STANDARD_LONGTEXT, true )
        change_string_list( ts_standards_list, ts_standards_list_text )

    add_string( "ts-extra-pmt", NULL, PMT_TEXT, PMT_LONGTEXT, true )
    add_bool( "ts-trust-pcr", true, PCR_TEXT, PCR_LONGTEXT, true )
        change_safe()
    add_bool( "ts-es-id-pid", true, PID_TEXT, PID_LONGTEXT, true )
        change_safe()
    add_obsolete_string( "ts-out" ) /* since 2.2.0 */
    add_obsolete_integer( "ts-out-mtu" ) /* since 2.2.0 */
    add_string( "ts-csa-ck", NULL, CSA_TEXT, CSA_LONGTEXT, true )
        change_safe()
    add_string( "ts-csa2-ck", NULL, CSA2_TEXT, CSA2_LONGTEXT, true )
        change_safe()
    add_integer( "ts-csa-pkt", 188, CPKT_TEXT, CPKT_LONGTEXT, true )
        change_safe()

    add_bool( "ts-split-es", true, SPLIT_ES_TEXT, SPLIT_ES_LONGTEXT, false )
    add_bool( "ts-seek-percent", false, SEEK_PERCENT_TEXT, SEEK_PERCENT_LONGTEXT, true )
    add_bool( "ts-cc-check", true, CC_CHECK_TEXT, CC_CHECK_LONGTEXT, true )
    add_bool( "ts-pmtfix-waitdata", true, TS_SKIP_GHOST_PROGRAM_TEXT, NULL, true )
    add_bool( "ts-patfix", true, TS_PATFIX_TEXT, NULL, true )
    add_bool( "ts-pcr-offsetfix", true, TS_OFFSETFIX_TEXT, NULL, true )
    add_integer_with_range( "ts-generated-pcr-offset", 120, 0, 500,
                            TS_GENERATED_PCR_OFFSET_TEXT, NULL, true )

    add_obsolete_bool( "ts-silent" );

    set_capability( "demux", 10 )
    set_callbacks( Open, Close )
    add_shortcut( "ts" )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux    ( demux_t *p_demux );
static int Control( demux_t *p_demux, int i_query, va_list args );

static int ChangeKeyCallback( vlc_object_t *, char const *, vlc_value_t, vlc_value_t, void * );

/* Helpers */
static bool PIDReferencedByProgram( const ts_pmt_t *, uint16_t );
void UpdatePESFilters( demux_t *p_demux, bool b_all );
static inline void FlushESBuffer( ts_stream_t *p_pes );
static void UpdatePIDScrambledState( demux_t *p_demux, ts_pid_t *p_pid, bool );
static inline int PIDGet( block_t *p )
{
    return ( (p->p_buffer[1]&0x1f)<<8 )|p->p_buffer[2];
}
static stime_t GetPCR( const block_t * );

static block_t * ProcessTSPacket( demux_t *p_demux, ts_pid_t *pid, block_t *p_pkt, int * );
static bool GatherSectionsData( demux_t *p_demux, ts_pid_t *, block_t *, size_t );
static bool GatherPESData( demux_t *p_demux, ts_pid_t *, block_t *, size_t );
static void ProgramSetPCR( demux_t *p_demux, ts_pmt_t *p_prg, stime_t i_pcr );

static block_t* ReadTSPacket( demux_t *p_demux );
static int SeekToTime( demux_t *p_demux, const ts_pmt_t *, stime_t time );
static void ReadyQueuesPostSeek( demux_t *p_demux );
static void PCRHandle( demux_t *p_demux, ts_pid_t *, stime_t );
static void PCRFixHandle( demux_t *, ts_pmt_t *, block_t * );

#define TS_PACKET_SIZE_188 188
#define TS_PACKET_SIZE_192 192
#define TS_PACKET_SIZE_204 204
#define TS_PACKET_SIZE_MAX 204
#define TS_HEADER_SIZE 4

#define PROBE_CHUNK_COUNT 500
#define PROBE_MAX         (PROBE_CHUNK_COUNT * 10)

static int DetectPacketSize( demux_t *p_demux, unsigned *pi_header_size, int i_offset )
{
    const uint8_t *p_peek;

    if( vlc_stream_Peek( p_demux->s,
                     &p_peek, i_offset + TS_PACKET_SIZE_MAX ) < i_offset + TS_PACKET_SIZE_MAX )
        return -1;

    for( int i_sync = 0; i_sync < TS_PACKET_SIZE_MAX; i_sync++ )
    {
        if( p_peek[i_offset + i_sync] != 0x47 )
            continue;

        /* Check next 3 sync bytes */
        int i_peek = i_offset + TS_PACKET_SIZE_MAX * 3 + i_sync + 1;
        if( ( vlc_stream_Peek( p_demux->s, &p_peek, i_peek ) ) < i_peek )
            return -1;

        if( p_peek[i_offset + i_sync + 1 * TS_PACKET_SIZE_188] == 0x47 &&
            p_peek[i_offset + i_sync + 2 * TS_PACKET_SIZE_188] == 0x47 &&
            p_peek[i_offset + i_sync + 3 * TS_PACKET_SIZE_188] == 0x47 )
        {
            return TS_PACKET_SIZE_188;
        }
        else if( p_peek[i_offset + i_sync + 1 * TS_PACKET_SIZE_192] == 0x47 &&
                 p_peek[i_offset + i_sync + 2 * TS_PACKET_SIZE_192] == 0x47 &&
                 p_peek[i_offset + i_sync + 3 * TS_PACKET_SIZE_192] == 0x47 )
        {
            if( i_sync == 4 )
            {
                *pi_header_size = 4; /* BluRay TS packets have 4-byte header */
            }
            return TS_PACKET_SIZE_192;
        }
        else if( p_peek[i_offset + i_sync + 1 * TS_PACKET_SIZE_204] == 0x47 &&
                 p_peek[i_offset + i_sync + 2 * TS_PACKET_SIZE_204] == 0x47 &&
                 p_peek[i_offset + i_sync + 3 * TS_PACKET_SIZE_204] == 0x47 )
        {
            return TS_PACKET_SIZE_204;
        }
    }

    if( p_demux->obj.force )
    {
        msg_Warn( p_demux, "this does not look like a TS stream, continuing" );
        return TS_PACKET_SIZE_188;
    }
    msg_Dbg( p_demux, "TS module discarded (lost sync)" );
    return -1;
}

#define TOPFIELD_HEADER_SIZE 3712

static int DetectPVRHeadersAndHeaderSize( demux_t *p_demux, unsigned *pi_header_size, vdr_info_t *p_vdr )
{
    const uint8_t *p_peek;
    *pi_header_size = 0;
    int i_packet_size = -1;

    if( vlc_stream_Peek( p_demux->s,
                     &p_peek, TS_PACKET_SIZE_MAX ) < TS_PACKET_SIZE_MAX )
        return -1;

    if( memcmp( p_peek, "TFrc", 4 ) == 0 && p_peek[6] == 0 &&
        vlc_stream_Peek( p_demux->s, &p_peek, TOPFIELD_HEADER_SIZE + TS_PACKET_SIZE_MAX )
            == TOPFIELD_HEADER_SIZE + TS_PACKET_SIZE_MAX )
    {
        const int i_service = GetWBE(&p_peek[18]);
        i_packet_size = DetectPacketSize( p_demux, pi_header_size, TOPFIELD_HEADER_SIZE );
        if( i_packet_size != -1 )
        {
            msg_Dbg( p_demux, "this is a topfield file" );
#if 0
            /* I used the TF5000PVR 2004 Firmware .doc header documentation,
             * /doc/Structure%20of%20Recorded%20File%20in%20TF5000PVR%20(Feb%2021%202004).doc on streams.vo
             * but after the filename the offsets seem to be incorrect.  - DJ */
            int i_duration, i_name;
            char *psz_name = xmalloc(25);
            char *psz_event_name;
            char *psz_event_text = xmalloc(130);
            char *psz_ext_text = xmalloc(1025);

            // 2 bytes version Uimsbf (4,5)
            // 2 bytes reserved (6,7)
            // 2 bytes duration in minutes Uimsbf (8,9(
            i_duration = (int) (p_peek[8] << 8) | p_peek[9];
            msg_Dbg( p_demux, "Topfield recording length: +/- %d minutes", i_duration);
            // 2 bytes service number in channel list (10, 11)
            // 2 bytes service type Bslbf 0=TV 1=Radio Bslb (12, 13)
            // 4 bytes of reserved + tuner info (14,15,16,17)
            // 2 bytes of Service ID  Bslbf (18,19)
            // 2 bytes of PMT PID  Uimsbf (20,21)
            // 2 bytes of PCR PID  Uimsbf (22,23)
            // 2 bytes of Video PID  Uimsbf (24,25)
            // 2 bytes of Audio PID  Uimsbf (26,27)
            // 24 bytes filename Bslbf
            memcpy( psz_name, &p_peek[28], 24 );
            psz_name[24] = '\0';
            msg_Dbg( p_demux, "recordingname=%s", psz_name );
            // 1 byte of sat index Uimsbf  (52)
            // 3 bytes (1 bit of polarity Bslbf +23 bits reserved)
            // 4 bytes of freq. Uimsbf (56,57,58,59)
            // 2 bytes of symbol rate Uimsbf (60,61)
            // 2 bytes of TS stream ID Uimsbf (62,63)
            // 4 bytes reserved
            // 2 bytes reserved
            // 2 bytes duration Uimsbf (70,71)
            //i_duration = (int) (p_peek[70] << 8) | p_peek[71];
            //msg_Dbg( p_demux, "Topfield 2nd duration field: +/- %d minutes", i_duration);
            // 4 bytes EventID Uimsbf (72-75)
            // 8 bytes of Start and End time info (76-83)
            // 1 byte reserved (84)
            // 1 byte event name length Uimsbf (89)
            i_name = (int)(p_peek[89]&~0x81);
            msg_Dbg( p_demux, "event name length = %d", i_name);
            psz_event_name = xmalloc( i_name+1 );
            // 1 byte parental rating (90)
            // 129 bytes of event text
            memcpy( psz_event_name, &p_peek[91], i_name );
            psz_event_name[i_name] = '\0';
            memcpy( psz_event_text, &p_peek[91+i_name], 129-i_name );
            psz_event_text[129-i_name] = '\0';
            msg_Dbg( p_demux, "event name=%s", psz_event_name );
            msg_Dbg( p_demux, "event text=%s", psz_event_text );
            // 12 bytes reserved (220)
            // 6 bytes reserved
            // 2 bytes Event Text Length Uimsbf
            // 4 bytes EventID Uimsbf
            // FIXME We just have 613 bytes. not enough for this entire text
            // 1024 bytes Extended Event Text Bslbf
            memcpy( psz_ext_text, p_peek+372, 1024 );
            psz_ext_text[1024] = '\0';
            msg_Dbg( p_demux, "extended event text=%s", psz_ext_text );
            // 52 bytes reserved Bslbf
#endif
            p_vdr->i_service = i_service;

            return i_packet_size;
            //return TS_PACKET_SIZE_188;
        }
    }

    return DetectPacketSize( p_demux, pi_header_size, 0 );
}

/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    int          i_packet_size;
    unsigned     i_packet_header_size = 0;

    ts_pid_t    *patpid;
    vdr_info_t   vdr = {0};

    /* Search first sync byte */
    i_packet_size = DetectPVRHeadersAndHeaderSize( p_demux, &i_packet_header_size, &vdr );
    if( i_packet_size < 0 )
        return VLC_EGENERIC;

    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;
    memset( p_sys, 0, sizeof( demux_sys_t ) );
    vlc_mutex_init( &p_sys->csa_lock );

    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    /* Init p_sys field */
    p_sys->b_end_preparse = false;
    ARRAY_INIT( p_sys->programs );
    p_sys->b_default_selection = false;
    p_sys->i_network_time = 0;
    p_sys->i_network_time_update = 0;

    p_sys->vdr = vdr;

    p_sys->arib.b25stream = NULL;
    p_sys->stream = p_demux->s;

    p_sys->b_broken_charset = false;

    ts_pid_list_Init( &p_sys->pids );

    p_sys->i_packet_size = i_packet_size;
    p_sys->i_packet_header_size = i_packet_header_size;
    p_sys->i_ts_read = 50;
    p_sys->csa = NULL;
    p_sys->b_start_record = false;

    vlc_dictionary_init( &p_sys->attachments, 0 );

    p_sys->patfix.i_first_dts = -1;
    p_sys->patfix.i_timesourcepid = 0;
    p_sys->patfix.status = var_CreateGetBool( p_demux, "ts-patfix" ) ? PAT_WAITING : PAT_FIXTRIED;

    /* Init PAT handler */
    patpid = GetPID(p_sys, 0);
    if ( !PIDSetup( p_demux, TYPE_PAT, patpid, NULL ) )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }
    if( !ts_psi_PAT_Attach( patpid, p_demux ) )
    {
        PIDRelease( p_demux, patpid );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->b_access_control = true;
    p_sys->b_access_control = ( VLC_SUCCESS == SetPIDFilter( p_sys, patpid, true ) );

    p_sys->i_pmt_es = 0;
    p_sys->seltype = PROGRAM_AUTO_DEFAULT;

    /* Read config */
    p_sys->b_es_id_pid = var_CreateGetBool( p_demux, "ts-es-id-pid" );
    p_sys->i_next_extraid = 1;

    p_sys->b_trust_pcr = var_CreateGetBool( p_demux, "ts-trust-pcr" );
    p_sys->b_check_pcr_offset = p_sys->b_trust_pcr && var_CreateGetBool(p_demux, "ts-pcr-offsetfix" );
    p_sys->i_generated_pcr_dpb_offset = VLC_TICK_FROM_MS(var_CreateGetInteger( p_demux, "ts-generated-pcr-offset" ));

    /* We handle description of an extra PMT */
    char* psz_string = var_CreateGetString( p_demux, "ts-extra-pmt" );
    p_sys->b_user_pmt = false;
    if( psz_string && *psz_string )
        UserPmt( p_demux, psz_string );
    free( psz_string );

    psz_string = var_CreateGetStringCommand( p_demux, "ts-csa-ck" );
    if( psz_string && *psz_string )
    {
        int i_res;
        char* psz_csa2;

        p_sys->csa = csa_New();

        psz_csa2 = var_CreateGetStringCommand( p_demux, "ts-csa2-ck" );
        i_res = csa_SetCW( (vlc_object_t*)p_demux, p_sys->csa, psz_string, true );
        if( i_res == VLC_SUCCESS && psz_csa2 && *psz_csa2 )
        {
            if( csa_SetCW( (vlc_object_t*)p_demux, p_sys->csa, psz_csa2, false ) != VLC_SUCCESS )
            {
                csa_SetCW( (vlc_object_t*)p_demux, p_sys->csa, psz_string, false );
            }
        }
        else if ( i_res == VLC_SUCCESS )
        {
            csa_SetCW( (vlc_object_t*)p_demux, p_sys->csa, psz_string, false );
        }
        else
        {
            csa_Delete( p_sys->csa );
            p_sys->csa = NULL;
        }

        if( p_sys->csa )
        {
            var_AddCallback( p_demux, "ts-csa-ck", ChangeKeyCallback, (void *)1 );
            var_AddCallback( p_demux, "ts-csa2-ck", ChangeKeyCallback, NULL );

            int i_pkt = var_CreateGetInteger( p_demux, "ts-csa-pkt" );
            if( i_pkt < 4 || i_pkt > 188 )
            {
                msg_Err( p_demux, "wrong packet size %d specified.", i_pkt );
                msg_Warn( p_demux, "using default packet size of 188 bytes" );
                p_sys->i_csa_pkt_size = 188;
            }
            else
                p_sys->i_csa_pkt_size = i_pkt;
            msg_Dbg( p_demux, "decrypting %d bytes of packet", p_sys->i_csa_pkt_size );
        }
        free( psz_csa2 );
    }
    free( psz_string );

    p_sys->b_split_es = var_InheritBool( p_demux, "ts-split-es" );

    p_sys->b_canseek = false;
    p_sys->b_canfastseek = false;
    p_sys->b_lowdelay = var_InheritBool( p_demux, "low-delay" );
    p_sys->b_ignore_time_for_positions = var_InheritBool( p_demux, "ts-seek-percent" );
    p_sys->b_cc_check = var_InheritBool( p_demux, "ts-cc-check" );

    p_sys->standard = TS_STANDARD_AUTO;
    char *psz_standard = var_InheritString( p_demux, "ts-standard" );
    if( psz_standard )
    {
        for( unsigned i=0; i<ARRAY_SIZE(ts_standards_list); i++ )
        {
            if( !strcmp( psz_standard, ts_standards_list[i] ) )
            {
                TsChangeStandard( p_sys, TS_STANDARD_AUTO + i );
                msg_Dbg( p_demux, "Standard set to %s", ts_standards_list_text[i] );
                break;
            }
        }
        free( psz_standard );
    }

    if( p_sys->standard == TS_STANDARD_AUTO &&
       ( !strncasecmp( p_demux->psz_url, "atsc", 4 ) ||
         !strncasecmp( p_demux->psz_url, "usdigital", 9 ) ) )
    {
        TsChangeStandard( p_sys, TS_STANDARD_ATSC );
    }

    vlc_stream_Control( p_sys->stream, STREAM_CAN_SEEK, &p_sys->b_canseek );
    vlc_stream_Control( p_sys->stream, STREAM_CAN_FASTSEEK,
                        &p_sys->b_canfastseek );

    if( !p_sys->b_access_control && var_CreateGetBool( p_demux, "ts-pmtfix-waitdata" ) )
        p_sys->es_creation = DELAY_ES;
    else
        p_sys->es_creation = CREATE_ES;

    /* Preparse time */
    if( p_demux->b_preparsing && p_sys->b_canseek )
    {
        while( !p_sys->i_pmt_es && !p_sys->b_end_preparse )
            if( Demux( p_demux ) != VLC_DEMUXER_SUCCESS )
                break;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
static void FreeDictAttachment( void *p_value, void *p_obj )
{
    VLC_UNUSED(p_obj);
    vlc_input_attachment_Delete( (input_attachment_t *) p_value );
}

static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    PIDRelease( p_demux, GetPID(p_sys, 0) );

    vlc_mutex_lock( &p_sys->csa_lock );
    if( p_sys->csa )
    {
        var_DelCallback( p_demux, "ts-csa-ck", ChangeKeyCallback, (void *)1 );
        var_DelCallback( p_demux, "ts-csa2-ck", ChangeKeyCallback, NULL );
        csa_Delete( p_sys->csa );
    }
    vlc_mutex_unlock( &p_sys->csa_lock );

    ARRAY_RESET( p_sys->programs );

#ifdef HAVE_ARIBB24
    if ( p_sys->arib.p_instance )
        arib_instance_destroy( p_sys->arib.p_instance );
#endif

    if ( p_sys->arib.b25stream )
    {
        p_sys->arib.b25stream->s = NULL; /* don't chain kill demuxer's source */
        vlc_stream_Delete( p_sys->arib.b25stream );
    }

    /* Release all non default pids */
    ts_pid_list_Release( p_demux, &p_sys->pids );

    /* Clear up attachments */
    vlc_dictionary_clear( &p_sys->attachments, FreeDictAttachment, NULL );

    free( p_sys );
}

/*****************************************************************************
 * ChangeKeyCallback: called when changing the odd encryption key on the fly.
 *****************************************************************************/
static int ChangeKeyCallback( vlc_object_t *p_this, char const *psz_cmd,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval);
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;
    int         i_tmp = (intptr_t)p_data;

    vlc_mutex_lock( &p_sys->csa_lock );
    if ( i_tmp )
        i_tmp = csa_SetCW( p_this, p_sys->csa, newval.psz_string, true );
    else
        i_tmp = csa_SetCW( p_this, p_sys->csa, newval.psz_string, false );

    vlc_mutex_unlock( &p_sys->csa_lock );
    return i_tmp;
}

/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    bool b_wait_es = p_sys->i_pmt_es <= 0;

    /* If we had no PAT within MIN_PAT_INTERVAL, create PAT/PMT from probed streams */
    if( p_sys->i_pmt_es == 0 && !SEEN(GetPID(p_sys, 0)) && p_sys->patfix.status == PAT_MISSING )
    {
        MissingPATPMTFixup( p_demux );
        p_sys->patfix.status = PAT_FIXTRIED;
        GetPID(p_sys, 0)->u.p_pat->b_generated = true;
    }

    /* We read at most 100 TS packet or until a frame is completed */
    for( unsigned i_pkt = 0; i_pkt < p_sys->i_ts_read; i_pkt++ )
    {
        bool         b_frame = false;
        int          i_header = 0;
        block_t     *p_pkt;
        if( !(p_pkt = ReadTSPacket( p_demux )) )
        {
            return VLC_DEMUXER_EOF;
        }

        if( p_sys->b_start_record )
        {
            /* Enable recording once synchronized */
            vlc_stream_Control( p_sys->stream, STREAM_SET_RECORD_STATE, true,
                                "ts" );
            p_sys->b_start_record = false;
        }

        /* Early reject truncated packets from hw devices */
        if( unlikely(p_pkt->i_buffer < TS_PACKET_SIZE_188) )
        {
            block_Release( p_pkt );
            continue;
        }

        /* Reject any fully uncorrected packet. Even PID can be incorrect */
        if( p_pkt->p_buffer[1]&0x80 )
        {
            msg_Dbg( p_demux, "transport_error_indicator set (pid=%d)",
                     PIDGet( p_pkt ) );
            block_Release( p_pkt );
            continue;
        }

        /* Parse the TS packet */
        ts_pid_t *p_pid = GetPID( p_sys, PIDGet( p_pkt ) );
        if( !SEEN(p_pid) )
        {
            if( p_pid->type == TYPE_FREE )
                msg_Dbg( p_demux, "pid[%d] unknown", p_pid->i_pid );
            p_pid->i_flags |= FLAG_SEEN;
            if( p_pid->i_pid == 0x01 )
                p_sys->b_valid_scrambling = true;
        }

        /* Drop duplicates and invalid (DOES NOT drop corrupted) */
        p_pkt = ProcessTSPacket( p_demux, p_pid, p_pkt, &i_header );
        if( !p_pkt )
            continue;

        if( !SCRAMBLED(*p_pid) != !(p_pkt->i_flags & BLOCK_FLAG_SCRAMBLED) )
        {
            UpdatePIDScrambledState( p_demux, p_pid, p_pkt->i_flags & BLOCK_FLAG_SCRAMBLED );
        }

        /* Adaptation field cannot be scrambled */
        stime_t i_pcr = GetPCR( p_pkt );
        if( i_pcr >= 0 )
            PCRHandle( p_demux, p_pid, i_pcr );

        /* Probe streams to build PAT/PMT after MIN_PAT_INTERVAL in case we don't see any PAT */
        if( !SEEN( GetPID( p_sys, 0 ) ) &&
            (p_pid->probed.i_fourcc == 0 || p_pid->i_pid == p_sys->patfix.i_timesourcepid) &&
            (p_pkt->p_buffer[1] & 0xC0) == 0x40 && /* Payload start but not corrupt */
            (p_pkt->p_buffer[3] & 0xD0) == 0x10 )  /* Has payload but is not encrypted */
        {
            ProbePES( p_demux, p_pid, p_pkt->p_buffer + TS_HEADER_SIZE,
                      p_pkt->i_buffer - TS_HEADER_SIZE, p_pkt->p_buffer[3] & 0x20 /* Adaptation field */);
        }

        switch( p_pid->type )
        {
        case TYPE_PAT:
        case TYPE_PMT:
            /* PAT and PMT are not allowed to be scrambled */
            ts_psi_Packet_Push( p_pid, p_pkt->p_buffer );
            block_Release( p_pkt );
            break;

        case TYPE_STREAM:
            p_sys->b_end_preparse = true;

            if( p_sys->es_creation == DELAY_ES ) /* No longer delay ES since that pid's program sends data */
            {
                msg_Dbg( p_demux, "Creating delayed ES" );
                AddAndCreateES( p_demux, p_pid, true );
                UpdatePESFilters( p_demux, p_sys->seltype == PROGRAM_ALL );
            }

            /* Emulate HW filter */
            if( !p_sys->b_access_control && !(p_pid->i_flags & FLAG_FILTERED) )
            {
                /* That packet is for an unselected ES, don't waste time/memory gathering its data */
                block_Release( p_pkt );
                continue;
            }

            if( p_pid->u.p_stream->transport == TS_TRANSPORT_PES )
            {
                b_frame = GatherPESData( p_demux, p_pid, p_pkt, i_header );
            }
            else if( p_pid->u.p_stream->transport == TS_TRANSPORT_SECTIONS )
            {
                b_frame = GatherSectionsData( p_demux, p_pid, p_pkt, i_header );
            }
            else // pid->u.p_pes->transport == TS_TRANSPORT_IGNORE
            {
                block_Release( p_pkt );
            }

            break;

        case TYPE_SI:
            if( (p_pkt->i_flags & (BLOCK_FLAG_SCRAMBLED|BLOCK_FLAG_CORRUPTED)) == 0 )
                ts_si_Packet_Push( p_pid, p_pkt->p_buffer );
            block_Release( p_pkt );
            break;

        case TYPE_PSIP:
            if( (p_pkt->i_flags & (BLOCK_FLAG_SCRAMBLED|BLOCK_FLAG_CORRUPTED)) == 0 )
                ts_psip_Packet_Push( p_pid, p_pkt->p_buffer );
            block_Release( p_pkt );
            break;

        case TYPE_CAT:
        default:
            /* We have to handle PCR if present */
            block_Release( p_pkt );
            break;
        }

        if( b_frame || ( b_wait_es && p_sys->i_pmt_es > 0 ) )
            break;
    }

    demux_UpdateTitleFromStream( p_demux );
    return VLC_DEMUXER_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int EITCurrentEventTime( const ts_pmt_t *p_pmt, demux_sys_t *p_sys,
                                time_t *pi_time, time_t *pi_length )
{
    if( p_sys->i_network_time == 0 || !p_pmt || p_pmt->eit.i_event_length == 0 )
        return VLC_EGENERIC;

    if( p_pmt->eit.i_event_start <= p_sys->i_network_time &&
        p_sys->i_network_time < p_pmt->eit.i_event_start + p_pmt->eit.i_event_length )
    {
        if( pi_length )
            *pi_length = p_pmt->eit.i_event_length;
        if( pi_time )
        {
            *pi_time = p_sys->i_network_time - p_pmt->eit.i_event_start;
            *pi_time += time(NULL) - p_sys->i_network_time_update;
        }
        return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}

static inline void HasSelectedES( es_out_t *out, const ts_es_t *p_es,
                                  const ts_pmt_t *p_pmt, bool *pb_stream_selected )
{
    for( ; p_es && !*pb_stream_selected; p_es = p_es->p_next )
    {
        if( p_es->id )
            es_out_Control( out, ES_OUT_GET_ES_STATE,
                            p_es->id, pb_stream_selected );
        HasSelectedES( out, p_es->p_extraes, p_pmt, pb_stream_selected );
    }
}

void UpdatePESFilters( demux_t *p_demux, bool b_all )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    ts_pat_t *p_pat = GetPID(p_sys, 0)->u.p_pat;

    /* We need 3 pass to avoid loss on deselect/relesect with hw filters and
       because pid could be shared and its state altered by another unselected pmt
       First clear flag on every referenced pid
       Then add flag if non on each selected pmt/pcr and active es
       Then commit it at hardware level if any */

    /* clear selection flag on every pmt referenced pid */
    for( int i=0; i< p_pat->programs.i_size; i++ )
    {
        ts_pid_t *p_pmt_pid = p_pat->programs.p_elems[i];
        ts_pmt_t *p_pmt = p_pmt_pid->u.p_pmt;

        p_pmt_pid->i_flags &= ~FLAG_FILTERED;
        for( int j=0; j< p_pmt->e_streams.i_size; j++ )
            p_pmt->e_streams.p_elems[j]->i_flags &= ~FLAG_FILTERED;
        GetPID(p_sys, p_pmt->i_pid_pcr)->i_flags &= ~FLAG_FILTERED;
    }

    /* set selection flag on selected pmt referenced pid with active es */
    for( int i=0; i< p_pat->programs.i_size; i++ )
    {
        ts_pid_t *p_pmt_pid = p_pat->programs.p_elems[i];
        ts_pmt_t *p_pmt = p_pmt_pid->u.p_pmt;

        if( (p_sys->b_default_selection && !p_sys->b_access_control) || b_all )
             p_pmt->b_selected = true;
        else
             p_pmt->b_selected = ProgramIsSelected( p_sys, p_pmt->i_number );

        if( p_pmt->b_selected )
        {
            p_pmt_pid->i_flags |= FLAG_FILTERED;

            for( int j=0; j<p_pmt->e_streams.i_size; j++ )
            {
                ts_pid_t *espid = p_pmt->e_streams.p_elems[j];
                ts_stream_t *p_pes = espid->u.p_stream;

                bool b_stream_selected = true;
                if( !p_pes->b_always_receive && !b_all )
                    HasSelectedES( p_demux->out, p_pes->p_es, p_pmt, &b_stream_selected );

                if( b_stream_selected )
                {
                    msg_Dbg( p_demux, "enabling pid %d from program %d", espid->i_pid, p_pmt->i_number );
                    espid->i_flags |= FLAG_FILTERED;
                }
            }

            /* Select pcr last in case it is handled by unselected ES */
            if( p_pmt->i_pid_pcr > 0 )
            {
                GetPID(p_sys, p_pmt->i_pid_pcr)->i_flags |= FLAG_FILTERED;
                msg_Dbg( p_demux, "enabling pcr pid %d from program %d", p_pmt->i_pid_pcr, p_pmt->i_number );
            }
        }
    }

    /* Commit HW changes based on flags */
    for( int i=0; i< p_pat->programs.i_size; i++ )
    {
        ts_pid_t *p_pmt_pid = p_pat->programs.p_elems[i];
        ts_pmt_t *p_pmt = p_pmt_pid->u.p_pmt;

        UpdateHWFilter( p_sys, p_pmt_pid );
        for( int j=0; j< p_pmt->e_streams.i_size; j++ )
        {
            ts_pid_t *espid = p_pmt->e_streams.p_elems[j];
            UpdateHWFilter( p_sys, espid );
            if( (espid->i_flags & FLAG_FILTERED) == 0 )
                FlushESBuffer( espid->u.p_stream );
        }
        UpdateHWFilter( p_sys, GetPID(p_sys, p_pmt->i_pid_pcr) );
    }
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double f, *pf;
    bool b_bool, *pb_bool;
    int64_t i64;
    int i_int;
    const ts_pmt_t *p_pmt = NULL;
    const ts_pat_t *p_pat = GetPID(p_sys, 0)->u.p_pat;

    for( int i=0; i<p_pat->programs.i_size && !p_pmt; i++ )
    {
        if( p_pat->programs.p_elems[i]->u.p_pmt->b_selected )
            p_pmt = p_pat->programs.p_elems[i]->u.p_pmt;
    }

    switch( i_query )
    {
    case DEMUX_CAN_SEEK:
        *va_arg( args, bool * ) = p_sys->b_canseek;
        return VLC_SUCCESS;

    case DEMUX_GET_TITLE:
        *va_arg( args, int * ) = p_sys->current_title;
        return VLC_SUCCESS;

    case DEMUX_GET_SEEKPOINT:
        *va_arg( args, int * ) = p_sys->current_seekpoint;
        return VLC_SUCCESS;

    case DEMUX_GET_POSITION:
        pf = va_arg( args, double * );

        /* Access control test is because EPG for recordings is not relevant */
        if( p_sys->b_access_control )
        {
            time_t i_time, i_length;
            if( !EITCurrentEventTime( p_pmt, p_sys, &i_time, &i_length ) && i_length > 0 )
            {
                *pf = (double)i_time/(double)i_length;
                return VLC_SUCCESS;
            }
        }

        if( !p_sys->b_ignore_time_for_positions &&
             p_pmt &&
             p_pmt->pcr.i_first > -1 && SETANDVALID(p_pmt->i_last_dts) &&
             p_pmt->pcr.i_current > -1 )
        {
            double i_length = TimeStampWrapAround( p_pmt->pcr.i_first,
                                                   p_pmt->i_last_dts ) - p_pmt->pcr.i_first;
            i_length += p_pmt->pcr.i_pcroffset;
            double i_pos = TimeStampWrapAround( p_pmt->pcr.i_first,
                                                p_pmt->pcr.i_current ) - p_pmt->pcr.i_first;
            if( i_length > 0 )
            {
                *pf = i_pos / i_length;
                return VLC_SUCCESS;
            }
        }

        if( (i64 = stream_Size( p_sys->stream) ) > 0 )
        {
            uint64_t offset = vlc_stream_Tell( p_sys->stream );
            *pf = (double)offset / (double)i64;
            return VLC_SUCCESS;
        }
        break;

    case DEMUX_SET_POSITION:
        f = va_arg( args, double );
        b_bool = (bool) va_arg( args, int ); /* precise */

        if(!p_sys->b_canseek)
            break;

        if( p_sys->b_access_control &&
           !p_sys->b_ignore_time_for_positions && b_bool && p_pmt )
        {
            time_t i_time, i_length;
            if( !EITCurrentEventTime( p_pmt, p_sys, &i_time, &i_length ) &&
                 i_length > 0 && !SeekToTime( p_demux, p_pmt, (int64_t)(TO_SCALE( vlc_tick_from_sec( i_length * f ))) ) )
            {
                ReadyQueuesPostSeek( p_demux );
                es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME,
                                (int64_t)(TO_SCALE( vlc_tick_from_sec( i_length * f ))) );
                return VLC_SUCCESS;
            }
        }

        if( !p_sys->b_ignore_time_for_positions && b_bool && p_pmt &&
             p_pmt->pcr.i_first > -1 && SETANDVALID(p_pmt->i_last_dts) &&
             p_pmt->pcr.i_current > -1 )
        {
            stime_t i_length = TimeStampWrapAround( p_pmt->pcr.i_first,
                                                    p_pmt->i_last_dts ) - p_pmt->pcr.i_first;
            i64 = p_pmt->pcr.i_first + (int64_t)(i_length * f);
            if( i64 <= p_pmt->i_last_dts )
            {
                if( !SeekToTime( p_demux, p_pmt, i64 ) )
                {
                    ReadyQueuesPostSeek( p_demux );
                    es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME, FROM_SCALE(i64) );
                    return VLC_SUCCESS;
                }
            }
        }

        i64 = stream_Size( p_sys->stream );
        if( i64 > 0 &&
            vlc_stream_Seek( p_sys->stream, (int64_t)(i64 * f) ) == VLC_SUCCESS )
        {
            ReadyQueuesPostSeek( p_demux );
            return VLC_SUCCESS;
        }
        break;

    case DEMUX_SET_TIME:
    {
        vlc_tick_t i_time = va_arg( args, vlc_tick_t );

        if( p_sys->b_canseek && p_pmt && p_pmt->pcr.i_first > -1 &&
           !SeekToTime( p_demux, p_pmt, p_pmt->pcr.i_first + TO_SCALE(i_time) ) )
        {
            ReadyQueuesPostSeek( p_demux );
            es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME,
                            FROM_SCALE(p_pmt->pcr.i_first) + i_time - VLC_TICK_0 );
            return VLC_SUCCESS;
        }
        break;
    }

    case DEMUX_GET_TIME:
        if( p_sys->b_access_control )
        {
            time_t i_event_start;
            if( !EITCurrentEventTime( p_pmt, p_sys, &i_event_start, NULL ) )
            {
                *va_arg( args, vlc_tick_t * ) = vlc_tick_from_sec( i_event_start );
                return VLC_SUCCESS;
            }
        }

        if( p_pmt && p_pmt->pcr.i_current > -1 && p_pmt->pcr.i_first > -1 )
        {
            stime_t i_pcr = TimeStampWrapAround( p_pmt->pcr.i_first, p_pmt->pcr.i_current );
            *va_arg( args, vlc_tick_t * ) = FROM_SCALE(i_pcr - p_pmt->pcr.i_first);
            return VLC_SUCCESS;
        }
        break;
    case DEMUX_GET_NORMAL_TIME:
        if ((p_sys->b_access_control && !EITCurrentEventTime( p_pmt, p_sys, NULL, NULL))
         || (!p_pmt || p_pmt->pcr.i_current == -1 || p_pmt->pcr.i_first == -1))
            return VLC_EGENERIC; /* use VLC_TICK_0 as Normal Play Time*/

        /* Use the first pcr of the current program as Normal Play Time */
        *va_arg( args, vlc_tick_t * ) = FROM_SCALE( p_pmt->pcr.i_first );
        return VLC_SUCCESS;

    case DEMUX_GET_LENGTH:
        if( p_sys->b_access_control )
        {
            time_t i_event_duration;
            if( !EITCurrentEventTime( p_pmt, p_sys, NULL, &i_event_duration ) )
            {
                *va_arg( args, vlc_tick_t * ) = vlc_tick_from_sec( i_event_duration );
                return VLC_SUCCESS;
            }
        }

        if( !p_sys->b_ignore_time_for_positions &&
            p_pmt &&
           ( p_pmt->pcr.i_first > -1 || p_pmt->pcr.i_first_dts != -1 ) &&
             p_pmt->i_last_dts > 0 )
        {
            stime_t i_start = (p_pmt->pcr.i_first > -1) ? p_pmt->pcr.i_first :
                              p_pmt->pcr.i_first_dts;
            stime_t i_last = TimeStampWrapAround( p_pmt->pcr.i_first, p_pmt->i_last_dts );
            i_last += p_pmt->pcr.i_pcroffset;
            *va_arg( args, vlc_tick_t * ) = FROM_SCALE(i_last - i_start);
            return VLC_SUCCESS;
        }
        break;

    case DEMUX_SET_GROUP_DEFAULT:
        msg_Dbg( p_demux, "DEMUX_SET_GROUP_%s", "DEFAULT" );

        if( !p_sys->b_default_selection )
        {
            ARRAY_RESET( p_sys->programs );
            p_sys->seltype = PROGRAM_AUTO_DEFAULT;
        }
        return VLC_SUCCESS;

    case DEMUX_SET_GROUP_ALL: // All ES Mode
    {
        msg_Dbg( p_demux, "DEMUX_SET_GROUP_%s", "ALL" );

        ARRAY_RESET( p_sys->programs );
        p_pat = GetPID(p_sys, 0)->u.p_pat;
        for( int i = 0; i < p_pat->programs.i_size; i++ )
             ARRAY_APPEND( p_sys->programs, p_pat->programs.p_elems[i]->i_pid );
        p_sys->seltype = PROGRAM_ALL;
        UpdatePESFilters( p_demux, true );

        p_sys->b_default_selection = false;
        return VLC_SUCCESS;
    }

    case DEMUX_SET_GROUP_LIST:
    {
        size_t count = va_arg(args, size_t);
        const int *pids = va_arg(args, const int *);

        msg_Dbg( p_demux, "DEMUX_SET_GROUP_%s", "LIST" );

        /* Deselect/filter current ones */
        ARRAY_RESET( p_sys->programs );
        p_sys->seltype = PROGRAM_LIST;
        for( size_t i = 0; i < count; i++ )
            ARRAY_APPEND( p_sys->programs, pids[i] );
        UpdatePESFilters( p_demux, false );

        p_sys->b_default_selection = false;
        return VLC_SUCCESS;
    }

    case DEMUX_SET_ES:
    case DEMUX_SET_ES_LIST:
    {
        if( i_query == DEMUX_SET_ES )
        {
            i_int = va_arg( args, int );
            msg_Dbg( p_demux, "DEMUX_SET_ES %d", i_int );
        }
        else
            msg_Dbg( p_demux, "DEMUX_SET_ES_LIST" );

        if( p_sys->seltype != PROGRAM_ALL ) /* Won't change anything */
            UpdatePESFilters( p_demux, false );

        return VLC_SUCCESS;
    }

    case DEMUX_GET_TITLE_INFO:
    {
        struct input_title_t ***v = va_arg( args, struct input_title_t*** );
        int *c = va_arg( args, int * );

        *va_arg( args, int* ) = 0; /* Title offset */
        *va_arg( args, int* ) = 0; /* Chapter offset */
        return vlc_stream_Control( p_sys->stream, STREAM_GET_TITLE_INFO, v,
                                   c );
    }

    case DEMUX_SET_TITLE:
        return vlc_stream_vaControl( p_sys->stream, STREAM_SET_TITLE, args );

    case DEMUX_SET_SEEKPOINT:
        return vlc_stream_vaControl( p_sys->stream, STREAM_SET_SEEKPOINT,
                                     args );

    case DEMUX_TEST_AND_CLEAR_FLAGS:
    {
        unsigned *restrict flags = va_arg(args, unsigned *);
        *flags &= p_sys->updates;
        p_sys->updates &= ~*flags;
        return VLC_SUCCESS;
    }

    case DEMUX_GET_META:
        return vlc_stream_vaControl( p_sys->stream, STREAM_GET_META, args );

    case DEMUX_CAN_RECORD:
        pb_bool = va_arg( args, bool * );
        *pb_bool = true;
        return VLC_SUCCESS;

    case DEMUX_SET_RECORD_STATE:
        b_bool = va_arg( args, int );

        if( !b_bool )
            vlc_stream_Control( p_sys->stream, STREAM_SET_RECORD_STATE,
                                false );
        p_sys->b_start_record = b_bool;
        return VLC_SUCCESS;

    case DEMUX_GET_SIGNAL:
        return vlc_stream_vaControl( p_sys->stream, STREAM_GET_SIGNAL, args );

    case DEMUX_GET_ATTACHMENTS:
    {
        input_attachment_t ***ppp_attach = va_arg( args, input_attachment_t *** );
        int *pi_int = va_arg( args, int * );

        *pi_int = vlc_dictionary_keys_count( &p_sys->attachments );
        if( *pi_int <= 0 )
            return VLC_EGENERIC;

        *ppp_attach = vlc_alloc( *pi_int, sizeof(input_attachment_t*) );
        if( !*ppp_attach )
            return VLC_EGENERIC;

        *pi_int = 0;
        for( int i = 0; i < p_sys->attachments.i_size; i++ )
        {
            for( vlc_dictionary_entry_t *p_entry = p_sys->attachments.p_entries[i];
                                         p_entry; p_entry = p_entry->p_next )
            {
                msg_Err(p_demux, "GET ATTACHMENT %s", p_entry->psz_key);
                (*ppp_attach)[*pi_int] = vlc_input_attachment_Duplicate(
                                                (input_attachment_t *) p_entry->p_value );
                if( (*ppp_attach)[*pi_int] )
                    (*pi_int)++;
            }
        }

        return VLC_SUCCESS;
    }

    case DEMUX_CAN_PAUSE:
    case DEMUX_SET_PAUSE_STATE:
    case DEMUX_CAN_CONTROL_PACE:
    case DEMUX_GET_PTS_DELAY:
        return demux_vaControlHelper( p_demux->s, 0, -1, 0, 1, i_query, args );

    default:
        break;
    }

    return VLC_EGENERIC;
}

/*****************************************************************************
 *
 *****************************************************************************/
static int16_t read_opus_flag(uint8_t **buf, size_t *len)
{
    if (*len < 2)
        return -1;

    int16_t ret = ((*buf)[0] << 8) | (*buf)[1];

    *len -= 2;
    *buf += 2;

    if (ret & (3<<13))
        ret = -1;

    return ret;
}

static block_t *Opus_Parse(demux_t *demux, block_t *block)
{
    block_t *p_chain = NULL;
    block_t **pp_chain_last = &p_chain;

    uint8_t *buf = block->p_buffer;
    size_t len = block->i_buffer;

    while (len > 3 && ((buf[0] << 3) | (buf[1] >> 5)) == 0x3ff) {
        int16_t start_trim = 0, end_trim = 0;
        int start_trim_flag        = (buf[1] >> 4) & 1;
        int end_trim_flag          = (buf[1] >> 3) & 1;
        int control_extension_flag = (buf[1] >> 2) & 1;

        len -= 2;
        buf += 2;

        unsigned au_size = 0;
        while (len--) {
            int c = *buf++;
            au_size += c;
            if (c != 0xff)
                break;
        }

        if (start_trim_flag) {
            start_trim = read_opus_flag(&buf, &len);
            if (start_trim < 0) {
                msg_Err(demux, "Invalid start trimming flag");
            }
        }
        if (end_trim_flag) {
            end_trim = read_opus_flag(&buf, &len);
            if (end_trim < 0) {
                msg_Err(demux, "Invalid end trimming flag");
            }
        }
        if (control_extension_flag && len) {
            unsigned l = *buf++; len--;
            if (l > len) {
                msg_Err(demux, "Invalid control extension length %d > %zu", l, len);
                break;
            }
            buf += l;
            len -= l;
        }

        if (!au_size || au_size > len) {
            msg_Err(demux, "Invalid Opus AU size %d (PES %zu)", au_size, len);
            break;
        }

        block_t *au = block_Alloc(au_size);
        if (!au)
            break;
        memcpy(au->p_buffer, buf, au_size);
        block_CopyProperties(au, block);
        block_ChainLastAppend( &pp_chain_last, au );

        au->i_nb_samples = opus_frame_duration(buf, au_size);
        if (end_trim && (uint16_t) end_trim <= au->i_nb_samples)
            au->i_length = end_trim; /* Blatant abuse of the i_length field. */
        else
            au->i_length = 0;

        if (start_trim && start_trim < (au->i_nb_samples - au->i_length)) {
            au->i_nb_samples -= start_trim;
            if (au->i_nb_samples == 0)
                au->i_flags |= BLOCK_FLAG_PREROLL;
        }

        buf += au_size;
        len -= au_size;
    }

    block_Release(block);
    return p_chain;
}

static block_t *J2K_Parse( demux_t *p_demux, block_t *p_block, bool b_interlaced )
{
    const uint8_t *p_buf = p_block->p_buffer;

    if( p_block->i_buffer < ((b_interlaced) ? 48 : 38) )
        goto invalid;

    if( memcmp( p_buf, "elsmfrat", 8 ) )
        goto invalid;

    uint16_t i_den = GetWBE( &p_buf[8] );
    uint16_t i_num = GetWBE( &p_buf[10] );
    if( i_den == 0 )
        goto invalid;
    p_block->i_length = vlc_tick_from_samples( i_den, i_num );

    p_block->p_buffer += (b_interlaced) ? 48 : 38;
    p_block->i_buffer -= (b_interlaced) ? 48 : 38;

    return p_block;

invalid:
    msg_Warn( p_demux, "invalid J2K header, dropping codestream" );
    block_Release( p_block );
    return NULL;
}

static vlc_tick_t GetTimeForUntimed( const ts_pmt_t *p_pmt )
{
    vlc_tick_t i_ts = p_pmt->pcr.i_current;
    const ts_stream_t *p_cand = NULL;
    for( int i=0; i< p_pmt->e_streams.i_size; i++ )
    {
        const ts_pid_t *p_pid = p_pmt->e_streams.p_elems[i];
        if( (p_pid->i_flags & FLAG_FILTERED) && SEEN(p_pid) &&
             p_pid->type == TYPE_STREAM &&
             p_pid->u.p_stream->p_es &&
             SETANDVALID(p_pid->u.p_stream->i_last_dts) )
        {
            const ts_es_t *p_es = p_pid->u.p_stream->p_es;
            if( p_es->fmt.i_cat == VIDEO_ES || p_es->fmt.i_cat == AUDIO_ES )
            {
                if( !p_cand || (p_es->fmt.i_cat == VIDEO_ES &&
                                p_cand->p_es->fmt.i_cat != VIDEO_ES) )
                {
                    p_cand = p_pid->u.p_stream;
                    i_ts = p_cand->i_last_dts;
                }
            }
        }
    }
    return i_ts;
}

static block_t * ConvertPESBlock( demux_t *p_demux, ts_es_t *p_es,
                                  size_t i_pes_size, uint8_t i_stream_id,
                                  block_t *p_block )
{
    if(!p_block)
        return NULL;

    if( p_es->fmt.i_codec == VLC_CODEC_SUBT )
    {
        if( i_pes_size > 0 && p_block->i_buffer > i_pes_size )
        {
            p_block->i_buffer = i_pes_size;
        }
        /* Append a \0 */
        p_block = block_Realloc( p_block, 0, p_block->i_buffer + 1 );
        if( p_block )
            p_block->p_buffer[p_block->i_buffer -1] = '\0';
    }
    else if( p_es->fmt.i_codec == VLC_CODEC_TELETEXT )
    {
        const ts_pmt_t *p_pmt = p_es->p_program;
        if( p_block->i_pts != VLC_TICK_INVALID &&
            p_pmt->pcr.i_current > -1 )
        {
            /* Teletext can have totally offset timestamps... RAI1, German */
            vlc_tick_t i_pcr = FROM_SCALE(TimeStampWrapAround( p_pmt->pcr.i_first,
                                                               p_pmt->pcr.i_current ));
            if( i_pcr < p_block->i_pts || i_pcr - p_block->i_pts > CLOCK_FREQ )
                p_block->i_dts = p_block->i_pts = VLC_TICK_INVALID;
        }
        if( p_block->i_pts == VLC_TICK_INVALID )
        {
            /* Teletext may have missing PTS (ETSI EN 300 472 Annexe A)
             * In this case use the last PCR + 40ms */
            stime_t i_ts = GetTimeForUntimed( p_es->p_program );
            if( SETANDVALID(i_ts) )
            {
                i_ts = TimeStampWrapAround( p_pmt->pcr.i_first, i_ts );
                p_block->i_dts = p_block->i_pts = FROM_SCALE(i_ts) + VLC_TICK_FROM_MS(40);
            }
        }
    }
    else if( p_es->fmt.i_codec == VLC_CODEC_ARIB_A ||
             p_es->fmt.i_codec == VLC_CODEC_ARIB_C )
    {
        if( p_block->i_pts == VLC_TICK_INVALID )
        {
            if( i_pes_size > 0 && p_block->i_buffer > i_pes_size )
            {
                p_block->i_buffer = i_pes_size;
            }
            /* Append a \0 */
            p_block = block_Realloc( p_block, 0, p_block->i_buffer + 1 );
            if( p_block )
                p_block->p_buffer[p_block->i_buffer -1] = '\0';
        }
    }
    else if( p_es->fmt.i_codec == VLC_CODEC_OPUS)
    {
        p_block = Opus_Parse(p_demux, p_block);
    }
    else if( p_es->fmt.i_codec == VLC_CODEC_JPEG2000 )
    {
        if( unlikely(i_stream_id != 0xBD) )
        {
            block_Release( p_block );
            p_block = NULL;
        }
        else
        {
            p_block = J2K_Parse( p_demux, p_block, p_es->b_interlaced );
        }
    }

    return p_block;
}

/****************************************************************************
 * fanouts current block to all subdecoders / shared pid es
 ****************************************************************************/
static void SendDataChain( demux_t *p_demux, ts_es_t *p_es, block_t *p_chain )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    while( p_chain )
    {
        block_t *p_block = p_chain;
        p_chain = p_block->p_next;
        p_block->p_next = NULL;

        /* clean up any private flag */
        p_block->i_flags &= ~BLOCK_FLAG_PRIVATE_MASK;

        if( p_sys->b_lowdelay )
            p_block->i_flags |= BLOCK_FLAG_AU_END;

        ts_es_t *p_es_send = p_es;
        if( p_es_send->i_next_block_flags )
        {
            p_block->i_flags |= p_es_send->i_next_block_flags;
            p_es_send->i_next_block_flags = 0;
        }

        while( p_es_send )
        {
            if( p_es_send->p_program->b_selected )
            {
                /* Send a copy to each extra es */
                ts_es_t *p_extra_es = p_es_send->p_extraes;
                while( p_extra_es )
                {
                    if( p_extra_es->id )
                    {
                        block_t *p_dup = block_Duplicate( p_block );
                        if( p_dup )
                            es_out_Send( p_demux->out, p_extra_es->id, p_dup );
                    }
                    p_extra_es = p_extra_es->p_next;
                }

                if( p_es_send->p_next )
                {
                    if( p_es_send->id )
                    {
                        block_t *p_dup = block_Duplicate( p_block );
                        if( p_dup )
                            es_out_Send( p_demux->out, p_es_send->id, p_dup );
                    }
                }
                else
                {
                    if( p_es_send->id )
                    {
                        es_out_Send( p_demux->out, p_es_send->id, p_block );
                        p_block = NULL;
                    }
                }
            }
            p_es_send = p_es_send->p_next;
        }

        if( p_block )
            block_Release( p_block );
    }
}

/****************************************************************************
 * gathering stuff
 ****************************************************************************/
static void ParsePESDataChain( demux_t *p_demux, ts_pid_t *pid, block_t *p_pes )
{
    uint8_t header[34];
    unsigned i_pes_size = 0;
    unsigned i_skip = 0;
    stime_t i_dts = -1;
    stime_t i_pts = -1;
    vlc_tick_t i_length = 0;
    uint8_t i_stream_id;
    bool b_pes_scrambling = false;
    const es_mpeg4_descriptor_t *p_mpeg4desc = NULL;
    demux_sys_t *p_sys = p_demux->p_sys;

    assert(pid->type == TYPE_STREAM);

    const int i_max = block_ChainExtract( p_pes, header, 34 );
    if ( i_max < 4 )
    {
        block_ChainRelease( p_pes );
        return;
    }

    if( header[0] != 0 || header[1] != 0 || header[2] != 1 )
    {
        if ( !(p_pes->i_flags & BLOCK_FLAG_SCRAMBLED) )
            msg_Warn( p_demux, "invalid header [0x%02x:%02x:%02x:%02x] (pid: %d)",
                        header[0], header[1],header[2],header[3], pid->i_pid );
        block_ChainRelease( p_pes );
        return;
    }
    else
    {
        /* Incorrect transport scrambling flag set by german boxes */
        p_pes->i_flags &= ~BLOCK_FLAG_SCRAMBLED;
    }

    ts_es_t *p_es = pid->u.p_stream->p_es;

    if( ParsePESHeader( VLC_OBJECT(p_demux), (uint8_t*)&header, i_max, &i_skip,
                        &i_dts, &i_pts, &i_stream_id, &b_pes_scrambling ) == VLC_EGENERIC )
    {
        block_ChainRelease( p_pes );
        return;
    }
    else
    {
        if( i_pts != -1 && p_es->p_program )
            i_pts = TimeStampWrapAround( p_es->p_program->pcr.i_first, i_pts );
        if( i_dts != -1 && p_es->p_program )
            i_dts = TimeStampWrapAround( p_es->p_program->pcr.i_first, i_dts );
        if( b_pes_scrambling )
            p_pes->i_flags |= BLOCK_FLAG_SCRAMBLED;
    }

    if( p_es->i_sl_es_id )
        p_mpeg4desc = GetMPEG4DescByEsId( p_es->p_program, p_es->i_sl_es_id );

    if( p_es->fmt.i_codec == VLC_FOURCC( 'a', '5', '2', 'b' ) ||
        p_es->fmt.i_codec == VLC_FOURCC( 'd', 't', 's', 'b' ) )
    {
        i_skip += 4;
    }
    else if( p_es->fmt.i_codec == VLC_FOURCC( 'l', 'p', 'c', 'b' ) ||
             p_es->fmt.i_codec == VLC_FOURCC( 's', 'p', 'u', 'b' ) ||
             p_es->fmt.i_codec == VLC_FOURCC( 's', 'd', 'd', 'b' ) )
    {
        i_skip += 1;
    }
    else if( p_es->fmt.i_codec == VLC_CODEC_SUBT && p_mpeg4desc )
    {
        const decoder_config_descriptor_t *dcd = &p_mpeg4desc->dec_descr;

        if( dcd->i_extra > 2 &&
            dcd->p_extra[0] == 0x10 &&
            ( dcd->p_extra[1]&0x10 ) )
        {
            /* display length */
            if( p_pes->i_buffer + 2 <= i_skip )
                i_length = GetWBE( &p_pes->p_buffer[i_skip] );

            i_skip += 2;
        }
        if( p_pes->i_buffer + 2 <= i_skip )
            i_pes_size = GetWBE( &p_pes->p_buffer[i_skip] );
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

    if( i_dts >= 0 )
        pid->u.p_stream->i_last_dts = i_dts;

    if( p_pes )
    {
        ts_pmt_t *p_pmt = p_es->p_program;
        if( unlikely(!p_pmt) )
        {
            block_ChainRelease( p_pes );
            return;
        }

        if( i_dts >= 0 )
            p_pes->i_dts = FROM_SCALE(i_dts);

        if( i_pts >= 0 )
            p_pes->i_pts = FROM_SCALE(i_pts);

        p_pes->i_length = FROM_SCALE_NZ(i_length);

        /* Can become a chain on next call due to prepcr */
        block_t *p_chain = block_ChainGather( p_pes );
        while ( p_chain ) {
            block_t *p_block = p_chain;
            p_chain = p_chain->p_next;
            p_block->p_next = NULL;

            if( !p_pmt->pcr.b_fix_done ) /* Not seen yet */
                PCRFixHandle( p_demux, p_pmt, p_block );

            if( p_es->id && (p_pmt->pcr.i_current > -1 || p_pmt->pcr.b_disable) )
            {
                if( pid->u.p_stream->prepcr.p_head )
                {
                    /* Rebuild current output chain, appending any prepcr outqueue */
                    block_ChainLastAppend( &pid->u.p_stream->prepcr.pp_last, p_block );
                    if( p_chain )
                        block_ChainLastAppend( &pid->u.p_stream->prepcr.pp_last, p_chain );
                    p_chain = pid->u.p_stream->prepcr.p_head;
                    pid->u.p_stream->prepcr.p_head = NULL;
                    pid->u.p_stream->prepcr.pp_last = &pid->u.p_stream->prepcr.p_head;
                    /* Then now output all data */
                    continue;
                }

                if ( p_pmt->pcr.b_disable && p_block->i_dts != VLC_TICK_INVALID &&
                     ( p_pmt->i_pid_pcr == pid->i_pid || p_pmt->i_pid_pcr == 0x1FFF ) )
                {
                    stime_t i_pcr = ( p_block->i_dts > p_sys->i_generated_pcr_dpb_offset )
                                  ? TO_SCALE(p_block->i_dts - p_sys->i_generated_pcr_dpb_offset)
                                  : TO_SCALE(p_block->i_dts);
                    ProgramSetPCR( p_demux, p_pmt, i_pcr );
                }

                /* Compute PCR/DTS offset if any */
                if( p_pmt->pcr.i_pcroffset == -1 && p_block->i_dts != VLC_TICK_INVALID &&
                    SETANDVALID(p_pmt->pcr.i_current) &&
                   (p_es->fmt.i_cat == VIDEO_ES || p_es->fmt.i_cat == AUDIO_ES) )
                {
                    stime_t i_dts27 = TO_SCALE(p_block->i_dts);
                    i_dts27 = TimeStampWrapAround( p_pmt->pcr.i_first, i_dts27 );
                    stime_t i_pcr = TimeStampWrapAround( p_pmt->pcr.i_first, p_pmt->pcr.i_current );
                    if( i_dts27 < i_pcr )
                    {
                        p_pmt->pcr.i_pcroffset = i_pcr - i_dts27 + TO_SCALE_NZ(VLC_TICK_FROM_MS(80));
                        msg_Warn( p_demux, "Broken stream: pid %d sends packets with dts %"PRId64
                                           "us later than pcr, applying delay",
                                  pid->i_pid, FROM_SCALE_NZ(p_pmt->pcr.i_pcroffset) );
                    }
                    else p_pmt->pcr.i_pcroffset = 0;
                }

                if( p_pmt->pcr.i_pcroffset != -1 )
                {
                    if( p_block->i_dts != VLC_TICK_INVALID )
                        p_block->i_dts += FROM_SCALE_NZ(p_pmt->pcr.i_pcroffset);
                    if( p_block->i_pts != VLC_TICK_INVALID )
                        p_block->i_pts += FROM_SCALE_NZ(p_pmt->pcr.i_pcroffset);
                }

                /*** From here, block can become a chain again though conversion below ***/

                if( pid->u.p_stream->p_proc )
                {
                    if( p_block->i_flags & BLOCK_FLAG_DISCONTINUITY )
                        ts_stream_processor_Reset( pid->u.p_stream->p_proc );
                    p_block = ts_stream_processor_Push( pid->u.p_stream->p_proc, i_stream_id, p_block );
                }
                else
                /* Some codecs might need xform or AU splitting */
                {
                    p_block = ConvertPESBlock( p_demux, p_es, i_pes_size, i_stream_id, p_block );
                }

                SendDataChain( p_demux, p_es, p_block );
            }
            else
            {
                if( !p_pmt->pcr.b_fix_done ) /* Not seen yet */
                    PCRFixHandle( p_demux, p_pmt, p_block );

                block_ChainLastAppend( &pid->u.p_stream->prepcr.pp_last, p_block );

                /* PCR Seen and no es->id, cleanup current and prepcr blocks */
                if( p_pmt->pcr.i_current > -1)
                {
                    block_ChainRelease( pid->u.p_stream->prepcr.p_head );
                    pid->u.p_stream->prepcr.p_head = NULL;
                    pid->u.p_stream->prepcr.pp_last = &pid->u.p_stream->prepcr.p_head;
                }
            }
        }
    }
    else
    {
        msg_Warn( p_demux, "empty pes" );
    }
}

static void PESDataChainHandle( vlc_object_t *p_obj, void *priv, block_t *p_data )
{
    ParsePESDataChain( (demux_t *)p_obj, (ts_pid_t *) priv, p_data );
}

static block_t* ReadTSPacket( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    block_t     *p_pkt;

    /* Get a new TS packet */
    if( !( p_pkt = vlc_stream_Block( p_sys->stream, p_sys->i_packet_size ) ) )
    {
        int64_t size = stream_Size( p_sys->stream );
        if( size >= 0 && (uint64_t)size == vlc_stream_Tell( p_sys->stream ) )
            msg_Dbg( p_demux, "EOF at %"PRIu64, vlc_stream_Tell( p_sys->stream ) );
        else
            msg_Dbg( p_demux, "Can't read TS packet at %"PRIu64, vlc_stream_Tell(p_sys->stream) );
        return NULL;
    }

    if( p_pkt->i_buffer < TS_HEADER_SIZE + p_sys->i_packet_header_size )
    {
        block_Release( p_pkt );
        return NULL;
    }

    /* Skip header (BluRay streams).
     * re-sync logic would do this (by adjusting packet start), but this would result in losing first and last ts packets.
     * First packet is usually PAT, and losing it means losing whole first GOP. This is fatal with still-image based menus.
     */
    p_pkt->p_buffer += p_sys->i_packet_header_size;
    p_pkt->i_buffer -= p_sys->i_packet_header_size;

    /* Check sync byte and re-sync if needed */
    if( p_pkt->p_buffer[0] != 0x47 )
    {
        msg_Warn( p_demux, "lost synchro" );
        block_Release( p_pkt );
        for( ;; )
        {
            const uint8_t *p_peek;
            int i_peek = 0;
            unsigned i_skip = 0;

            i_peek = vlc_stream_Peek( p_sys->stream, &p_peek,
                    p_sys->i_packet_size * 10 );
            if( i_peek < 0 || (unsigned)i_peek < p_sys->i_packet_size + 1 )
            {
                msg_Dbg( p_demux, "eof ?" );
                return NULL;
            }

            while( i_skip < i_peek - p_sys->i_packet_size )
            {
                if( p_peek[i_skip + p_sys->i_packet_header_size] == 0x47 &&
                        p_peek[i_skip + p_sys->i_packet_header_size + p_sys->i_packet_size] == 0x47 )
                {
                    break;
                }
                i_skip++;
            }
            msg_Dbg( p_demux, "skipping %d bytes of garbage", i_skip );
            if (vlc_stream_Read( p_sys->stream, NULL, i_skip ) != i_skip)
                return NULL;

            if( i_skip < i_peek - p_sys->i_packet_size )
            {
                break;
            }
        }
        if( !( p_pkt = vlc_stream_Block( p_sys->stream, p_sys->i_packet_size ) ) )
        {
            msg_Dbg( p_demux, "eof ?" );
            return NULL;
        }
    }
    return p_pkt;
}

static stime_t GetPCR( const block_t *p_pkt )
{
    const uint8_t *p = p_pkt->p_buffer;

    stime_t i_pcr = -1;

    if( likely(p_pkt->i_buffer > 11) &&
        ( p[3]&0x20 ) && /* adaptation */
        ( p[5]&0x10 ) &&
        ( p[4] >= 7 ) )
    {
        /* PCR is 33 bits */
        i_pcr = ( (stime_t)p[6] << 25 ) |
                ( (stime_t)p[7] << 17 ) |
                ( (stime_t)p[8] << 9 ) |
                ( (stime_t)p[9] << 1 ) |
                ( (stime_t)p[10] >> 7 );
    }
    return i_pcr;
}

static inline void UpdateESScrambledState( es_out_t *out, const ts_es_t *p_es, bool b_scrambled )
{
    for( ; p_es; p_es = p_es->p_next )
    {
        if( p_es->id )
            es_out_Control( out, ES_OUT_SET_ES_SCRAMBLED_STATE,
                            p_es->id, b_scrambled );
        UpdateESScrambledState( out, p_es->p_extraes, b_scrambled );
    }
}

static void UpdatePIDScrambledState( demux_t *p_demux, ts_pid_t *p_pid, bool b_scrambled )
{
    if( !SCRAMBLED(*p_pid) == !b_scrambled )
        return;

    msg_Warn( p_demux, "scrambled state changed on pid %d (%d->%d)",
              p_pid->i_pid, !!SCRAMBLED(*p_pid), b_scrambled );

    if( b_scrambled )
        p_pid->i_flags |= FLAG_SCRAMBLED;
    else
        p_pid->i_flags &= ~FLAG_SCRAMBLED;

    if( p_pid->type == TYPE_STREAM )
        UpdateESScrambledState( p_demux->out, p_pid->u.p_stream->p_es, b_scrambled );
}

static inline void FlushESBuffer( ts_stream_t *p_pes )
{
    if( p_pes->gather.p_data )
    {
        p_pes->gather.i_gathered = p_pes->gather.i_data_size = 0;
        block_ChainRelease( p_pes->gather.p_data );
        p_pes->gather.p_data = NULL;
        p_pes->gather.pp_last = &p_pes->gather.p_data;
        p_pes->gather.i_saved = 0;
    }
    if( p_pes->p_proc )
        ts_stream_processor_Reset( p_pes->p_proc );
}

static void ReadyQueuesPostSeek( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    ts_pat_t *p_pat = GetPID(p_sys, 0)->u.p_pat;
    for( int i=0; i< p_pat->programs.i_size; i++ )
    {
        ts_pmt_t *p_pmt = p_pat->programs.p_elems[i]->u.p_pmt;
        for( int j=0; j<p_pmt->e_streams.i_size; j++ )
        {
            ts_pid_t *pid = p_pmt->e_streams.p_elems[j];
            ts_stream_t *p_pes = pid->u.p_stream;

            if( pid->type != TYPE_STREAM )
                continue;

            for( ts_es_t *p_es = p_pes->p_es; p_es; p_es = p_es->p_next )
                p_es->i_next_block_flags |= BLOCK_FLAG_DISCONTINUITY;

            pid->i_cc = 0xff;
            pid->i_dup = 0;
            pid->u.p_stream->i_last_dts = -1;

            if( pid->u.p_stream->prepcr.p_head )
            {
                block_ChainRelease( pid->u.p_stream->prepcr.p_head );
                pid->u.p_stream->prepcr.p_head = NULL;
                pid->u.p_stream->prepcr.pp_last = &pid->u.p_stream->prepcr.p_head;
            }

            ts_sections_processor_Reset( pid->u.p_stream->p_sections_proc );
            ts_stream_processor_Reset( pid->u.p_stream->p_proc );

            FlushESBuffer( pid->u.p_stream );
        }
        p_pmt->pcr.i_current = -1;
    }
}

static int SeekToTime( demux_t *p_demux, const ts_pmt_t *p_pmt, stime_t i_scaledtime )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    /* Deal with common but worst binary search case */
    if( p_pmt->pcr.i_first == i_scaledtime && p_sys->b_canseek )
        return vlc_stream_Seek( p_sys->stream, 0 );

    const int64_t i_stream_size = stream_Size( p_sys->stream );
    if( !p_sys->b_canfastseek || i_stream_size < p_sys->i_packet_size )
        return VLC_EGENERIC;

    const uint64_t i_initial_pos = vlc_stream_Tell( p_sys->stream );

    /* Find the time position by using binary search algorithm. */
    uint64_t i_head_pos = 0;
    uint64_t i_tail_pos = (uint64_t) i_stream_size - p_sys->i_packet_size;
    if( i_head_pos >= i_tail_pos )
        return VLC_EGENERIC;

    bool b_found = false;
    while( (i_head_pos + p_sys->i_packet_size) <= i_tail_pos && !b_found )
    {
        /* Round i_pos to a multiple of p_sys->i_packet_size */
        uint64_t i_splitpos = i_head_pos + (i_tail_pos - i_head_pos) / 2;
        uint64_t i_div = i_splitpos % p_sys->i_packet_size;
        i_splitpos -= i_div;

        if ( vlc_stream_Seek( p_sys->stream, i_splitpos ) != VLC_SUCCESS )
            break;

        uint64_t i_pos = i_splitpos;
        while( i_pos < i_tail_pos )
        {
            stime_t i_pcr = -1;
            block_t *p_pkt = ReadTSPacket( p_demux );
            if( !p_pkt )
            {
                i_head_pos = i_tail_pos;
                break;
            }
            else
                i_pos = vlc_stream_Tell( p_sys->stream );

            int i_pid = PIDGet( p_pkt );
            ts_pid_t *p_pid = GetPID(p_sys, i_pid);
            if( i_pid != 0x1FFF && p_pid->type == TYPE_STREAM &&
                ts_stream_Find_es( p_pid->u.p_stream, p_pmt ) &&
               (p_pkt->p_buffer[1] & 0xC0) == 0x40 && /* Payload start but not corrupt */
               (p_pkt->p_buffer[3] & 0xD0) == 0x10    /* Has payload but is not encrypted */
            )
            {
                unsigned i_skip = 4;
                if ( p_pkt->p_buffer[3] & 0x20 ) // adaptation field
                {
                    if( p_pkt->i_buffer >= 4 + 2 + 5 )
                    {
                        if( p_pmt->i_pid_pcr == i_pid )
                            i_pcr = GetPCR( p_pkt );
                        i_skip += 1 + __MIN(p_pkt->p_buffer[4], 182);
                    }
                }

                if( i_pcr == -1 )
                {
                    stime_t i_dts = -1;
                    stime_t i_pts = -1;
                    uint8_t i_stream_id;
                    if ( VLC_SUCCESS == ParsePESHeader( VLC_OBJECT(p_demux), &p_pkt->p_buffer[i_skip],
                                                        p_pkt->i_buffer - i_skip, &i_skip,
                                                        &i_dts, &i_pts, &i_stream_id, NULL ) )
                    {
                        if( i_dts > -1 )
                            i_pcr = i_dts;
                    }
                }
            }
            block_Release( p_pkt );

            if( i_pcr != -1 )
            {
                stime_t i_diff = i_scaledtime - TimeStampWrapAround( p_pmt->pcr.i_first, i_pcr );
                if ( i_diff < 0 )
                    i_tail_pos = (i_splitpos >= p_sys->i_packet_size) ? i_splitpos - p_sys->i_packet_size : 0;
                else if( i_diff < TO_SCALE(VLC_TICK_0 + VLC_TICK_FROM_MS(500)) )
                    b_found = true;
                else
                    i_head_pos = i_pos;
                break;
            }
        }

        if ( !b_found && i_pos + p_sys->i_packet_size > i_tail_pos )
            i_tail_pos = (i_splitpos >= p_sys->i_packet_size) ? i_splitpos - p_sys->i_packet_size : 0;
    }

    if( !b_found )
    {
        msg_Dbg( p_demux, "Seek():cannot find a time position." );
        if( vlc_stream_Seek( p_sys->stream, i_initial_pos ) != VLC_SUCCESS )
            msg_Err( p_demux, "Can't seek back to %" PRIu64, i_initial_pos );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int ProbeChunk( demux_t *p_demux, int i_program, bool b_end, stime_t *pi_pcr, bool *pb_found )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_count = 0;
    block_t *p_pkt = NULL;

    for( ;; )
    {
        *pi_pcr = -1;

        if( i_count++ > PROBE_CHUNK_COUNT || !( p_pkt = ReadTSPacket( p_demux ) ) )
        {
            break;
        }

        if( p_pkt->i_size < TS_PACKET_SIZE_188 &&
           ( p_pkt->p_buffer[1]&0x80 ) /* transport error */ )
        {
            block_Release( p_pkt );
            continue;
        }

        const int i_pid = PIDGet( p_pkt );
        ts_pid_t *p_pid = GetPID(p_sys, i_pid);

        p_pid->i_flags |= FLAG_SEEN;

        if( i_pid != 0x1FFF && (p_pkt->p_buffer[1] & 0x80) == 0 ) /* not corrupt */
        {
            bool b_pcrresult = true;
            bool b_adaptfield = p_pkt->p_buffer[3] & 0x20;

            if( b_adaptfield && p_pkt->i_buffer >= 4 + 2 + 5 )
                *pi_pcr = GetPCR( p_pkt );

            if( *pi_pcr == -1 &&
                (p_pkt->p_buffer[1] & 0xC0) == 0x40 && /* payload start */
                (p_pkt->p_buffer[3] & 0xD0) == 0x10 && /* Has payload but is not encrypted */
                p_pid->type == TYPE_STREAM &&
                p_pid->u.p_stream->p_es->fmt.i_cat != UNKNOWN_ES
              )
            {
                b_pcrresult = false;
                stime_t i_dts = -1;
                stime_t i_pts = -1;
                uint8_t i_stream_id;
                unsigned i_skip = 4;
                if ( b_adaptfield ) // adaptation field
                    i_skip += 1 + __MIN(p_pkt->p_buffer[4], 182);

                if ( VLC_SUCCESS == ParsePESHeader( VLC_OBJECT(p_demux), &p_pkt->p_buffer[i_skip],
                                                    p_pkt->i_buffer - i_skip, &i_skip,
                                                    &i_dts, &i_pts, &i_stream_id, NULL ) )
                {
                    if( i_dts != -1 )
                        *pi_pcr = i_dts;
                    else if( i_pts != -1 )
                        *pi_pcr = i_pts;
                }
            }

            if( *pi_pcr != -1 )
            {
                ts_pat_t *p_pat = GetPID(p_sys, 0)->u.p_pat;
                for( int i=0; i<p_pat->programs.i_size; i++ )
                {
                    ts_pmt_t *p_pmt = p_pat->programs.p_elems[i]->u.p_pmt;
                    if( p_pmt->i_pid_pcr == p_pid->i_pid ||
                          ( p_pmt->i_pid_pcr == 0x1FFF &&
                            PIDReferencedByProgram( p_pmt, p_pid->i_pid ) )
                      )
                    {
                        if( b_end )
                        {
                            p_pmt->i_last_dts = *pi_pcr;
                            p_pmt->i_last_dts_byte = vlc_stream_Tell( p_sys->stream );
                        }
                        /* Start, only keep first */
                        else if( b_pcrresult && p_pmt->pcr.i_first == -1 )
                        {
                            p_pmt->pcr.i_first = *pi_pcr;
                        }
                        else if( p_pmt->pcr.i_first_dts == -1 )
                        {
                            p_pmt->pcr.i_first_dts = *pi_pcr;
                        }

                        if( i_program == 0 || i_program == p_pmt->i_number )
                            *pb_found = true;
                    }
                }
            }
        }

        block_Release( p_pkt );
    }

    return i_count;
}

int ProbeStart( demux_t *p_demux, int i_program )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const uint64_t i_initial_pos = vlc_stream_Tell( p_sys->stream );
    int64_t i_stream_size = stream_Size( p_sys->stream );

    int i_probe_count = 0;
    int64_t i_pos;
    stime_t i_pcr = -1;
    bool b_found = false;

    do
    {
        i_pos = p_sys->i_packet_size * i_probe_count;
        i_pos = __MIN( i_pos, i_stream_size );

        if( vlc_stream_Seek( p_sys->stream, i_pos ) )
            return VLC_EGENERIC;

        ProbeChunk( p_demux, i_program, false, &i_pcr, &b_found );

        /* Go ahead one more chunk if end of file contained only stuffing packets */
        i_probe_count += PROBE_CHUNK_COUNT;
    } while( i_pos < i_stream_size && !b_found &&
             i_probe_count < PROBE_MAX );

    if( vlc_stream_Seek( p_sys->stream, i_initial_pos ) )
        return VLC_EGENERIC;

    return (b_found) ? VLC_SUCCESS : VLC_EGENERIC;
}

int ProbeEnd( demux_t *p_demux, int i_program )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const uint64_t i_initial_pos = vlc_stream_Tell( p_sys->stream );
    int64_t i_stream_size = stream_Size( p_sys->stream );

    int i_probe_count = PROBE_CHUNK_COUNT;
    int64_t i_pos;
    stime_t i_pcr = -1;
    bool b_found = false;

    do
    {
        i_pos = i_stream_size - (p_sys->i_packet_size * i_probe_count);
        i_pos = __MAX( i_pos, 0 );

        if( vlc_stream_Seek( p_sys->stream, i_pos ) )
            return VLC_EGENERIC;

        ProbeChunk( p_demux, i_program, true, &i_pcr, &b_found );

        /* Go ahead one more chunk if end of file contained only stuffing packets */
        i_probe_count += PROBE_CHUNK_COUNT;
    } while( i_pos > 0 && !b_found &&
             i_probe_count < PROBE_MAX );

    if( vlc_stream_Seek( p_sys->stream, i_initial_pos ) )
        return VLC_EGENERIC;

    return (b_found) ? VLC_SUCCESS : VLC_EGENERIC;
}

static void ProgramSetPCR( demux_t *p_demux, ts_pmt_t *p_pmt, stime_t i_pcr )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    /* Check if we have enqueued blocks waiting the/before the
       PCR barrier, and then adapt pcr so they have valid PCR when dequeuing */
    if( p_pmt->pcr.i_current == -1 && p_pmt->pcr.b_fix_done )
    {
        vlc_tick_t i_mindts = VLC_TICK_INVALID;

        ts_pat_t *p_pat = GetPID(p_sys, 0)->u.p_pat;
        for( int i=0; i< p_pat->programs.i_size; i++ )
        {
            ts_pmt_t *p_opmt = p_pat->programs.p_elems[i]->u.p_pmt;
            for( int j=0; j<p_opmt->e_streams.i_size; j++ )
            {
                ts_pid_t *p_pid = p_opmt->e_streams.p_elems[j];
                block_t *p_block = p_pid->u.p_stream->prepcr.p_head;
                while( p_block && p_block->i_dts == VLC_TICK_INVALID )
                    p_block = p_block->p_next;

                if( p_block && ( i_mindts == VLC_TICK_INVALID || p_block->i_dts < i_mindts ) )
                    i_mindts = p_block->i_dts;
            }
        }

        if( i_mindts != VLC_TICK_INVALID )
        {
            msg_Dbg( p_demux, "Program %d PCR prequeue fixup %"PRId64"->%"PRId64,
                     p_pmt->i_number, TO_SCALE(i_mindts), i_pcr );
            i_pcr = TO_SCALE(i_mindts);
        }
    }

    p_pmt->pcr.i_current = i_pcr;
    if( p_pmt->pcr.i_first == -1 )
    {
        p_pmt->pcr.i_first = i_pcr; // now seen
    }

    if ( p_sys->i_pmt_es )
    {
        es_out_Control( p_demux->out, ES_OUT_SET_GROUP_PCR, p_pmt->i_number, FROM_SCALE(i_pcr) );
        /* growing files/named fifo handling */
        if( p_sys->b_access_control == false &&
            vlc_stream_Tell( p_sys->stream ) > p_pmt->i_last_dts_byte )
        {
            if( p_pmt->i_last_dts_byte == 0 ) /* first run */
                p_pmt->i_last_dts_byte = stream_Size( p_sys->stream );
            else
            {
                p_pmt->i_last_dts = i_pcr;
                p_pmt->i_last_dts_byte = vlc_stream_Tell( p_sys->stream );
            }
        }
    }
}

static int IsVideoEnd( ts_pid_t *p_pid )
{
    /* jump to near end of PES packet */
    block_t *p = p_pid->u.p_stream->gather.p_data;
    if( !p || !p->p_next )
        return 0;
    while( p->p_next->p_next )
        p = p->p_next;
    if( p->p_next->i_buffer > 4)
        p = p->p_next;

    /* extract last bytes */
    uint8_t tail[ 188 ];
    const int i_tail = block_ChainExtract( p, tail, sizeof( tail ) );
    if( i_tail < 4 )
        return 0;

    /* check for start code at end */
    return ( tail[ i_tail - 4 ] == 0 && tail[ i_tail - 3 ] == 0 && tail[ i_tail - 2 ] == 1 &&
             ( tail[ i_tail - 1 ] == 0xb7 ||  tail[ i_tail - 1 ] == 0x0a ) );
}

static void PCRCheckDTS( demux_t *p_demux, ts_pmt_t *p_pmt, stime_t i_pcr)
{
    for( int i=0; i<p_pmt->e_streams.i_size; i++ )
    {
        ts_pid_t *p_pid = p_pmt->e_streams.p_elems[i];

        if( p_pid->type != TYPE_STREAM || SCRAMBLED(*p_pid) )
            continue;

        ts_stream_t *p_pes = p_pid->u.p_stream;
        ts_es_t *p_es = p_pes->p_es;

        if( p_pes->gather.p_data == NULL )
            continue;
        if( p_pes->gather.i_data_size != 0 )
            continue;

        /* check only MPEG2, H.264 and VC-1 */
        if( p_es->fmt.i_codec != VLC_CODEC_MPGV &&
            p_es->fmt.i_codec != VLC_CODEC_H264 &&
            p_es->fmt.i_codec != VLC_CODEC_VC1 )
            continue;

        uint8_t header[34];
        const int i_max = block_ChainExtract( p_pes->gather.p_data, header, 34 );

        if( i_max < 6 || header[0] != 0 || header[1] != 0 || header[2] != 1 )
            continue;

        unsigned i_skip = 0;
        stime_t i_dts = -1;
        stime_t i_pts = -1;
        uint8_t i_stream_id;

        if( ParsePESHeader( VLC_OBJECT(p_demux), (uint8_t*)&header, i_max, &i_skip,
                            &i_dts, &i_pts, &i_stream_id, NULL ) == VLC_EGENERIC )
            continue;

        if (p_pmt->pcr.i_pcroffset > 0) {
            if( i_dts != -1 )
                i_dts += p_pmt->pcr.i_pcroffset;
            if( i_pts != -1 )
                i_pts += p_pmt->pcr.i_pcroffset;
        }

        if( i_dts != -1 )
            i_dts = TimeStampWrapAround( i_pcr, i_dts );
        if( i_pts != -1 )
            i_pts = TimeStampWrapAround( i_pcr, i_pts );

        if(( i_dts != -1 && i_dts <= i_pcr ) ||
           ( i_pts != -1 && i_pts <= i_pcr ))
        {
            if( IsVideoEnd( p_pid ) )
            {
                msg_Warn( p_demux, "send queued data for pid %d: TS %"PRId64" <= PCR %"PRId64"\n",
                          p_pid->i_pid, i_dts != -1 ? i_dts : i_pts, i_pcr);
                ts_pes_parse_callback cb = { .p_obj = VLC_OBJECT(p_demux),
                                             .priv = p_pid,
                                             .pf_parse = PESDataChainHandle };
                ts_pes_Drain( &cb, p_pes );
            }
        }
    }
}

static void PCRHandle( demux_t *p_demux, ts_pid_t *pid, stime_t i_pcr )
{
    demux_sys_t   *p_sys = p_demux->p_sys;

    pid->probed.i_pcr_count++;

    if( p_sys->i_pmt_es <= 0 )
        return;

    if(unlikely(GetPID(p_sys, 0)->type != TYPE_PAT))
        return;

    /* Search program and set the PCR */
    ts_pat_t *p_pat = GetPID(p_sys, 0)->u.p_pat;
    for( int i = 0; i < p_pat->programs.i_size; i++ )
    {
        ts_pmt_t *p_pmt = p_pat->programs.p_elems[i]->u.p_pmt;
        if( p_pmt->pcr.b_disable )
            continue;
        stime_t i_program_pcr = TimeStampWrapAround( p_pmt->pcr.i_first, i_pcr );

        if( p_pmt->i_pid_pcr == 0x1FFF ) /* That program has no dedicated PCR pid ISO/IEC 13818-1 2.4.4.9 */
        {
            if( PIDReferencedByProgram( p_pmt, pid->i_pid ) ) /* PCR shall be on pid itself */
            {
                /* ? update PCR for the whole group program ? */
                ProgramSetPCR( p_demux, p_pmt, i_program_pcr );
            }
        }
        else /* set PCR provided by current pid to program(s) referencing it */
        {
            /* Can be dedicated PCR pid (no owned then) or another pid (owner == pmt) */
            if( p_pmt->i_pid_pcr == pid->i_pid ) /* If that program references current pid as PCR */
            {
                /* We've found a target group for update */
                PCRCheckDTS( p_demux, p_pmt, i_pcr );
                ProgramSetPCR( p_demux, p_pmt, i_program_pcr );
            }
        }

    }
}

int FindPCRCandidate( ts_pmt_t *p_pmt )
{
    ts_pid_t *p_cand = NULL;
    int i_previous = p_pmt->i_pid_pcr;

    for( int i=0; i<p_pmt->e_streams.i_size; i++ )
    {
        ts_pid_t *p_pid = p_pmt->e_streams.p_elems[i];
        if( SEEN(p_pid) && p_pid->i_pid != i_previous )
        {
            if( p_pid->probed.i_pcr_count ) /* check PCR frequency first */
            {
                if( !p_cand || p_pid->probed.i_pcr_count > p_cand->probed.i_pcr_count )
                {
                    p_cand = p_pid;
                    continue;
                }
            }

            if( p_pid->u.p_stream->p_es->fmt.i_cat == AUDIO_ES )
            {
                if( !p_cand )
                    p_cand = p_pid;
            }
            else if ( p_pid->u.p_stream->p_es->fmt.i_cat == VIDEO_ES ) /* Otherwise prioritize video dts */
            {
                if( !p_cand || p_cand->u.p_stream->p_es->fmt.i_cat == AUDIO_ES )
                    p_cand = p_pid;
            }
        }
    }

    if( p_cand )
        return p_cand->i_pid;
    else
        return 0x1FFF;
}

/* Tries to reselect a new PCR when none has been received */
static void PCRFixHandle( demux_t *p_demux, ts_pmt_t *p_pmt, block_t *p_block )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    /* disable PCR offset check */
    if( !p_sys->b_check_pcr_offset && p_pmt->pcr.i_pcroffset == -1 )
        p_pmt->pcr.i_pcroffset = 0;

    if ( p_pmt->pcr.b_disable || p_pmt->pcr.b_fix_done )
    {
        return;
    }
    /* Record the first data packet timestamp in case there wont be any PCR */
    else if( p_pmt->pcr.i_first_dts == TS_TICK_UNKNOWN )
    {
        p_pmt->pcr.i_first_dts = TO_SCALE(p_block->i_dts);
    }
    else if( p_block->i_dts - FROM_SCALE(p_pmt->pcr.i_first_dts) > VLC_TICK_FROM_MS(500) ) /* "PCR repeat rate shall not exceed 100ms" */
    {
        if( p_pmt->pcr.i_current < 0 &&
            GetPID( p_sys, p_pmt->i_pid_pcr )->probed.i_pcr_count == 0 )
        {
            int i_cand = FindPCRCandidate( p_pmt );
            p_pmt->i_pid_pcr = i_cand;
            if ( GetPID( p_sys, p_pmt->i_pid_pcr )->probed.i_pcr_count == 0 )
                p_pmt->pcr.b_disable = true;
            msg_Warn( p_demux, "No PCR received for program %d, set up workaround using pid %d",
                      p_pmt->i_number, i_cand );
            UpdatePESFilters( p_demux, p_sys->seltype == PROGRAM_ALL );
        }
        p_pmt->pcr.b_fix_done = true;
    }
}

static block_t * ProcessTSPacket( demux_t *p_demux, ts_pid_t *pid, block_t *p_pkt, int *pi_skip )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const uint8_t *p = p_pkt->p_buffer;
    const bool b_adaptation = p[3]&0x20;
    const bool b_payload    = p[3]&0x10;
    const bool b_scrambled  = p[3]&0xc0;
    const int  i_cc         = p[3]&0x0f; /* continuity counter */
    bool       b_discontinuity = false;  /* discontinuity */

    /* transport_scrambling_control is ignored */
    *pi_skip = 4;

#if 0
    msg_Dbg( p_demux, "pid=%d unit_start=%d adaptation=%d payload=%d "
             "cc=0x%x", pid->i_pid, b_unit_start, b_adaptation,
             b_payload, i_cc );
#endif

    /* Drop null packets */
    if( unlikely(pid->i_pid == 0x1FFF) )
    {
        block_Release( p_pkt );
        return NULL;
    }

    /* For now, ignore additional error correction
     * TODO: handle Reed-Solomon 204,188 error correction */
    p_pkt->i_buffer = TS_PACKET_SIZE_188;

    if( b_scrambled )
    {
        if( p_sys->csa )
        {
            vlc_mutex_lock( &p_sys->csa_lock );
            csa_Decrypt( p_sys->csa, p_pkt->p_buffer, p_sys->i_csa_pkt_size );
            vlc_mutex_unlock( &p_sys->csa_lock );
        }
        else
            p_pkt->i_flags |= BLOCK_FLAG_SCRAMBLED;
    }

    /* We don't have any adaptation_field, so payload starts
     * immediately after the 4 byte TS header */
    if( b_adaptation )
    {
        /* p[4] is adaptation_field_length minus one */
        *pi_skip += 1 + p[4];
        if( p[4] + 5 > 188 /* adaptation field only == 188 */ )
        {
            /* Broken is broken */
            block_Release( p_pkt );
            return NULL;
        }
        else if( p[4] > 0 )
        {
            /* discontinuity indicator found in stream */
            b_discontinuity = (p[5]&0x80) ? true : false;
            if( b_discontinuity )
            {
                msg_Warn( p_demux, "discontinuity indicator (pid=%d) ",
                            pid->i_pid );
                /* ignore, that's not that simple 2.4.3.5 */
                //p_pkt->i_flags |= BLOCK_FLAG_DISCONTINUITY;

                /* ... or don't ignore for our Bluray still frames and seek hacks */
                if(p[5] == 0x82 && !strncmp((const char *)&p[7], "VLC_DISCONTINU", 14))
                    p_pkt->i_flags |= BLOCK_FLAG_SOURCE_RANDOM_ACCESS;
            }
#if 0
            if( p[5]&0x40 )
                msg_Dbg( p_demux, "random access indicator (pid=%d) ", pid->i_pid );
#endif
        }
    }

    /* Test continuity counter */
    /* continuous when (one of this):
        * diff == 1
        * diff == 0 and payload == 0
        * diff == 0 and duplicate packet (playload != 0) <- should we
        *   test the content ?
     */
    if( b_payload && p_sys->b_cc_check )
    {
        const int i_diff = ( i_cc - pid->i_cc )&0x0f;
        if( i_diff == 1 )
        {
            pid->i_cc = ( pid->i_cc + 1 ) & 0xf;
            pid->i_dup = 0;
        }
        else
        {
            if( pid->i_cc == 0xff )
            {
                msg_Dbg( p_demux, "first packet for pid=%d cc=0x%x",
                         pid->i_pid, i_cc );
                pid->i_cc = i_cc;
            }
            else if( i_diff == 0 && pid->i_dup == 0 &&
                     !memcmp(pid->prevpktbytes, /* see comment below */
                             &p_pkt->p_buffer[1], PREVPKTKEEPBYTES)  )
            {
                /* Discard duplicated payload 2.4.3.3 */
                /* Added previous pkt bytes comparison for
                 * stupid HLS dumps/joined segments which are
                 * triggering erroneous duplicates instead of discontinuity.
                 * That should not need CRC or full payload as it should be
                 * restarting with PSI packets */
                pid->i_dup++;
                block_Release( p_pkt );
                return NULL;
            }
            else if( i_diff != 0 && !b_discontinuity )
            {
                msg_Warn( p_demux, "discontinuity received 0x%x instead of 0x%x (pid=%d)",
                          i_cc, ( pid->i_cc + 1 )&0x0f, pid->i_pid );

                pid->i_cc = i_cc;
                pid->i_dup = 0;
                p_pkt->i_flags |= BLOCK_FLAG_DISCONTINUITY;
            }
            else pid->i_cc = i_cc;
        }
        memcpy(pid->prevpktbytes, &p_pkt->p_buffer[1], PREVPKTKEEPBYTES);
    }
    else /* Ignore all 00 or 10 as in 2.4.3.3 CC counter must not be
            incremented in those cases, but there is humax inserting
            empty/10 packets always set with cc = 0 between 2 payload pkts
            see stream_main_pcr_1280x720p50_5mbps.ts */
    {
        if( b_discontinuity )
            pid->i_cc = i_cc;
    }

    if( unlikely(!(b_payload || b_adaptation)) ) /* Invalid, ignore */
    {
        block_Release( p_pkt );
        return NULL;
    }

    return p_pkt;
}

static bool GatherPESData( demux_t *p_demux, ts_pid_t *p_pid, block_t *p_pkt, size_t i_skip )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    ts_pes_parse_callback cb = { .p_obj = VLC_OBJECT(p_demux),
                                 .priv = p_pid,
                                 .pf_parse = PESDataChainHandle };
    const bool b_unit_start = p_pkt->p_buffer[1]&0x40;
    p_pkt->p_buffer += i_skip; /* point to PES */
    p_pkt->i_buffer -= i_skip;
    return ts_pes_Gather( &cb, p_pid->u.p_stream,
                          p_pkt, b_unit_start,
                          p_sys->b_valid_scrambling );
}

static bool GatherSectionsData( demux_t *p_demux, ts_pid_t *p_pid, block_t *p_pkt, size_t i_skip )
{
    VLC_UNUSED(i_skip); VLC_UNUSED(p_demux);
    bool b_ret = false;

    if( p_pkt->i_flags & BLOCK_FLAG_DISCONTINUITY )
    {
        ts_sections_processor_Reset( p_pid->u.p_stream->p_sections_proc );
    }

    if( (p_pkt->i_flags & (BLOCK_FLAG_SCRAMBLED | BLOCK_FLAG_CORRUPTED)) == 0 )
    {
        ts_sections_processor_Push( p_pid->u.p_stream->p_sections_proc, p_pkt->p_buffer );
        b_ret = true;
    }

    block_Release( p_pkt );

    return b_ret;
}

void TsChangeStandard( demux_sys_t *p_sys, ts_standards_e v )
{
    if( p_sys->standard != TS_STANDARD_AUTO &&
        p_sys->standard != v )
        return; /* TODO */
    p_sys->standard = v;
}

bool ProgramIsSelected( demux_sys_t *p_sys, uint16_t i_pgrm )
{
    if( p_sys->seltype == PROGRAM_ALL )
        return true;

    for(int i=0; i<p_sys->programs.i_size; i++)
        if( p_sys->programs.p_elems[i] == i_pgrm )
            return true;

    return false;
}

static bool PIDReferencedByProgram( const ts_pmt_t *p_pmt, uint16_t i_pid )
{
    for(int i=0; i<p_pmt->e_streams.i_size; i++)
        if( p_pmt->e_streams.p_elems[i]->i_pid == i_pid )
            return true;

    return false;
}

static void DoCreateES( demux_t *p_demux, ts_es_t *p_es, const ts_es_t *p_parent_es )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    for( ; p_es ; p_es = p_es->p_next )
    {
        if( !p_es->id )
        {
            if( !p_es->fmt.i_group )
                p_es->fmt.i_group = p_es->p_program->i_number;
            p_es->id = es_out_Add( p_demux->out, &p_es->fmt );
            if( p_parent_es ) /* Set Extra ES group and original ID */
            {
                if ( p_sys->b_es_id_pid ) /* pid is 13 bits */
                    p_es->fmt.i_id = (p_sys->i_next_extraid++ << 13) | p_parent_es->fmt.i_id;
                p_es->fmt.i_group = p_parent_es->fmt.i_group;
            }
            p_sys->i_pmt_es++;
        }
        DoCreateES( p_demux, p_es->p_extraes, p_es );
    }
}

void AddAndCreateES( demux_t *p_demux, ts_pid_t *pid, bool b_create_delayed )
{
    demux_sys_t  *p_sys = p_demux->p_sys;

    if( b_create_delayed )
        p_sys->es_creation = CREATE_ES;

    if( pid && p_sys->es_creation == CREATE_ES )
    {
        DoCreateES( p_demux, pid->u.p_stream->p_es, NULL );

        /* Update the default program == first created ES group */
        if( p_sys->b_default_selection && p_sys->programs.i_size > 0)
        {
            p_sys->b_default_selection = false;
            const int i_first_program = pid->u.p_stream->p_es->p_program->i_number;
            if( p_sys->programs.p_elems[0] != i_first_program )
                p_sys->programs.p_elems[0] = i_first_program;
            msg_Dbg( p_demux, "Default program is %d", i_first_program );
        }
    }

    if( b_create_delayed )
    {
        ts_pat_t *p_pat = GetPID(p_sys, 0)->u.p_pat;
        for( int i=0; i< p_pat->programs.i_size; i++ )
        {
            ts_pmt_t *p_pmt = p_pat->programs.p_elems[i]->u.p_pmt;
            for( int j=0; j<p_pmt->e_streams.i_size; j++ )
                DoCreateES( p_demux, p_pmt->e_streams.p_elems[j]->u.p_stream->p_es, NULL );
        }
    }
}
