/*****************************************************************************
 * ty.c - TiVo ty stream video demuxer for VLC
 *****************************************************************************
 * Copyright (C) 2005 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *
 * CODE CHANGES:
 * v1.0.0 - 24-Feb-2005 - Initial release - Series 1 support ONLY!
 * v1.0.1 - 25-Feb-2005 - Added fix for bad GOP headers - Neal
 * v1.0.2 - 26-Feb-2005 - No longer require "seekable" input stream - Neal
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <stdlib.h>
#include <vlc/vlc.h>
#include <vlc/input.h>
#include "vlc_codec.h"

#define SERIES1_PES_LENGTH  (11)
#define SERIES2_PES_LENGTH  (16)
#define AC3_PES_LENGTH      (14)
#define DTIVO_PTS_OFFSET    (6)
#define SA_PTS_OFFSET       (9)
#define AC3_PTS_OFFSET      (9)
static const unsigned char ty_VideoPacket[] = { 0x00, 0x00, 0x01, 0xe0 };
static const unsigned char ty_MPEGAudioPacket[] = { 0x00, 0x00, 0x01, 0xc0 };
static const unsigned char ty_AC3AudioPacket[] = { 0x00, 0x00, 0x01, 0xbd };

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int get_chunk_header(demux_t *);
static void setup_audio_streams(char, demux_t *);
static mtime_t get_pts( unsigned char *buf );
static int find_es_header( unsigned const char *header,
   unsigned char *buffer, int bufferSize, int *esOffset1 );
static int ty_stream_seek(demux_t *p_demux, double seek_pct);

static int TyOpen (vlc_object_t *);
static void TyClose(vlc_object_t *);
static int TyDemux(demux_t *);
static int Control(demux_t *, int, va_list);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_shortname( _("TY") );
    set_description(_("TY Stream audio/video demux"));
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_capability("demux2", 8);
    /* FIXME: there seems to be a segfault when using PVR access
     * and TY demux has a bigger priority than PS
     * Something must be wrong.
     */
    set_callbacks(TyOpen, TyClose);
    add_shortcut("ty");
    add_shortcut("tivo");
vlc_module_end();

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
*/

#define TIVO_PES_FILEID   ( 0xf5467abd )
#define TIVO_PART_LENGTH  ( 0x20000000 )    /* 536,870,912 bytes */
#define CHUNK_SIZE        ( 128 * 1024 )

typedef struct
{
  long l_rec_size;
  unsigned char ex1, ex2;
  unsigned char rec_type;
  unsigned char subrec_type;
  char b_ext;
} ty_rec_hdr_t;

struct demux_sys_t
{
  es_out_id_t *p_video;               /* ptr to video codec */
  es_out_id_t *p_audio;               /* holds either ac3 or mpeg codec ptr */

  int             i_chunk_count;
  int             i_stuff_cnt;
  size_t          i_stream_size;      /* size of input stream (if known) */
  vlc_bool_t      b_seekable;         /* is this stream seekable? */
  int             tivoType;           /* 1 = SA, 2 = DTiVo */
  vlc_bool_t      b_mpeg_audio;       /* true if we're using MPEG audio */
  uint8_t         pes_buffer[20];     /* holds incomplete pes headers */
  int             i_pes_buf_cnt;      /* how many bytes in our buffer */

  mtime_t         firstAudioPTS;
  mtime_t         lastAudioPTS;
  mtime_t         lastVideoPTS;

  ty_rec_hdr_t    *rec_hdrs;          /* record headers array */
  int             i_cur_rec;          /* current record in this chunk */
  int             i_num_recs;         /* number of recs in this chunk */
  int             i_seq_rec;          /* record number where seq start is */
  vlc_bool_t      eof;
  vlc_bool_t      b_first_chunk;
};


/*
 * TyOpen: check file and initialize demux structures
 *
 * here's what we do:
 * 1. peek at the first 12 bytes of the stream for the
 *    magic TiVo PART header & stream type & chunk size
 * 2. if it's not there, error with VLC_EGENERIC
 * 3. set up video (mpgv) codec
 * 4. return VLC_SUCCESS
 */
