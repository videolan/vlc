/*****************************************************************************
 * ts.c: Transport Stream input module for VLC.
 *****************************************************************************
 * Copyright (C) 2004-2005 VLC authors and VideoLAN
 * $Id$
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

#include <assert.h>
#include <time.h>

#include <vlc_access.h>    /* DVB-specific things */
#include <vlc_demux.h>
#include <vlc_meta.h>
#include <vlc_epg.h>
#include <vlc_charset.h>   /* FromCharset, for EIT */
#include <vlc_bits.h>

#include "../../mux/mpeg/csa.h"

/* Include dvbpsi headers */
# include <dvbpsi/dvbpsi.h>
# include <dvbpsi/demux.h>
# include <dvbpsi/descriptor.h>
# include <dvbpsi/pat.h>
# include <dvbpsi/pmt.h>
# include <dvbpsi/sdt.h>
# include <dvbpsi/dr.h>
# include <dvbpsi/psi.h>

/* EIT support */
# include <dvbpsi/eit.h>

/* TDT support */
# include <dvbpsi/tot.h>

#include "../../mux/mpeg/dvbpsi_compat.h"
#include "../../mux/mpeg/streams.h"
#include "../../mux/mpeg/tsutil.h"
#include "../../mux/mpeg/tables.h"

#include "../../codec/opus_header.h"

#include "../opus.h"

#include "pes.h"
#include "mpeg4_iod.h"

#ifdef HAVE_ARIBB24
 #include <aribb24/aribb24.h>
 #include <aribb24/decoder.h>
#endif

typedef enum arib_modes_e
{
    ARIBMODE_AUTO = -1,
    ARIBMODE_DISABLED = 0,
    ARIBMODE_ENABLED = 1
} arib_modes_e;

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
    "decrypting. " )

#define SPLIT_ES_TEXT N_("Separate sub-streams")
#define SPLIT_ES_LONGTEXT N_( \
    "Separate teletex/dvbs pages into independent ES. " \
    "It can be useful to turn off this option when using stream output." )

#define SEEK_PERCENT_TEXT N_("Seek based on percent not time")
#define SEEK_PERCENT_LONGTEXT N_( \
    "Seek and position based on a percent byte position, not a PCR generated " \
    "time position. If seeking doesn't work property, turn on this option." )

#define PCR_TEXT N_("Trust in-stream PCR")
#define PCR_LONGTEXT N_("Use the stream PCR as a reference.")

static const int const arib_mode_list[] =
  { ARIBMODE_AUTO, ARIBMODE_ENABLED, ARIBMODE_DISABLED };
static const char *const arib_mode_list_text[] =
  { N_("Auto"), N_("Enabled"), N_("Disabled") };

#define SUPPORT_ARIB_TEXT N_("ARIB STD-B24 mode")
#define SUPPORT_ARIB_LONGTEXT N_( \
    "Forces ARIB STD-B24 mode for decoding characters." \
    "This feature affects EPG information and subtitles." )

vlc_module_begin ()
    set_description( N_("MPEG Transport Stream demuxer") )
    set_shortname ( "MPEG-TS" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )

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

    add_integer( "ts-arib", ARIBMODE_AUTO, SUPPORT_ARIB_TEXT, SUPPORT_ARIB_LONGTEXT, false )
        change_integer_list( arib_mode_list, arib_mode_list_text )

    add_obsolete_bool( "ts-silent" );

    set_capability( "demux", 10 )
    set_callbacks( Open, Close )
    add_shortcut( "ts" )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static const char *const ppsz_teletext_type[] = {
 "",
 N_("Teletext"),
 N_("Teletext subtitles"),
 N_("Teletext: additional information"),
 N_("Teletext: program schedule"),
 N_("Teletext subtitles: hearing impaired")
};

typedef struct ts_pid_t ts_pid_t;

typedef struct
{
    int             i_version;
    int             i_ts_id;
    dvbpsi_t       *handle;
    DECL_ARRAY(ts_pid_t *) programs;

} ts_pat_t;

typedef struct
{
    dvbpsi_t       *handle;
    int             i_version;
    int             i_number;
    int             i_pid_pcr;
    /* IOD stuff (mpeg4) */
    od_descriptor_t *iod;
    od_descriptors_t od;

    DECL_ARRAY(ts_pid_t *) e_streams;

    struct
    {
        mtime_t i_current;
        mtime_t i_first; // seen <> != -1
        /* broken PCR handling */
        mtime_t i_first_dts;
        mtime_t i_pcroffset;
        bool    b_disable; /* ignore PCR field, use dts */
        bool    b_fix_done;
    } pcr;

    mtime_t i_last_dts;

} ts_pmt_t;

typedef struct
{
    es_format_t  fmt;
    es_out_id_t *id;
    uint16_t i_sl_es_id;
} ts_pes_es_t;

typedef enum
{
    TS_ES_DATA_PES,
    TS_ES_DATA_TABLE_SECTION
} ts_es_data_type_t;

typedef struct
{
    ts_pes_es_t es;
    /* Some private streams encapsulate several ES (eg. DVB subtitles)*/
    DECL_ARRAY( ts_pes_es_t * ) extra_es;

    uint8_t i_stream_type;

    ts_es_data_type_t data_type;
    int         i_data_size;
    int         i_data_gathered;
    block_t     *p_data;
    block_t     **pp_last;

    block_t *   p_prepcr_outqueue;

    /* SL AU */
    struct
    {
        block_t     *p_data;
        block_t     **pp_last;
    } sl;
} ts_pes_t;


typedef struct
{
    /* for special PAT/SDT case */
    dvbpsi_t       *handle; /* PAT/SDT/EIT */
    int             i_version;

} ts_psi_t;

typedef enum
{
    TS_PMT_REGISTRATION_NONE = 0,
    TS_PMT_REGISTRATION_HDMV
} ts_pmt_registration_type_t;

typedef enum
{
    TYPE_FREE = 0,
    TYPE_PAT,
    TYPE_PMT,
    TYPE_PES,
    TYPE_SDT,
    TYPE_TDT,
    TYPE_EIT,
} ts_pid_type_t;

enum
{
    FLAGS_NONE = 0,
    FLAG_SEEN  = 1,
    FLAG_SCRAMBLED = 2,
    FLAG_FILTERED = 4
};

#define SEEN(x) ((x)->i_flags & FLAG_SEEN)
#define SCRAMBLED(x) ((x).i_flags & FLAG_SCRAMBLED)

struct ts_pid_t
{
    uint16_t    i_pid;

    uint8_t     i_flags;
    uint8_t     i_cc;   /* countinuity counter */
    uint8_t     type;

    /* PSI owner (ie PMT -> PAT, ES -> PMT */
    uint8_t     i_refcount;
    ts_pid_t   *p_parent;

    /* */
    union
    {
        ts_pat_t    *p_pat;
        ts_pmt_t    *p_pmt;
        ts_pes_t    *p_pes;
        ts_psi_t    *p_psi;
    } u;

    struct
    {
        vlc_fourcc_t i_fourcc;
        int i_type;
        int i_pcr_count;
    } probed;

};

typedef struct
{
    int i_service;
} vdr_info_t;

#define MIN_ES_PID 4    /* Should be 32.. broken muxers */
#define MAX_ES_PID 8190
#define MIN_PAT_INTERVAL CLOCK_FREQ // DVB is 500ms

#define PID_ALLOC_CHUNK 16

struct demux_sys_t
{
    stream_t   *stream;
    bool        b_canseek;
    bool        b_canfastseek;
    vlc_mutex_t     csa_lock;

    /* TS packet size (188, 192, 204) */
    unsigned    i_packet_size;

    /* Additional TS packet header size (BluRay TS packets have 4-byte header before sync byte) */
    unsigned    i_packet_header_size;

    /* how many TS packet we read at once */
    unsigned    i_ts_read;

    bool        b_force_seek_per_percent;

    struct
    {
        arib_modes_e e_mode;
#ifdef HAVE_ARIBB24
        arib_instance_t *p_instance;
#endif
        stream_t     *b25stream;
    } arib;

    /* All pid */
    struct
    {
        ts_pid_t pat;
        ts_pid_t dummy;
        /* all non commons ones, dynamically allocated */
        ts_pid_t **pp_all;
        int        i_all;
        int        i_all_alloc;
        /* last recently used */
        uint16_t   i_last_pid;
        ts_pid_t  *p_last;
    } pids;

    bool        b_user_pmt;
    int         i_pmt_es;
    bool        b_es_all; /* If we need to return all es/programs */

    enum
    {
        NO_ES, /* for preparse */
        DELAY_ES,
        CREATE_ES
    } es_creation;
    #define PREPARSING p_sys->es_creation == NO_ES

    /* */
    bool        b_es_id_pid;
    uint16_t    i_next_extraid;

    csa_t       *csa;
    int         i_csa_pkt_size;
    bool        b_split_es;

    bool        b_trust_pcr;

    /* */
    bool        b_access_control;
    bool        b_end_preparse;

    /* */
    bool        b_dvb_meta;
    int64_t     i_tdt_delta;
    int64_t     i_dvb_start;
    int64_t     i_dvb_length;
    bool        b_broken_charset; /* True if broken encoding is used in EPG/SDT */

    /* Selected programs */
    DECL_ARRAY( int ) programs; /* List of selected/access-filtered programs */
    bool        b_default_selection; /* True if set by default to first pmt seen (to get data from filtered access) */

    struct
    {
        mtime_t i_first_dts;     /* first dts encountered for the stream */
        int     i_timesourcepid; /* which pid we saved the dts from */
        enum { PAT_WAITING = 0, PAT_MISSING, PAT_FIXTRIED } status; /* set if we haven't seen PAT within MIN_PAT_INTERVAL */
    } patfix;

    vdr_info_t  vdr;

    /* */
    bool        b_start_record;
};

static int Demux    ( demux_t *p_demux );
static int Control( demux_t *p_demux, int i_query, va_list args );

static void PIDFillFormat( es_format_t *fmt, int i_stream_type, ts_es_data_type_t * );

static bool PIDSetup( demux_t *p_demux, ts_pid_type_t i_type, ts_pid_t *pid, ts_pid_t *p_parent );
static void PIDRelease( demux_t *p_demux, ts_pid_t *pid );

static void PATCallBack( void*, dvbpsi_pat_t * );
static void PMTCallBack( void *data, dvbpsi_pmt_t *p_pmt );
static void PSINewTableCallBack( dvbpsi_t *handle, uint8_t  i_table_id,
                                 uint16_t i_extension, demux_t * );

static int ChangeKeyCallback( vlc_object_t *, char const *, vlc_value_t, vlc_value_t, void * );

/* Structs */
static ts_pat_t *ts_pat_New( demux_t * );
static void ts_pat_Del( demux_t *, ts_pat_t * );
static ts_pmt_t *ts_pmt_New( demux_t * );
static void ts_pmt_Del( demux_t *, ts_pmt_t * );
static ts_pes_t *ts_pes_New( demux_t * );
static void ts_pes_Del( demux_t *, ts_pes_t * );
static ts_psi_t *ts_psi_New( demux_t * );
static void ts_psi_Del( demux_t *, ts_psi_t * );

/* Helpers */
static ts_pid_t *GetPID( demux_sys_t *, uint16_t i_pid );
static ts_pmt_t * GetProgramByID( demux_sys_t *, int i_program );
static bool ProgramIsSelected( demux_sys_t *, uint16_t i_pgrm );
static void UpdatePESFilters( demux_t *p_demux, bool b_all );
static inline void FlushESBuffer( ts_pes_t *p_pes );
static void UpdateScrambledState( demux_t *p_demux, ts_pid_t *p_pid, bool );
static inline int PIDGet( block_t *p )
{
    return ( (p->p_buffer[1]&0x1f)<<8 )|p->p_buffer[2];
}

static bool GatherData( demux_t *p_demux, ts_pid_t *pid, block_t *p_bk );
static void AddAndCreateES( demux_t *p_demux, ts_pid_t *pid, bool );
static void ProgramSetPCR( demux_t *p_demux, ts_pmt_t *p_prg, mtime_t i_pcr );

static block_t* ReadTSPacket( demux_t *p_demux );
static int ProbeStart( demux_t *p_demux, int i_program );
static int ProbeEnd( demux_t *p_demux, int i_program );
static int SeekToTime( demux_t *p_demux, ts_pmt_t *, int64_t time );
static void ReadyQueuesPostSeek( demux_t *p_demux );
static void PCRHandle( demux_t *p_demux, ts_pid_t *, block_t * );
static void PCRFixHandle( demux_t *, ts_pmt_t *, block_t * );
static int64_t TimeStampWrapAround( ts_pmt_t *, int64_t );

/* MPEG4 related */
static const es_mpeg4_descriptor_t * GetMPEG4DescByEsId( const ts_pmt_t *, uint16_t );
static ts_pes_es_t * GetPMTESBySLEsId( ts_pmt_t *, uint16_t );
static bool SetupISO14496LogicalStream( demux_t *, const decoder_config_descriptor_t *,
                                        es_format_t * );

#define TS_USER_PMT_NUMBER (0)
static int UserPmt( demux_t *p_demux, const char * );

static int  SetPIDFilter( demux_sys_t *, ts_pid_t *, bool b_selected );

#define TS_PACKET_SIZE_188 188
#define TS_PACKET_SIZE_192 192
#define TS_PACKET_SIZE_204 204
#define TS_PACKET_SIZE_MAX 204
#define TS_HEADER_SIZE 4

