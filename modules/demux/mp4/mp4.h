/*****************************************************************************
 * mp4.h : MP4 file input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2004, 2010, 2014 VLC authors and VideoLAN
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
 *****************************************************************************/
#ifndef VLC_MP4_MP4_H_
#define VLC_MP4_MP4_H_

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <vlc_common.h>
#include "libmp4.h"
#include "fragments.h"
#include "../asf/asfpacket.h"

/* Contain all information about a chunk */
typedef struct
{
    uint64_t     i_offset; /* absolute position of this chunk in the file */
    uint32_t     i_sample_description_index; /* index for SampleEntry to use */
    uint32_t     i_sample_count; /* how many samples in this chunk */
    uint32_t     i_sample_first; /* index of the first sample in this chunk */
    uint32_t     i_sample; /* index of the next sample to read in this chunk */
    uint32_t     i_virtual_run_number; /* chunks interleaving sequence */

    /* now provide way to calculate pts, dts, and offset without too
        much memory and with fast access */

    /* with this we can calculate dts/pts without waste memory */
    uint64_t     i_first_dts;   /* DTS of the first sample */
    uint64_t     i_duration;    /* total duration of all samples */

    uint32_t     i_entries_dts;
    uint32_t     *p_sample_count_dts;
    uint32_t     *p_sample_delta_dts;   /* dts delta */

    uint32_t     i_entries_pts;
    uint32_t     *p_sample_count_pts;
    int32_t      *p_sample_offset_pts;  /* pts-dts */

    uint32_t     *p_sample_size;
    /* TODO if needed add pts
        but quickly *add* support for edts and seeking */

} mp4_chunk_t;

typedef struct
{
    uint64_t i_offset;
    stime_t  i_first_dts;
    const MP4_Box_t *p_trun;
} mp4_run_t;

typedef enum RTP_timstamp_synchronization_s
{
    UNKNOWN_SYNC = 0, UNSYNCHRONIZED = 1, SYNCHRONIZED = 2, RESERVED = 3
} RTP_timstamp_synchronization_t;

enum
{
    USEAS_NONE = 0,
    USEAS_CHAPTERS = 1 << 0,
    USEAS_TIMECODE = 1 << 1,
};

typedef struct
{
    uint32_t i_timescale_override;
    uint32_t i_sample_size_override;
    const MP4_Box_t *p_asf;
    uint8_t     rgi_chans_reordering[AOUT_CHAN_MAX];
    bool        b_chans_reorder;

    bool b_forced_spu; /* forced track selection (never done by default/priority) */

    uint32_t    i_block_flags;
} track_config_t;

 /* Contain all needed information for read all track with vlc */
typedef struct
{
    unsigned int i_track_ID;/* this should be unique */

    int b_ok;               /* The track is usable */
    int b_enable;           /* is the trak enable by default */
    bool b_selected;  /* is the trak being played */
    int i_use_flags;  /* !=0 Set when track is referenced by specific reference types.
                         You'll need to lookup other tracks tref to know the ref source */
    bool b_forced_spu; /* forced track selection (never done by default/priority) */
    uint32_t i_switch_group;

    bool b_mac_encoding;

    es_format_t fmt;
    uint32_t    i_block_flags;
    uint32_t    i_next_block_flags;
    uint8_t     rgi_chans_reordering[AOUT_CHAN_MAX];
    bool        b_chans_reorder;
    es_out_id_t *p_es;

    /* display size only ! */
    int i_width;
    int i_height;
    float f_rotation;
    int i_flip;

    /* more internal data */
    uint32_t        i_timescale;    /* time scale for this track only */

    /* elst */
    int             i_elst;         /* current elst */
    int64_t         i_elst_time;    /* current elst start time (in movie time scale)*/
    const MP4_Box_t *p_elst;        /* elst (could be NULL) */

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
//                                    too much time to do sumations each time*/

    uint32_t     i_sample_first; /* i_sample_first value
                                                   of the next chunk */
    uint64_t     i_first_dts;    /* i_first_dts value
                                                   of the next chunk */

    const MP4_Box_t *p_track;
    const MP4_Box_t *p_stbl;  /* will contain all timing information */
    const MP4_Box_t *p_stsd;  /* will contain all data to initialize decoder */
    const MP4_Box_t *p_sample;/* point on actual sdsd */

#if 0
    bool b_codec_need_restart;
#endif

    stime_t i_time; // track scaled

    struct
    {
        /* for moof parsing */
        bool b_resync_time_offset;

        /* tfhd defaults */
        uint32_t i_default_sample_size;
        uint32_t i_default_sample_duration;

        struct
        {
            mp4_run_t *p_array;
            uint32_t   i_current;
            uint32_t   i_count;
        } runs;
        uint64_t i_trun_sample;
        uint64_t i_trun_sample_pos;

        int i_temp;
    } context;

    /* ASF packets handling */
    const MP4_Box_t *p_asf;
    vlc_tick_t       i_dts_backup;
    vlc_tick_t       i_pts_backup;
    asf_track_info_t asfinfo;
} mp4_track_t;

int SetupVideoES( demux_t *p_demux, const mp4_track_t *p_track,
                  const MP4_Box_t *p_sample, es_format_t *, track_config_t *);
int SetupAudioES( demux_t *p_demux, const mp4_track_t *p_track,
                  const MP4_Box_t *p_sample, es_format_t *, track_config_t * );
int SetupSpuES( demux_t *p_demux, const mp4_track_t *p_track,
                const MP4_Box_t *p_sample, es_format_t *, track_config_t * );
void SetupMeta( vlc_meta_t *p_meta, const MP4_Box_t *p_udta );

/* format of RTP reception hint track sample constructor */
typedef struct
{
    uint8_t  type;
    int8_t   trackrefindex;
    uint16_t length;
    uint32_t samplenumber;
    uint32_t sampleoffset; /* indicates where the payload is located within sample */
    uint16_t bytesperblock;
    uint16_t samplesperblock;

} mp4_rtpsampleconstructor_t;

#endif
