/*****************************************************************************
 * ty.c - TiVo ty stream video demuxer for VLC
 *****************************************************************************
 * Copyright (C) 2005 VLC authors and VideoLAN
 * Copyright (C) 2005 by Neal Symms (tivo@freakinzoo.com) - February 2005
 * based on code by Christopher Wingert for tivo-mplayer
 * tivo(at)wingert.org, February 2003
 *
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
 *
 * CODE CHANGES:
 * v1.0.0 - 24-Feb-2005 - Initial release - Series 1 support ONLY!
 * v1.0.1 - 25-Feb-2005 - Added fix for bad GOP headers - Neal
 * v1.0.2 - 26-Feb-2005 - No longer require "seekable" input stream - Neal
 * v2.0.0 - 21-Mar-2005 - Series 2 support!  No AC-3 on S2 DTivo yet.
 * v2.1.0 - 22-Mar-2005 - Support for AC-3 on S2 DTivo (long ac3 packets)
 * v3.0.0 - 14-Jul-2005 - Support for skipping fwd/back via VLC hotkeys
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
#include <vlc_demux.h>
#include <vlc_codec.h>
#include <vlc_meta.h>
#include <vlc_input_item.h>
#include "../codec/cc.h"

#include "mpeg/pes.h"

#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("TY") )
    set_description(N_("TY Stream audio/video demux"))
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_capability("demux", 6)
    /* FIXME: there seems to be a segfault when using PVR access
     * and TY demux has a bigger priority than PS
     * Something must be wrong.
     */
    set_callbacks( Open, Close )
    add_shortcut("ty", "tivo")
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int, va_list );

#define SERIES1_PES_LENGTH  (11)    /* length of audio PES hdr on S1 */
#define SERIES2_PES_LENGTH  (16)    /* length of audio PES hdr on S2 */
#define AC3_PES_LENGTH      (14)    /* length of audio PES hdr for AC3 */
#define VIDEO_PES_LENGTH    (16)    /* length of video PES header */
#define DTIVO_PTS_OFFSET    (6)     /* offs into PES for MPEG PTS on DTivo */
#define SA_PTS_OFFSET       (9)     /* offset into PES for MPEG PTS on SA */
#define AC3_PTS_OFFSET      (9)     /* offset into PES for AC3 PTS on DTivo */
#define VIDEO_PTS_OFFSET    (9)     /* offset into PES for video PTS on all */
#define AC3_PKT_LENGTH      (1536)  /* size of TiVo AC3 pkts (w/o PES hdr) */
static const uint8_t ty_VideoPacket[] = { 0x00, 0x00, 0x01, 0xe0 };
static const uint8_t ty_MPEGAudioPacket[] = { 0x00, 0x00, 0x01, 0xc0 };
static const uint8_t ty_AC3AudioPacket[] = { 0x00, 0x00, 0x01, 0xbd };

#define CHUNK_PEEK_COUNT    (3)         /* number of chunks to probe */

/* packet types for reference:
 2/c0: audio data continued
 3/c0: audio packet header (PES header)
 4/c0: audio data (S/A only?)
 9/c0: audio packet header, AC-3 audio
 2/e0: video data continued
 6/e0: video packet header (PES header)
 7/e0: video sequence header start
 8/e0: video I-frame header start
 a/e0: video P-frame header start
 b/e0: video B-frame header start
 c/e0: video GOP header start
 e/01: closed-caption data
 e/02: Extended data services data
 e/03: ipreview data ("thumbs up to record" signal)
 e/05: UK Teletext
*/

#define TIVO_PES_FILEID   ( 0xf5467abd )
#define TIVO_PART_LENGTH  ( 0x20000000 )    /* 536,870,912 bytes */
#define CHUNK_SIZE        ( 128 * 1024 )

typedef struct
{
  long l_rec_size;
  uint8_t ex[2];
  uint8_t rec_type;
  uint8_t subrec_type;
  bool b_ext;
  uint64_t l_ty_pts;            /* TY PTS in the record header */
} ty_rec_hdr_t;

typedef struct
{
    uint64_t l_timestamp;
    uint8_t chunk_bitmask[8];
} ty_seq_table_t;

typedef enum
{
    TIVO_TYPE_UNKNOWN,
    TIVO_TYPE_SA,
    TIVO_TYPE_DTIVO
} tivo_type_t;

typedef enum
{
    TIVO_SERIES_UNKNOWN,
    TIVO_SERIES1,
    TIVO_SERIES2
} tivo_series_t;

typedef enum
{
    TIVO_AUDIO_UNKNOWN,
    TIVO_AUDIO_AC3,
    TIVO_AUDIO_MPEG
} tivo_audio_t;

#define XDS_MAX_DATA_SIZE (32)
typedef enum
{
    XDS_CLASS_CURRENT        = 0,
    XDS_CLASS_FUTURE         = 1,
    XDS_CLASS_CHANNEL        = 2,
    XDS_CLASS_MISCELLANEOUS  = 3,
    XDS_CLASS_PUBLIC_SERVICE = 4,
    XDS_CLASS_RESERVED       = 5,
    XDS_CLASS_UNDEFINED      = 6,
    XDS_CLASS_OTHER          = 7,

    XDS_MAX_CLASS_COUNT
} xds_class_t;
typedef struct
{
    bool b_started;
    size_t     i_data;
    uint8_t    p_data[XDS_MAX_DATA_SIZE];
    int        i_sum;
} xds_packet_t;
typedef enum
{
    XDS_META_PROGRAM_RATING_NONE,
    XDS_META_PROGRAM_RATING_MPAA,
    XDS_META_PROGRAM_RATING_TPG,
    /* TODO add CA/CE rating */
} xds_meta_program_rating_t;
typedef struct
{
    char *psz_name;
    xds_meta_program_rating_t rating;
    char *psz_rating;
    /* Add the other fields once I have the samples */
} xds_meta_program_t;
typedef struct
{
    char *psz_channel_name;
    char *psz_channel_call_letter;
    char *psz_channel_number;

    xds_meta_program_t  current;
    xds_meta_program_t  future;
} xds_meta_t;
typedef struct
{
    /* Are we in XDS mode */
    bool b_xds;

    /* Current class type */
    xds_class_t i_class;
    int         i_type;
    bool  b_future;

    /* */
    xds_packet_t pkt[XDS_MAX_CLASS_COUNT][128]; /* XXX it is way too much, but simpler */

    /* */
    bool  b_meta_changed;
    xds_meta_t  meta;

} xds_t;

typedef struct
{
  es_out_id_t *p_video;               /* ptr to video codec */
  es_out_id_t *p_audio;               /* holds either ac3 or mpeg codec ptr */

  cc_data_t   cc;
  es_out_id_t *p_cc[4];

  xds_t       xds;

  int             i_cur_chunk;
  int             i_stuff_cnt;
  size_t          i_stream_size;      /* size of input stream (if known) */
  //uint64_t        l_program_len;      /* length of this stream in msec */
  bool      b_seekable;         /* is this stream seekable? */
  bool      b_have_master;      /* are master chunks present? */
  tivo_type_t     tivo_type;          /* tivo type (SA / DTiVo) */
  tivo_series_t   tivo_series;        /* Series1 or Series2 */
  tivo_audio_t    audio_type;         /* AC3 or MPEG */
  int             i_Pes_Length;       /* Length of Audio PES header */
  int             i_Pts_Offset;       /* offset into audio PES of PTS */
  uint8_t         pes_buffer[20];     /* holds incomplete pes headers */
  int             i_pes_buf_cnt;      /* how many bytes in our buffer */
  size_t          l_ac3_pkt_size;     /* len of ac3 pkt we've seen so far */
  uint64_t        l_last_ty_pts;      /* last TY timestamp we've seen */
  //vlc_tick_t      l_last_ty_pts_sync; /* audio PTS at time of last TY PTS */
  uint64_t        l_first_ty_pts;     /* first TY PTS in this master chunk */
  uint64_t        l_final_ty_pts;     /* final TY PTS in this master chunk */
  unsigned        i_seq_table_size;   /* number of entries in SEQ table */
  unsigned        i_bits_per_seq_entry; /* # of bits in SEQ table bitmask */

  vlc_tick_t      lastAudioPTS;
  vlc_tick_t      lastVideoPTS;

  ty_rec_hdr_t    *rec_hdrs;          /* record headers array */
  int             i_cur_rec;          /* current record in this chunk */
  int             i_num_recs;         /* number of recs in this chunk */
  int             i_seq_rec;          /* record number where seq start is */
  ty_seq_table_t  *seq_table;         /* table of SEQ entries from mstr chk */
  bool      eof;
  bool      b_first_chunk;
} demux_sys_t;

static int get_chunk_header(demux_t *);
static vlc_tick_t get_pts( const uint8_t *buf );
static int find_es_header( const uint8_t *header,
                           const uint8_t *buffer, int i_search_len );
static int ty_stream_seek_pct(demux_t *p_demux, double seek_pct);
static int ty_stream_seek_time(demux_t *, uint64_t);

static ty_rec_hdr_t *parse_chunk_headers( const uint8_t *p_buf,
                                          int i_num_recs, int *pi_payload_size);
static int probe_stream(demux_t *p_demux);
static void analyze_chunk(demux_t *p_demux, const uint8_t *p_chunk);
static int  parse_master(demux_t *p_demux);

static int DemuxRecVideo( demux_t *p_demux, ty_rec_hdr_t *rec_hdr, block_t *p_block_in );
static int DemuxRecAudio( demux_t *p_demux, ty_rec_hdr_t *rec_hdr, block_t *p_block_in );
static int DemuxRecCc( demux_t *p_demux, ty_rec_hdr_t *rec_hdr, block_t *p_block_in );

static void DemuxDecodeXds( demux_t *p_demux, uint8_t d1, uint8_t d2 );

static void XdsInit( xds_t * );
static void XdsExit( xds_t * );

#define TY_ES_GROUP (1)