static int DetectPacketSize( demux_t *p_demux, unsigned *pi_header_size, int i_offset )
{
    const uint8_t *p_peek;

    if( stream_Peek( p_demux->s,
                     &p_peek, i_offset + TS_PACKET_SIZE_MAX ) < i_offset + TS_PACKET_SIZE_MAX )
        return -1;

    for( int i_sync = 0; i_sync < TS_PACKET_SIZE_MAX; i_sync++ )
    {
        if( p_peek[i_offset + i_sync] != 0x47 )
            continue;

        /* Check next 3 sync bytes */
        int i_peek = i_offset + TS_PACKET_SIZE_MAX * 3 + i_sync + 1;
        if( ( stream_Peek( p_demux->s, &p_peek, i_peek ) ) < i_peek )
        {
            msg_Err( p_demux, "cannot peek" );
            return -1;
        }
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

    if( p_demux->b_force )
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

    if( stream_Peek( p_demux->s,
                     &p_peek, TS_PACKET_SIZE_MAX ) < TS_PACKET_SIZE_MAX )
        return -1;

    if( memcmp( p_peek, "TFrc", 4 ) == 0 &&
        p_peek[6] == 0 && memcmp( &p_peek[53], "\x80\x00\x00", 4 ) == 0 &&
        stream_Peek( p_demux->s, &p_peek, TOPFIELD_HEADER_SIZE + TS_PACKET_SIZE_MAX )
            == TOPFIELD_HEADER_SIZE + TS_PACKET_SIZE_MAX )
    {
        i_packet_size = DetectPacketSize( p_demux, pi_header_size, TOPFIELD_HEADER_SIZE );
        if( i_packet_size != -1 )
        {
            msg_Dbg( p_demux, "this is a topfield file" );
#if 0
            /* I used the TF5000PVR 2004 Firmware .doc header documentation,
             * http://www.i-topfield.com/data/product/firmware/Structure%20of%20Recorded%20File%20in%20TF5000PVR%20(Feb%2021%202004).doc
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
            p_vdr->i_service = GetWBE(&p_peek[18]);

            return i_packet_size;
            //return TS_PACKET_SIZE_188;
        }
    }

    return DetectPacketSize( p_demux, pi_header_size, 0 );
}

static void ProbePES( demux_t *p_demux, ts_pid_t *pid, const uint8_t *p_pesstart, size_t i_data, bool b_adaptfield )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const uint8_t *p_pes = p_pesstart;
    pid->probed.i_type = -1;

    if( b_adaptfield )
    {
        if ( i_data < 2 )
            return;

        uint8_t len = *p_pes;
        p_pes++; i_data--;

        if(len == 0)
        {
            p_pes++; i_data--;/* stuffing */
        }
        else
        {
            if( i_data < len )
                return;
            if( len >= 7 && (p_pes[1] & 0x10) )
                pid->probed.i_pcr_count++;
            p_pes += len;
            i_data -= len;
        }
    }

    if( i_data < 9 )
        return;

    if( p_pes[0] != 0 || p_pes[1] != 0 || p_pes[2] != 1 )
        return;

    size_t i_pesextoffset = 8;
    mtime_t i_dts = -1;
    if( p_pes[7] & 0x80 ) // PTS
    {
        i_pesextoffset += 5;
        if ( i_data < i_pesextoffset )
            return;
        i_dts = ExtractPESTimestamp( &p_pes[9] );
    }
    if( p_pes[7] & 0x40 ) // DTS
    {
        i_pesextoffset += 5;
        if ( i_data < i_pesextoffset )
            return;
        i_dts = ExtractPESTimestamp( &p_pes[14] );
    }
    if( p_pes[7] & 0x20 ) // ESCR
        i_pesextoffset += 6;
    if( p_pes[7] & 0x10 ) // ESrate
        i_pesextoffset += 3;
    if( p_pes[7] & 0x08 ) // DSM
        i_pesextoffset += 1;
    if( p_pes[7] & 0x04 ) // CopyInfo
        i_pesextoffset += 1;
    if( p_pes[7] & 0x02 ) // PESCRC
        i_pesextoffset += 2;

    if ( i_data < i_pesextoffset )
        return;

     /* HeaderdataLength */
    const size_t i_payloadoffset = 8 + 1 + p_pes[8];
    i_pesextoffset += 1;

    if ( i_data < i_pesextoffset || i_data < i_payloadoffset )
        return;

    i_data -= 8 + 1 + p_pes[8];

    if( p_pes[7] & 0x01 ) // PESExt
    {
        size_t i_extension2_offset = 1;
        if ( p_pes[i_pesextoffset] & 0x80 ) // private data
            i_extension2_offset += 16;
        if ( p_pes[i_pesextoffset] & 0x40 ) // pack
            i_extension2_offset += 1;
        if ( p_pes[i_pesextoffset] & 0x20 ) // seq
            i_extension2_offset += 2;
        if ( p_pes[i_pesextoffset] & 0x10 ) // P-STD
            i_extension2_offset += 2;
        if ( p_pes[i_pesextoffset] & 0x01 ) // Extension 2
        {
            uint8_t i_len = p_pes[i_pesextoffset + i_extension2_offset] & 0x7F;
            i_extension2_offset += i_len;
        }
        if( i_data < i_extension2_offset )
            return;

        i_data -= i_extension2_offset;
    }
    /* (i_payloadoffset - i_pesextoffset) 0xFF stuffing */

    if ( i_data < 4 )
        return;

    const uint8_t *p_data = &p_pes[i_payloadoffset];
    /* NON MPEG audio & subpictures STREAM */
    if(p_pes[3] == 0xBD)
    {
        if( !memcmp( p_data, "\x7F\xFE\x80\x01", 4 ) )
        {
            pid->probed.i_type = 0x06;
            pid->probed.i_fourcc = VLC_CODEC_DTS;
        }
        else if( !memcmp( p_data, "\x0B\x77", 2 ) )
        {
            pid->probed.i_type = 0x06;
            pid->probed.i_fourcc = VLC_CODEC_EAC3;
        }
    }
    /* MPEG AUDIO STREAM */
    else if(p_pes[3] >= 0xC0 && p_pes[3] <= 0xDF)
    {
        if( p_data[0] == 0xFF && (p_data[1] & 0xE0) == 0xE0 )
        {
            switch(p_data[1] & 18)
            {
            /* 10 - MPEG Version 2 (ISO/IEC 13818-3)
               11 - MPEG Version 1 (ISO/IEC 11172-3) */
                case 0x10:
                    pid->probed.i_type = 0x04;
                    break;
                case 0x18:
                    pid->probed.i_type = 0x03;
                default:
                    break;
            }

            switch(p_data[1] & 6)
            {
            /* 01 - Layer III
               10 - Layer II
               11 - Layer I */
                case 0x06:
                    pid->probed.i_type = 0x04;
                    pid->probed.i_fourcc = VLC_CODEC_MPGA;
                    break;
                case 0x04:
                    pid->probed.i_type = 0x04;
                    pid->probed.i_fourcc = VLC_CODEC_MP2;
                    break;
                case 0x02:
                    pid->probed.i_type = 0x04;
                    pid->probed.i_fourcc = VLC_CODEC_MP3;
                default:
                    break;
            }
        }
    }
    /* VIDEO STREAM */
    else if( p_pes[3] >= 0xE0 && p_pes[3] <= 0xEF )
    {
        if( !memcmp( p_data, "\x00\x00\x00\x01", 4 ) )
        {
            pid->probed.i_type = 0x1b;
            pid->probed.i_fourcc = VLC_CODEC_H264;
        }
        else if( !memcmp( p_data, "\x00\x00\x01", 4 ) )
        {
            pid->probed.i_type = 0x02;
            pid->probed.i_fourcc = VLC_CODEC_MPGV;
        }
    }

    /* Track timestamps and flag missing PAT */
    if( !p_sys->patfix.i_timesourcepid && i_dts > -1 )
    {
        p_sys->patfix.i_first_dts = i_dts;
        p_sys->patfix.i_timesourcepid = pid->i_pid;
    }
    else if( p_sys->patfix.i_timesourcepid == pid->i_pid && i_dts > -1 &&
             p_sys->patfix.status == PAT_WAITING )
    {
        if( i_dts - p_sys->patfix.i_first_dts > TO_SCALE(MIN_PAT_INTERVAL) )
            p_sys->patfix.status = PAT_MISSING;
    }

}

static void BuildPATCallback( void *p_opaque, block_t *p_block )
{
    ts_pid_t *pat_pid = (ts_pid_t *) p_opaque;
    dvbpsi_packet_push( pat_pid->u.p_pat->handle, p_block->p_buffer );
}

static void BuildPMTCallback( void *p_opaque, block_t *p_block )
{
    ts_pid_t *program_pid = (ts_pid_t *) p_opaque;
    assert(program_pid->type == TYPE_PMT);
    while( p_block )
    {
        dvbpsi_packet_push( program_pid->u.p_pmt->handle,
                            p_block->p_buffer );
        p_block = p_block->p_next;
    }
}

static void MissingPATPMTFixup( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_program_number = 1234;
    int i_program_pid = 1337;
    int i_pcr_pid = 0x1FFF;
    int i_num_pes = 0;

    ts_pid_t *p_program_pid = GetPID( p_sys, i_program_pid );
    if( SEEN(p_program_pid) )
    {
        /* Find a free one */
        for( i_program_pid = MIN_ES_PID;
             i_program_pid <= MAX_ES_PID && SEEN(p_program_pid);
             i_program_pid++ )
        {
            p_program_pid = GetPID( p_sys, i_program_pid );
        }
    }

    for( int i=0; i<p_sys->pids.i_all; i++ )
    {
        const ts_pid_t *p_pid = p_sys->pids.pp_all[i];
        if( !SEEN(p_pid) ||
            p_pid->probed.i_type == -1 )
            continue;

        if( i_pcr_pid == 0x1FFF && ( p_pid->probed.i_type == 0x03 ||
                                     p_pid->probed.i_pcr_count ) )
            i_pcr_pid = p_pid->i_pid;

        i_num_pes++;
    }

    if( i_num_pes == 0 )
        return;

    ts_stream_t patstream =
    {
        .i_pid = 0,
        .i_continuity_counter = 0x10,
        .b_discontinuity = false
    };

    ts_stream_t pmtprogramstream =
    {
        .i_pid = i_program_pid,
        .i_continuity_counter = 0x0,
        .b_discontinuity = false
    };

    BuildPAT( GetPID(p_sys, 0)->u.p_pat->handle,
            &p_sys->pids.pat, BuildPATCallback,
            0, 1,
            &patstream,
            1, &pmtprogramstream, &i_program_number );

    /* PAT callback should have been triggered */
    if( p_program_pid->type != TYPE_PMT )
    {
        msg_Err( p_demux, "PAT creation failed" );
        return;
    }

    struct esstreams_t
    {
        pes_stream_t pes;
        ts_stream_t ts;
    };
    es_format_t esfmt = {0};
    struct esstreams_t *esstreams = calloc( i_num_pes, sizeof(struct esstreams_t) );
    pes_mapped_stream_t *mapped = calloc( i_num_pes, sizeof(pes_mapped_stream_t) );
    if( esstreams && mapped )
    {
        int j=0;
        for( int i=0; i<p_sys->pids.i_all; i++ )
        {
            const ts_pid_t *p_pid = p_sys->pids.pp_all[i];

            if( !SEEN(p_pid) ||
                p_pid->probed.i_type == -1 )
                continue;

            esstreams[j].pes.i_codec = p_pid->probed.i_fourcc;
            esstreams[j].pes.i_stream_type = p_pid->probed.i_type;
            esstreams[j].ts.i_pid = p_pid->i_pid;
            mapped[j].pes = &esstreams[j].pes;
            mapped[j].ts = &esstreams[j].ts;
            mapped[j].fmt = &esfmt;
            j++;
        }

        BuildPMT( GetPID(p_sys, 0)->u.p_pat->handle, VLC_OBJECT(p_demux),
                p_program_pid, BuildPMTCallback,
                0, 1,
                i_pcr_pid,
                NULL,
                1, &pmtprogramstream, &i_program_number,
                i_num_pes, mapped );
    }
    free(esstreams);
    free(mapped);
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
    p_sys->b_dvb_meta = true;
    p_sys->b_access_control = true;
    p_sys->b_end_preparse = false;
    ARRAY_INIT( p_sys->programs );
    p_sys->b_default_selection = false;
    p_sys->i_tdt_delta = 0;
    p_sys->i_dvb_start = 0;
    p_sys->i_dvb_length = 0;

    p_sys->vdr = vdr;

    p_sys->arib.b25stream = NULL;
    p_sys->stream = p_demux->s;

    p_sys->b_broken_charset = false;

    p_sys->pids.dummy.i_pid = 8191;
    p_sys->pids.dummy.i_flags = FLAG_SEEN;

    p_sys->i_packet_size = i_packet_size;
    p_sys->i_packet_header_size = i_packet_header_size;
    p_sys->i_ts_read = 50;
    p_sys->csa = NULL;
    p_sys->b_start_record = false;

    p_sys->patfix.i_first_dts = -1;
    p_sys->patfix.i_timesourcepid = 0;
    p_sys->patfix.status = PAT_WAITING;

# define VLC_DVBPSI_DEMUX_TABLE_INIT(table,obj) \
    do { \
        if( !dvbpsi_AttachDemux( (table)->u.p_psi->handle, (dvbpsi_demux_new_cb_t)PSINewTableCallBack, (obj) ) ) \
        { \
            msg_Warn( obj, "Can't dvbpsi_AttachDemux on pid %d", (table)->i_pid );\
        } \
    } while (0)

    /* Init PAT handler */
    patpid = GetPID(p_sys, 0);
    if ( !PIDSetup( p_demux, TYPE_PAT, patpid, NULL ) )
    {
        vlc_mutex_destroy( &p_sys->csa_lock );
        free( p_sys );
        return VLC_ENOMEM;
    }
    if( !dvbpsi_pat_attach( patpid->u.p_pat->handle, PATCallBack, p_demux ) )
    {
        PIDRelease( p_demux, patpid );
        vlc_mutex_destroy( &p_sys->csa_lock );
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( p_sys->b_dvb_meta )
    {
          if( !PIDSetup( p_demux, TYPE_SDT, GetPID(p_sys, 0x11), NULL ) ||
              !PIDSetup( p_demux, TYPE_EIT, GetPID(p_sys, 0x12), NULL ) ||
              !PIDSetup( p_demux, TYPE_TDT, GetPID(p_sys, 0x14), NULL ) )
          {
              PIDRelease( p_demux, GetPID(p_sys, 0x11) );
              PIDRelease( p_demux, GetPID(p_sys, 0x12) );
              PIDRelease( p_demux, GetPID(p_sys, 0x14) );
              p_sys->b_dvb_meta = false;
          }
          else
          {
              VLC_DVBPSI_DEMUX_TABLE_INIT(GetPID(p_sys, 0x11), p_demux);
              VLC_DVBPSI_DEMUX_TABLE_INIT(GetPID(p_sys, 0x12), p_demux);
              VLC_DVBPSI_DEMUX_TABLE_INIT(GetPID(p_sys, 0x14), p_demux);
              if( p_sys->b_access_control &&
                  ( SetPIDFilter( p_sys, GetPID(p_sys, 0x11), true ) ||
                    SetPIDFilter( p_sys, GetPID(p_sys, 0x14), true ) ||
                    SetPIDFilter( p_sys, GetPID(p_sys, 0x12), true ) )
                 )
                     p_sys->b_access_control = false;
          }
    }

# undef VLC_DVBPSI_DEMUX_TABLE_INIT

    p_sys->i_pmt_es = 0;
    p_sys->b_es_all = false;

    /* Read config */
    p_sys->b_es_id_pid = var_CreateGetBool( p_demux, "ts-es-id-pid" );
    p_sys->i_next_extraid = 1;

    p_sys->b_trust_pcr = var_CreateGetBool( p_demux, "ts-trust-pcr" );

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
    p_sys->b_force_seek_per_percent = var_InheritBool( p_demux, "ts-seek-percent" );

    p_sys->arib.e_mode = var_InheritInteger( p_demux, "ts-arib" );

    stream_Control( p_sys->stream, STREAM_CAN_SEEK, &p_sys->b_canseek );
    stream_Control( p_sys->stream, STREAM_CAN_FASTSEEK, &p_sys->b_canfastseek );

    /* Preparse time */
    if( p_sys->b_canseek )
    {
        p_sys->es_creation = NO_ES;
        while( !p_sys->i_pmt_es && !p_sys->b_end_preparse )
            if( Demux( p_demux ) != VLC_DEMUXER_SUCCESS )
                break;
        p_sys->es_creation = DELAY_ES;
    }
    else
        p_sys->es_creation = ( p_sys->b_access_control ? CREATE_ES : DELAY_ES );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    PIDRelease( p_demux, GetPID(p_sys, 0) );

    if( p_sys->b_dvb_meta )
    {
        PIDRelease( p_demux, GetPID(p_sys, 0x11) );
        PIDRelease( p_demux, GetPID(p_sys, 0x12) );
        PIDRelease( p_demux, GetPID(p_sys, 0x14) );
    }

    vlc_mutex_lock( &p_sys->csa_lock );
    if( p_sys->csa )
    {
        var_DelCallback( p_demux, "ts-csa-ck", ChangeKeyCallback, NULL );
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
        p_sys->arib.b25stream->p_source = NULL; /* don't chain kill demuxer's source */
        stream_Delete( p_sys->arib.b25stream );
    }

    vlc_mutex_destroy( &p_sys->csa_lock );

    /* Release all non default pids */
    for( int i = 0; i < p_sys->pids.i_all; i++ )
    {
        ts_pid_t *pid = p_sys->pids.pp_all[i];
#ifndef NDEBUG
        if( pid->type != TYPE_FREE )
            msg_Err( p_demux, "PID %d type %d not freed", pid->i_pid, pid->type );
#endif
        free( pid );
    }
    free( p_sys->pids.pp_all );

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
    }

    /* We read at most 100 TS packet or until a frame is completed */
    for( unsigned i_pkt = 0; i_pkt < p_sys->i_ts_read; i_pkt++ )
    {
        bool         b_frame = false;
        block_t     *p_pkt;
        if( !(p_pkt = ReadTSPacket( p_demux )) )
        {
            return VLC_DEMUXER_EOF;
        }

        if( p_sys->b_start_record )
        {
            /* Enable recording once synchronized */
            stream_Control( p_sys->stream, STREAM_SET_RECORD_STATE, true, "ts" );
            p_sys->b_start_record = false;
        }

        /* Parse the TS packet */
        ts_pid_t *p_pid = GetPID( p_sys, PIDGet( p_pkt ) );

        if( (p_pkt->p_buffer[1] & 0x40) && (p_pkt->p_buffer[3] & 0x10) &&
            !SCRAMBLED(*p_pid) != !(p_pkt->p_buffer[3] & 0x80) )
        {
            UpdateScrambledState( p_demux, p_pid, p_pkt->p_buffer[3] & 0x80 );
        }

        if( !SEEN(p_pid) )
        {
            if( p_pid->type == TYPE_FREE )
                msg_Dbg( p_demux, "pid[%d] unknown", p_pid->i_pid );
            p_pid->i_flags |= FLAG_SEEN;
        }

        if ( SCRAMBLED(*p_pid) && !p_demux->p_sys->csa )
        {
            PCRHandle( p_demux, p_pid, p_pkt );
            block_Release( p_pkt );
            continue;
        }

        /* Probe streams to build PAT/PMT after MIN_PAT_INTERVAL in case we don't see any PAT */
        if( !SEEN( GetPID( p_sys, 0 ) ) &&
            (p_pid->probed.i_type == 0 || p_pid->i_pid == p_sys->patfix.i_timesourcepid) &&
            (p_pkt->p_buffer[1] & 0xC0) == 0x40 && /* Payload start but not corrupt */
            (p_pkt->p_buffer[3] & 0xD0) == 0x10 )  /* Has payload but is not encrypted */
        {
            ProbePES( p_demux, p_pid, p_pkt->p_buffer + TS_HEADER_SIZE,
                      p_pkt->i_buffer - TS_HEADER_SIZE, p_pkt->p_buffer[3] & 0x20 /* Adaptation field */);
        }

        switch( p_pid->type )
        {
        case TYPE_PAT:
            dvbpsi_packet_push( p_pid->u.p_pat->handle, p_pkt->p_buffer );
            block_Release( p_pkt );
            break;

        case TYPE_PMT:
            dvbpsi_packet_push( p_pid->u.p_pmt->handle, p_pkt->p_buffer );
            block_Release( p_pkt );
            break;

        case TYPE_PES:
            p_sys->b_end_preparse = true;

            if( p_sys->es_creation == DELAY_ES ) /* No longer delay ES since that pid's program sends data */
            {
                msg_Dbg( p_demux, "Creating delayed ES" );
                AddAndCreateES( p_demux, p_pid, true );
            }

            if( !p_sys->b_access_control && !(p_pid->i_flags & FLAG_FILTERED) )
            {
                /* That packet is for an unselected ES, don't waste time/memory gathering its data */
                block_Release( p_pkt );
                continue;
            }

            b_frame = GatherData( p_demux, p_pid, p_pkt );
            break;

        case TYPE_SDT:
        case TYPE_TDT:
        case TYPE_EIT:
            if( p_sys->b_dvb_meta )
                dvbpsi_packet_push( p_pid->u.p_psi->handle, p_pkt->p_buffer );
            block_Release( p_pkt );
            break;

        default:
            /* We have to handle PCR if present */
            PCRHandle( p_demux, p_pid, p_pkt );
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
static int DVBEventInformation( demux_t *p_demux, int64_t *pi_time, int64_t *pi_length )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    if( pi_length )
        *pi_length = 0;
    if( pi_time )
        *pi_time = 0;

    if( p_sys->i_dvb_length > 0 )
    {
        const int64_t t = mdate() + p_sys->i_tdt_delta;

        if( p_sys->i_dvb_start <= t && t < p_sys->i_dvb_start + p_sys->i_dvb_length )
        {
            if( pi_length )
                *pi_length = p_sys->i_dvb_length;
            if( pi_time )
                *pi_time   = t - p_sys->i_dvb_start;
            return VLC_SUCCESS;
        }
    }
    return VLC_EGENERIC;
}

static void UpdatePESFilters( demux_t *p_demux, bool b_all )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    ts_pat_t *p_pat = GetPID(p_sys, 0)->u.p_pat;
    for( int i=0; i< p_pat->programs.i_size; i++ )
    {
        ts_pmt_t *p_pmt = p_pat->programs.p_elems[i]->u.p_pmt;
        bool b_program_selected;
        if( (p_sys->b_default_selection && !p_sys->b_access_control) || b_all )
             b_program_selected = true;
        else
             b_program_selected = ProgramIsSelected( p_sys, p_pmt->i_number );

        SetPIDFilter( p_sys, p_pat->programs.p_elems[i], b_program_selected );

        for( int j=0; j<p_pmt->e_streams.i_size; j++ )
        {
            ts_pid_t *espid = p_pmt->e_streams.p_elems[j];
            bool b_stream_selected = b_program_selected;
            if( b_program_selected && !b_all && espid->u.p_pes->es.id )
            {
                es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE,
                                espid->u.p_pes->es.id, &b_stream_selected );
                for( int k=0; !b_stream_selected &&
                               k< espid->u.p_pes->extra_es.i_size; k++ )
                {
                    if( espid->u.p_pes->extra_es.p_elems[k]->id )
                        es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE,
                                        espid->u.p_pes->extra_es.p_elems[k]->id,
                                        &b_stream_selected );
                }
            }

            if( espid->u.p_pes->es.fmt.i_cat == UNKNOWN_ES )
            {
                if( espid->u.p_pes->i_stream_type == 0x13 ) /* Object channel */
                    b_stream_selected = true;
                else if( !p_sys->b_es_all )
                    b_stream_selected = false;
            }

            if( b_stream_selected )
                msg_Dbg( p_demux, "enabling pid %d from program %d", espid->i_pid, p_pmt->i_number );

            SetPIDFilter( p_sys, espid, b_stream_selected );
            if( !b_stream_selected )
                FlushESBuffer( espid->u.p_pes );
        }

        /* Select pcr last in case it is handled by unselected ES */
        if( p_pmt->i_pid_pcr > 0 )
        {
            SetPIDFilter( p_sys, GetPID(p_sys, p_pmt->i_pid_pcr), b_program_selected );
            if( b_program_selected )
                msg_Dbg( p_demux, "enabling pcr pid %d from program %d", p_pmt->i_pid_pcr, p_pmt->i_number );
        }
    }
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double f, *pf;
    bool b_bool, *pb_bool;
    int64_t i64;
    int64_t *pi64;
    int i_int;
    ts_pmt_t *p_pmt;
    int i_first_program = ( p_sys->programs.i_size ) ? p_sys->programs.p_elems[0] : 0;

    if( PREPARSING || !i_first_program || p_sys->b_default_selection )
    {
        if( likely(GetPID(p_sys, 0)->type == TYPE_PAT) )
        {
            ts_pat_t *p_pat = GetPID(p_sys, 0)->u.p_pat;
            /* Set default program for preparse time (no program has been selected) */
            for( int i = 0; i < p_pat->programs.i_size; i++ )
            {
                assert(p_pat->programs.p_elems[i]->type == TYPE_PMT);
                p_pmt = p_pat->programs.p_elems[i]->u.p_pmt;
                if( ( p_pmt->pcr.i_first > -1 || p_pmt->pcr.i_first_dts > VLC_TS_INVALID ) && p_pmt->i_last_dts > 0 )
                {
                    i_first_program = p_pmt->i_number;
                    break;
                }
            }
        }
    }

    switch( i_query )
    {
    case DEMUX_CAN_SEEK:
        *va_arg( args, bool * ) = p_sys->b_canseek;
        return VLC_SUCCESS;

    case DEMUX_GET_POSITION:
        pf = (double*) va_arg( args, double* );

        /* Access control test is because EPG for recordings is not relevant */
        if( p_sys->b_dvb_meta && p_sys->b_access_control )
        {
            int64_t i_time, i_length;
            if( !DVBEventInformation( p_demux, &i_time, &i_length ) && i_length > 0 )
            {
                *pf = (double)i_time/(double)i_length;
                return VLC_SUCCESS;
            }
        }

        if( (p_pmt = GetProgramByID( p_sys, i_first_program )) &&
             p_pmt->pcr.i_first > -1 && p_pmt->i_last_dts > VLC_TS_INVALID &&
             p_pmt->pcr.i_current > -1 )
        {
            double i_length = TimeStampWrapAround( p_pmt,
                                                   p_pmt->i_last_dts ) - p_pmt->pcr.i_first;
            i_length += p_pmt->pcr.i_pcroffset;
            double i_pos = TimeStampWrapAround( p_pmt,
                                                p_pmt->pcr.i_current ) - p_pmt->pcr.i_first;
            *pf = i_pos / i_length;
            return VLC_SUCCESS;
        }

        if( (i64 = stream_Size( p_sys->stream) ) > 0 )
        {
            int64_t offset = stream_Tell( p_sys->stream );
            *pf = (double)offset / (double)i64;
            return VLC_SUCCESS;
        }
        break;

    case DEMUX_SET_POSITION:
        f = (double) va_arg( args, double );

        if(!p_sys->b_canseek)
            break;

        if( p_sys->b_dvb_meta && p_sys->b_access_control &&
           !p_sys->b_force_seek_per_percent &&
           (p_pmt = GetProgramByID( p_sys, i_first_program )) )
        {
            int64_t i_time, i_length;
            if( !DVBEventInformation( p_demux, &i_time, &i_length ) &&
                 i_length > 0 && !SeekToTime( p_demux, p_pmt, TO_SCALE(i_length) * f ) )
            {
                ReadyQueuesPostSeek( p_demux );
                es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME,
                                TO_SCALE(i_length) * f );
                return VLC_SUCCESS;
            }
        }

        if( !p_sys->b_force_seek_per_percent &&
            (p_pmt = GetProgramByID( p_sys, i_first_program )) &&
             p_pmt->pcr.i_first > -1 && p_pmt->i_last_dts > VLC_TS_INVALID &&
             p_pmt->pcr.i_current > -1 )
        {
            double i_length = TimeStampWrapAround( p_pmt,
                                                   p_pmt->i_last_dts ) - p_pmt->pcr.i_first;
            if( !SeekToTime( p_demux, p_pmt, p_pmt->pcr.i_first + i_length * f ) )
            {
                ReadyQueuesPostSeek( p_demux );
                es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME,
                                FROM_SCALE(p_pmt->pcr.i_first + i_length * f) );
                return VLC_SUCCESS;
            }
        }

        i64 = stream_Size( p_sys->stream );
        if( i64 > 0 &&
            stream_Seek( p_sys->stream, (int64_t)(i64 * f) ) == VLC_SUCCESS )
        {
            ReadyQueuesPostSeek( p_demux );
            return VLC_SUCCESS;
        }
        break;

    case DEMUX_SET_TIME:
        i64 = (int64_t)va_arg( args, int64_t );

        if( p_sys->b_canseek &&
           (p_pmt = GetProgramByID( p_sys, i_first_program )) &&
            p_pmt->pcr.i_first > -1 &&
           !SeekToTime( p_demux, p_pmt, p_pmt->pcr.i_first + TO_SCALE(i64) ) )
        {
            ReadyQueuesPostSeek( p_demux );
            es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME,
                            FROM_SCALE(p_pmt->pcr.i_first) + i64 - VLC_TS_0 );
            return VLC_SUCCESS;
        }
        break;

    case DEMUX_GET_TIME:
        pi64 = (int64_t*)va_arg( args, int64_t * );

        if( p_sys->b_dvb_meta && p_sys->b_access_control )
        {
            if( !DVBEventInformation( p_demux, pi64, NULL ) )
                return VLC_SUCCESS;
        }

        if( (p_pmt = GetProgramByID( p_sys, i_first_program )) &&
             p_pmt->pcr.i_current > -1 && p_pmt->pcr.i_first > -1 )
        {
            int64_t i_pcr = TimeStampWrapAround( p_pmt, p_pmt->pcr.i_current );
            *pi64 = FROM_SCALE(i_pcr - p_pmt->pcr.i_first);
            return VLC_SUCCESS;
        }
        break;

    case DEMUX_GET_LENGTH:
        pi64 = (int64_t*)va_arg( args, int64_t * );

        if( p_sys->b_dvb_meta && p_sys->b_access_control )
        {
            if( !DVBEventInformation( p_demux, NULL, pi64 ) )
                return VLC_SUCCESS;
        }

        if( (p_pmt = GetProgramByID( p_sys, i_first_program )) &&
           ( p_pmt->pcr.i_first > -1 || p_pmt->pcr.i_first_dts > VLC_TS_INVALID ) &&
             p_pmt->i_last_dts > 0 )
        {
            int64_t i_start = (p_pmt->pcr.i_first > -1) ? p_pmt->pcr.i_first :
                              TO_SCALE(p_pmt->pcr.i_first_dts);
            int64_t i_last = TimeStampWrapAround( p_pmt, p_pmt->i_last_dts );
            i_last += p_pmt->pcr.i_pcroffset;
            *pi64 = FROM_SCALE(i_last - i_start);
            return VLC_SUCCESS;
        }
        break;

    case DEMUX_SET_GROUP:
    {
        vlc_list_t *p_list;

        i_int = va_arg( args, int );
        p_list = (vlc_list_t *)va_arg( args, vlc_list_t * );
        msg_Dbg( p_demux, "DEMUX_SET_GROUP %d %p", i_int, (void *)p_list );

        if( i_int != 0 ) /* If not default program */
        {
            /* Deselect/filter current ones */

            if( i_int != -1 )
            {
                p_sys->b_es_all = false;
                ARRAY_APPEND( p_sys->programs, i_int );
                UpdatePESFilters( p_demux, false );
            }
            else if( likely( p_list != NULL ) )
            {
                p_sys->b_es_all = false;
                for( int i = 0; i < p_list->i_count; i++ )
                   ARRAY_APPEND( p_sys->programs, p_list->p_values[i].i_int );
                UpdatePESFilters( p_demux, false );
            }
            else // All ES Mode
            {
                p_sys->b_es_all = true;
                ts_pat_t *p_pat = GetPID(p_sys, 0)->u.p_pat;
                for( int i = 0; i < p_pat->programs.i_size; i++ )
                   ARRAY_APPEND( p_sys->programs, p_pat->programs.p_elems[i]->i_pid );
                UpdatePESFilters( p_demux, true );
            }

            p_sys->b_default_selection = false;
        }

        return VLC_SUCCESS;
    }

    case DEMUX_SET_ES:
    {
        i_int = (int)va_arg( args, int );
        msg_Dbg( p_demux, "DEMUX_SET_ES %d", i_int );

        if( !p_sys->b_es_all ) /* Won't change anything */
            UpdatePESFilters( p_demux, false );

        return VLC_SUCCESS;
    }

    case DEMUX_GET_TITLE_INFO:
    {
        struct input_title_t ***v = va_arg( args, struct input_title_t*** );
        int *c = va_arg( args, int * );

        *va_arg( args, int* ) = 0; /* Title offset */
        *va_arg( args, int* ) = 0; /* Chapter offset */
        return stream_Control( p_sys->stream, STREAM_GET_TITLE_INFO, v, c );
    }

    case DEMUX_SET_TITLE:
        return stream_vaControl( p_sys->stream, STREAM_SET_TITLE, args );

    case DEMUX_SET_SEEKPOINT:
        return stream_vaControl( p_sys->stream, STREAM_SET_SEEKPOINT, args );

    case DEMUX_GET_META:
        return stream_vaControl( p_sys->stream, STREAM_GET_META, args );

    case DEMUX_CAN_RECORD:
        pb_bool = (bool*)va_arg( args, bool * );
        *pb_bool = true;
        return VLC_SUCCESS;

    case DEMUX_SET_RECORD_STATE:
        b_bool = (bool)va_arg( args, int );

        if( !b_bool )
            stream_Control( p_sys->stream, STREAM_SET_RECORD_STATE, false );
        p_sys->b_start_record = b_bool;
        return VLC_SUCCESS;

    case DEMUX_GET_SIGNAL:
        return stream_vaControl( p_sys->stream, STREAM_GET_SIGNAL, args );

    default:
        break;
    }

    return VLC_EGENERIC;
}

