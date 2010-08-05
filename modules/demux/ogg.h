/*****************************************************************************
 * ogg.h : ogg stream demux module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2010 the VideoLAN team
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *          Andre Pang <Andre.Pang@csiro.au> (Annodex support)
 *          Gabriel Finch <salsaman@gmail.com> (moved from ogg.c to ogg.h)
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




/*****************************************************************************
 * Definitions of structures and functions used by this plugins
 *****************************************************************************/


typedef struct oggseek_index_entry demux_index_entry_t;


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
    int              b_force_backup;
    int              i_packets_backup;
    void             *p_headers;
    int              i_headers;

    /* program clock reference (in units of 90kHz) derived from the previous
     * granulepos */
    mtime_t          i_pcr;
    mtime_t          i_interpolated_pcr;
    mtime_t          i_previous_pcr;

    /* Misc */
    bool b_reinit;
    int i_granule_shift;

    /* offset of first keyframe for theora; can be 0 or 1 depending on version number */
    int64_t i_keyframe_offset;

    /* keyframe index for seeking, created as we discover keyframes */
    demux_index_entry_t *idx;

    /* skip some frames after a seek */
    int i_skip_frames;

    /* data start offset (absolute) in bytes */
    int64_t i_data_start;

    /* kate streams have the number of headers in the ID header */
    int i_kate_num_headers;

    /* for Annodex logical bitstreams */
    int i_secondary_header_packets;

} logical_stream_t;






struct demux_sys_t
{
    ogg_sync_state oy;        /* sync and verify incoming physical bitstream */

    int i_streams;                           /* number of logical bitstreams */
    logical_stream_t **pp_stream;  /* pointer to an array of logical streams */

    logical_stream_t *p_old_stream; /* pointer to a old logical stream to avoid recreating it */

    /* program clock reference (in units of 90kHz) derived from the pcr of
     * the sub-streams */
    mtime_t i_pcr;

    /* stream state */
    int     i_bos;
    int     i_eos;

    /* bitrate */
    int     i_bitrate;

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

    mtime_t i_st_pts;


    /* */
    vlc_meta_t *p_meta;
};
