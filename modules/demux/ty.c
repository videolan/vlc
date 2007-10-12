/*****************************************************************************
 * ty.c - TiVo ty stream video demuxer for VLC
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * Copyright (C) 2005 by Neal Symms (tivo@freakinzoo.com) - February 2005
 * based on code by Christopher Wingert for tivo-mplayer
 * tivo(at)wingert.org, February 2003
 *
 * $Id$
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

#include <vlc/vlc.h>
#include <vlc_demux.h>
#include "vlc_codec.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_shortname( _("TY") );
    set_description(_("TY Stream audio/video demux"));
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_capability("demux2", 6);
    /* FIXME: there seems to be a segfault when using PVR access
     * and TY demux has a bigger priority than PS
     * Something must be wrong.
     */
    set_callbacks( Open, Close );
    add_shortcut("ty");
    add_shortcut("tivo");
vlc_module_end();

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
  uint8_t ex1, ex2;
  uint8_t rec_type;
  uint8_t subrec_type;
  vlc_bool_t b_ext;
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

struct demux_sys_t
{
  es_out_id_t *p_video;               /* ptr to video codec */
  es_out_id_t *p_audio;               /* holds either ac3 or mpeg codec ptr */

  int             i_cur_chunk;
  int             i_stuff_cnt;
  size_t          i_stream_size;      /* size of input stream (if known) */
  //uint64_t        l_program_len;      /* length of this stream in msec */
  vlc_bool_t      b_seekable;         /* is this stream seekable? */
  vlc_bool_t      b_have_master;      /* are master chunks present? */
  tivo_type_t     tivo_type;          /* tivo type (SA / DTiVo) */
  tivo_series_t   tivo_series;        /* Series1 or Series2 */
  tivo_audio_t    audio_type;         /* AC3 or MPEG */
  int             i_Pes_Length;       /* Length of Audio PES header */
  int             i_Pts_Offset;       /* offset into audio PES of PTS */
  uint8_t         pes_buffer[20];     /* holds incomplete pes headers */
  int             i_pes_buf_cnt;      /* how many bytes in our buffer */
  size_t          l_ac3_pkt_size;     /* len of ac3 pkt we've seen so far */
  uint64_t        l_last_ty_pts;      /* last TY timestamp we've seen */
  //mtime_t         l_last_ty_pts_sync; /* audio PTS at time of last TY PTS */
  uint64_t        l_first_ty_pts;     /* first TY PTS in this master chunk */
  uint64_t        l_final_ty_pts;     /* final TY PTS in this master chunk */
  int             i_seq_table_size;   /* number of entries in SEQ table */
  int             i_bits_per_seq_entry; /* # of bits in SEQ table bitmask */

  mtime_t         firstAudioPTS;
  mtime_t         lastAudioPTS;
  mtime_t         lastVideoPTS;

  ty_rec_hdr_t    *rec_hdrs;          /* record headers array */
  int             i_cur_rec;          /* current record in this chunk */
  int             i_num_recs;         /* number of recs in this chunk */
  int             i_seq_rec;          /* record number where seq start is */
  ty_seq_table_t  *seq_table;         /* table of SEQ entries from mstr chk */
  vlc_bool_t      eof;
  vlc_bool_t      b_first_chunk;
};

static int get_chunk_header(demux_t *);
static mtime_t get_pts( const uint8_t *buf );
static int find_es_header( const uint8_t *header,
                           const uint8_t *buffer, int i_search_len );
static int ty_stream_seek_pct(demux_t *p_demux, double seek_pct);
static int ty_stream_seek_time(demux_t *, uint64_t);

static ty_rec_hdr_t *parse_chunk_headers( demux_t *p_demux, const uint8_t *p_buf,
                                          int i_num_recs, int *pi_payload_size);
