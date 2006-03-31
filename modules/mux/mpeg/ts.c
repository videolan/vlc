/*****************************************************************************
 * ts.c: MPEG-II TS Muxer
 *****************************************************************************
 * Copyright (C) 2001-2005 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *          Jean-Paul Saman <jpsaman #_at_# m2x.nl>
 *          Wallace Wadge <wwadge #_at_# gmail.com>
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
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#include "iso_lang.h"

#include "bits.h"
#include "pes.h"
#include "csa.h"

#ifdef HAVE_DVBPSI_DR_H
#   include <dvbpsi/dvbpsi.h>
#   include <dvbpsi/demux.h>
#   include <dvbpsi/descriptor.h>
#   include <dvbpsi/pat.h>
#   include <dvbpsi/pmt.h>
#   include <dvbpsi/sdt.h>
#   include <dvbpsi/dr.h>
#   include <dvbpsi/psi.h>
#else
#   include "dvbpsi.h"
#   include "demux.h"
#   include "descriptor.h"
#   include "tables/pat.h"
#   include "tables/pmt.h"
#   include "tables/sdt.h"
#   include "descriptors/dr.h"
#   include "psi.h"
#endif

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
 * Module descriptor
 *****************************************************************************/
static int     Open   ( vlc_object_t * );
static void    Close  ( vlc_object_t * );

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
#define SDTDESC_LONGTEXT N_("Defines the descriptors of each SDT. This" \
                        "requires \"Set PID to ID of ES\" to be enabled." )

#define PID_TEXT N_("Set PID to ID of ES")
#define PID_LONGTEXT N_("Sets PID to the ID if the incoming ES. This is for " \
  "use with --ts-es-id-pid, and allows to have the same PIDs in the input " \
  "and output streams.")

#define ALIGNMENT_TEXT N_("Data alignment")
#define ALIGNMENT_LONGTEXT N_("Enforces alignment of all access units on " \
  "PES boundaries. This is a waste of bandwidth.")

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

#define PCR_TEXT N_("PCR delay (ms)")
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

#define CPKT_TEXT N_("Packet size in bytes to encrypt")
#define CPKT_LONGTEXT N_("Size of the TS packet to encrypt. " \
    "The encryption routines subtract the TS-header from the value before " \
    "encrypting. " )

#define SOUT_CFG_PREFIX "sout-ts-"
#ifdef HAVE_BSEARCH
#   define MAX_PMT 64       /* Maximum number of programs. FIXME: I just chose an arbitary number. Where is the maximum in the spec? */
#else
#   define MAX_PMT 1
#endif
#define MAX_PMT_PID 64       /* Maximum pids in each pmt.  FIXME: I just chose an arbitary number. Where is the maximum in the spec? */

vlc_module_begin();
    set_description( _("TS muxer (libdvbpsi)") );
    set_shortname( "MPEG-TS");
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_MUX );
    set_capability( "sout mux", 120 );
    add_shortcut( "ts" );

    add_integer( SOUT_CFG_PREFIX "pid-video", 0, NULL,VPID_TEXT, VPID_LONGTEXT,
                                  VLC_TRUE );
    add_integer( SOUT_CFG_PREFIX "pid-audio", 0, NULL, APID_TEXT,
                 APID_LONGTEXT, VLC_TRUE );
    add_integer( SOUT_CFG_PREFIX "pid-spu", 0, NULL, SPUPID_TEXT,
                 SPUPID_LONGTEXT, VLC_TRUE );
    add_integer( SOUT_CFG_PREFIX "pid-pmt", 0, NULL, PMTPID_TEXT,
                 PMTPID_LONGTEXT, VLC_TRUE );
    add_integer( SOUT_CFG_PREFIX "tsid", 0, NULL, TSID_TEXT,
                 TSID_LONGTEXT, VLC_TRUE );
#ifdef HAVE_DVBPSI_SDT
    add_integer( SOUT_CFG_PREFIX "netid", 0, NULL, NETID_TEXT,
                 NETID_LONGTEXT, VLC_TRUE );
#endif
    add_string( SOUT_CFG_PREFIX "program-pmt", NULL, NULL, PMTPROG_TEXT,
                PMTPROG_LONGTEXT, VLC_TRUE );
    add_bool( SOUT_CFG_PREFIX "es-id-pid", 0, NULL, PID_TEXT, PID_LONGTEXT,
              VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "muxpmt", NULL, NULL, MUXPMT_TEXT, MUXPMT_LONGTEXT, VLC_TRUE );
#ifdef HAVE_DVBPSI_SDT
    add_string( SOUT_CFG_PREFIX "sdtdesc", NULL, NULL, SDTDESC_TEXT, SDTDESC_LONGTEXT, VLC_TRUE );
#endif
    add_bool( SOUT_CFG_PREFIX "alignment", 0, NULL, ALIGNMENT_TEXT,
              ALIGNMENT_LONGTEXT, VLC_TRUE );

    add_integer( SOUT_CFG_PREFIX "shaping", 200, NULL,SHAPING_TEXT,
                 SHAPING_LONGTEXT, VLC_TRUE );
    add_bool( SOUT_CFG_PREFIX "use-key-frames", VLC_FALSE, NULL, KEYF_TEXT,
              KEYF_LONGTEXT, VLC_TRUE );

    add_integer( SOUT_CFG_PREFIX "pcr", 70, NULL, PCR_TEXT, PCR_LONGTEXT,
                 VLC_TRUE );
    add_integer( SOUT_CFG_PREFIX "bmin", 0, NULL, BMIN_TEXT, BMIN_LONGTEXT,
                 VLC_TRUE );
    add_integer( SOUT_CFG_PREFIX "bmax", 0, NULL, BMAX_TEXT, BMAX_LONGTEXT,
                 VLC_TRUE );
    add_integer( SOUT_CFG_PREFIX "dts-delay", 400, NULL, DTS_TEXT,
                 DTS_LONGTEXT, VLC_TRUE );

    add_bool( SOUT_CFG_PREFIX "crypt-audio", VLC_TRUE, NULL, ACRYPT_TEXT,
              ACRYPT_LONGTEXT, VLC_TRUE );
    add_bool( SOUT_CFG_PREFIX "crypt-video", VLC_TRUE, NULL, VCRYPT_TEXT,
              VCRYPT_LONGTEXT, VLC_TRUE );

    add_string( SOUT_CFG_PREFIX "csa-ck", NULL, NULL, CK_TEXT, CK_LONGTEXT,
                VLC_TRUE );
    add_integer( SOUT_CFG_PREFIX "csa-pkt", 188, NULL, CPKT_TEXT, CPKT_LONGTEXT, VLC_TRUE );

    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local data structures
 *****************************************************************************/
static const char *ppsz_sout_options[] = {
    "pid-video", "pid-audio", "pid-spu", "pid-pmt", "tsid", "netid",
    "es-id-pid", "shaping", "pcr", "bmin", "bmax", "use-key-frames",
    "dts-delay", "csa-ck", "csa-pkt", "crypt-audio", "crypt-video",
    "muxpmt", "sdtdesc", "program-pmt", "alignment",
    NULL
};

typedef struct pmt_map_t   /* Holds the mapping between the pmt-pid/pmt table */
{
    int i_pid;
    unsigned long i_prog;
} pmt_map_t;

typedef struct sdt_desc_t
{
    char *psz_provider;
    char *psz_service_name;  /* name of program */
} sdt_desc_t;

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

static inline void BufferChainClean( sout_instance_t *p_sout,
                                     sout_buffer_chain_t *c )
{
    block_t *b;

    while( ( b = BufferChainGet( c ) ) )
    {
        block_Release( b );
    }
    BufferChainInit( c );
}

typedef struct ts_stream_t
{
    int             i_pid;
    vlc_fourcc_t    i_codec;

    int             i_stream_type;
    int             i_stream_id;
    int             i_continuity_counter;
    vlc_bool_t      b_discontinuity;

    /* to be used for carriege of DIV3 */
    vlc_fourcc_t    i_bih_codec;
    int             i_bih_width, i_bih_height;

    /* Specific to mpeg4 in mpeg2ts */
    int             i_es_id;

    int             i_decoder_specific_info;
    uint8_t         *p_decoder_specific_info;

    /* language is iso639-2T */
    uint8_t         lang[3];

    sout_buffer_chain_t chain_pes;
    mtime_t             i_pes_dts;
    mtime_t             i_pes_length;
    int                 i_pes_used;
    vlc_bool_t          b_key_frame;

} ts_stream_t;

struct sout_mux_sys_t
{
    int             i_pcr_pid;
    sout_input_t    *p_pcr_input;

    int             i_audio_bound;
    int             i_video_bound;

    vlc_bool_t      b_es_id_pid;
    vlc_bool_t      b_sdt;
    int             i_pid_video;
    int             i_pid_audio;
    int             i_pid_spu;
    int             i_pid_free; /* first usable pid */

    int             i_tsid;
    int             i_netid;
    int             i_num_pmt;
    int             i_pmtslots;
    int             i_pat_version_number;
    ts_stream_t     pat;

    int             i_pmt_version_number;
    ts_stream_t     pmt[MAX_PMT];
    pmt_map_t       pmtmap[MAX_PMT_PID];
    int             i_pmt_program_number[MAX_PMT];
    sdt_desc_t      sdt_descriptors[MAX_PMT];
    vlc_bool_t      b_data_alignment;

    int             i_mpeg4_streams;

    int             i_null_continuity_counter;  /* Needed ? */
    ts_stream_t     sdt;
    dvbpsi_pmt_t    *dvbpmt;

    /* for TS building */
    int64_t             i_bitrate_min;
    int64_t             i_bitrate_max;

    int64_t             i_shaping_delay;
    int64_t             i_pcr_delay;

    int64_t             i_dts_delay;

    vlc_bool_t          b_use_key_frames;

    mtime_t             i_pcr;  /* last PCR emited */

    csa_t               *csa;
    int                 i_csa_pkt_size;
    vlc_bool_t          b_crypt_audio;
    vlc_bool_t          b_crypt_video;
};

/* Reserve a pid and return it */
static int  AllocatePID( sout_mux_sys_t *p_sys, int i_cat )
{
    int i_pid;
    if ( i_cat == VIDEO_ES && p_sys->i_pid_video )
    {
        i_pid = p_sys->i_pid_video;
        p_sys->i_pid_video = 0;
    }
    else if ( i_cat == AUDIO_ES && p_sys->i_pid_audio )
    {
        i_pid = p_sys->i_pid_audio;
        p_sys->i_pid_audio = 0;
    }
    else if ( i_cat == SPU_ES && p_sys->i_pid_spu )
    {
        i_pid = p_sys->i_pid_spu;
        p_sys->i_pid_spu = 0;
    }
    else
    {
        i_pid = ++p_sys->i_pid_free;
    }
    return i_pid;
}