static int TyOpen(vlc_object_t *p_this)
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys;
    vlc_bool_t b_seekable;
    es_format_t fmt;
    uint8_t *p_peek;

    /* see if this stream is seekable */
    stream_Control( p_demux->s, STREAM_CAN_SEEK, &b_seekable );
  
    /* peek at the first 12 bytes. */
    /* for TY streams, they're always the same */
    if( stream_Peek( p_demux->s, &p_peek, 12 ) < 12 )
        return VLC_EGENERIC;

    if ( U32_AT(p_peek) != TIVO_PES_FILEID ||
         U32_AT(&p_peek[4]) != 0x02 ||
         U32_AT(&p_peek[8]) != CHUNK_SIZE )
    {
        /* doesn't look like a TY file... */
        char *psz_ext = strrchr(p_demux->psz_path, '.');
        /* if they specified tydemux, or if the file ends in .ty we try anyway */
        if (strcmp(p_demux->psz_demux, "tydemux") && strcasecmp(psz_ext, ".ty"))
            return VLC_EGENERIC;
        msg_Warn(p_demux, "this does not look like a TY file, continuing anyway...");
    }

    /* at this point, we assume we have a valid TY stream */  
    msg_Dbg( p_demux, "valid TY stream detected" );

    /* Set exported functions */
    p_demux->pf_demux = TyDemux;
    p_demux->pf_control = Control;

    /* create our structure that will hold all data */
    p_demux->p_sys = p_sys = malloc(sizeof(demux_sys_t));
    memset(p_sys, 0, sizeof(demux_sys_t));

    /* set up our struct (most were zero'd out with the memset above) */
    p_sys->b_first_chunk = VLC_TRUE;
    p_sys->firstAudioPTS = -1;
    p_sys->i_stream_size = stream_Size(p_demux->s);
    p_sys->b_mpeg_audio = VLC_FALSE;
    p_sys->b_seekable = b_seekable;
  
    /* TODO: read first chunk & parse first audio PTS, then (if seekable)
     *       seek to last chunk & last record; read its PTS and compute
     *       overall program time.  Also determine Tivo type.   */

    /* NOTE: we wait to create the audio ES until we know what
     * audio type we have.   */
    p_sys->p_audio = NULL;

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


/* set up audio codec.
 * this will be called once we determine audio type */
static void setup_audio_streams(char stream_type, demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    es_format_t  fmt;

    if (stream_type == 'A') {
        /* AC3 audio detected */
        es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC( 'a', '5', '2', ' ' ) );
        p_sys->tivoType = 2;      /* AC3 is only on dtivo */
    } else {
        /* assume MPEG */
        es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC( 'm', 'p', 'g', 'a' ) );
        p_sys->b_mpeg_audio = VLC_TRUE;
    }
    /* register the chosen audio output codec */
    p_sys->p_audio = es_out_Add( p_demux->out, &fmt );
}


/* =========================================================================== */
/* Compute Presentation Time Stamp (PTS)
 * Assume buf points to beginning of PTS */
static mtime_t get_pts( unsigned char *buf )
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
static int find_es_header( unsigned const char *header,
   unsigned char *buffer, int bufferSize, int *esOffset1 )
{
    int count;

    *esOffset1 = -1;
    for( count = 0 ; count < bufferSize ; count++ )
    {
        if ( ( buffer[ count + 0 ] == header[ 0 ] ) &&
             ( buffer[ count + 1 ] == header[ 1 ] ) &&
             ( buffer[ count + 2 ] == header[ 2 ] ) &&
             ( buffer[ count + 3 ] == header[ 3 ] ) )
        {
            *esOffset1 = count;
            return 1;
        }
    }
    return( -1 );
}


/* =========================================================================== */
/* check if we have a full PES header, if not, then save what we have.
 * this is called when audio-start packets are encountered.
 * Returns:
 *     1 partial PES hdr found, some audio data found (buffer adjusted),
 *    -1 partial PES hdr found, no audio data found
 *     0 otherwise (complete PES found, pts extracted, pts set, buffer adjusted) */