static int probe_stream(demux_t *p_demux);
static void analyze_chunk(demux_t *p_demux, const uint8_t *p_chunk);
static void parse_master(demux_t *p_demux);

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

    /* peek at the first 12 bytes. */
    /* for TY streams, they're always the same */
    if( stream_Peek( p_demux->s, &p_peek, 12 ) < 12 )
        return VLC_EGENERIC;

    if ( U32_AT(p_peek) != TIVO_PES_FILEID ||
         U32_AT(&p_peek[4]) != 0x02 ||
         U32_AT(&p_peek[8]) != CHUNK_SIZE )
    {
        if( !p_demux->b_force )
            return VLC_EGENERIC;
        msg_Warn( p_demux, "this does not look like a TY file, "
                           "continuing anyway..." );
    }

	/* at this point, we assume we have a valid TY stream */  
    msg_Dbg( p_demux, "valid TY stream detected" );

    /* Set exported functions */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    /* create our structure that will hold all data */
    p_demux->p_sys = p_sys = malloc(sizeof(demux_sys_t));
    memset(p_sys, 0, sizeof(demux_sys_t));

    /* set up our struct (most were zero'd out with the memset above) */
    p_sys->b_first_chunk = VLC_TRUE;
    p_sys->b_have_master = (U32_AT(p_peek) == TIVO_PES_FILEID);
    p_sys->firstAudioPTS = -1;
    p_sys->i_stream_size = stream_Size(p_demux->s);
    p_sys->tivo_type = TIVO_TYPE_UNKNOWN;
    p_sys->audio_type = TIVO_AUDIO_UNKNOWN;
    p_sys->tivo_series = TIVO_SERIES_UNKNOWN;
    p_sys->i_Pes_Length = 0;
    p_sys->i_Pts_Offset = 0;
    p_sys->l_ac3_pkt_size = 0;
  
    /* see if this stream is seekable */
    stream_Control( p_demux->s, STREAM_CAN_SEEK, &p_sys->b_seekable );

    if (probe_stream(p_demux) != VLC_SUCCESS) {
        //TyClose(p_demux);
        return VLC_EGENERIC;
    }

    if (!p_sys->b_have_master)
      msg_Warn(p_demux, "No master chunk found; seeking will be limited.");

    /* register the proper audio codec */
    if (p_sys->audio_type == TIVO_AUDIO_MPEG) {
        es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC( 'm', 'p', 'g', 'a' ) );
    } else {
        es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC( 'a', '5', '2', ' ' ) );
    }
    p_sys->p_audio = es_out_Add( p_demux->out, &fmt );

    /* register the video stream */
    es_format_Init( &fmt, VIDEO_ES, VLC_FOURCC( 'm', 'p', 'g', 'v' ) );
    p_sys->p_video = es_out_Add( p_demux->out, &fmt );

#if 0
    /* register the CC decoder */
    es_format_Init( &fmt, SPU_ES, VLC_FOURCC('s', 'u', 'b', 't'));
    p_sys->p_subt_es = es_out_Add(p_demux->out, &fmt);
#endif

    return VLC_SUCCESS;
}


/* =========================================================================== */
/* Compute Presentation Time Stamp (PTS)
 * Assume buf points to beginning of PTS */
