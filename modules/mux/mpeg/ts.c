/*****************************************************************************
 * ts.c: MPEG-II TS Muxer
 *****************************************************************************
 * Copyright (C) 2001-2005 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *          Jean-Paul Saman <jpsaman #_at_# m2x.nl>
 *          Wallace Wadge <wwadge #_at_# gmail.com>
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

#include <limits.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_rand.h>
#include <vlc_charset.h>

#include <vlc_iso_lang.h>

#include "bits.h"
#include "pes.h"
#include "csa.h"
#include "tsutil.h"
#include "streams.h"

# include <dvbpsi/dvbpsi.h>
# include <dvbpsi/demux.h>
# include <dvbpsi/descriptor.h>
# include <dvbpsi/pat.h>
# include <dvbpsi/pmt.h>
# include <dvbpsi/sdt.h>
# include <dvbpsi/dr.h>
# include <dvbpsi/psi.h>

#include "dvbpsi_compat.h"

#include "tables.h"

#include "../../codec/jpeg2000.h"
#include "../../demux/mpeg/timestamps.h"

/*
 * TODO:
 *  - check PCR frequency requirement
 *  - check PAT/PMT  "        "
 *  - check PCR/PCR "soft"
 *  - check if "registration" descriptor : "AC-3" should be a program
 *    descriptor or an es one. (xine want an es one)
 *
 *  - remove creation of PAT/PMT without dvbpsi
 *  - ?
 * FIXME:
 *  - subtitle support is far from perfect. I expect some subtitles drop
 *    if they arrive a bit late
 *    (We cannot rely on the fact that the fifo should be full)
 */

/*****************************************************************************
 * Callback prototypes
 *****************************************************************************/
static int ChangeKeyCallback    ( vlc_object_t *, char const *, vlc_value_t, vlc_value_t, void * );
static int ActiveKeyCallback    ( vlc_object_t *, char const *, vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int     Open   ( vlc_object_t * );
static void    Close  ( vlc_object_t * );

static const char *const ts_standards_list[] =
    { "dvb", "atsc", };
static const char *const ts_standards_list_text[] =
    { "DVB", "ATSC", };

#define STANDARD_TEXT N_("Digital TV Standard")

#define VPID_TEXT N_("Video PID")
#define VPID_LONGTEXT N_("Assign a fixed PID to the video stream. The PCR " \
  "PID will automatically be the video.")
#define APID_TEXT N_("Audio PID")
#define APID_LONGTEXT N_("Assign a fixed PID to the audio stream.")
#define SPUPID_TEXT N_("SPU PID")
#define SPUPID_LONGTEXT N_("Assign a fixed PID to the SPU.")
#define PMTPID_TEXT N_("PMT PID")
#define PMTPID_LONGTEXT N_("Assign a fixed PID to the PMT")
#define TSID_TEXT N_("TS ID")
#define TSID_LONGTEXT N_("Assign a fixed Transport Stream ID.")
#define NETID_TEXT N_("NET ID")
#define NETID_LONGTEXT N_("Assign a fixed Network ID (for SDT table)")

#define PMTPROG_TEXT N_("PMT Program numbers")
#define PMTPROG_LONGTEXT N_("Assign a program number to each PMT. This " \
                            "requires \"Set PID to ID of ES\" to be enabled." )

#define MUXPMT_TEXT N_("Mux PMT (requires --sout-ts-es-id-pid)")
#define MUXPMT_LONGTEXT N_("Define the pids to add to each pmt. This " \
                           "requires \"Set PID to ID of ES\" to be enabled." )

#define SDTDESC_TEXT N_("SDT Descriptors (requires --sout-ts-es-id-pid)")
#define SDTDESC_LONGTEXT N_("Defines the descriptors of each SDT. This " \
                        "requires \"Set PID to ID of ES\" to be enabled." )

#define PID_TEXT N_("Set PID to ID of ES")
#define PID_LONGTEXT N_("Sets PID to the ID if the incoming ES. This is for " \
  "use with --ts-es-id-pid, and allows having the same PIDs in the input " \
  "and output streams.")

#define ALIGNMENT_TEXT N_("Data alignment")
#define ALIGNMENT_LONGTEXT N_("Enforces alignment of all access units on " \
  "PES boundaries. Disabling this might save some bandwidth but introduce incompatibilities.")

#define SHAPING_TEXT N_("Shaping delay (ms)")
#define SHAPING_LONGTEXT N_("Cut the " \
  "stream in slices of the given duration, and ensure a constant bitrate " \
  "between the two boundaries. This avoids having huge bitrate peaks, " \
  "especially for reference frames." )

#define KEYF_TEXT N_("Use keyframes")
#define KEYF_LONGTEXT N_("If enabled, and shaping is specified, " \
  "the TS muxer will place the boundaries at the end of I pictures. In " \
  "that case, the shaping duration given by the user is a worse case " \
  "used when no reference frame is available. This enhances the efficiency " \
  "of the shaping algorithm, since I frames are usually the biggest " \
  "frames in the stream.")

#define PCR_TEXT N_("PCR interval (ms)")
#define PCR_LONGTEXT N_("Set at which interval " \
  "PCRs (Program Clock Reference) will be sent (in milliseconds). " \
  "This value should be below 100ms. (default is 70ms).")

#define BMIN_TEXT N_( "Minimum B (deprecated)")
#define BMIN_LONGTEXT N_( "This setting is deprecated and not used anymore" )

#define BMAX_TEXT N_( "Maximum B (deprecated)")
#define BMAX_LONGTEXT N_( "This setting is deprecated and not used anymore")

#define DTS_TEXT N_("DTS delay (ms)")
#define DTS_LONGTEXT N_("Delay the DTS (decoding time " \
  "stamps) and PTS (presentation timestamps) of the data in the " \
  "stream, compared to the PCRs. This allows for some buffering inside " \
  "the client decoder.")

#define ACRYPT_TEXT N_("Crypt audio")
#define ACRYPT_LONGTEXT N_("Crypt audio using CSA")
#define VCRYPT_TEXT N_("Crypt video")
#define VCRYPT_LONGTEXT N_("Crypt video using CSA")

#define CK_TEXT N_("CSA Key")
#define CK_LONGTEXT N_("CSA encryption key. This must be a " \
  "16 char string (8 hexadecimal bytes).")

#define CK2_TEXT N_("Second CSA Key")
#define CK2_LONGTEXT N_("The even CSA encryption key. This must be a " \
  "16 char string (8 hexadecimal bytes).")

#define CU_TEXT N_("CSA Key in use")
#define CU_LONGTEXT N_("CSA encryption key used. It can be the odd/first/1 " \
  "(default) or the even/second/2 one.")

#define CPKT_TEXT N_("Packet size in bytes to encrypt")
#define CPKT_LONGTEXT N_("Size of the TS packet to encrypt. " \
    "The encryption routines subtract the TS-header from the value before " \
    "encrypting." )

#define SOUT_CFG_PREFIX "sout-ts-"
#define MAX_PMT 64       /* Maximum number of programs. FIXME: I just chose an arbitrary number. Where is the maximum in the spec? */
#define MAX_PMT_PID 64       /* Maximum pids in each pmt.  FIXME: I just chose an arbitrary number. Where is the maximum in the spec? */
#if MAX_SDT_DESC < MAX_PMT
  #error "MAX_SDT_DESC < MAX_PMT"
#endif

#define BLOCK_FLAG_NO_KEYFRAME (1 << BLOCK_FLAG_PRIVATE_SHIFT) /* This is not a key frame for bitrate shaping */

vlc_module_begin ()
    set_description( N_("TS muxer (libdvbpsi)") )
    set_shortname( "MPEG-TS")
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_MUX )
    set_capability( "sout mux", 120 )
    add_shortcut( "ts" )

    add_string( SOUT_CFG_PREFIX "standard", "dvb", STANDARD_TEXT, NULL, true )
        change_string_list( ts_standards_list, ts_standards_list_text )
    add_integer(SOUT_CFG_PREFIX "pid-video", 100, VPID_TEXT, VPID_LONGTEXT, true)
        change_integer_range( 32, 8190 )
    add_integer(SOUT_CFG_PREFIX "pid-audio", 200, APID_TEXT, APID_LONGTEXT, true)
        change_integer_range( 32, 8190 )
    add_integer(SOUT_CFG_PREFIX "pid-spu",   300, SPUPID_TEXT, SPUPID_LONGTEXT, true)
        change_integer_range( 32, 8190 )
    add_integer(SOUT_CFG_PREFIX "pid-pmt", 32, PMTPID_TEXT, PMTPID_LONGTEXT, true)
        change_integer_range( 32, 8190 )
    add_integer(SOUT_CFG_PREFIX "tsid",  0, TSID_TEXT, TSID_LONGTEXT, true)
    add_integer(SOUT_CFG_PREFIX "netid", 0, NETID_TEXT, NETID_LONGTEXT, true)
    add_string(SOUT_CFG_PREFIX "program-pmt", NULL, PMTPROG_TEXT, PMTPROG_LONGTEXT, true)
    add_bool(SOUT_CFG_PREFIX "es-id-pid", false, PID_TEXT, PID_LONGTEXT, true)
    add_string(SOUT_CFG_PREFIX "muxpmt",  NULL, MUXPMT_TEXT, MUXPMT_LONGTEXT, true)
    add_string(SOUT_CFG_PREFIX "sdtdesc", NULL, SDTDESC_TEXT, SDTDESC_LONGTEXT, true)
    add_bool(SOUT_CFG_PREFIX "alignment", true, ALIGNMENT_TEXT, ALIGNMENT_LONGTEXT, true)

    add_integer(SOUT_CFG_PREFIX "shaping", 200, SHAPING_TEXT, SHAPING_LONGTEXT, true)
    add_bool(SOUT_CFG_PREFIX "use-key-frames", false, KEYF_TEXT, KEYF_LONGTEXT, true)

    add_integer( SOUT_CFG_PREFIX "pcr", 70, PCR_TEXT, PCR_LONGTEXT, true)
    add_integer( SOUT_CFG_PREFIX "bmin", 0, BMIN_TEXT, BMIN_LONGTEXT, true)
    add_integer( SOUT_CFG_PREFIX "bmax", 0, BMAX_TEXT, BMAX_LONGTEXT, true)
    add_integer( SOUT_CFG_PREFIX "dts-delay", 400, DTS_TEXT, DTS_LONGTEXT, true)

    add_bool( SOUT_CFG_PREFIX "crypt-audio", true, ACRYPT_TEXT, ACRYPT_LONGTEXT, true)
    add_bool( SOUT_CFG_PREFIX "crypt-video", true, VCRYPT_TEXT, VCRYPT_LONGTEXT, true)
    add_string( SOUT_CFG_PREFIX "csa-ck",  NULL, CK_TEXT,   CK_LONGTEXT,   true)
    add_string( SOUT_CFG_PREFIX "csa2-ck", NULL, CK2_TEXT,  CK2_LONGTEXT,  true)
    add_string( SOUT_CFG_PREFIX "csa-use", "1",  CU_TEXT,   CU_LONGTEXT,   true)
    add_integer(SOUT_CFG_PREFIX "csa-pkt", 188,  CPKT_TEXT, CPKT_LONGTEXT, true)

    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local data structures
 *****************************************************************************/