/* TODO: fix it so it works with S2 / SA / DTivo / HD etc... */
static int check_sync_pes( demux_t *p_demux, block_t *p_block,
                           int32_t offset, int32_t rec_len )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int pts_offset;
    int pes_length = p_sys->b_mpeg_audio?SERIES1_PES_LENGTH:AC3_PES_LENGTH;

    if( p_sys->tivoType == 1 )
    {
        /* SA tivo */
        pts_offset = SA_PTS_OFFSET;
    }
    else
    {
        /* DTivo */
        pts_offset = p_sys->b_mpeg_audio?DTIVO_PTS_OFFSET:AC3_PTS_OFFSET;
    }
    if ( offset < 0 || offset + pes_length > rec_len )
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
    p_sys->lastAudioPTS = get_pts( &p_block->p_buffer[ offset + pts_offset ] );
    if (p_sys->firstAudioPTS < 0)
        p_sys->firstAudioPTS = p_sys->lastAudioPTS;
    p_block->i_pts = p_sys->lastAudioPTS;
    /*msg_Dbg(p_demux, "Audio PTS %lld", p_sys->lastAudioPTS );*/
    /* adjust audio record to remove PES header */
    memmove(p_block->p_buffer + offset, p_block->p_buffer + offset + pes_length,
            rec_len - pes_length);
    p_block->i_buffer -= pes_length;
    return 0;
}


/* =========================================================================== */
/* TyDemux: Read & Demux one record from the chunk
 *
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *
 * NOTE: I think we can return the number of packets sent instead of just 1.
 * that means we can demux an entire chunk and shoot it back (may be more efficient)
 * -- should try that some day :) --
 */