static mtime_t get_pts( const uint8_t *buf )
{
    mtime_t i_pts;

    i_pts = ((mtime_t)(buf[0]&0x0e ) << 29)|
             (mtime_t)(buf[1] << 22)|
            ((mtime_t)(buf[2]&0xfe) << 14)|
             (mtime_t)(buf[3] << 7)|
             (mtime_t)(buf[4] >> 1);
    i_pts *= 100 / 9;   /* convert PTS (90Khz clock) to microseconds */
    return i_pts;
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
            memset( p_sys->pes_buffer, 4, 0 );
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
    p_sys->lastAudioPTS = get_pts( &p_block->p_buffer[ offset +
            p_sys->i_Pts_Offset ] );
    if (p_sys->firstAudioPTS < 0)
        p_sys->firstAudioPTS = p_sys->lastAudioPTS;
    p_block->i_pts = p_sys->lastAudioPTS;
    /*msg_Dbg(p_demux, "Audio PTS %lld", p_sys->lastAudioPTS );*/
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
    demux_sys_t      *p_sys = p_demux->p_sys;

    int              invalidType = 0;
    int              recordsDecoded = 0;

    int              rec_type;
    long             l_rec_size;
    int              i_cur_rec;
    int              subrec_type;
    ty_rec_hdr_t     *rec_hdr;

    block_t          *p_block_in = NULL;
    int              esOffset1;

    uint8_t          lastCC[ 16 ];
    uint8_t          lastXDS[ 16 ];

    /*msg_Dbg(p_demux, "ty demux processing" );*/
   
    /* did we hit EOF earlier? */
    if (p_sys->eof) return 0;

    /*
     * what we do (1 record now.. maybe more later):
    * - use stream_Read() to read the chunk header & record headers
    * - discard entire chunk if it is a PART header chunk
    * - parse all the headers into record header array
    * - keep a pointer of which record we're on
    * - use stream_Block() to fetch each record
    * - parse out PTS from PES headers
    * - set PTS for data packets
    * - pass the data on to the proper codec via es_out_Send()

    * if this is the first time or  
    * if we're at the end of this chunk, start a new one
    */
    /* parse the next chunk's record headers */
    if (p_sys->b_first_chunk || p_sys->i_cur_rec >= p_sys->i_num_recs)
        if (get_chunk_header(p_demux) == 0)
            return 0;

    /*======================================================================
     * parse & send one record of the chunk
     *====================================================================== */
    i_cur_rec = p_sys->i_cur_rec;
    recordsDecoded++;
    rec_hdr = &p_sys->rec_hdrs[ i_cur_rec ];
    subrec_type = rec_hdr->subrec_type;
    rec_type = rec_hdr->rec_type;
    l_rec_size = rec_hdr->l_rec_size;

    if (!rec_hdr->b_ext)
    {
        /*msg_Dbg(p_demux, "Record Type 0x%x/%02x %ld bytes",
                    subrec_type, rec_type, l_rec_size );*/
  
        /* some normal records are 0 length, so check for that... */
        if (l_rec_size > 0)
        {
            /* read in this record's payload */
            if( !( p_block_in = stream_Block( p_demux->s, l_rec_size ) ) )
            {
                /* EOF */
                p_sys->eof = 1;
                return 0;
            }
            /* set these as 'unknown' for now */
            p_block_in->i_pts = p_block_in->i_dts = 0;
        }
        else
        {
            /* no data in payload; we're done */
            p_sys->i_cur_rec++;
            return 1;
        }
    }
    /*else
    {
        -- don't read any data from the stream, data was in the record header --
        msg_Dbg(p_demux,
               "Record Type 0x%02x/%02x, ext data = %02x, %02x", subrec_type,
                rec_type, rec_hdr->ex1, rec_hdr->ex2);
    }*/

    /*================================================================*
     * Video Parsing
     *================================================================*/
    if ( rec_type == 0xe0 )
    {
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
        if( subrec_type != 0x02 && subrec_type != 0x0c
            && subrec_type != 0x08 && l_rec_size > 4 )
        {
            /* get the PTS from this packet if it has one.
             * on S1, only 0x06 has PES.  On S2, however, most all do.
             * Do NOT Pass the PES Header to the MPEG2 codec */
            esOffset1 = find_es_header( ty_VideoPacket, p_block_in->p_buffer,
                    5 );
            if ( esOffset1 != -1 )
            {
                //msg_Dbg(p_demux, "Video PES hdr in pkt type 0x%02x at offset %d",
                    //subrec_type, esOffset1);
                p_sys->lastVideoPTS = get_pts(
                        &p_block_in->p_buffer[ esOffset1 + VIDEO_PTS_OFFSET ] );
                /*msg_Dbg(p_demux, "Video rec %d PTS "I64Fd, p_sys->i_cur_rec,
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
        if (subrec_type == 0x06) {
            /* type 6 (S1 DTivo) has no data, so we're done */
            block_Release(p_block_in);
        } else {
            /* if it's not a continue blk, then set PTS */
            if (subrec_type != 0x02)
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
                if (p_sys->lastVideoPTS > 0)
                {
                    p_block_in->i_pts = p_sys->lastVideoPTS;
                    /* PTS gets used ONCE. 
                     * Any subsequent frames we get BEFORE next PES
                     * header will have their PTS computed in the codec */
                    p_sys->lastVideoPTS = 0;
                }
            }
            //msg_Dbg(p_demux, "sending rec %d as video type 0x%02x",
                    //p_sys->i_cur_rec, subrec_type);
            es_out_Send(p_demux->out, p_sys->p_video, p_block_in);
        }
    } /* end if video rec type */
    /* ================================================================
     * Audio Parsing
     * ================================================================
     * parse PES headers and send the rest to the codec
     */
    else if ( rec_type == 0xc0 )
    {
#if 0
        int i;
        printf( "Audio Packet Header " );
        for( i = 0 ; i < 24 ; i++ )
            printf( "%2.2x ", p_block_in->p_buffer[i] );
        printf( "\n" );
#endif

        /* SA or DTiVo Audio Data, no PES (continued block)
         * ================================================
         */
        if ( subrec_type == 2 )
        {
            /* continue PES if previous was incomplete */
            if (p_sys->i_pes_buf_cnt > 0)
            {
                int i_need = p_sys->i_Pes_Length - p_sys->i_pes_buf_cnt;

                msg_Dbg(p_demux, "continuing PES header");
                /* do we have enough data to complete? */
                if (i_need < l_rec_size)
                {
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
                        p_sys->lastAudioPTS = get_pts( 
                            &p_sys->pes_buffer[ esOffset1 + p_sys->i_Pts_Offset ] );
                        p_block_in->i_pts = p_sys->lastAudioPTS;
                    }
                    p_sys->i_pes_buf_cnt = 0;
                }
                else
                {
                    /* don't have complete PES hdr; save what we have and return */
                    memcpy(&p_sys->pes_buffer[p_sys->i_pes_buf_cnt],
                            p_block_in->p_buffer, l_rec_size);
                    p_sys->i_pes_buf_cnt += l_rec_size;
                    p_sys->i_cur_rec++;
                    block_Release(p_block_in);
                    return 1;
                }
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
            es_out_Send( p_demux->out, p_sys->p_audio, p_block_in );
        } /* subrec == 2 */

        /* MPEG Audio with PES Header, either SA or DTiVo   */
        /* ================================================ */
        if ( subrec_type == 0x03 )
        {
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
                p_sys->lastAudioPTS = get_pts( &p_block_in->p_buffer[
                            SA_PTS_OFFSET ] );
                if (p_sys->firstAudioPTS < 0)
                    p_sys->firstAudioPTS = p_sys->lastAudioPTS;
                block_Release(p_block_in);
                /*msg_Dbg(p_demux, "SA Audio PTS %lld",
                           p_sys->lastAudioPTS );*/
            }
            else
            /* DTiVo Audio with PES Header                      */
            /* ================================================ */
            {
                /* Check for complete PES */
                if (check_sync_pes(p_demux, p_block_in, esOffset1,
                                    l_rec_size) == -1)
                {
                    /* partial PES header found, nothing else. 
                     * we're done. */
                    p_sys->i_cur_rec++;
                    block_Release(p_block_in);
                    return 1;
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
                /* set PCR before we send */
                if( p_block_in->i_pts > 0 )
                    es_out_Control( p_demux->out, ES_OUT_SET_PCR,
                                    p_block_in->i_pts );
                es_out_Send( p_demux->out, p_sys->p_audio, p_block_in );
            }   /* if DTiVo */
        }   /* if subrec == 0x03 */

        /* SA Audio with no PES Header                      */
        /* ================================================ */
        if ( subrec_type == 0x04 )
        {
            /*msg_Dbg(p_demux,
                    "Adding SA Audio Packet Size %ld", l_rec_size ); */

            /* set PCR before we send */
            if (p_sys->lastAudioPTS > 0)
            {
                p_block_in->i_pts = p_sys->lastAudioPTS;
                es_out_Control( p_demux->out, ES_OUT_SET_PCR,
                                p_block_in->i_pts );
            }
            es_out_Send( p_demux->out, p_sys->p_audio, p_block_in );
        }

        /* DTiVo AC3 Audio Data with PES Header             */
        /* ================================================ */
        if ( subrec_type == 0x09 )
        {
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
                p_sys->i_cur_rec++;
                return 1;
            }
            /* set PCR before we send (if PTS found) */
            if( p_block_in->i_pts > 0 )
            {
                es_out_Control( p_demux->out, ES_OUT_SET_PCR,
                                p_block_in->i_pts );
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
            es_out_Send( p_demux->out, p_sys->p_audio, p_block_in );
        }
    } /* end "if audio" */
    /* ================================================================ */
    /* Closed Caption                                                   */
    /* ================================================================ */
    else if ( rec_type == 0x01 )
    {
        /*msg_Dbg(p_demux, "CC1 %02x %02x [%c%c]", rec_hdr->ex1,
                   rec_hdr->ex2, rec_hdr->ex1, rec_hdr->ex2 );*/

        /* construct a 'user-data' MPEG packet */
        lastCC[ 0x00 ] = 0x00;
        lastCC[ 0x01 ] = 0x00;
        lastCC[ 0x02 ] = 0x01;
        lastCC[ 0x03 ] = 0xb2;
        lastCC[ 0x04 ] = 'T';    /* vcdimager code says this byte should be 0x11 */
        lastCC[ 0x05 ] = 'Y';    /* (no other notes there) */
        lastCC[ 0x06 ] = 0x01;
        lastCC[ 0x07 ] = rec_hdr->ex1;
        lastCC[ 0x08 ] = rec_hdr->ex2;
        /* not sure what to send, because VLC doesn't yet support
         * teletext type of subtitles (only supports the full-sentence type) */
        /*p_block_in = block_NewEmpty(); ????
        es_out_Send( p_demux->out, p_sys->p_subt_es, p_block_in );*/
    }
    else if ( rec_type == 0x02 )
    {
        /*msg_Dbg(p_demux, "CC2 %02x %02x", rec_hdr->ex1, rec_hdr->ex2 );*/

        /* construct a 'user-data' MPEG packet */
        lastXDS[ 0x00 ] = 0x00;
        lastXDS[ 0x01 ] = 0x00;
        lastXDS[ 0x02 ] = 0x01;
        lastXDS[ 0x03 ] = 0xb2;
        lastXDS[ 0x04 ] = 'T';    /* vcdimager code says this byte should be 0x11 */
        lastXDS[ 0x05 ] = 'Y';    /* (no other notes there) */
        lastXDS[ 0x06 ] = 0x02;
        lastXDS[ 0x07 ] = rec_hdr->ex1;
        lastXDS[ 0x08 ] = rec_hdr->ex2;
        /* not sure what to send, because VLC doesn't support this?? */
        /*p_block_in = block_NewEmpty(); ????
        es_out_Send( p_demux->out, p_sys->p_audio, p_block_in );*/
    }
    /* ================================================================ */
    /* Tivo data services (e.g. "thumbs-up to record!")  useless for us */
    /* ================================================================ */
    else if ( rec_type == 0x03 )
    {
    }
    /* ================================================================ */
    /* Unknown, but seen regularly */
    /* ================================================================ */
    else if ( rec_type == 0x05 )
    {
    }
    else
    {
        msg_Dbg(p_demux, "Invalid record type 0x%02x", rec_type );
        if (p_block_in) block_Release(p_block_in);
            invalidType++;
    }
    p_sys->i_cur_rec++;
    return 1;
}