/*****************************************************************************
 *
 *****************************************************************************/
static int UserPmt( demux_t *p_demux, const char *psz_fmt )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    char *psz_dup = strdup( psz_fmt );
    char *psz = psz_dup;
    int  i_pid;
    int  i_number;

    if( !psz_dup )
        return VLC_ENOMEM;

    /* Parse PID */
    i_pid = strtol( psz, &psz, 0 );
    if( i_pid < 2 || i_pid >= 8192 )
        goto error;

    /* Parse optional program number */
    i_number = 0;
    if( *psz == ':' )
        i_number = strtol( &psz[1], &psz, 0 );

    /* */
    ts_pid_t *pmtpid = GetPID(p_sys, i_pid);

    msg_Dbg( p_demux, "user pmt specified (pid=%d,number=%d)", i_pid, i_number );
    if ( !PIDSetup( p_demux, TYPE_PMT, pmtpid, GetPID(p_sys, 0) ) )
        goto error;

    /* Dummy PMT */
    ts_pmt_t *p_pmt = pmtpid->u.p_pmt;
    p_pmt->i_number   = i_number != 0 ? i_number : TS_USER_PMT_NUMBER;
    if( !dvbpsi_pmt_attach( p_pmt->handle,
                            ((i_number != TS_USER_PMT_NUMBER ? i_number : 1)),
                            PMTCallBack, p_demux ) )
    {
        PIDRelease( p_demux, pmtpid );
        goto error;
    }

    ARRAY_APPEND( GetPID(p_sys, 0)->u.p_pat->programs, pmtpid );

    psz = strchr( psz, '=' );
    if( psz )
        psz++;
    while( psz && *psz )
    {
        char *psz_next = strchr( psz, ',' );
        int i_pid;

        if( psz_next )
            *psz_next++ = '\0';

        i_pid = strtol( psz, &psz, 0 );
        if( *psz != ':' || i_pid < 2 || i_pid >= 8192 )
            goto next;

        char *psz_opt = &psz[1];
        if( !strcmp( psz_opt, "pcr" ) )
        {
            p_pmt->i_pid_pcr = i_pid;
        }
        else if( GetPID(p_sys, i_pid)->type == TYPE_FREE )
        {
            ts_pid_t *pid = GetPID(p_sys, i_pid);

            char *psz_arg = strchr( psz_opt, '=' );
            if( psz_arg )
                *psz_arg++ = '\0';

            if ( !PIDSetup( p_demux, TYPE_PES, pid, pmtpid ) )
                continue;

            ARRAY_APPEND( p_pmt->e_streams, pid );

            if( p_pmt->i_pid_pcr <= 0 )
                p_pmt->i_pid_pcr = i_pid;

            es_format_t *fmt = &pid->u.p_pes->es.fmt;

            if( psz_arg && strlen( psz_arg ) == 4 )
            {
                const vlc_fourcc_t i_codec = VLC_FOURCC( psz_arg[0], psz_arg[1],
                                                         psz_arg[2], psz_arg[3] );
                int i_cat = UNKNOWN_ES;

                if( !strcmp( psz_opt, "video" ) )
                    i_cat = VIDEO_ES;
                else if( !strcmp( psz_opt, "audio" ) )
                    i_cat = AUDIO_ES;
                else if( !strcmp( psz_opt, "spu" ) )
                    i_cat = SPU_ES;

                es_format_Init( fmt, i_cat, i_codec );
                fmt->b_packetized = false;
            }
            else
            {
                const int i_stream_type = strtol( psz_opt, NULL, 0 );
                PIDFillFormat( fmt, i_stream_type, &pid->u.p_pes->data_type );
            }

            fmt->i_group = i_number;
            if( p_sys->b_es_id_pid )
                fmt->i_id = i_pid;

            if( fmt->i_cat != UNKNOWN_ES )
            {
                msg_Dbg( p_demux, "  * es pid=%d fcc=%4.4s", i_pid,
                         (char*)&fmt->i_codec );
                pid->u.p_pes->es.id = es_out_Add( p_demux->out, fmt );
                p_sys->i_pmt_es++;
            }
        }

    next:
        psz = psz_next;
    }

    p_sys->b_user_pmt = true;
    free( psz_dup );
    return VLC_SUCCESS;

error:
    free( psz_dup );
    return VLC_EGENERIC;
}

static int SetPIDFilter( demux_sys_t *p_sys, ts_pid_t *p_pid, bool b_selected )
{
    if( b_selected )
        p_pid->i_flags |= FLAG_FILTERED;
    else
        p_pid->i_flags &= ~FLAG_FILTERED;

    if( !p_sys->b_access_control )
        return VLC_EGENERIC;

    return stream_Control( p_sys->stream, STREAM_SET_PRIVATE_ID_STATE,
                           p_pid->i_pid, b_selected );
}

static void PIDReset( ts_pid_t *pid )
{
    assert(pid->i_refcount == 0);
    pid->i_cc       = 0xff;
    pid->i_flags    &= ~FLAG_SCRAMBLED;
    pid->p_parent    = NULL;
    pid->type = TYPE_FREE;
}

static bool PIDSetup( demux_t *p_demux, ts_pid_type_t i_type, ts_pid_t *pid, ts_pid_t *p_parent )
{
    if( pid == p_parent || pid->i_pid == 0x1FFF )
        return false;

    if( pid->i_refcount == 0 )
    {
        assert( pid->type == TYPE_FREE );
        switch( i_type )
        {
        case TYPE_FREE: /* nonsense ?*/
            PIDReset( pid );
            return true;

        case TYPE_PAT:
            PIDReset( pid );
            pid->u.p_pat = ts_pat_New( p_demux );
            if( !pid->u.p_pat )
                return false;
            break;

        case TYPE_PMT:
            PIDReset( pid );
            pid->u.p_pmt = ts_pmt_New( p_demux );
            if( !pid->u.p_pmt )
                return false;
            break;

        case TYPE_PES:
            PIDReset( pid );
            pid->u.p_pes = ts_pes_New( p_demux );
            if( !pid->u.p_pes )
                return false;
            break;

        case TYPE_SDT:
        case TYPE_TDT:
        case TYPE_EIT:
            PIDReset( pid );
            pid->u.p_psi = ts_psi_New( p_demux );
            if( !pid->u.p_psi )
                return false;
            break;

        default:
            assert(false);
            break;
        }

        pid->i_refcount++;
        pid->type = i_type;
        pid->p_parent = p_parent;
    }
    else if( pid->type == i_type && pid->i_refcount < UINT8_MAX )
    {
        pid->i_refcount++;
    }
    else
    {
        if( pid->type != TYPE_FREE )
            msg_Warn( p_demux, "Tried to redeclare pid %d with another type", pid->i_pid );
        return false;
    }

    return true;
}

static void PIDRelease( demux_t *p_demux, ts_pid_t *pid )
{
    if( pid->i_refcount == 0 )
    {
        assert( pid->type == TYPE_FREE );
        return;
    }
    else if( pid->i_refcount == 1 )
    {
        pid->i_refcount--;
    }
    else if( pid->i_refcount > 1 )
    {
        assert( pid->type != TYPE_FREE && pid->type != TYPE_PAT );
        pid->i_refcount--;
    }

    if( pid->i_refcount == 0 )
    {
        switch( pid->type )
        {
        default:
        case TYPE_FREE: /* nonsense ?*/
            assert( pid->type != TYPE_FREE );
            break;

        case TYPE_PAT:
            ts_pat_Del( p_demux, pid->u.p_pat );
            pid->u.p_pat = NULL;
            break;

        case TYPE_PMT:
            ts_pmt_Del( p_demux, pid->u.p_pmt );
            pid->u.p_pmt = NULL;
            break;

        case TYPE_PES:
            ts_pes_Del( p_demux, pid->u.p_pes );
            pid->u.p_pes = NULL;
            break;

        case TYPE_SDT:
        case TYPE_TDT:
        case TYPE_EIT:
            ts_psi_Del( p_demux, pid->u.p_psi );
            pid->u.p_psi = NULL;
            break;

        }

        SetPIDFilter( p_demux->p_sys, pid, false );
        PIDReset( pid );
    }
}

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
    block_t *out = NULL;
    block_t **last = NULL;

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
        au->p_next = NULL;

        if (!out)
            out = au;
        else
            *last = au;
        last = &au->p_next;

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
    return out;
}

/****************************************************************************
 * gathering stuff
 ****************************************************************************/
static void ParsePES( demux_t *p_demux, ts_pid_t *pid, block_t *p_pes )
{
    uint8_t header[34];
    unsigned i_pes_size = 0;
    unsigned i_skip = 0;
    mtime_t i_dts = -1;
    mtime_t i_pts = -1;
    mtime_t i_length = 0;
    uint8_t i_stream_id;
    const es_mpeg4_descriptor_t *p_mpeg4desc = NULL;

    assert(pid->type == TYPE_PES);
    assert(pid->p_parent && pid->p_parent->type == TYPE_PMT);

    const int i_max = block_ChainExtract( p_pes, header, 34 );
    if ( i_max < 4 )
    {
        block_ChainRelease( p_pes );
        return;
    }

    if( SCRAMBLED(*pid) || header[0] != 0 || header[1] != 0 || header[2] != 1 )
    {
        if ( !SCRAMBLED(*pid) )
            msg_Warn( p_demux, "invalid header [0x%02x:%02x:%02x:%02x] (pid: %d)",
                        header[0], header[1],header[2],header[3], pid->i_pid );
        block_ChainRelease( p_pes );
        return;
    }

    if( ParsePESHeader( VLC_OBJECT(p_demux), (uint8_t*)&header, i_max, &i_skip,
                        &i_dts, &i_pts, &i_stream_id ) == VLC_EGENERIC )
    {
        block_ChainRelease( p_pes );
        return;
    }
    else
    {
        if( i_pts != -1 )
            i_pts = TimeStampWrapAround( pid->p_parent->u.p_pmt, i_pts );
        if( i_dts != -1 )
            i_dts = TimeStampWrapAround( pid->p_parent->u.p_pmt, i_dts );
    }

    if( pid->u.p_pes->es.i_sl_es_id )
        p_mpeg4desc = GetMPEG4DescByEsId( pid->p_parent->u.p_pmt,
                                          pid->u.p_pes->es.i_sl_es_id );

    if( pid->u.p_pes->es.fmt.i_codec == VLC_FOURCC( 'a', '5', '2', 'b' ) ||
        pid->u.p_pes->es.fmt.i_codec == VLC_FOURCC( 'd', 't', 's', 'b' ) )
    {
        i_skip += 4;
    }
    else if( pid->u.p_pes->es.fmt.i_codec == VLC_FOURCC( 'l', 'p', 'c', 'b' ) ||
             pid->u.p_pes->es.fmt.i_codec == VLC_FOURCC( 's', 'p', 'u', 'b' ) ||
             pid->u.p_pes->es.fmt.i_codec == VLC_FOURCC( 's', 'd', 'd', 'b' ) )
    {
        i_skip += 1;
    }
    else if( pid->u.p_pes->es.fmt.i_codec == VLC_CODEC_SUBT && p_mpeg4desc )
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

    if( p_pes )
    {
        block_t *p_block;

        if( i_dts >= 0 )
            p_pes->i_dts = VLC_TS_0 + i_dts * 100 / 9;

        if( i_pts >= 0 )
            p_pes->i_pts = VLC_TS_0 + i_pts * 100 / 9;

        p_pes->i_length = i_length * 100 / 9;

        p_block = block_ChainGather( p_pes );
        if( pid->u.p_pes->es.fmt.i_codec == VLC_CODEC_SUBT )
        {
            if( i_pes_size > 0 && p_block->i_buffer > i_pes_size )
            {
                p_block->i_buffer = i_pes_size;
            }
            /* Append a \0 */
            p_block = block_Realloc( p_block, 0, p_block->i_buffer + 1 );
            if( !p_block )
                return;
            p_block->p_buffer[p_block->i_buffer -1] = '\0';
        }
        else if( pid->u.p_pes->es.fmt.i_codec == VLC_CODEC_TELETEXT )
        {
            if( p_block->i_pts <= VLC_TS_INVALID && pid->p_parent )
            {
                /* Teletext may have missing PTS (ETSI EN 300 472 Annexe A)
                 * In this case use the last PCR + 40ms */
                assert( pid->p_parent->type == TYPE_PMT );
                if( likely(pid->p_parent->type == TYPE_PMT) )
                {
                    mtime_t i_pcr = pid->p_parent->u.p_pmt->pcr.i_current;
                    if( i_pcr > VLC_TS_INVALID )
                        p_block->i_pts = VLC_TS_0 + i_pcr * 100 / 9 + 40000;
                }
            }
        }
        else if( pid->u.p_pes->es.fmt.i_codec == VLC_CODEC_ARIB_A ||
                 pid->u.p_pes->es.fmt.i_codec == VLC_CODEC_ARIB_C )
        {
            if( p_block->i_pts <= VLC_TS_INVALID )
            {
                if( i_pes_size > 0 && p_block->i_buffer > i_pes_size )
                {
                    p_block->i_buffer = i_pes_size;
                }
                /* Append a \0 */
                p_block = block_Realloc( p_block, 0, p_block->i_buffer + 1 );
                if( !p_block )
                    return;
                p_block->p_buffer[p_block->i_buffer -1] = '\0';
            }
        }
        else if( pid->u.p_pes->es.fmt.i_codec == VLC_CODEC_OPUS)
        {
            p_block = Opus_Parse(p_demux, p_block);
        }

        if( !pid->p_parent || pid->p_parent->type != TYPE_PMT )
        {
            block_Release( p_block );
            return;
        }

        ts_pmt_t *p_pmt = pid->p_parent->u.p_pmt;

        while (p_block) {
            block_t *p_next = p_block->p_next;
            p_block->p_next = NULL;

            if( !p_pmt->pcr.b_fix_done ) /* Not seen yet */
                PCRFixHandle( p_demux, p_pmt, p_block );

            if( pid->u.p_pes->es.id && (p_pmt->pcr.i_current > -1 || p_pmt->pcr.b_disable) )
            {
                if( pid->u.p_pes->p_prepcr_outqueue )
                {
                    block_ChainAppend( &pid->u.p_pes->p_prepcr_outqueue, p_block );
                    p_block = pid->u.p_pes->p_prepcr_outqueue;
                    p_next = p_block->p_next;
                    p_block->p_next = NULL;
                    pid->u.p_pes->p_prepcr_outqueue = NULL;
                }

                if ( p_pmt->pcr.b_disable && p_block->i_dts > VLC_TS_INVALID &&
                     ( p_pmt->i_pid_pcr == pid->i_pid || p_pmt->i_pid_pcr == 0x1FFF ) )
                {
                    ProgramSetPCR( p_demux, p_pmt, (p_block->i_dts - VLC_TS_0) * 9 / 100 - 120000 );
                }

                /* Compute PCR/DTS offset if any */
                if( p_pmt->pcr.i_pcroffset == -1 && p_block->i_dts > VLC_TS_INVALID &&
                    p_pmt->pcr.i_current > VLC_TS_INVALID )
                {
                    int64_t i_dts27 = (p_block->i_dts - VLC_TS_0) * 9 / 100;
                    int64_t i_pcr = TimeStampWrapAround( p_pmt, p_pmt->pcr.i_current );
                    if( i_dts27 < i_pcr )
                    {
                        p_pmt->pcr.i_pcroffset = i_pcr - i_dts27 + 80000;
                        msg_Warn( p_demux, "Broken stream: pid %d sends packets with dts %"PRId64
                                           "us later than pcr, applying delay",
                                  pid->i_pid, p_pmt->pcr.i_pcroffset * 100 / 9 );
                    }
                    else p_pmt->pcr.i_pcroffset = 0;
                }

                if( p_pmt->pcr.i_pcroffset != -1 )
                {
                    if( p_block->i_dts > VLC_TS_INVALID )
                        p_block->i_dts += (p_pmt->pcr.i_pcroffset * 100 / 9);
                    if( p_block->i_pts > VLC_TS_INVALID )
                        p_block->i_pts += (p_pmt->pcr.i_pcroffset * 100 / 9);
                }

                /* SL in PES */
                if( pid->u.p_pes->i_stream_type == 0x12 &&
                    ((i_stream_id & 0xFE) == 0xFA) /* 0xFA || 0xFB */ )
                {
                    const es_mpeg4_descriptor_t *p_desc =
                            GetMPEG4DescByEsId( p_pmt, pid->u.p_pes->es.i_sl_es_id );
                    if(!p_desc)
                    {
                        block_Release( p_block );
                        p_block = NULL;
                    }
                    else
                    {
                        sl_header_data header = DecodeSLHeader( p_block->i_buffer, p_block->p_buffer,
                                                                &p_mpeg4desc->sl_descr );
                        p_block->i_buffer -= header.i_size;
                        p_block->p_buffer += header.i_size;
                        p_block->i_dts = header.i_dts ? header.i_dts : p_block->i_dts;
                        p_block->i_pts = header.i_pts ? header.i_pts : p_block->i_pts;

                        /* Assemble access units */
                        if( header.b_au_start && pid->u.p_pes->sl.p_data )
                        {
                            block_ChainRelease( pid->u.p_pes->sl.p_data );
                            pid->u.p_pes->sl.p_data = NULL;
                            pid->u.p_pes->sl.pp_last = &pid->u.p_pes->sl.p_data;
                        }
                        block_ChainLastAppend( &pid->u.p_pes->sl.pp_last, p_block );
                        p_block = NULL;
                        if( header.b_au_end )
                        {
                            p_block = block_ChainGather( pid->u.p_pes->sl.p_data );
                            pid->u.p_pes->sl.p_data = NULL;
                            pid->u.p_pes->sl.pp_last = &pid->u.p_pes->sl.p_data;
                        }
                    }
                }

                if ( p_block )
                {
                    for( int i = 0; i < pid->u.p_pes->extra_es.i_size; i++ )
                    {
                        es_out_Send( p_demux->out, pid->u.p_pes->extra_es.p_elems[i]->id,
                                block_Duplicate( p_block ) );
                    }

                    es_out_Send( p_demux->out, pid->u.p_pes->es.id, p_block );
                }
            }
            else
            {
                if( !p_pmt->pcr.b_fix_done ) /* Not seen yet */
                    PCRFixHandle( p_demux, p_pmt, p_block );

                block_ChainAppend( &pid->u.p_pes->p_prepcr_outqueue, p_block );
            }

            p_block = p_next;
        }
    }
    else
    {
        msg_Warn( p_demux, "empty pes" );
    }
}

static void ParseTableSection( demux_t *p_demux, ts_pid_t *pid, block_t *p_data )
{
    block_t *p_content = block_ChainGather( p_data );

    if( p_content->i_buffer <= 9 || pid->type != TYPE_PES )
    {
        block_Release( p_content );
        return;
    }

    const uint8_t i_table_id = p_content->p_buffer[0];
    const uint8_t i_version = ( p_content->p_buffer[5] & 0x3F ) >> 1;
    ts_pmt_t *p_pmt = pid->p_parent->u.p_pmt;

    if ( pid->u.p_pes->i_stream_type == 0x82 && i_table_id == 0xC6 ) /* SCTE_27 */
    {
        assert( pid->u.p_pes->es.fmt.i_codec == VLC_CODEC_SCTE_27 );
        mtime_t i_date = p_pmt->pcr.i_current;

        /* We need to extract the truncated pts stored inside the payload */
        int i_index = 0;
        size_t i_offset = 4;
        if( p_content->p_buffer[3] & 0x40 )
        {
            i_index = ((p_content->p_buffer[7] & 0x0f) << 8) |
                    p_content->p_buffer[8];
            i_offset = 9;
        }
        if( i_index == 0 && p_content->i_buffer > i_offset + 8 )
        {
            bool is_immediate = p_content->p_buffer[i_offset + 3] & 0x40;
            if( !is_immediate )
            {
                mtime_t i_display_in = GetDWBE( &p_content->p_buffer[i_offset + 4] );
                if( i_display_in < i_date )
                    i_date = i_display_in + (1ll << 32);
                else
                    i_date = i_display_in;
            }

        }

        p_content->i_dts = p_content->i_pts = VLC_TS_0 + i_date * 100 / 9;
        PCRFixHandle( p_demux, p_pmt, p_content );
    }
    /* Object stream SL in table sections */
    else if( pid->u.p_pes->i_stream_type == 0x13 && i_table_id == 0x05 &&
             pid->u.p_pes->es.i_sl_es_id && p_content->i_buffer > 12 )
    {
        const es_mpeg4_descriptor_t *p_mpeg4desc = GetMPEG4DescByEsId( p_pmt, pid->u.p_pes->es.i_sl_es_id );
        if( p_mpeg4desc && p_mpeg4desc->dec_descr.i_objectTypeIndication == 0x01 &&
            p_mpeg4desc->dec_descr.i_streamType == 0x01 /* Object */ &&
            p_pmt->od.i_version != i_version )
        {
            const uint8_t *p_data = p_content->p_buffer;
            int i_data = p_content->i_buffer;

            /* Forward into section */
            uint16_t len = ((p_content->p_buffer[1] & 0x0f) << 8) | p_content->p_buffer[2];
            p_data += 8; i_data -= 8; // SL in table
            i_data = __MIN(i_data, len - 5);
            i_data -= 4; // CRC

            od_descriptors_t *p_ods = &p_pmt->od;
            sl_header_data header = DecodeSLHeader( i_data, p_data, &p_mpeg4desc->sl_descr );

            DecodeODCommand( VLC_OBJECT(p_demux), p_ods, i_data - header.i_size, &p_data[header.i_size] );
            bool b_changed = false;

            for( int i=0; i<p_ods->objects.i_size; i++ )
            {
                od_descriptor_t *p_od = p_ods->objects.p_elems[i];
                for( int j = 0; j < ES_DESCRIPTOR_COUNT && p_od->es_descr[j].b_ok; j++ )
                {
                    p_mpeg4desc = &p_od->es_descr[j];
                    ts_pes_es_t *p_es = GetPMTESBySLEsId( p_pmt, p_mpeg4desc->i_es_id );
                    es_format_t fmt;
                    es_format_Init( &fmt, UNKNOWN_ES, 0 );
                    fmt.i_id = p_es->fmt.i_id;
                    fmt.i_group = p_es->fmt.i_group;

                    if ( p_mpeg4desc && p_mpeg4desc->b_ok && p_es &&
                         SetupISO14496LogicalStream( p_demux, &p_mpeg4desc->dec_descr, &fmt ) &&
                         !es_format_IsSimilar( &fmt, &p_es->fmt ) )
                    {
                        es_format_Clean( &p_es->fmt );
                        p_es->fmt = fmt;

                        es_out_Del( p_demux->out, p_es->id );
                        p_es->fmt.b_packetized = true; /* Split by access unit, no sync code */
                        FREENULL( p_es->fmt.psz_description );
                        p_es->id = es_out_Add( p_demux->out, &p_es->fmt );
                        b_changed = true;
                    }
                }
            }

            if( b_changed )
                UpdatePESFilters( p_demux, p_demux->p_sys->b_es_all );

            p_ods->i_version = i_version;
        }
        block_Release( p_content );
        return;
    }

    if( pid->u.p_pes->es.id )
        es_out_Send( p_demux->out, pid->u.p_pes->es.id, p_content );
    else
        block_Release( p_content );
}