static const char *const ppsz_sout_options[] = {
    "standard",
    "pid-video", "pid-audio", "pid-spu", "pid-pmt", "tsid",
    "netid", "sdtdesc",
    "es-id-pid", "shaping", "pcr", "bmin", "bmax", "use-key-frames",
    "dts-delay", "csa-ck", "csa2-ck", "csa-use", "csa-pkt", "crypt-audio", "crypt-video",
    "muxpmt", "program-pmt", "alignment",
    NULL
};

typedef struct pmt_map_t   /* Holds the mapping between the pmt-pid/pmt table */
{
    int i_pid;
    unsigned i_prog;
} pmt_map_t;

typedef struct
{
    int     i_depth;
    block_t *p_first;
    block_t **pp_last;
} sout_buffer_chain_t;

static inline void BufferChainInit  ( sout_buffer_chain_t *c )
{
    c->i_depth = 0;
    c->p_first = NULL;
    c->pp_last = &c->p_first;
}

static inline void BufferChainAppend( sout_buffer_chain_t *c, block_t *b )
{
    *c->pp_last = b;
    c->i_depth++;

    while( b->p_next )
    {
        b = b->p_next;
        c->i_depth++;
    }
    c->pp_last = &b->p_next;
}

static inline block_t *BufferChainGet( sout_buffer_chain_t *c )
{
    block_t *b = c->p_first;

    if( b )
    {
        c->i_depth--;
        c->p_first = b->p_next;

        if( c->p_first == NULL )
        {
            c->pp_last = &c->p_first;
        }

        b->p_next = NULL;
    }
    return b;
}

static inline block_t *BufferChainPeek( sout_buffer_chain_t *c )
{
    block_t *b = c->p_first;

    return b;
}

static inline void BufferChainClean( sout_buffer_chain_t *c )
{
    block_t *b;

    while( ( b = BufferChainGet( c ) ) )
    {
        block_Release( b );
    }
    BufferChainInit( c );
}

typedef struct
{
    sout_buffer_chain_t chain_pes;
    vlc_tick_t          i_pes_dts;
    vlc_tick_t          i_pes_length;
    int                 i_pes_used;
    bool                b_key_frame;

} pes_state_t;

typedef struct
{
    tsmux_stream_t  ts;
    pesmux_stream_t pes;
    pes_state_t  state;
} sout_input_sys_t;

typedef struct
{
    sout_input_t    *p_pcr_input;

    vlc_mutex_t     csa_lock;

    dvbpsi_t        *p_dvbpsi;
    bool            b_es_id_pid;
    int             i_pid_video;
    int             i_pid_audio;
    int             i_pid_spu;

    int             i_tsid;
    unsigned        i_num_pmt;
    int             i_pmtslots;
    int             i_pat_version_number;
    tsmux_stream_t  pat;

    int             i_pmt_version_number;
    tsmux_stream_t  pmt[MAX_PMT];
    pmt_map_t       pmtmap[MAX_PMT_PID];
    int             i_pmt_program_number[MAX_PMT];
    bool            b_data_alignment;

    sdt_psi_t       sdt;
    ts_mux_standard standard;

    /* for TS building */
    int64_t         i_bitrate_min;
    int64_t         i_bitrate_max;

    vlc_tick_t      i_shaping_delay;
    vlc_tick_t      i_pcr_delay;

    vlc_tick_t      i_dts_delay;
    vlc_tick_t      first_dts;

    bool            b_use_key_frames;

    vlc_tick_t      i_pcr;  /* last PCR emited */

    csa_t           *csa;
    int             i_csa_pkt_size;
    bool            b_crypt_audio;
    bool            b_crypt_video;
} sout_mux_sys_t;


static int GetNextFreePID( sout_mux_t *p_mux, int i_pid_start )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    restart:
    for(unsigned i=i_pid_start; i<p_sys->i_num_pmt; i++)
    {
        if(p_sys->pmt[i].i_pid == i_pid_start)
        {
            i_pid_start++;
            goto restart;
        }
    }

    for(int i=0; i<p_mux->i_nb_inputs; i++)
    {
        sout_input_sys_t *p_stream = (sout_input_sys_t*)p_mux->pp_inputs[i]->p_sys;
        if(p_stream->ts.i_pid == i_pid_start)
        {
            i_pid_start++;
            goto restart;
        }
    }

    if( i_pid_start > 8190 )
    {
        i_pid_start = 32;
        goto restart;
    }

    return i_pid_start;
}

/* Reserve a pid and return it */
static int  AllocatePID( sout_mux_t *p_mux, int i_cat )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    int i_pid;
    int *pi_candidate_pid = NULL;

    switch( i_cat )
    {
    case VIDEO_ES:
        pi_candidate_pid = &p_sys->i_pid_video;
        break;

    case AUDIO_ES:
        pi_candidate_pid = &p_sys->i_pid_audio;
        break;

    case SPU_ES:
    default:
        pi_candidate_pid = &p_sys->i_pid_spu;
        break;
    }

    *pi_candidate_pid = GetNextFreePID( p_mux, *pi_candidate_pid );
    i_pid = (*pi_candidate_pid)++;

    return i_pid;
}

static int pmtcompare( const void *pa, const void *pb )
{
    int id1 = ((pmt_map_t *)pa)->i_pid;
    int id2 = ((pmt_map_t *)pb)->i_pid;

    return id1 - id2;
}

static int intcompare( const void *pa, const void *pb )
{
    return *(int*)pa - *(int*)pb;
}

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Control  ( sout_mux_t *, int, va_list );
static int AddStream( sout_mux_t *, sout_input_t * );
static void DelStream( sout_mux_t *, sout_input_t * );
static int Mux      ( sout_mux_t * );

static block_t *FixPES( sout_mux_t *p_mux, block_fifo_t *p_fifo );
static block_t *Add_ADTS( block_t *, const es_format_t * );
static void TSSchedule  ( sout_mux_t *p_mux, sout_buffer_chain_t *p_chain_ts,
                          vlc_tick_t i_pcr_length, vlc_tick_t i_pcr_dts );
static void TSDate      ( sout_mux_t *p_mux, sout_buffer_chain_t *p_chain_ts,
                          vlc_tick_t i_pcr_length, vlc_tick_t i_pcr_dts );
static void GetPAT( sout_mux_t *p_mux, sout_buffer_chain_t *c );
static void GetPMT( sout_mux_t *p_mux, sout_buffer_chain_t *c );

static block_t *TSNew( sout_mux_t *p_mux, sout_input_sys_t *p_stream, bool b_pcr );
static void TSSetPCR( block_t *p_ts, vlc_tick_t i_dts );

