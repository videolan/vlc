/*****************************************************************************
 * mp4.h : MP4 file input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: mp4.h,v 1.10 2004/01/05 12:37:52 jlj Exp $
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
typedef struct chunk_data_mp4_s
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

} chunk_data_mp4_t;


/*****************************************************************************
 * Contain all needed information for read all track with vlc
 *****************************************************************************/
typedef struct track_data_mp4_s
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
    uint64_t         i_timescale;  /* time scale for this track only */

    /* give the next sample to read, i_chunk is to find quickly where
      the sample is located */
    uint32_t         i_sample;       /* next sample to read */
    uint32_t         i_chunk;        /* chunk where next sample is stored */
    /* total count of chunk and sample */
    uint32_t         i_chunk_count;
    uint32_t         i_sample_count;

    chunk_data_mp4_t    *chunk; /* always defined  for each chunk */

    /* sample size, p_sample_size defined only if i_sample_size == 0
        else i_sample_size is size for all sample */
    uint32_t         i_sample_size;
    uint32_t         *p_sample_size; /* XXX perhaps add file offset if take
                                    too much time to do sumations each time*/

    MP4_Box_t *p_stbl;  /* will contain all timing information */
    MP4_Box_t *p_stsd;  /* will contain all data to initialize decoder */
    MP4_Box_t *p_sample;/* point on actual sdsd */

    void *p_drms;

} track_data_mp4_t;


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
    track_data_mp4_t *track;    /* array of track */
};

#if 0
static inline uint64_t MP4_GetTrackPos( track_data_mp4_t *p_track )
{
    unsigned int i_sample;
    uint64_t i_pos;


    i_pos = p_track->chunk[p_track->i_chunk].i_offset;

    if( p_track->i_sample_size )
    {
        i_pos += ( p_track->i_sample -
                        p_track->chunk[p_track->i_chunk].i_sample_first ) *
                                p_track->i_sample_size;
    }
    else
    {
        for( i_sample = p_track->chunk[p_track->i_chunk].i_sample_first;
                i_sample < p_track->i_sample; i_sample++ )
        {
            i_pos += p_track->p_sample_size[i_sample];
        }

    }
    return( i_pos );
}
#endif

/* Return time in µs of a track */
static inline mtime_t MP4_GetTrackPTS( track_data_mp4_t *p_track )
{
    unsigned int i_sample;
    unsigned int i_index;
    uint64_t i_dts;

    i_sample = p_track->i_sample - p_track->chunk[p_track->i_chunk].i_sample_first;
    i_dts = p_track->chunk[p_track->i_chunk].i_first_dts;
    i_index = 0;
    while( i_sample > 0 )
    {
        if( i_sample > p_track->chunk[p_track->i_chunk].p_sample_count_dts[i_index] )
        {
            i_dts += p_track->chunk[p_track->i_chunk].p_sample_count_dts[i_index] *
                        p_track->chunk[p_track->i_chunk].p_sample_delta_dts[i_index];
            i_sample -= p_track->chunk[p_track->i_chunk].p_sample_count_dts[i_index];
            i_index++;
        }
        else
        {
            i_dts += i_sample *
                        p_track->chunk[p_track->i_chunk].p_sample_delta_dts[i_index];
            i_sample = 0;
            break;
        }
    }
    return( (mtime_t)(
                (mtime_t)1000000 *
                (mtime_t)i_dts /
                (mtime_t)p_track->i_timescale ) );
}

static inline mtime_t MP4_GetMoviePTS(demux_sys_t *p_demux )
{
    return( (mtime_t)(
                (mtime_t)1000000 *
                (mtime_t)p_demux->i_time /
                (mtime_t)p_demux->i_timescale )
          );
}