int TyDemux(demux_t *p_demux)
{
    int              invalidType = 0;
    int              recordsDecoded = 0;

    int              rec_type;
    long             l_rec_size;
    int              i_cur_rec;
    int              subrec_type;
    ty_rec_hdr_t     *rec_hdr;

    block_t          *p_block_in = NULL;
    int              esOffset1;

    unsigned char    lastCC[ 16 ];
    unsigned char    lastXDS[ 16 ];

    demux_sys_t      *p_sys = p_demux->p_sys;

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
        if( subrec_type == 0x06 )
        {
            /* get the PTS from this packet.
             * Do NOT Pass this packet (a PES Header) on to the MPEG2 codec */
            find_es_header( ty_VideoPacket, p_block_in->p_buffer,
                            l_rec_size, &esOffset1 );
            if ( esOffset1 != -1 )
            {
                /* msg_Dbg(p_demux, "Video PES hdr at offset %d", esOffset1); */
                p_sys->lastVideoPTS = get_pts( &p_block_in->p_buffer[ esOffset1 + 9 ] );
                /*msg_Dbg(p_demux, "Video rec %d PTS "I64Fd, p_sys->i_cur_rec,
                            p_sys->lastVideoPTS );*/
            }
            block_Release(p_block_in);
        }
        else
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
        /* load a codec if we haven't yet */
        if ( p_sys->p_audio == NULL )
        {
            if ( subrec_type == 0x09 )
            {
                /* set up for AC-3 audio */
                msg_Dbg(p_demux, "detected AC-3 Audio" );
                        setup_audio_streams('A', p_demux);
            }
            else
            {
                /* set up for MPEG audio */
                msg_Dbg(p_demux, "detected MPEG Audio" );
                setup_audio_streams('M', p_demux);
            }
        }

        /* SA or DTiVo Audio Data, no PES (continued block)
         * ================================================
         */
        if ( subrec_type == 2 )
        {
            /* continue PES if previous was incomplete */
            /* TODO: Make this work for all series & types of tivos */
            if (p_sys->i_pes_buf_cnt > 0)
            {
                int i_need = SERIES1_PES_LENGTH - p_sys->i_pes_buf_cnt;

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
                    if (p_sys->b_mpeg_audio)
                        find_es_header(ty_MPEGAudioPacket, p_sys->pes_buffer,
                                        10, &esOffset1);
                    else
                        find_es_header(ty_AC3AudioPacket, p_sys->pes_buffer,
                                        10, &esOffset1);
                    if (esOffset1 < 0)
                    {
                        /* god help us; something's really wrong */
                        msg_Err(p_demux, "can't find audio PES header in packet");
                    }
                    else
                    {
                        p_sys->lastAudioPTS = get_pts( 
                            &p_sys->pes_buffer[ esOffset1 + DTIVO_PTS_OFFSET ] );
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
            /* set PCR before we send */
            /*es_out_Control( p_demux->out, ES_OUT_SET_PCR,
                              p_block_in->i_pts );*/
            es_out_Send( p_demux->out, p_sys->p_audio, p_block_in );
        } /* subrec == 2 */

        /* MPEG Audio with PES Header, either SA or DTiVo   */
        /* ================================================ */
        if ( subrec_type == 0x03 )
        {
            find_es_header( ty_MPEGAudioPacket, p_block_in->p_buffer,
            l_rec_size, &esOffset1 );

            /*msg_Dbg(p_demux, "buffer has %#02x %#02x %#02x %#02x",
               p_block_in->p_buffer[0], p_block_in->p_buffer[1],
               p_block_in->p_buffer[2], p_block_in->p_buffer[3]);
            msg_Dbg(p_demux, "audio ES hdr at offset %d", esOffset1);*/

            /* SA PES Header, No Audio Data                     */
            /* ================================================ */
            if ( ( esOffset1 == 0 ) && ( l_rec_size == 16 ) )
            {
                p_sys->tivoType = 1;
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
                p_sys->tivoType = 2;

                /* Check for complete PES
                 * (TODO: handle proper size for tivo version) */
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
            find_es_header( ty_AC3AudioPacket, p_block_in->p_buffer,
                            l_rec_size, &esOffset1 );

            /*msg_Dbg(p_demux, "buffer has %#02x %#02x %#02x %#02x",
                       p_block_in->p_buffer[0], p_block_in->p_buffer[1],
                       p_block_in->p_buffer[2], p_block_in->p_buffer[3]);
            msg_Dbg(p_demux, "audio ES AC3 hdr at offset %d", esOffset1);*/

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
static int ty_stream_seek(demux_t *p_demux, double seek_pct)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int64_t seek_pos = p_sys->i_stream_size * seek_pct;
    int i;
    long l_skip_amt;

    /* if we're not seekable, there's nothing to do */
    if (!p_sys->b_seekable)
        return VLC_EGENERIC;

    /* figure out which chunk we want & go there */
    p_sys->i_chunk_count = seek_pos / CHUNK_SIZE;

    if ( stream_Seek( p_demux->s, p_sys->i_chunk_count * CHUNK_SIZE))
    {
        /* can't seek stream */
        return VLC_EGENERIC;
    }
    /* load the chunk */
    get_chunk_header(p_demux);
  
    /* seek within the chunk to get roughly to where we want */
    p_sys->i_cur_rec = (int)
      ((double) ((seek_pos % CHUNK_SIZE) / (double) (CHUNK_SIZE)) * p_sys->i_num_recs);
    msg_Dbg(p_demux, "Seeked to file pos " I64Fd, seek_pos);
    msg_Dbg(p_demux, " (chunk %d, record %d)",
             p_sys->i_chunk_count - 1, p_sys->i_cur_rec);

    /* seek to the start of this record's data.
     * to do that, we have to skip past all prior records */
    l_skip_amt = 0;
    for (i=0; i<p_sys->i_cur_rec; i++)
        l_skip_amt += p_sys->rec_hdrs[i].l_rec_size;
    stream_Seek(p_demux->s, ((p_sys->i_chunk_count-1) * CHUNK_SIZE) +
                 (p_sys->i_num_recs * 16) + l_skip_amt + 4);

    /* to hell with syncing any audio or video, just start reading records... :) */
    /*p_sys->lastAudioPTS = p_sys->lastVideoPTS = 0;*/
    es_out_Control( p_demux->out, ES_OUT_RESET_PCR );
    return VLC_SUCCESS;
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
        //msg_Dbg(p_demux, "Control - set position to %2.3f", f);
        if ((i64 = p_sys->i_stream_size) > 0)
            return ty_stream_seek(p_demux, f);
        return VLC_EGENERIC;
    case DEMUX_GET_TIME:
        /* return latest PTS - start PTS */
        p_i64 = (int64_t *) va_arg(args, int64_t *);
        *p_i64 = p_sys->lastAudioPTS - p_sys->firstAudioPTS;
        return VLC_SUCCESS;
    case DEMUX_SET_TIME:      /* arg is time in microsecs */
    case DEMUX_GET_LENGTH:    /* length of program in microseconds, 0 if unk */
    case DEMUX_GET_FPS:
    default:
        return VLC_EGENERIC;
    }
}


/* =========================================================================== */
static void TyClose( vlc_object_t *p_this )
{
    demux_sys_t *p_sys = ((demux_t *) p_this)->p_sys;

    free(p_sys->rec_hdrs);
    free(p_sys);
}


/* =========================================================================== */
static int get_chunk_header(demux_t *p_demux)
{
    int i_readSize, i_num_recs, i;
    uint8_t packet_header[4];
    uint8_t record_header[16];
    ty_rec_hdr_t *p_rec_hdr;
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_payload_size = 0;         /* sum of all records */

    msg_Dbg(p_demux, "parsing ty chunk #%d", p_sys->i_chunk_count );

    /* if we have left-over filler space from the last chunk, get that */
    if (p_sys->i_stuff_cnt > 0)
        stream_Read( p_demux->s, NULL, p_sys->i_stuff_cnt);

    /* read the TY packet header */
    i_readSize = stream_Read( p_demux->s, packet_header, 4 );
    p_sys->i_chunk_count++;
  
    if ( i_readSize < 4 )
    {
        /* EOF */
        p_sys->eof = 1;
        return 0;
    }
  
    /* if it's a PART Header, then try again. */
    if( U32_AT( &packet_header[ 0 ] ) == TIVO_PES_FILEID )
    {
        msg_Dbg( p_demux, "skipping TY PART Header" );
        /* TODO: if stream is seekable, should we seek() instead of read() ?? */
        stream_Read( p_demux->s, NULL, CHUNK_SIZE - 4 );
        return get_chunk_header(p_demux);
    }

    /* number of records in chunk (8- or 16-bit number) */
    if (packet_header[3] & 0x80)
    {
        /* 16 bit rec cnt */
        p_sys->i_num_recs = i_num_recs = (packet_header[1] << 8) + packet_header[0];
        p_sys->i_seq_rec = (packet_header[3] << 8) + packet_header[2];
        if (p_sys->i_seq_rec != 0xffff)
        {
            p_sys->i_seq_rec &= ~0x8000;
        }
    }
    else
    {
        /* 8 bit reclen - tivo 1.3 format */
        p_sys->i_num_recs = i_num_recs = packet_header[0];
        p_sys->i_seq_rec = packet_header[1];
    }
    p_sys->i_cur_rec = 0;
    p_sys->b_first_chunk = VLC_FALSE;
  
    /*msg_Dbg( p_demux, "chunk has %d records", i_num_recs );*/

    /* parse headers into array */
    if (p_sys->rec_hdrs)
        free(p_sys->rec_hdrs);
    p_sys->rec_hdrs = malloc(i_num_recs * sizeof(ty_rec_hdr_t));
    for (i = 0; i < i_num_recs; i++)
    {
        i_readSize = stream_Read( p_demux->s, record_header, 16 );
        if (i_readSize < 16)
        {
            /* EOF */
            p_sys->eof = VLC_TRUE;
            return 0;
        }
        p_rec_hdr = &p_sys->rec_hdrs[i];     /* for brevity */
        p_rec_hdr->rec_type = record_header[3];
        p_rec_hdr->subrec_type = record_header[2] & 0x0f;
        if ((record_header[ 0 ] & 0x80) == 0x80)
        {
            unsigned char b1, b2;
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
            p_rec_hdr->b_ext = VLC_TRUE;
        }
        else
        {
            p_rec_hdr->l_rec_size = ( record_header[ 0 ] << 8 |
                record_header[ 1 ] ) << 4 | ( record_header[ 2 ] >> 4 );
            i_payload_size += p_rec_hdr->l_rec_size;
            p_rec_hdr->b_ext = VLC_FALSE;
        }
    } /* end of record-header loop */
    p_sys->i_stuff_cnt = CHUNK_SIZE - 4 -
        (p_sys->i_num_recs * 16) - i_payload_size;
    if (p_sys->i_stuff_cnt > 0)
        msg_Dbg( p_demux, "chunk has %d stuff bytes at end",
                 p_sys->i_stuff_cnt );
    return 1;
}