static csa_t *csaSetup( vlc_object_t *p_this )
{
    sout_mux_t *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    char *csack = var_CreateGetNonEmptyStringCommand( p_mux, SOUT_CFG_PREFIX "csa-ck" );
    if( !csack )
        return NULL;

    csa_t *csa = csa_New();

    if( csa_SetCW( p_this, csa, csack, true ) )
    {
        free(csack);
        csa_Delete( csa );
        return NULL;
    }

    vlc_mutex_init( &p_sys->csa_lock );
    p_sys->b_crypt_audio = var_GetBool( p_mux, SOUT_CFG_PREFIX "crypt-audio" );
    p_sys->b_crypt_video = var_GetBool( p_mux, SOUT_CFG_PREFIX "crypt-video" );

    char *csa2ck = var_CreateGetNonEmptyStringCommand( p_mux, SOUT_CFG_PREFIX "csa2-ck");
    if (!csa2ck || csa_SetCW( p_this, csa, csa2ck, false ) )
        csa_SetCW( p_this, csa, csack, false );
    free(csa2ck);

    var_Create( p_mux, SOUT_CFG_PREFIX "csa-use", VLC_VAR_STRING | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_mux, SOUT_CFG_PREFIX "csa-use", ActiveKeyCallback, NULL );
    var_AddCallback( p_mux, SOUT_CFG_PREFIX "csa-ck", ChangeKeyCallback, p_mux );
    var_AddCallback( p_mux, SOUT_CFG_PREFIX "csa2-ck", ChangeKeyCallback, NULL );

    vlc_value_t use_val;
    var_Get( p_mux, SOUT_CFG_PREFIX "csa-use", &use_val );
    if ( var_Set( p_mux, SOUT_CFG_PREFIX "csa-use", use_val ) )
        var_SetString( p_mux, SOUT_CFG_PREFIX "csa-use", "odd" );
    free( use_val.psz_string );

    p_sys->i_csa_pkt_size = var_GetInteger( p_mux, SOUT_CFG_PREFIX "csa-pkt" );
    if( p_sys->i_csa_pkt_size < 12 || p_sys->i_csa_pkt_size > 188 )
    {
        msg_Err( p_mux, "wrong packet size %d specified",
            p_sys->i_csa_pkt_size );
        p_sys->i_csa_pkt_size = 188;
    }

    msg_Dbg( p_mux, "encrypting %d bytes of packet", p_sys->i_csa_pkt_size );

    free(csack);

    return csa;
}

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_mux_t          *p_mux =(sout_mux_t*)p_this;
    sout_mux_sys_t      *p_sys = NULL;

    config_ChainParse( p_mux, SOUT_CFG_PREFIX, ppsz_sout_options, p_mux->p_cfg );

    p_sys = calloc( 1, sizeof( sout_mux_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;
    p_sys->i_num_pmt = 1;

    p_sys->p_dvbpsi = dvbpsi_new( &dvbpsi_messages, DVBPSI_MSG_DEBUG );
    if( !p_sys->p_dvbpsi )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }
    p_sys->p_dvbpsi->p_sys = (void *) p_mux;

    char *psz_standard = var_GetString( p_mux, SOUT_CFG_PREFIX "standard" );
    if( psz_standard && !strcmp("atsc", psz_standard) )
        p_sys->standard = TS_MUX_STANDARD_ATSC;
    free( psz_standard );

    p_sys->b_es_id_pid = var_GetBool( p_mux, SOUT_CFG_PREFIX "es-id-pid" );

    /*
       fetch string of pmts. Here's a sample: --sout-ts-muxpmt="0x451,0x200,0x28a,0x240,,0x450,0x201,0x28b,0x241,,0x452,0x202,0x28c,0x242"
       This would mean 0x451, 0x200, 0x28a, 0x240 would fall under one pmt (program), 0x450,0x201,0x28b,0x241 would fall under another
    */
    char *muxpmt = var_GetNonEmptyString(p_mux, SOUT_CFG_PREFIX "muxpmt");
    for (char *psz = muxpmt; psz; )
    {
        char *psz_next;
        uint16_t i_pid = strtoul( psz, &psz_next, 0 );
        psz = *psz_next ? &psz_next[1] : NULL;

        if ( i_pid == 0 )
        {
            if ( ++p_sys->i_num_pmt > MAX_PMT )
            {
                msg_Err( p_mux, "Number of PMTs > %d)", MAX_PMT );
                p_sys->i_num_pmt = MAX_PMT;
            }
        }
        else
        {
            p_sys->pmtmap[p_sys->i_pmtslots].i_pid = i_pid;
            p_sys->pmtmap[p_sys->i_pmtslots].i_prog = p_sys->i_num_pmt - 1;
            if ( ++p_sys->i_pmtslots >= MAX_PMT_PID )
            {
                msg_Err( p_mux, "Number of pids in PMT > %d", MAX_PMT_PID );
                p_sys->i_pmtslots = MAX_PMT_PID - 1;
            }
        }
    }
    /* Now sort according to pids for fast search later on */
    qsort( (void *)p_sys->pmtmap, p_sys->i_pmtslots,
            sizeof(pmt_map_t), pmtcompare );
    free(muxpmt);

    unsigned short subi[3];
    vlc_rand_bytes(subi, sizeof(subi));
    p_sys->i_pat_version_number = nrand48(subi) & 0x1f;

    vlc_value_t val,val2;
    var_Get( p_mux, SOUT_CFG_PREFIX "tsid", &val );
    if ( val.i_int )
        p_sys->i_tsid = val.i_int;
    else
        p_sys->i_tsid = nrand48(subi) & 0xffff;

    var_Get( p_mux, SOUT_CFG_PREFIX "netid", &val );
    if ( val.i_int )
        p_sys->sdt.i_netid = val.i_int;
    else
        p_sys->sdt.i_netid = 0xff00 | ( nrand48(subi) & 0xfa );

    p_sys->i_pmt_version_number = nrand48(subi) & 0x1f;
    p_sys->sdt.ts.i_pid = 0x11;

    char *sdtdesc = var_GetNonEmptyString( p_mux, SOUT_CFG_PREFIX "sdtdesc" );

    /* Syntax is provider_sdt1,service_name_sdt1,provider_sdt2,service_name_sdt2... */
    if( sdtdesc )
    {
        char *psz_sdttoken = sdtdesc;

        for (int i = 0; i < MAX_SDT_DESC * 2 && psz_sdttoken; i++)
        {
            char *psz_end = strchr( psz_sdttoken, ',' );
            if ( psz_end )
                *psz_end++ = '\0';

            if (i % 2)
                p_sys->sdt.desc[i/2].psz_service_name = FromLocaleDup( psz_sdttoken );
            else
                p_sys->sdt.desc[i/2].psz_provider = FromLocaleDup( psz_sdttoken );

            psz_sdttoken = psz_end;
        }
        free(sdtdesc);
    }

    p_sys->b_data_alignment = var_GetBool( p_mux, SOUT_CFG_PREFIX "alignment" );

    char *pgrpmt = var_GetNonEmptyString(p_mux, SOUT_CFG_PREFIX "program-pmt");
    if( pgrpmt )
    {
        char *psz = pgrpmt;
        char *psz_next = psz;

        for (int i = 0; psz; )
        {
            uint16_t i_pid = strtoul( psz, &psz_next, 0 );
            if( psz_next[0] != '\0' )
                psz = &psz_next[1];
            else
                psz = NULL;

            if( i_pid == 0 )
            {
                if( i >= MAX_PMT )
                    msg_Err( p_mux, "Number of PMTs > maximum (%d)", MAX_PMT );
            }
            else
            {
                p_sys->i_pmt_program_number[i] = i_pid;
                i++;
            }
        }
        free(pgrpmt);
    }
    else
    {
        /* Option not specified, use 1, 2, 3... */
        for (unsigned i = 0; i < p_sys->i_num_pmt; i++ )
            p_sys->i_pmt_program_number[i] = i + 1;
    }

    var_Get( p_mux, SOUT_CFG_PREFIX "pid-pmt", &val );
    for (unsigned i = 0; i < p_sys->i_num_pmt; i++ )
        p_sys->pmt[i].i_pid = val.i_int + i;

    p_sys->i_pid_video = var_GetInteger( p_mux, SOUT_CFG_PREFIX "pid-video" );
    p_sys->i_pid_audio = var_GetInteger( p_mux, SOUT_CFG_PREFIX "pid-audio" );
    p_sys->i_pid_spu = var_GetInteger( p_mux, SOUT_CFG_PREFIX "pid-spu" );

    /* Allow to create constrained stream */
    p_sys->i_bitrate_min = var_GetInteger( p_mux, SOUT_CFG_PREFIX "bmin" );

    p_sys->i_bitrate_max = var_GetInteger( p_mux, SOUT_CFG_PREFIX "bmax" );

    if( p_sys->i_bitrate_min > 0 && p_sys->i_bitrate_max > 0 &&
        p_sys->i_bitrate_min > p_sys->i_bitrate_max )
    {
        msg_Err( p_mux, "incompatible minimum and maximum bitrate, "
                 "disabling bitrate control" );
        p_sys->i_bitrate_min = 0;
        p_sys->i_bitrate_max = 0;
    }
    if( p_sys->i_bitrate_min > 0 || p_sys->i_bitrate_max > 0 )
    {
        msg_Err( p_mux, "bmin and bmax no more supported "
                 "(if you need them report it)" );
    }

    var_Get( p_mux, SOUT_CFG_PREFIX "shaping", &val );
    if( val.i_int <= 0 )
    {
        msg_Err( p_mux,
                 "invalid shaping (%"PRId64"ms) resetting to 200ms",
                 val.i_int );
        p_sys->i_shaping_delay = VLC_TICK_FROM_MS(200);
    }
    else
    {
        p_sys->i_shaping_delay = VLC_TICK_FROM_MS(val.i_int);
    }

    var_Get( p_mux, SOUT_CFG_PREFIX "pcr", &val2 );
    if( val2.i_int <= 0 || val2.i_int >= val.i_int )
    {
        msg_Err( p_mux,
                 "invalid pcr delay (%"PRId64"ms) resetting to 70ms",
                 val2.i_int );
        p_sys->i_pcr_delay = VLC_TICK_FROM_MS(70);
    }
    else
    {
        p_sys->i_pcr_delay = VLC_TICK_FROM_MS(val2.i_int);
    }

    var_Get( p_mux, SOUT_CFG_PREFIX "dts-delay", &val );
    p_sys->i_dts_delay = VLC_TICK_FROM_MS(val.i_int);

    msg_Dbg( p_mux, "shaping=%"PRId64" pcr=%"PRId64" dts_delay=%"PRId64,
             p_sys->i_shaping_delay, p_sys->i_pcr_delay, p_sys->i_dts_delay );

    p_sys->b_use_key_frames = var_GetBool( p_mux, SOUT_CFG_PREFIX "use-key-frames" );

    p_mux->p_sys        = p_sys;

    p_sys->csa = csaSetup(p_this);

    p_mux->pf_control   = Control;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_mux_t          *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t      *p_sys = p_mux->p_sys;

    if( p_sys->p_dvbpsi )
        dvbpsi_delete( p_sys->p_dvbpsi );

    if( p_sys->csa )
    {
        var_DelCallback( p_mux, SOUT_CFG_PREFIX "csa-ck", ChangeKeyCallback, p_mux );
        var_DelCallback( p_mux, SOUT_CFG_PREFIX "csa2-ck", ChangeKeyCallback, NULL );
        var_DelCallback( p_mux, SOUT_CFG_PREFIX "csa-use", ActiveKeyCallback, NULL );
        csa_Delete( p_sys->csa );
    }

    for (int i = 0; i < MAX_SDT_DESC; i++ )
    {
        free( p_sys->sdt.desc[i].psz_service_name );
        free( p_sys->sdt.desc[i].psz_provider );
    }

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
    sout_mux_t      *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    int ret;

    vlc_mutex_lock(&p_sys->csa_lock);
    ret = csa_SetCW(p_this, p_sys->csa, newval.psz_string, p_data != NULL);
    vlc_mutex_unlock(&p_sys->csa_lock);

    return ret;
}