/*
 * Open: check file and initialize demux structures
 *
 * here's what we do:
 * 1. peek at the first 12 bytes of the stream for the
 *    magic TiVo PART header & stream type & chunk size
 * 2. if it's not there, error with VLC_EGENERIC
 * 3. set up video (mpgv) codec
 * 4. return VLC_SUCCESS
 */
static int Open(vlc_object_t *p_this)
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys;
    es_format_t fmt;
    const uint8_t *p_peek;
    int i;

    /* peek at the first 12 bytes. */
    /* for TY streams, they're always the same */
    if( vlc_stream_Peek( p_demux->s, &p_peek, 12 ) < 12 )
        return VLC_EGENERIC;

    if ( U32_AT(p_peek) != TIVO_PES_FILEID ||
         U32_AT(&p_peek[4]) != 0x02 ||
         U32_AT(&p_peek[8]) != CHUNK_SIZE )
    {
        if( !p_demux->obj.force &&
            !demux_IsPathExtension( p_demux, ".ty" ) &&
            !demux_IsPathExtension( p_demux, ".ty+" ) )
            return VLC_EGENERIC;
        msg_Warn( p_demux, "this does not look like a TY file, "
                           "continuing anyway..." );
    }

    /* at this point, we assume we have a valid TY stream */
    msg_Dbg( p_demux, "valid TY stream detected" );

    p_sys = malloc(sizeof(demux_sys_t));
    if( unlikely(p_sys == NULL) )
        return VLC_ENOMEM;

    /* Set exported functions */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    /* create our structure that will hold all data */
    p_demux->p_sys = p_sys;
    memset(p_sys, 0, sizeof(demux_sys_t));

    /* set up our struct (most were zero'd out with the memset above) */
    p_sys->b_first_chunk = true;
    p_sys->b_have_master = (U32_AT(p_peek) == TIVO_PES_FILEID);
    p_sys->lastAudioPTS  = VLC_TICK_INVALID;
    p_sys->lastVideoPTS  = VLC_TICK_INVALID;
    p_sys->i_stream_size = stream_Size(p_demux->s);
    p_sys->tivo_type = TIVO_TYPE_UNKNOWN;
    p_sys->audio_type = TIVO_AUDIO_UNKNOWN;
    p_sys->tivo_series = TIVO_SERIES_UNKNOWN;
    p_sys->i_Pes_Length = 0;
    p_sys->i_Pts_Offset = 0;
    p_sys->l_ac3_pkt_size = 0;

    /* see if this stream is seekable */
    vlc_stream_Control( p_demux->s, STREAM_CAN_SEEK, &p_sys->b_seekable );

    if (probe_stream(p_demux) != VLC_SUCCESS) {
        //TyClose(p_demux);
        return VLC_EGENERIC;
    }

    if (!p_sys->b_have_master)
      msg_Warn(p_demux, "No master chunk found; seeking will be limited.");

    /* register the proper audio codec */
    if (p_sys->audio_type == TIVO_AUDIO_MPEG) {
        es_format_Init( &fmt, AUDIO_ES, VLC_CODEC_MPGA );
    } else {
        es_format_Init( &fmt, AUDIO_ES, VLC_CODEC_A52 );
    }
    fmt.i_group = TY_ES_GROUP;
    p_sys->p_audio = es_out_Add( p_demux->out, &fmt );

    /* register the video stream */
    es_format_Init( &fmt, VIDEO_ES, VLC_CODEC_MPGV );
    fmt.i_group = TY_ES_GROUP;
    p_sys->p_video = es_out_Add( p_demux->out, &fmt );

    /* */
    for( i = 0; i < 4; i++ )
        p_sys->p_cc[i] = NULL;
    cc_Init( &p_sys->cc );

    XdsInit( &p_sys->xds );

    return VLC_SUCCESS;
}

/* =========================================================================== */
/* Demux: Read & Demux one record from the chunk
 *
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *
 * NOTE: I think we can return the number of packets sent instead of just 1.
 * that means we can demux an entire chunk and shoot it back (may be more efficient)
 * -- should try that some day :) --
 */
static int Demux( demux_t *p_demux )
{
    demux_sys_t  *p_sys = p_demux->p_sys;
    ty_rec_hdr_t *p_rec;
    block_t      *p_block_in = NULL;

    /*msg_Dbg(p_demux, "ty demux processing" );*/

    /* did we hit EOF earlier? */
    if( p_sys->eof )
        return VLC_DEMUXER_EOF;

    /*
     * what we do (1 record now.. maybe more later):
    * - use vlc_stream_Read() to read the chunk header & record headers
    * - discard entire chunk if it is a PART header chunk
    * - parse all the headers into record header array
    * - keep a pointer of which record we're on
    * - use vlc_stream_Block() to fetch each record
    * - parse out PTS from PES headers
    * - set PTS for data packets
    * - pass the data on to the proper codec via es_out_Send()

    * if this is the first time or
    * if we're at the end of this chunk, start a new one
    */
    /* parse the next chunk's record headers */
    if( p_sys->b_first_chunk || p_sys->i_cur_rec >= p_sys->i_num_recs )
    {
        if( get_chunk_header(p_demux) == 0 || p_sys->i_num_recs == 0 )
            return VLC_DEMUXER_EOF;
    }

    /*======================================================================
     * parse & send one record of the chunk
     *====================================================================== */
    p_rec = &p_sys->rec_hdrs[p_sys->i_cur_rec];

    if( !p_rec->b_ext )
    {
        const long l_rec_size = p_rec->l_rec_size;
        /*msg_Dbg(p_demux, "Record Type 0x%x/%02x %ld bytes",
                    subrec_type, p_rec->rec_type, l_rec_size );*/

        /* some normal records are 0 length, so check for that... */
        if( l_rec_size <= 0 )
        {
            /* no data in payload; we're done */
            p_sys->i_cur_rec++;
            return VLC_DEMUXER_SUCCESS;
        }

        /* read in this record's payload */
        if( !( p_block_in = vlc_stream_Block( p_demux->s, l_rec_size ) ) )
            return VLC_DEMUXER_EOF;

        /* set these as 'unknown' for now */
        p_block_in->i_pts =
        p_block_in->i_dts = VLC_TICK_INVALID;
    }
    /*else
    {
        -- don't read any data from the stream, data was in the record header --
        msg_Dbg(p_demux,
               "Record Type 0x%02x/%02x, ext data = %02x, %02x", subrec_type,
                p_rec->rec_type, p_rec->ex1, p_rec->ex2);
    }*/

    switch( p_rec->rec_type )
    {
        case 0xe0: /* video */
            DemuxRecVideo( p_demux, p_rec, p_block_in );
            break;

        case 0xc0: /* audio */
            DemuxRecAudio( p_demux, p_rec, p_block_in );
            break;

        case 0x01:
        case 0x02:
            /* closed captions/XDS */
            DemuxRecCc( p_demux, p_rec, p_block_in );
            break;

        default:
            msg_Dbg(p_demux, "Invalid record type 0x%02x", p_rec->rec_type );
            /* fall-through */

        case 0x03: /* tivo data services */
        case 0x05: /* unknown, but seen regularly */
            if( p_block_in )
                block_Release( p_block_in );
    }

    /* */
    p_sys->i_cur_rec++;
    return VLC_DEMUXER_SUCCESS;
}

/* Control */
static int Control(demux_t *p_demux, int i_query, va_list args)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double f, *pf;
    int64_t i64;

    /*msg_Info(p_demux, "control cmd %d", i_query);*/
    switch( i_query )
    {
    case DEMUX_CAN_SEEK:
        *va_arg( args, bool * ) = p_sys->b_seekable;
        return VLC_SUCCESS;

    case DEMUX_GET_POSITION:
        /* arg is 0.0 - 1.0 percent of overall file position */
        if( ( i64 = p_sys->i_stream_size ) > 0 )
        {
            pf = va_arg( args, double* );
            *pf = ((double)1.0) * vlc_stream_Tell( p_demux->s ) / (double) i64;
            return VLC_SUCCESS;
        }
        return VLC_EGENERIC;

    case DEMUX_SET_POSITION:
        /* arg is 0.0 - 1.0 percent of overall file position */
        f = va_arg( args, double );
        /* msg_Dbg(p_demux, "Control - set position to %2.3f", f); */
        if ((i64 = p_sys->i_stream_size) > 0)
            return ty_stream_seek_pct(p_demux, f);
        return VLC_EGENERIC;
    case DEMUX_GET_TIME:
        /* return TiVo timestamp */
        //*p_i64 = p_sys->lastAudioPTS - p_sys->firstAudioPTS;
        //*p_i64 = (p_sys->l_last_ty_pts / 1000) + (p_sys->lastAudioPTS -
        //    p_sys->l_last_ty_pts_sync);
        *va_arg(args, vlc_tick_t *) = VLC_TICK_FROM_NS(p_sys->l_last_ty_pts);
        return VLC_SUCCESS;
    case DEMUX_GET_LENGTH:    /* length of program in microseconds, 0 if unk */
        /* size / bitrate */
        *va_arg(args, vlc_tick_t *) = 0;
        return VLC_SUCCESS;
    case DEMUX_SET_TIME:      /* arg is time in microsecs */
        return ty_stream_seek_time(p_demux,
                                   NS_FROM_VLC_TICK(va_arg( args, vlc_tick_t )));
    case DEMUX_CAN_PAUSE:
    case DEMUX_SET_PAUSE_STATE:
    case DEMUX_CAN_CONTROL_PACE:
    case DEMUX_GET_PTS_DELAY:
        return demux_vaControlHelper( p_demux->s, 0, -1, 0, 1, i_query, args );
    case DEMUX_GET_FPS:
    default:
        return VLC_EGENERIC;
    }
}

/* Close */
static void Close( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    XdsExit( &p_sys->xds );
    cc_Exit( &p_sys->cc );
    free( p_sys->rec_hdrs );
    free( p_sys->seq_table );
    free(p_sys);
}