static void ParseData( demux_t *p_demux, ts_pid_t *pid )
{
    block_t *p_data = pid->u.p_pes->p_data;
    assert(p_data);
    if(!p_data)
        return;

    /* remove the pes from pid */
    pid->u.p_pes->p_data = NULL;
    pid->u.p_pes->i_data_size = 0;
    pid->u.p_pes->i_data_gathered = 0;
    pid->u.p_pes->pp_last = &pid->u.p_pes->p_data;

    if( pid->u.p_pes->data_type == TS_ES_DATA_PES )
    {
        ParsePES( p_demux, pid, p_data );
    }
    else if( pid->u.p_pes->data_type == TS_ES_DATA_TABLE_SECTION )
    {
        ParseTableSection( p_demux, pid, p_data );
    }
    else
    {
        block_ChainRelease( p_data );
    }
}

static block_t* ReadTSPacket( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    block_t     *p_pkt;

    /* Get a new TS packet */
    if( !( p_pkt = stream_Block( p_sys->stream, p_sys->i_packet_size ) ) )
    {
        if( stream_Tell( p_sys->stream ) == stream_Size( p_sys->stream ) )
            msg_Dbg( p_demux, "EOF at %"PRId64, stream_Tell( p_sys->stream ) );
        else
            msg_Dbg( p_demux, "Can't read TS packet at %"PRId64, stream_Tell(p_sys->stream) );
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

            i_peek = stream_Peek( p_sys->stream, &p_peek,
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
            if (stream_Read( p_sys->stream, NULL, i_skip ) != i_skip)
                return NULL;

            if( i_skip < i_peek - p_sys->i_packet_size )
            {
                break;
            }
        }
        if( !( p_pkt = stream_Block( p_sys->stream, p_sys->i_packet_size ) ) )
        {
            msg_Dbg( p_demux, "eof ?" );
            return NULL;
        }
    }
    return p_pkt;
}

static int64_t TimeStampWrapAround( ts_pmt_t *p_pmt, int64_t i_time )
{
    int64_t i_adjust = 0;
    if( p_pmt && p_pmt->pcr.i_first > 0x0FFFFFFFF && i_time < 0x0FFFFFFFF )
        i_adjust = 0x1FFFFFFFF;

    return i_time + i_adjust;
}

static mtime_t GetPCR( block_t *p_pkt )
{
    const uint8_t *p = p_pkt->p_buffer;

    mtime_t i_pcr = -1;

    if( ( p[3]&0x20 ) && /* adaptation */
        ( p[5]&0x10 ) &&
        ( p[4] >= 7 ) )
    {
        /* PCR is 33 bits */
        i_pcr = ( (mtime_t)p[6] << 25 ) |
                ( (mtime_t)p[7] << 17 ) |
                ( (mtime_t)p[8] << 9 ) |
                ( (mtime_t)p[9] << 1 ) |
                ( (mtime_t)p[10] >> 7 );
    }
    return i_pcr;
}

static void UpdateScrambledState( demux_t *p_demux, ts_pid_t *p_pid, bool b_scrambled )
{
    if( !SCRAMBLED(*p_pid) == !b_scrambled )
        return;

    msg_Warn( p_demux, "scrambled state changed on pid %d (%d->%d)",
              p_pid->i_pid, !!SCRAMBLED(*p_pid), b_scrambled );

    if( b_scrambled )
        p_pid->i_flags |= FLAG_SCRAMBLED;
    else
        p_pid->i_flags &= ~FLAG_SCRAMBLED;

    if( p_pid->type == TYPE_PES && p_pid->u.p_pes->es.id )
    {
        for( int i = 0; i < p_pid->u.p_pes->extra_es.i_size; i++ )
        {
            if( p_pid->u.p_pes->extra_es.p_elems[i]->id )
                es_out_Control( p_demux->out, ES_OUT_SET_ES_SCRAMBLED_STATE,
                                p_pid->u.p_pes->extra_es.p_elems[i]->id, b_scrambled );
        }
        es_out_Control( p_demux->out, ES_OUT_SET_ES_SCRAMBLED_STATE,
                        p_pid->u.p_pes->es.id, b_scrambled );
    }
}

static inline void FlushESBuffer( ts_pes_t *p_pes )
{
    if( p_pes->p_data )
    {
        p_pes->i_data_gathered = p_pes->i_data_size = 0;
        block_ChainRelease( p_pes->p_data );
        p_pes->p_data = NULL;
        p_pes->pp_last = &p_pes->p_data;
    }

    if( p_pes->sl.p_data )
    {
        block_ChainRelease( p_pes->sl.p_data );
        p_pes->sl.p_data = NULL;
        p_pes->sl.pp_last = &p_pes->sl.p_data;
    }
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

            if( pid->type != TYPE_PES )
                continue;

            if( pid->u.p_pes->es.id )
            {
                block_t *p_block = block_Alloc(1);
                if( p_block )
                {
                    p_block->i_buffer = 0;
                    p_block->i_flags = BLOCK_FLAG_DISCONTINUITY | BLOCK_FLAG_CORRUPTED;
                    es_out_Send( p_demux->out, pid->u.p_pes->es.id, p_block );
                }
            }

            pid->i_cc = 0xff;

            if( pid->u.p_pes->p_prepcr_outqueue )
            {
                block_ChainRelease( pid->u.p_pes->p_prepcr_outqueue );
                pid->u.p_pes->p_prepcr_outqueue = NULL;
            }

            FlushESBuffer( pid->u.p_pes );
        }
        p_pmt->pcr.i_current = -1;
    }
}

static int SeekToTime( demux_t *p_demux, ts_pmt_t *p_pmt, int64_t i_scaledtime )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    /* Deal with common but worst binary search case */
    if( p_pmt->pcr.i_first == i_scaledtime && p_sys->b_canseek )
        return stream_Seek( p_sys->stream, 0 );

    if( !p_sys->b_canfastseek )
        return VLC_EGENERIC;

    int64_t i_initial_pos = stream_Tell( p_sys->stream );

    /* Find the time position by using binary search algorithm. */
    int64_t i_head_pos = 0;
    int64_t i_tail_pos = stream_Size( p_sys->stream ) - p_sys->i_packet_size;
    if( i_head_pos >= i_tail_pos )
        return VLC_EGENERIC;

    bool b_found = false;
    while( (i_head_pos + p_sys->i_packet_size) <= i_tail_pos && !b_found )
    {
        /* Round i_pos to a multiple of p_sys->i_packet_size */
        int64_t i_splitpos = i_head_pos + (i_tail_pos - i_head_pos) / 2;
        int64_t i_div = i_splitpos % p_sys->i_packet_size;
        i_splitpos -= i_div;

        if ( stream_Seek( p_sys->stream, i_splitpos ) != VLC_SUCCESS )
            break;

        int64_t i_pos = i_splitpos;
        while( i_pos > -1 && i_pos < i_tail_pos )
        {
            int64_t i_pcr = -1;
            block_t *p_pkt = ReadTSPacket( p_demux );
            if( !p_pkt )
            {
                i_head_pos = i_tail_pos;
                break;
            }
            else
                i_pos = stream_Tell( p_sys->stream );

            int i_pid = PIDGet( p_pkt );
            if( i_pid != 0x1FFF && GetPID(p_sys, i_pid)->type == TYPE_PES &&
                GetPID(p_sys, i_pid)->p_parent->u.p_pmt == p_pmt &&
               (p_pkt->p_buffer[1] & 0xC0) == 0x40 && /* Payload start but not corrupt */
               (p_pkt->p_buffer[3] & 0xD0) == 0x10    /* Has payload but is not encrypted */
            )
            {
                unsigned i_skip = 4;
                if ( p_pkt->p_buffer[3] & 0x20 ) // adaptation field
                {
                    if( p_pkt->i_buffer >= 4 + 2 + 5 )
                    {
                        i_pcr = GetPCR( p_pkt );
                        i_skip += 1 + p_pkt->p_buffer[4];
                    }
                }
                else
                {
                    mtime_t i_dts = -1;
                    mtime_t i_pts = -1;
                    uint8_t i_stream_id;
                    if ( VLC_SUCCESS == ParsePESHeader( VLC_OBJECT(p_demux), &p_pkt->p_buffer[i_skip],
                                                        p_pkt->i_buffer - i_skip, &i_skip,
                                                        &i_dts, &i_pts, &i_stream_id ) )
                    {
                        if( i_dts > -1 )
                            i_pcr = i_dts;
                    }
                }
            }
            block_Release( p_pkt );

            if( i_pcr != -1 )
            {
                int64_t i_diff = i_scaledtime - TimeStampWrapAround( p_pmt, i_pcr );
                if ( i_diff < 0 )
                    i_tail_pos = i_splitpos - p_sys->i_packet_size;
                else if( i_diff < TO_SCALE(VLC_TS_0 + CLOCK_FREQ / 2) ) // 500ms
                    b_found = true;
                else
                    i_head_pos = i_pos;
                break;
            }
        }

        if ( !b_found && i_pos > i_tail_pos - p_sys->i_packet_size )
            i_tail_pos = i_splitpos - p_sys->i_packet_size;
    }

    if( !b_found )
    {
        msg_Dbg( p_demux, "Seek():cannot find a time position." );
        stream_Seek( p_sys->stream, i_initial_pos );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static ts_pid_t *GetPID( demux_sys_t *p_sys, uint16_t i_pid )
{
    switch( i_pid )
    {
        case 0:
            return &p_sys->pids.pat;
        case 0x1FFF:
            return &p_sys->pids.dummy;
        default:
            if( p_sys->pids.i_last_pid == i_pid )
                return p_sys->pids.p_last;
        break;
    }

    for( int i=0; i < p_sys->pids.i_all; i++ )
    {
        if( p_sys->pids.pp_all[i]->i_pid == i_pid )
        {
            p_sys->pids.p_last = p_sys->pids.pp_all[i];
            p_sys->pids.i_last_pid = i_pid;
            return p_sys->pids.p_last;
        }
    }

    if( p_sys->pids.i_all >= p_sys->pids.i_all_alloc )
    {
        ts_pid_t **p_realloc = realloc( p_sys->pids.pp_all,
                                       (p_sys->pids.i_all_alloc + PID_ALLOC_CHUNK) * sizeof(ts_pid_t *) );
        if( !p_realloc )
            return NULL;
        p_sys->pids.pp_all = p_realloc;
        p_sys->pids.i_all_alloc += PID_ALLOC_CHUNK;
    }

    ts_pid_t *p_pid = calloc( 1, sizeof(*p_pid) );
    if( !p_pid )
    {
        abort();
        //return NULL;
    }

    p_pid->i_pid = i_pid;
    p_sys->pids.pp_all[p_sys->pids.i_all++] = p_pid;

    p_sys->pids.p_last = p_pid;
    p_sys->pids.i_last_pid = i_pid;

    return p_pid;
}

static ts_pmt_t * GetProgramByID( demux_sys_t *p_sys, int i_program )
{
    if(unlikely(GetPID(p_sys, 0)->type != TYPE_PAT))
        return NULL;

    ts_pat_t *p_pat = GetPID(p_sys, 0)->u.p_pat;
    for( int i = 0; i < p_pat->programs.i_size; i++ )
    {
        assert(p_pat->programs.p_elems[i]->type == TYPE_PMT);
        ts_pmt_t *p_pmt = p_pat->programs.p_elems[i]->u.p_pmt;
        if( p_pmt->i_number == i_program )
            return p_pmt;
    }
    return NULL;
}

#define PROBE_CHUNK_COUNT 250

static int ProbeChunk( demux_t *p_demux, int i_program, bool b_end, int64_t *pi_pcr, bool *pb_found )
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
                p_pid->type == TYPE_PES &&
                p_pid->u.p_pes->es.fmt.i_cat != UNKNOWN_ES
              )
            {
                b_pcrresult = false;
                mtime_t i_dts = -1;
                mtime_t i_pts = -1;
                uint8_t i_stream_id;
                unsigned i_skip = 4;
                if ( b_adaptfield ) // adaptation field
                    i_skip += 1 + p_pkt->p_buffer[4];

                if ( VLC_SUCCESS == ParsePESHeader( VLC_OBJECT(p_demux), &p_pkt->p_buffer[i_skip],
                                                    p_pkt->i_buffer - i_skip, &i_skip,
                                                    &i_dts, &i_pts, &i_stream_id ) )
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
                    if( ( p_pmt->i_pid_pcr == p_pid->i_pid ||
                        ( p_pmt->i_pid_pcr == 0x1FFF && p_pid->p_parent == p_pat->programs.p_elems[i] ) ) )
                    {
                        if( b_end )
                        {
                            p_pmt->i_last_dts = *pi_pcr;
                        }
                        /* Start, only keep first */
                        else if( b_pcrresult && p_pmt->pcr.i_first == -1 )
                        {
                            p_pmt->pcr.i_first = *pi_pcr;
                        }
                        else if( p_pmt->pcr.i_first_dts < VLC_TS_0 )
                        {
                            p_pmt->pcr.i_first_dts = FROM_SCALE(*pi_pcr);
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

static int ProbeStart( demux_t *p_demux, int i_program )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const int64_t i_initial_pos = stream_Tell( p_sys->stream );
    int64_t i_stream_size = stream_Size( p_sys->stream );

    int i_probe_count = 0;
    int64_t i_pos;
    mtime_t i_pcr = -1;
    bool b_found = false;

    do
    {
        i_pos = p_sys->i_packet_size * i_probe_count;
        i_pos = __MIN( i_pos, i_stream_size );

        if( stream_Seek( p_sys->stream, i_pos ) )
            return VLC_EGENERIC;

        ProbeChunk( p_demux, i_program, false, &i_pcr, &b_found );

        /* Go ahead one more chunk if end of file contained only stuffing packets */
        i_probe_count += PROBE_CHUNK_COUNT;
    } while( i_pos > 0 && (i_pcr == -1 || !b_found) && i_probe_count < (2 * PROBE_CHUNK_COUNT) );

    stream_Seek( p_sys->stream, i_initial_pos );

    return (b_found) ? VLC_SUCCESS : VLC_EGENERIC;
}

static int ProbeEnd( demux_t *p_demux, int i_program )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const int64_t i_initial_pos = stream_Tell( p_sys->stream );
    int64_t i_stream_size = stream_Size( p_sys->stream );

    int i_probe_count = PROBE_CHUNK_COUNT;
    int64_t i_pos;
    mtime_t i_pcr = -1;
    bool b_found = false;

    do
    {
        i_pos = i_stream_size - (p_sys->i_packet_size * i_probe_count);
        i_pos = __MAX( i_pos, 0 );

        if( stream_Seek( p_sys->stream, i_pos ) )
            return VLC_EGENERIC;

        ProbeChunk( p_demux, i_program, true, &i_pcr, &b_found );

        /* Go ahead one more chunk if end of file contained only stuffing packets */
        i_probe_count += PROBE_CHUNK_COUNT;
    } while( i_pos > 0 && (i_pcr == -1 || !b_found) && i_probe_count < (6 * PROBE_CHUNK_COUNT) );

    stream_Seek( p_sys->stream, i_initial_pos );

    return (b_found) ? VLC_SUCCESS : VLC_EGENERIC;
}

static void ProgramSetPCR( demux_t *p_demux, ts_pmt_t *p_pmt, mtime_t i_pcr )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    /* Check if we have enqueued blocks waiting the/before the
       PCR barrier, and then adapt pcr so they have valid PCR when dequeuing */
    if( p_pmt->pcr.i_current == -1 && p_pmt->pcr.b_fix_done )
    {
        mtime_t i_mindts = -1;

        ts_pat_t *p_pat = GetPID(p_sys, 0)->u.p_pat;
        for( int i=0; i< p_pat->programs.i_size; i++ )
        {
            ts_pmt_t *p_pmt = p_pat->programs.p_elems[i]->u.p_pmt;
            for( int j=0; j<p_pmt->e_streams.i_size; j++ )
            {
                ts_pid_t *p_pid = p_pmt->e_streams.p_elems[j];
                block_t *p_block = p_pid->u.p_pes->p_prepcr_outqueue;
                while( p_block && p_block->i_dts == VLC_TS_INVALID )
                    p_block = p_block->p_next;

                if( p_block && ( i_mindts == -1 || p_block->i_dts < i_mindts ) )
                    i_mindts = p_block->i_dts;
            }
        }

        if( i_mindts > VLC_TS_INVALID )
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
        es_out_Control( p_demux->out, ES_OUT_SET_GROUP_PCR,
                        p_pmt->i_number, VLC_TS_0 + i_pcr * 100 / 9 );
    }
}

static void PCRHandle( demux_t *p_demux, ts_pid_t *pid, block_t *p_bk )
{
    demux_sys_t   *p_sys = p_demux->p_sys;

    mtime_t i_pcr = GetPCR( p_bk );
    if( i_pcr < 0 )
        return;

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
        mtime_t i_program_pcr = TimeStampWrapAround( p_pmt, i_pcr );

        if( p_pmt->i_pid_pcr == 0x1FFF ) /* That program has no dedicated PCR pid ISO/IEC 13818-1 2.4.4.9 */
        {
            if( pid->p_parent == p_pat->programs.p_elems[i] ) /* PCR shall be on pid itself */
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
                ProgramSetPCR( p_demux, p_pmt, i_program_pcr );
            }
        }

    }
}