/*****************************************************************************
 * ActiveKeyCallback: called when changing the active (in use) encryption key on the fly.
 *****************************************************************************/
static int ActiveKeyCallback( vlc_object_t *p_this, char const *psz_cmd,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);
    sout_mux_t      *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    int             i_res, use_odd = -1;

    if( !strcmp(newval.psz_string, "odd" ) ||
        !strcmp(newval.psz_string, "first" ) ||
        !strcmp(newval.psz_string, "1" ) )
    {
        use_odd = 1;
    }
    else if( !strcmp(newval.psz_string, "even" ) ||
             !strcmp(newval.psz_string, "second" ) ||
             !strcmp(newval.psz_string, "2" ) )
    {
        use_odd = 0;
    }

    if (use_odd < 0)
        return VLC_EBADVAR;

    vlc_mutex_lock( &p_sys->csa_lock );
    i_res = csa_UseKey( p_this, p_sys->csa, use_odd );
    vlc_mutex_unlock( &p_sys->csa_lock );

    return i_res;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( sout_mux_t *p_mux, int i_query, va_list args )
{
    VLC_UNUSED(p_mux);
    bool *pb_bool;
    char **ppsz;

    switch( i_query )
    {
    case MUX_CAN_ADD_STREAM_WHILE_MUXING:
        pb_bool = va_arg( args, bool * );
        *pb_bool = true;
        return VLC_SUCCESS;

    case MUX_GET_MIME:
        ppsz = va_arg( args, char ** );
        *ppsz = strdup( "video/mp2t" );
        return VLC_SUCCESS;

    default:
        return VLC_EGENERIC;
    }
}

/* returns a pointer to a valid string, with length 0 or 3 */
static const char *GetIso639_2LangCode(const char *lang)
{
    const iso639_lang_t *pl;

    if (strlen(lang) == 2)
    {
        pl = GetLang_1(lang);
    }
    else
    {
        pl = GetLang_2B(lang);      /* try native code first */
        if (!*pl->psz_iso639_2T)
            pl = GetLang_2T(lang);  /* else fallback to english code */

    }

    return pl->psz_iso639_2T;   /* returns the english code */
}

static void SelectPCRStream( sout_mux_t *p_mux, sout_input_t *p_removed_pcr_input )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    /* Find a new pcr stream (Prefer Video Stream) */
    if( p_removed_pcr_input != NULL )
        p_sys->p_pcr_input = NULL;

    for ( int i = 0; i < p_mux->i_nb_inputs; i++ )
    {
        sout_input_t *p_input = p_mux->pp_inputs[i];
        if( p_input == p_removed_pcr_input )
            continue;

        if( p_input->p_fmt->i_cat == VIDEO_ES &&
           (p_sys->p_pcr_input == NULL ||
            p_sys->p_pcr_input->p_fmt->i_cat != VIDEO_ES) )
        {
            p_sys->p_pcr_input = p_input;
            break;
        }
        else if( p_input->p_fmt->i_cat != SPU_ES &&
                 p_sys->p_pcr_input == NULL )
        {
            p_sys->p_pcr_input = p_input;
        }
    }

    if( p_sys->p_pcr_input )
    {
        /* Empty TS buffer */
        /* FIXME */
        msg_Dbg( p_mux, "new PCR PID is %d",
                 ((sout_input_sys_t *)p_sys->p_pcr_input->p_sys)->ts.i_pid );
    }

}

/*****************************************************************************
 * AddStream: called for each stream addition
 *****************************************************************************/
static int AddStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t      *p_sys = p_mux->p_sys;
    sout_input_sys_t    *p_stream;

    p_input->p_sys = p_stream = calloc( 1, sizeof( sout_input_sys_t ) );
    if( !p_stream )
        goto oom;

    if ( p_sys->b_es_id_pid )
        p_stream->ts.i_pid = p_input->fmt.i_id & 0x1fff;
    else
        p_stream->ts.i_pid = AllocatePID( p_mux, p_input->p_fmt->i_cat );

    if( FillPMTESParams( p_sys->standard, p_input->p_fmt,
                        &p_stream->ts, &p_stream->pes ) != VLC_SUCCESS )
    {
        msg_Warn( p_mux, "rejecting stream with unsupported codec %4.4s",
                  (char*)&p_input->p_fmt->i_codec );
        free( p_stream );
        return VLC_EGENERIC;
    }

    p_stream->pes.i_langs = 1 + p_input->p_fmt->i_extra_languages;
    p_stream->pes.lang = calloc(1, p_stream->pes.i_langs * 4);
    if( !p_stream->pes.lang )
        goto oom;

    msg_Dbg( p_mux, "adding input codec=%4.4s pid=%d",
             (char*)&p_input->fmt.i_codec, p_stream->ts.i_pid );

    for (size_t i = 0; i < p_stream->pes.i_langs; i++) {
        char *lang = (i == 0)
            ? p_input->p_fmt->psz_language
            : p_input->p_fmt->p_extra_languages[i-1].psz_language;

        if (!lang)
            continue;

        const char *code = GetIso639_2LangCode(lang);
        if (*code)
        {
            memcpy(&p_stream->pes.lang[i*4], code, 3);
            p_stream->pes.lang[i*4+3] = 0x00; /* audio type: 0x00 undefined */
            msg_Dbg( p_mux, "    - lang=%3.3s", &p_stream->pes.lang[i*4] );
        }
    }

    /* Init pes chain */
    BufferChainInit( &p_stream->state.chain_pes );

    /* We only change PMT version (PAT isn't changed) */
    p_sys->i_pmt_version_number = ( p_sys->i_pmt_version_number + 1 )%32;

    /* Update pcr_pid */
    SelectPCRStream( p_mux, NULL );

    return VLC_SUCCESS;

oom:
    if(p_stream)
    {
        free(p_stream->pes.lang);
        free(p_stream);
    }
    return VLC_ENOMEM;
}

/*****************************************************************************
 * DelStream: called before a stream deletion
 *****************************************************************************/
static void DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t   *p_sys = p_mux->p_sys;
    sout_input_sys_t *p_stream = (sout_input_sys_t*)p_input->p_sys;
    int               pid;

    msg_Dbg( p_mux, "removing input pid=%d", p_stream->ts.i_pid );

    if( p_sys->p_pcr_input == p_input )
    {
        /* Repick new PCR */
        SelectPCRStream( p_mux, p_input );
    }

    /* Empty all data in chain_pes */
    BufferChainClean( &p_stream->state.chain_pes );

    pid = var_GetInteger( p_mux, SOUT_CFG_PREFIX "pid-video" );
    if ( pid > 0 && pid == p_stream->ts.i_pid )
    {
        p_sys->i_pid_video = pid;
        msg_Dbg( p_mux, "freeing video PID %d", pid);
    }
    pid = var_GetInteger( p_mux, SOUT_CFG_PREFIX "pid-audio" );
    if ( pid > 0 && pid == p_stream->ts.i_pid )
    {
        p_sys->i_pid_audio = pid;
        msg_Dbg( p_mux, "freeing audio PID %d", pid);
    }
    pid = var_GetInteger( p_mux, SOUT_CFG_PREFIX "pid-spu" );
    if ( pid > 0 && pid == p_stream->ts.i_pid )
    {
        p_sys->i_pid_spu = pid;
        msg_Dbg( p_mux, "freeing spu PID %d", pid);
    }

    free(p_stream->pes.lang);
    free( p_stream );

    /* We only change PMT version (PAT isn't changed) */
    p_sys->i_pmt_version_number++;
    p_sys->i_pmt_version_number %= 32;
}

static void SetHeader( sout_buffer_chain_t *c,
                        int depth )
{
    block_t *p_ts = BufferChainPeek( c );
    while( depth > 0 )
    {
        p_ts = p_ts->p_next;
        depth--;
    }
    p_ts->i_flags |= BLOCK_FLAG_HEADER;
}