/* =========================================================================== */
/* Compute Presentation Time Stamp (PTS)
 * Assume buf points to beginning of PTS */
static vlc_tick_t get_pts( const uint8_t *buf )
{
    stime_t i_pts = GetPESTimestamp( buf );
    return FROM_SCALE_NZ(i_pts); /* convert PTS (90Khz clock) to microseconds */
}


/* =========================================================================== */
static int find_es_header( const uint8_t *header,
                           const uint8_t *buffer, int i_search_len )
{
    int count;

    for( count = 0; count < i_search_len; count++ )
    {
        if( !memcmp( &buffer[count], header, 4 ) )
            return count;
    }
    return -1;
}


/* =========================================================================== */
/* check if we have a full PES header, if not, then save what we have.
 * this is called when audio-start packets are encountered.
 * Returns:
 *     1 partial PES hdr found, some audio data found (buffer adjusted),
 *    -1 partial PES hdr found, no audio data found
 *     0 otherwise (complete PES found, pts extracted, pts set, buffer adjusted) */
/* TODO: HD support -- nothing known about those streams */
static int check_sync_pes( demux_t *p_demux, block_t *p_block,
                           int32_t offset, int32_t rec_len )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if ( offset < 0 || offset + p_sys->i_Pes_Length > rec_len )
    {
        /* entire PES header not present */
        msg_Dbg( p_demux, "PES header at %d not complete in record. storing.",
                 offset );
        /* save the partial pes header */
        if( offset < 0 )
        {
            /* no header found, fake some 00's (this works, believe me) */
            memset( p_sys->pes_buffer, 0, 4 );
            p_sys->i_pes_buf_cnt = 4;
            if( rec_len > 4 )
                msg_Err( p_demux, "PES header not found in record of %d bytes!",
                         rec_len );
            return -1;
        }
        /* copy the partial pes header we found */
        memcpy( p_sys->pes_buffer, p_block->p_buffer + offset,
                rec_len - offset );
        p_sys->i_pes_buf_cnt = rec_len - offset;

        if( offset > 0 )
        {
            /* PES Header was found, but not complete, so trim the end of this record */
            p_block->i_buffer -= rec_len - offset;
            return 1;
        }
        return -1;    /* partial PES, no audio data */
    }
    /* full PES header present, extract PTS */
    p_sys->lastAudioPTS = VLC_TICK_0 + get_pts( &p_block->p_buffer[ offset +
                                   p_sys->i_Pts_Offset ] );
    p_block->i_pts = p_sys->lastAudioPTS;
    /*msg_Dbg(p_demux, "Audio PTS %"PRId64, p_sys->lastAudioPTS );*/
    /* adjust audio record to remove PES header */
    memmove(p_block->p_buffer + offset, p_block->p_buffer + offset +
            p_sys->i_Pes_Length, rec_len - p_sys->i_Pes_Length);
    p_block->i_buffer -= p_sys->i_Pes_Length;
#if 0
    msg_Dbg(p_demux, "pes hdr removed; buffer len=%d and has "
             "%02x %02x %02x %02x %02x %02x %02x %02x "
             "%02x %02x %02x %02x %02x %02x %02x %02x", p_block->i_buffer,
             p_block->p_buffer[0], p_block->p_buffer[1],
             p_block->p_buffer[2], p_block->p_buffer[3],
             p_block->p_buffer[4], p_block->p_buffer[5],
             p_block->p_buffer[6], p_block->p_buffer[7],
             p_block->p_buffer[8], p_block->p_buffer[9],
             p_block->p_buffer[10], p_block->p_buffer[11],
             p_block->p_buffer[12], p_block->p_buffer[13],
             p_block->p_buffer[14], p_block->p_buffer[15]);
#endif
    return 0;
}

static int DemuxRecVideo( demux_t *p_demux, ty_rec_hdr_t *rec_hdr, block_t *p_block_in )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const int subrec_type = rec_hdr->subrec_type;
    const long l_rec_size = rec_hdr->l_rec_size;    // p_block_in->i_buffer might be better
    int esOffset1;
    int i;

    assert( rec_hdr->rec_type == 0xe0 );
    if( !p_block_in )
        return -1;

#if 0
    msg_Dbg(p_demux, "packet buffer has "
            "%02x %02x %02x %02x %02x %02x %02x %02x "
            "%02x %02x %02x %02x %02x %02x %02x %02x",
            p_block_in->p_buffer[0], p_block_in->p_buffer[1],
            p_block_in->p_buffer[2], p_block_in->p_buffer[3],
            p_block_in->p_buffer[4], p_block_in->p_buffer[5],
            p_block_in->p_buffer[6], p_block_in->p_buffer[7],
            p_block_in->p_buffer[8], p_block_in->p_buffer[9],
            p_block_in->p_buffer[10], p_block_in->p_buffer[11],
            p_block_in->p_buffer[12], p_block_in->p_buffer[13],
            p_block_in->p_buffer[14], p_block_in->p_buffer[15]);
#endif
    //if( subrec_type == 0x06 || subrec_type == 0x07 )
    if( subrec_type != 0x02 && subrec_type != 0x0c &&
        subrec_type != 0x08 && l_rec_size > 4 )
    {
        /* get the PTS from this packet if it has one.
         * on S1, only 0x06 has PES.  On S2, however, most all do.
         * Do NOT Pass the PES Header to the MPEG2 codec */
        esOffset1 = find_es_header( ty_VideoPacket, p_block_in->p_buffer, 5 );
        if( esOffset1 != -1 )
        {
            //msg_Dbg(p_demux, "Video PES hdr in pkt type 0x%02x at offset %d",
                //subrec_type, esOffset1);
            p_sys->lastVideoPTS = VLC_TICK_0 + get_pts(
                    &p_block_in->p_buffer[ esOffset1 + VIDEO_PTS_OFFSET ] );
            /*msg_Dbg(p_demux, "Video rec %d PTS %"PRId64, p_sys->i_cur_rec,
                        p_sys->lastVideoPTS );*/
            if (subrec_type != 0x06) {
                /* if we found a PES, and it's not type 6, then we're S2 */
                /* The packet will have video data (& other headers) so we
                 * chop out the PES header and send the rest */
                if (l_rec_size >= VIDEO_PES_LENGTH) {
                    p_block_in->p_buffer += VIDEO_PES_LENGTH + esOffset1;
                    p_block_in->i_buffer -= VIDEO_PES_LENGTH + esOffset1;
                } else {
                    msg_Dbg(p_demux, "video rec type 0x%02x has short PES"
                        " (%ld bytes)", subrec_type, l_rec_size);
                    /* nuke this block; it's too short, but has PES marker */
                    p_block_in->i_buffer = 0;
                }
            }
        }/* else
            msg_Dbg(p_demux, "No Video PES hdr in pkt type 0x%02x",
                subrec_type); */
    }

    if(subrec_type == 0x06 )
    {
        /* type 6 (S1 DTivo) has no data, so we're done */
        block_Release(p_block_in);
        return 0;
    }

    /* if it's not a continue blk, then set PTS */
    if( subrec_type != 0x02 )
    {
        /*msg_Dbg(p_demux, "Video rec %d type 0x%02X", p_sys->i_cur_rec,
                   subrec_type);*/
        /* if it's a GOP header, make sure it's legal
         * (if we have enough data) */
        /* Some ty files don't have this bit set
         * and it causes problems */
        if (subrec_type == 0x0c && l_rec_size >= 6)
            p_block_in->p_buffer[5] |= 0x08;
        /* store the TY PTS if there is one */
        if (subrec_type == 0x07) {
            p_sys->l_last_ty_pts = rec_hdr->l_ty_pts;
            /* should we use audio or video PTS? */
            //p_sys->l_last_ty_pts_sync = p_sys->lastAudioPTS;
        } else {
            /* yes I know this is a cheap hack.  It's the timestamp
               used for display and skipping fwd/back, so it
               doesn't have to be accurate to the millisecond.
               I adjust it here by roughly one 1/30 sec.  Yes it
               will be slightly off for UK streams, but it's OK.
             */
            p_sys->l_last_ty_pts += 35000000;
            //p_sys->l_last_ty_pts += 33366667;
        }
        /* set PTS for this block before we send */
        if (p_sys->lastVideoPTS != VLC_TICK_INVALID)
        {
            p_block_in->i_pts = p_sys->lastVideoPTS;
            /* PTS gets used ONCE.
             * Any subsequent frames we get BEFORE next PES
             * header will have their PTS computed in the codec */
            p_sys->lastVideoPTS = VLC_TICK_INVALID;
        }
    }

    /* Register the CC decoders when needed */
    uint64_t i_chans = p_sys->cc.i_608channels;
    for( i = 0; i_chans > 0; i++, i_chans >>= 1 )
    {
        if( (i_chans & 1) == 0 || p_sys->p_cc[i] )
            continue;

        static const char *ppsz_description[4] = {
            N_("Closed captions 1"),
            N_("Closed captions 2"),
            N_("Closed captions 3"),
            N_("Closed captions 4"),
        };

        es_format_t fmt;


        es_format_Init( &fmt, SPU_ES, VLC_CODEC_CEA608 );
        fmt.subs.cc.i_channel = i;
        fmt.psz_description = strdup( vlc_gettext(ppsz_description[i]) );
        fmt.i_group = TY_ES_GROUP;
        p_sys->p_cc[i] = es_out_Add( p_demux->out, &fmt );
        es_format_Clean( &fmt );

    }
    /* Send the CC data */
    if( p_block_in->i_pts != VLC_TICK_INVALID && p_sys->cc.i_data > 0 )
    {
        for( i = 0; i < 4; i++ )
        {
            if( p_sys->p_cc[i] )
            {
                block_t *p_cc = block_Alloc( p_sys->cc.i_data );
                p_cc->i_flags |= BLOCK_FLAG_TYPE_I;
                p_cc->i_pts = p_block_in->i_pts;
                memcpy( p_cc->p_buffer, p_sys->cc.p_data, p_sys->cc.i_data );

                es_out_Send( p_demux->out, p_sys->p_cc[i], p_cc );
            }
        }
        cc_Flush( &p_sys->cc );
    }

    //msg_Dbg(p_demux, "sending rec %d as video type 0x%02x",
            //p_sys->i_cur_rec, subrec_type);
    es_out_Send(p_demux->out, p_sys->p_video, p_block_in);
    return 0;
}
static int DemuxRecAudio( demux_t *p_demux, ty_rec_hdr_t *rec_hdr, block_t *p_block_in )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const int subrec_type = rec_hdr->subrec_type;
    const long l_rec_size = rec_hdr->l_rec_size;
    int esOffset1;

    assert( rec_hdr->rec_type == 0xc0 );
    if( !p_block_in )
        return -1;