static int FindPCRCandidate( ts_pmt_t *p_pmt )
{
    ts_pid_t *p_cand = NULL;
    int i_previous = p_pmt->i_pid_pcr;

    for( int i=0; i<p_pmt->e_streams.i_size; i++ )
    {
        ts_pid_t *p_pid = p_pmt->e_streams.p_elems[i];
        if( SEEN(p_pid) &&
            (!p_cand || p_cand->i_pid != i_previous) )
        {
            if( p_pid->probed.i_pcr_count ) /* check PCR frequency first */
            {
                if( !p_cand || p_pid->probed.i_pcr_count > p_cand->probed.i_pcr_count )
                {
                    p_cand = p_pid;
                    continue;
                }
            }

            if( p_pid->u.p_pes->es.fmt.i_cat == AUDIO_ES )
            {
                if( !p_cand )
                    p_cand = p_pid;
            }
            else if ( p_pid->u.p_pes->es.fmt.i_cat == VIDEO_ES ) /* Otherwise prioritize video dts */
            {
                if( !p_cand || p_cand->u.p_pes->es.fmt.i_cat == AUDIO_ES )
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
    if ( p_pmt->pcr.b_disable || p_pmt->pcr.b_fix_done )
    {
        return;
    }
    /* Record the first data packet timestamp in case there wont be any PCR */
    else if( !p_pmt->pcr.i_first_dts )
    {
        p_pmt->pcr.i_first_dts = p_block->i_dts;
    }
    else if( p_block->i_dts - p_pmt->pcr.i_first_dts > CLOCK_FREQ / 2 ) /* "PCR repeat rate shall not exceed 100ms" */
    {
        if( p_pmt->pcr.i_current < 0 &&
            GetPID( p_demux->p_sys, p_pmt->i_pid_pcr )->probed.i_pcr_count == 0 )
        {
            int i_cand = FindPCRCandidate( p_pmt );
            p_pmt->i_pid_pcr = i_cand;
            if ( GetPID( p_demux->p_sys, p_pmt->i_pid_pcr )->probed.i_pcr_count == 0 )
                p_pmt->pcr.b_disable = true;
            msg_Warn( p_demux, "No PCR received for program %d, set up workaround using pid %d",
                      p_pmt->i_number, i_cand );
            UpdatePESFilters( p_demux, p_demux->p_sys->b_es_all );
        }
        p_pmt->pcr.b_fix_done = true;
    }
}

static bool GatherData( demux_t *p_demux, ts_pid_t *pid, block_t *p_bk )
{
    const uint8_t *p = p_bk->p_buffer;
    const bool b_unit_start = p[1]&0x40;
    const bool b_adaptation = p[3]&0x20;
    const bool b_payload    = p[3]&0x10;
    const int  i_cc         = p[3]&0x0f; /* continuity counter */
    bool       b_discontinuity = false;  /* discontinuity */

    /* transport_scrambling_control is ignored */
    int         i_skip = 0;
    bool        i_ret  = false;

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
        if( pid->u.p_pes->p_data ) //&& pid->es->fmt.i_cat == VIDEO_ES )
            pid->u.p_pes->p_data->i_flags |= BLOCK_FLAG_CORRUPTED;
    }

    if( p_demux->p_sys->csa )
    {
        vlc_mutex_lock( &p_demux->p_sys->csa_lock );
        csa_Decrypt( p_demux->p_sys->csa, p_bk->p_buffer, p_demux->p_sys->i_csa_pkt_size );
        vlc_mutex_unlock( &p_demux->p_sys->csa_lock );
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
            /* discontinuity indicator found in stream */
            b_discontinuity = (p[5]&0x80) ? true : false;
            if( b_discontinuity && pid->u.p_pes->p_data )
            {
                msg_Warn( p_demux, "discontinuity indicator (pid=%d) ",
                            pid->i_pid );
                /* pid->es->p_data->i_flags |= BLOCK_FLAG_DISCONTINUITY; */
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
    const int i_diff = ( i_cc - pid->i_cc )&0x0f;
    if( b_payload && i_diff == 1 )
    {
        pid->i_cc = ( pid->i_cc + 1 ) & 0xf;
    }
    else
    {
        if( pid->i_cc == 0xff )
        {
            msg_Warn( p_demux, "first packet for pid=%d cc=0x%x",
                      pid->i_pid, i_cc );
            pid->i_cc = i_cc;
        }
        else if( i_diff != 0 && !b_discontinuity )
        {
            msg_Warn( p_demux, "discontinuity received 0x%x instead of 0x%x (pid=%d)",
                      i_cc, ( pid->i_cc + 1 )&0x0f, pid->i_pid );

            pid->i_cc = i_cc;
            if( pid->u.p_pes->p_data && pid->u.p_pes->es.fmt.i_cat != VIDEO_ES &&
                pid->u.p_pes->es.fmt.i_cat != AUDIO_ES )
            {
                /* Small audio/video artifacts are usually better than
                 * dropping full frames */
                pid->u.p_pes->p_data->i_flags |= BLOCK_FLAG_CORRUPTED;
            }
        }
    }

    PCRHandle( p_demux, pid, p_bk );

    if( i_skip >= 188 )
    {
        block_Release( p_bk );
        return i_ret;
    }

    /* We have to gather it */
    p_bk->p_buffer += i_skip;
    p_bk->i_buffer -= i_skip;

    if( b_unit_start )
    {
        if( pid->u.p_pes->data_type == TS_ES_DATA_TABLE_SECTION && p_bk->i_buffer > 0 )
        {
            int i_pointer_field = __MIN( p_bk->p_buffer[0], p_bk->i_buffer - 1 );
            block_t *p = block_Duplicate( p_bk );
            if( p )
            {
                p->i_buffer = i_pointer_field;
                p->p_buffer++;
                block_ChainLastAppend( &pid->u.p_pes->pp_last, p );
            }
            p_bk->i_buffer -= 1 + i_pointer_field;
            p_bk->p_buffer += 1 + i_pointer_field;
        }
        if( pid->u.p_pes->p_data )
        {
            ParseData( p_demux, pid );
            i_ret = true;
        }

        block_ChainLastAppend( &pid->u.p_pes->pp_last, p_bk );
        if( pid->u.p_pes->data_type == TS_ES_DATA_PES )
        {
            if( p_bk->i_buffer > 6 )
            {
                pid->u.p_pes->i_data_size = GetWBE( &p_bk->p_buffer[4] );
                if( pid->u.p_pes->i_data_size > 0 )
                {
                    pid->u.p_pes->i_data_size += 6;
                }
            }
        }
        else if( pid->u.p_pes->data_type == TS_ES_DATA_TABLE_SECTION )
        {
            if( p_bk->i_buffer > 3 && p_bk->p_buffer[0] != 0xff )
            {
                pid->u.p_pes->i_data_size = 3 + (((p_bk->p_buffer[1] & 0xf) << 8) | p_bk->p_buffer[2]);
            }
        }
        pid->u.p_pes->i_data_gathered += p_bk->i_buffer;
        if( pid->u.p_pes->i_data_size > 0 &&
            pid->u.p_pes->i_data_gathered >= pid->u.p_pes->i_data_size )
        {
            ParseData( p_demux, pid );
            i_ret = true;
        }
    }
    else
    {
        if( pid->u.p_pes->p_data == NULL )
        {
            /* msg_Dbg( p_demux, "broken packet" ); */
            block_Release( p_bk );
        }
        else
        {
            block_ChainLastAppend( &pid->u.p_pes->pp_last, p_bk );
            pid->u.p_pes->i_data_gathered += p_bk->i_buffer;

            if( pid->u.p_pes->i_data_size > 0 &&
                pid->u.p_pes->i_data_gathered >= pid->u.p_pes->i_data_size )
            {
                ParseData( p_demux, pid );
                i_ret = true;
            }
        }
    }

    return i_ret;
}

static void PIDFillFormat( es_format_t *fmt, int i_stream_type, ts_es_data_type_t *p_datatype )
{
    switch( i_stream_type )
    {
    case 0x01:  /* MPEG-1 video */
    case 0x02:  /* MPEG-2 video */
    case 0x80:  /* MPEG-2 MOTO video */
        es_format_Init( fmt, VIDEO_ES, VLC_CODEC_MPGV );
        break;
    case 0x03:  /* MPEG-1 audio */
    case 0x04:  /* MPEG-2 audio */
        es_format_Init( fmt, AUDIO_ES, VLC_CODEC_MPGA );
        break;
    case 0x11:  /* MPEG4 (audio) LATM */
    case 0x0f:  /* ISO/IEC 13818-7 Audio with ADTS transport syntax */
    case 0x1c:  /* ISO/IEC 14496-3 Audio, without using any additional
                   transport syntax, such as DST, ALS and SLS */
        es_format_Init( fmt, AUDIO_ES, VLC_CODEC_MP4A );
        break;
    case 0x10:  /* MPEG4 (video) */
        es_format_Init( fmt, VIDEO_ES, VLC_CODEC_MP4V );
        break;
    case 0x1B:  /* H264 <- check transport syntax/needed descriptor */
        es_format_Init( fmt, VIDEO_ES, VLC_CODEC_H264 );
        break;
    case 0x24:  /* HEVC */
        es_format_Init( fmt, VIDEO_ES, VLC_CODEC_HEVC );
        break;
    case 0x42:  /* CAVS (Chinese AVS) */
        es_format_Init( fmt, VIDEO_ES, VLC_CODEC_CAVS );
        break;

    case 0x81:  /* A52 (audio) */
        es_format_Init( fmt, AUDIO_ES, VLC_CODEC_A52 );
        break;
    case 0x82:  /* SCTE-27 (sub) */
        es_format_Init( fmt, SPU_ES, VLC_CODEC_SCTE_27 );
        *p_datatype = TS_ES_DATA_TABLE_SECTION;
        break;
    case 0x84:  /* SDDS (audio) */
        es_format_Init( fmt, AUDIO_ES, VLC_CODEC_SDDS );
        break;
    case 0x85:  /* DTS (audio) */
        es_format_Init( fmt, AUDIO_ES, VLC_CODEC_DTS );
        break;
    case 0x87: /* E-AC3 */
        es_format_Init( fmt, AUDIO_ES, VLC_CODEC_EAC3 );
        break;

    case 0x91:  /* A52 vls (audio) */
        es_format_Init( fmt, AUDIO_ES, VLC_FOURCC( 'a', '5', '2', 'b' ) );
        break;
    case 0x92:  /* DVD_SPU vls (sub) */
        es_format_Init( fmt, SPU_ES, VLC_FOURCC( 's', 'p', 'u', 'b' ) );
        break;

    case 0x94:  /* SDDS (audio) */
        es_format_Init( fmt, AUDIO_ES, VLC_FOURCC( 's', 'd', 'd', 'b' ) );
        break;

    case 0xa0:  /* MSCODEC vlc (video) (fixed later) */
        es_format_Init( fmt, UNKNOWN_ES, 0 );
        break;

    case 0x06:  /* PES_PRIVATE  (fixed later) */
    case 0x12:  /* MPEG-4 generic (sub/scene/...) (fixed later) */
    case 0xEA:  /* Privately managed ES (VC-1) (fixed later */
    default:
        es_format_Init( fmt, UNKNOWN_ES, 0 );
        break;
    }

    /* PES packets usually contain truncated frames */
    fmt->b_packetized = false;
}

/****************************************************************************
 ****************************************************************************
 ** libdvbpsi callbacks
 ****************************************************************************
 ****************************************************************************/
static bool ProgramIsSelected( demux_sys_t *p_sys, uint16_t i_pgrm )
{
    for(int i=0; i<p_sys->programs.i_size; i++)
        if( p_sys->programs.p_elems[i] == i_pgrm )
            return true;

    return false;
}

static void ValidateDVBMeta( demux_t *p_demux, int i_pid )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( !p_sys->b_dvb_meta || ( i_pid != 0x11 && i_pid != 0x12 && i_pid != 0x14 ) )
        return;

    msg_Warn( p_demux, "Switching to non DVB mode" );

    /* This doesn't look like a DVB stream so don't try
     * parsing the SDT/EDT/TDT */

    PIDRelease( p_demux, GetPID(p_sys, 0x11) );
    PIDRelease( p_demux, GetPID(p_sys, 0x12) );
    PIDRelease( p_demux, GetPID(p_sys, 0x14) );
    p_sys->b_dvb_meta = false;
}

#include "../dvb-text.h"

static char *EITConvertToUTF8( demux_t *p_demux,
                               const unsigned char *psz_instring,
                               size_t i_length,
                               bool b_broken )
{
    demux_sys_t *p_sys = p_demux->p_sys;
#ifdef HAVE_ARIBB24
    if( p_sys->arib.e_mode == ARIBMODE_ENABLED )
    {
        if ( !p_sys->arib.p_instance )
            p_sys->arib.p_instance = arib_instance_new( p_demux );
        if ( !p_sys->arib.p_instance )
            return NULL;
        arib_decoder_t *p_decoder = arib_get_decoder( p_sys->arib.p_instance );
        if ( !p_decoder )
            return NULL;

        char *psz_outstring = NULL;
        size_t i_out;

        i_out = i_length * 4;
        psz_outstring = (char*) calloc( i_out + 1, sizeof(char) );
        if( !psz_outstring )
            return NULL;

        arib_initialize_decoder( p_decoder );
        i_out = arib_decode_buffer( p_decoder, psz_instring, i_length,
                                    psz_outstring, i_out );
        arib_finalize_decoder( p_decoder );

        return psz_outstring;
    }
#else
    VLC_UNUSED(p_sys);
#endif
    /* Deal with no longer broken providers (no switch byte
      but sending ISO_8859-1 instead of ISO_6937) without
      removing them from the broken providers table
      (keep the entry for correctly handling recorded TS).
    */
    b_broken = b_broken && i_length && *psz_instring > 0x20;

    if( b_broken )
        return FromCharset( "ISO_8859-1", psz_instring, i_length );
    return vlc_from_EIT( psz_instring, i_length );
}

static void SDTCallBack( demux_t *p_demux, dvbpsi_sdt_t *p_sdt )
{
    demux_sys_t          *p_sys = p_demux->p_sys;
    ts_pid_t             *sdt = GetPID(p_sys, 0x11);
    dvbpsi_sdt_service_t *p_srv;

    msg_Dbg( p_demux, "SDTCallBack called" );

    if( p_sys->es_creation != CREATE_ES ||
       !p_sdt->b_current_next ||
        p_sdt->i_version == sdt->u.p_psi->i_version )
    {
        dvbpsi_sdt_delete( p_sdt );
        return;
    }

    msg_Dbg( p_demux, "new SDT ts_id=%d version=%d current_next=%d "
             "network_id=%d",
             p_sdt->i_extension,
             p_sdt->i_version, p_sdt->b_current_next,
             p_sdt->i_network_id );

    p_sys->b_broken_charset = false;

    for( p_srv = p_sdt->p_first_service; p_srv; p_srv = p_srv->p_next )
    {
        vlc_meta_t          *p_meta;
        dvbpsi_descriptor_t *p_dr;

        const char *psz_type = NULL;
        const char *psz_status = NULL;

        msg_Dbg( p_demux, "  * service id=%d eit schedule=%d present=%d "
                 "running=%d free_ca=%d",
                 p_srv->i_service_id, p_srv->b_eit_schedule,
                 p_srv->b_eit_present, p_srv->i_running_status,
                 p_srv->b_free_ca );

        if( p_sys->vdr.i_service && p_srv->i_service_id != p_sys->vdr.i_service )
        {
            msg_Dbg( p_demux, "  * service id=%d skipped (not declared in vdr header)",
                     p_sys->vdr.i_service );
            continue;
        }

        p_meta = vlc_meta_New();
        for( p_dr = p_srv->p_first_descriptor; p_dr; p_dr = p_dr->p_next )
        {
            if( p_dr->i_tag == 0x48 )
            {
                static const char *ppsz_type[17] = {
                    "Reserved",
                    "Digital television service",
                    "Digital radio sound service",
                    "Teletext service",
                    "NVOD reference service",
                    "NVOD time-shifted service",
                    "Mosaic service",
                    "PAL coded signal",
                    "SECAM coded signal",
                    "D/D2-MAC",
                    "FM Radio",
                    "NTSC coded signal",
                    "Data broadcast service",
                    "Reserved for Common Interface Usage",
                    "RCS Map (see EN 301 790 [35])",
                    "RCS FLS (see EN 301 790 [35])",
                    "DVB MHP service"
                };
                dvbpsi_service_dr_t *pD = dvbpsi_DecodeServiceDr( p_dr );
                char *str1 = NULL;
                char *str2 = NULL;

                /* Workarounds for broadcasters with broken EPG */

                if( p_sdt->i_network_id == 133 )
                    p_sys->b_broken_charset = true;  /* SKY DE & BetaDigital use ISO8859-1 */

                /* List of providers using ISO8859-1 */
                static const char ppsz_broken_providers[][8] = {
                    "CSAT",     /* CanalSat FR */
                    "GR1",      /* France televisions */
                    "MULTI4",   /* NT1 */
                    "MR5",      /* France 2/M6 HD */
                    ""
                };
                for( int i = 0; *ppsz_broken_providers[i]; i++ )
                {
                    const size_t i_length = strlen(ppsz_broken_providers[i]);
                    if( pD->i_service_provider_name_length == i_length &&
                        !strncmp( (char *)pD->i_service_provider_name, ppsz_broken_providers[i], i_length ) )
                        p_sys->b_broken_charset = true;
                }

                /* FIXME: Digital+ ES also uses ISO8859-1 */

                str1 = EITConvertToUTF8(p_demux,
                                        pD->i_service_provider_name,
                                        pD->i_service_provider_name_length,
                                        p_sys->b_broken_charset );
                str2 = EITConvertToUTF8(p_demux,
                                        pD->i_service_name,
                                        pD->i_service_name_length,
                                        p_sys->b_broken_charset );

                msg_Dbg( p_demux, "    - type=%d provider=%s name=%s",
                         pD->i_service_type, str1, str2 );

                vlc_meta_SetTitle( p_meta, str2 );
                vlc_meta_SetPublisher( p_meta, str1 );
                if( pD->i_service_type >= 0x01 && pD->i_service_type <= 0x10 )
                    psz_type = ppsz_type[pD->i_service_type];
                free( str1 );
                free( str2 );
            }
        }

        if( p_srv->i_running_status >= 0x01 && p_srv->i_running_status <= 0x04 )
        {
            static const char *ppsz_status[5] = {
                "Unknown",
                "Not running",
                "Starts in a few seconds",
                "Pausing",
                "Running"
            };
            psz_status = ppsz_status[p_srv->i_running_status];
        }

        if( psz_type )
            vlc_meta_AddExtra( p_meta, "Type", psz_type );
        if( psz_status )
            vlc_meta_AddExtra( p_meta, "Status", psz_status );

        es_out_Control( p_demux->out, ES_OUT_SET_GROUP_META,
                        p_srv->i_service_id, p_meta );
        vlc_meta_Delete( p_meta );
    }

    sdt->u.p_psi->i_version = p_sdt->i_version;
    dvbpsi_sdt_delete( p_sdt );
}

/* i_year: year - 1900  i_month: 0-11  i_mday: 1-31 i_hour: 0-23 i_minute: 0-59 i_second: 0-59 */
static int64_t vlc_timegm( int i_year, int i_month, int i_mday, int i_hour, int i_minute, int i_second )
{
    static const int pn_day[12+1] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    int64_t i_day;

    if( i_year < 70 ||
        i_month < 0 || i_month > 11 || i_mday < 1 || i_mday > 31 ||
        i_hour < 0 || i_hour > 23 || i_minute < 0 || i_minute > 59 || i_second < 0 || i_second > 59 )
        return -1;

    /* Count the number of days */
    i_day = 365 * (i_year-70) + pn_day[i_month] + i_mday - 1;
#define LEAP(y) ( ((y)%4) == 0 && (((y)%100) != 0 || ((y)%400) == 0) ? 1 : 0)
    for( int i = 70; i < i_year; i++ )
        i_day += LEAP(1900+i);
    if( i_month > 1 )
        i_day += LEAP(1900+i_year);
#undef LEAP
    /**/
    return ((24*i_day + i_hour)*60 + i_minute)*60 + i_second;
}

static void EITDecodeMjd( int i_mjd, int *p_y, int *p_m, int *p_d )
{
    const int yp = (int)( ( (double)i_mjd - 15078.2)/365.25 );
    const int mp = (int)( ((double)i_mjd - 14956.1 - (int)(yp * 365.25)) / 30.6001 );
    const int c = ( mp == 14 || mp == 15 ) ? 1 : 0;

    *p_y = 1900 + yp + c*1;
    *p_m = mp - 1 - c*12;
    *p_d = i_mjd - 14956 - (int)(yp*365.25) - (int)(mp*30.6001);
}
#define CVT_FROM_BCD(v) ((((v) >> 4)&0xf)*10 + ((v)&0xf))
static int64_t EITConvertStartTime( uint64_t i_date )
{
    const int i_mjd = i_date >> 24;
    const int i_hour   = CVT_FROM_BCD(i_date >> 16);
    const int i_minute = CVT_FROM_BCD(i_date >>  8);
    const int i_second = CVT_FROM_BCD(i_date      );
    int i_year;
    int i_month;
    int i_day;

    /* if all 40 bits are 1, the start is unknown */
    if( i_date == UINT64_C(0xffffffffff) )
        return -1;

    EITDecodeMjd( i_mjd, &i_year, &i_month, &i_day );
    return vlc_timegm( i_year - 1900, i_month - 1, i_day, i_hour, i_minute, i_second );
}
static int EITConvertDuration( uint32_t i_duration )
{
    return CVT_FROM_BCD(i_duration >> 16) * 3600 +
           CVT_FROM_BCD(i_duration >> 8 ) * 60 +
           CVT_FROM_BCD(i_duration      );
}
#undef CVT_FROM_BCD

static void TDTCallBack( demux_t *p_demux, dvbpsi_tot_t *p_tdt )
{
    demux_sys_t        *p_sys = p_demux->p_sys;

    p_sys->i_tdt_delta = CLOCK_FREQ * EITConvertStartTime( p_tdt->i_utc_time )
                         - mdate();
    dvbpsi_tot_delete(p_tdt);
}


static void EITCallBack( demux_t *p_demux,
                         dvbpsi_eit_t *p_eit, bool b_current_following )
{
    demux_sys_t        *p_sys = p_demux->p_sys;
    dvbpsi_eit_event_t *p_evt;
    vlc_epg_t *p_epg;

    msg_Dbg( p_demux, "EITCallBack called" );
    if( !p_eit->b_current_next )
    {
        dvbpsi_eit_delete( p_eit );
        return;
    }

    msg_Dbg( p_demux, "new EIT service_id=%d version=%d current_next=%d "
             "ts_id=%d network_id=%d segment_last_section_number=%d "
             "last_table_id=%d",
             p_eit->i_extension,
             p_eit->i_version, p_eit->b_current_next,
             p_eit->i_ts_id, p_eit->i_network_id,
             p_eit->i_segment_last_section_number, p_eit->i_last_table_id );

    p_epg = vlc_epg_New( NULL );
    for( p_evt = p_eit->p_first_event; p_evt; p_evt = p_evt->p_next )
    {
        dvbpsi_descriptor_t *p_dr;
        char                *psz_name = NULL;
        char                *psz_text = NULL;
        char                *psz_extra = strdup("");
        int64_t i_start;
        int i_duration;
        int i_min_age = 0;
        int64_t i_tot_time = 0;

        i_start = EITConvertStartTime( p_evt->i_start_time );
        i_duration = EITConvertDuration( p_evt->i_duration );

        if( p_sys->arib.e_mode == ARIBMODE_ENABLED )
        {
            if( p_sys->i_tdt_delta == 0 )
                p_sys->i_tdt_delta = CLOCK_FREQ * (i_start + i_duration - 5) - mdate();

            i_tot_time = (mdate() + p_sys->i_tdt_delta) / CLOCK_FREQ;

            tzset(); // JST -> UTC
            i_start += timezone; // FIXME: what about DST?
            i_tot_time += timezone;

            if( p_evt->i_running_status == 0x00 &&
                (i_start - 5 < i_tot_time &&
                 i_tot_time < i_start + i_duration + 5) )
            {
                p_evt->i_running_status = 0x04;
                msg_Dbg( p_demux, "  EIT running status 0x00 -> 0x04" );
            }
        }

        msg_Dbg( p_demux, "  * event id=%d start_time:%d duration=%d "
                          "running=%d free_ca=%d",
                 p_evt->i_event_id, (int)i_start, (int)i_duration,
                 p_evt->i_running_status, p_evt->b_free_ca );

        for( p_dr = p_evt->p_first_descriptor; p_dr; p_dr = p_dr->p_next )
        {
            switch(p_dr->i_tag)
            {
            case 0x4d:
            {
                dvbpsi_short_event_dr_t *pE = dvbpsi_DecodeShortEventDr( p_dr );

                /* Only take first description, as we don't handle language-info
                   for epg atm*/
                if( pE && psz_name == NULL )
                {
                    psz_name = EITConvertToUTF8( p_demux,
                                                 pE->i_event_name, pE->i_event_name_length,
                                                 p_sys->b_broken_charset );
                    free( psz_text );
                    psz_text = EITConvertToUTF8( p_demux,
                                                 pE->i_text, pE->i_text_length,
                                                 p_sys->b_broken_charset );
                    msg_Dbg( p_demux, "    - short event lang=%3.3s '%s' : '%s'",
                             pE->i_iso_639_code, psz_name, psz_text );
                }
            }
                break;

            case 0x4e:
            {
                dvbpsi_extended_event_dr_t *pE = dvbpsi_DecodeExtendedEventDr( p_dr );
                if( pE )
                {
                    msg_Dbg( p_demux, "    - extended event lang=%3.3s [%d/%d]",
                             pE->i_iso_639_code,
                             pE->i_descriptor_number, pE->i_last_descriptor_number );

                    if( pE->i_text_length > 0 )
                    {
                        char *psz_text = EITConvertToUTF8( p_demux,
                                                           pE->i_text, pE->i_text_length,
                                                           p_sys->b_broken_charset );
                        if( psz_text )
                        {
                            msg_Dbg( p_demux, "       - text='%s'", psz_text );

                            psz_extra = xrealloc( psz_extra,
                                   strlen(psz_extra) + strlen(psz_text) + 1 );
                            strcat( psz_extra, psz_text );
                            free( psz_text );
                        }
                    }

                    for( int i = 0; i < pE->i_entry_count; i++ )
                    {
                        char *psz_dsc = EITConvertToUTF8( p_demux,
                                                          pE->i_item_description[i],
                                                          pE->i_item_description_length[i],
                                                          p_sys->b_broken_charset );
                        char *psz_itm = EITConvertToUTF8( p_demux,
                                                          pE->i_item[i], pE->i_item_length[i],
                                                          p_sys->b_broken_charset );

                        if( psz_dsc && psz_itm )
                        {
                            msg_Dbg( p_demux, "       - desc='%s' item='%s'", psz_dsc, psz_itm );
#if 0
                            psz_extra = xrealloc( psz_extra,
                                         strlen(psz_extra) + strlen(psz_dsc) +
                                         strlen(psz_itm) + 3 + 1 );
                            strcat( psz_extra, "(" );
                            strcat( psz_extra, psz_dsc );
                            strcat( psz_extra, " " );
                            strcat( psz_extra, psz_itm );
                            strcat( psz_extra, ")" );
#endif
                        }
                        free( psz_dsc );
                        free( psz_itm );
                    }
                }
            }
                break;

            case 0x55:
            {
                dvbpsi_parental_rating_dr_t *pR = dvbpsi_DecodeParentalRatingDr( p_dr );
                if ( pR )
                {
                    for ( int i = 0; i < pR->i_ratings_number; i++ )
                    {
                        const dvbpsi_parental_rating_t *p_rating = & pR->p_parental_rating[ i ];
                        if ( p_rating->i_rating > 0x00 && p_rating->i_rating <= 0x0F )
                        {
                            if ( p_rating->i_rating + 3 > i_min_age )
                                i_min_age = p_rating->i_rating + 3;
                            msg_Dbg( p_demux, "    - parental control set to %d years",
                                     i_min_age );
                        }
                    }
                }
            }
                break;

            default:
                msg_Dbg( p_demux, "    - event unknown dr 0x%x(%d)", p_dr->i_tag, p_dr->i_tag );
                break;
            }
        }

        /* */
        if( i_start > 0 && psz_name && psz_text)
            vlc_epg_AddEvent( p_epg, i_start, i_duration, psz_name, psz_text,
                              *psz_extra ? psz_extra : NULL, i_min_age );

        /* Update "now playing" field */
        if( p_evt->i_running_status == 0x04 && i_start > 0  && psz_name && psz_text )
            vlc_epg_SetCurrent( p_epg, i_start );

        free( psz_name );
        free( psz_text );

        free( psz_extra );
    }
    if( p_epg->i_event > 0 )
    {
        if( b_current_following &&
            (  p_sys->programs.i_size == 0 ||
               p_sys->programs.p_elems[0] ==
                    p_eit->i_extension
                ) )
        {
            p_sys->i_dvb_length = 0;
            p_sys->i_dvb_start = 0;

            if( p_epg->p_current )
            {
                p_sys->i_dvb_start = CLOCK_FREQ * p_epg->p_current->i_start;
                p_sys->i_dvb_length = CLOCK_FREQ * p_epg->p_current->i_duration;
            }
        }
        es_out_Control( p_demux->out, ES_OUT_SET_GROUP_EPG,
                        p_eit->i_extension,
                        p_epg );
    }
    vlc_epg_Delete( p_epg );

    dvbpsi_eit_delete( p_eit );
}
static void EITCallBackCurrentFollowing( demux_t *p_demux, dvbpsi_eit_t *p_eit )
{
    EITCallBack( p_demux, p_eit, true );
}
static void EITCallBackSchedule( demux_t *p_demux, dvbpsi_eit_t *p_eit )
{
    EITCallBack( p_demux, p_eit, false );
}

static void PSINewTableCallBack( dvbpsi_t *h, uint8_t i_table_id,
                                 uint16_t i_extension, demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    assert( h );
#if 0
    msg_Dbg( p_demux, "PSINewTableCallBack: table 0x%x(%d) ext=0x%x(%d)",
             i_table_id, i_table_id, i_extension, i_extension );
#endif
    if( GetPID(p_sys, 0)->u.p_pat->i_version != -1 && i_table_id == 0x42 )
    {
        msg_Dbg( p_demux, "PSINewTableCallBack: table 0x%x(%d) ext=0x%x(%d)",
                 i_table_id, i_table_id, i_extension, i_extension );

        if( !dvbpsi_sdt_attach( h, i_table_id, i_extension, (dvbpsi_sdt_callback)SDTCallBack, p_demux ) )
            msg_Err( p_demux, "PSINewTableCallback: failed attaching SDTCallback" );
    }
    else if( GetPID(p_sys, 0x11)->u.p_psi->i_version != -1 &&
             ( i_table_id == 0x4e || /* Current/Following */
               (i_table_id >= 0x50 && i_table_id <= 0x5f) ) ) /* Schedule */
    {
        msg_Dbg( p_demux, "PSINewTableCallBack: table 0x%x(%d) ext=0x%x(%d)",
                 i_table_id, i_table_id, i_extension, i_extension );

        dvbpsi_eit_callback cb = i_table_id == 0x4e ?
                                    (dvbpsi_eit_callback)EITCallBackCurrentFollowing :
                                    (dvbpsi_eit_callback)EITCallBackSchedule;

        if( !dvbpsi_eit_attach( h, i_table_id, i_extension, cb, p_demux ) )
            msg_Err( p_demux, "PSINewTableCallback: failed attaching EITCallback" );
    }
    else if( GetPID(p_sys, 0x11)->u.p_psi->i_version != -1 &&
            (i_table_id == 0x70 /* TDT */ || i_table_id == 0x73 /* TOT */) )
    {
         msg_Dbg( p_demux, "PSINewTableCallBack: table 0x%x(%d) ext=0x%x(%d)",
                 i_table_id, i_table_id, i_extension, i_extension );

        if( !dvbpsi_tot_attach( h, i_table_id, i_extension, (dvbpsi_tot_callback)TDTCallBack, p_demux ) )
            msg_Err( p_demux, "PSINewTableCallback: failed attaching TDTCallback" );
    }
}

/*****************************************************************************
 * PMT callback and helpers
 *****************************************************************************/
static dvbpsi_descriptor_t *PMTEsFindDescriptor( const dvbpsi_pmt_es_t *p_es,
                                                 int i_tag )
{
    dvbpsi_descriptor_t *p_dr = p_es->p_first_descriptor;;
    while( p_dr && ( p_dr->i_tag != i_tag ) )
        p_dr = p_dr->p_next;
    return p_dr;
}
static bool PMTEsHasRegistration( demux_t *p_demux,
                                  const dvbpsi_pmt_es_t *p_es,
                                  const char *psz_tag )
{
    dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_es, 0x05 );
    if( !p_dr )
        return false;

    if( p_dr->i_length < 4 )
    {
        msg_Warn( p_demux, "invalid Registration Descriptor" );
        return false;
    }

    assert( strlen(psz_tag) == 4 );
    return !memcmp( p_dr->p_data, psz_tag, 4 );
}

static bool PMTEsHasComponentTag( const dvbpsi_pmt_es_t *p_es,
                                  int i_component_tag )
{
    dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_es, 0x52 );
    if( !p_dr )
        return false;
    dvbpsi_stream_identifier_dr_t *p_si = dvbpsi_DecodeStreamIdentifierDr( p_dr );
    if( !p_si )
        return false;

    return p_si->i_component_tag == i_component_tag;
}

static const es_mpeg4_descriptor_t * GetMPEG4DescByEsId( const ts_pmt_t *pmt, uint16_t i_es_id )
{
    for( int i = 0; i < ES_DESCRIPTOR_COUNT; i++ )
    {
        const es_mpeg4_descriptor_t *es_descr = &pmt->iod->es_descr[i];
        if( es_descr->i_es_id == i_es_id && es_descr->b_ok )
            return es_descr;
    }
    for( int i=0; i<pmt->od.objects.i_size; i++ )
    {
        const od_descriptor_t *od = pmt->od.objects.p_elems[i];
        for( int j = 0; j < ES_DESCRIPTOR_COUNT; j++ )
        {
            const es_mpeg4_descriptor_t *es_descr = &od->es_descr[j];
            if( es_descr->i_es_id == i_es_id && es_descr->b_ok )
                return es_descr;
        }
    }
    return NULL;
}

static ts_pes_es_t * GetPMTESBySLEsId( ts_pmt_t *pmt, uint16_t i_sl_es_id )
{
    for( int i=0; i< pmt->e_streams.i_size; i++ )
    {
        ts_pes_es_t *p_es = &pmt->e_streams.p_elems[i]->u.p_pes->es;
        if( p_es->i_sl_es_id == i_sl_es_id )
            return p_es;
    }
    return NULL;
}

static bool SetupISO14496LogicalStream( demux_t *p_demux, const decoder_config_descriptor_t *dcd,
                                        es_format_t *p_fmt )
{
    msg_Dbg( p_demux, "     - IOD objecttype: %"PRIx8" streamtype:%"PRIx8,
             dcd->i_objectTypeIndication, dcd->i_streamType );

    if( dcd->i_streamType == 0x04 )    /* VisualStream */
    {
        p_fmt->i_cat = VIDEO_ES;
        switch( dcd->i_objectTypeIndication )
        {
        case 0x0B: /* mpeg4 sub */
            p_fmt->i_cat = SPU_ES;
            p_fmt->i_codec = VLC_CODEC_SUBT;
            break;

        case 0x20: /* mpeg4 */
            p_fmt->i_codec = VLC_CODEC_MP4V;
            break;
        case 0x21: /* h264 */
            p_fmt->i_codec = VLC_CODEC_H264;
            break;
        case 0x60:
        case 0x61:
        case 0x62:
        case 0x63:
        case 0x64:
        case 0x65: /* mpeg2 */
            p_fmt->i_codec = VLC_CODEC_MPGV;
            break;
        case 0x6a: /* mpeg1 */
            p_fmt->i_codec = VLC_CODEC_MPGV;
            break;
        case 0x6c: /* mpeg1 */
            p_fmt->i_codec = VLC_CODEC_JPEG;
            break;
        default:
            p_fmt->i_cat = UNKNOWN_ES;
            break;
        }
    }
    else if( dcd->i_streamType == 0x05 )    /* AudioStream */
    {
        p_fmt->i_cat = AUDIO_ES;
        switch( dcd->i_objectTypeIndication )
        {
        case 0x40: /* mpeg4 */
            p_fmt->i_codec = VLC_CODEC_MP4A;
            break;
        case 0x66:
        case 0x67:
        case 0x68: /* mpeg2 aac */
            p_fmt->i_codec = VLC_CODEC_MP4A;
            break;
        case 0x69: /* mpeg2 */
            p_fmt->i_codec = VLC_CODEC_MPGA;
            break;
        case 0x6b: /* mpeg1 */
            p_fmt->i_codec = VLC_CODEC_MPGA;
            break;
        default:
            p_fmt->i_cat = UNKNOWN_ES;
            break;
        }
    }
    else
    {
        p_fmt->i_cat = UNKNOWN_ES;
    }

    if( p_fmt->i_cat != UNKNOWN_ES )
    {
        p_fmt->i_extra = __MIN(dcd->i_extra, INT32_MAX);
        if( p_fmt->i_extra > 0 )
        {
            p_fmt->p_extra = malloc( p_fmt->i_extra );
            if( p_fmt->p_extra )
                memcpy( p_fmt->p_extra, dcd->p_extra, p_fmt->i_extra );
            else
                p_fmt->i_extra = 0;
        }
    }

    return true;
}

static void SetupISO14496Descriptors( demux_t *p_demux, ts_pes_es_t *p_es,
                                      const ts_pmt_t *p_pmt, const dvbpsi_pmt_es_t *p_dvbpsies )
{
    const dvbpsi_descriptor_t *p_dr = p_dvbpsies->p_first_descriptor;

    while( p_dr )
    {
        uint8_t i_length = p_dr->i_length;

        switch( p_dr->i_tag )
        {
            case 0x1f: /* FMC Descriptor */
                while( i_length >= 3 && !p_es->i_sl_es_id )
                {
                    p_es->i_sl_es_id = ( p_dr->p_data[0] << 8 ) | p_dr->p_data[1];
                    /* FIXME: map all ids and flexmux channels */
                    i_length -= 3;
                    msg_Dbg( p_demux, "     - found FMC_descriptor mapping es_id=%"PRIu16, p_es->i_sl_es_id );
                }
                break;
            case 0x1e: /* SL Descriptor */
                if( i_length == 2 )
                {
                    p_es->i_sl_es_id = ( p_dr->p_data[0] << 8 ) | p_dr->p_data[1];
                    msg_Dbg( p_demux, "     - found SL_descriptor mapping es_id=%"PRIu16, p_es->i_sl_es_id );
                }
                break;
            default:
                break;
        }

        p_dr = p_dr->p_next;
    }

    if( p_es->i_sl_es_id )
    {
        const es_mpeg4_descriptor_t *p_mpeg4desc = GetMPEG4DescByEsId( p_pmt, p_es->i_sl_es_id );
        if( p_mpeg4desc && p_mpeg4desc->b_ok )
        {
            if( !SetupISO14496LogicalStream( p_demux, &p_mpeg4desc->dec_descr, &p_es->fmt ) )
                msg_Dbg( p_demux, "     - IOD not yet available for es_id=%"PRIu16, p_es->i_sl_es_id );
        }
    }
    else
    {
        switch( p_dvbpsies->i_type )
        {
        /* non fatal, set by packetizer */
        case 0x0f: /* ADTS */
        case 0x11: /* LOAS */
            msg_Info( p_demux, "     - SL/FMC descriptor not found/matched" );
            break;
        default:
            msg_Err( p_demux, "      - SL/FMC descriptor not found/matched" );
            break;
        }
    }
}

typedef struct
{
    int  i_type;
    int  i_magazine;
    int  i_page;
    char p_iso639[3];
} ts_teletext_page_t;

static void PMTSetupEsTeletext( demux_t *p_demux, ts_pes_t *p_pes,
                                const dvbpsi_pmt_es_t *p_dvbpsies )
{
    es_format_t *p_fmt = &p_pes->es.fmt;

    ts_teletext_page_t p_page[2 * 64 + 20];
    unsigned i_page = 0;

    /* Gather pages information */
    for( unsigned i_tag_idx = 0; i_tag_idx < 2; i_tag_idx++ )
    {
        dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_dvbpsies, i_tag_idx == 0 ? 0x46 : 0x56 );
        if( !p_dr )
            continue;

        dvbpsi_teletext_dr_t *p_sub = dvbpsi_DecodeTeletextDr( p_dr );
        if( !p_sub )
            continue;

        for( int i = 0; i < p_sub->i_pages_number; i++ )
        {
            const dvbpsi_teletextpage_t *p_src = &p_sub->p_pages[i];

            if( p_src->i_teletext_type >= 0x06 )
                continue;

            assert( i_page < sizeof(p_page)/sizeof(*p_page) );

            ts_teletext_page_t *p_dst = &p_page[i_page++];

            p_dst->i_type = p_src->i_teletext_type;
            p_dst->i_magazine = p_src->i_teletext_magazine_number
                ? p_src->i_teletext_magazine_number : 8;
            p_dst->i_page = p_src->i_teletext_page_number;
            memcpy( p_dst->p_iso639, p_src->i_iso6392_language_code, 3 );
        }
    }

    dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_dvbpsies, 0x59 );
    if( p_dr )
    {
        dvbpsi_subtitling_dr_t *p_sub = dvbpsi_DecodeSubtitlingDr( p_dr );
        for( int i = 0; p_sub && i < p_sub->i_subtitles_number; i++ )
        {
            dvbpsi_subtitle_t *p_src = &p_sub->p_subtitle[i];

            if( p_src->i_subtitling_type < 0x01 || p_src->i_subtitling_type > 0x03 )
                continue;

            assert( i_page < sizeof(p_page)/sizeof(*p_page) );

            ts_teletext_page_t *p_dst = &p_page[i_page++];

            switch( p_src->i_subtitling_type )
            {
            case 0x01:
                p_dst->i_type = 0x02;
                break;
            default:
                p_dst->i_type = 0x03;
                break;
            }
            /* FIXME check if it is the right split */
            p_dst->i_magazine = (p_src->i_composition_page_id >> 8)
                ? (p_src->i_composition_page_id >> 8) : 8;
            p_dst->i_page = p_src->i_composition_page_id & 0xff;
            memcpy( p_dst->p_iso639, p_src->i_iso6392_language_code, 3 );
        }
    }

    /* */
    es_format_Init( p_fmt, SPU_ES, VLC_CODEC_TELETEXT );

    if( !p_demux->p_sys->b_split_es || i_page <= 0 )
    {
        p_fmt->subs.teletext.i_magazine = -1;
        p_fmt->subs.teletext.i_page = 0;
        p_fmt->psz_description = strdup( vlc_gettext(ppsz_teletext_type[1]) );

        dvbpsi_descriptor_t *p_dr;
        p_dr = PMTEsFindDescriptor( p_dvbpsies, 0x46 );
        if( !p_dr )
            p_dr = PMTEsFindDescriptor( p_dvbpsies, 0x56 );

        if( !p_demux->p_sys->b_split_es && p_dr && p_dr->i_length > 0 )
        {
            /* Descriptor pass-through */
            p_fmt->p_extra = malloc( p_dr->i_length );
            if( p_fmt->p_extra )
            {
                p_fmt->i_extra = p_dr->i_length;
                memcpy( p_fmt->p_extra, p_dr->p_data, p_dr->i_length );
            }
        }
    }
    else
    {
        for( unsigned i = 0; i < i_page; i++ )
        {
            ts_pes_es_t *p_page_es;

            /* */
            if( i == 0 )
            {
                p_page_es = &p_pes->es;
            }
            else
            {
                p_page_es = calloc( 1, sizeof(*p_page_es) );
                if( !p_page_es )
                    break;

                es_format_Copy( &p_page_es->fmt, p_fmt );
                free( p_page_es->fmt.psz_language );
                free( p_page_es->fmt.psz_description );
                p_page_es->fmt.psz_language = NULL;
                p_page_es->fmt.psz_description = NULL;

                ARRAY_APPEND( p_pes->extra_es, p_page_es );
            }

            /* */
            const ts_teletext_page_t *p = &p_page[i];
            p_page_es->fmt.i_priority = (p->i_type == 0x02 || p->i_type == 0x05) ?
                      ES_PRIORITY_SELECTABLE_MIN : ES_PRIORITY_NOT_DEFAULTABLE;
            p_page_es->fmt.psz_language = strndup( p->p_iso639, 3 );
            p_page_es->fmt.psz_description = strdup(vlc_gettext(ppsz_teletext_type[p->i_type]));
            p_page_es->fmt.subs.teletext.i_magazine = p->i_magazine;
            p_page_es->fmt.subs.teletext.i_page = p->i_page;

            msg_Dbg( p_demux,
                         "    * ttxt type=%s lan=%s page=%d%02x",
                         p_page_es->fmt.psz_description,
                         p_page_es->fmt.psz_language,
                         p->i_magazine, p->i_page );
        }
    }
}
static void PMTSetupEsDvbSubtitle( demux_t *p_demux, ts_pes_t *p_pes,
                                   const dvbpsi_pmt_es_t *p_dvbpsies )
{
    es_format_t *p_fmt = &p_pes->es.fmt;

    es_format_Init( p_fmt, SPU_ES, VLC_CODEC_DVBS );

    dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_dvbpsies, 0x59 );
    int i_page = 0;
    dvbpsi_subtitling_dr_t *p_sub = dvbpsi_DecodeSubtitlingDr( p_dr );
    for( int i = 0; p_sub && i < p_sub->i_subtitles_number; i++ )
    {
        const int i_type = p_sub->p_subtitle[i].i_subtitling_type;
        if( ( i_type >= 0x10 && i_type <= 0x14 ) ||
            ( i_type >= 0x20 && i_type <= 0x24 ) )
            i_page++;
    }

    if( !p_demux->p_sys->b_split_es  || i_page <= 0 )
    {
        p_fmt->subs.dvb.i_id = -1;
        p_fmt->psz_description = strdup( _("DVB subtitles") );

        if( !p_demux->p_sys->b_split_es && p_dr && p_dr->i_length > 0 )
        {
            /* Descriptor pass-through */
            p_fmt->p_extra = malloc( p_dr->i_length );
            if( p_fmt->p_extra )
            {
                p_fmt->i_extra = p_dr->i_length;
                memcpy( p_fmt->p_extra, p_dr->p_data, p_dr->i_length );
            }
        }
    }
    else
    {
        for( int i = 0; i < p_sub->i_subtitles_number; i++ )
        {
            ts_pes_es_t *p_subs_es;

            /* */
            if( i == 0 )
            {
                p_subs_es = &p_pes->es;
            }
            else
            {
                p_subs_es = malloc( sizeof(*p_subs_es) );
                if( !p_subs_es )
                    break;

                es_format_Copy( &p_subs_es->fmt, p_fmt );
                free( p_subs_es->fmt.psz_language );
                free( p_subs_es->fmt.psz_description );
                p_subs_es->fmt.psz_language = NULL;
                p_subs_es->fmt.psz_description = NULL;

                ARRAY_APPEND( p_pes->extra_es, p_subs_es );
            }

            /* */
            const dvbpsi_subtitle_t *p = &p_sub->p_subtitle[i];
            p_subs_es->fmt.psz_language = strndup( (char *)p->i_iso6392_language_code, 3 );
            switch( p->i_subtitling_type )
            {
            case 0x10: /* unspec. */
            case 0x11: /* 4:3 */
            case 0x12: /* 16:9 */
            case 0x13: /* 2.21:1 */
            case 0x14: /* HD monitor */
                p_subs_es->fmt.psz_description = strdup( _("DVB subtitles") );
                break;
            case 0x20: /* Hearing impaired unspec. */
            case 0x21: /* h.i. 4:3 */
            case 0x22: /* h.i. 16:9 */
            case 0x23: /* h.i. 2.21:1 */
            case 0x24: /* h.i. HD monitor */
                p_subs_es->fmt.psz_description = strdup( _("DVB subtitles: hearing impaired") );
                break;
            default:
                break;
            }

            /* Hack, FIXME */
            p_subs_es->fmt.subs.dvb.i_id = ( p->i_composition_page_id <<  0 ) |
                                      ( p->i_ancillary_page_id   << 16 );
        }
    }
}