static block_t *Pack_Opus(block_t *p_data)
{
    lldiv_t d = lldiv(p_data->i_buffer, 255);
    p_data = block_Realloc(p_data, 2 + d.quot + 1, p_data->i_buffer);
    if (p_data) { /* no flags */
        p_data->p_buffer[0] = 0x7f;
        p_data->p_buffer[1] = 0xe0;
        memset(&p_data->p_buffer[2], 0xff, d.quot);
        p_data->p_buffer[2+d.quot] = d.rem;
    }

    return p_data;
}

static void SetBlockDuration( sout_input_t *p_input, block_t *p_data )
{
    sout_input_sys_t *p_stream = (sout_input_sys_t*) p_input->p_sys;

    if( p_input->p_fmt->i_cat != SPU_ES &&
        block_FifoCount( p_input->p_fifo ) > 0 )
    {
        block_t *p_next = block_FifoShow( p_input->p_fifo );
        vlc_tick_t i_diff = p_next->i_dts - p_data->i_dts;
        if( i_diff > 0 &&
                (p_next->i_flags & BLOCK_FLAG_DISCONTINUITY) == 0 )
        {
            p_data->i_length = i_diff;
        }
        else if ( p_data->i_length == 0 )
        {
            /* Try rates */
            if( p_input->p_fmt->i_cat == VIDEO_ES &&
                p_input->p_fmt->video.i_frame_rate &&
                p_input->p_fmt->video.i_frame_rate_base )
            {
                p_data->i_length = vlc_tick_from_samples(
                                   p_input->p_fmt->video.i_frame_rate,
                                   p_input->p_fmt->video.i_frame_rate_base);
            }
            else if( p_input->p_fmt->i_cat == AUDIO_ES &&
                     p_input->p_fmt->audio.i_bytes_per_frame &&
                     p_input->p_fmt->audio.i_frame_length )
            {
                p_data->i_length = p_data->i_buffer *
                                   p_input->p_fmt->audio.i_frame_length /
                                   p_input->p_fmt->audio.i_bytes_per_frame;
            }
            /* Try a previous duration */
            else if( p_stream->state.chain_pes.p_first )
                p_data->i_length = p_stream->state.chain_pes.p_first->i_length;
            /* Or next */
            else if( p_next->i_length > 0 )
                p_data->i_length = p_next->i_length;
            /* or worse */
            else
                p_data->i_length = VLC_TICK_FROM_MS(1);
        }
    }
    else if( p_input->p_fmt->i_codec != VLC_CODEC_SUBT )
    {
        p_data->i_length = VLC_TICK_FROM_MS(1);
    }
}

static block_t *Encap_J2K( block_t *p_data, const es_format_t *p_fmt )
{
    size_t i_offset = 0;
    uint32_t i_box = 0;
    while( p_data->i_buffer > 8 && p_data->i_buffer - i_offset > 8 )
    {
        const uint32_t i_size = GetDWBE( &p_data->p_buffer[i_offset] );
        i_box = VLC_FOURCC( p_data->p_buffer[i_offset + 4],
                            p_data->p_buffer[i_offset + 5],
                            p_data->p_buffer[i_offset + 6],
                            p_data->p_buffer[i_offset + 7] );
        if( p_data->i_buffer - i_offset < i_size || i_size < 8 )
        {
            i_box = 0;
            break;
        }
        else if( i_box == J2K_BOX_JP2C )
        {
            break;
        }

        i_offset += i_size;
    }

    if( i_box != J2K_BOX_JP2C )
    {
        block_Release( p_data );
        return NULL;
    }

    if( i_offset < 38 )
    {
        block_t *p_realloc = block_Realloc( p_data, 38 - i_offset, p_data->i_buffer );
        if( unlikely(!p_realloc) )
        {
            block_Release( p_data );
            return NULL;
        }
        p_data = p_realloc;
    }
    else
    {
        p_data->p_buffer += (i_offset - 38);
        p_data->i_buffer -= (i_offset - 38);
    }

    const int profile = j2k_get_profile( p_fmt->video.i_visible_width,
                                         p_fmt->video.i_visible_height,
                                         p_fmt->video.i_frame_rate,
                                         p_fmt->video.i_frame_rate_base, true );
    memcpy(  p_data->p_buffer,     "elsmfrat", 8 );
    SetWBE( &p_data->p_buffer[8],  p_fmt->video.i_frame_rate_base );
    SetWBE( &p_data->p_buffer[10], p_fmt->video.i_frame_rate );
    memcpy( &p_data->p_buffer[12], "brat", 4 );
    unsigned min = j2k_profiles_rates[profile].min * 1000000;
    unsigned max = j2k_profiles_rates[profile].max * 1000000;
    SetDWBE(&p_data->p_buffer[16], max );
    SetDWBE(&p_data->p_buffer[20], min );
    memcpy( &p_data->p_buffer[24], "tcod", 4 );
    const unsigned s = SEC_FROM_VLC_TICK(p_data->i_pts);
    const unsigned m = s / 60;
    const unsigned h = m / 60;
    const uint64_t l = p_fmt->video.i_frame_rate_base * CLOCK_FREQ /
                       p_fmt->video.i_frame_rate;
    const unsigned f = (p_data->i_pts % CLOCK_FREQ) / l;
    p_data->p_buffer[28] = h;
    p_data->p_buffer[29] = m % 60;
    p_data->p_buffer[30] = s % 60;
    p_data->p_buffer[31] = f;
    memcpy( &p_data->p_buffer[32], "bcol", 4 );
    p_data->p_buffer[36] = j2k_get_color_spec( p_fmt->video.primaries,
                                               p_fmt->video.transfer,
                                               p_fmt->video.space );
    p_data->p_buffer[37] = 0;

    return p_data;
}