#if 0
        int i;
        fprintf( stderr, "Audio Packet Header " );
        for( i = 0 ; i < 24 ; i++ )
            fprintf( stderr, "%2.2x ", p_block_in->p_buffer[i] );
        fprintf( stderr, "\n" );
#endif

    if( subrec_type == 2 )
    {
        /* SA or DTiVo Audio Data, no PES (continued block)
         * ================================================
         */

        /* continue PES if previous was incomplete */
        if (p_sys->i_pes_buf_cnt > 0)
        {
            const int i_need = p_sys->i_Pes_Length - p_sys->i_pes_buf_cnt;

            msg_Dbg(p_demux, "continuing PES header");
            /* do we have enough data to complete? */
            if (i_need >= l_rec_size)
            {
                /* don't have complete PES hdr; save what we have and return */
                memcpy(&p_sys->pes_buffer[p_sys->i_pes_buf_cnt],
                        p_block_in->p_buffer, l_rec_size);
                p_sys->i_pes_buf_cnt += l_rec_size;
                /* */
                block_Release(p_block_in);
                return 0;
            }

            /* we have enough; reconstruct this p_frame with the new hdr */
            memcpy(&p_sys->pes_buffer[p_sys->i_pes_buf_cnt],
                   p_block_in->p_buffer, i_need);
            /* advance the block past the PES header (don't want to send it) */
            p_block_in->p_buffer += i_need;
            p_block_in->i_buffer -= i_need;
            /* get the PTS out of this PES header (MPEG or AC3) */
            if (p_sys->audio_type == TIVO_AUDIO_MPEG)
                esOffset1 = find_es_header(ty_MPEGAudioPacket,
                        p_sys->pes_buffer, 5);
            else
                esOffset1 = find_es_header(ty_AC3AudioPacket,
                        p_sys->pes_buffer, 5);
            if (esOffset1 < 0)
            {
                /* god help us; something's really wrong */
                msg_Err(p_demux, "can't find audio PES header in packet");
            }
            else
            {
                p_sys->lastAudioPTS = VLC_TICK_0 + get_pts(
                    &p_sys->pes_buffer[ esOffset1 + p_sys->i_Pts_Offset ] );
                p_block_in->i_pts = p_sys->lastAudioPTS;
            }
            p_sys->i_pes_buf_cnt = 0;
        }
        /* S2 DTivo has AC3 packets with 2 padding bytes at end.  This is
         * not allowed in the AC3 spec and will cause problems.  So here
         * we try to trim things. */
        /* Also, S1 DTivo has alternating short / long AC3 packets.  That
         * is, one packet is short (incomplete) and the next packet has
         * the first one's missing data, plus all of its own.  Strange. */
        if (p_sys->audio_type == TIVO_AUDIO_AC3 &&
                p_sys->tivo_series == TIVO_SERIES2) {
            if (p_sys->l_ac3_pkt_size + p_block_in->i_buffer >
                    AC3_PKT_LENGTH) {
                p_block_in->i_buffer -= 2;
                p_sys->l_ac3_pkt_size = 0;
            } else {
                p_sys->l_ac3_pkt_size += p_block_in->i_buffer;
            }
        }
    }
    else if( subrec_type == 0x03 )
    {
        /* MPEG Audio with PES Header, either SA or DTiVo   */
        /* ================================================ */
        esOffset1 = find_es_header( ty_MPEGAudioPacket,
                p_block_in->p_buffer, 5 );

        /*msg_Dbg(p_demux, "buffer has %#02x %#02x %#02x %#02x",
           p_block_in->p_buffer[0], p_block_in->p_buffer[1],
           p_block_in->p_buffer[2], p_block_in->p_buffer[3]);
        msg_Dbg(p_demux, "audio ES hdr at offset %d", esOffset1);*/

        /* SA PES Header, No Audio Data                     */
        /* ================================================ */
        if ( ( esOffset1 == 0 ) && ( l_rec_size == 16 ) )
        {
            p_sys->lastAudioPTS = VLC_TICK_0 + get_pts( &p_block_in->p_buffer[
                        SA_PTS_OFFSET ] );

            block_Release(p_block_in);
            return 0;
            /*msg_Dbg(p_demux, "SA Audio PTS %"PRId64, p_sys->lastAudioPTS );*/
        }
        /* DTiVo Audio with PES Header                      */
        /* ================================================ */

        /* Check for complete PES */
        if (check_sync_pes(p_demux, p_block_in, esOffset1,
                            l_rec_size) == -1)
        {
            /* partial PES header found, nothing else.
             * we're done. */
            block_Release(p_block_in);
            return 0;
        }
#if 0
        msg_Dbg(p_demux, "packet buffer has "
                 "%02x %02x %02x %02x %02x %02x %02x %02x "
                 "%02x %02x %02x %02x %02x %02x %02x %02x",
                 p_block_in->p_buffer[0], p_block_in->p_buffer[1],
                 p_block_in->p_buffer[2], p_block_in->p_buffer[3],
                 p_block_in->p_buffer[4], p_block_in->p_buffer[5],
                 p_block_in->p_buffer[6], p_block_in->p_buffer[7],
                 p_block_in->p_buffer[8], p_block_in->p_buffer[9],
                 p_block_in->p_buffer[10], p_block_in->p_buffer[11],
                 p_block_in->p_buffer[12], p_block_in->p_buffer[13],
                 p_block_in->p_buffer[14], p_block_in->p_buffer[15]);
#endif
    }
    else if( subrec_type == 0x04 )
    {
        /* SA Audio with no PES Header                      */
        /* ================================================ */
        /*msg_Dbg(p_demux,
                "Adding SA Audio Packet Size %ld", l_rec_size ); */

        if (p_sys->lastAudioPTS != VLC_TICK_INVALID )
            p_block_in->i_pts = p_sys->lastAudioPTS;
    }
    else if( subrec_type == 0x09 )
    {
        /* DTiVo AC3 Audio Data with PES Header             */
        /* ================================================ */
        esOffset1 = find_es_header( ty_AC3AudioPacket,
                p_block_in->p_buffer, 5 );

#if 0
        msg_Dbg(p_demux, "buffer has "
                 "%02x %02x %02x %02x %02x %02x %02x %02x "
                 "%02x %02x %02x %02x %02x %02x %02x %02x",
                 p_block_in->p_buffer[0], p_block_in->p_buffer[1],
                 p_block_in->p_buffer[2], p_block_in->p_buffer[3],
                 p_block_in->p_buffer[4], p_block_in->p_buffer[5],
                 p_block_in->p_buffer[6], p_block_in->p_buffer[7],
                 p_block_in->p_buffer[8], p_block_in->p_buffer[9],
                 p_block_in->p_buffer[10], p_block_in->p_buffer[11],
                 p_block_in->p_buffer[12], p_block_in->p_buffer[13],
                 p_block_in->p_buffer[14], p_block_in->p_buffer[15]);
        msg_Dbg(p_demux, "audio ES AC3 hdr at offset %d", esOffset1);
#endif

        /* Check for complete PES */
        if (check_sync_pes(p_demux, p_block_in, esOffset1,
                            l_rec_size) == -1)
        {
            /* partial PES header found, nothing else.  we're done. */
            block_Release(p_block_in);
            return 0;
        }
        /* S2 DTivo has invalid long AC3 packets */
        if (p_sys->tivo_series == TIVO_SERIES2) {
            if (p_block_in->i_buffer > AC3_PKT_LENGTH) {
                p_block_in->i_buffer -= 2;
                p_sys->l_ac3_pkt_size = 0;
            } else {
                p_sys->l_ac3_pkt_size = p_block_in->i_buffer;
            }
        }
    }
    else
    {
        /* Unsupported/Unknown */
        block_Release(p_block_in);
        return 0;
    }

    /* set PCR before we send (if PTS found) */
    if( p_block_in->i_pts != VLC_TICK_INVALID )
        es_out_Control( p_demux->out, ES_OUT_SET_PCR,
                        p_block_in->i_pts );
    /* Send data */
    es_out_Send( p_demux->out, p_sys->p_audio, p_block_in );
    return 0;
}

static int DemuxRecCc( demux_t *p_demux, ty_rec_hdr_t *rec_hdr, block_t *p_block_in )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_field;

    if( p_block_in )
        block_Release(p_block_in);

    if( rec_hdr->rec_type == 0x01 )
        i_field = 0;
    else if( rec_hdr->rec_type == 0x02 )
        i_field = 1;
    else
        return 0;

    /* XDS data (extract programs infos) transmitted on field 2 only */
    if( i_field == 1 )
        DemuxDecodeXds( p_demux, rec_hdr->ex[0], rec_hdr->ex[1] );

    if( p_sys->cc.i_data + 3 > CC_MAX_DATA_SIZE )
        return 0;

    cc_AppendData( &p_sys->cc, CC_PKT_BYTE0(i_field), rec_hdr->ex );
    return 0;
}