static int vlc_ceil_log2( const unsigned int val )
{
    int n = 31 - clz(val);
    if ((1U << n) != val)
        n++;

    return n;
}

static void OpusSetup(demux_t *demux, uint8_t *p, size_t len, es_format_t *p_fmt)
{
    OpusHeader h;

    /* default mapping */
    static const unsigned char map[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    memcpy(h.stream_map, map, sizeof(map));

    int csc, mapping;
    int channels = 0;
    int stream_count = 0;
    int ccc = p[1]; // channel_config_code
    if (ccc <= 8) {
        channels = ccc;
        if (channels)
            mapping = channels > 2;
        else {
            mapping = 255;
            channels = 2; // dual mono
        }
        static const uint8_t p_csc[8] = { 0, 1, 1, 2, 2, 2, 3, 3 };
        csc = p_csc[channels - 1];
        stream_count = channels - csc;

        static const uint8_t map[6][7] = {
            { 2,1 },
            { 1,2,3 },
            { 4,1,2,3 },
            { 4,1,2,3,5 },
            { 4,1,2,3,5,6 },
            { 6,1,2,3,4,5,7 },
        };
        if (channels > 2)
            memcpy(&h.stream_map[1], map[channels-3], channels - 1);
    } else if (ccc == 0x81) {
        if (len < 4)
            goto explicit_config_too_short;

        channels = p[2];
        mapping = p[3];
        csc = 0;
        if (mapping) {
            bs_t s;
            bs_init(&s, &p[4], len - 4);
            stream_count = 1;
            if (channels) {
                int bits = vlc_ceil_log2(channels);
                if (s.i_left < bits)
                    goto explicit_config_too_short;
                stream_count = bs_read(&s, bits) + 1;
                bits = vlc_ceil_log2(stream_count + 1);
                if (s.i_left < bits)
                    goto explicit_config_too_short;
                csc = bs_read(&s, bits);
            }
            int channel_bits = vlc_ceil_log2(stream_count + csc + 1);
            if (s.i_left < channels * channel_bits)
                goto explicit_config_too_short;

            unsigned char silence = (1U << (stream_count + csc + 1)) - 1;
            for (int i = 0; i < channels; i++) {
                unsigned char m = bs_read(&s, channel_bits);
                if (m == silence)
                    m = 0xff;
                h.stream_map[i] = m;
            }
        }
    } else if (ccc >= 0x80 && ccc <= 0x88) {
        channels = ccc - 0x80;
        if (channels)
            mapping = 1;
        else {
            mapping = 255;
            channels = 2; // dual mono
        }
        csc = 0;
        stream_count = channels;
    } else {
        msg_Err(demux, "Opus channel configuration 0x%.2x is reserved", ccc);
    }

    if (!channels) {
        msg_Err(demux, "Opus channel configuration 0x%.2x not supported yet", p[1]);
        return;
    }

    opus_prepare_header(channels, 0, &h);
    h.preskip = 0;
    h.input_sample_rate = 48000;
    h.nb_coupled = csc;
    h.nb_streams = channels - csc;
    h.channel_mapping = mapping;

    if (h.channels) {
        opus_write_header((uint8_t**)&p_fmt->p_extra, &p_fmt->i_extra, &h, NULL /* FIXME */);
        if (p_fmt->p_extra) {
            p_fmt->i_cat = AUDIO_ES;
            p_fmt->i_codec = VLC_CODEC_OPUS;
            p_fmt->audio.i_channels = h.channels;
            p_fmt->audio.i_rate = 48000;
        }
    }

    return;

explicit_config_too_short:
    msg_Err(demux, "Opus descriptor too short");
}

static void PMTSetupEs0x06( demux_t *p_demux, ts_pes_t *p_pes,
                            const dvbpsi_pmt_es_t *p_dvbpsies )
{
    es_format_t *p_fmt = &p_pes->es.fmt;
    dvbpsi_descriptor_t *p_subs_dr = PMTEsFindDescriptor( p_dvbpsies, 0x59 );
    dvbpsi_descriptor_t *desc;

    if( PMTEsHasRegistration( p_demux, p_dvbpsies, "AC-3" ) ||
        PMTEsFindDescriptor( p_dvbpsies, 0x6a ) ||
        PMTEsFindDescriptor( p_dvbpsies, 0x81 ) )
    {
        p_fmt->i_cat = AUDIO_ES;
        p_fmt->i_codec = VLC_CODEC_A52;
    }
    else if( (desc = PMTEsFindDescriptor( p_dvbpsies, 0x7f ) ) && desc->i_length >= 2 &&
              PMTEsHasRegistration(p_demux, p_dvbpsies, "Opus"))
    {
        OpusSetup(p_demux, desc->p_data, desc->i_length, p_fmt);
    }
    else if( PMTEsFindDescriptor( p_dvbpsies, 0x7a ) )
    {
        /* DVB with stream_type 0x06 (ETS EN 300 468) */
        p_fmt->i_cat = AUDIO_ES;
        p_fmt->i_codec = VLC_CODEC_EAC3;
    }
    else if( PMTEsHasRegistration( p_demux, p_dvbpsies, "DTS1" ) ||
             PMTEsHasRegistration( p_demux, p_dvbpsies, "DTS2" ) ||
             PMTEsHasRegistration( p_demux, p_dvbpsies, "DTS3" ) ||
             PMTEsFindDescriptor( p_dvbpsies, 0x73 ) )
    {
        /*registration descriptor(ETSI TS 101 154 Annex F)*/
        p_fmt->i_cat = AUDIO_ES;
        p_fmt->i_codec = VLC_CODEC_DTS;
    }
    else if( PMTEsHasRegistration( p_demux, p_dvbpsies, "BSSD" ) && !p_subs_dr )
    {
        /* BSSD is AES3 DATA, but could also be subtitles
         * we need to check for secondary descriptor then s*/
        p_fmt->i_cat = AUDIO_ES;
        p_fmt->b_packetized = true;
        p_fmt->i_codec = VLC_CODEC_302M;
    }
    else if( PMTEsHasRegistration( p_demux, p_dvbpsies, "HEVC" ) )
    {
        p_fmt->i_cat = VIDEO_ES;
        p_fmt->i_codec = VLC_CODEC_HEVC;
    }
    else if ( p_demux->p_sys->arib.e_mode == ARIBMODE_ENABLED )
    {
        /* Lookup our data component descriptor first ARIB STD B10 6.4 */
        dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_dvbpsies, 0xFD );
        /* and check that it maps to something ARIB STD B14 Table 5.1/5.2 */
        if ( p_dr && p_dr->i_length >= 2 )
        {
            if( !memcmp( p_dr->p_data, "\x00\x08", 2 ) &&  (
                    PMTEsHasComponentTag( p_dvbpsies, 0x30 ) ||
                    PMTEsHasComponentTag( p_dvbpsies, 0x31 ) ||
                    PMTEsHasComponentTag( p_dvbpsies, 0x32 ) ||
                    PMTEsHasComponentTag( p_dvbpsies, 0x33 ) ||
                    PMTEsHasComponentTag( p_dvbpsies, 0x34 ) ||
                    PMTEsHasComponentTag( p_dvbpsies, 0x35 ) ||
                    PMTEsHasComponentTag( p_dvbpsies, 0x36 ) ||
                    PMTEsHasComponentTag( p_dvbpsies, 0x37 ) ) )
            {
                es_format_Init( p_fmt, SPU_ES, VLC_CODEC_ARIB_A );
                p_fmt->psz_language = strndup ( "jpn", 3 );
                p_fmt->psz_description = strdup( _("ARIB subtitles") );
            }
            else if( !memcmp( p_dr->p_data, "\x00\x12", 2 ) && (
                     PMTEsHasComponentTag( p_dvbpsies, 0x87 ) ||
                     PMTEsHasComponentTag( p_dvbpsies, 0x88 ) ) )
            {
                es_format_Init( p_fmt, SPU_ES, VLC_CODEC_ARIB_C );
                p_fmt->psz_language = strndup ( "jpn", 3 );
                p_fmt->psz_description = strdup( _("ARIB subtitles") );
            }
        }
    }
    else
    {
        /* Subtitle/Teletext/VBI fallbacks */
        dvbpsi_subtitling_dr_t *p_sub;
        if( p_subs_dr && ( p_sub = dvbpsi_DecodeSubtitlingDr( p_subs_dr ) ) )
        {
            for( int i = 0; i < p_sub->i_subtitles_number; i++ )
            {
                if( p_fmt->i_cat != UNKNOWN_ES )
                    break;

                switch( p_sub->p_subtitle[i].i_subtitling_type )
                {
                case 0x01: /* EBU Teletext subtitles */
                case 0x02: /* Associated EBU Teletext */
                case 0x03: /* VBI data */
                    PMTSetupEsTeletext( p_demux, p_pes, p_dvbpsies );
                    break;
                case 0x10: /* DVB Subtitle (normal) with no monitor AR critical */
                case 0x11: /*                 ...   on 4:3 AR monitor */
                case 0x12: /*                 ...   on 16:9 AR monitor */
                case 0x13: /*                 ...   on 2.21:1 AR monitor */
                case 0x14: /*                 ...   for display on a high definition monitor */
                case 0x20: /* DVB Subtitle (impaired) with no monitor AR critical */
                case 0x21: /*                 ...   on 4:3 AR monitor */
                case 0x22: /*                 ...   on 16:9 AR monitor */
                case 0x23: /*                 ...   on 2.21:1 AR monitor */
                case 0x24: /*                 ...   for display on a high definition monitor */
                    PMTSetupEsDvbSubtitle( p_demux, p_pes, p_dvbpsies );
                    break;
                default:
                    msg_Err( p_demux, "Unrecognized DVB subtitle type (0x%x)",
                             p_sub->p_subtitle[i].i_subtitling_type );
                    break;
                }
            }
        }

        if( p_fmt->i_cat == UNKNOWN_ES &&
            ( PMTEsFindDescriptor( p_dvbpsies, 0x45 ) ||  /* VBI Data descriptor */
              PMTEsFindDescriptor( p_dvbpsies, 0x46 ) ||  /* VBI Teletext descriptor */
              PMTEsFindDescriptor( p_dvbpsies, 0x56 ) ) ) /* EBU Teletext descriptor */
        {
            /* Teletext/VBI */
            PMTSetupEsTeletext( p_demux, p_pes, p_dvbpsies );
        }
    }

    /* FIXME is it useful ? */
    if( PMTEsFindDescriptor( p_dvbpsies, 0x52 ) )
    {
        dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_dvbpsies, 0x52 );
        dvbpsi_stream_identifier_dr_t *p_si = dvbpsi_DecodeStreamIdentifierDr( p_dr );

        msg_Dbg( p_demux, "    * Stream Component Identifier: %d", p_si->i_component_tag );
    }
}