/* returns true if needs more data */
static bool MuxStreams(sout_mux_t *p_mux )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    sout_input_sys_t *p_pcr_stream = (sout_input_sys_t*)p_sys->p_pcr_input->p_sys;

    sout_buffer_chain_t chain_ts;
    vlc_tick_t i_shaping_delay = p_pcr_stream->state.b_key_frame
        ? p_pcr_stream->state.i_pes_length
        : p_sys->i_shaping_delay;

    bool b_ok = true;

    /* Accumulate enough data in the pcr stream (>i_shaping_delay) */
    /* Accumulate enough data in all other stream ( >= length of pcr)*/
    for (int i = -1; !b_ok || i < p_mux->i_nb_inputs; i++ )
    {
        if (i == p_mux->i_nb_inputs)
        {
            /* get enough PES packet for all input */
            b_ok = true;
            i = -1;
        }
        sout_input_t *p_input;

        if( i == -1 )
            p_input = p_sys->p_pcr_input;
        else if( p_mux->pp_inputs[i] != p_sys->p_pcr_input )
            p_input = p_mux->pp_inputs[i];
        else
            continue;

        sout_input_sys_t *p_stream = (sout_input_sys_t*)p_input->p_sys;

        if( ( p_stream != p_pcr_stream ||
              p_stream->state.i_pes_length >= i_shaping_delay ) &&
            p_stream->state.i_pes_dts + p_stream->state.i_pes_length >=
            p_pcr_stream->state.i_pes_dts + p_pcr_stream->state.i_pes_length )
            continue;

        /* Need more data */
        if( block_FifoCount( p_input->p_fifo ) <= 1 )
        {
            if( ( p_input->p_fmt->i_cat == AUDIO_ES ) ||
                ( p_input->p_fmt->i_cat == VIDEO_ES ) )
            {
                /* We need more data */
                return true;
            }
            else if( block_FifoCount( p_input->p_fifo ) <= 0 )
            {
                /* spu, only one packet is needed */
                continue;
            }
            else if( p_input->p_fmt->i_cat == SPU_ES )
            {
                /* Don't mux the SPU yet if it is too early */
                block_t *p_spu = block_FifoShow( p_input->p_fifo );

                int64_t i_spu_delay = p_spu->i_dts - p_pcr_stream->state.i_pes_dts;
                if( ( i_spu_delay > i_shaping_delay ) &&
                    ( i_spu_delay < VLC_TICK_FROM_SEC(100)) )
                    continue;

                if ( ( i_spu_delay >= VLC_TICK_FROM_SEC(100)) ||
                     ( i_spu_delay < VLC_TICK_FROM_MS(10) ) )
                {
                    BufferChainClean( &p_stream->state.chain_pes );
                    p_stream->state.i_pes_dts = 0;
                    p_stream->state.i_pes_used = 0;
                    p_stream->state.i_pes_length = 0;
                    continue;
                }
            }
        }
        b_ok = false;

        block_t *p_data;
        if( p_stream == p_pcr_stream || p_sys->b_data_alignment
             || ((p_input->p_fmt->i_codec != VLC_CODEC_MPGA ) &&
                 (p_input->p_fmt->i_codec != VLC_CODEC_MP3) ) )
        {
            p_data = block_FifoGet( p_input->p_fifo );
            if( p_data->i_dts == VLC_TICK_INVALID )
                p_data->i_dts = p_data->i_pts;
            else if ( p_data->i_pts == VLC_TICK_INVALID )
                p_data->i_pts = p_data->i_dts;

            if( p_input->p_fmt->i_codec == VLC_CODEC_MP4A )
                p_data = Add_ADTS( p_data, p_input->p_fmt );
            else if( p_input->p_fmt->i_codec == VLC_CODEC_OPUS )
                p_data = Pack_Opus( p_data );
        }
        else
            p_data = FixPES( p_mux, p_input->p_fifo );

        SetBlockDuration( p_input, p_data );

        if( p_data->i_dts == VLC_TICK_INVALID )
        {
            msg_Err( p_mux, "non dated packet dropped" );
            block_Release( p_data );
            continue;
        }

        if ( p_sys->first_dts == 0 )
        {
            /* Pick the really first DTS */
            p_sys->first_dts = p_data->i_dts;
            for (int j = 0; j < p_mux->i_nb_inputs; j++ )
            {
                if( p_mux->pp_inputs[j] != p_input &&
                    block_FifoCount( p_mux->pp_inputs[j]->p_fifo) > 0 )
                {
                    block_t *p_block = block_FifoShow( p_mux->pp_inputs[j]->p_fifo );
                    if( p_block->i_dts != VLC_TICK_INVALID &&
                        p_block->i_dts < p_sys->first_dts )
                        p_sys->first_dts = p_block->i_dts;
                }
            }
        }

        if( ( p_pcr_stream->state.i_pes_dts > 0 &&
              p_data->i_dts - VLC_TICK_FROM_SEC(10)> p_pcr_stream->state.i_pes_dts +
              p_pcr_stream->state.i_pes_length ) ||
            p_data->i_dts + i_shaping_delay < p_stream->state.i_pes_dts ||
            ( p_stream->state.i_pes_dts > 0 &&
              p_input->p_fmt->i_cat != SPU_ES &&
              p_data->i_dts - VLC_TICK_FROM_SEC(10)> p_stream->state.i_pes_dts +
              p_stream->state.i_pes_length ) )
        {
            msg_Warn( p_mux, "packet with too strange dts on pid %d (%4.4s)"
                      "(dts=%"PRId64",old=%"PRId64",pcr=%"PRId64")",
                      p_stream->ts.i_pid, (char *) &p_input->fmt.i_codec,
                      p_data->i_dts, p_stream->state.i_pes_dts,
                      p_pcr_stream->state.i_pes_dts );
            block_Release( p_data );

            BufferChainClean( &p_stream->state.chain_pes );
            p_stream->state.i_pes_dts = 0;
            p_stream->state.i_pes_used = 0;
            p_stream->state.i_pes_length = 0;

            if( p_input->p_fmt->i_cat != SPU_ES )
            {
                BufferChainClean( &p_pcr_stream->state.chain_pes );
                p_pcr_stream->state.i_pes_dts = 0;
                p_pcr_stream->state.i_pes_used = 0;
                p_pcr_stream->state.i_pes_length = 0;
            }

            continue;
        }

        int i_header_size = 0;
        int i_max_pes_size = 0;
        int b_data_alignment = 0;
        if( p_input->p_fmt->i_cat == SPU_ES ) switch (p_input->p_fmt->i_codec)
        {
        case VLC_CODEC_SUBT:
            /* Prepend header */
            p_data = block_Realloc( p_data, 2, p_data->i_buffer );
            p_data->p_buffer[0] = ( (p_data->i_buffer - 2) >> 8) & 0xff;
            p_data->p_buffer[1] = ( (p_data->i_buffer - 2)     ) & 0xff;

            /* remove trailling \0 if any */
            if( p_data->i_buffer > 2 && !p_data->p_buffer[p_data->i_buffer-1] )
                p_data->i_buffer--;

            /* Append a empty sub (sub text only) */
            if( p_data->i_length > 0 &&
                ( p_data->i_buffer != 1 || *p_data->p_buffer != ' ' ) )
            {
                block_t *p_spu = block_Alloc( 3 );

                p_spu->i_dts = p_data->i_dts + p_data->i_length;
                p_spu->i_pts = p_spu->i_dts;
                p_spu->i_length = VLC_TICK_FROM_MS(1);

                p_spu->p_buffer[0] = 0;
                p_spu->p_buffer[1] = 1;
                p_spu->p_buffer[2] = ' ';

                EStoPES( &p_spu, p_input->p_fmt,
                         p_stream->pes.i_stream_id, 1, 0, 0, 0,
                         p_sys->first_dts - p_sys->i_dts_delay );
                p_data->p_next = p_spu;
            }
            break;

        case VLC_CODEC_TELETEXT:
            /* EN 300 472 */
            i_header_size = 0x24;
            b_data_alignment = 1;
            break;

        case VLC_CODEC_DVBS:
            /* EN 300 743 */
            b_data_alignment = 1;
            break;
        }
        else if( p_input->fmt.i_cat == VIDEO_ES )
        {
            if( p_input->fmt.i_codec == VLC_CODEC_JPEG2000 )
            {
                if( p_data->i_flags & BLOCK_FLAG_INTERLACED_MASK )
                    msg_Warn( p_mux, "Unsupported interlaced J2K content. Expect broken result");
                p_data = Encap_J2K( p_data, &p_input->fmt );
                if( !p_data )
                    return false;
            }
        }
        else if( p_data->i_length < 0 || p_data->i_length > VLC_TICK_FROM_SEC(2) )
        {
            /* FIXME choose a better value, but anyway we
             * should never have to do that */
            p_data->i_length = VLC_TICK_FROM_MS(1);
        }

        p_stream->state.i_pes_length += p_data->i_length;
        if( p_stream->state.i_pes_dts == 0 )
        {
            p_stream->state.i_pes_dts = p_data->i_dts;
        }

        /* Convert to pes */
        if( p_stream->pes.i_stream_id == 0xa0 && p_data->i_pts <= 0 )
        {
            /* XXX yes I know, it's awful, but it's needed,
             * so don't remove it ... */
            p_data->i_pts = p_data->i_dts;
        }

        if( (p_input->p_fmt->i_codec == VLC_CODEC_DIRAC) ||
            (p_input->p_fmt->i_codec == VLC_CODEC_H264) ||
            (p_input->p_fmt->i_codec == VLC_CODEC_HEVC) ||
            (p_input->p_fmt->i_codec == VLC_CODEC_MP2V)
          )
        {
            b_data_alignment = 1;
            /* dirac and mpeg video pes packets should be unbounded in
             * length, specify a suitibly large max size */
            i_max_pes_size = INT_MAX;
        }

        EStoPES ( &p_data, p_input->p_fmt, p_stream->pes.i_stream_id,
                       1, b_data_alignment, i_header_size,
                       i_max_pes_size, p_sys->first_dts - p_sys->i_dts_delay );

        BufferChainAppend( &p_stream->state.chain_pes, p_data );

        if( p_sys->b_use_key_frames && p_stream == p_pcr_stream
            && (p_data->i_flags & BLOCK_FLAG_TYPE_I)
            && !(p_data->i_flags & BLOCK_FLAG_NO_KEYFRAME)
            && (p_stream->state.i_pes_length > VLC_TICK_FROM_MS(400)) )
        {
            i_shaping_delay = p_stream->state.i_pes_length;
            p_stream->state.b_key_frame = 1;
        }
    }

    /* save */
    const vlc_tick_t i_pcr_length = p_pcr_stream->state.i_pes_length;
    p_pcr_stream->state.b_key_frame = 0;

    /* msg_Dbg( p_mux, "starting muxing %lldms", i_pcr_length / 1000 ); */
    /* 2: calculate non accurate total size of muxed ts */
    int i_packet_count = 0;
    for (int i = 0; i < p_mux->i_nb_inputs; i++ )
    {
        sout_input_sys_t *p_stream = (sout_input_sys_t*)p_mux->pp_inputs[i]->p_sys;

        /* False for pcr stream but it will be enough to do PCR algo */
        for (block_t *p_pes = p_stream->state.chain_pes.p_first; p_pes != NULL;
             p_pes = p_pes->p_next )
        {
            int i_size = p_pes->i_buffer;
            vlc_tick_t i_frag = p_pcr_stream->state.i_pes_dts +
                             p_pcr_stream->state.i_pes_length - p_pes->i_dts;
            if( p_pes->i_length > i_frag )
            {
                if( i_frag < 0 )
                {
                    /* Next stream */
                    break;
                }
                i_size = p_pes->i_buffer * i_frag / p_pes->i_length;
            }
            i_packet_count += ( i_size + 183 ) / 184;
        }
    }
    /* add overhead for PCR (not really exact) */
    i_packet_count += (8 * i_pcr_length / p_sys->i_pcr_delay + 175) / 176;

    /* 3: mux PES into TS */
    BufferChainInit( &chain_ts );
    /* append PAT/PMT  -> FIXME with big pcr delay it won't have enough pat/pmt */
    bool pat_was_previous = true; //This is to prevent unnecessary double PAT/PMT insertions
    GetPAT( p_mux, &chain_ts );
    GetPMT( p_mux, &chain_ts );
    int i_packet_pos = 0;
    i_packet_count += chain_ts.i_depth;
    /* msg_Dbg( p_mux, "estimated pck=%d", i_packet_count ); */

    const vlc_tick_t i_pcr_dts = p_pcr_stream->state.i_pes_dts;
    for (;;)
    {
        int          i_stream = -1;
        vlc_tick_t   i_dts = 0;
        sout_input_sys_t *p_stream;

        /* Select stream (lowest dts) */
        for (int i = 0; i < p_mux->i_nb_inputs; i++ )
        {
            p_stream = (sout_input_sys_t*)p_mux->pp_inputs[i]->p_sys;

            if( p_stream->state.i_pes_dts == 0 )
            {
                continue;
            }

            if( i_stream == -1 || p_stream->state.i_pes_dts < i_dts )
            {
                i_stream = i;
                i_dts = p_stream->state.i_pes_dts;
            }
        }
        if( i_stream == -1 || i_dts > i_pcr_dts + i_pcr_length )
        {
            break;
        }
        p_stream = (sout_input_sys_t*)p_mux->pp_inputs[i_stream]->p_sys;
        sout_input_t *p_input = p_mux->pp_inputs[i_stream];

        /* do we need to issue pcr */
        bool b_pcr = false;
        vlc_tick_t packet_length = i_pcr_length * i_packet_pos / i_packet_count;
        if( p_stream == p_pcr_stream &&
            i_pcr_dts + packet_length >=
            p_sys->i_pcr + p_sys->i_pcr_delay )
        {
            b_pcr = true;
            p_sys->i_pcr = i_pcr_dts + packet_length;
        }

        /* Build the TS packet */
        block_t *p_ts = TSNew( p_mux, p_stream, b_pcr );
        if( p_sys->csa != NULL &&
             (p_input->p_fmt->i_cat != AUDIO_ES || p_sys->b_crypt_audio) &&
             (p_input->p_fmt->i_cat != VIDEO_ES || p_sys->b_crypt_video) )
        {
            p_ts->i_flags |= BLOCK_FLAG_SCRAMBLED;
        }
        i_packet_pos++;

        /* Write PAT/PMT before every keyframe if use-key-frames is enabled,
         * this helps to do segmenting with livehttp-output so it can cut segment
         * and start new one with pat,pmt,keyframe*/
        if( ( p_sys->b_use_key_frames ) &&
            ( p_input->p_fmt->i_cat == VIDEO_ES ) &&
            ( p_ts->i_flags & BLOCK_FLAG_TYPE_I ) )
        {
            if( likely( !pat_was_previous ) )
            {
                int startcount = chain_ts.i_depth;
                GetPAT( p_mux, &chain_ts );
                GetPMT( p_mux, &chain_ts );
                SetHeader( &chain_ts, startcount );
                i_packet_count += (chain_ts.i_depth - startcount );
            } else {
                SetHeader( &chain_ts, 0); //We just inserted pat/pmt,so just flag it instead of adding new one
            }
        }
        pat_was_previous = false;

        /* */
        BufferChainAppend( &chain_ts, p_ts );
    }

    /* 4: date and send */
    TSSchedule( p_mux, &chain_ts, i_pcr_length, i_pcr_dts );
    return false;
}