/* seek to a position within the stream, if possible */
static int ty_stream_seek_pct(demux_t *p_demux, double seek_pct)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int64_t seek_pos = p_sys->i_stream_size * seek_pct;
    uint64_t l_skip_amt;
    unsigned i_cur_part;

    /* if we're not seekable, there's nothing to do */
    if (!p_sys->b_seekable)
        return VLC_EGENERIC;

    /* figure out which part & chunk we want & go there */
    i_cur_part = seek_pos / TIVO_PART_LENGTH;
    p_sys->i_cur_chunk = seek_pos / CHUNK_SIZE;

    /* try to read the part header (master chunk) if it's there */
    if (vlc_stream_Seek( p_demux->s, i_cur_part * TIVO_PART_LENGTH ) ||
        parse_master(p_demux) != VLC_SUCCESS)
    {
        /* can't seek stream */
        return VLC_EGENERIC;
    }

    /* now for the actual chunk */
    if ( vlc_stream_Seek( p_demux->s, p_sys->i_cur_chunk * CHUNK_SIZE))
    {
        /* can't seek stream */
        return VLC_EGENERIC;
    }
    /* load the chunk */
    p_sys->i_stuff_cnt = 0;
    get_chunk_header(p_demux);

    /* seek within the chunk to get roughly to where we want */
    p_sys->i_cur_rec = (int)
      ((double) ((seek_pos % CHUNK_SIZE) / (double) (CHUNK_SIZE)) * p_sys->i_num_recs);
    msg_Dbg(p_demux, "Seeked to file pos %"PRId64, seek_pos);
    msg_Dbg(p_demux, " (chunk %d, record %d)",
             p_sys->i_cur_chunk - 1, p_sys->i_cur_rec);

    /* seek to the start of this record's data.
     * to do that, we have to skip past all prior records */
    l_skip_amt = 0;
    for ( int i=0; i<p_sys->i_cur_rec; i++)
        l_skip_amt += p_sys->rec_hdrs[i].l_rec_size;
    if( vlc_stream_Seek(p_demux->s, ((p_sys->i_cur_chunk-1) * CHUNK_SIZE) +
                        (p_sys->i_num_recs * 16) + l_skip_amt + 4) != VLC_SUCCESS )
        return VLC_EGENERIC;

    /* to hell with syncing any audio or video, just start reading records... :) */
    /*p_sys->lastAudioPTS = p_sys->lastVideoPTS = VLC_TICK_INVALID;*/
    return VLC_SUCCESS;
}

/* XDS decoder */
//#define TY_XDS_DEBUG
static void XdsInit( xds_t *h )
{
    h->b_xds = false;
    h->i_class = XDS_MAX_CLASS_COUNT;
    h->i_type = 0;
    h->b_future = false;
    for( int i = 0; i < XDS_MAX_CLASS_COUNT; i++ )
    {
        for( int j = 0; j < 128; j++ )
            h->pkt[i][j].b_started = false;
    }
    h->b_meta_changed = false;
    memset( &h->meta, 0, sizeof(h->meta) );
}
static void XdsExit( xds_t *h )
{
    /* */
    free( h->meta.psz_channel_name );
    free( h->meta.psz_channel_call_letter );
    free( h->meta.psz_channel_number );

    /* */
    free( h->meta.current.psz_name );
    free( h->meta.current.psz_rating );
    /* */
    free( h->meta.future.psz_name );
    free( h->meta.future.psz_rating );
}
static void XdsStringUtf8( char dst[2*32+1], const uint8_t *p_src, size_t i_src )
{
    size_t i_dst = 0;
    for( size_t i = 0; i < i_src; i++ )
    {
        switch( p_src[i] )
        {
#define E2( c, u1, u2 ) case c: dst[i_dst++] = u1; dst[i_dst++] = u2; break
        E2( 0x2a, 0xc3,0xa1); // lowercase a, acute accent
        E2( 0x5c, 0xc3,0xa9); // lowercase e, acute accent
        E2( 0x5e, 0xc3,0xad); // lowercase i, acute accent
        E2( 0x5f, 0xc3,0xb3); // lowercase o, acute accent
        E2( 0x60, 0xc3,0xba); // lowercase u, acute accent
        E2( 0x7b, 0xc3,0xa7); // lowercase c with cedilla
        E2( 0x7c, 0xc3,0xb7); // division symbol
        E2( 0x7d, 0xc3,0x91); // uppercase N tilde
        E2( 0x7e, 0xc3,0xb1); // lowercase n tilde
#undef E2
        default:
            dst[i_dst++] = p_src[i];
            break;
        }
    }
    dst[i_dst++] = '\0';
}
static bool XdsChangeString( xds_t *h, char **ppsz_dst, const char *psz_new )
{
    if( *ppsz_dst && psz_new && !strcmp( *ppsz_dst, psz_new ) )
        return false;
    if( *ppsz_dst == NULL && psz_new == NULL )
        return false;

    free( *ppsz_dst );
    if( psz_new )
        *ppsz_dst = strdup( psz_new );
    else
        *ppsz_dst = NULL;

    h->b_meta_changed = true;
    return true;
}

static void XdsDecodeCurrentFuture( xds_t *h, xds_packet_t *pk )
{
    xds_meta_program_t *p_prg = h->b_future ? &h->meta.future : &h->meta.current;
    char name[2*32+1];
    int i_rating;

    switch( h->i_type )
    {
    case 0x03:
        XdsStringUtf8( name, pk->p_data, pk->i_data );
        if( XdsChangeString( h, &p_prg->psz_name, name ) )
        {
            //fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: class 'Current/Future (Program Name) %d'\n", pk->i_data );
            //fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: ====> program name %s\n", name );
        }
        break;
    case 0x05:
        i_rating = (pk->p_data[0] & 0x18);
        if( i_rating == 0x08 )
        {
            /* TPG */
            static const char *pppsz_ratings[8][2] = {
                { "None",   "No rating (no content advisory)" },
                { "TV-Y",   "All Children (no content advisory)" },
                { "TV-Y7",  "Directed to Older Children (V = Fantasy Violence)" },
                { "TV-G",   "General Audience (no content advisory)" },
                { "TV-PG",  "Parental Guidance Suggested" },
                { "TV-14",  "Parents Strongly Cautioned" },
                { "TV-MA",  "Mature Audience Only" },
                { "None",   "No rating (no content advisory)" }
            };
            p_prg->rating = XDS_META_PROGRAM_RATING_TPG;
            if( XdsChangeString( h, &p_prg->psz_rating, pppsz_ratings[pk->p_data[1]&0x07][0] ) )
            {
                //fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: class 'Current/Future (Rating) %d'\n", pk->i_data );
                //fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: ====> TPG Rating %s (%s)\n",
                //         pppsz_ratings[pk->p_data[1]&0x07][0], pppsz_ratings[pk->p_data[1]&0x07][1] );
            }
        }
        else if( i_rating == 0x00 || i_rating == 0x10 )
        {
            /* MPAA */
            static const char *pppsz_ratings[8][2] = {
                { "N/A",    "N/A" },
                { "G",      "General Audiences" },
                { "PG",     "Parental Guidance Suggested" },
                { "PG-13",  "Parents Strongly Cautioned" },
                { "R",      "Restricted" },
                { "NC-17",  "No one 17 and under admitted" },
                { "X",      "No one under 17 admitted" },
                { "NR",     "Not Rated" },
            };
            p_prg->rating = XDS_META_PROGRAM_RATING_MPAA;
            if( XdsChangeString( h, &p_prg->psz_rating, pppsz_ratings[pk->p_data[0]&0x07][0] ) )
            {
                //fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: class 'Current/Future (Rating) %d'\n", pk->i_data );
                //fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: ====> TPG Rating %s (%s)\n",
                //         pppsz_ratings[pk->p_data[0]&0x07][0], pppsz_ratings[pk->p_data[0]&0x07][1] );
            }
        }
        else
        {
            /* Non US Rating TODO */
            assert( i_rating == 0x18 ); // only left value possible */
            p_prg->rating = XDS_META_PROGRAM_RATING_NONE;
            if( XdsChangeString( h, &p_prg->psz_rating, NULL ) )
            {
                //fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: class 'Current/Future (Rating) %d'\n", pk->i_data );
                //fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: ====> 0x%2.2x 0x%2.2x\n", pk->p_data[0], pk->p_data[1] );
            }
        }
        break;

    default:
#ifdef TY_XDS_DEBUG
        fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: class 'Current/Future (Unknown 0x%x)'\n", h->i_type );
#endif
        break;
    }
}

static void XdsDecodeChannel( xds_t *h, xds_packet_t *pk )
{
    char name[2*32+1];
    char chan[2*32+1];

    switch( h->i_type )
    {
    case 0x01:
        if( pk->i_data < 2 )
            return;
        XdsStringUtf8( name, pk->p_data, pk->i_data );
        if( XdsChangeString( h, &h->meta.psz_channel_name, name ) )
        {
            //fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: class 'Channel (Network Name) %d'\n", pk->i_data );
            //fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: ====> %s\n", name );
        }
        break;

    case 0x02:
        if( pk->i_data < 4 )
            return;

        XdsStringUtf8( name, pk->p_data, 4 );
        if( XdsChangeString( h, &h->meta.psz_channel_call_letter, name ) )
        {
            //fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: class 'Channel (Network Call Letter)' %d\n", pk->i_data );
            //fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: ====> call letter %s\n", name );
        }
        if( pk->i_data >= 6 )
        {
            XdsStringUtf8( chan, &pk->p_data[4], 2 );
            if( XdsChangeString( h, &h->meta.psz_channel_number, chan ) )
            {
                //fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: class 'Channel (Network Call Letter)' %d\n", pk->i_data );
                //fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: ====> channel number %s\n", chan );
            }
        }
        else
        {
            if( XdsChangeString( h, &h->meta.psz_channel_number, NULL ) )
            {
                //fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: class 'Channel (Network Call Letter)' %d\n", pk->i_data );
                //fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: ====> no channel number letter anymore\n" );
            }
        }
        break;
    case 0x03:
        //fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: class 'Channel (Channel Tape Delay)'\n" );
        break;
    case 0x04:
        //fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: class 'Channel (Transmission Signal Identifier)'\n" );
        break;
    default:
#ifdef TY_XDS_DEBUG
        fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: class 'Channel (Unknown 0x%x)'\n", h->i_type );
#endif
        break;
    }
}

