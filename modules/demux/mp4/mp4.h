/*****************************************************************************
 * mp4.h : MP4 file input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2004 VideoLAN
 * $Id: mp4.h,v 1.14 2004/01/25 20:05:28 hartman Exp $
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
 * Contain all information about a chunk
 *****************************************************************************/
typedef struct
{
    uint64_t     i_offset; /* absolute position of this chunk in the file */
    uint32_t     i_sample_description_index; /* index for SampleEntry to use */
    uint32_t     i_sample_count; /* how many samples in this chunk */
    uint32_t     i_sample_first; /* index of the first sample in this chunk */

    /* now provide way to calculate pts, dts, and offset without to
        much memory and with fast acces */

    /* with this we can calculate dts/pts without waste memory */
    uint64_t     i_first_dts;
    uint32_t     *p_sample_count_dts;
    uint32_t     *p_sample_delta_dts; /* dts delta */

    /* TODO if needed add pts
        but quickly *add* support for edts and seeking */

} mp4_chunk_t;


/*****************************************************************************
 * Contain all needed information for read all track with vlc
 *****************************************************************************/
typedef struct
{
    int i_track_ID;     /* this should be unique */

    int b_ok;           /* The track is usable */
    int b_enable;       /* is the trak enable by default */
    vlc_bool_t b_selected;     /* is the trak being played */

    es_format_t fmt;
    es_out_id_t *p_es;

    /* display size only ! */
    int i_width;
    int i_height;

    /* more internal data */
    uint64_t        i_timescale;    /* time scale for this track only */

    /* elst */
    int             i_elst;         /* current elst */
    int64_t         i_elst_time;    /* current elst start time (in movie time scale)*/
    MP4_Box_t       *p_elst;        /* elst (could be NULL) */

    /* give the next sample to read, i_chunk is to find quickly where
      the sample is located */
    uint32_t         i_sample;       /* next sample to read */
    uint32_t         i_chunk;        /* chunk where next sample is stored */
    /* total count of chunk and sample */
    uint32_t         i_chunk_count;
    uint32_t         i_sample_count;

    mp4_chunk_t    *chunk; /* always defined  for each chunk */

    /* sample size, p_sample_size defined only if i_sample_size == 0
        else i_sample_size is size for all sample */
    uint32_t         i_sample_size;
    uint32_t         *p_sample_size; /* XXX perhaps add file offset if take
                                    too much time to do sumations each time*/

    MP4_Box_t *p_stbl;  /* will contain all timing information */
    MP4_Box_t *p_stsd;  /* will contain all data to initialize decoder */
    MP4_Box_t *p_sample;/* point on actual sdsd */

    vlc_bool_t b_drms;
    void      *p_drms;

} mp4_track_t;


/*****************************************************************************
 *
 *****************************************************************************/
struct demux_sys_t
{
    MP4_Box_t    *p_root;      /* container for the whole file */

    mtime_t      i_pcr;

    uint64_t     i_time;        /* time position of the presentation
                                 * in movie timescale */
    uint64_t     i_timescale;   /* movie time scale */
    uint64_t     i_duration;    /* movie duration */
    unsigned int i_tracks;      /* number of tracks */
    mp4_track_t *track;    /* array of track */
};