/*****************************************************************************
 * Mux: Call each time there is new data for at least one stream
 *****************************************************************************
 *
 *****************************************************************************/
static int Mux( sout_mux_t *p_mux )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;

    if( p_sys->p_pcr_input == NULL )
    {
        for (int i = 0; i < p_mux->i_nb_inputs; i++ )
        {
            block_FifoEmpty( p_mux->pp_inputs[i]->p_fifo );
        }
        msg_Dbg( p_mux, "waiting for PCR streams" );
        return VLC_SUCCESS;
    }

    while (!MuxStreams(p_mux))
        ;
    return VLC_SUCCESS;
}

#define STD_PES_PAYLOAD 170
static block_t *FixPES( sout_mux_t *p_mux, block_fifo_t *p_fifo )
{
    VLC_UNUSED(p_mux);
    block_t *p_data;
    size_t i_size;

    p_data = block_FifoShow( p_fifo );
    i_size = p_data->i_buffer;

    if( i_size == STD_PES_PAYLOAD )
    {
        return block_FifoGet( p_fifo );
    }
    else if( i_size > STD_PES_PAYLOAD )
    {
        block_t *p_new = block_Alloc( STD_PES_PAYLOAD );
        memcpy( p_new->p_buffer, p_data->p_buffer, STD_PES_PAYLOAD );
        p_new->i_pts = p_data->i_pts;
        p_new->i_dts = p_data->i_dts;
        p_new->i_length = p_data->i_length * STD_PES_PAYLOAD
                            / p_data->i_buffer;
        p_data->i_buffer -= STD_PES_PAYLOAD;
        p_data->p_buffer += STD_PES_PAYLOAD;
        p_data->i_pts += p_new->i_length;
        p_data->i_dts += p_new->i_length;
        p_data->i_length -= p_new->i_length;
        p_data->i_flags |= BLOCK_FLAG_NO_KEYFRAME;
        return p_new;
    }
    else
    {
        block_t *p_next;
        int i_copy;

        p_data = block_FifoGet( p_fifo );
        p_data = block_Realloc( p_data, 0, STD_PES_PAYLOAD );
        p_next = block_FifoShow( p_fifo );
        if ( p_data->i_flags & BLOCK_FLAG_NO_KEYFRAME )
        {
            p_data->i_flags &= ~BLOCK_FLAG_NO_KEYFRAME;
            p_data->i_pts = p_next->i_pts;
            p_data->i_dts = p_next->i_dts;
        }
        i_copy = __MIN( STD_PES_PAYLOAD - i_size, p_next->i_buffer );

        memcpy( &p_data->p_buffer[i_size], p_next->p_buffer, i_copy );
        vlc_tick_t offset = p_next->i_length * i_copy / p_next->i_buffer;
        if( p_next->i_pts )
            p_next->i_pts += offset;
        if( p_next->i_dts )
            p_next->i_dts += offset;
        p_next->i_length -= offset;
        p_next->i_buffer -= i_copy;
        p_next->p_buffer += i_copy;
        p_next->i_flags |= BLOCK_FLAG_NO_KEYFRAME;

        if( !p_next->i_buffer )
        {
            p_next = block_FifoGet( p_fifo );
            block_Release( p_next );
        }
        return p_data;
    }
}

static block_t *Add_ADTS( block_t *p_data, const es_format_t *p_fmt )
{
#define ADTS_HEADER_SIZE 7 /* CRC needs 2 more bytes */

    const uint8_t *p_extra = p_fmt->p_extra;

    if( !p_data || p_fmt->i_extra < 2 || !p_extra )
        return p_data; /* no data to construct the headers */

    size_t frame_length = p_data->i_buffer + ADTS_HEADER_SIZE;
    int i_index = ( (p_extra[0] << 1) | (p_extra[1] >> 7) ) & 0x0f;
    int i_profile = (p_extra[0] >> 3) - 1; /* i_profile < 4 */

    if( i_index == 0x0f && p_fmt->i_extra < 5 )
        return p_data; /* not enough data */

    int i_channels = (p_extra[i_index == 0x0f ? 4 : 1] >> 3) & 0x0f;

    block_t *p_new_block = block_Realloc( p_data, ADTS_HEADER_SIZE,
                                            p_data->i_buffer );
    uint8_t *p_buffer = p_new_block->p_buffer;

    /* fixed header */
    p_buffer[0] = 0xff;
    p_buffer[1] = 0xf1; /* 0xf0 | 0x00 | 0x00 | 0x01 */
    p_buffer[2] = (i_profile << 6) | ((i_index & 0x0f) << 2) | ((i_channels >> 2) & 0x01) ;
    p_buffer[3] = (i_channels << 6) | ((frame_length >> 11) & 0x03);

    /* variable header (starts at last 2 bits of 4th byte) */

    int i_fullness = 0x7ff; /* 0x7ff means VBR */
    /* XXX: We should check if it's CBR or VBR, but no known implementation
     * do that, and it's a pain to calculate this field */

    p_buffer[4] = frame_length >> 3;
    p_buffer[5] = ((frame_length & 0x07) << 5) | ((i_fullness >> 6) & 0x1f);
    p_buffer[6] = ((i_fullness & 0x3f) << 2) /* | 0xfc */;

    return p_new_block;
}

static void TSSchedule( sout_mux_t *p_mux, sout_buffer_chain_t *p_chain_ts,
                        vlc_tick_t i_pcr_length, vlc_tick_t i_pcr_dts )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    sout_buffer_chain_t new_chain;
    int i_packet_count = p_chain_ts->i_depth;

    BufferChainInit( &new_chain );

    if ( unlikely(i_pcr_length <= 0) )
    {
        i_pcr_length = i_packet_count;
    }

    for (int i = 0; i < i_packet_count; i++ )
    {
        block_t *p_ts = BufferChainGet( p_chain_ts );
        vlc_tick_t i_new_dts = i_pcr_dts + i_pcr_length * i / i_packet_count;

        BufferChainAppend( &new_chain, p_ts );

        if (!p_ts->i_dts || p_ts->i_dts + p_sys->i_dts_delay * 2/3 >= i_new_dts)
            continue;

        vlc_tick_t i_max_diff = i_new_dts - p_ts->i_dts;
        vlc_tick_t i_cut_dts = p_ts->i_dts;

        while( (p_ts = BufferChainPeek( p_chain_ts )) )
        {
            i_new_dts = i_pcr_dts + i_pcr_length * i++ / i_packet_count;
            if( p_ts->i_dts >= i_pcr_dts &&
                i_new_dts - p_ts->i_dts >= i_max_diff )
               break;
            p_ts = BufferChainGet( p_chain_ts );
            BufferChainAppend( &new_chain, p_ts );
            i_max_diff = i_new_dts - p_ts->i_dts;
            i_cut_dts = p_ts->i_dts;
        }
        msg_Dbg( p_mux, "adjusting rate at %"PRId64"/%"PRId64" (%d/%d)",
                 i_cut_dts - i_pcr_dts, i_pcr_length, new_chain.i_depth,
                 p_chain_ts->i_depth );
        if ( new_chain.i_depth )
            TSDate( p_mux, &new_chain, i_cut_dts - i_pcr_dts, i_pcr_dts );
        if ( p_chain_ts->i_depth )
            TSSchedule( p_mux, p_chain_ts, i_pcr_dts + i_pcr_length - i_cut_dts,
                        i_cut_dts );
        return;
    }

    if ( new_chain.i_depth )
        TSDate( p_mux, &new_chain, i_pcr_length, i_pcr_dts );
}