static void XdsDecode( xds_t *h, xds_packet_t *pk )
{
    switch( h->i_class )
    {
    case XDS_CLASS_CURRENT:
    case XDS_CLASS_FUTURE:
        XdsDecodeCurrentFuture( h, pk );
        break;
    case XDS_CLASS_CHANNEL:
        XdsDecodeChannel( h, pk );
        break;
    case XDS_CLASS_MISCELLANEOUS:
#ifdef TY_XDS_DEBUG
        fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: class 'Miscellaneous'\n" );
#endif
        break;
    case XDS_CLASS_PUBLIC_SERVICE:
#ifdef TY_XDS_DEBUG
        fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: class 'Public Service'\n" );
#endif
        break;
    default:
        //fprintf( stderr, "xxxxxxxxxxxxxxxXDS XdsDecode: unknown class\n" );
        break;
    }
}

static void XdsParse( xds_t *h, uint8_t d1, uint8_t d2 )
{
    /* TODO check parity */
    d1 &= 0x7f;
    d2 &= 0x7f;

    /* */
    if( d1 >= 0x01 && d1 <= 0x0e )
    {
        const xds_class_t i_class = ( d1 - 1 ) >> 1;
        const int i_type = d2;
        const bool b_start = d1 & 0x01;
        xds_packet_t *pk = &h->pkt[i_class][i_type];

        if( !b_start && !pk->b_started )
        {
            //fprintf( stderr, "xxxxxxxxxxxxxxxXDS Continuying a non started packet, ignoring\n" );
            h->b_xds = false;
            return;
        }

        h->b_xds = true;
        h->i_class = i_class;
        h->i_type  = i_type;
        h->b_future = !b_start;
        pk->b_started = true;
        if( b_start )
        {
            pk->i_data = 0;
            pk->i_sum = d1 + d2;
        }
    }
    else if( d1 == 0x0f && h->b_xds )
    {
        xds_packet_t *pk = &h->pkt[h->i_class][h->i_type];

        /* TODO checksum and decode */
        pk->i_sum += d1 + d2;
        if( pk->i_sum & 0x7f )
        {
            //fprintf( stderr, "xxxxxxxxxxxxxxxXDS invalid checksum, ignoring ---------------------------------\n" );
            pk->b_started = false;
            return;
        }
        if( pk->i_data <= 0 )
        {
            //fprintf( stderr, "xxxxxxxxxxxxxxxXDS empty packet, ignoring ---------------------------------\n" );
            pk->b_started = false;
            return;
        }

        //if( pk->p_data[pk->i_data-1] == 0x40 ) /* Padding byte */
        //    pk->i_data--;
        XdsDecode( h, pk );

        /* Reset it */
        pk->b_started = false;
    }
    else if( d1 >= 0x20 && h->b_xds )
    {
        xds_packet_t *pk = &h->pkt[h->i_class][h->i_type];

        if( pk->i_data+2 > XDS_MAX_DATA_SIZE )
        {
            /* Broken -> reinit */
            //fprintf( stderr, "xxxxxxxxxxxxxxxXDS broken, reset\n" );
            h->b_xds = false;
            pk->b_started = false;
            return;
        }
        /* TODO check parity bit */
        pk->p_data[pk->i_data++] = d1 & 0x7f;
        pk->p_data[pk->i_data++] = d2 & 0x7f;
        pk->i_sum += d1+d2;
    }
    else
    {
        h->b_xds = false;
    }
}

static void DemuxDecodeXds( demux_t *p_demux, uint8_t d1, uint8_t d2 )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    XdsParse( &p_sys->xds, d1, d2 );
    if( p_sys->xds.b_meta_changed )
    {
        xds_meta_t *m = &p_sys->xds.meta;
        vlc_meta_t *p_meta;

        /* Channel meta data */
        p_meta = vlc_meta_New();
        if( m->psz_channel_name )
            vlc_meta_SetPublisher( p_meta, m->psz_channel_name );
        if( m->psz_channel_call_letter )
            vlc_meta_SetTitle( p_meta, m->psz_channel_call_letter );
        if( m->psz_channel_number )
            vlc_meta_AddExtra( p_meta, "Channel number", m->psz_channel_number );
        es_out_Control( p_demux->out, ES_OUT_SET_GROUP_META, TY_ES_GROUP, p_meta );
        vlc_meta_Delete( p_meta );

        /* Event meta data (current/future) */
        if( m->current.psz_name )
        {
            vlc_epg_t *p_epg = vlc_epg_New( TY_ES_GROUP, TY_ES_GROUP );
            if ( p_epg )
            {
                vlc_epg_event_t *p_evt = vlc_epg_event_New( 0, 0, 0 );
                if ( p_evt )
                {
                    if( m->current.psz_name )
                        p_evt->psz_name = strdup( m->current.psz_name );
                    if( !vlc_epg_AddEvent( p_epg, p_evt ) )
                        vlc_epg_event_Delete( p_evt );
                }
                //if( m->current.psz_rating )
                //  TODO but VLC cannot yet handle rating per epg event
                vlc_epg_SetCurrent( p_epg, 0 );

                if( m->future.psz_name )
                {
                }
                if( p_epg->i_event > 0 )
                    es_out_Control( p_demux->out, ES_OUT_SET_GROUP_EPG,
                                    TY_ES_GROUP, p_epg );
                vlc_epg_Delete( p_epg );
            }
        }
    }
    p_sys->xds.b_meta_changed = false;
}

/* seek to an exact time position within the stream, if possible.
 * l_seek_time is in nanoseconds, the TIVO time standard.
 */
static int ty_stream_seek_time(demux_t *p_demux, uint64_t l_seek_time)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned i_seq_entry = 0;
    unsigned i;
    int i_skip_cnt;
    int64_t l_cur_pos = vlc_stream_Tell(p_demux->s);
    unsigned i_cur_part = l_cur_pos / TIVO_PART_LENGTH;
    uint64_t l_seek_secs = l_seek_time / 1000000000;
    uint64_t l_fwd_stamp = 1;

    /* if we're not seekable, there's nothing to do */
    if (!p_sys->b_seekable || !p_sys->b_have_master)
        return VLC_EGENERIC;

    msg_Dbg(p_demux, "Skipping to time %02"PRIu64":%02"PRIu64":%02"PRIu64,
            l_seek_secs / 3600, (l_seek_secs / 60) % 60, l_seek_secs % 60);

    /* seek to the proper segment if necessary */
    /* first see if we need to go back */
    while (l_seek_time < p_sys->l_first_ty_pts) {
        msg_Dbg(p_demux, "skipping to prior segment.");
        /* load previous part */
        if (i_cur_part == 0) {
            p_sys->eof = (vlc_stream_Seek(p_demux->s, l_cur_pos) != VLC_SUCCESS);
            msg_Err(p_demux, "Attempt to seek past BOF");
            return VLC_EGENERIC;
        }
        if(vlc_stream_Seek(p_demux->s, (i_cur_part - 1) * TIVO_PART_LENGTH) != VLC_SUCCESS)
            return VLC_EGENERIC;
        i_cur_part--;
        if(parse_master(p_demux) != VLC_SUCCESS)
            return VLC_EGENERIC;
    }
    /* maybe we need to go forward */
    while (l_seek_time > p_sys->l_final_ty_pts) {
        msg_Dbg(p_demux, "skipping to next segment.");
        /* load next part */
        if ((i_cur_part + 1) * TIVO_PART_LENGTH > p_sys->i_stream_size) {
            /* error; restore previous file position */
            p_sys->eof = (vlc_stream_Seek(p_demux->s, l_cur_pos) != VLC_SUCCESS);
            msg_Err(p_demux, "seek error");
            return VLC_EGENERIC;
        }
        if(vlc_stream_Seek(p_demux->s, (i_cur_part + 1) * TIVO_PART_LENGTH) != VLC_SUCCESS)
            return VLC_EGENERIC;
        i_cur_part++;
        if(parse_master(p_demux) != VLC_SUCCESS)
            return VLC_EGENERIC;
    }

    /* our target is somewhere within this part;
       find the proper chunk using seq_table */
    for (i=1; i<p_sys->i_seq_table_size; i++) {
        if (p_sys->seq_table[i].l_timestamp > l_seek_time) {
            /* i-1 is the section we want; remember the next timestamp in case
               we have to use it (this section may not have a proper SEQ hdr
               for the time we're seeking) */
            msg_Dbg(p_demux, "stopping at seq entry %d.", i);
            l_fwd_stamp = p_sys->seq_table[i].l_timestamp;
            i_seq_entry = i-1;
            break;
        }
    }

    /* if we went through the entire last loop and didn't find our target,
       then we skip to the next part.  What has happened is that the actual
       time we're seeking is within this part, but there isn't a SEQ hdr
       for it here.  So we skip to the next part */
    if (i == p_sys->i_seq_table_size) {
        if ((i_cur_part + 1) * TIVO_PART_LENGTH > p_sys->i_stream_size) {
            /* error; restore previous file position */
            p_sys->eof = (vlc_stream_Seek(p_demux->s, l_cur_pos) != VLC_SUCCESS);
            msg_Err(p_demux, "seek error");
            return VLC_EGENERIC;
        }
        if(vlc_stream_Seek(p_demux->s, (i_cur_part + 1) * TIVO_PART_LENGTH) != VLC_SUCCESS)
            return VLC_EGENERIC;
        i_cur_part++;
        if(parse_master(p_demux) != VLC_SUCCESS)
            return VLC_EGENERIC;
        i_seq_entry = 0;
    }

    /* determine which chunk has our seek_time */
    for (i=0; i<p_sys->i_bits_per_seq_entry; i++) {
        uint64_t l_chunk_nr = i_seq_entry * p_sys->i_bits_per_seq_entry + i;
        uint64_t l_chunk_offset = (l_chunk_nr + 1) * CHUNK_SIZE;
        msg_Dbg(p_demux, "testing part %d chunk %"PRIu64" mask 0x%02X bit %d",
            i_cur_part, l_chunk_nr,
            p_sys->seq_table[i_seq_entry].chunk_bitmask[i/8], i%8);
        if (p_sys->seq_table[i_seq_entry].chunk_bitmask[i/8] & (1 << (i%8))) {
            /* check this chunk's SEQ header timestamp */
            msg_Dbg(p_demux, "has SEQ. seeking to chunk at 0x%"PRIu64,
                (i_cur_part * TIVO_PART_LENGTH) + l_chunk_offset);
            if(vlc_stream_Seek(p_demux->s, (i_cur_part * TIVO_PART_LENGTH) +
                l_chunk_offset) != VLC_SUCCESS)
                return VLC_EGENERIC;
            // TODO: we don't have to parse the full header set;
            // just test the seq_rec entry for its timestamp
            p_sys->i_stuff_cnt = 0;
            get_chunk_header(p_demux);
            // check ty PTS for the SEQ entry in this chunk
            if (p_sys->i_seq_rec < 0 || p_sys->i_seq_rec > p_sys->i_num_recs) {
                msg_Err(p_demux, "no SEQ hdr in chunk; table had one.");
                /* Seek to beginning of original chunk & reload it */
                if(vlc_stream_Seek(p_demux->s, (l_cur_pos / CHUNK_SIZE) * CHUNK_SIZE) != VLC_SUCCESS)
                    p_sys->eof = true;
                p_sys->i_stuff_cnt = 0;
                get_chunk_header(p_demux);
                return VLC_EGENERIC;
            }
            l_seek_secs = p_sys->rec_hdrs[p_sys->i_seq_rec].l_ty_pts /
                1000000000;
            msg_Dbg(p_demux, "found SEQ hdr for timestamp %02"PRIu64":%02"PRIu64":%02"PRIu64,
                l_seek_secs / 3600,
                (l_seek_secs / 60) % 60, l_seek_secs % 60);
            if (p_sys->rec_hdrs[p_sys->i_seq_rec].l_ty_pts >= l_seek_time) {
                // keep this one?  go back?
                /* for now, we take this one.  it's the first SEQ hdr AFTER
                   the time we were searching for. */
                msg_Dbg(p_demux, "seek target found.");
                break;
            }
            msg_Dbg(p_demux, "timestamp too early. still scanning.");
        }
    }
    /* if we made it through this entire loop without finding our target,
       then we skip to the next section.  What has happened is that the actual
       time we're seeking is within this section, but there isn't a SEQ hdr
       for it here.  So we skip to the next closest one (l_fwd_stamp) */
    if (i == p_sys->i_bits_per_seq_entry)
        return ty_stream_seek_time(p_demux, l_fwd_stamp);

    /* current stream ptr is at beginning of data for this chunk,
       so we need to skip past any stream data prior to the seq_rec
       in this chunk */
    i_skip_cnt = 0;
    for (int j=0; j<p_sys->i_seq_rec; j++)
        i_skip_cnt += p_sys->rec_hdrs[j].l_rec_size;
    if(vlc_stream_Read(p_demux->s, NULL, i_skip_cnt) != i_skip_cnt)
        return VLC_EGENERIC;
    p_sys->i_cur_rec = p_sys->i_seq_rec;
    //p_sys->l_last_ty_pts = p_sys->rec_hdrs[p_sys->i_seq_rec].l_ty_pts;
    //p_sys->l_last_ty_pts_sync = p_sys->lastAudioPTS;

    return VLC_SUCCESS;
}