static void PMTSetupEs0xEA( demux_t *p_demux, ts_pes_es_t *p_es,
                           const dvbpsi_pmt_es_t *p_dvbpsies )
{
    /* Registration Descriptor */
    if( !PMTEsHasRegistration( p_demux, p_dvbpsies, "VC-1" ) )
    {
        msg_Err( p_demux, "Registration descriptor not found or invalid" );
        return;
    }

    es_format_t *p_fmt = &p_es->fmt;

    /* registration descriptor for VC-1 (SMPTE rp227) */
    p_fmt->i_cat = VIDEO_ES;
    p_fmt->i_codec = VLC_CODEC_VC1;

    /* XXX With Simple and Main profile the SEQUENCE
     * header is modified: video width and height are
     * inserted just after the start code as 2 int16_t
     * The packetizer will take care of that. */
}

static void PMTSetupEs0xD1( demux_t *p_demux, ts_pes_es_t *p_es,
                           const dvbpsi_pmt_es_t *p_dvbpsies )
{
    /* Registration Descriptor */
    if( !PMTEsHasRegistration( p_demux, p_dvbpsies, "drac" ) )
    {
        msg_Err( p_demux, "Registration descriptor not found or invalid" );
        return;
    }

    es_format_t *p_fmt = &p_es->fmt;

    /* registration descriptor for Dirac
     * (backwards compatable with VC-2 (SMPTE Sxxxx:2008)) */
    p_fmt->i_cat = VIDEO_ES;
    p_fmt->i_codec = VLC_CODEC_DIRAC;
}

static void PMTSetupEs0xA0( demux_t *p_demux, ts_pes_es_t *p_es,
                           const dvbpsi_pmt_es_t *p_dvbpsies )
{
    /* MSCODEC sent by vlc */
    dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_dvbpsies, 0xa0 );
    if( !p_dr || p_dr->i_length < 10 )
    {
        msg_Warn( p_demux,
                  "private MSCODEC (vlc) without bih private descriptor" );
        return;
    }

    es_format_t *p_fmt = &p_es->fmt;
    p_fmt->i_cat = VIDEO_ES;
    p_fmt->i_codec = VLC_FOURCC( p_dr->p_data[0], p_dr->p_data[1],
                                 p_dr->p_data[2], p_dr->p_data[3] );
    p_fmt->video.i_width = GetWBE( &p_dr->p_data[4] );
    p_fmt->video.i_height = GetWBE( &p_dr->p_data[6] );
    p_fmt->video.i_visible_width = p_fmt->video.i_width;
    p_fmt->video.i_visible_height = p_fmt->video.i_height;
    p_fmt->i_extra = GetWBE( &p_dr->p_data[8] );

    if( p_fmt->i_extra > 0 )
    {
        p_fmt->p_extra = malloc( p_fmt->i_extra );
        if( p_fmt->p_extra )
            memcpy( p_fmt->p_extra, &p_dr->p_data[10],
                    __MIN( p_fmt->i_extra, p_dr->i_length - 10 ) );
        else
            p_fmt->i_extra = 0;
    }
    /* For such stream we will gather them ourself and don't launch a
     * packetizer.
     * Yes it's ugly but it's the only way to have DIV3 working */
    p_fmt->b_packetized = true;
}

static void PMTSetupEs0x83( const dvbpsi_pmt_t *p_pmt, ts_pes_es_t *p_es, int i_pid )
{
    /* WiDi broadcasts without registration on PMT 0x1, PCR 0x1000 and
     * with audio track pid being 0x1100..0x11FF */
    if ( p_pmt->i_program_number == 0x1 &&
         p_pmt->i_pcr_pid == 0x1000 &&
        ( i_pid >> 8 ) == 0x11 )
    {
        /* Not enough ? might contain 0x83 private descriptor, 2 bytes 0x473F */
        es_format_Init( &p_es->fmt, AUDIO_ES, VLC_CODEC_WIDI_LPCM );
    }
    else
        es_format_Init( &p_es->fmt, AUDIO_ES, VLC_CODEC_DVD_LPCM );
}

static bool PMTSetupEsHDMV( demux_t *p_demux, ts_pes_es_t *p_es,
                            const dvbpsi_pmt_es_t *p_dvbpsies )
{
    es_format_t *p_fmt = &p_es->fmt;

    /* Blu-Ray mapping */
    switch( p_dvbpsies->i_type )
    {
    case 0x80:
        p_fmt->i_cat = AUDIO_ES;
        p_fmt->i_codec = VLC_CODEC_BD_LPCM;
        break;
    case 0x81:
        p_fmt->i_cat = AUDIO_ES;
        p_fmt->i_codec = VLC_CODEC_A52;
        break;
    case 0x82:
    case 0x85: /* DTS-HD High resolution audio */
    case 0x86: /* DTS-HD Master audio */
    case 0xA2: /* Secondary DTS audio */
        p_fmt->i_cat = AUDIO_ES;
        p_fmt->i_codec = VLC_CODEC_DTS;
        break;

    case 0x83: /* TrueHD AC3 */
        p_fmt->i_cat = AUDIO_ES;
        p_fmt->i_codec = VLC_CODEC_TRUEHD;
        break;

    case 0x84: /* E-AC3 */
    case 0xA1: /* Secondary E-AC3 */
        p_fmt->i_cat = AUDIO_ES;
        p_fmt->i_codec = VLC_CODEC_EAC3;
        break;
    case 0x90: /* Presentation graphics */
        p_fmt->i_cat = SPU_ES;
        p_fmt->i_codec = VLC_CODEC_BD_PG;
        break;
    case 0x91: /* Interactive graphics */
    case 0x92: /* Subtitle */
        return false;
    case 0xEA:
        p_fmt->i_cat = VIDEO_ES;
        p_fmt->i_codec = VLC_CODEC_VC1;
        break;
    default:
        msg_Info( p_demux, "HDMV registration not implemented for pid 0x%x type 0x%x",
                  p_dvbpsies->i_pid, p_dvbpsies->i_type );
        return false;
        break;
    }
    return true;
}

static bool PMTSetupEsRegistration( demux_t *p_demux, ts_pes_es_t *p_es,
                                    const dvbpsi_pmt_es_t *p_dvbpsies )
{
    static const struct
    {
        char         psz_tag[5];
        int          i_cat;
        vlc_fourcc_t i_codec;
    } p_regs[] = {
        { "AC-3", AUDIO_ES, VLC_CODEC_A52   },
        { "DTS1", AUDIO_ES, VLC_CODEC_DTS   },
        { "DTS2", AUDIO_ES, VLC_CODEC_DTS   },
        { "DTS3", AUDIO_ES, VLC_CODEC_DTS   },
        { "BSSD", AUDIO_ES, VLC_CODEC_302M  },
        { "VC-1", VIDEO_ES, VLC_CODEC_VC1   },
        { "drac", VIDEO_ES, VLC_CODEC_DIRAC },
        { "", UNKNOWN_ES, 0 }
    };
    es_format_t *p_fmt = &p_es->fmt;

    for( int i = 0; p_regs[i].i_cat != UNKNOWN_ES; i++ )
    {
        if( PMTEsHasRegistration( p_demux, p_dvbpsies, p_regs[i].psz_tag ) )
        {
            p_fmt->i_cat   = p_regs[i].i_cat;
            p_fmt->i_codec = p_regs[i].i_codec;
            if (p_dvbpsies->i_type == 0x87)
                p_fmt->i_codec = VLC_CODEC_EAC3;
            return true;
        }
    }
    return false;
}

static char *GetAudioTypeDesc(demux_t *p_demux, int type)
{
    static const char *audio_type[] = {
        NULL,
        N_("clean effects"),
        N_("hearing impaired"),
        N_("visual impaired commentary"),
    };

    if (type < 0 || type > 3)
        msg_Dbg( p_demux, "unknown audio type: %d", type);
    else if (type > 0)
        return strdup(audio_type[type]);

    return NULL;
}
static void PMTParseEsIso639( demux_t *p_demux, ts_pes_es_t *p_es,
                              const dvbpsi_pmt_es_t *p_dvbpsies )
{
    /* get language descriptor */
    dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_dvbpsies, 0x0a );

    if( !p_dr )
        return;

    dvbpsi_iso639_dr_t *p_decoded = dvbpsi_DecodeISO639Dr( p_dr );
    if( !p_decoded )
    {
        msg_Err( p_demux, "Failed to decode a ISO 639 descriptor" );
        return;
    }

#if defined(DR_0A_API_VER) && (DR_0A_API_VER >= 2)
    p_es->fmt.psz_language = malloc( 4 );
    if( p_es->fmt.psz_language )
    {
        memcpy( p_es->fmt.psz_language, p_decoded->code[0].iso_639_code, 3 );
        p_es->fmt.psz_language[3] = 0;
        msg_Dbg( p_demux, "found language: %s", p_es->fmt.psz_language);
    }
    int type = p_decoded->code[0].i_audio_type;
    p_es->fmt.psz_description = GetAudioTypeDesc(p_demux, type);
    if (type == 0)
        p_es->fmt.i_priority = ES_PRIORITY_SELECTABLE_MIN + 1; // prioritize normal audio tracks

    p_es->fmt.i_extra_languages = p_decoded->i_code_count-1;
    if( p_es->fmt.i_extra_languages > 0 )
        p_es->fmt.p_extra_languages =
            malloc( sizeof(*p_es->fmt.p_extra_languages) *
                    p_es->fmt.i_extra_languages );
    if( p_es->fmt.p_extra_languages )
    {
        for( unsigned i = 0; i < p_es->fmt.i_extra_languages; i++ )
        {
            p_es->fmt.p_extra_languages[i].psz_language = malloc(4);
            if( p_es->fmt.p_extra_languages[i].psz_language )
            {
                memcpy( p_es->fmt.p_extra_languages[i].psz_language,
                    p_decoded->code[i+1].iso_639_code, 3 );
                p_es->fmt.p_extra_languages[i].psz_language[3] = '\0';
            }
            int type = p_decoded->code[i].i_audio_type;
            p_es->fmt.p_extra_languages[i].psz_description = GetAudioTypeDesc(p_demux, type);
        }
    }
#else
    p_es->fmt.psz_language = malloc( 4 );
    if( p_es->fmt.psz_language )
    {
        memcpy( p_es->fmt.psz_language,
                p_decoded->i_iso_639_code, 3 );
        p_es->fmt.psz_language[3] = 0;
    }
#endif
}

static inline void SetExtraESGroupAndID( demux_sys_t *p_sys, es_format_t *p_fmt,
                                         const es_format_t *p_parent_fmt )
{
    if ( p_sys->b_es_id_pid ) /* pid is 13 bits */
        p_fmt->i_id = (p_sys->i_next_extraid++ << 13) | p_parent_fmt->i_id;
    p_fmt->i_group = p_parent_fmt->i_group;
}

static void AddAndCreateES( demux_t *p_demux, ts_pid_t *pid, bool b_create_delayed )
{
    demux_sys_t  *p_sys = p_demux->p_sys;

    if( b_create_delayed )
        p_sys->es_creation = CREATE_ES;

    if( pid && p_sys->es_creation == CREATE_ES )
    {
        /* FIXME: other owners / shared pid */
        pid->u.p_pes->es.id = es_out_Add( p_demux->out, &pid->u.p_pes->es.fmt );
        for( int i = 0; i < pid->u.p_pes->extra_es.i_size; i++ )
        {
            es_format_t *p_fmt = &pid->u.p_pes->extra_es.p_elems[i]->fmt;
            SetExtraESGroupAndID( p_sys, p_fmt, &pid->u.p_pes->es.fmt );
            pid->u.p_pes->extra_es.p_elems[i]->id = es_out_Add( p_demux->out, p_fmt );
        }
        p_sys->i_pmt_es += 1 + pid->u.p_pes->extra_es.i_size;

        /* Update the default program == first created ES group */
        if( p_sys->b_default_selection )
        {
            p_sys->b_default_selection = false;
            assert(p_sys->programs.i_size == 1);
            if( p_sys->programs.p_elems[0] != pid->p_parent->u.p_pmt->i_number )
                p_sys->programs.p_elems[0] = pid->p_parent->u.p_pmt->i_number;
            msg_Dbg( p_demux, "Default program is %d", pid->p_parent->u.p_pmt->i_number );
        }
    }

    if( b_create_delayed )
    {
        ts_pat_t *p_pat = GetPID(p_sys, 0)->u.p_pat;
        for( int i=0; i< p_pat->programs.i_size; i++ )
        {
            ts_pmt_t *p_pmt = p_pat->programs.p_elems[i]->u.p_pmt;
            for( int j=0; j<p_pmt->e_streams.i_size; j++ )
            {
                ts_pid_t *pid = p_pmt->e_streams.p_elems[j];
                if( pid->u.p_pes->es.id )
                    continue;

                pid->u.p_pes->es.id = es_out_Add( p_demux->out, &pid->u.p_pes->es.fmt );
                for( int k = 0; k < pid->u.p_pes->extra_es.i_size; k++ )
                {
                    es_format_t *p_fmt = &pid->u.p_pes->extra_es.p_elems[k]->fmt;
                    SetExtraESGroupAndID( p_sys, p_fmt, &pid->u.p_pes->es.fmt );
                    pid->u.p_pes->extra_es.p_elems[k]->id = es_out_Add( p_demux->out, p_fmt );
                }
                p_sys->i_pmt_es += 1 + pid->u.p_pes->extra_es.i_size;
            }
        }
    }

    UpdatePESFilters( p_demux, p_sys->b_es_all );
}