/* seek to a position within the stream, if possible */
static int ty_stream_seek_pct(demux_t *p_demux, double seek_pct)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int64_t seek_pos = p_sys->i_stream_size * seek_pct;
    int i, i_cur_part;
    long l_skip_amt;

    /* if we're not seekable, there's nothing to do */
    if (!p_sys->b_seekable)
        return VLC_EGENERIC;

    /* figure out which part & chunk we want & go there */
    i_cur_part = seek_pos / TIVO_PART_LENGTH;
    p_sys->i_cur_chunk = seek_pos / CHUNK_SIZE;
    
    /* try to read the part header (master chunk) if it's there */
    if ( stream_Seek( p_demux->s, i_cur_part * TIVO_PART_LENGTH ))
    {
        /* can't seek stream */
        return VLC_EGENERIC;
    }
    parse_master(p_demux);

    /* now for the actual chunk */
    if ( stream_Seek( p_demux->s, p_sys->i_cur_chunk * CHUNK_SIZE))
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
    msg_Dbg(p_demux, "Seeked to file pos " I64Fd, seek_pos);
    msg_Dbg(p_demux, " (chunk %d, record %d)",
             p_sys->i_cur_chunk - 1, p_sys->i_cur_rec);

    /* seek to the start of this record's data.
     * to do that, we have to skip past all prior records */
    l_skip_amt = 0;
    for (i=0; i<p_sys->i_cur_rec; i++)
        l_skip_amt += p_sys->rec_hdrs[i].l_rec_size;
    stream_Seek(p_demux->s, ((p_sys->i_cur_chunk-1) * CHUNK_SIZE) +
                 (p_sys->i_num_recs * 16) + l_skip_amt + 4);

    /* to hell with syncing any audio or video, just start reading records... :) */
    /*p_sys->lastAudioPTS = p_sys->lastVideoPTS = 0;*/
    es_out_Control( p_demux->out, ES_OUT_RESET_PCR );
    return VLC_SUCCESS;
}