/* parse a master chunk, filling the SEQ table and other variables.
 * We assume the stream is currently pointing to it.
 */
static int parse_master(demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    uint8_t mst_buf[32];
    int64_t i_save_pos = vlc_stream_Tell(p_demux->s);
    int64_t i_pts_secs;

    /* Note that the entries in the SEQ table in the stream may have
       different sizes depending on the bits per entry.  We store them
       all in the same size structure, so we have to parse them out one
       by one.  If we had a dynamic structure, we could simply read the
       entire table directly from the stream into memory in place. */

    /* clear the SEQ table */
    free(p_sys->seq_table);

    /* parse header info */
    if( vlc_stream_Read(p_demux->s, mst_buf, 32) != 32 )
        return VLC_EGENERIC;

    uint32_t i_map_size = U32_AT(&mst_buf[20]);  /* size of bitmask, in bytes */
    uint32_t i = U32_AT(&mst_buf[28]);   /* size of SEQ table, in bytes */

    p_sys->i_bits_per_seq_entry = i_map_size * 8;
    p_sys->i_seq_table_size = i / (8 + i_map_size);

    if(p_sys->i_seq_table_size == 0)
    {
        p_sys->seq_table = NULL;
        return VLC_SUCCESS;
    }

#if (UINT32_MAX > SSIZE_MAX)
    if (i_map_size > SSIZE_MAX)
        return VLC_EGENERIC;
#endif

    /* parse all the entries */
    p_sys->seq_table = calloc(p_sys->i_seq_table_size, sizeof(ty_seq_table_t));
    if (p_sys->seq_table == NULL)
    {
        p_sys->i_seq_table_size = 0;
        return VLC_SUCCESS;
    }
    for (unsigned j=0; j<p_sys->i_seq_table_size; j++) {
        if(vlc_stream_Read(p_demux->s, mst_buf, 8) != 8)
            return VLC_EGENERIC;
        p_sys->seq_table[j].l_timestamp = U64_AT(&mst_buf[0]);
        if (i_map_size > 8) {
            msg_Err(p_demux, "Unsupported SEQ bitmap size in master chunk");
            if (vlc_stream_Read(p_demux->s, NULL, i_map_size)
                                       < (ssize_t)i_map_size)
                return VLC_EGENERIC;
        } else {
            if (vlc_stream_Read(p_demux->s, mst_buf + 8, i_map_size)
                                              < (ssize_t)i_map_size)
                return VLC_EGENERIC;
            memcpy(p_sys->seq_table[j].chunk_bitmask, &mst_buf[8], i_map_size);
        }
    }

    /* set up a few of our variables */
    p_sys->l_first_ty_pts = p_sys->seq_table[0].l_timestamp;
    p_sys->l_final_ty_pts =
        p_sys->seq_table[p_sys->i_seq_table_size - 1].l_timestamp;
    p_sys->b_have_master = true;

    i_pts_secs = p_sys->l_first_ty_pts / 1000000000;
    msg_Dbg( p_demux,
             "first TY pts in master is %02"PRId64":%02"PRId64":%02"PRId64,
             i_pts_secs / 3600, (i_pts_secs / 60) % 60, i_pts_secs % 60 );
    i_pts_secs = p_sys->l_final_ty_pts / 1000000000;
    msg_Dbg( p_demux,
             "final TY pts in master is %02"PRId64":%02"PRId64":%02"PRId64,
             i_pts_secs / 3600, (i_pts_secs / 60) % 60, i_pts_secs % 60 );

    /* seek past this chunk */
    return vlc_stream_Seek(p_demux->s, i_save_pos + CHUNK_SIZE);
}


/* ======================================================================== */
/* "Peek" at some chunks.  Skip over the Part header if we find it.
 * We parse the peeked data and determine audio type,
 * SA vs. DTivo, & Tivo Series.
 * Set global vars i_Pes_Length, i_Pts_Offset,
 * p_sys->tivo_series, p_sys->tivo_type, p_sys->audio_type */
static int probe_stream(demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const uint8_t *p_buf;
    int i;
    bool b_probe_error = false;

    /* we need CHUNK_PEEK_COUNT chunks of data, first one might be a Part header, so ... */
    if (vlc_stream_Peek( p_demux->s, &p_buf, CHUNK_PEEK_COUNT * CHUNK_SIZE ) <
            CHUNK_PEEK_COUNT * CHUNK_SIZE) {
        msg_Err(p_demux, "Can't peek %d chunks", CHUNK_PEEK_COUNT);
        /* TODO: if seekable, then loop reading chunks into a temp buffer */
        return VLC_EGENERIC;
    }

    /* the real work: analyze this chunk */
    for (i = 0; i < CHUNK_PEEK_COUNT; i++) {
        analyze_chunk(p_demux, p_buf);
        if (p_sys->tivo_series != TIVO_SERIES_UNKNOWN &&
            p_sys->audio_type  != TIVO_AUDIO_UNKNOWN &&
            p_sys->tivo_type   != TIVO_TYPE_UNKNOWN)
            break;
        p_buf += CHUNK_SIZE;
    }

    /* the final tally */
    if (p_sys->tivo_series == TIVO_SERIES_UNKNOWN) {
        msg_Err(p_demux, "Can't determine Tivo Series.");
        b_probe_error = true;
    }
    if (p_sys->audio_type == TIVO_AUDIO_UNKNOWN) {
        msg_Err(p_demux, "Can't determine Tivo Audio Type.");
        b_probe_error = true;
    }
    if (p_sys->tivo_type == TIVO_TYPE_UNKNOWN) {
        msg_Err(p_demux, "Can't determine Tivo Type (SA/DTivo).");
        b_probe_error = true;
    }
    return b_probe_error?VLC_EGENERIC:VLC_SUCCESS;
}