static void PMTCallBack( void *data, dvbpsi_pmt_t *p_dvbpsipmt )
{
    demux_t      *p_demux = data;
    demux_sys_t  *p_sys = p_demux->p_sys;

    ts_pid_t     *pmtpid = NULL;
    ts_pmt_t     *p_pmt = NULL;

    msg_Dbg( p_demux, "PMTCallBack called" );

    if (unlikely(GetPID(p_sys, 0)->type != TYPE_PAT))
    {
        assert(GetPID(p_sys, 0)->type == TYPE_PAT);
        dvbpsi_pmt_delete(p_dvbpsipmt);
    }

    const ts_pat_t *p_pat = GetPID(p_sys, 0)->u.p_pat;

    /* First find this PMT declared in PAT */
    for( int i = 0; !pmtpid && i < p_pat->programs.i_size; i++ )
    {
        const int i_pmt_prgnumber = p_pat->programs.p_elems[i]->u.p_pmt->i_number;
        if( i_pmt_prgnumber != TS_USER_PMT_NUMBER &&
            i_pmt_prgnumber == p_dvbpsipmt->i_program_number )
        {
            pmtpid = p_pat->programs.p_elems[i];
            assert(pmtpid->type == TYPE_PMT);
            p_pmt = pmtpid->u.p_pmt;
        }
    }

    if( pmtpid == NULL )
    {
        msg_Warn( p_demux, "unreferenced program (broken stream)" );
        dvbpsi_pmt_delete(p_dvbpsipmt);
        return;
    }

    pmtpid->i_flags |= FLAG_SEEN;

    if( p_pmt->i_version != -1 &&
        ( !p_dvbpsipmt->b_current_next || p_pmt->i_version == p_dvbpsipmt->i_version ) )
    {
        dvbpsi_pmt_delete( p_dvbpsipmt );
        return;
    }

    /* Save old es array */
    DECL_ARRAY(ts_pid_t *) old_es_rm;
    old_es_rm.i_alloc = p_pmt->e_streams.i_alloc;
    old_es_rm.i_size = p_pmt->e_streams.i_size;
    old_es_rm.p_elems = p_pmt->e_streams.p_elems;
    ARRAY_INIT(p_pmt->e_streams);

    if( p_pmt->iod )
    {
        ODFree( p_pmt->iod );
        p_pmt->iod = NULL;
    }

    msg_Dbg( p_demux, "new PMT program number=%d version=%d pid_pcr=%d",
             p_dvbpsipmt->i_program_number, p_dvbpsipmt->i_version, p_dvbpsipmt->i_pcr_pid );
    p_pmt->i_pid_pcr = p_dvbpsipmt->i_pcr_pid;
    p_pmt->i_version = p_dvbpsipmt->i_version;

    ValidateDVBMeta( p_demux, p_pmt->i_pid_pcr );

    if( ProgramIsSelected( p_sys, p_pmt->i_number ) )
        SetPIDFilter( p_sys, GetPID(p_sys, p_pmt->i_pid_pcr), true ); /* Set demux filter */

    /* Parse PMT descriptors */
    ts_pmt_registration_type_t registration_type = TS_PMT_REGISTRATION_NONE;
    dvbpsi_descriptor_t  *p_dr;

    /* First pass for standard detection */
    if ( p_sys->arib.e_mode == ARIBMODE_AUTO )
    {
        int i_arib_flags = 0; /* Descriptors can be repeated */
        for( p_dr = p_dvbpsipmt->p_first_descriptor; p_dr != NULL; p_dr = p_dr->p_next )
        {
            switch(p_dr->i_tag)
            {
            case 0x09:
            {
                dvbpsi_ca_dr_t *p_cadr = dvbpsi_DecodeCADr( p_dr );
                i_arib_flags |= (p_cadr->i_ca_system_id == 0x05);
            }
                break;
            case 0xF6:
                i_arib_flags |= 1 << 1;
                break;
            case 0xC1:
                i_arib_flags |= 1 << 2;
                break;
            default:
                break;
            }
        }
        if ( i_arib_flags == 0x07 ) //0b111
            p_sys->arib.e_mode = ARIBMODE_ENABLED;
    }

    for( p_dr = p_dvbpsipmt->p_first_descriptor; p_dr != NULL; p_dr = p_dr->p_next )
    {
        /* special descriptors handling */
        switch(p_dr->i_tag)
        {
        case 0x1d: /* We have found an IOD descriptor */
            msg_Dbg( p_demux, " * PMT descriptor : IOD (0x1d)" );
            p_pmt->iod = IODNew( VLC_OBJECT(p_demux), p_dr->i_length, p_dr->p_data );
            break;

        case 0x9:
            msg_Dbg( p_demux, " * PMT descriptor : CA (0x9) SysID 0x%x",
                    (p_dr->p_data[0] << 8) | p_dr->p_data[1] );
            break;

        case 0x5: /* Registration Descriptor */
            if( p_dr->i_length != 4 )
            {
                msg_Warn( p_demux, " * PMT invalid Registration Descriptor" );
            }
            else
            {
                msg_Dbg( p_demux, " * PMT descriptor : registration %4.4s", p_dr->p_data );
                if( !memcmp( p_dr->p_data, "HDMV", 4 ) || !memcmp( p_dr->p_data, "HDPR", 4 ) )
                    registration_type = TS_PMT_REGISTRATION_HDMV; /* Blu-Ray */
            }
            break;

        case 0x0f:
            msg_Dbg( p_demux, " * PMT descriptor : Private Data (0x0f)" );
            break;

        case 0xC1:
            msg_Dbg( p_demux, " * PMT descriptor : Digital copy control (0xC1)" );
            break;

        case 0x88: /* EACEM Simulcast HD Logical channels ordering */
            msg_Dbg( p_demux, " * descriptor : EACEM Simulcast HD" );
            /* TODO: apply visibility flags */
            break;

        default:
            msg_Dbg( p_demux, " * PMT descriptor : unknown (0x%x)", p_dr->i_tag );
        }
    }

    dvbpsi_pmt_es_t      *p_dvbpsies;
    for( p_dvbpsies = p_dvbpsipmt->p_first_es; p_dvbpsies != NULL; p_dvbpsies = p_dvbpsies->p_next )
    {
        bool b_reusing_pid = false;
        ts_pes_t *p_pes;

        ts_pid_t *pespid = GetPID(p_sys, p_dvbpsies->i_pid);
        if ( pespid->type == TYPE_PES && pespid->p_parent->u.p_pmt->i_number != p_pmt->i_number )
        {
            msg_Warn( p_demux, " * PMT wants to get a share or pid %d (unsupported)", pespid->i_pid );
            continue;
        }

        /* Find out if the PID was already declared */
        for( int i = 0; i < old_es_rm.i_size; i++ )
        {
            if( old_es_rm.p_elems[i]->i_pid == p_dvbpsies->i_pid )
            {
                b_reusing_pid = true;
                break;
            }
        }
        ValidateDVBMeta( p_demux, p_dvbpsies->i_pid );

        char const * psz_typedesc = "";
        switch(p_dvbpsies->i_type)
        {
        case 0x00:
            psz_typedesc = "ISO/IEC Reserved";
            break;
        case 0x01:
            psz_typedesc = "ISO/IEC 11172 Video";
            break;
        case 0x02:
            psz_typedesc = "ISO/IEC 13818-2 Video or ISO/IEC 11172-2 constrained parameter video stream";
            break;
        case 0x03:
            psz_typedesc = "ISO/IEC 11172 Audio";
            break;
        case 0x04:
            psz_typedesc = "ISO/IEC 13818-3 Audio";
            break;
        case 0x05:
            psz_typedesc = "ISO/IEC 13818-1 private_sections";
            break;
        case 0x06:
            psz_typedesc = "ISO/IEC 13818-1 PES packets containing private data";
            break;
        case 0x07:
            psz_typedesc = "ISO/IEC 13522 MHEG";
            break;
        case 0x08:
            psz_typedesc = "ISO/IEC 13818-1 Annex A DSM CC";
            break;
        case 0x09:
            psz_typedesc = "ITU-T Rec. H.222.1";
            break;
        case 0x0A:
            psz_typedesc = "ISO/IEC 13818-6 type A";
            break;
        case 0x0B:
            psz_typedesc = "ISO/IEC 13818-6 type B";
            break;
        case 0x0C:
            psz_typedesc = "ISO/IEC 13818-6 type C";
            break;
        case 0x0D:
            psz_typedesc = "ISO/IEC 13818-6 type D";
            break;
        case 0x0E:
            psz_typedesc = "ISO/IEC 13818-1 auxiliary";
            break;
        case 0x12:
            psz_typedesc = "ISO/IEC 14496-1 SL-packetized or FlexMux stream carried in PES packets";
            break;
        case 0x13:
            psz_typedesc = "ISO/IEC 14496-1 SL-packetized or FlexMux stream carried in sections";
            break;
        default:
            if (p_dvbpsies->i_type >= 0x0F && p_dvbpsies->i_type <=0x7F)
                psz_typedesc = "ISO/IEC 13818-1 Reserved";
            else
                psz_typedesc = "User Private";
        }

        msg_Dbg( p_demux, "  * pid=%d type=0x%x %s",
                 p_dvbpsies->i_pid, p_dvbpsies->i_type, psz_typedesc );

        for( p_dr = p_dvbpsies->p_first_descriptor; p_dr != NULL;
             p_dr = p_dr->p_next )
        {
            msg_Dbg( p_demux, "    - descriptor tag 0x%x",
                     p_dr->i_tag );
        }

        if ( !PIDSetup( p_demux, TYPE_PES, pespid, pmtpid ) )
        {
            msg_Warn( p_demux, "  * pid=%d type=0x%x %s (skipped)",
                      p_dvbpsies->i_pid, p_dvbpsies->i_type, psz_typedesc );
            continue;
        }
        else
        {
            if( b_reusing_pid )
            {
                p_pes = ts_pes_New( p_demux );
                if( !p_pes )
                    continue;
            }
            else
            {
                p_pes = pespid->u.p_pes;
            }
        }

        ARRAY_APPEND( p_pmt->e_streams, pespid );

        ts_es_data_type_t type_change = TS_ES_DATA_PES;
        PIDFillFormat( &p_pes->es.fmt, p_dvbpsies->i_type, &type_change );

        p_pes->i_stream_type = p_dvbpsies->i_type;
        pespid->i_flags |= SEEN(GetPID(p_sys, p_dvbpsies->i_pid));

        bool b_registration_applied = false;
        if ( p_dvbpsies->i_type >= 0x80 ) /* non standard, extensions */
        {
            if ( registration_type == TS_PMT_REGISTRATION_HDMV )
            {
                if (( b_registration_applied = PMTSetupEsHDMV( p_demux, &p_pes->es, p_dvbpsies ) ))
                    msg_Dbg( p_demux, "    + HDMV registration applied to pid %d type 0x%x",
                             p_dvbpsies->i_pid, p_dvbpsies->i_type );
            }
            else
            {
                if (( b_registration_applied = PMTSetupEsRegistration( p_demux, &p_pes->es, p_dvbpsies ) ))
                    msg_Dbg( p_demux, "    + registration applied to pid %d type 0x%x",
                        p_dvbpsies->i_pid, p_dvbpsies->i_type );
            }
        }

        if ( !b_registration_applied )
        {
            p_pes->data_type = type_change; /* Only change type if registration has not changed meaning */

            switch( p_dvbpsies->i_type )
            {
            case 0x06:
                /* Handle PES private data */
                PMTSetupEs0x06( p_demux, p_pes, p_dvbpsies );
                break;
            /* All other private or reserved types */
            case 0x13: /* SL in sections */
                p_pes->data_type = TS_ES_DATA_TABLE_SECTION;
                //ft
            case 0x0f:
            case 0x10:
            case 0x11:
            case 0x12:
                SetupISO14496Descriptors( p_demux, &p_pes->es, p_pmt, p_dvbpsies );
                break;
            case 0x83:
                /* LPCM (audio) */
                PMTSetupEs0x83( p_dvbpsipmt, &p_pes->es, p_dvbpsies->i_pid );
                break;
            case 0xa0:
                PMTSetupEs0xA0( p_demux, &p_pes->es, p_dvbpsies );
                break;
            case 0xd1:
                PMTSetupEs0xD1( p_demux, &p_pes->es, p_dvbpsies );
                break;
            case 0xEA:
                PMTSetupEs0xEA( p_demux, &p_pes->es, p_dvbpsies );
            default:
                break;
            }
        }

        if( p_pes->es.fmt.i_cat == AUDIO_ES ||
            ( p_pes->es.fmt.i_cat == SPU_ES &&
              p_pes->es.fmt.i_codec != VLC_CODEC_DVBS &&
              p_pes->es.fmt.i_codec != VLC_CODEC_TELETEXT ) )
        {
            PMTParseEsIso639( p_demux, &p_pes->es, p_dvbpsies );
        }

        /* Set Groups / ID */
        p_pes->es.fmt.i_group = p_dvbpsipmt->i_program_number;
        if( p_sys->b_es_id_pid )
            p_pes->es.fmt.i_id = p_dvbpsies->i_pid;

        if( p_pes->es.fmt.i_cat == UNKNOWN_ES )
        {
            msg_Dbg( p_demux, "   => pid %d content is *unknown*",
                     p_dvbpsies->i_pid );
            p_pes->es.fmt.psz_description = strdup( psz_typedesc );
        }
        else
        {
            msg_Dbg( p_demux, "   => pid %d has now es fcc=%4.4s",
                     p_dvbpsies->i_pid, (char*)&p_pes->es.fmt.i_codec );

            /* Check if we can avoid restarting the ES */
            if( b_reusing_pid )
            {
                /* p_pes points to a tmp pes */
                if( !es_format_IsSimilar( &pespid->u.p_pes->es.fmt, &p_pes->es.fmt ) ||
                    pespid->u.p_pes->es.fmt.i_extra != p_pes->es.fmt.i_extra ||
                    ( pespid->u.p_pes->es.fmt.i_extra > 0 &&
                      memcmp( pespid->u.p_pes->es.fmt.p_extra,
                              p_pes->es.fmt.p_extra,
                              p_pes->es.fmt.i_extra ) ) ||
                    pespid->u.p_pes->extra_es.i_size != p_pes->extra_es.i_size ||
                    !!pespid->u.p_pes->es.fmt.psz_language != !!p_pes->es.fmt.psz_language ||
                    ( pespid->u.p_pes->es.fmt.psz_language != NULL &&
                      strcmp( pespid->u.p_pes->es.fmt.psz_language, p_pes->es.fmt.psz_language ) )
                  )
                {
                    /* Differs, swap then */
                    ts_pes_t *old = pespid->u.p_pes;
                    pespid->u.p_pes = p_pes;
                    AddAndCreateES( p_demux, pespid, false );
                    ts_pes_Del( p_demux, old );
                }
                else
                    ts_pes_Del( p_demux, p_pes ); // delete temp, stay with current es/es_id
            }
            else
            {
                AddAndCreateES( p_demux, pespid, false );
            }
        }

        p_dr = PMTEsFindDescriptor( p_dvbpsies, 0x09 );
        if( p_dr && p_dr->i_length >= 2 )
        {
            msg_Dbg( p_demux, "   * PMT descriptor : CA (0x9) SysID 0x%x",
                     (p_dr->p_data[0] << 8) | p_dr->p_data[1] );
        }
    }

    /* Set CAM descrambling */
    if( !ProgramIsSelected( p_sys, p_pmt->i_number ) )
    {
        dvbpsi_pmt_delete( p_dvbpsipmt );
    }
    else if( stream_Control( p_sys->stream, STREAM_SET_PRIVATE_ID_CA,
                             p_dvbpsipmt ) != VLC_SUCCESS )
    {
        if ( p_sys->arib.e_mode == ARIBMODE_ENABLED && !p_sys->arib.b25stream )
        {
            p_sys->arib.b25stream = stream_FilterNew( p_demux->s, "aribcam" );
            p_sys->stream = ( p_sys->arib.b25stream ) ? p_sys->arib.b25stream : p_demux->s;
            if (!p_sys->arib.b25stream)
                dvbpsi_pmt_delete( p_dvbpsipmt );
        } else dvbpsi_pmt_delete( p_dvbpsipmt );
    }

    /* Decref or clean now unused es */
    for( int i = 0; i < old_es_rm.i_size; i++ )
        PIDRelease( p_demux, old_es_rm.p_elems[i] );
    ARRAY_RESET( old_es_rm );

    UpdatePESFilters( p_demux, p_sys->b_es_all );

    if( !p_sys->b_trust_pcr )
    {
        int i_cand = FindPCRCandidate( p_pmt );
        p_pmt->i_pid_pcr = i_cand;
        p_pmt->pcr.b_disable = true;
        msg_Warn( p_demux, "PCR not trusted for program %d, set up workaround using pid %d",
                  p_pmt->i_number, i_cand );
    }

    /* Probe Boundaries */
    if( p_sys->b_canfastseek && p_pmt->i_last_dts == -1 )
    {
        p_pmt->i_last_dts = 0;
        ProbeStart( p_demux, p_pmt->i_number );
        ProbeEnd( p_demux, p_pmt->i_number );
    }
}

static int PATCheck( demux_t *p_demux, dvbpsi_pat_t *p_pat )
{
    /* Some Dreambox streams have all PMT set to same pid */
    int i_prev_pid = -1;
    for( dvbpsi_pat_program_t * p_program = p_pat->p_first_program;
         p_program != NULL;
         p_program = p_program->p_next )
    {
        if( p_program->i_pid == i_prev_pid )
        {
            msg_Warn( p_demux, "PAT check failed: duplicate program pid %d", i_prev_pid );
            return VLC_EGENERIC;
        }
        i_prev_pid = p_program->i_pid;
    }
    return VLC_SUCCESS;
}

static void PATCallBack( void *data, dvbpsi_pat_t *p_dvbpsipat )
{
    demux_t              *p_demux = data;
    demux_sys_t          *p_sys = p_demux->p_sys;
    dvbpsi_pat_program_t *p_program;
    ts_pid_t             *patpid = GetPID(p_sys, 0);
    ts_pat_t             *p_pat = GetPID(p_sys, 0)->u.p_pat;

    patpid->i_flags |= FLAG_SEEN;

    msg_Dbg( p_demux, "PATCallBack called" );

    if(unlikely( GetPID(p_sys, 0)->type != TYPE_PAT ))
    {
        msg_Warn( p_demux, "PATCallBack called on invalid pid" );
        return;
    }

    if( ( p_pat->i_version != -1 &&
            ( !p_dvbpsipat->b_current_next ||
              p_dvbpsipat->i_version == p_pat->i_version ) ) ||
        ( p_pat->i_ts_id != -1 && p_dvbpsipat->i_ts_id != p_pat->i_ts_id ) ||
        p_sys->b_user_pmt || PATCheck( p_demux, p_dvbpsipat ) )
    {
        dvbpsi_pat_delete( p_dvbpsipat );
        return;
    }

    msg_Dbg( p_demux, "new PAT ts_id=%d version=%d current_next=%d",
             p_dvbpsipat->i_ts_id, p_dvbpsipat->i_version, p_dvbpsipat->b_current_next );

    /* Save old programs array */
    DECL_ARRAY(ts_pid_t *) old_pmt_rm;
    old_pmt_rm.i_alloc = p_pat->programs.i_alloc;
    old_pmt_rm.i_size = p_pat->programs.i_size;
    old_pmt_rm.p_elems = p_pat->programs.p_elems;
    ARRAY_INIT(p_pat->programs);

    /* now create programs */
    for( p_program = p_dvbpsipat->p_first_program; p_program != NULL;
         p_program = p_program->p_next )
    {
        msg_Dbg( p_demux, "  * number=%d pid=%d", p_program->i_number,
                 p_program->i_pid );
        if( p_program->i_number == 0 )
            continue;

        ts_pid_t *pmtpid = GetPID(p_sys, p_program->i_pid);

        ValidateDVBMeta( p_demux, p_program->i_pid );

        bool b_existing = (pmtpid->type == TYPE_PMT);
        /* create or temporary incref pid */
        if( !PIDSetup( p_demux, TYPE_PMT, pmtpid, patpid ) )
        {
            msg_Warn( p_demux, "  * number=%d pid=%d (ignored)", p_program->i_number,
                     p_program->i_pid );
            continue;
        }

        if( !b_existing || pmtpid->u.p_pmt->i_number != p_program->i_number )
        {
            if( b_existing && pmtpid->u.p_pmt->i_number != p_program->i_number )
                dvbpsi_pmt_detach(pmtpid->u.p_pmt->handle);

            if( !dvbpsi_pmt_attach( pmtpid->u.p_pmt->handle, p_program->i_number, PMTCallBack, p_demux ) )
                msg_Err( p_demux, "PATCallback failed attaching PMTCallback to program %d",
                         p_program->i_number );
        }

        pmtpid->u.p_pmt->i_number = p_program->i_number;

        ARRAY_APPEND( p_pat->programs, pmtpid );

        /* Now select PID at access level */
        if( p_sys->programs.i_size == 0 ||
            ProgramIsSelected( p_sys, p_program->i_number ) )
        {
            if( p_sys->programs.i_size == 0 )
            {
                msg_Dbg( p_demux, "temporary receiving program %d", p_program->i_number );
                p_sys->b_default_selection = true;
                ARRAY_APPEND( p_sys->programs, p_program->i_number );
            }

            if( SetPIDFilter( p_sys, pmtpid, true ) )
                p_sys->b_access_control = false;
            else if ( p_sys->es_creation == DELAY_ES )
                p_sys->es_creation = CREATE_ES;
        }
    }
    p_pat->i_version = p_dvbpsipat->i_version;
    p_pat->i_ts_id = p_dvbpsipat->i_ts_id;

    for(int i=0; i<old_pmt_rm.i_size; i++)
    {
        /* decref current or release now unreferenced */
        PIDRelease( p_demux, old_pmt_rm.p_elems[i] );
    }
    ARRAY_RESET(old_pmt_rm);

    dvbpsi_pat_delete( p_dvbpsipat );
}

static inline bool handle_Init( demux_t *p_demux, dvbpsi_t **handle )
{
    *handle = dvbpsi_new( &dvbpsi_messages, DVBPSI_MSG_DEBUG );
    if( !*handle )
        return false;
    (*handle)->p_sys = (void *) p_demux;
    return true;
}

static ts_pat_t *ts_pat_New( demux_t *p_demux )
{
    ts_pat_t *pat = malloc( sizeof( ts_pat_t ) );
    if( !pat )
        return NULL;

    if( !handle_Init( p_demux, &pat->handle ) )
    {
        free( pat );
        return NULL;
    }

    pat->i_version  = -1;
    pat->i_ts_id    = -1;
    ARRAY_INIT( pat->programs );

    return pat;
}

static void ts_pat_Del( demux_t *p_demux, ts_pat_t *pat )
{
    if( dvbpsi_decoder_present( pat->handle ) )
        dvbpsi_pat_detach( pat->handle );
    dvbpsi_delete( pat->handle );
    for( int i=0; i<pat->programs.i_size; i++ )
        PIDRelease( p_demux, pat->programs.p_elems[i] );
    ARRAY_RESET( pat->programs );
    free( pat );
}

static ts_pmt_t *ts_pmt_New( demux_t *p_demux )
{
    ts_pmt_t *pmt = malloc( sizeof( ts_pmt_t ) );
    if( !pmt )
        return NULL;

    if( !handle_Init( p_demux, &pmt->handle ) )
    {
        free( pmt );
        return NULL;
    }

    ARRAY_INIT( pmt->e_streams );

    pmt->i_version  = -1;
    pmt->i_number   = -1;
    pmt->i_pid_pcr  = 0x1FFF;
    pmt->iod        = NULL;
    pmt->od.i_version = -1;
    ARRAY_INIT( pmt->od.objects );

    pmt->i_last_dts = -1;

    pmt->pcr.i_current = -1;
    pmt->pcr.i_first  = -1;
    pmt->pcr.b_disable = false;
    pmt->pcr.i_first_dts = VLC_TS_INVALID;
    pmt->pcr.i_pcroffset = -1;

    pmt->pcr.b_fix_done = false;

    return pmt;
}

static void ts_pmt_Del( demux_t *p_demux, ts_pmt_t *pmt )
{
    if( dvbpsi_decoder_present( pmt->handle ) )
        dvbpsi_pmt_detach( pmt->handle );
    dvbpsi_delete( pmt->handle );
    for( int i=0; i<pmt->e_streams.i_size; i++ )
        PIDRelease( p_demux, pmt->e_streams.p_elems[i] );
    ARRAY_RESET( pmt->e_streams );
    if( pmt->iod )
        ODFree( pmt->iod );
    for( int i=0; i<pmt->od.objects.i_size; i++ )
        ODFree( pmt->od.objects.p_elems[i] );
    ARRAY_RESET( pmt->od.objects );
    if( pmt->i_number > -1 )
        es_out_Control( p_demux->out, ES_OUT_DEL_GROUP, pmt->i_number );
    free( pmt );
}

static ts_pes_t *ts_pes_New( demux_t *p_demux )
{
    VLC_UNUSED(p_demux);
    ts_pes_t *pes = malloc( sizeof( ts_pes_t ) );
    if( !pes )
        return NULL;

    pes->es.id = NULL;
    pes->es.i_sl_es_id = 0;
    es_format_Init( &pes->es.fmt, UNKNOWN_ES, 0 );
    ARRAY_INIT( pes->extra_es );
    pes->i_stream_type = 0;
    pes->data_type = TS_ES_DATA_PES;
    pes->i_data_size = 0;
    pes->i_data_gathered = 0;
    pes->p_data = NULL;
    pes->pp_last = &pes->p_data;
    pes->p_prepcr_outqueue = NULL;
    pes->sl.p_data = NULL;
    pes->sl.pp_last = &pes->sl.p_data;

    return pes;
}

static void ts_pes_Del( demux_t *p_demux, ts_pes_t *pes )
{
    if( pes->es.id )
    {
        /* Ensure we don't wait for overlap hacks #14257 */
        es_out_Control( p_demux->out, ES_OUT_SET_ES_STATE, pes->es.id, false );
        es_out_Del( p_demux->out, pes->es.id );
        p_demux->p_sys->i_pmt_es--;
    }

    if( pes->p_data )
        block_ChainRelease( pes->p_data );

    if( pes->p_prepcr_outqueue )
        block_ChainRelease( pes->p_prepcr_outqueue );

    es_format_Clean( &pes->es.fmt );

    for( int i = 0; i < pes->extra_es.i_size; i++ )
    {
        if( pes->extra_es.p_elems[i]->id )
        {
            es_out_Del( p_demux->out, pes->extra_es.p_elems[i]->id );
            p_demux->p_sys->i_pmt_es--;
        }
        es_format_Clean( &pes->extra_es.p_elems[i]->fmt );
        free( pes->extra_es.p_elems[i] );
    }
    ARRAY_RESET( pes->extra_es );

    free( pes );
}

static ts_psi_t *ts_psi_New( demux_t *p_demux )
{
    ts_psi_t *psi = malloc( sizeof( ts_psi_t ) );
    if( !psi )
        return NULL;

    if( !handle_Init( p_demux, &psi->handle ) )
    {
        free( psi );
        return NULL;
    }

    psi->i_version  = -1;

    return psi;
}

static void ts_psi_Del( demux_t *p_demux, ts_psi_t *psi )
{
    VLC_UNUSED(p_demux);
    if( dvbpsi_decoder_present( psi->handle ) )
        dvbpsi_DetachDemux( psi->handle );
    dvbpsi_delete( psi->handle );
    free( psi );
}