static int pmtcompare( const void *pa, const void *pb )
{
    if ( ((pmt_map_t *)pa)->i_pid  < ((pmt_map_t *)pb)->i_pid )
        return -1;
    else if ( ((pmt_map_t *)pa)->i_pid  > ((pmt_map_t *)pb)->i_pid )
        return 1;
    else
        return 0;
}

static int intcompare( const void *pa, const void *pb )
{
    if ( *(int *)pa  < *(int *)pb )
        return -1;
    else if ( *(int *)pa > *(int *)pb )
        return 1;
    else
        return 0;
}

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Control  ( sout_mux_t *, int, va_list );
static int AddStream( sout_mux_t *, sout_input_t * );
static int DelStream( sout_mux_t *, sout_input_t * );
static int Mux      ( sout_mux_t * );

static block_t *FixPES( sout_mux_t *p_mux, block_fifo_t *p_fifo );
static void TSSchedule  ( sout_mux_t *p_mux, sout_buffer_chain_t *p_chain_ts,
                          mtime_t i_pcr_length, mtime_t i_pcr_dts );
static void TSDate      ( sout_mux_t *p_mux, sout_buffer_chain_t *p_chain_ts,
                          mtime_t i_pcr_length, mtime_t i_pcr_dts );
static void GetPAT( sout_mux_t *p_mux, sout_buffer_chain_t *c );
static void GetPMT( sout_mux_t *p_mux, sout_buffer_chain_t *c );

static block_t *TSNew( sout_mux_t *p_mux, ts_stream_t *p_stream, vlc_bool_t b_pcr );
static void TSSetPCR( block_t *p_ts, mtime_t i_dts );

static void PEStoTS  ( sout_instance_t *, sout_buffer_chain_t *, block_t *, ts_stream_t * );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_mux_t          *p_mux =(sout_mux_t*)p_this;
    sout_mux_sys_t      *p_sys = NULL;
    vlc_value_t         val;
    int i;

    sout_CfgParse( p_mux, SOUT_CFG_PREFIX, ppsz_sout_options, p_mux->p_cfg );

    p_sys = malloc( sizeof( sout_mux_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;
    p_sys->i_pmtslots = p_sys->b_sdt = 0;
    p_sys->i_num_pmt = 1;
    p_sys->dvbpmt = NULL;
    memset( &p_sys->pmtmap, 0, sizeof(p_sys->pmtmap) );

    p_mux->pf_control   = Control;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;
    p_mux->p_sys        = p_sys;

    srand( (uint32_t)mdate() );
    for ( i = 0; i < MAX_PMT; i++ )
        p_sys->sdt_descriptors[i].psz_service_name
            = p_sys->sdt_descriptors[i].psz_provider = NULL;
    memset( p_sys->sdt_descriptors, 0, sizeof(sdt_desc_t) );

    p_sys->i_audio_bound = 0;
    p_sys->i_video_bound = 0;

    var_Get( p_mux, SOUT_CFG_PREFIX "es-id-pid", &val );
    p_sys->b_es_id_pid = val.b_bool;

    var_Get( p_mux, SOUT_CFG_PREFIX "muxpmt", &val );
    /*
       fetch string of pmts. Here's a sample: --sout-ts-muxpmt="0x451,0x200,0x28a,0x240,,0x450,0x201,0x28b,0x241,,0x452,0x202,0x28c,0x242"
       This would mean 0x451, 0x200, 0x28a, 0x240 would fall under one pmt (program), 0x450,0x201,0x28b,0x241 would fall under another
    */
    if( val.psz_string != NULL && *val.psz_string )
    {
        char *psz_next;
        char *psz = val.psz_string;
        uint16_t i_pid;
        psz_next = psz;

        while( psz != NULL )
        {
            i_pid = strtoul( psz, &psz_next, 0 );

            if ( strlen(psz_next) > 0 )
                psz = &psz_next[1];
            if ( i_pid == 0 )
            {
                p_sys->i_num_pmt++;
                if ( p_sys->i_num_pmt > MAX_PMT )
                {
                    msg_Err( p_mux,
             "Number of PMTs greater than compiled maximum (%d)", MAX_PMT );
                    p_sys->i_num_pmt = MAX_PMT;
                }
            }
            else
            {
                p_sys->pmtmap[p_sys->i_pmtslots].i_pid = i_pid;
                p_sys->pmtmap[p_sys->i_pmtslots].i_prog = p_sys->i_num_pmt - 1;
                p_sys->i_pmtslots++;
                if ( p_sys->i_pmtslots > MAX_PMT_PID )
                {
                    msg_Err( p_mux,
             "Number of pids in PMT greater than compiled maximum (%d)",
                             MAX_PMT_PID );
                    p_sys->i_pmtslots = MAX_PMT_PID;
                }
            }

            /* Now sort according to pids for fast search later on */
            qsort( (void *)p_sys->pmtmap, p_sys->i_pmtslots,
                   sizeof(pmt_map_t), &pmtcompare );
            if ( !*psz_next )
                psz = NULL;
        }
    }
    if( val.psz_string != NULL) free( val.psz_string );

    p_sys->i_pat_version_number = rand() % 32;
    p_sys->pat.i_pid = 0;
    p_sys->pat.i_continuity_counter = 0;
    p_sys->pat.b_discontinuity = VLC_FALSE;

    var_Get( p_mux, SOUT_CFG_PREFIX "tsid", &val );
    if ( val.i_int )
        p_sys->i_tsid = val.i_int;
    else
        p_sys->i_tsid = rand() % 65536;

    p_sys->i_netid = rand() % 65536;
#ifdef HAVE_DVBPSI_SDT
    var_Get( p_mux, SOUT_CFG_PREFIX "netid", &val );
    if ( val.i_int )
        p_sys->i_netid = val.i_int;
#endif

    p_sys->i_pmt_version_number = rand() % 32;
    for( i = 0; i < p_sys->i_num_pmt; i++ )
    {
        p_sys->pmt[i].i_continuity_counter = 0;
        p_sys->pmt[i].b_discontinuity = VLC_FALSE;
    }

    p_sys->sdt.i_pid = 0x11;
    p_sys->sdt.i_continuity_counter = 0;
    p_sys->sdt.b_discontinuity = VLC_FALSE;

#ifdef HAVE_DVBPSI_SDT
    var_Get( p_mux, SOUT_CFG_PREFIX "sdtdesc", &val );
    p_sys->b_sdt = val.psz_string && *val.psz_string ? VLC_TRUE : VLC_FALSE;

    /* Syntax is provider_sdt1,service_name_sdt1,provider_sdt2,service_name_sdt2... */
    if( p_sys->b_sdt )
    {

        char *psz = val.psz_string;
        char *psz_sdttoken = psz;

        i = 0;
        while ( psz_sdttoken != NULL )
        {
            char *psz_end = strchr( psz_sdttoken, ',' );
            if( psz_end != NULL )
            {
                *psz_end++ = '\0';
            }
            if ( !(i % 2) )
            {
                p_sys->sdt_descriptors[i/2].psz_provider
                    = strdup(psz_sdttoken);
            }
            else
            {
                p_sys->sdt_descriptors[i/2].psz_service_name
                    = strdup(psz_sdttoken);
            }

            i++;
            psz_sdttoken = psz_end;
        }
    }
    if( val.psz_string != NULL ) free( val.psz_string );
#else
    p_sys->b_sdt = VLC_FALSE;
#endif

    var_Get( p_mux, SOUT_CFG_PREFIX "alignment", &val );
    p_sys->b_data_alignment = val.b_bool;

    var_Get( p_mux, SOUT_CFG_PREFIX "program-pmt", &val );
    if( val.psz_string && *val.psz_string )
    {
        char *psz_next;
        char *psz = val.psz_string;
        uint16_t i_pid;

        psz_next = psz;
        i = 0;
        while ( psz != NULL )
        {
            i_pid = strtoul( psz, &psz_next, 0 );
            if( strlen(psz_next) > 0 )
                psz = &psz_next[1];
            else
                psz = NULL;

            if( i_pid == 0 )
            {
                if( i > MAX_PMT )
                    msg_Err( p_mux, "Number of PMTs > maximum (%d)",
                             MAX_PMT );
            }
            else
            {
                p_sys->i_pmt_program_number[i] = i_pid;
                i++;
            }
        }
    }
    else
    {
        /* Option not specified, use 1, 2, 3... */
        for( i = 0; i < p_sys->i_num_pmt; i++ )
            p_sys->i_pmt_program_number[i] = i + 1;
    }
    if( val.psz_string != NULL ) free( val.psz_string );

    var_Get( p_mux, SOUT_CFG_PREFIX "pid-pmt", &val );
    if( val.i_int )
    {
        for( i = 0; i < p_sys->i_num_pmt; i++ )
            p_sys->pmt[i].i_pid = val.i_int + i; /* Does this make any sense? */
    }
    else
    {
        for( i = 0; i < p_sys->i_num_pmt; i++ )
            p_sys->pmt[i].i_pid = 0x42 + i;
    }

    p_sys->i_pid_free = p_sys->pmt[p_sys->i_num_pmt - 1].i_pid + 1;

    var_Get( p_mux, SOUT_CFG_PREFIX "pid-video", &val );
    p_sys->i_pid_video = val.i_int;
    if ( p_sys->i_pid_video > p_sys->i_pid_free )
    {
        p_sys->i_pid_free = p_sys->i_pid_video + 1;
    }

    var_Get( p_mux, SOUT_CFG_PREFIX "pid-audio", &val );
    p_sys->i_pid_audio = val.i_int;
    if ( p_sys->i_pid_audio > p_sys->i_pid_free )
    {
        p_sys->i_pid_free = p_sys->i_pid_audio + 1;
    }

    var_Get( p_mux, SOUT_CFG_PREFIX "pid-spu", &val );
    p_sys->i_pid_spu = val.i_int;
    if ( p_sys->i_pid_spu > p_sys->i_pid_free )
    {
        p_sys->i_pid_free = p_sys->i_pid_spu + 1;
    }

    p_sys->i_pcr_pid = 0x1fff;
    p_sys->p_pcr_input = NULL;

    p_sys->i_mpeg4_streams = 0;

    p_sys->i_null_continuity_counter = 0;

    /* Allow to create constrained stream */
    var_Get( p_mux, SOUT_CFG_PREFIX "bmin", &val );
    p_sys->i_bitrate_min = val.i_int;

    var_Get( p_mux, SOUT_CFG_PREFIX "bmax", &val );
    p_sys->i_bitrate_max = val.i_int;

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
    p_sys->i_shaping_delay = (int64_t)val.i_int * 1000;
    if( p_sys->i_shaping_delay <= 0 )
    {
        msg_Err( p_mux,
                 "invalid shaping ("I64Fd"ms) resetting to 200ms",
                 p_sys->i_shaping_delay / 1000 );
        p_sys->i_shaping_delay = 200000;
    }

    var_Get( p_mux, SOUT_CFG_PREFIX "pcr", &val );
    p_sys->i_pcr_delay = (int64_t)val.i_int * 1000;
    if( p_sys->i_pcr_delay <= 0 ||
        p_sys->i_pcr_delay >= p_sys->i_shaping_delay )
    {
        msg_Err( p_mux,
                 "invalid pcr delay ("I64Fd"ms) resetting to 70ms",
                 p_sys->i_pcr_delay / 1000 );
        p_sys->i_pcr_delay = 70000;
    }

    var_Get( p_mux, SOUT_CFG_PREFIX "dts-delay", &val );
    p_sys->i_dts_delay = (int64_t)val.i_int * 1000;

    msg_Dbg( p_mux, "shaping="I64Fd" pcr="I64Fd" dts_delay="I64Fd,
             p_sys->i_shaping_delay, p_sys->i_pcr_delay, p_sys->i_dts_delay );

    var_Get( p_mux, SOUT_CFG_PREFIX "use-key-frames", &val );
    p_sys->b_use_key_frames = val.b_bool;

    /* for TS generation */
    p_sys->i_pcr    = 0;

    p_sys->csa      = NULL;
    var_Get( p_mux, SOUT_CFG_PREFIX "csa-ck", &val );
    if( val.psz_string && *val.psz_string )
    {
        char *psz = val.psz_string;

        /* skip 0x */
        if( psz[0] == '0' && ( psz[1] == 'x' || psz[1] == 'X' ) )
        {
            psz += 2;
        }
        if( strlen( psz ) != 16 )
        {
            msg_Dbg( p_mux, "invalid csa ck (it must be 16 chars long)" );
        }
        else
        {
            uint64_t i_ck = strtoull( psz, NULL, 16 );
            uint8_t  ck[8];
            int      i;

            for( i = 0; i < 8; i++ )
            {
                ck[i] = ( i_ck >> ( 56 - 8*i) )&0xff;
            }
#ifndef TS_NO_CSA_CK_MSG
            msg_Dbg( p_mux, "using CSA scrambling with ck=%x:%x:%x:%x:%x:%x:%x:%x",
                     ck[0], ck[1], ck[2], ck[3], ck[4], ck[5], ck[6], ck[7] );
#endif
            p_sys->csa = csa_New();
            if( p_sys->csa )
            {
                vlc_value_t pkt_val;

                csa_SetCW( p_sys->csa, ck, ck );

                var_Get( p_mux, SOUT_CFG_PREFIX "csa-pkt", &pkt_val );
                if( pkt_val.i_int < 12 || pkt_val.i_int > 188 )
                {
                    msg_Err( p_mux, "wrong packet size %d specified.", pkt_val.i_int );
                    msg_Warn( p_mux, "using default packet size of 188 bytes" );
                    p_sys->i_csa_pkt_size = 188;
                }
                else p_sys->i_csa_pkt_size = pkt_val.i_int;
                msg_Dbg( p_mux, "encrypting %d bytes of packet", p_sys->i_csa_pkt_size );
            }
        }
    }
    if( val.psz_string ) free( val.psz_string );

    var_Get( p_mux, SOUT_CFG_PREFIX "crypt-audio", &val );
    p_sys->b_crypt_audio = val.b_bool;

    var_Get( p_mux, SOUT_CFG_PREFIX "crypt-video", &val );
    p_sys->b_crypt_video = val.b_bool;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_mux_t          *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t      *p_sys = p_mux->p_sys;
    int i;

    if( p_sys->csa )
    {
        csa_Delete( p_sys->csa );
    }
    for( i = 0; i < MAX_PMT; i++ )
    {
        if( p_sys->sdt_descriptors[i].psz_service_name != NULL )
            free( p_sys->sdt_descriptors[i].psz_service_name );
        if( p_sys->sdt_descriptors[i].psz_provider != NULL )
            free( p_sys->sdt_descriptors[i].psz_provider );
    }

    if( p_sys->dvbpmt != NULL )  /* safety */
        free ( p_sys->dvbpmt );

    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( sout_mux_t *p_mux, int i_query, va_list args )
{
    vlc_bool_t *pb_bool;
    char **ppsz;

   switch( i_query )
   {
       case MUX_CAN_ADD_STREAM_WHILE_MUXING:
           pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t * );
           *pb_bool = VLC_TRUE;
           return VLC_SUCCESS;

       case MUX_GET_ADD_STREAM_WAIT:
           pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t * );
           *pb_bool = VLC_FALSE;
           return VLC_SUCCESS;

       case MUX_GET_MIME:
           ppsz = (char**)va_arg( args, char ** );
           *ppsz = strdup( "video/mpeg" );  /* FIXME not sure */
           return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
   }
}