/* ======================================================================== */
/* gather statistics for this chunk & set our tivo-type vars accordingly */
static void analyze_chunk(demux_t *p_demux, const uint8_t *p_chunk)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_num_recs, i;
    ty_rec_hdr_t *p_hdrs;
    int i_num_6e0, i_num_be0, i_num_9c0, i_num_3c0;
    int i_payload_size;

    /* skip if it's a Part header */
    if( U32_AT( &p_chunk[ 0 ] ) == TIVO_PES_FILEID )
        return;

    /* number of records in chunk (we ignore high order byte;
     * rarely are there > 256 chunks & we don't need that many anyway) */
    i_num_recs = p_chunk[0];
    if (i_num_recs < 5) {
        /* try again with the next chunk.  Sometimes there are dead ones */
        return;
    }

    p_chunk += 4;       /* skip past rec count & SEQ bytes */
    //msg_Dbg(p_demux, "probe: chunk has %d recs", i_num_recs);
    p_hdrs = parse_chunk_headers(p_chunk, i_num_recs, &i_payload_size);
    /* scan headers.
     * 1. check video packets.  Presence of 0x6e0 means S1.
     *    No 6e0 but have be0 means S2.
     * 2. probe for audio 0x9c0 vs 0x3c0 (AC3 vs Mpeg)
     *    If AC-3, then we have DTivo.
     *    If MPEG, search for PTS offset.  This will determine SA vs. DTivo.
     */
    i_num_6e0 = i_num_be0 = i_num_9c0 = i_num_3c0 = 0;
    for (i=0; i<i_num_recs; i++) {
        //msg_Dbg(p_demux, "probe: rec is %d/%d = 0x%04x", p_hdrs[i].subrec_type,
            //p_hdrs[i].rec_type,
            //p_hdrs[i].subrec_type << 8 | p_hdrs[i].rec_type);
        switch (p_hdrs[i].subrec_type << 8 | p_hdrs[i].rec_type) {
            case 0x6e0:
                i_num_6e0++;
                break;
            case 0xbe0:
                i_num_be0++;
                break;
            case 0x3c0:
                i_num_3c0++;
                break;
            case 0x9c0:
                i_num_9c0++;
                break;
        }
    }
    msg_Dbg(p_demux, "probe: chunk has %d 0x6e0 recs, %d 0xbe0 recs.",
        i_num_6e0, i_num_be0);

    /* set up our variables */
    if (i_num_6e0 > 0) {
        msg_Dbg(p_demux, "detected Series 1 Tivo");
        p_sys->tivo_series = TIVO_SERIES1;
        p_sys->i_Pes_Length = SERIES1_PES_LENGTH;
    } else if (i_num_be0 > 0) {
        msg_Dbg(p_demux, "detected Series 2 Tivo");
        p_sys->tivo_series = TIVO_SERIES2;
        p_sys->i_Pes_Length = SERIES2_PES_LENGTH;
    }
    if (i_num_9c0 > 0) {
        msg_Dbg(p_demux, "detected AC-3 Audio (DTivo)" );
        p_sys->audio_type = TIVO_AUDIO_AC3;
        p_sys->tivo_type = TIVO_TYPE_DTIVO;
        p_sys->i_Pts_Offset = AC3_PTS_OFFSET;
        p_sys->i_Pes_Length = AC3_PES_LENGTH;
    } else if (i_num_3c0 > 0) {
        p_sys->audio_type = TIVO_AUDIO_MPEG;
        msg_Dbg(p_demux, "detected MPEG Audio" );
    }

    /* if tivo_type still unknown, we can check PTS location
     * in MPEG packets to determine tivo_type */
    if (p_sys->tivo_type == TIVO_TYPE_UNKNOWN) {
        uint32_t i_data_offset = (16 * i_num_recs);
        for (i=0; i<i_num_recs; i++) {
            if ((p_hdrs[i].subrec_type << 0x08 | p_hdrs[i].rec_type) == 0x3c0 &&
                    p_hdrs[i].l_rec_size > 15) {
                /* first make sure we're aligned */
                int i_pes_offset = find_es_header(ty_MPEGAudioPacket,
                        &p_chunk[i_data_offset], 5);
                if (i_pes_offset >= 0) {
                    /* pes found. on SA, PES has hdr data at offset 6, not PTS. */
                    //msg_Dbg(p_demux, "probe: mpeg es header found in rec %d at offset %d",
                            //i, i_pes_offset);
                    if ((p_chunk[i_data_offset + 6 + i_pes_offset] & 0x80) == 0x80) {
                        /* S1SA or S2(any) Mpeg Audio (PES hdr, not a PTS start) */
                        if (p_sys->tivo_series == TIVO_SERIES1)
                            msg_Dbg(p_demux, "detected Stand-Alone Tivo" );
                        p_sys->tivo_type = TIVO_TYPE_SA;
                        p_sys->i_Pts_Offset = SA_PTS_OFFSET;
                    } else {
                        if (p_sys->tivo_series == TIVO_SERIES1)
                            msg_Dbg(p_demux, "detected DirecTV Tivo" );
                        p_sys->tivo_type = TIVO_TYPE_DTIVO;
                        p_sys->i_Pts_Offset = DTIVO_PTS_OFFSET;
                    }
                    break;
                }
            }
            i_data_offset += p_hdrs[i].l_rec_size;
        }
    }
    free(p_hdrs);
}


/* =========================================================================== */
static int get_chunk_header(demux_t *p_demux)
{
    int i_readSize, i_num_recs;
    uint8_t *p_hdr_buf;
    const uint8_t *p_peek;
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_payload_size;             /* sum of all records' sizes */

    msg_Dbg(p_demux, "parsing ty chunk #%d", p_sys->i_cur_chunk );

    /* if we have left-over filler space from the last chunk, get that */
    if (p_sys->i_stuff_cnt > 0) {
        if(vlc_stream_Read(p_demux->s, NULL, p_sys->i_stuff_cnt) != p_sys->i_stuff_cnt)
            return 0;
        p_sys->i_stuff_cnt = 0;
    }

    /* read the TY packet header */
    i_readSize = vlc_stream_Peek( p_demux->s, &p_peek, 4 );
    p_sys->i_cur_chunk++;

    if ( (i_readSize < 4) || ( U32_AT(&p_peek[ 0 ] ) == 0 ))
    {
        /* EOF */
        p_sys->eof = 1;
        return 0;
    }

    /* check if it's a PART Header */
    if( U32_AT( &p_peek[ 0 ] ) == TIVO_PES_FILEID )
    {
        /* parse master chunk */
        if(parse_master(p_demux) != VLC_SUCCESS)
            return 0;
        return get_chunk_header(p_demux);
    }

    /* number of records in chunk (8- or 16-bit number) */
    if (p_peek[3] & 0x80)
    {
        /* 16 bit rec cnt */
        p_sys->i_num_recs = i_num_recs = (p_peek[1] << 8) + p_peek[0];
        p_sys->i_seq_rec = (p_peek[3] << 8) + p_peek[2];
        if (p_sys->i_seq_rec != 0xffff)
        {
            p_sys->i_seq_rec &= ~0x8000;
        }
    }
    else
    {
        /* 8 bit reclen - tivo 1.3 format */
        p_sys->i_num_recs = i_num_recs = p_peek[0];
        p_sys->i_seq_rec = p_peek[1];
    }
    p_sys->i_cur_rec = 0;
    p_sys->b_first_chunk = false;

    /*msg_Dbg( p_demux, "chunk has %d records", i_num_recs );*/

    free(p_sys->rec_hdrs);
    p_sys->rec_hdrs = NULL;

    /* skip past the 4 bytes we "peeked" earlier */
    if(vlc_stream_Read(p_demux->s, NULL, 4) != 4)
        return 0;

    /* read the record headers into a temp buffer */
    p_hdr_buf = xmalloc(i_num_recs * 16);
    if (vlc_stream_Read(p_demux->s, p_hdr_buf, i_num_recs * 16) < i_num_recs * 16) {
        free( p_hdr_buf );
        p_sys->eof = true;
        return 0;
    }
    /* parse them */
    p_sys->rec_hdrs = parse_chunk_headers(p_hdr_buf, i_num_recs,
            &i_payload_size);
    free(p_hdr_buf);

    p_sys->i_stuff_cnt = CHUNK_SIZE - 4 -
        (p_sys->i_num_recs * 16) - i_payload_size;
    if (p_sys->i_stuff_cnt > 0)
        msg_Dbg( p_demux, "chunk has %d stuff bytes at end",
                 p_sys->i_stuff_cnt );
    return 1;
}


static ty_rec_hdr_t *parse_chunk_headers( const uint8_t *p_buf,
                                          int i_num_recs, int *pi_payload_size)
{
    int i;
    ty_rec_hdr_t *p_hdrs, *p_rec_hdr;

    *pi_payload_size = 0;
    p_hdrs = xmalloc(i_num_recs * sizeof(ty_rec_hdr_t));

    for (i = 0; i < i_num_recs; i++)
    {
        const uint8_t *record_header = p_buf + (i * 16);
        p_rec_hdr = &p_hdrs[i];     /* for brevity */
        p_rec_hdr->rec_type = record_header[3];
        p_rec_hdr->subrec_type = record_header[2] & 0x0f;
        if ((record_header[ 0 ] & 0x80) == 0x80)
        {
            uint8_t b1, b2;
            /* marker bit 2 set, so read extended data */
            b1 = ( ( ( record_header[ 0 ] & 0x0f ) << 4 ) |
                   ( ( record_header[ 1 ] & 0xf0 ) >> 4 ) );
            b2 = ( ( ( record_header[ 1 ] & 0x0f ) << 4 ) |
                   ( ( record_header[ 2 ] & 0xf0 ) >> 4 ) );

            p_rec_hdr->ex[0] = b1;
            p_rec_hdr->ex[1] = b2;
            p_rec_hdr->l_rec_size = 0;
            p_rec_hdr->l_ty_pts = 0;
            p_rec_hdr->b_ext = true;
        }
        else
        {
            p_rec_hdr->l_rec_size = ( record_header[ 0 ] << 8 |
                record_header[ 1 ] ) << 4 | ( record_header[ 2 ] >> 4 );
            *pi_payload_size += p_rec_hdr->l_rec_size;
            p_rec_hdr->b_ext = false;
            p_rec_hdr->l_ty_pts = U64_AT( &record_header[ 8 ] );
        }
        //fprintf( stderr, "parse_chunk_headers[%d] t=0x%x s=%d\n", i, p_rec_hdr->rec_type, p_rec_hdr->subrec_type );
    } /* end of record-header loop */
    return p_hdrs;
}