/* seek to an exact time position within the stream, if possible.
 * l_seek_time is in nanoseconds, the TIVO time standard.
 */
static int ty_stream_seek_time(demux_t *p_demux, uint64_t l_seek_time)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i, i_seq_entry = 0;
    int i_skip_cnt;
    long l_cur_pos = stream_Tell(p_demux->s);
    int i_cur_part = l_cur_pos / TIVO_PART_LENGTH;
    long l_seek_secs = l_seek_time / 1000000000;
    uint64_t l_fwd_stamp = 1;

    /* if we're not seekable, there's nothing to do */
    if (!p_sys->b_seekable || !p_sys->b_have_master)
        return VLC_EGENERIC;

    msg_Dbg(p_demux, "Skipping to time %02ld:%02ld:%02ld",
            l_seek_secs / 3600, (l_seek_secs / 60) % 60, l_seek_secs % 60);

    /* seek to the proper segment if necessary */
    /* first see if we need to go back */
    while (l_seek_time < p_sys->l_first_ty_pts) {
        msg_Dbg(p_demux, "skipping to prior segment.");
        /* load previous part */
        if (i_cur_part == 0) {
            stream_Seek(p_demux->s, l_cur_pos);
            msg_Err(p_demux, "Attempt to seek past BOF");
            return VLC_EGENERIC;
        }
        stream_Seek(p_demux->s, (i_cur_part - 1) * TIVO_PART_LENGTH);
        i_cur_part--;
        parse_master(p_demux);
    }
    /* maybe we need to go forward */
    while (l_seek_time > p_sys->l_final_ty_pts) {
        msg_Dbg(p_demux, "skipping to next segment.");
        /* load next part */
        if ((i_cur_part + 1) * TIVO_PART_LENGTH > p_sys->i_stream_size) {
            /* error; restore previous file position */
            stream_Seek(p_demux->s, l_cur_pos);
            msg_Err(p_demux, "seek error");
            return VLC_EGENERIC;
        }
        stream_Seek(p_demux->s, (i_cur_part + 1) * TIVO_PART_LENGTH);
        i_cur_part++;
        parse_master(p_demux);
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
            stream_Seek(p_demux->s, l_cur_pos);
            msg_Err(p_demux, "seek error");
            return VLC_EGENERIC;
        }
        stream_Seek(p_demux->s, (i_cur_part + 1) * TIVO_PART_LENGTH);
        i_cur_part++;
        parse_master(p_demux);
        i_seq_entry = 0;
    }     
     
    /* determine which chunk has our seek_time */
    for (i=0; i<p_sys->i_bits_per_seq_entry; i++) {
        long l_chunk_nr = i_seq_entry * p_sys->i_bits_per_seq_entry + i;
        long l_chunk_offset = (l_chunk_nr + 1) * CHUNK_SIZE;
        msg_Dbg(p_demux, "testing part %d chunk %ld mask 0x%02X bit %d",
            i_cur_part, l_chunk_nr,
            p_sys->seq_table[i_seq_entry].chunk_bitmask[i/8], i%8);
        if (p_sys->seq_table[i_seq_entry].chunk_bitmask[i/8] & (1 << (i%8))) {
            /* check this chunk's SEQ header timestamp */
            msg_Dbg(p_demux, "has SEQ. seeking to chunk at 0x%lX",
                (i_cur_part * TIVO_PART_LENGTH) + l_chunk_offset);
            stream_Seek(p_demux->s, (i_cur_part * TIVO_PART_LENGTH) +
                l_chunk_offset);
            // TODO: we don't have to parse the full header set;
            // just test the seq_rec entry for its timestamp
            p_sys->i_stuff_cnt = 0;
            get_chunk_header(p_demux);
            // check ty PTS for the SEQ entry in this chunk
            if (p_sys->i_seq_rec < 0 || p_sys->i_seq_rec > p_sys->i_num_recs) {
                msg_Err(p_demux, "no SEQ hdr in chunk; table had one.");
                /* Seek to beginning of original chunk & reload it */
                stream_Seek(p_demux->s, (l_cur_pos / CHUNK_SIZE) * CHUNK_SIZE);
                p_sys->i_stuff_cnt = 0;
                get_chunk_header(p_demux);
                return VLC_EGENERIC;
            }
            l_seek_secs = p_sys->rec_hdrs[p_sys->i_seq_rec].l_ty_pts /
                1000000000;
            msg_Dbg(p_demux, "found SEQ hdr for timestamp %02ld:%02ld:%02ld",
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
    for (i=0; i<p_sys->i_seq_rec; i++)
        i_skip_cnt += p_sys->rec_hdrs[i].l_rec_size;
    stream_Read(p_demux->s, NULL, i_skip_cnt);
    p_sys->i_cur_rec = p_sys->i_seq_rec;
    //p_sys->l_last_ty_pts = p_sys->rec_hdrs[p_sys->i_seq_rec].l_ty_pts;
    //p_sys->l_last_ty_pts_sync = p_sys->lastAudioPTS;

    return VLC_SUCCESS;
}


/* parse a master chunk, filling the SEQ table and other variables.
 * We assume the stream is currently pointing to it.
 */
static void parse_master(demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    uint8_t mst_buf[32];
    int i, i_map_size;
    int64_t i_save_pos = stream_Tell(p_demux->s);
    int64_t i_pts_secs;

    /* Note that the entries in the SEQ table in the stream may have
       different sizes depending on the bits per entry.  We store them
       all in the same size structure, so we have to parse them out one
       by one.  If we had a dynamic structure, we could simply read the
       entire table directly from the stream into memory in place. */

    /* clear the SEQ table */
    if (p_sys->seq_table != NULL)
        free(p_sys->seq_table);
    
    /* parse header info */
    stream_Read(p_demux->s, mst_buf, 32);
    i_map_size = U32_AT(&mst_buf[20]);  /* size of bitmask, in bytes */
    p_sys->i_bits_per_seq_entry = i_map_size * 8;
    i = U32_AT(&mst_buf[28]);   /* size of SEQ table, in bytes */
    p_sys->i_seq_table_size = i / (8 + i_map_size);

    /* parse all the entries */
    p_sys->seq_table = malloc(p_sys->i_seq_table_size * sizeof(ty_seq_table_t));
    for (i=0; i<p_sys->i_seq_table_size; i++) {
        stream_Read(p_demux->s, mst_buf, 8 + i_map_size);
        p_sys->seq_table[i].l_timestamp = U64_AT(&mst_buf[0]);
        if (i_map_size > 8) {
            msg_Err(p_demux, "Unsupported SEQ bitmap size in master chunk");
            memset(p_sys->seq_table[i].chunk_bitmask, i_map_size, 0);
        } else {
            memcpy(p_sys->seq_table[i].chunk_bitmask, &mst_buf[8], i_map_size);
        }
    }

    /* set up a few of our variables */
    p_sys->l_first_ty_pts = p_sys->seq_table[0].l_timestamp;
    p_sys->l_final_ty_pts =
        p_sys->seq_table[p_sys->i_seq_table_size - 1].l_timestamp;
    p_sys->b_have_master = VLC_TRUE;

    i_pts_secs = p_sys->l_first_ty_pts / 1000000000;
    msg_Dbg( p_demux, "first TY pts in master is %02d:%02d:%02d",
             (int)(i_pts_secs / 3600), (int)((i_pts_secs / 60) % 60), (int)(i_pts_secs % 60) );
    i_pts_secs = p_sys->l_final_ty_pts / 1000000000;
    msg_Dbg( p_demux, "final TY pts in master is %02d:%02d:%02d",
             (int)(i_pts_secs / 3600), (int)((i_pts_secs / 60) % 60), (int)(i_pts_secs % 60) );

    /* seek past this chunk */
    stream_Seek(p_demux->s, i_save_pos + CHUNK_SIZE);
}


static int Control(demux_t *p_demux, int i_query, va_list args)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double f, *pf;
    int64_t i64, *p_i64;

    /*msg_Info(p_demux, "control cmd %d", i_query);*/
    switch( i_query )
    {
    case DEMUX_GET_POSITION:
        /* arg is 0.0 - 1.0 percent of overall file position */
        if( ( i64 = p_sys->i_stream_size ) > 0 )
        {
            pf = (double*) va_arg( args, double* );
            *pf = (double)stream_Tell( p_demux->s ) / (double) i64;
            return VLC_SUCCESS;
        }
        return VLC_EGENERIC;

    case DEMUX_SET_POSITION:
        /* arg is 0.0 - 1.0 percent of overall file position */
        f = (double) va_arg( args, double );
        /* msg_Dbg(p_demux, "Control - set position to %2.3f", f); */
        if ((i64 = p_sys->i_stream_size) > 0)
            return ty_stream_seek_pct(p_demux, f);
        return VLC_EGENERIC;
    case DEMUX_GET_TIME:
        /* return TiVo timestamp */
        p_i64 = (int64_t *) va_arg(args, int64_t *);
        //*p_i64 = p_sys->lastAudioPTS - p_sys->firstAudioPTS;
        //*p_i64 = (p_sys->l_last_ty_pts / 1000) + (p_sys->lastAudioPTS -
        //    p_sys->l_last_ty_pts_sync);
        *p_i64 = (p_sys->l_last_ty_pts / 1000);
        return VLC_SUCCESS;
    case DEMUX_GET_LENGTH:    /* length of program in microseconds, 0 if unk */
        /* size / bitrate */
        p_i64 = (int64_t *) va_arg(args, int64_t *);
        *p_i64 = 0;
        return VLC_SUCCESS;
    case DEMUX_SET_TIME:      /* arg is time in microsecs */
        i64 = (int64_t) va_arg( args, int64_t );
        return ty_stream_seek_time(p_demux, i64 * 1000);
    case DEMUX_GET_FPS:
    default:
        return VLC_EGENERIC;
    }
}


/* ======================================================================== */
static void Close( vlc_object_t *p_this )
{
    demux_sys_t *p_sys = ((demux_t *) p_this)->p_sys;

    free(p_sys->rec_hdrs);
    free(p_sys);
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
    vlc_bool_t b_probe_error = VLC_FALSE;

    /* we need CHUNK_PEEK_COUNT chunks of data, first one might be a Part header, so ... */
    if (stream_Peek( p_demux->s, &p_buf, CHUNK_PEEK_COUNT * CHUNK_SIZE ) <
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
        b_probe_error = VLC_TRUE;
    }
    if (p_sys->audio_type == TIVO_AUDIO_UNKNOWN) {
        msg_Err(p_demux, "Can't determine Tivo Audio Type.");
        b_probe_error = VLC_TRUE;
    }
    if (p_sys->tivo_type == TIVO_TYPE_UNKNOWN) {
        msg_Err(p_demux, "Can't determine Tivo Type (SA/DTivo).");
        b_probe_error = VLC_TRUE;
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
    uint32_t i_payload_size;

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
    p_hdrs = parse_chunk_headers(p_demux, p_chunk, i_num_recs, &i_payload_size);
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
        stream_Read( p_demux->s, NULL, p_sys->i_stuff_cnt);
        p_sys->i_stuff_cnt = 0;
    }

    /* read the TY packet header */
    i_readSize = stream_Peek( p_demux->s, &p_peek, 4 );
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
        parse_master(p_demux);
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
    p_sys->b_first_chunk = VLC_FALSE;
  
    /*msg_Dbg( p_demux, "chunk has %d records", i_num_recs );*/

    if (p_sys->rec_hdrs)
        free(p_sys->rec_hdrs);

    /* skip past the 4 bytes we "peeked" earlier */
    stream_Read( p_demux->s, NULL, 4 );

    /* read the record headers into a temp buffer */
    p_hdr_buf = malloc(i_num_recs * 16);
    if (stream_Read(p_demux->s, p_hdr_buf, i_num_recs * 16) < i_num_recs * 16) {
        p_sys->eof = VLC_TRUE;
        return 0;
    }
    /* parse them */
    p_sys->rec_hdrs = parse_chunk_headers(p_demux, p_hdr_buf, i_num_recs,
            &i_payload_size);
    free(p_hdr_buf);

    p_sys->i_stuff_cnt = CHUNK_SIZE - 4 -
        (p_sys->i_num_recs * 16) - i_payload_size;
    if (p_sys->i_stuff_cnt > 0)
        msg_Dbg( p_demux, "chunk has %d stuff bytes at end",
                 p_sys->i_stuff_cnt );
    return 1;
}


static ty_rec_hdr_t *parse_chunk_headers( demux_t *p_demux, const uint8_t *p_buf,
                                          int i_num_recs, int *pi_payload_size)
{
    int i;
    ty_rec_hdr_t *p_hdrs, *p_rec_hdr;

    *pi_payload_size = 0;
    p_hdrs = malloc(i_num_recs * sizeof(ty_rec_hdr_t));

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
            b1 &= 0x7f;
            b2 = ( ( ( record_header[ 1 ] & 0x0f ) << 4 ) | 
                   ( ( record_header[ 2 ] & 0xf0 ) >> 4 ) );
            b2 &= 0x7f;

            p_rec_hdr->ex1 = b1;
            p_rec_hdr->ex2 = b2;
            p_rec_hdr->l_rec_size = 0;
            p_rec_hdr->l_ty_pts = 0;
            p_rec_hdr->b_ext = VLC_TRUE;
        }
        else
        {
            p_rec_hdr->l_rec_size = ( record_header[ 0 ] << 8 |
                record_header[ 1 ] ) << 4 | ( record_header[ 2 ] >> 4 );
            *pi_payload_size += p_rec_hdr->l_rec_size;
            p_rec_hdr->b_ext = VLC_FALSE;
            p_rec_hdr->l_ty_pts = U64_AT( &record_header[ 8 ] );
        }
    } /* end of record-header loop */
    return p_hdrs;
}