/*****************************************************************************
 * AddStream: called for each stream addition
 *****************************************************************************/
static int AddStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t      *p_sys = p_mux->p_sys;
    ts_stream_t         *p_stream;

    p_input->p_sys = p_stream = malloc( sizeof( ts_stream_t ) );

    /* Init this new stream */
    if ( p_sys->b_es_id_pid )
        p_stream->i_pid = p_input->p_fmt->i_id & 0x1fff;
    else
        p_stream->i_pid = AllocatePID( p_sys, p_input->p_fmt->i_cat );
    p_stream->i_codec = p_input->p_fmt->i_codec;
    p_stream->i_continuity_counter    = 0;
    p_stream->b_discontinuity         = VLC_FALSE;
    p_stream->i_decoder_specific_info = 0;
    p_stream->p_decoder_specific_info = NULL;

    msg_Dbg( p_mux, "adding input codec=%4.4s pid=%d",
             (char*)&p_input->p_fmt->i_codec, p_stream->i_pid );

    /* All others fields depand on codec */
    switch( p_input->p_fmt->i_cat )
    {
        case VIDEO_ES:
            switch( p_input->p_fmt->i_codec )
            {
                case VLC_FOURCC( 'm', 'p','g', 'v' ):
                    /* TODO: do we need to check MPEG-I/II ? */
                    p_stream->i_stream_type = 0x02;
                    p_stream->i_stream_id = 0xe0;
                    break;
                case VLC_FOURCC( 'm', 'p','4', 'v' ):
                    p_stream->i_stream_type = 0x10;
                    p_stream->i_stream_id = 0xe0;
                    p_stream->i_es_id = p_stream->i_pid;
                    break;
                case VLC_FOURCC( 'h', '2','6', '4' ):
                    p_stream->i_stream_type = 0x1b;
                    p_stream->i_stream_id = 0xe0;
                    break;
                /* XXX dirty dirty but somebody want that:
                 *     using crapy MS-codec XXX */
                /* I didn't want to do that :P */
                case VLC_FOURCC( 'H', '2', '6', '3' ):
                case VLC_FOURCC( 'I', '2', '6', '3' ):
                case VLC_FOURCC( 'W', 'M', 'V', '3' ):
                case VLC_FOURCC( 'W', 'M', 'V', '2' ):
                case VLC_FOURCC( 'W', 'M', 'V', '1' ):
                case VLC_FOURCC( 'D', 'I', 'V', '3' ):
                case VLC_FOURCC( 'D', 'I', 'V', '2' ):
                case VLC_FOURCC( 'D', 'I', 'V', '1' ):
                case VLC_FOURCC( 'M', 'J', 'P', 'G' ):
                    p_stream->i_stream_type = 0xa0; /* private */
                    p_stream->i_stream_id = 0xa0;   /* beurk */
                    p_stream->i_bih_codec  = p_input->p_fmt->i_codec;
                    p_stream->i_bih_width  = p_input->p_fmt->video.i_width;
                    p_stream->i_bih_height = p_input->p_fmt->video.i_height;
                    break;
                default:
                    free( p_stream );
                    return VLC_EGENERIC;
            }
            p_sys->i_video_bound++;
            break;

        case AUDIO_ES:
            switch( p_input->p_fmt->i_codec )
            {
                case VLC_FOURCC( 'm', 'p','g', 'a' ):
                    p_stream->i_stream_type =
                        p_input->p_fmt->audio.i_rate >= 32000 ? 0x03 : 0x04;
                    p_stream->i_stream_id = 0xc0;
                    break;
                case VLC_FOURCC( 'a', '5','2', ' ' ):
                    p_stream->i_stream_type = 0x81;
                    p_stream->i_stream_id = 0xbd;
                    break;
                case VLC_FOURCC( 'l', 'p','c', 'm' ):
                    p_stream->i_stream_type = 0x83;
                    p_stream->i_stream_id = 0xbd;
                    break;
                case VLC_FOURCC( 'd', 't','s', ' ' ):
                    p_stream->i_stream_type = 0x06;
                    p_stream->i_stream_id = 0xbd;
                    break;
                case VLC_FOURCC( 'm', 'p','4', 'a' ):
                    p_stream->i_stream_type = 0x11;
                    p_stream->i_stream_id = 0xfa;
                    p_sys->i_mpeg4_streams++;
                    p_stream->i_es_id = p_stream->i_pid;
                    break;
                default:
                    free( p_stream );
                    return VLC_EGENERIC;
            }
            p_sys->i_audio_bound++;
            break;

        case SPU_ES:
            switch( p_input->p_fmt->i_codec )
            {
                case VLC_FOURCC( 's', 'p','u', ' ' ):
                    p_stream->i_stream_type = 0x82;
                    p_stream->i_stream_id = 0xbd;
                    break;
                case VLC_FOURCC( 's', 'u','b', 't' ):
                    p_stream->i_stream_type = 0x12;
                    p_stream->i_stream_id = 0xfa;
                    p_sys->i_mpeg4_streams++;
                    p_stream->i_es_id = p_stream->i_pid;
                    break;
                case VLC_FOURCC('d','v','b','s'):
                    p_stream->i_stream_type = 0x06;
                    p_stream->i_es_id = p_input->p_fmt->subs.dvb.i_id;
                    p_stream->i_stream_id = 0xbd;
                    break;
                case VLC_FOURCC('t','e','l','x'):
                    p_stream->i_stream_type = 0x06;
                    p_stream->i_stream_id = 0xbd; /* FIXME */
                    break;
                default:
                    free( p_stream );
                    return VLC_EGENERIC;
            }
            break;

        default:
            free( p_stream );
            return VLC_EGENERIC;
    }

    p_stream->lang[0] =
    p_stream->lang[1] =
    p_stream->lang[2] = '\0';
    if( p_input->p_fmt->psz_language )
    {
        char *psz = p_input->p_fmt->psz_language;
        const iso639_lang_t *pl = NULL;

        if( strlen( psz ) == 2 )
        {
            pl = GetLang_1( psz );
        }
        else if( strlen( psz ) == 3 )
        {
            pl = GetLang_2B( psz );
            if( !strcmp( pl->psz_iso639_1, "??" ) )
            {
                pl = GetLang_2T( psz );
            }
        }
        if( pl && strcmp( pl->psz_iso639_1, "??" ) )
        {
            p_stream->lang[0] = pl->psz_iso639_2T[0];
            p_stream->lang[1] = pl->psz_iso639_2T[1];
            p_stream->lang[2] = pl->psz_iso639_2T[2];

            msg_Dbg( p_mux, "    - lang=%c%c%c",
                     p_stream->lang[0], p_stream->lang[1], p_stream->lang[2] );
        }
    }

    /* Copy extra data (VOL for MPEG-4 and extra BitMapInfoHeader for VFW */
    p_stream->i_decoder_specific_info = p_input->p_fmt->i_extra;
    if( p_stream->i_decoder_specific_info > 0 )
    {
        p_stream->p_decoder_specific_info =
            malloc( p_stream->i_decoder_specific_info );
        memcpy( p_stream->p_decoder_specific_info,
                p_input->p_fmt->p_extra,
                p_input->p_fmt->i_extra );
    }

    /* Create decoder specific info for subt */
    if( p_stream->i_codec == VLC_FOURCC( 's', 'u','b', 't' ) )
    {
        uint8_t *p;

        p_stream->i_decoder_specific_info = 55;
        p_stream->p_decoder_specific_info = p =
            malloc( p_stream->i_decoder_specific_info );

        p[0] = 0x10;    /* textFormat, 0x10 for 3GPP TS 26.245 */
        p[1] = 0x00;    /* flags: 1b: associated video info flag
                                  3b: reserved
                                  1b: duration flag
                                  3b: reserved */
        p[2] = 52;      /* remaining size */

        p += 3;

        p[0] = p[1] = p[2] = p[3] = 0; p+=4;    /* display flags */
        *p++ = 0;  /* horizontal justification (-1: left, 0 center, 1 right) */
        *p++ = 1;  /* vertical   justification (-1: top, 0 center, 1 bottom) */

        p[0] = p[1] = p[2] = 0x00; p+=3;/* background rgb */
        *p++ = 0xff;                    /* background a */

        p[0] = p[1] = 0; p += 2;        /* text box top */
        p[0] = p[1] = 0; p += 2;        /* text box left */
        p[0] = p[1] = 0; p += 2;        /* text box bottom */
        p[0] = p[1] = 0; p += 2;        /* text box right */

        p[0] = p[1] = 0; p += 2;        /* start char */
        p[0] = p[1] = 0; p += 2;        /* end char */
        p[0] = p[1] = 0; p += 2;        /* default font id */

        *p++ = 0;                       /* font style flags */
        *p++ = 12;                      /* font size */

        p[0] = p[1] = p[2] = 0x00; p+=3;/* foreground rgb */
        *p++ = 0x00;                    /* foreground a */

        p[0] = p[1] = p[2] = 0; p[3] = 22; p += 4;
        memcpy( p, "ftab", 4 ); p += 4;
        *p++ = 0; *p++ = 1;             /* entry count */
        p[0] = p[1] = 0; p += 2;        /* font id */
        *p++ = 9;                       /* font name length */
        memcpy( p, "Helvetica", 9 );    /* font name */
    }

    /* Init pes chain */
    BufferChainInit( &p_stream->chain_pes );
    p_stream->i_pes_dts    = 0;
    p_stream->i_pes_length = 0;
    p_stream->i_pes_used   = 0;
    p_stream->b_key_frame  = 0;

    /* We only change PMT version (PAT isn't changed) */
    p_sys->i_pmt_version_number = ( p_sys->i_pmt_version_number + 1 )%32;

    /* Update pcr_pid */
    if( p_input->p_fmt->i_cat != SPU_ES &&
        ( p_sys->i_pcr_pid == 0x1fff || p_input->p_fmt->i_cat == VIDEO_ES ) )
    {
        if( p_sys->p_pcr_input )
        {
            /* There was already a PCR stream, so clean context */
            /* FIXME */
        }
        p_sys->i_pcr_pid   = p_stream->i_pid;
        p_sys->p_pcr_input = p_input;

        msg_Dbg( p_mux, "new PCR PID is %d", p_sys->i_pcr_pid );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DelStream: called before a stream deletion
 *****************************************************************************/
static int DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    ts_stream_t     *p_stream;
    vlc_value_t     val;

    p_stream = (ts_stream_t*)p_input->p_sys;
    msg_Dbg( p_mux, "removing input pid=%d", p_stream->i_pid );

    if( p_sys->i_pcr_pid == p_stream->i_pid )
    {
        int i;

        /* Find a new pcr stream (Prefer Video Stream) */
        p_sys->i_pcr_pid = 0x1fff;
        p_sys->p_pcr_input = NULL;
        for( i = 0; i < p_mux->i_nb_inputs; i++ )
        {
            if( p_mux->pp_inputs[i] == p_input )
            {
                continue;
            }

            if( p_mux->pp_inputs[i]->p_fmt->i_cat == VIDEO_ES )
            {
                p_sys->i_pcr_pid  =
                    ((ts_stream_t*)p_mux->pp_inputs[i]->p_sys)->i_pid;
                p_sys->p_pcr_input= p_mux->pp_inputs[i];
                break;
            }
            else if( p_mux->pp_inputs[i]->p_fmt->i_cat != SPU_ES &&
                     p_sys->i_pcr_pid == 0x1fff )
            {
                p_sys->i_pcr_pid  =
                    ((ts_stream_t*)p_mux->pp_inputs[i]->p_sys)->i_pid;
                p_sys->p_pcr_input= p_mux->pp_inputs[i];
            }
        }
        if( p_sys->p_pcr_input )
        {
            /* Empty TS buffer */
            /* FIXME */
        }
        msg_Dbg( p_mux, "new PCR PID is %d", p_sys->i_pcr_pid );
    }

    /* Empty all data in chain_pes */
    BufferChainClean( p_mux->p_sout, &p_stream->chain_pes );

    if( p_stream->p_decoder_specific_info )
    {
        free( p_stream->p_decoder_specific_info );
    }
    if( p_stream->i_stream_id == 0xfa ||
        p_stream->i_stream_id == 0xfb ||
        p_stream->i_stream_id == 0xfe )
    {
        p_sys->i_mpeg4_streams--;
    }

    var_Get( p_mux, SOUT_CFG_PREFIX "pid-video", &val );
    if( val.i_int > 0 )
    {
        int i_pid_video = val.i_int;
        if ( i_pid_video == p_stream->i_pid )
        {
            p_sys->i_pid_video = i_pid_video;
            msg_Dbg( p_mux, "freeing video PID %d", i_pid_video );
        }
    }
    var_Get( p_mux, SOUT_CFG_PREFIX "pid-audio", &val );
    if( val.i_int > 0 )
    {
        int i_pid_audio = val.i_int;
        if ( i_pid_audio == p_stream->i_pid )
        {
            p_sys->i_pid_audio = i_pid_audio;
            msg_Dbg( p_mux, "freeing audio PID %d", i_pid_audio );
        }
    }
    var_Get( p_mux, SOUT_CFG_PREFIX "pid-spu", &val );
    if( val.i_int > 0 )
    {
        int i_pid_spu = val.i_int;
        if ( i_pid_spu == p_stream->i_pid )
        {
            p_sys->i_pid_spu = i_pid_spu;
            msg_Dbg( p_mux, "freeing spu PID %d", i_pid_spu );
        }
    }
    free( p_stream );

    /* We only change PMT version (PAT isn't changed) */
    p_sys->i_pmt_version_number++; p_sys->i_pmt_version_number %= 32;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Mux: Call each time there is new data for at least one stream
 *****************************************************************************
 *
 *****************************************************************************/
static int Mux( sout_mux_t *p_mux )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    ts_stream_t     *p_pcr_stream;

    if( p_sys->i_pcr_pid == 0x1fff )
    {
        msg_Dbg( p_mux, "waiting for PCR streams" );
        msleep( 1000 );
        return VLC_SUCCESS;
    }
    p_pcr_stream = (ts_stream_t*)p_sys->p_pcr_input->p_sys;

    for( ;; )
    {
        sout_buffer_chain_t chain_ts;
        int                 i_packet_count;
        int                 i_packet_pos;
        mtime_t             i_pcr_dts;
        mtime_t             i_pcr_length;
        mtime_t             i_shaping_delay;
        int i;

        if( p_pcr_stream->b_key_frame )
        {
            i_shaping_delay = p_pcr_stream->i_pes_length;
        }
        else
        {
            i_shaping_delay = p_sys->i_shaping_delay;
        }

        /* 1: get enough PES packet for all input */
        for( ;; )
        {
            vlc_bool_t b_ok = VLC_TRUE;
            block_t *p_data;

            /* Accumulate enough data in the pcr stream (>i_shaping_delay) */
            /* Accumulate enough data in all other stream ( >= length of pcr)*/
            for( i = -1; i < p_mux->i_nb_inputs; i++ )
            {
                sout_input_t *p_input;
                ts_stream_t *p_stream;
                int64_t i_spu_delay = 0;

                if( i == -1 )
                    p_input = p_sys->p_pcr_input;
                else if( p_mux->pp_inputs[i]->p_sys == p_pcr_stream )
                    continue;
                else
                    p_input = p_mux->pp_inputs[i];
                p_stream = (ts_stream_t*)p_input->p_sys;

                if( ( p_stream == p_pcr_stream &&
                      p_stream->i_pes_length < i_shaping_delay ) ||
                    p_stream->i_pes_dts + p_stream->i_pes_length <
                    p_pcr_stream->i_pes_dts + p_pcr_stream->i_pes_length )
                {
                    /* Need more data */
                    if( p_input->p_fifo->i_depth <= 1 )
                    {
                        if( p_input->p_fmt->i_cat == AUDIO_ES ||
                            p_input->p_fmt->i_cat == VIDEO_ES )
                        {
                            /* We need more data */
                            return VLC_SUCCESS;
                        }
                        else if( p_input->p_fifo->i_depth <= 0 )
                        {
                            /* spu, only one packet is needed */
                            continue;
                        }
                        else
                        {
                            /* Don't mux the SPU yet if it is too early */
                            block_t *p_spu = block_FifoShow( p_input->p_fifo );

                            i_spu_delay =
                                p_spu->i_dts - p_pcr_stream->i_pes_dts;

                            if( i_spu_delay > i_shaping_delay &&
                                i_spu_delay < I64C(100000000) )
                                continue;

                            if ( i_spu_delay >= I64C(100000000)
                                  || i_spu_delay < 10000 )
                            {
                                BufferChainClean( p_mux->p_sout,
                                                  &p_stream->chain_pes );
                                p_stream->i_pes_dts = 0;
                                p_stream->i_pes_used = 0;
                                p_stream->i_pes_length = 0;
                                continue;
                            }
                        }
                    }
                    b_ok = VLC_FALSE;

                    if( p_stream == p_pcr_stream || p_sys->b_data_alignment
                         || p_input->p_fmt->i_codec !=
                             VLC_FOURCC('m', 'p', 'g', 'a') )
                        p_data = block_FifoGet( p_input->p_fifo );
                    else
                        p_data = FixPES( p_mux, p_input->p_fifo );

                    if( p_input->p_fifo->i_depth > 0 &&
                        p_input->p_fmt->i_cat != SPU_ES )
                    {
                        block_t *p_next = block_FifoShow( p_input->p_fifo );
                        p_data->i_length = p_next->i_dts - p_data->i_dts;
                    }
                    else if( p_input->p_fmt->i_codec !=
                               VLC_FOURCC('s', 'u', 'b', 't' ) )
                        p_data->i_length = 1000;

                    if( ( p_pcr_stream->i_pes_dts > 0 &&
                          p_data->i_dts - 10000000 > p_pcr_stream->i_pes_dts +
                          p_pcr_stream->i_pes_length ) ||
                        p_data->i_dts < p_stream->i_pes_dts ||
                        ( p_stream->i_pes_dts > 0 &&
                          p_input->p_fmt->i_cat != SPU_ES &&
                          p_data->i_dts - 10000000 > p_stream->i_pes_dts +
                          p_stream->i_pes_length ) )
                    {
                        msg_Warn( p_mux, "packet with too strange dts "
                                  "(dts="I64Fd",old="I64Fd",pcr="I64Fd")",
                                  p_data->i_dts, p_stream->i_pes_dts,
                                  p_pcr_stream->i_pes_dts );
                        block_Release( p_data );

                        BufferChainClean( p_mux->p_sout,
                                          &p_stream->chain_pes );
                        p_stream->i_pes_dts = 0;
                        p_stream->i_pes_used = 0;
                        p_stream->i_pes_length = 0;

                        if( p_input->p_fmt->i_cat != SPU_ES )
                        {
                            BufferChainClean( p_mux->p_sout,
                                              &p_pcr_stream->chain_pes );
                            p_pcr_stream->i_pes_dts = 0;
                            p_pcr_stream->i_pes_used = 0;
                            p_pcr_stream->i_pes_length = 0;
                        }
                    }
                    else
                    {
                        int i_header_size = 0;
                        int b_data_alignment = 0;
                        if( p_input->p_fmt->i_cat == SPU_ES )
                        {
                            if( p_input->p_fmt->i_codec ==
                                VLC_FOURCC('s','u','b','t') )
                            {
                                /* Prepend header */
                                p_data = block_Realloc( p_data, 2,
                                                        p_data->i_buffer );
                                p_data->p_buffer[0] =
                                    ( (p_data->i_buffer - 2) >> 8) & 0xff;
                                p_data->p_buffer[1] =
                                    ( (p_data->i_buffer - 2)     ) & 0xff;

                                /* remove trailling \0 if any */
                                if( p_data->i_buffer > 2 &&
                                    p_data->p_buffer[p_data->i_buffer -1] ==
                                    '\0' )
                                    p_data->i_buffer--;

                                /* Append a empty sub (sub text only) */
                                if( p_data->i_length > 0 &&
                                    !( p_data->i_buffer == 1 &&
                                       *p_data->p_buffer == ' ' ) )
                                {
                                    block_t *p_spu = block_New( p_mux, 3 );

                                    p_spu->i_dts = p_spu->i_pts =
                                        p_data->i_dts + p_data->i_length;
                                    p_spu->i_length = 1000;

                                    p_spu->p_buffer[0] = 0;
                                    p_spu->p_buffer[1] = 1;
                                    p_spu->p_buffer[2] = ' ';

                                    E_(EStoPES)( p_mux->p_sout, &p_spu, p_spu,
                                                 p_input->p_fmt,
                                                 p_stream->i_stream_id, 1,
                                                 0, 0, 0 );
                                    p_data->p_next = p_spu;
                                }
                            }
                            else if( p_input->p_fmt->i_codec ==
                                       VLC_FOURCC('t','e','l','x') )
                            {
                                /* EN 300 472 */
                                i_header_size = 0x24;
                                b_data_alignment = 1;
                            }
                        }
                        else if( p_data->i_length < 0 ||
                                 p_data->i_length > 2000000 )
                        {
                            /* FIXME choose a better value, but anyway we
                             * should never have to do that */
                            p_data->i_length = 1000;
                        }

                        p_stream->i_pes_length += p_data->i_length;
                        if( p_stream->i_pes_dts == 0 )
                        {
                            p_stream->i_pes_dts = p_data->i_dts;
                        }

                        /* Convert to pes */
                        if( p_stream->i_stream_id == 0xa0 &&
                            p_data->i_pts <= 0 )
                        {
                            /* XXX yes I know, it's awfull, but it's needed,
                             * so don't remove it ... */
                            p_data->i_pts = p_data->i_dts;
                        }

                        E_( EStoPES )( p_mux->p_sout, &p_data, p_data,
                                       p_input->p_fmt, p_stream->i_stream_id,
                                       1, b_data_alignment, i_header_size, 0 );

                        BufferChainAppend( &p_stream->chain_pes, p_data );

                        if( p_sys->b_use_key_frames && p_stream == p_pcr_stream
                            && (p_data->i_flags & BLOCK_FLAG_TYPE_I)
                            && !(p_data->i_flags & BLOCK_FLAG_NO_KEYFRAME)
                            && (p_stream->i_pes_length > 400000) )
                        {
                            i_shaping_delay = p_stream->i_pes_length;
                            p_stream->b_key_frame = 1;
                        }
                    }
                }
            }

            if( b_ok )
            {
                break;
            }
        }

        /* save */
        i_pcr_dts      = p_pcr_stream->i_pes_dts;
        i_pcr_length   = p_pcr_stream->i_pes_length;
        p_pcr_stream->b_key_frame = 0;

        /* msg_Dbg( p_mux, "starting muxing %lldms", i_pcr_length / 1000 ); */
        /* 2: calculate non accurate total size of muxed ts */
        i_packet_count = 0;
        for( i = 0; i < p_mux->i_nb_inputs; i++ )
        {
            ts_stream_t *p_stream = (ts_stream_t*)p_mux->pp_inputs[i]->p_sys;
            block_t *p_pes;

            /* False for pcr stream but it will be enough to do PCR algo */
            for( p_pes = p_stream->chain_pes.p_first; p_pes != NULL;
                 p_pes = p_pes->p_next )
            {
                int i_size = p_pes->i_buffer;
                if( p_pes->i_dts + p_pes->i_length >
                    p_pcr_stream->i_pes_dts + p_pcr_stream->i_pes_length )
                {
                    mtime_t i_frag = p_pcr_stream->i_pes_dts +
                        p_pcr_stream->i_pes_length - p_pes->i_dts;
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
        GetPAT( p_mux, &chain_ts );
        GetPMT( p_mux, &chain_ts );
        i_packet_pos = 0;
        i_packet_count += chain_ts.i_depth;
        /* msg_Dbg( p_mux, "estimated pck=%d", i_packet_count ); */

        for( ;; )
        {
            int         i_stream;
            mtime_t     i_dts;
            ts_stream_t *p_stream;
            sout_input_t *p_input;
            block_t *p_ts;
            vlc_bool_t   b_pcr;

            /* Select stream (lowest dts) */
            for( i = 0, i_stream = -1, i_dts = 0; i < p_mux->i_nb_inputs; i++ )
            {
                p_input = p_mux->pp_inputs[i];
                p_stream = (ts_stream_t*)p_mux->pp_inputs[i]->p_sys;

                if( p_stream->i_pes_dts == 0 )
                {
                    continue;
                }

                if( i_stream == -1 ||
                    p_stream->i_pes_dts < i_dts )
                {
                    i_stream = i;
                    i_dts = p_stream->i_pes_dts;
                }
            }
            if( i_stream == -1 || i_dts > i_pcr_dts + i_pcr_length )
            {
                break;
            }
            p_stream = (ts_stream_t*)p_mux->pp_inputs[i_stream]->p_sys;

            /* do we need to issue pcr */
            b_pcr = VLC_FALSE;
            if( p_stream == p_pcr_stream &&
                i_pcr_dts + i_packet_pos * i_pcr_length / i_packet_count >=
                p_sys->i_pcr + p_sys->i_pcr_delay )
            {
                b_pcr = VLC_TRUE;
                p_sys->i_pcr = i_pcr_dts + i_packet_pos *
                    i_pcr_length / i_packet_count;
            }

            /* Build the TS packet */
            p_ts = TSNew( p_mux, p_stream, b_pcr );
            if( p_sys->csa != NULL &&
                 (p_input->p_fmt->i_cat != AUDIO_ES || p_sys->b_crypt_audio) &&
                 (p_input->p_fmt->i_cat != VIDEO_ES || p_sys->b_crypt_video) )
            {
                p_ts->i_flags |= BLOCK_FLAG_SCRAMBLED;
            }
            i_packet_pos++;

            /* */
            BufferChainAppend( &chain_ts, p_ts );
        }

        /* 4: date and send */
        TSSchedule( p_mux, &chain_ts, i_pcr_length, i_pcr_dts );
    }
}

#define STD_PES_PAYLOAD 170
static block_t *FixPES( sout_mux_t *p_mux, block_fifo_t *p_fifo )
{
    block_t *p_data;
    int i_size;

    p_data = block_FifoShow( p_fifo );
    i_size = p_data->i_buffer;

    if( i_size == STD_PES_PAYLOAD )
    {
        return block_FifoGet( p_fifo );
    }
    else if( i_size > STD_PES_PAYLOAD )
    {
        block_t *p_new = block_New( p_mux, STD_PES_PAYLOAD );
        p_mux->p_vlc->pf_memcpy( p_new->p_buffer, p_data->p_buffer, STD_PES_PAYLOAD );
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

        p_mux->p_vlc->pf_memcpy( &p_data->p_buffer[i_size], p_next->p_buffer,
                                 i_copy );
        p_next->i_pts += p_next->i_length * i_copy / p_next->i_buffer;
        p_next->i_dts += p_next->i_length * i_copy / p_next->i_buffer;
        p_next->i_length -= p_next->i_length * i_copy / p_next->i_buffer;
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

static void TSSchedule( sout_mux_t *p_mux, sout_buffer_chain_t *p_chain_ts,
                        mtime_t i_pcr_length, mtime_t i_pcr_dts )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    sout_buffer_chain_t new_chain;
    int i_packet_count = p_chain_ts->i_depth;
    int i;

    BufferChainInit( &new_chain );

    if ( i_pcr_length <= 0 )
    {
        i_pcr_length = i_packet_count;
    }

    for( i = 0; i < i_packet_count; i++ )
    {
        block_t *p_ts = BufferChainGet( p_chain_ts );
        mtime_t i_new_dts = i_pcr_dts + i_pcr_length * i / i_packet_count;

        BufferChainAppend( &new_chain, p_ts );

        if( p_ts->i_dts &&
            p_ts->i_dts + p_sys->i_dts_delay * 2/3 < i_new_dts )
        {
            mtime_t i_max_diff = i_new_dts - p_ts->i_dts;
            mtime_t i_cut_dts = p_ts->i_dts;

            p_ts = BufferChainPeek( p_chain_ts );
            i++;
            i_new_dts = i_pcr_dts + i_pcr_length * i / i_packet_count;
            while ( p_ts != NULL && i_new_dts - p_ts->i_dts >= i_max_diff )
            {
                p_ts = BufferChainGet( p_chain_ts );
                i_max_diff = i_new_dts - p_ts->i_dts;
                i_cut_dts = p_ts->i_dts;
                BufferChainAppend( &new_chain, p_ts );

                p_ts = BufferChainPeek( p_chain_ts );
                i++;
                i_new_dts = i_pcr_dts + i_pcr_length * i / i_packet_count;
            }
            msg_Dbg( p_mux, "adjusting rate at "I64Fd"/"I64Fd" (%d/%d)",
                     i_cut_dts - i_pcr_dts, i_pcr_length, new_chain.i_depth,
                     p_chain_ts->i_depth );
            if ( new_chain.i_depth )
                TSDate( p_mux, &new_chain,
                        i_cut_dts - i_pcr_dts,
                        i_pcr_dts );
            if ( p_chain_ts->i_depth )
                TSSchedule( p_mux,
                            p_chain_ts, i_pcr_dts + i_pcr_length - i_cut_dts,
                            i_cut_dts );
            return;
        }
    }

    if ( new_chain.i_depth )
        TSDate( p_mux, &new_chain, i_pcr_length, i_pcr_dts );
}

static void TSDate( sout_mux_t *p_mux, sout_buffer_chain_t *p_chain_ts,
                    mtime_t i_pcr_length, mtime_t i_pcr_dts )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    int i_packet_count = p_chain_ts->i_depth;
    int i;

    if ( i_pcr_length / 1000 > 0 )
    {
        int i_bitrate = ((uint64_t)i_packet_count * 188 * 8000)
                          / (uint64_t)(i_pcr_length / 1000);
        if ( p_sys->i_bitrate_max && p_sys->i_bitrate_max < i_bitrate )
        {
            msg_Warn( p_mux, "max bitrate exceeded at "I64Fd
                      " (%d bi/s for %d pkt in "I64Fd" us)",
                      i_pcr_dts + p_sys->i_shaping_delay * 3 / 2 - mdate(),
                      i_bitrate, i_packet_count, i_pcr_length);
        }
#if 0
        else
        {
            msg_Dbg( p_mux, "starting at "I64Fd
                     " (%d bi/s for %d packets in "I64Fd" us)",
                     i_pcr_dts + p_sys->i_shaping_delay * 3 / 2 - mdate(),
                     i_bitrate, i_packet_count, i_pcr_length);
        }
#endif
    }
    else
    {
        /* This shouldn't happen, but happens in some rare heavy load
         * and packet losses conditions. */
        i_pcr_length = i_packet_count;
    }

    /* msg_Dbg( p_mux, "real pck=%d", i_packet_count ); */
    for( i = 0; i < i_packet_count; i++ )
    {
        block_t *p_ts = BufferChainGet( p_chain_ts );
        mtime_t i_new_dts = i_pcr_dts + i_pcr_length * i / i_packet_count;

        p_ts->i_dts    = i_new_dts;
        p_ts->i_length = i_pcr_length / i_packet_count;

        if( p_ts->i_flags & BLOCK_FLAG_CLOCK )
        {
            /* msg_Dbg( p_mux, "pcr=%lld ms", p_ts->i_dts / 1000 ); */
            TSSetPCR( p_ts, p_ts->i_dts - p_sys->i_dts_delay );
        }
        if( p_ts->i_flags & BLOCK_FLAG_SCRAMBLED )
        {
            csa_Encrypt( p_sys->csa, p_ts->p_buffer, p_sys->i_csa_pkt_size, 0 );
        }

        /* latency */
        p_ts->i_dts += p_sys->i_shaping_delay * 3 / 2;

        sout_AccessOutWrite( p_mux->p_access, p_ts );
    }
}

static block_t *TSNew( sout_mux_t *p_mux, ts_stream_t *p_stream,
                       vlc_bool_t b_pcr )
{
    block_t *p_pes = p_stream->chain_pes.p_first;
    block_t *p_ts;

    vlc_bool_t b_new_pes = VLC_FALSE;
    vlc_bool_t b_adaptation_field = VLC_FALSE;

    int        i_payload_max = 184 - ( b_pcr ? 8 : 0 );
    int        i_payload;

    if( p_stream->i_pes_used <= 0 )
    {
        b_new_pes = VLC_TRUE;
    }
    i_payload = __MIN( (int)p_pes->i_buffer - p_stream->i_pes_used,
                       i_payload_max );

    if( b_pcr || i_payload < i_payload_max )
    {
        b_adaptation_field = VLC_TRUE;
    }

    p_ts = block_New( p_mux, 188 );
    p_ts->i_dts = p_pes->i_dts;

    p_ts->p_buffer[0] = 0x47;
    p_ts->p_buffer[1] = ( b_new_pes ? 0x40 : 0x00 ) |
        ( ( p_stream->i_pid >> 8 )&0x1f );
    p_ts->p_buffer[2] = p_stream->i_pid & 0xff;
    p_ts->p_buffer[3] = ( b_adaptation_field ? 0x30 : 0x10 ) |
        p_stream->i_continuity_counter;

    p_stream->i_continuity_counter = (p_stream->i_continuity_counter+1)%16;
    p_stream->b_discontinuity = (p_pes->i_flags & BLOCK_FLAG_DISCONTINUITY);

    if( b_adaptation_field )
    {
        int i;

        if( b_pcr )
        {
            int     i_stuffing = i_payload_max - i_payload;

            p_ts->i_flags |= BLOCK_FLAG_CLOCK;

            p_ts->p_buffer[4] = 7 + i_stuffing;
            p_ts->p_buffer[5] = 0x10;   /* flags */
            if( p_stream->b_discontinuity )
            {
                p_ts->p_buffer[5] |= 0x80; /* flag TS dicontinuity */
                p_stream->b_discontinuity = VLC_FALSE;
            }
            p_ts->p_buffer[6] = ( 0 )&0xff;
            p_ts->p_buffer[7] = ( 0 )&0xff;
            p_ts->p_buffer[8] = ( 0 )&0xff;
            p_ts->p_buffer[9] = ( 0 )&0xff;
            p_ts->p_buffer[10]= ( ( 0 )&0x80 ) | 0x7e;
            p_ts->p_buffer[11]= 0;

            for( i = 12; i < 12 + i_stuffing; i++ )
            {
                p_ts->p_buffer[i] = 0xff;
            }
        }
        else
        {
            int i_stuffing = i_payload_max - i_payload;

            p_ts->p_buffer[4] = i_stuffing - 1;
            if( i_stuffing > 1 )
            {
                p_ts->p_buffer[5] = 0x00;
                for( i = 6; i < 6 + i_stuffing - 2; i++ )
                {
                    p_ts->p_buffer[i] = 0xff;
                }
            }
        }
    }

    /* copy payload */
    memcpy( &p_ts->p_buffer[188 - i_payload],
            &p_pes->p_buffer[p_stream->i_pes_used], i_payload );

    p_stream->i_pes_used += i_payload;
    p_stream->i_pes_dts = p_pes->i_dts + p_pes->i_length *
        p_stream->i_pes_used / p_pes->i_buffer;
    p_stream->i_pes_length -= p_pes->i_length * i_payload / p_pes->i_buffer;

    if( p_stream->i_pes_used >= (int)p_pes->i_buffer )
    {
        p_pes = BufferChainGet( &p_stream->chain_pes );
        block_Release( p_pes );

        p_pes = p_stream->chain_pes.p_first;
        if( p_pes )
        {
            p_stream->i_pes_dts    = p_pes->i_dts;
            p_stream->i_pes_length = 0;
            while( p_pes )
            {
                p_stream->i_pes_length += p_pes->i_length;

                p_pes = p_pes->p_next;
            }
        }
        else
        {
            p_stream->i_pes_dts = 0;
            p_stream->i_pes_length = 0;
        }
        p_stream->i_pes_used = 0;
    }

    return p_ts;
}

static void TSSetPCR( block_t *p_ts, mtime_t i_dts )
{
    mtime_t i_pcr = 9 * i_dts / 100;

    p_ts->p_buffer[6]  = ( i_pcr >> 25 )&0xff;
    p_ts->p_buffer[7]  = ( i_pcr >> 17 )&0xff;
    p_ts->p_buffer[8]  = ( i_pcr >> 9  )&0xff;
    p_ts->p_buffer[9]  = ( i_pcr >> 1  )&0xff;
    p_ts->p_buffer[10]|= ( i_pcr << 7  )&0x80;
}

#if 0
static void TSSetConstraints( sout_mux_t *p_mux, sout_buffer_chain_t *c,
                              mtime_t i_length, int i_bitrate_min,
                              int i_bitrate_max )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    sout_buffer_chain_t s = *c;

    int i_packets = 0;
    int i_packets_min = 0;
    int i_packets_max = 0;

    if( i_length <= 0 )
    {
        return;
    }

    i_packets     = c->i_depth;
    i_packets_min = ( (int64_t)i_bitrate_min * i_length / 8 / 1000000  + 187 ) / 188;
    i_packets_max = ( (int64_t)i_bitrate_max * i_length / 8 / 1000000  + 187 ) / 188;

    if( i_packets < i_packets_min && i_packets_min > 0 )
    {
        block_t *p_pk;
        int i_div = ( i_packets_min - i_packets ) / i_packets;
        int i_mod = ( i_packets_min - i_packets ) % i_packets;
        int i_rest = 0;

        /* We need to pad with null packets (pid=0x1fff)
         * We try to melt null packets with true packets */
        msg_Dbg( p_mux,
                 "packets=%d but min=%d -> adding %d packets of padding",
                 i_packets, i_packets_min, i_packets_min - i_packets );

        BufferChainInit( c );
        while( ( p_pk = BufferChainGet( &s ) ) )
        {
            int i, i_null;

            BufferChainAppend( c, p_pk );

            i_null = i_div + ( i_rest + i_mod ) / i_packets;

            for( i = 0; i < i_null; i++ )
            {
                block_t *p_null;

                p_null = sout_BufferNew( p_mux->p_sout, 188 );
                p_null->p_buffer[0] = 0x47;
                p_null->p_buffer[1] = 0x1f;
                p_null->p_buffer[2] = 0xff;
                p_null->p_buffer[3] = 0x10 | p_sys->i_null_continuity_counter;
                memset( &p_null->p_buffer[4], 0, 184 );
                p_sys->i_null_continuity_counter =
                    ( p_sys->i_null_continuity_counter + 1 ) % 16;

                BufferChainAppend( c, p_null );
            }

            i_rest = ( i_rest + i_mod ) % i_packets;
        }
    }
    else if( i_packets > i_packets_max && i_packets_max > 0 )
    {
        block_t *p_pk;
        int           i;

        /* Arg, we need to drop packets, I don't do something clever (like
         * dropping complete pid, b frames, ... ), I just get the right amount
         * of packets and discard the others */
        msg_Warn( p_mux,
                  "packets=%d but max=%d -> removing %d packets -> stream broken",
                  i_packets, i_packets_max, i_packets - i_packets_max );

        BufferChainInit( c );
        for( i = 0; i < i_packets_max; i++ )
        {
            BufferChainAppend( c, BufferChainGet( &s ) );
        }

        while( ( p_pk = BufferChainGet( &s ) ) )
        {
            sout_BufferDelete( p_mux->p_sout, p_pk );
        }
    }
}
#endif

static void PEStoTS( sout_instance_t *p_sout,
                     sout_buffer_chain_t *c, block_t *p_pes,
                     ts_stream_t *p_stream )
{
    uint8_t *p_data;
    int     i_size;
    int     b_new_pes;

    /* get PES total size */
    i_size = p_pes->i_buffer;
    p_data = p_pes->p_buffer;

    b_new_pes = VLC_TRUE;

    for( ;; )
    {
        int           b_adaptation_field;
        int           i_copy;
        block_t *p_ts;

        p_ts = block_New( p_sout, 188 );
        /* write header
         * 8b   0x47    sync byte
         * 1b           transport_error_indicator
         * 1b           payload_unit_start
         * 1b           transport_priority
         * 13b          pid
         * 2b           transport_scrambling_control
         * 2b           if adaptation_field 0x03 else 0x01
         * 4b           continuity_counter
         */

        i_copy    = __MIN( i_size, 184 );
        b_adaptation_field = i_size < 184 ? VLC_TRUE : VLC_FALSE;

        p_ts->p_buffer[0] = 0x47;
        p_ts->p_buffer[1] = ( b_new_pes ? 0x40 : 0x00 )|
                            ( ( p_stream->i_pid >> 8 )&0x1f );
        p_ts->p_buffer[2] = p_stream->i_pid & 0xff;
        p_ts->p_buffer[3] = ( b_adaptation_field ? 0x30 : 0x10 )|
                            p_stream->i_continuity_counter;

        b_new_pes = VLC_FALSE;
        p_stream->i_continuity_counter = (p_stream->i_continuity_counter+1)%16;

        if( b_adaptation_field )
        {
            int i_stuffing = 184 - i_copy;
            int i;

            p_ts->p_buffer[4] = i_stuffing - 1;
            if( i_stuffing > 1 )
            {
                p_ts->p_buffer[5] = 0x00;
                if( p_stream->b_discontinuity )
                {
                    p_ts->p_buffer[5] |= 0x80;
                    p_stream->b_discontinuity = VLC_FALSE;
                }
                for( i = 6; i < 6 + i_stuffing - 2; i++ )
                {
                    p_ts->p_buffer[i] = 0xff;
                }
            }
        }
        /* copy payload */
        memcpy( &p_ts->p_buffer[188 - i_copy], p_data, i_copy );
        p_data += i_copy;
        i_size -= i_copy;

        BufferChainAppend( c, p_ts );

        if( i_size <= 0 )
        {
            block_t *p_next = p_pes->p_next;

            p_pes->p_next = NULL;
            block_Release( p_pes );
            if( p_next == NULL )
            {
                break;
            }
            b_new_pes = VLC_TRUE;
            p_pes = p_next;
            i_size = p_pes->i_buffer;
            p_data = p_pes->p_buffer;
        }
    }

    return;
}

static block_t *WritePSISection( sout_instance_t *p_sout,
                                       dvbpsi_psi_section_t* p_section )
{
    block_t   *p_psi, *p_first = NULL;

    while( p_section )
    {
        int             i_size;

        i_size =  (uint32_t)( p_section->p_payload_end - p_section->p_data )+
                  ( p_section->b_syntax_indicator ? 4 : 0 );

        p_psi = block_New( p_sout, i_size + 1 );
        p_psi->i_pts = 0;
        p_psi->i_dts = 0;
        p_psi->i_length = 0;
        p_psi->i_buffer = i_size + 1;

        p_psi->p_buffer[0] = 0; /* pointer */
        memcpy( p_psi->p_buffer + 1,
                p_section->p_data,
                i_size );

        block_ChainAppend( &p_first, p_psi );

        p_section = p_section->p_next;
    }

    return( p_first );
}

static void GetPAT( sout_mux_t *p_mux,
                    sout_buffer_chain_t *c )
{
    sout_mux_sys_t       *p_sys = p_mux->p_sys;
    block_t              *p_pat;
    dvbpsi_pat_t         pat;
    dvbpsi_psi_section_t *p_section;
    int i;

    dvbpsi_InitPAT( &pat, p_sys->i_tsid, p_sys->i_pat_version_number,
                    1 );      /* b_current_next */
    /* add all programs */
    for ( i = 0; i < p_sys->i_num_pmt; i++ )
        dvbpsi_PATAddProgram( &pat,
                              p_sys->i_pmt_program_number[i],
                              p_sys->pmt[i].i_pid );

    p_section = dvbpsi_GenPATSections( &pat,
                                       0 );     /* max program per section */

    p_pat = WritePSISection( p_mux->p_sout, p_section );

    PEStoTS( p_mux->p_sout, c, p_pat, &p_sys->pat );

    dvbpsi_DeletePSISections( p_section );
    dvbpsi_EmptyPAT( &pat );
}

static uint32_t GetDescriptorLength24b( int i_length )
{
    uint32_t i_l1, i_l2, i_l3;

    i_l1 = i_length&0x7f;
    i_l2 = ( i_length >> 7 )&0x7f;
    i_l3 = ( i_length >> 14 )&0x7f;

    return( 0x808000 | ( i_l3 << 16 ) | ( i_l2 << 8 ) | i_l1 );
}

static void GetPMT( sout_mux_t *p_mux, sout_buffer_chain_t *c )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    block_t   *p_pmt[MAX_PMT];
    block_t   *p_sdt;

    dvbpsi_sdt_t        sdt;
    dvbpsi_pmt_es_t     *p_es;
    dvbpsi_psi_section_t *p_section[MAX_PMT], *p_section2;
    dvbpsi_sdt_service_t *p_service;
    char            *psz_sdt_desc;
    int             i_pidinput;

    int             i_stream;
    int             i;
    int             *p_usepid = NULL;

    if( p_sys->dvbpmt == NULL )
        p_sys->dvbpmt = malloc( p_sys->i_num_pmt * sizeof(dvbpsi_pmt_t) );
#ifdef HAVE_DVBPSI_SDT
    if( p_sys->b_sdt )
        dvbpsi_InitSDT( &sdt, p_sys->i_tsid, 1, 1, p_sys->i_netid );
#endif

    for( i = 0; i < p_sys->i_num_pmt; i++ )
    {
        dvbpsi_InitPMT( &p_sys->dvbpmt[i],
                        p_sys->i_pmt_program_number[i],   /* program number */
                        p_sys->i_pmt_version_number,
                        1,      /* b_current_next */
                        p_sys->i_pcr_pid );

#ifdef HAVE_DVBPSI_SDT
        if( p_sys->b_sdt )
        {
            p_service = dvbpsi_SDTAddService( &sdt,
                p_sys->i_pmt_program_number[i],  /* service id */
                0,         /* eit schedule */
                0,         /* eit present */
                4,         /* running status ("4=RUNNING") */
                0 );       /* free ca */

#define psz_sdtprov p_sys->sdt_descriptors[i].psz_provider
#define psz_sdtserv p_sys->sdt_descriptors[i].psz_service_name

            /* FIXME: Ineffecient malloc's & ugly code......  */
            if( psz_sdtprov != NULL && psz_sdtserv != NULL )
            {
                psz_sdt_desc = malloc( 3 + strlen(psz_sdtprov)
                                         + strlen(psz_sdtserv) );
                psz_sdt_desc[0] = 0x01; /* digital television service */

                /* service provider name length */
                psz_sdt_desc[1] = (char)strlen(psz_sdtprov);
                memcpy( &psz_sdt_desc[2], psz_sdtprov, strlen(psz_sdtprov) );

                /* service name length */
                psz_sdt_desc[ 2 + strlen(psz_sdtprov) ]
                    = (char)strlen(psz_sdtserv);
                memcpy( &psz_sdt_desc[3+strlen(psz_sdtprov)], psz_sdtserv,
                        strlen(psz_sdtserv) );

                dvbpsi_SDTServiceAddDescriptor( p_service, 0x48,
                        3 + strlen(psz_sdtprov) + strlen(psz_sdtserv),
                        psz_sdt_desc );
                free( psz_sdt_desc );
            }
#undef psz_sdtprov
#undef psz_sdtserv
        }
#endif
    }

    if( p_sys->i_mpeg4_streams > 0 )
    {
        uint8_t iod[4096];
        bits_buffer_t bits;
        bits_buffer_t bits_fix_IOD;

        /* Make valgrind happy : it works at byte level not bit one so
         * bit_write confuse it (but DON'T CHANGE the way that bit_write is
         * working (needed when fixing some bits) */
        memset( iod, 0, 4096 );

        bits_initwrite( &bits, 4096, iod );
        /* IOD_label_scope */
        bits_write( &bits, 8,   0x11 );
        /* IOD_label */
        bits_write( &bits, 8,   0x01 );
        /* InitialObjectDescriptor */
        bits_align( &bits );
        bits_write( &bits, 8,   0x02 );     /* tag */
        bits_fix_IOD = bits;    /* save states to fix length later */
        bits_write( &bits, 24,
            GetDescriptorLength24b( 0 ) );  /* variable length (fixed later) */
        bits_write( &bits, 10,  0x01 );     /* ObjectDescriptorID */
        bits_write( &bits, 1,   0x00 );     /* URL Flag */
        bits_write( &bits, 1,   0x00 );     /* includeInlineProfileLevelFlag */
        bits_write( &bits, 4,   0x0f );     /* reserved */
        bits_write( &bits, 8,   0xff );     /* ODProfile (no ODcapability ) */
        bits_write( &bits, 8,   0xff );     /* sceneProfile */
        bits_write( &bits, 8,   0xfe );     /* audioProfile (unspecified) */
        bits_write( &bits, 8,   0xfe );     /* visualProfile( // ) */
        bits_write( &bits, 8,   0xff );     /* graphicProfile (no ) */
        for( i_stream = 0; i_stream < p_mux->i_nb_inputs; i_stream++ )
        {
            ts_stream_t *p_stream;
            p_stream = (ts_stream_t*)p_mux->pp_inputs[i_stream]->p_sys;

            if( p_stream->i_stream_id == 0xfa ||
                p_stream->i_stream_id == 0xfb ||
                p_stream->i_stream_id == 0xfe )
            {
                bits_buffer_t bits_fix_ESDescr, bits_fix_Decoder;
                /* ES descriptor */
                bits_align( &bits );
                bits_write( &bits, 8,   0x03 );     /* ES_DescrTag */
                bits_fix_ESDescr = bits;
                bits_write( &bits, 24,
                            GetDescriptorLength24b( 0 ) ); /* variable size */
                bits_write( &bits, 16,  p_stream->i_es_id );
                bits_write( &bits, 1,   0x00 );     /* streamDependency */
                bits_write( &bits, 1,   0x00 );     /* URL Flag */
                bits_write( &bits, 1,   0x00 );     /* OCRStreamFlag */
                bits_write( &bits, 5,   0x1f );     /* streamPriority */

                /* DecoderConfigDesciptor */
                bits_align( &bits );
                bits_write( &bits, 8,   0x04 ); /* DecoderConfigDescrTag */
                bits_fix_Decoder = bits;
                bits_write( &bits, 24,  GetDescriptorLength24b( 0 ) );
                if( p_stream->i_stream_type == 0x10 )
                {
                    bits_write( &bits, 8, 0x20 );   /* Visual 14496-2 */
                    bits_write( &bits, 6, 0x04 );   /* VisualStream */
                }
                else if( p_stream->i_stream_type == 0x1b )
                {
                    bits_write( &bits, 8, 0x21 );   /* Visual 14496-2 */
                    bits_write( &bits, 6, 0x04 );   /* VisualStream */
                }
                else if( p_stream->i_stream_type == 0x11  || p_stream->i_stream_type == 0x0f )
                {
                    bits_write( &bits, 8, 0x40 );   /* Audio 14496-3 */
                    bits_write( &bits, 6, 0x05 );   /* AudioStream */
                }
                else if( p_stream->i_stream_type == 0x12 &&
                         p_stream->i_codec == VLC_FOURCC('s','u','b','t') )
                {
                    bits_write( &bits, 8, 0x0B );   /* Text Stream */
                    bits_write( &bits, 6, 0x04 );   /* VisualStream */
                }
                else
                {
                    bits_write( &bits, 8, 0x00 );
                    bits_write( &bits, 6, 0x00 );

                    msg_Err( p_mux->p_sout,"Unsupported stream_type => "
                             "broken IOD" );
                }
                bits_write( &bits, 1,   0x00 );         /* UpStream */
                bits_write( &bits, 1,   0x01 );         /* reserved */
                bits_write( &bits, 24,  1024 * 1024 );  /* bufferSizeDB */
                bits_write( &bits, 32,  0x7fffffff );   /* maxBitrate */
                bits_write( &bits, 32,  0 );            /* avgBitrate */

                if( p_stream->i_decoder_specific_info > 0 )
                {
                    int i;
                    /* DecoderSpecificInfo */
                    bits_align( &bits );
                    bits_write( &bits, 8,   0x05 ); /* tag */
                    bits_write( &bits, 24, GetDescriptorLength24b(
                                p_stream->i_decoder_specific_info ) );
                    for( i = 0; i < p_stream->i_decoder_specific_info; i++ )
                    {
                        bits_write( &bits, 8,
                            ((uint8_t*)p_stream->p_decoder_specific_info)[i] );
                    }
                }
                /* fix Decoder length */
                bits_write( &bits_fix_Decoder, 24,
                            GetDescriptorLength24b( bits.i_data -
                            bits_fix_Decoder.i_data - 3 ) );

                /* SLConfigDescriptor : predefined (0x01) */
                bits_align( &bits );
                bits_write( &bits, 8,   0x06 ); /* tag */
                bits_write( &bits, 24,  GetDescriptorLength24b( 8 ) );
                bits_write( &bits, 8,   0x01 );/* predefined */
                bits_write( &bits, 1,   0 );   /* durationFlag */
                bits_write( &bits, 32,  0 );   /* OCRResolution */
                bits_write( &bits, 8,   0 );   /* OCRLength */
                bits_write( &bits, 8,   0 );   /* InstantBitrateLength */
                bits_align( &bits );

                /* fix ESDescr length */
                bits_write( &bits_fix_ESDescr, 24,
                            GetDescriptorLength24b( bits.i_data -
                            bits_fix_ESDescr.i_data - 3 ) );
            }
        }
        bits_align( &bits );
        /* fix IOD length */
        bits_write( &bits_fix_IOD, 24,
                    GetDescriptorLength24b( bits.i_data -
                                            bits_fix_IOD.i_data - 3 ) );

#if 0//def HAVE_BSEARCH /* FIXME!!! This can't possibly work */
        i_pidinput = p_mux->pp_inputs[i]->p_fmt->i_id;
        p_usepid = bsearch( &i_pidinput, p_sys->pmtmap, p_sys->i_pmtslots,
                            sizeof(pmt_map_t), intcompare );
        p_usepid = bsearch( &p_usepid, p_sys->pmtmap, p_sys->i_num_pmt,
                            sizeof(pmt_map_t), pmtcompare );
        if( p_usepid != NULL )
            dvbpsi_PMTAddDescriptor(
                    &p_sys->dvbpmt[((pmt_map_t *)p_usepid)->i_prog], 0x1d,
                    bits.i_data, bits.p_data );
        else
            msg_Err( p_mux, "Received an unmapped PID" );
#else
        dvbpsi_PMTAddDescriptor( &p_sys->dvbpmt[0], 0x1d, bits.i_data,
                                 bits.p_data );
#endif
    }

    for( i_stream = 0; i_stream < p_mux->i_nb_inputs; i_stream++ )
    {
        ts_stream_t *p_stream;

        p_stream = (ts_stream_t *)p_mux->pp_inputs[i_stream]->p_sys;

#ifdef HAVE_BSEARCH
        i_pidinput = p_mux->pp_inputs[i_stream]->p_fmt->i_id;
        p_usepid = bsearch( &i_pidinput, p_sys->pmtmap, p_sys->i_pmtslots,
                            sizeof(pmt_map_t), intcompare );

        if( p_usepid != NULL )
            p_es = dvbpsi_PMTAddES(
                    &p_sys->dvbpmt[((pmt_map_t *)p_usepid)->i_prog],
                    p_stream->i_stream_type, p_stream->i_pid );
        else
            /* If there's an error somewhere, dump it to the first pmt */
#endif
            p_es = dvbpsi_PMTAddES( &p_sys->dvbpmt[0], p_stream->i_stream_type,
                                    p_stream->i_pid );

        if( p_stream->i_stream_id == 0xfa || p_stream->i_stream_id == 0xfb )
        {
            uint8_t     es_id[2];

            /* SL descriptor */
            es_id[0] = (p_stream->i_es_id >> 8)&0xff;
            es_id[1] = (p_stream->i_es_id)&0xff;
            dvbpsi_PMTESAddDescriptor( p_es, 0x1f, 2, es_id );
        }
        else if( p_stream->i_stream_type == 0xa0 )
        {
            uint8_t data[512];
            int i_extra = __MIN( p_stream->i_decoder_specific_info, 502 );

            /* private DIV3 descripor */
            memcpy( &data[0], &p_stream->i_bih_codec, 4 );
            data[4] = ( p_stream->i_bih_width >> 8 )&0xff;
            data[5] = ( p_stream->i_bih_width      )&0xff;
            data[6] = ( p_stream->i_bih_height>> 8 )&0xff;
            data[7] = ( p_stream->i_bih_height     )&0xff;
            data[8] = ( i_extra >> 8 )&0xff;
            data[9] = ( i_extra      )&0xff;
            if( i_extra > 0 )
            {
                memcpy( &data[10], p_stream->p_decoder_specific_info, i_extra );
            }

            /* 0xa0 is private */
            dvbpsi_PMTESAddDescriptor( p_es, 0xa0, i_extra + 10, data );
        }
        else if( p_stream->i_stream_type == 0x81 )
        {
            uint8_t format[4] = { 0x41, 0x43, 0x2d, 0x33 };

            /* "registration" descriptor : "AC-3" */
            dvbpsi_PMTESAddDescriptor( p_es, 0x05, 4, format );
        }
        else if( p_stream->i_codec == VLC_FOURCC('d','t','s',' ') )
        {
            /* DTS registration descriptor (ETSI TS 101 154 Annex F) */

            /* DTS format identifier, frame size 1024 - FIXME */
            uint8_t data[4] = { 0x44, 0x54, 0x53, 0x32 };
            dvbpsi_PMTESAddDescriptor( p_es, 0x05, 4, data );
        }
        else if( p_stream->i_codec == VLC_FOURCC('t','e','l','x') )
        {
            dvbpsi_PMTESAddDescriptor( p_es, 0x56,
                                       p_stream->i_decoder_specific_info,
                                       p_stream->p_decoder_specific_info );
        }
#ifdef _DVBPSI_DR_59_H_
        else if( p_stream->i_codec == VLC_FOURCC('d','v','b','s') )
        {
            /* DVB subtitles */
            dvbpsi_subtitling_dr_t descr;
            dvbpsi_subtitle_t sub;
            dvbpsi_descriptor_t *p_descr;

            memcpy( sub.i_iso6392_language_code, p_stream->lang, 3 );
            sub.i_subtitling_type = 0x10; /* no aspect-ratio criticality */
            sub.i_composition_page_id = p_stream->i_es_id & 0xFF;
            sub.i_ancillary_page_id = p_stream->i_es_id >> 16;

            descr.i_subtitles_number = 1;
            descr.p_subtitle[0] = sub;

            p_descr = dvbpsi_GenSubtitlingDr( &descr, 0 );
            /* Work around bug in old libdvbpsi */ p_descr->i_length = 8;
            dvbpsi_PMTESAddDescriptor( p_es, p_descr->i_tag,
                                       p_descr->i_length, p_descr->p_data );
            continue;
        }
#endif /* _DVBPSI_DR_59_H_ */

        if( p_stream->lang[0] != 0 )
        {
            uint8_t data[4];

            /* I construct the content myself, way faster than looking at
             * over complicated/mind broken libdvbpsi way */
            data[0] = p_stream->lang[0];
            data[1] = p_stream->lang[1];
            data[2] = p_stream->lang[2];
            data[3] = 0x00; /* audio type: 0x00 undefined */

            dvbpsi_PMTESAddDescriptor( p_es, 0x0a, 4, data );
        }
    }

    for( i = 0; i < p_sys->i_num_pmt; i++ )
    {
        p_section[i] = dvbpsi_GenPMTSections( &p_sys->dvbpmt[i] );
        p_pmt[i] = WritePSISection( p_mux->p_sout, p_section[i] );
        PEStoTS( p_mux->p_sout, c, p_pmt[i], &p_sys->pmt[i] );
        dvbpsi_DeletePSISections( p_section[i] );
        dvbpsi_EmptyPMT( &p_sys->dvbpmt[i] );
    }

#ifdef HAVE_DVBPSI_SDT
    if( p_sys->b_sdt )
    {
        p_section2 = dvbpsi_GenSDTSections( &sdt );
        p_sdt = WritePSISection( p_mux->p_sout, p_section2 );
        PEStoTS( p_mux->p_sout, c, p_sdt, &p_sys->sdt );
        dvbpsi_DeletePSISections( p_section2 );
        dvbpsi_EmptySDT( &sdt );
    }
#endif
}