static void TSDate( sout_mux_t *p_mux, sout_buffer_chain_t *p_chain_ts,
                    vlc_tick_t i_pcr_length, vlc_tick_t i_pcr_dts )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    int i_packet_count = p_chain_ts->i_depth;

    if ( likely(i_pcr_length / 1000 > 0) )
    {
        int i_bitrate = ((uint64_t)i_packet_count * 188 * 8000)
                          / MS_FROM_VLC_TICK(i_pcr_length);
        if ( p_sys->i_bitrate_max && p_sys->i_bitrate_max < i_bitrate )
        {
            msg_Warn( p_mux, "max bitrate exceeded at %"PRId64
                      " (%d bi/s for %d pkt in %"PRId64" us)",
                      i_pcr_dts + p_sys->i_shaping_delay * 3 / 2 - vlc_tick_now(),
                      i_bitrate, i_packet_count, i_pcr_length);
        }
    }
    else
    {
        /* This shouldn't happen, but happens in some rare heavy load
         * and packet losses conditions. */
        i_pcr_length = i_packet_count;
    }

    /* msg_Dbg( p_mux, "real pck=%d", i_packet_count ); */
    for (int i = 0; i < i_packet_count; i++ )
    {
        block_t *p_ts = BufferChainGet( p_chain_ts );
        vlc_tick_t i_new_dts = i_pcr_dts + i_pcr_length * i / i_packet_count;

        p_ts->i_dts    = i_new_dts;
        p_ts->i_length = i_pcr_length / i_packet_count;

        if( p_ts->i_flags & BLOCK_FLAG_CLOCK )
        {
            /* msg_Dbg( p_mux, "pcr=%lld ms", p_ts->i_dts / 1000 ); */
            TSSetPCR( p_ts, p_ts->i_dts - p_sys->first_dts );
        }
        if( p_ts->i_flags & BLOCK_FLAG_SCRAMBLED )
        {
            vlc_mutex_lock( &p_sys->csa_lock );
            csa_Encrypt( p_sys->csa, p_ts->p_buffer, p_sys->i_csa_pkt_size );
            vlc_mutex_unlock( &p_sys->csa_lock );
        }

        /* latency */
        p_ts->i_dts += p_sys->i_shaping_delay * 3 / 2;

        sout_AccessOutWrite( p_mux->p_access, p_ts );
    }
}

static block_t *TSNew( sout_mux_t *p_mux, sout_input_sys_t *p_stream,
                       bool b_pcr )
{
    VLC_UNUSED(p_mux);
    block_t *p_pes = p_stream->state.chain_pes.p_first;

    bool b_new_pes = false;
    bool b_adaptation_field = false;

    int i_payload_max = 184 - ( b_pcr ? 8 : 0 );

    if( p_stream->state.i_pes_used <= 0 )
    {
        b_new_pes = true;
    }
    int i_payload = __MIN( (int)p_pes->i_buffer - p_stream->state.i_pes_used,
                       i_payload_max );

    if( b_pcr || i_payload < i_payload_max )
    {
        b_adaptation_field = true;
    }

    block_t *p_ts = block_Alloc( 188 );

    if (b_new_pes && !(p_pes->i_flags & BLOCK_FLAG_NO_KEYFRAME) && p_pes->i_flags & BLOCK_FLAG_TYPE_I)
    {
        p_ts->i_flags |= BLOCK_FLAG_TYPE_I;
    }

    p_ts->i_dts = p_pes->i_dts;

    p_ts->p_buffer[0] = 0x47;
    p_ts->p_buffer[1] = ( b_new_pes ? 0x40 : 0x00 ) |
        ( ( p_stream->ts.i_pid >> 8 )&0x1f );
    p_ts->p_buffer[2] = p_stream->ts.i_pid & 0xff;
    p_ts->p_buffer[3] = ( b_adaptation_field ? 0x30 : 0x10 ) |
        p_stream->ts.i_continuity_counter;

    p_stream->ts.i_continuity_counter = (p_stream->ts.i_continuity_counter+1)%16;
    p_stream->ts.b_discontinuity = p_pes->i_flags & BLOCK_FLAG_DISCONTINUITY;

    if( b_adaptation_field )
    {
        int i_stuffing = i_payload_max - i_payload;
        if( b_pcr )
        {
            p_ts->i_flags |= BLOCK_FLAG_CLOCK;

            p_ts->p_buffer[4] = 7 + i_stuffing;
            p_ts->p_buffer[5] = 1 << 4; /* PCR_flag */
            if( p_stream->ts.b_discontinuity )
            {
                p_ts->p_buffer[5] |= 0x80; /* flag TS dicontinuity */
                p_stream->ts.b_discontinuity = false;
            }
            memset(&p_ts->p_buffer[12], 0xff, i_stuffing);
        }
        else
        {
            p_ts->p_buffer[4] = --i_stuffing;
            if( i_stuffing-- )
            {
                p_ts->p_buffer[5] = 0;
                memset(&p_ts->p_buffer[6], 0xff, i_stuffing);
            }
        }
    }

    /* copy payload */
    memcpy( &p_ts->p_buffer[188 - i_payload],
            &p_pes->p_buffer[p_stream->state.i_pes_used], i_payload );

    p_stream->state.i_pes_used += i_payload;
    p_stream->state.i_pes_dts = p_pes->i_dts + p_pes->i_length *
        p_stream->state.i_pes_used / p_pes->i_buffer;
    p_stream->state.i_pes_length -= p_pes->i_length * i_payload / p_pes->i_buffer;

    if( p_stream->state.i_pes_used >= (int)p_pes->i_buffer )
    {
        block_Release(BufferChainGet( &p_stream->state.chain_pes ));

        p_pes = p_stream->state.chain_pes.p_first;
        p_stream->state.i_pes_length = 0;
        if( p_pes )
        {
            p_stream->state.i_pes_dts = p_pes->i_dts;
            while( p_pes )
            {
                p_stream->state.i_pes_length += p_pes->i_length;
                p_pes = p_pes->p_next;
            }
        }
        else
        {
            p_stream->state.i_pes_dts = 0;
        }
        p_stream->state.i_pes_used = 0;
    }

    return p_ts;
}

static void TSSetPCR( block_t *p_ts, vlc_tick_t i_dts )
{
    int64_t i_pcr = TO_SCALE_NZ(i_dts);

    p_ts->p_buffer[6]  = ( i_pcr >> 25 )&0xff;
    p_ts->p_buffer[7]  = ( i_pcr >> 17 )&0xff;
    p_ts->p_buffer[8]  = ( i_pcr >> 9  )&0xff;
    p_ts->p_buffer[9]  = ( i_pcr >> 1  )&0xff;
    p_ts->p_buffer[10] = ( i_pcr << 7  )&0x80;
    p_ts->p_buffer[10] |= 0x7e;
    p_ts->p_buffer[11] = 0; /* we don't set PCR extension */
}

void GetPAT( sout_mux_t *p_mux, sout_buffer_chain_t *c )
{
    sout_mux_sys_t       *p_sys = p_mux->p_sys;

    BuildPAT( p_sys->p_dvbpsi,
              c, (PEStoTSCallback)BufferChainAppend,
              p_sys->i_tsid, p_sys->i_pat_version_number,
              &p_sys->pat,
              p_sys->i_num_pmt, p_sys->pmt, p_sys->i_pmt_program_number );
}

static void GetPMT( sout_mux_t *p_mux, sout_buffer_chain_t *c )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    pes_mapped_stream_t mappeds[p_mux->i_nb_inputs];

    for (int i_stream = 0; i_stream < p_mux->i_nb_inputs; i_stream++ )
    {
        sout_input_t *p_input = p_mux->pp_inputs[i_stream];
        sout_input_sys_t *p_stream = (sout_input_sys_t*)p_input->p_sys;

        int i_pidinput = p_input->p_fmt->i_id;
        pmt_map_t *p_usepid = bsearch( &i_pidinput, p_sys->pmtmap,
                                       p_sys->i_pmtslots, sizeof(pmt_map_t), intcompare );

        /* If there's an error somewhere, dump it to the first pmt */
        mappeds[i_stream].i_mapped_prog = p_usepid ? p_usepid->i_prog : 0;
        mappeds[i_stream].fmt = p_input->p_fmt;
        mappeds[i_stream].pes = &p_stream->pes;
        mappeds[i_stream].ts = &p_stream->ts;
    }

    BuildPMT( p_sys->p_dvbpsi, VLC_OBJECT(p_mux), p_sys->standard,
              c, (PEStoTSCallback)BufferChainAppend,
              p_sys->i_tsid, p_sys->i_pmt_version_number,
              ((sout_input_sys_t *)p_sys->p_pcr_input->p_sys)->ts.i_pid,
              &p_sys->sdt,
              p_sys->i_num_pmt, p_sys->pmt, p_sys->i_pmt_program_number,
              p_mux->i_nb_inputs, mappeds );
}
