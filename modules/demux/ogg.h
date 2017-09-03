/*****************************************************************************
 * ogg.h : ogg stream demux module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2010 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *          Andre Pang <Andre.Pang@csiro.au> (Annodex support)
 *          Gabriel Finch <salsaman@gmail.com> (moved from ogg.c to ogg.h)
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_LIBVORBIS
  #include <vorbis/codec.h>
#endif

/*****************************************************************************
 * Definitions of structures and functions used by this plugin
 *****************************************************************************/

//#define OGG_DEMUX_DEBUG 1
#ifdef OGG_DEMUX_DEBUG
  #define DemuxDebug(code) code
#else
  #define DemuxDebug(code)
#endif

/* Some defines from OggDS http://svn.xiph.org/trunk/oggds/ */
#define PACKET_TYPE_HEADER   0x01
#define PACKET_TYPE_BITS     0x07
#define PACKET_LEN_BITS01    0xc0
#define PACKET_LEN_BITS2     0x02
#define PACKET_IS_SYNCPOINT  0x08

typedef struct oggseek_index_entry demux_index_entry_t;
typedef struct ogg_skeleton_t ogg_skeleton_t;

typedef struct backup_queue
{
    block_t *p_block;
    mtime_t i_duration;
} backup_queue_t;

typedef struct logical_stream_s
{
    ogg_stream_state os;                        /* logical stream of packets */

    es_format_t      fmt;
    es_format_t      fmt_old;                  /* format of old ES is reused */
    es_out_id_t      *p_es;
    double           f_rate;

    int              i_serial_no;

    /* the header of some logical streams (eg vorbis) contain essential
     * data for the decoder. We back them up here in case we need to re-feed
     * them to the decoder. */
    bool             b_force_backup;
    int              i_packets_backup;
    int32_t          i_extra_headers_packets;
    void             *p_headers;
    int              i_headers;
    ogg_int64_t      i_previous_granulepos;
    ogg_int64_t      i_granulepos_offset;/* first granule offset */

    /* program clock reference (in units of 90kHz) derived from the previous
     * granulepos */
    mtime_t          i_pcr;
    mtime_t          i_previous_pcr;

    /* Misc */
    bool b_initializing;
    bool b_finished;
    bool b_reinit;
    bool b_oggds;
    int i_granule_shift;

    /* Opus has a starting offset in the headers. */
    int i_pre_skip;
    /* Vorbis and Opus can trim the end of a stream using granule positions. */
    int i_end_trim;

    /* offset of first keyframe for theora; can be 0 or 1 depending on version number */
    int8_t i_keyframe_offset;

    /* keyframe index for seeking, created as we discover keyframes */
    demux_index_entry_t *idx;

    /* Skeleton data */
    ogg_skeleton_t *p_skel;

    /* skip some frames after a seek */
    unsigned int i_skip_frames;

    /* data start offset (absolute) in bytes */
    int64_t i_data_start;

    /* for Annodex logical bitstreams */
    int i_secondary_header_packets;

    /* All blocks which can't be sent because track PCR isn't known yet */
    struct
    {
        block_t **pp_blocks;
        uint8_t i_size; /* max 255 */
        uint8_t i_used;
    } prepcr;
    /* All blocks that are queued because ES isn't created yet */
    block_t *p_preparse_block;

    union
    {
#ifdef HAVE_LIBVORBIS
        struct
        {
            vorbis_info *p_info;
            vorbis_comment *p_comment;
            int i_headers_flags;
            int i_prev_blocksize;
        } vorbis;
#endif
        struct
        {
            /* kate streams have the number of headers in the ID header */
            int i_num_headers;
        } kate;
        struct
        {
            bool b_interlaced;
        } dirac;
        struct
        {
            int32_t i_framesize;
            int32_t i_framesperpacket;
        } speex;
        struct
        {
            bool b_old;
        } flac;
    } special;

} logical_stream_t;

struct ogg_skeleton_t
{
    int            i_messages;
    char         **ppsz_messages;
    unsigned char *p_index;
    uint64_t       i_index;
    uint64_t       i_index_size;
    int64_t        i_indexstampden;/* time denominator */
    int64_t        i_indexfirstnum;/* first sample time numerator */
    int64_t        i_indexlastnum;
};

struct demux_sys_t
{
    ogg_sync_state oy;        /* sync and verify incoming physical bitstream */

    int i_streams;                           /* number of logical bitstreams */
    logical_stream_t **pp_stream;  /* pointer to an array of logical streams */
    logical_stream_t *p_skelstream; /* pointer to skeleton stream if any */

    logical_stream_t *p_old_stream; /* pointer to a old logical stream to avoid recreating it */

    /* program clock reference (in units of 90kHz) derived from the pcr of
     * the sub-streams */
    mtime_t i_pcr;
    mtime_t i_nzpcr_offset;
    /* informative only */
    mtime_t i_pcr_jitter;
    int64_t i_access_delay;

    /* new stream or starting from a chain */
    bool b_chained_boundary;

    /* bitrate */
    int     i_bitrate;
    bool    b_partial_bitrate;

    /* after reading all headers, the first data page is stuffed into the relevant stream, ready to use */
    bool    b_page_waiting;

    /* count of total frames in video stream */
    int64_t i_total_frames;

    /* length of file in bytes */
    int64_t i_total_length;

    /* offset position in file (for reading) */
    int64_t i_input_position;

    /* current page being parsed */
    ogg_page current_page;

    /* */
    vlc_meta_t          *p_meta;
    int                 i_seekpoints;
    seekpoint_t         **pp_seekpoints;

    /* skeleton */
    struct
    {
        uint16_t major;
        uint16_t minor;
    } skeleton;

    /* */
    int                 i_attachments;
    input_attachment_t  **attachments;

    /* preparsing info */
    bool b_preparsing_done;
    bool b_es_created;

    /* Length, if available. */
    int64_t i_length;

    bool b_slave;

};


unsigned const char * Read7BitsVariableLE( unsigned const char *,
                                           unsigned const char *,
                                           uint64_t * );
bool Ogg_GetBoundsUsingSkeletonIndex( logical_stream_t *p_stream, int64_t i_time,
                                      int64_t *pi_lower, int64_t *pi_upper );
