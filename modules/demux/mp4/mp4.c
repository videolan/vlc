/*****************************************************************************
 * mp4.c : MP4 file input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2004, 2010 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "mp4.h"

#include <vlc_demux.h>
#include <vlc_charset.h>                           /* EnsureUTF8 */
#include <vlc_input.h>
#include <vlc_aout.h>
#include <vlc_plugin.h>
#include <vlc_dialog.h>
#include <assert.h>
#include <limits.h>
#include "../codec/cc.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define CFG_PREFIX "mp4-"

#define MP4_M4A_TEXT     N_("M4A audio only")
#define MP4_M4A_LONGTEXT N_("Ignore non audio tracks from iTunes audio files")

vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_description( N_("MP4 stream demuxer") )
    set_shortname( N_("MP4") )
    set_capability( "demux", 240 )
    set_callbacks( Open, Close )

    add_category_hint("Hacks", NULL, true)
    add_bool( CFG_PREFIX"m4a-audioonly", false, MP4_M4A_TEXT, MP4_M4A_LONGTEXT, true )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int   Demux   ( demux_t * );
static int   DemuxRef( demux_t *p_demux ){ (void)p_demux; return 0;}
static int   DemuxFrag( demux_t * );
static int   Control ( demux_t *, int, va_list );

struct demux_sys_t
{
    MP4_Box_t    *p_root;      /* container for the whole file */

    mtime_t      i_pcr;

    uint64_t     i_moov_duration;
    uint64_t     i_duration;           /* Declared fragmented duration (movie time scale) */
    uint64_t     i_cumulated_duration; /* Same as above, but not from probing, (movie time scale) */
    uint32_t     i_timescale;          /* movie time scale */
    uint64_t     i_nztime;             /* time position of the presentation (CLOCK_FREQ timescale) */
    unsigned int i_tracks;       /* number of tracks */
    mp4_track_t  *track;         /* array of track */
    float        f_fps;          /* number of frame per seconds */

    bool         b_fragmented;   /* fMP4 */
    bool         b_seekable;
    bool         b_fastseekable;
    bool         b_error;        /* unrecoverable */

    bool            b_index_probed;     /* mFra sync points index */
    bool            b_fragments_probed; /* moof segments index created */

    MP4_Box_t *p_moov;

    struct
    {
        uint32_t        i_current_box_type;
        MP4_Box_t      *p_fragment_atom;
        uint64_t        i_post_mdat_offset;
        uint32_t        i_lastseqnumber;
    } context;

    /* */
    MP4_Box_t    *p_tref_chap;

    /* */
    input_title_t *p_title;
    vlc_meta_t    *p_meta;

    /* ASF in MP4 */
    asf_packet_sys_t asfpacketsys;
    uint64_t i_preroll;         /* foobar */
    int64_t  i_preroll_start;

    struct
    {
        int es_cat_filters;
    } hacks;

    mp4_fragments_index_t *p_fragsindex;
};

#define DEMUX_INCREMENT (CLOCK_FREQ / 4) /* How far the pcr will go, each round */
#define DEMUX_TRACK_MAX_PRELOAD (CLOCK_FREQ * 15) /* maximum preloading, to deal with interleaving */

#define VLC_DEMUXER_EOS (VLC_DEMUXER_EGENERIC - 1)
#define VLC_DEMUXER_FATAL (VLC_DEMUXER_EGENERIC - 2)

const uint32_t rgi_pict_atoms[2] = { ATOM_PICT, ATOM_pict };
const char *psz_meta_roots[] = { "/moov/udta/meta/ilst",
                                 "/moov/meta/ilst",
                                 "/moov/udta/meta",
                                 "/moov/udta",
                                 "/meta/ilst",
                                 "/udta",
                                 NULL };

/*****************************************************************************
 * Declaration of local function
 *****************************************************************************/
static void MP4_TrackSetup( demux_t *, mp4_track_t *, MP4_Box_t  *, bool, bool );
static void MP4_TrackInit( mp4_track_t * );
static void MP4_TrackClean( es_out_t *, mp4_track_t * );

static void MP4_Block_Send( demux_t *, mp4_track_t *, block_t * );

static void MP4_TrackSelect  ( demux_t *, mp4_track_t *, bool );
static int  MP4_TrackSeek   ( demux_t *, mp4_track_t *, mtime_t );

static uint64_t MP4_TrackGetPos    ( mp4_track_t * );
static uint32_t MP4_TrackGetReadSize( mp4_track_t *, uint32_t * );
static int      MP4_TrackNextSample( demux_t *, mp4_track_t *, uint32_t );
static void     MP4_TrackSetELST( demux_t *, mp4_track_t *, int64_t );

static void     MP4_UpdateSeekpoint( demux_t *, int64_t );

static MP4_Box_t * MP4_GetTrexByTrackID( MP4_Box_t *p_moov, const uint32_t i_id );
static void MP4_GetDefaultSizeAndDuration( MP4_Box_t *p_moov,
                                           const MP4_Box_data_tfhd_t *p_tfhd_data,
                                           uint32_t *pi_default_size,
                                           uint32_t *pi_default_duration );

static stime_t GetMoovTrackDuration( demux_sys_t *p_sys, unsigned i_track_ID );

static int  ProbeFragments( demux_t *p_demux, bool b_force, bool *pb_fragmented );
static int  ProbeIndex( demux_t *p_demux );

static int FragCreateTrunIndex( demux_t *, MP4_Box_t *, MP4_Box_t *, stime_t, bool );

static int FragGetMoofBySidxIndex( demux_t *p_demux, mtime_t i_target_time,
                                   uint64_t *pi_moof_pos, mtime_t *pi_sampletime );
static int FragGetMoofByTfraIndex( demux_t *p_demux, const mtime_t i_target_time, unsigned i_track_ID,
                                   uint64_t *pi_moof_pos, mtime_t *pi_sampletime );
static void FragResetContext( demux_sys_t * );

/* ASF Handlers */
static asf_track_info_t * MP4ASF_GetTrackInfo( asf_packet_sys_t *p_packetsys, uint8_t i_stream_number );
static void MP4ASF_Send(asf_packet_sys_t *p_packetsys, uint8_t i_stream_number, block_t **pp_frame);
static void MP4ASF_ResetFrames( demux_sys_t *p_sys );

/* RTP Hint track */
static block_t * MP4_RTPHint_Convert( demux_t *p_demux, block_t *p_block, vlc_fourcc_t i_codec );
static block_t * MP4_RTPHintToFrame( demux_t *p_demux, block_t *p_block, uint32_t packetcount );

static int MP4_LoadMeta( demux_sys_t *p_sys, vlc_meta_t *p_meta );

/* Helpers */

static int64_t MP4_rescale( int64_t i_value, uint32_t i_timescale, uint32_t i_newscale )
{
    if( i_timescale == i_newscale )
        return i_value;

    if( i_value <= INT64_MAX / i_newscale )
        return i_value * i_newscale / i_timescale;

    /* overflow */
    int64_t q = i_value / i_timescale;
    int64_t r = i_value % i_timescale;
    return q * i_newscale + r * i_newscale / i_timescale;
}

static uint32_t stream_ReadU32( stream_t *s, void *p_read, uint32_t i_toread )
{
    ssize_t i_return = 0;
    if ( i_toread > INT32_MAX )
    {
        i_return = vlc_stream_Read( s, p_read, (size_t) INT32_MAX );
        if ( i_return < INT32_MAX )
            return i_return;
        else
            i_toread -= INT32_MAX;
    }
    i_return += vlc_stream_Read( s, (uint8_t *)p_read + i_return, (size_t) i_toread );
    return i_return;
}

static MP4_Box_t * MP4_GetTrexByTrackID( MP4_Box_t *p_moov, const uint32_t i_id )
{
    if(!p_moov)
        return NULL;
    MP4_Box_t *p_trex = MP4_BoxGet( p_moov, "mvex/trex" );
    while( p_trex )
    {
        if ( p_trex->i_type == ATOM_trex &&
             BOXDATA(p_trex) && BOXDATA(p_trex)->i_track_ID == i_id )
                break;
        else
            p_trex = p_trex->p_next;
    }
    return p_trex;
}

/**
 * Return the track identified by tid
 */
static mp4_track_t *MP4_GetTrackByTrackID( demux_t *p_demux, const uint32_t tid )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    mp4_track_t *ret = NULL;
    for( unsigned i = 0; i < p_sys->i_tracks; i++ )
    {
        ret = &p_sys->track[i];
        if( ret->i_track_ID == tid )
            return ret;
    }
    return NULL;
}

static MP4_Box_t * MP4_GetTrakByTrackID( MP4_Box_t *p_moov, const uint32_t i_id )
{
    MP4_Box_t *p_trak = MP4_BoxGet( p_moov, "trak" );
    MP4_Box_t *p_tkhd;
    while( p_trak )
    {
        if( p_trak->i_type == ATOM_trak &&
            (p_tkhd = MP4_BoxGet( p_trak, "tkhd" )) && BOXDATA(p_tkhd) &&
            BOXDATA(p_tkhd)->i_track_ID == i_id )
                break;
        else
            p_trak = p_trak->p_next;
    }
    return p_trak;
}

static MP4_Box_t * MP4_GetTrafByTrackID( MP4_Box_t *p_moof, const uint32_t i_id )
{
    MP4_Box_t *p_traf = MP4_BoxGet( p_moof, "traf" );
    MP4_Box_t *p_tfhd;
    while( p_traf )
    {
        if( p_traf->i_type == ATOM_traf &&
            (p_tfhd = MP4_BoxGet( p_traf, "tfhd" )) && BOXDATA(p_tfhd) &&
            BOXDATA(p_tfhd)->i_track_ID == i_id )
                break;
        else
            p_traf = p_traf->p_next;
    }
    return p_traf;
}

static es_out_id_t * MP4_AddTrackES( es_out_t *out, mp4_track_t *p_track )
{
    es_out_id_t *p_es = es_out_Add( out, &p_track->fmt );
    /* Force SPU which isn't selected/defaulted */
    if( p_track->fmt.i_cat == SPU_ES && p_es && p_track->b_forced_spu )
        es_out_Control( out, ES_OUT_SET_ES_DEFAULT, p_es );

    return p_es;
}

/* Return time in microsecond of a track */
static inline int64_t MP4_TrackGetDTS( demux_t *p_demux, mp4_track_t *p_track )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const mp4_chunk_t *p_chunk = &p_track->chunk[p_track->i_chunk];

    unsigned int i_index = 0;
    unsigned int i_sample = p_track->i_sample - p_chunk->i_sample_first;
    int64_t i_dts = p_chunk->i_first_dts;

    while( i_sample > 0 && i_index < p_chunk->i_entries_dts )
    {
        if( i_sample > p_chunk->p_sample_count_dts[i_index] )
        {
            i_dts += p_chunk->p_sample_count_dts[i_index] *
                p_chunk->p_sample_delta_dts[i_index];
            i_sample -= p_chunk->p_sample_count_dts[i_index];
            i_index++;
        }
        else
        {
            i_dts += i_sample * p_chunk->p_sample_delta_dts[i_index];
            break;
        }
    }

    i_dts = MP4_rescale( i_dts, p_track->i_timescale, CLOCK_FREQ );

    /* now handle elst */
    if( p_track->p_elst && p_track->BOXDATA(p_elst)->i_entry_count )
    {
        MP4_Box_data_elst_t *elst = p_track->BOXDATA(p_elst);

        /* convert to offset */
        if( ( elst->i_media_rate_integer[p_track->i_elst] > 0 ||
              elst->i_media_rate_fraction[p_track->i_elst] > 0 ) &&
            elst->i_media_time[p_track->i_elst] > 0 )
        {
            i_dts -= MP4_rescale( elst->i_media_time[p_track->i_elst], p_track->i_timescale, CLOCK_FREQ );
        }

        /* add i_elst_time */
        i_dts += MP4_rescale( p_track->i_elst_time, p_sys->i_timescale, CLOCK_FREQ );

        if( i_dts < 0 ) i_dts = 0;
    }

    return i_dts;
}

static inline bool MP4_TrackGetPTSDelta( demux_t *p_demux, mp4_track_t *p_track,
                                         int64_t *pi_delta )
{
    VLC_UNUSED( p_demux );
    mp4_chunk_t *ck = &p_track->chunk[p_track->i_chunk];

    unsigned int i_index = 0;
    unsigned int i_sample = p_track->i_sample - ck->i_sample_first;

    if( ck->p_sample_count_pts == NULL || ck->p_sample_offset_pts == NULL )
        return false;

    for( i_index = 0; i_index < ck->i_entries_pts ; i_index++ )
    {
        if( i_sample < ck->p_sample_count_pts[i_index] )
        {
            *pi_delta = MP4_rescale( ck->p_sample_offset_pts[i_index],
                                     p_track->i_timescale, CLOCK_FREQ );
            return true;
        }

        i_sample -= ck->p_sample_count_pts[i_index];
    }
    return false;
}

static inline int64_t MP4_GetMoviePTS(demux_sys_t *p_sys )
{
    return p_sys->i_nztime;
}

static void LoadChapter( demux_t  *p_demux );

static int LoadInitFrag( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    /* Load all boxes ( except raw data ) */
    if( ( p_sys->p_root = MP4_BoxGetRoot( p_demux->s ) ) == NULL )
    {
        goto LoadInitFragError;
    }

    return VLC_SUCCESS;

LoadInitFragError:
    msg_Warn( p_demux, "MP4 plugin discarded (not a valid initialization chunk)" );
    return VLC_EGENERIC;
}

static int CreateTracks( demux_t *p_demux, unsigned i_tracks )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( SIZE_MAX / i_tracks < sizeof(mp4_track_t) )
        return VLC_EGENERIC;

    p_sys->track = vlc_alloc( i_tracks, sizeof(mp4_track_t)  );
    if( p_sys->track == NULL )
        return VLC_ENOMEM;
    p_sys->i_tracks = i_tracks;

    for( unsigned i=0; i<i_tracks; i++ )
        MP4_TrackInit( &p_sys->track[i] );

    return VLC_SUCCESS;
}

static block_t * MP4_EIA608_Convert( block_t * p_block )
{
    /* Rebuild codec data from encap */
    size_t i_copied = 0;
    size_t i_remaining = __MIN(p_block->i_buffer, INT64_MAX / 3);
    uint32_t i_bytes = 0;
    block_t *p_newblock;

    /* always need at least 10 bytes (atom size+header+1pair)*/
    if ( i_remaining < 10 ||
         !(i_bytes = GetDWBE(p_block->p_buffer)) ||
         (i_bytes > i_remaining) ||
         memcmp("cdat", &p_block->p_buffer[4], 4) ||
         !(p_newblock = block_Alloc( i_remaining * 3 - 8 )) )
    {
        p_block->i_buffer = 0;
        return p_block;
    }

    uint8_t *p_write = p_newblock->p_buffer;
    uint8_t *p_read = &p_block->p_buffer[8];
    i_bytes -= 8;
    i_remaining -= 8;

    do
    {
        p_write[i_copied++] = CC_PKT_BYTE0(0); /* cc1 == field 0 */
        p_write[i_copied++] = p_read[0];
        p_write[i_copied++] = p_read[1];
        p_read += 2;
        i_bytes -= 2;
        i_remaining -= 2;
    } while( i_bytes >= 2 );

    /* cdt2 is optional */
    if ( i_remaining >= 10 &&
         (i_bytes = GetDWBE(p_read)) &&
         (i_bytes <= i_remaining) &&
         !memcmp("cdt2", &p_read[4], 4) )
    {
        p_read += 8;
        i_bytes -= 8;
        i_remaining -= 8;
        do
        {
            p_write[i_copied++] = CC_PKT_BYTE0(0); /* cc1 == field 0 */
            p_write[i_copied++] = p_read[0];
            p_write[i_copied++] = p_read[1];
            p_read += 2;
            i_bytes -= 2;
        } while( i_bytes >= 2 );
    }

    p_newblock->i_pts = p_block->i_dts;
    p_newblock->i_buffer = i_copied;
    p_newblock->i_flags = BLOCK_FLAG_TYPE_P;
    block_Release( p_block );

    return p_newblock;
}

static uint32_t MP4_TrackGetRunSeq( mp4_track_t *p_track )
{
    if( p_track->i_chunk_count > 0 )
        return p_track->chunk[p_track->i_chunk].i_virtual_run_number;
    return 0;
}

/* Analyzes chunks to find max interleave length
 * sets flat flag if no interleaving is in use */
static void MP4_GetInterleaving( demux_t *p_demux, uint64_t *pi_max_contiguous, bool *pb_flat )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    *pi_max_contiguous = 0;
    *pb_flat = true;

    /* Find first recorded chunk */
    mp4_track_t *tk = NULL;
    uint64_t i_duration = 0;
    for( unsigned i=0; i < p_sys->i_tracks; i++ )
    {
        mp4_track_t *cur = &p_sys->track[i];
        if( !cur->i_chunk_count )
            continue;

        if( tk == NULL || cur->chunk[0].i_offset < tk->chunk[0].i_offset )
            tk = cur;
    }

    for( ; tk != NULL; )
    {
        i_duration += tk->chunk[tk->i_chunk].i_duration;
        tk->i_chunk++;

        /* Find next chunk in data order */
        mp4_track_t *nexttk = NULL;
        for( unsigned i=0; i < p_sys->i_tracks; i++ )
        {
            mp4_track_t *cur = &p_sys->track[i];
            if( cur->i_chunk == cur->i_chunk_count )
                continue;

            if( nexttk == NULL ||
                cur->chunk[cur->i_chunk].i_offset < nexttk->chunk[nexttk->i_chunk].i_offset )
                nexttk = cur;
        }

        /* copy previous run */
        if( nexttk && nexttk->i_chunk > 0 )
            nexttk->chunk[nexttk->i_chunk].i_virtual_run_number =
                    nexttk->chunk[nexttk->i_chunk - 1].i_virtual_run_number;

        if( tk != nexttk )
        {
            i_duration = MP4_rescale( i_duration, tk->i_timescale, CLOCK_FREQ );
            if( i_duration > *pi_max_contiguous )
                *pi_max_contiguous = i_duration;
            i_duration = 0;

            if( tk->i_chunk != tk->i_chunk_count )
                *pb_flat = false;

            if( nexttk && nexttk->i_chunk > 0 ) /* new run number */
                nexttk->chunk[nexttk->i_chunk].i_virtual_run_number++;
        }

        tk = nexttk;
    }

    /* reset */
    for( unsigned i=0; i < p_sys->i_tracks; i++ )
        p_sys->track[i].i_chunk = 0;
}

static block_t * MP4_Block_Convert( demux_t *p_demux, const mp4_track_t *p_track, block_t *p_block )
{
    /* might have some encap */
    if( p_track->fmt.i_cat == SPU_ES )
    {
        switch( p_track->fmt.i_codec )
        {
            case VLC_CODEC_WEBVTT:
            case VLC_CODEC_TTML:
            case VLC_CODEC_TX3G:
            case VLC_CODEC_SPU:
            case VLC_CODEC_SUBT:
            /* accept as-is */
            break;
            case VLC_CODEC_CEA608:
                p_block = MP4_EIA608_Convert( p_block );
            break;
        default:
            p_block->i_buffer = 0;
            break;
        }
    }
    else if( p_track->fmt.i_original_fourcc == ATOM_rrtp )
    {
        p_block = MP4_RTPHint_Convert( p_demux, p_block, p_track->fmt.i_codec );
    }

    return p_block;
}

static void MP4_Block_Send( demux_t *p_demux, mp4_track_t *p_track, block_t *p_block )
{
    p_block = MP4_Block_Convert( p_demux, p_track, p_block );
    if( p_block == NULL )
        return;

    if ( p_track->b_chans_reorder )
    {
        aout_ChannelReorder( p_block->p_buffer, p_block->i_buffer,
                             p_track->fmt.audio.i_channels,
                             p_track->rgi_chans_reordering,
                             p_track->fmt.i_codec );
    }

    p_block->i_flags |= p_track->i_block_flags;
    if( p_track->i_next_block_flags )
    {
        p_block->i_flags |= p_track->i_next_block_flags;
        p_track->i_next_block_flags = 0;
    }

    /* ASF packets in mov */
    if( p_track->p_asf )
    {
        /* Fake a new stream from MP4 block */
        stream_t *p_stream = p_demux->s;
        p_demux->s = vlc_stream_MemoryNew( p_demux, p_block->p_buffer, p_block->i_buffer, true );
        if ( p_demux->s )
        {
            p_track->i_dts_backup = p_block->i_dts;
            p_track->i_pts_backup = p_block->i_pts;
            /* And demux it as ASF packet */
            DemuxASFPacket( &p_demux->p_sys->asfpacketsys, p_block->i_buffer, p_block->i_buffer );
            vlc_stream_Delete(p_demux->s);
        }
        block_Release(p_block);
        p_demux->s = p_stream;
    }
    else
        es_out_Send( p_demux->out, p_track->p_es, p_block );
}

/*****************************************************************************
 * Open: check file and initializes MP4 structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t  *p_demux = (demux_t *)p_this;
    demux_sys_t     *p_sys;

    const uint8_t   *p_peek;

    MP4_Box_t       *p_ftyp;
    const MP4_Box_t *p_mvhd = NULL;
    const MP4_Box_t *p_mvex = NULL;

    bool      b_enabled_es;

    /* A little test to see if it could be a mp4 */
    if( vlc_stream_Peek( p_demux->s, &p_peek, 11 ) < 11 ) return VLC_EGENERIC;

    switch( VLC_FOURCC( p_peek[4], p_peek[5], p_peek[6], p_peek[7] ) )
    {
        case ATOM_moov:
        case ATOM_foov:
        case ATOM_moof:
        case ATOM_mdat:
        case ATOM_udta:
        case ATOM_free:
        case ATOM_skip:
        case ATOM_wide:
        case ATOM_uuid:
        case VLC_FOURCC( 'p', 'n', 'o', 't' ):
            break;
        case ATOM_ftyp:
            /* We don't yet support f4v, but avformat does. */
            if( p_peek[8] == 'f' && p_peek[9] == '4' && p_peek[10] == 'v' )
                return VLC_EGENERIC;
            break;
         default:
            return VLC_EGENERIC;
    }

    /* create our structure that will contains all data */
    p_sys = calloc( 1, sizeof( demux_sys_t ) );
    if ( !p_sys )
        return VLC_EGENERIC;

    /* I need to seek */
    vlc_stream_Control( p_demux->s, STREAM_CAN_SEEK, &p_sys->b_seekable );
    if( p_sys->b_seekable )
        vlc_stream_Control( p_demux->s, STREAM_CAN_FASTSEEK, &p_sys->b_fastseekable );

    /*Set exported functions */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    p_sys->context.i_lastseqnumber = UINT32_MAX;

    p_demux->p_sys = p_sys;

    if( LoadInitFrag( p_demux ) != VLC_SUCCESS )
        goto error;

    MP4_BoxDumpStructure( p_demux->s, p_sys->p_root );

    if( ( p_ftyp = MP4_BoxGet( p_sys->p_root, "/ftyp" ) ) )
    {
        switch( BOXDATA(p_ftyp)->i_major_brand )
        {
            case MAJOR_isom:
                msg_Dbg( p_demux,
                         "ISO Media (isom) version %d.",
                         BOXDATA(p_ftyp)->i_minor_version );
                break;
            case MAJOR_3gp4:
            case MAJOR_3gp5:
            case MAJOR_3gp6:
            case MAJOR_3gp7:
                msg_Dbg( p_demux, "3GPP Media Release: %4.4s",
                         (char *)&BOXDATA(p_ftyp)->i_major_brand );
                break;
            case MAJOR_qt__:
                msg_Dbg( p_demux, "Apple QuickTime media" );
                break;
            case MAJOR_isml:
                msg_Dbg( p_demux, "PIFF (= isml = fMP4) media" );
                break;
            case MAJOR_dash:
                msg_Dbg( p_demux, "DASH Stream" );
                break;
            case MAJOR_M4A:
                msg_Dbg( p_demux, "iTunes audio" );
                if( var_InheritBool( p_demux, CFG_PREFIX"m4a-audioonly" ) )
                    p_sys->hacks.es_cat_filters = AUDIO_ES;
                break;
            default:
                msg_Dbg( p_demux,
                         "unrecognized major media specification (%4.4s).",
                          (char*)&BOXDATA(p_ftyp)->i_major_brand );
                break;
        }
        /* also lookup in compatibility list */
        for(uint32_t i=0; i<BOXDATA(p_ftyp)->i_compatible_brands_count; i++)
        {
            if (BOXDATA(p_ftyp)->i_compatible_brands[i] == MAJOR_dash)
            {
                msg_Dbg( p_demux, "DASH Stream" );
            }
            else if (BOXDATA(p_ftyp)->i_compatible_brands[i] == VLC_FOURCC('s', 'm', 'o', 'o') )
            {
                msg_Dbg( p_demux, "Handling VLC Smooth Stream" );
            }
        }
    }
    else
    {
        msg_Dbg( p_demux, "file type box missing (assuming ISO Media)" );
    }

    /* the file need to have one moov box */
    p_sys->p_moov = MP4_BoxGet( p_sys->p_root, "/moov" );
    if( unlikely(!p_sys->p_moov) )
    {
        p_sys->p_moov = MP4_BoxGet( p_sys->p_root, "/foov" );
        if( !p_sys->p_moov )
        {
            msg_Err( p_demux, "MP4 plugin discarded (no moov,foov,moof box)" );
            goto error;
        }
        /* we have a free box as a moov, rename it */
        p_sys->p_moov->i_type = ATOM_moov;
    }

    p_mvhd = MP4_BoxGet( p_sys->p_moov, "mvhd" );
    if( p_mvhd && BOXDATA(p_mvhd) && BOXDATA(p_mvhd)->i_timescale )
    {
        p_sys->i_timescale = BOXDATA(p_mvhd)->i_timescale;
        p_sys->i_moov_duration = p_sys->i_duration = BOXDATA(p_mvhd)->i_duration;
        p_sys->i_cumulated_duration = BOXDATA(p_mvhd)->i_duration;
    }
    else
    {
        msg_Warn( p_demux, "No valid mvhd found" );
        goto error;
    }

    MP4_Box_t *p_rmra = MP4_BoxGet( p_sys->p_root, "/moov/rmra" );
    if( p_rmra != NULL && p_demux->p_input != NULL )
    {
        int        i_count = MP4_BoxCount( p_rmra, "rmda" );
        int        i;

        msg_Dbg( p_demux, "detected playlist mov file (%d ref)", i_count );

        input_thread_t *p_input = p_demux->p_input;
        input_item_t *p_current = input_GetItem( p_input );

        input_item_node_t *p_subitems = input_item_node_Create( p_current );

        for( i = 0; i < i_count; i++ )
        {
            MP4_Box_t *p_rdrf = MP4_BoxGet( p_rmra, "rmda[%d]/rdrf", i );
            char      *psz_ref;
            uint32_t  i_ref_type;

            if( !p_rdrf || !BOXDATA(p_rdrf) || !( psz_ref = strdup( BOXDATA(p_rdrf)->psz_ref ) ) )
            {
                continue;
            }
            i_ref_type = BOXDATA(p_rdrf)->i_ref_type;

            msg_Dbg( p_demux, "new ref=`%s' type=%4.4s",
                     psz_ref, (char*)&i_ref_type );

            if( i_ref_type == VLC_FOURCC( 'u', 'r', 'l', ' ' ) )
            {
                if( strstr( psz_ref, "qt5gateQT" ) )
                {
                    msg_Dbg( p_demux, "ignoring pseudo ref =`%s'", psz_ref );
                    free( psz_ref );
                    continue;
                }
                if( !strncmp( psz_ref, "http://", 7 ) ||
                    !strncmp( psz_ref, "rtsp://", 7 ) )
                {
                    ;
                }
                else
                {
                    char *psz_absolute;
                    char *psz_path = strdup( p_demux->psz_location );
                    char *end = strrchr( psz_path, '/' );
                    if( end ) end[1] = '\0';
                    else *psz_path = '\0';

                    if( asprintf( &psz_absolute, "%s://%s%s",
                                  p_demux->psz_access, psz_path, psz_ref ) < 0 )
                    {
                        free( psz_ref );
                        free( psz_path );
                        input_item_node_Delete( p_subitems );
                        return VLC_ENOMEM;
                    }

                    free( psz_ref );
                    psz_ref = psz_absolute;
                    free( psz_path );
                }
                msg_Dbg( p_demux, "adding ref = `%s'", psz_ref );
                input_item_t *p_item = input_item_New( psz_ref, NULL );
                input_item_CopyOptions( p_item, p_current );
                input_item_node_AppendItem( p_subitems, p_item );
                input_item_Release( p_item );
            }
            else
            {
                msg_Err( p_demux, "unknown ref type=%4.4s FIXME (send a bug report)",
                         (char*)&BOXDATA(p_rdrf)->i_ref_type );
            }
            free( psz_ref );
        }

        /* FIXME: create a stream_filter sub-module for this */
        if (es_out_Control(p_demux->out, ES_OUT_POST_SUBNODE, p_subitems))
            input_item_node_Delete(p_subitems);
    }

    if( !(p_mvhd = MP4_BoxGet( p_sys->p_root, "/moov/mvhd" ) ) )
    {
        if( !p_rmra )
        {
            msg_Err( p_demux, "cannot find /moov/mvhd" );
            goto error;
        }
        else
        {
            msg_Warn( p_demux, "cannot find /moov/mvhd (pure ref file)" );
            p_demux->pf_demux = DemuxRef;
            return VLC_SUCCESS;
        }
    }
    else
    {
        p_sys->i_timescale = BOXDATA(p_mvhd)->i_timescale;
        if( p_sys->i_timescale == 0 )
        {
            msg_Err( p_this, "bad timescale" );
            goto error;
        }
    }

    const unsigned i_tracks = MP4_BoxCount( p_sys->p_root, "/moov/trak" );
    if( i_tracks < 1 )
    {
        msg_Err( p_demux, "cannot find any /moov/trak" );
        goto error;
    }
    msg_Dbg( p_demux, "found %u track%c", i_tracks, i_tracks ? 's':' ' );

    if( CreateTracks( p_demux, i_tracks ) != VLC_SUCCESS )
        goto error;

    /* Search the first chap reference (like quicktime) and
     * check that at least 1 stream is enabled */
    p_sys->p_tref_chap = NULL;
    b_enabled_es = false;
    for( unsigned i = 0; i < p_sys->i_tracks; i++ )
    {
        MP4_Box_t *p_trak = MP4_BoxGet( p_sys->p_root, "/moov/trak[%d]", i );


        MP4_Box_t *p_tkhd = MP4_BoxGet( p_trak, "tkhd" );
        if( p_tkhd && BOXDATA(p_tkhd) && (BOXDATA(p_tkhd)->i_flags&MP4_TRACK_ENABLED) )
            b_enabled_es = true;

        MP4_Box_t *p_chap = MP4_BoxGet( p_trak, "tref/chap", i );
        if( p_chap && p_chap->data.p_tref_generic &&
            p_chap->data.p_tref_generic->i_entry_count > 0 && !p_sys->p_tref_chap )
            p_sys->p_tref_chap = p_chap;
    }

    /* Set and store metadata */
    if( (p_sys->p_meta = vlc_meta_New()) )
        MP4_LoadMeta( p_sys, p_sys->p_meta );

    /* now process each track and extract all useful information */
    for( unsigned i = 0; i < p_sys->i_tracks; i++ )
    {
        MP4_Box_t *p_trak = MP4_BoxGet( p_sys->p_root, "/moov/trak[%u]", i );
        MP4_TrackSetup( p_demux, &p_sys->track[i], p_trak, true, !b_enabled_es );

        if( p_sys->track[i].b_ok && !p_sys->track[i].b_chapters_source )
        {
            const char *psz_cat;
            switch( p_sys->track[i].fmt.i_cat )
            {
                case( VIDEO_ES ):
                    psz_cat = "video";
                    break;
                case( AUDIO_ES ):
                    psz_cat = "audio";
                    break;
                case( SPU_ES ):
                    psz_cat = "subtitle";
                    break;

                default:
                    psz_cat = "unknown";
                    break;
            }

            msg_Dbg( p_demux, "adding track[Id 0x%x] %s (%s) language %s",
                     p_sys->track[i].i_track_ID, psz_cat,
                     p_sys->track[i].b_enable ? "enable":"disable",
                     p_sys->track[i].fmt.psz_language ?
                     p_sys->track[i].fmt.psz_language : "undef" );
        }
        else if( p_sys->track[i].b_ok && p_sys->track[i].b_chapters_source )
        {
            msg_Dbg( p_demux, "using track[Id 0x%x] for chapter language %s",
                     p_sys->track[i].i_track_ID,
                     p_sys->track[i].fmt.psz_language ?
                     p_sys->track[i].fmt.psz_language : "undef" );
        }
        else
        {
            msg_Dbg( p_demux, "ignoring track[Id 0x%x]",
                     p_sys->track[i].i_track_ID );
        }
    }

    p_mvex = MP4_BoxGet( p_sys->p_moov, "mvex" );
    if( p_mvex != NULL )
    {
        const MP4_Box_t *p_mehd = MP4_BoxGet( p_mvex, "mehd");
        if ( p_mehd && BOXDATA(p_mehd) )
        {
            if( BOXDATA(p_mehd)->i_fragment_duration > p_sys->i_duration )
            {
                p_sys->b_fragmented = true;
                p_sys->i_duration = BOXDATA(p_mehd)->i_fragment_duration;
            }
        }

        const MP4_Box_t *p_sidx = MP4_BoxGet( p_sys->p_root, "sidx");
        if( p_sidx )
            p_sys->b_fragmented = true;

        if ( p_sys->b_seekable )
        {
            if( !p_sys->b_fragmented /* as unknown */ )
            {
                /* Probe remaining to check if there's really fragments
                   or if that file is just ready to append fragments */
                ProbeFragments( p_demux, (p_sys->i_duration == 0), &p_sys->b_fragmented );
            }

            if( vlc_stream_Seek( p_demux->s, p_sys->p_moov->i_pos ) != VLC_SUCCESS )
                goto error;
        }
        else /* Handle as fragmented by default as we can't see moof */
        {
            p_sys->context.p_fragment_atom = p_sys->p_moov;
            p_sys->context.i_current_box_type = ATOM_moov;
            p_sys->b_fragmented = true;
        }
    }

    if( p_sys->b_fragmented )
    {
        p_demux->pf_demux = DemuxFrag;
        msg_Dbg( p_demux, "Set Fragmented demux mode" );
    }

    if( !p_sys->b_seekable && p_demux->pf_demux == Demux )
    {
        msg_Warn( p_demux, "MP4 plugin discarded (not seekable)" );
        goto error;
    }

    if( p_sys->i_tracks > 1 && !p_sys->b_fastseekable )
    {
        uint64_t i_max_continuity;
        bool b_flat;
        MP4_GetInterleaving( p_demux, &i_max_continuity, &b_flat );
        if( b_flat )
            msg_Warn( p_demux, "that media doesn't look interleaved, will need to seek");
        else if( i_max_continuity > DEMUX_TRACK_MAX_PRELOAD )
            msg_Warn( p_demux, "that media doesn't look properly interleaved, will need to seek");
    }

    /* */
    LoadChapter( p_demux );

    p_sys->asfpacketsys.p_demux = p_demux;
    p_sys->asfpacketsys.pi_preroll = &p_sys->i_preroll;
    p_sys->asfpacketsys.pi_preroll_start = &p_sys->i_preroll_start;
    p_sys->asfpacketsys.pf_doskip = NULL;
    p_sys->asfpacketsys.pf_send = MP4ASF_Send;
    p_sys->asfpacketsys.pf_gettrackinfo = MP4ASF_GetTrackInfo;
    p_sys->asfpacketsys.pf_updatetime = NULL;
    p_sys->asfpacketsys.pf_setaspectratio = NULL;

    return VLC_SUCCESS;

error:
    if( vlc_stream_Tell( p_demux->s ) > 0 )
    {
        if( vlc_stream_Seek( p_demux->s, 0 ) != VLC_SUCCESS )
            msg_Warn( p_demux, "Can't reset stream position from probing" );
    }

    Close( p_this );

    return VLC_EGENERIC;
}

const unsigned int SAMPLEHEADERSIZE = 4;
const unsigned int RTPPACKETSIZE = 12;
const unsigned int CONSTRUCTORSIZE = 16;

/*******************************************************************************
 * MP4_RTPHintToFrame: converts RTP Reception Hint Track sample to H.264 frame
 *******************************************************************************/
static block_t * MP4_RTPHintToFrame( demux_t *p_demux, block_t *p_block, uint32_t packetcount )
{
    uint8_t *p_slice = p_block->p_buffer + SAMPLEHEADERSIZE;
    block_t *p_newblock = NULL;
    size_t i_payload = 0;

    if( p_block->i_buffer < SAMPLEHEADERSIZE + RTPPACKETSIZE + CONSTRUCTORSIZE )
    {
        msg_Err( p_demux, "Sample not large enough for necessary structs");
        block_Release( p_block );
        return NULL;
    }

    for( uint32_t i = 0; i < packetcount; ++i )
    {
        if( (size_t)(p_slice - p_block->p_buffer) + RTPPACKETSIZE + CONSTRUCTORSIZE > p_block->i_buffer )
            goto error;

        /* skip RTP header in sample. Could be used to detect packet losses */
        p_slice += RTPPACKETSIZE;

        mp4_rtpsampleconstructor_t sample_cons;

        sample_cons.type =                      p_slice[0];
        sample_cons.trackrefindex =             p_slice[1];
        sample_cons.length =          GetWBE(  &p_slice[2] );
        sample_cons.samplenumber =    GetDWBE( &p_slice[4] );
        sample_cons.sampleoffset =    GetDWBE( &p_slice[8] );
        sample_cons.bytesperblock =   GetWBE(  &p_slice[12] );
        sample_cons.samplesperblock = GetWBE(  &p_slice[14] );

        /* skip packet constructor */
        p_slice += CONSTRUCTORSIZE;

        /* check that is RTPsampleconstructor, referencing itself and no weird audio stuff */
        if( sample_cons.type != 2||sample_cons.trackrefindex != -1
            ||sample_cons.samplesperblock != 1||sample_cons.bytesperblock != 1 )
        {
            msg_Err(p_demux, "Unhandled constructor in RTP Reception Hint Track. Type:%u", sample_cons.type);
            goto error;
        }

        /* slice doesn't fit in buffer */
        if( sample_cons.sampleoffset + sample_cons.length > p_block->i_buffer)
        {
            msg_Err(p_demux, "Sample buffer is smaller than sample" );
            goto error;
        }

        block_t *p_realloc = ( p_newblock ) ?
                             block_Realloc( p_newblock, 0, i_payload + sample_cons.length + 4 ):
                             block_Alloc( i_payload + sample_cons.length + 4 );
        if( !p_realloc )
            goto error;

        p_newblock = p_realloc;
        uint8_t *p_dst = &p_newblock->p_buffer[i_payload];

        const uint8_t* p_src = p_block->p_buffer + sample_cons.sampleoffset;
        uint8_t i_type = (*p_src) & ((1<<5)-1);

        const uint8_t synccode[4] = { 0, 0, 0, 1 };
        if( memcmp( p_src, synccode, 4 ) )
        {
            if( i_type == 7 || i_type == 8 )
                *p_dst++=0;

            p_dst[0] = 0;
            p_dst[1] = 0;
            p_dst[2] = 1;
            p_dst += 3;
        }

        memcpy( p_dst, p_src, sample_cons.length );
        p_dst += sample_cons.length;

        i_payload = p_dst - p_newblock->p_buffer;
    }

    block_Release( p_block );
    if( p_newblock )
        p_newblock->i_buffer = i_payload;
    return p_newblock;

error:
    block_Release( p_block );
    if( p_newblock )
        block_Release( p_newblock );
    return NULL;
}

/* RTP Reception Hint Track */
static block_t * MP4_RTPHint_Convert( demux_t *p_demux, block_t *p_block, vlc_fourcc_t i_codec )
{
    block_t *p_converted = NULL;
    if( p_block->i_buffer < 2 )
    {
        block_Release( p_block );
        return NULL;
    }

    /* number of RTP packets contained in this sample */
    const uint16_t i_packets = GetWBE( p_block->p_buffer );
    if( i_packets <= 1 || i_codec != VLC_CODEC_H264 )
    {
        const size_t i_skip = SAMPLEHEADERSIZE + i_packets * ( RTPPACKETSIZE + CONSTRUCTORSIZE );
        if( i_packets == 1 && i_skip < p_block->i_buffer )
        {
            p_block->p_buffer += i_skip;
            p_converted = p_block;
        }
        else
        {
            block_Release( p_block );
        }
    }
    else
    {
        p_converted = MP4_RTPHintToFrame( p_demux, p_block, i_packets );
    }

    return p_converted;
}

/*****************************************************************************
 * Demux: read packet and send them to decoders
 *****************************************************************************
 * TODO check for newly selected track (ie audio upt to now )
 *****************************************************************************/
static int DemuxTrack( demux_t *p_demux, mp4_track_t *tk, uint64_t i_readpos,
                       unsigned i_max_preload )
{
    uint32_t i_nb_samples = 0;
    uint32_t i_samplessize = 0;

    if( !tk->b_ok || tk->i_sample >= tk->i_sample_count )
        return VLC_DEMUXER_EOS;

    if( tk->b_chapters_source )
        return VLC_DEMUXER_SUCCESS;

    uint32_t i_run_seq = MP4_TrackGetRunSeq( tk );
    mtime_t i_current_nzdts = MP4_TrackGetDTS( p_demux, tk );
    const mtime_t i_demux_max_nzdts =(i_max_preload < UINT_MAX)
                                    ? i_current_nzdts + i_max_preload
                                    : INT64_MAX;

    for( ; i_demux_max_nzdts >= i_current_nzdts; )
    {
        if( tk->i_sample >= tk->i_sample_count )
            return VLC_DEMUXER_EOS;

#if 0
        msg_Dbg( p_demux, "tk(%i)=%"PRId64" mv=%"PRId64" pos=%"PRIu64, tk->i_track_ID,
                 MP4_TrackGetDTS( p_demux, tk ),
                 MP4_GetMoviePTS( p_demux->p_sys ), i_readpos );
#endif

        i_samplessize = MP4_TrackGetReadSize( tk, &i_nb_samples );
        if( i_samplessize > 0 )
        {
            block_t *p_block;
            int64_t i_delta;

            if( vlc_stream_Tell( p_demux->s ) != i_readpos )
            {
                if( MP4_Seek( p_demux->s, i_readpos ) != VLC_SUCCESS )
                {
                    msg_Warn( p_demux, "track[0x%x] will be disabled (eof?)"
                                       ": Failed to seek to %"PRIu64,
                              tk->i_track_ID, i_readpos );
                    MP4_TrackSelect( p_demux, tk, false );
                    goto end;
                }
            }

            /* now read pes */
            if( !(p_block = vlc_stream_Block( p_demux->s, i_samplessize )) )
            {
                msg_Warn( p_demux, "track[0x%x] will be disabled (eof?)"
                                   ": Failed to read %d bytes sample at %"PRIu64,
                          tk->i_track_ID, i_samplessize, i_readpos );
                MP4_TrackSelect( p_demux, tk, false );
                goto end;
            }

            /* !important! Ensure clock is set before sending data */
            if( p_demux->p_sys->i_pcr == VLC_TS_INVALID )
            {
                es_out_SetPCR( p_demux->out, VLC_TS_0 + i_current_nzdts );
                p_demux->p_sys->i_pcr = VLC_TS_0 + i_current_nzdts;
            }

            /* dts */
            p_block->i_dts = VLC_TS_0 + i_current_nzdts;
            /* pts */
            if( MP4_TrackGetPTSDelta( p_demux, tk, &i_delta ) )
                p_block->i_pts = p_block->i_dts + i_delta;
            else if( tk->fmt.i_cat != VIDEO_ES )
                p_block->i_pts = p_block->i_dts;
            else
                p_block->i_pts = VLC_TS_INVALID;

            MP4_Block_Send( p_demux, tk, p_block );
        }

        /* Next sample */
        if ( i_nb_samples ) /* sample size could be 0, need to go fwd. see return */
            MP4_TrackNextSample( p_demux, tk, i_nb_samples );

        uint32_t i_next_run_seq = MP4_TrackGetRunSeq( tk );
        if( i_next_run_seq != i_run_seq )
            break;

        i_current_nzdts = MP4_TrackGetDTS( p_demux, tk );
        i_readpos = MP4_TrackGetPos( tk );
    }

    return VLC_DEMUXER_SUCCESS;

end:
    return VLC_DEMUXER_EGENERIC;
}

static int DemuxMoov( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned int i_track;

    /* check for newly selected/unselected track */
    for( i_track = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        mp4_track_t *tk = &p_sys->track[i_track];
        bool b = true;

        if( !tk->b_ok || tk->b_chapters_source ||
            ( tk->b_selected && tk->i_sample >= tk->i_sample_count ) )
        {
            continue;
        }

        if( p_sys->b_seekable )
            es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE, tk->p_es, &b );

        if( tk->b_selected && !b )
        {
            MP4_TrackSelect( p_demux, tk, false );
        }
        else if( !tk->b_selected && b)
        {
            MP4_TrackSeek( p_demux, tk, MP4_GetMoviePTS( p_sys ) );
        }
    }

    const mtime_t i_nztime = MP4_GetMoviePTS( p_sys );

    /* We demux/set pcr, even without selected tracks, (empty edits, ...) */
    if( p_sys->i_pcr != VLC_TS_INVALID /* not after a seek */ )
    {
        bool b_eof = true;
        for( i_track = 0; i_track < p_sys->i_tracks; i_track++ )
        {
            mp4_track_t *tk = &p_sys->track[i_track];
            if( !tk->b_ok || tk->b_chapters_source || tk->i_sample >= tk->i_sample_count )
                continue;
            /* Test for EOF on each track (samples count, edit list) */
            b_eof &= ( i_nztime > MP4_TrackGetDTS( p_demux, tk ) );
        }
        if( b_eof )
            return VLC_DEMUXER_EOS;
    }

    const unsigned i_max_preload = ( p_sys->b_fastseekable ) ? 0 : ( p_sys->b_seekable ) ? DEMUX_TRACK_MAX_PRELOAD : UINT_MAX;
    int i_status;
    /* demux up to increment amount of data on every track, or just set pcr if empty data */
    for( ;; )
    {
        mp4_track_t *tk = NULL;
        i_status = VLC_DEMUXER_EOS;

        /* First pass, find any track within our target increment, ordered by position */
        for( i_track = 0; i_track < p_sys->i_tracks; i_track++ )
        {
            mp4_track_t *tk_tmp = &p_sys->track[i_track];
            if( !tk_tmp->b_ok || tk_tmp->b_chapters_source ||
                tk_tmp->i_sample >= tk_tmp->i_sample_count ||
                (!tk_tmp->b_selected && p_sys->b_seekable) )
                continue;

            /* At least still have data to demux on this or next turns */
            i_status = VLC_DEMUXER_SUCCESS;

            if ( MP4_TrackGetDTS( p_demux, tk_tmp ) <= i_nztime + DEMUX_INCREMENT )
            {
                if( tk == NULL || MP4_TrackGetPos( tk_tmp ) < MP4_TrackGetPos( tk ) )
                    tk = tk_tmp;
            }
        }

        if( tk )
        {
            /* Second pass, refine and find any best candidate having a chunk pos closer than
             * current candidate (avoids seeks when increment falls between the 2) from
             * current position, but within extended interleave time */
            for( i_track = 0; i_max_preload > 0 && i_track < p_sys->i_tracks; i_track++ )
            {
                mp4_track_t *tk_tmp = &p_sys->track[i_track];
                if( tk_tmp == tk ||
                    !tk_tmp->b_ok || tk_tmp->b_chapters_source ||
                   (!tk_tmp->b_selected && p_sys->b_seekable) ||
                    tk_tmp->i_sample >= tk_tmp->i_sample_count )
                    continue;

                mtime_t i_nzdts = MP4_TrackGetDTS( p_demux, tk_tmp );
                if ( i_nzdts <= i_nztime + DEMUX_TRACK_MAX_PRELOAD )
                {
                    /* Found a better candidate to avoid seeking */
                    if( MP4_TrackGetPos( tk_tmp ) < MP4_TrackGetPos( tk ) )
                        tk = tk_tmp;
                    /* Note: previous candidate will be repicked on next loop */
                }
            }

            uint64_t i_pos = MP4_TrackGetPos( tk );
            int i_ret = DemuxTrack( p_demux, tk, i_pos, i_max_preload );

            if( i_ret == VLC_DEMUXER_SUCCESS )
                i_status = VLC_DEMUXER_SUCCESS;
        }

        if( i_status != VLC_DEMUXER_SUCCESS || !tk )
            break;
    }

    p_sys->i_nztime += DEMUX_INCREMENT;
    if( p_sys->i_pcr > VLC_TS_INVALID )
    {
        p_sys->i_pcr = VLC_TS_0 + p_sys->i_nztime;
        es_out_SetPCR( p_demux->out, p_sys->i_pcr );
    }

    /* */
    MP4_UpdateSeekpoint( p_demux, i_nztime + DEMUX_INCREMENT );

    return i_status;
}

static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    assert( ! p_sys->b_fragmented );

    int i_status = DemuxMoov( p_demux );

    if( i_status == VLC_DEMUXER_EOS )
        i_status = VLC_DEMUXER_EOF;

    return i_status;
}

static void MP4_UpdateSeekpoint( demux_t *p_demux, int64_t i_time )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i;
    if( !p_sys->p_title )
        return;
    for( i = 0; i < p_sys->p_title->i_seekpoint; i++ )
    {
        if( i_time < p_sys->p_title->seekpoint[i]->i_time_offset )
            break;
    }
    i--;

    if( i != p_demux->info.i_seekpoint && i >= 0 )
    {
        p_demux->info.i_seekpoint = i;
        p_demux->info.i_update |= INPUT_UPDATE_SEEKPOINT;
    }
}
/*****************************************************************************
 * Seek: Go to i_date
******************************************************************************/
static int Seek( demux_t *p_demux, mtime_t i_date, bool b_accurate )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned int i_track;

    /* Now for each stream try to go to this time */
    mtime_t i_start = i_date;
    for( i_track = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        mp4_track_t *tk = &p_sys->track[i_track];
        /* FIXME: we should find the lowest time from tracks with indexes.
           considering only video for now */
        if( tk->fmt.i_cat != VIDEO_ES )
            continue;
        if( MP4_TrackSeek( p_demux, tk, i_date ) == VLC_SUCCESS )
        {
            mtime_t i_seeked = MP4_TrackGetDTS( p_demux, tk );
            if( i_seeked < i_start )
                i_start = i_seeked;
        }
    }

    msg_Dbg( p_demux, "seeking with %"PRId64 "ms %s", (i_date - i_start) / 1000,
            !b_accurate ? "alignment" : "preroll (use input-fast-seek to avoid)" );

    for( i_track = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        mp4_track_t *tk = &p_sys->track[i_track];
        if( tk->fmt.i_cat == VIDEO_ES )
            continue;
        MP4_TrackSeek( p_demux, tk, i_start );
    }

    MP4_UpdateSeekpoint( p_demux, i_date );
    MP4ASF_ResetFrames( p_sys );
    /* update global time */
    p_sys->i_nztime = i_start;
    p_sys->i_pcr  = VLC_TS_INVALID;

    if( b_accurate )
        es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME, i_date );

    return VLC_SUCCESS;
}

static int FragPrepareChunk( demux_t *p_demux, MP4_Box_t *p_moof,
                             MP4_Box_t *p_sidx, stime_t i_moof_time, bool b_discontinuity )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( FragCreateTrunIndex( p_demux, p_moof, p_sidx, i_moof_time, b_discontinuity ) == VLC_SUCCESS )
    {
        for( unsigned i=0; i<p_sys->i_tracks; i++ )
        {
            mp4_track_t *p_track = &p_sys->track[i];
            if( p_track->context.runs.i_count )
            {
                const mp4_run_t *p_run = &p_track->context.runs.p_array[0];
                p_track->context.i_trun_sample_pos = p_run->i_offset;
                p_track->context.i_trun_sample = 0;
                p_track->i_time = p_run->i_first_dts;
            }
        }
        return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}

static mtime_t FragGetDemuxTimeFromTracksTime( demux_sys_t *p_sys )
{
    mtime_t i_time = INT64_MAX;
    for( unsigned int i = 0; i < p_sys->i_tracks; i++ )
    {
        if( p_sys->track[i].context.runs.i_count == 0 )
            continue;
        mtime_t i_ttime = MP4_rescale( p_sys->track[i].i_time,
                                       p_sys->track[i].i_timescale, CLOCK_FREQ );
        i_time = __MIN( i_time, i_ttime );
    }
    return i_time;
}

static uint32_t FragGetMoofSequenceNumber( MP4_Box_t *p_moof )
{
    const MP4_Box_t *p_mfhd = MP4_BoxGet( p_moof, "mfhd" );
    if( p_mfhd && BOXDATA(p_mfhd) )
        return BOXDATA(p_mfhd)->i_sequence_number;
    return 0;
}

static int FragSeekLoadFragment( demux_t *p_demux, uint32_t i_moox, stime_t i_moox_time )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    MP4_Box_t *p_moox;

    if( i_moox == ATOM_moov )
    {
        p_moox = p_sys->p_moov;
    }
    else
    {
        const uint8_t *p_peek;
        if( vlc_stream_Peek( p_demux->s, &p_peek, 8 ) != 8 )
            return VLC_EGENERIC;

        if( ATOM_moof != VLC_FOURCC( p_peek[4], p_peek[5], p_peek[6], p_peek[7] ) )
            return VLC_EGENERIC;

        MP4_Box_t *p_vroot = MP4_BoxGetNextChunk( p_demux->s );
        if(!p_vroot)
            return VLC_EGENERIC;
        p_moox = MP4_BoxExtract( &p_vroot->p_first, ATOM_moof );
        MP4_BoxFree( p_vroot );

        if(!p_moox)
            return VLC_EGENERIC;
    }

    FragResetContext( p_sys );

    /* map context */
    p_sys->context.p_fragment_atom = p_moox;
    p_sys->context.i_current_box_type = i_moox;

    if( i_moox == ATOM_moof )
    {
        FragPrepareChunk( p_demux, p_moox, NULL, i_moox_time, true );
        p_sys->context.i_lastseqnumber = FragGetMoofSequenceNumber( p_moox );

        p_sys->i_nztime = FragGetDemuxTimeFromTracksTime( p_sys );
        p_sys->i_pcr = VLC_TS_INVALID;
    }

    msg_Dbg( p_demux, "seeked to %4.4s at pos %" PRIu64, (char *) &i_moox, p_moox->i_pos );
    return VLC_SUCCESS;
}

static unsigned GetSeekTrackIndex( demux_sys_t *p_sys )
{
    unsigned cand = 0;
    for( unsigned i=0; i<p_sys->i_tracks; i++ )
    {
        if( p_sys->track[i].fmt.i_cat == VIDEO_ES ||
            p_sys->track[i].fmt.i_cat == AUDIO_ES )
        {
            if( cand != i && !p_sys->track[cand].b_selected )
                cand = i;
        }
    }
    return cand;
}

static void FragTrunSeekToTime( mp4_track_t *p_track, stime_t i_target_time )
{
    if( !p_track->b_ok || p_track->context.runs.i_count < 1 )
        return;

    unsigned i_run = 0;
    unsigned i_sample = 0;
    uint64_t i_pos = p_track->context.runs.p_array[0].i_offset;
    stime_t  i_time = p_track->context.runs.p_array[0].i_first_dts;

    for( unsigned r = 0; r < p_track->context.runs.i_count; r++ )
    {
        const mp4_run_t *p_run = &p_track->context.runs.p_array[r];
        const MP4_Box_data_trun_t *p_data =
                    p_track->context.runs.p_array[r].p_trun->data.p_trun;
        if( i_time > i_target_time )
            break;

        i_run = r;
        i_time = p_run->i_first_dts;
        i_pos = p_run->i_offset;
        i_sample = 0;

        uint32_t dur = p_track->context.i_default_sample_duration;
        uint32_t len = p_track->context.i_default_sample_size;
        for ( unsigned i=0; i<p_data->i_sample_count; i++ )
        {
            if( p_data->i_flags & MP4_TRUN_SAMPLE_DURATION )
                dur = p_data->p_samples[i].i_duration;

            /* check condition */
            if( i_time + dur > i_target_time )
                break;

            if( p_data->i_flags & MP4_TRUN_SAMPLE_SIZE )
                len = p_data->p_samples[i].i_size;

            i_time += dur;
            i_pos += len;
        }
    }

    p_track->context.i_trun_sample = i_sample;
    p_track->context.i_trun_sample_pos = i_pos;
    p_track->context.runs.i_current = i_run;
}

static int FragSeekToTime( demux_t *p_demux, mtime_t i_nztime, bool b_accurate )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    uint64_t i64 = UINT64_MAX;
    uint32_t i_segment_type = ATOM_moof;
    stime_t  i_segment_time = INT64_MAX;
    mtime_t i_sync_time = i_nztime;
    bool b_iframesync = false;

    const uint64_t i_duration = __MAX(p_sys->i_duration, p_sys->i_cumulated_duration);
    if ( !p_sys->i_timescale || !i_duration || !p_sys->b_seekable )
         return VLC_EGENERIC;

    uint64_t i_backup_pos = vlc_stream_Tell( p_demux->s );

    if ( !p_sys->b_fragments_probed && !p_sys->b_index_probed && p_sys->b_seekable )
    {
        ProbeIndex( p_demux );
        p_sys->b_index_probed = true;
    }

    const unsigned i_seek_track_index = GetSeekTrackIndex( p_sys );
    const unsigned i_seek_track_ID = p_sys->track[i_seek_track_index].i_track_ID;

    if( MP4_rescale( i_nztime, CLOCK_FREQ, p_sys->i_timescale )
                     < GetMoovTrackDuration( p_sys, i_seek_track_ID ) )
    {
        i64 = p_sys->p_moov->i_pos;
        i_segment_type = ATOM_moov;
    }
    else if( FragGetMoofBySidxIndex( p_demux, i_nztime, &i64, &i_sync_time ) == VLC_SUCCESS )
    {
        /* provides base offset */
        i_segment_time = i_sync_time;
        msg_Dbg( p_demux, "seeking to sidx moof pos %" PRId64 " %" PRId64, i64, i_sync_time );
    }
    else
    {
        bool b_buildindex = false;

        if( FragGetMoofByTfraIndex( p_demux, i_nztime, i_seek_track_ID, &i64, &i_sync_time ) == VLC_SUCCESS )
        {
            /* Does only provide segment position and a sync sample time */
            msg_Dbg( p_demux, "seeking to sync point %" PRId64, i_sync_time );
            b_iframesync = true;
        }
        else if( !p_sys->b_fragments_probed && !p_sys->b_fastseekable )
        {
            const char *psz_msg = _(
                "Because this file index is broken or missing, "
                "seeking will not work correctly.\n"
                "VLC won't repair your file but can temporary fix this "
                "problem by building an index in memory.\n"
                "This step might take a long time on a large file.\n"
                "What do you want to do?");
            b_buildindex = vlc_dialog_wait_question( p_demux,
                                                     VLC_DIALOG_QUESTION_NORMAL,
                                                     _("Do not seek"),
                                                     _("Build index"),
                                                     NULL,
                                                     _("Broken or missing Index"),
                                                     "%s", psz_msg );
        }

        if( !p_sys->b_fragments_probed && ( p_sys->b_fastseekable || b_buildindex ) )
        {
            bool foo;
            int i_ret = vlc_stream_Seek( p_demux->s, p_sys->p_moov->i_pos + p_sys->p_moov->i_size );
            if( i_ret == VLC_SUCCESS )
            {
                i_ret = ProbeFragments( p_demux, true, &foo );
                p_sys->b_fragments_probed = true;
            }
            if( i_ret != VLC_SUCCESS )
            {
                p_sys->b_error = (vlc_stream_Seek( p_demux->s, i_backup_pos ) != VLC_SUCCESS);
                return i_ret;
            }
        }

        if( p_sys->b_fragments_probed && p_sys->p_fragsindex )
        {
            stime_t i_basetime = MP4_rescale( i_sync_time, CLOCK_FREQ, p_sys->i_timescale );
            if( !MP4_Fragments_Index_Lookup( p_sys->p_fragsindex, &i_basetime, &i64, i_seek_track_index ) )
            {
                p_sys->b_error = (vlc_stream_Seek( p_demux->s, i_backup_pos ) != VLC_SUCCESS);
                return VLC_EGENERIC;
            }
            msg_Dbg( p_demux, "seeking to fragment index pos %" PRId64 " %" PRId64, i64,
                     MP4_rescale( i_basetime, p_sys->i_timescale, CLOCK_FREQ ) );
        }
    }

    if( i64 == UINT64_MAX )
    {
        msg_Warn( p_demux, "seek by index failed" );
        p_sys->b_error = (vlc_stream_Seek( p_demux->s, i_backup_pos ) != VLC_SUCCESS);
        return VLC_EGENERIC;
    }

    msg_Dbg( p_demux, "final seek to fragment at %"PRId64, i64 );
    if( vlc_stream_Seek( p_demux->s, i64 ) )
    {
        msg_Err( p_demux, "seek failed to %"PRId64, i64 );
        p_sys->b_error = (vlc_stream_Seek( p_demux->s, i_backup_pos ) != VLC_SUCCESS);
        return VLC_EGENERIC;
    }

    /* Context is killed on success */
    if( FragSeekLoadFragment( p_demux, i_segment_type, i_segment_time ) != VLC_SUCCESS )
    {
        p_sys->b_error = (vlc_stream_Seek( p_demux->s, i_backup_pos ) != VLC_SUCCESS);
        return VLC_EGENERIC;
    }

    p_sys->i_pcr  = VLC_TS_INVALID;

    for( unsigned i=0; i<p_sys->i_tracks; i++ )
    {
        if( i_segment_type == ATOM_moov )
        {
            MP4_TrackSeek( p_demux, &p_sys->track[i], i_sync_time );
            p_sys->i_nztime = i_sync_time;
            p_sys->i_pcr  = VLC_TS_INVALID;
        }
        else if( b_iframesync )
        {
            stime_t i_tst = MP4_rescale( i_sync_time, CLOCK_FREQ, p_sys->track[i].i_timescale );
            FragTrunSeekToTime( &p_sys->track[i], i_tst );
            p_sys->track[i].i_next_block_flags |= BLOCK_FLAG_DISCONTINUITY;
        }
    }

    MP4ASF_ResetFrames( p_sys );
    /* And set next display time in that trun/fragment */
    if( b_iframesync && b_accurate )
        es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME, VLC_TS_0 + i_nztime );
    return VLC_SUCCESS;
}

static int FragSeekToPos( demux_t *p_demux, double f, bool b_accurate )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const uint64_t i_duration = __MAX(p_sys->i_duration, p_sys->i_cumulated_duration);

    if ( !p_sys->b_seekable || !p_sys->i_timescale || !i_duration )
        return VLC_EGENERIC;

    return FragSeekToTime( p_demux, (mtime_t)( f *
                           MP4_rescale( i_duration, p_sys->i_timescale, CLOCK_FREQ ) ), b_accurate );
}

static bool imageTypeCompatible( const MP4_Box_data_data_t *p_data )
{
    return p_data && (
    p_data->e_wellknowntype == DATA_WKT_PNG ||
    p_data->e_wellknowntype == DATA_WKT_JPEG ||
    p_data->e_wellknowntype == DATA_WKT_BMP );
}

static int MP4_LoadMeta( demux_sys_t *p_sys, vlc_meta_t *p_meta )
{
    MP4_Box_t *p_data = NULL;
    MP4_Box_t *p_udta = NULL;
    bool b_attachment_set = false;

    if( !p_meta )
        return VLC_EGENERIC;

    for( int i_index = 0; psz_meta_roots[i_index] && !p_udta; i_index++ )
    {
        p_udta = MP4_BoxGet( p_sys->p_root, psz_meta_roots[i_index] );
        if ( p_udta )
        {
            p_data = MP4_BoxGet( p_udta, "covr/data" );
            if ( p_data && imageTypeCompatible( BOXDATA(p_data) ) )
            {
                char *psz_attachment;
                if ( -1 != asprintf( &psz_attachment, "attachment://%s/covr/data[0]",
                                     psz_meta_roots[i_index] ) )
                {
                    vlc_meta_SetArtURL( p_meta, psz_attachment );
                    b_attachment_set = true;
                    free( psz_attachment );
                }
            }
        }
    }

    const MP4_Box_t *p_pnot;
    if ( !b_attachment_set && (p_pnot = MP4_BoxGet( p_sys->p_root, "pnot" )) )
    {
        for ( size_t i=0; i< ARRAY_SIZE(rgi_pict_atoms) && !b_attachment_set; i++ )
        {
            if ( rgi_pict_atoms[i] == BOXDATA(p_pnot)->i_type )
            {
                char rgsz_path[26];
                snprintf( rgsz_path, 26, "attachment://%4.4s[%"PRIu16"]",
                          (char*)&rgi_pict_atoms[i], BOXDATA(p_pnot)->i_index - 1 );
                vlc_meta_SetArtURL( p_meta, rgsz_path );
                b_attachment_set = true;
            }
        }
    }

    if( p_udta == NULL )
    {
        if( !b_attachment_set )
            return VLC_EGENERIC;
    }
    else SetupMeta( p_meta, p_udta );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    double f, *pf;
    int64_t i64, *pi64;
    bool b;

    const uint64_t i_duration = __MAX(p_sys->i_duration, p_sys->i_cumulated_duration);

    switch( i_query )
    {
        case DEMUX_CAN_SEEK:
            *va_arg( args, bool * ) = p_sys->b_seekable;
            return VLC_SUCCESS;

        case DEMUX_GET_POSITION:
            pf = va_arg( args, double * );
            if( i_duration > 0 )
            {
                *pf = (double)p_sys->i_nztime /
                      MP4_rescale( i_duration, p_sys->i_timescale, CLOCK_FREQ );
            }
            else
            {
                *pf = 0.0;
            }
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            f = va_arg( args, double );
            b = va_arg( args, int );
            if ( p_demux->pf_demux == DemuxFrag )
                return FragSeekToPos( p_demux, f, b );
            else if( p_sys->i_timescale > 0 )
            {
                i64 = (int64_t)( f * MP4_rescale( p_sys->i_duration,
                                                  p_sys->i_timescale, CLOCK_FREQ ) );
                return Seek( p_demux, i64, b );
            }
            else return VLC_EGENERIC;

        case DEMUX_GET_TIME:
            pi64 = va_arg( args, int64_t * );
            if( p_sys->i_timescale > 0 )
                *pi64 = p_sys->i_nztime;
            else
                *pi64 = 0;
            return VLC_SUCCESS;

        case DEMUX_SET_TIME:
            i64 = va_arg( args, int64_t );
            b = va_arg( args, int );
            if ( p_demux->pf_demux == DemuxFrag )
                return FragSeekToTime( p_demux, i64, b );
            else
                return Seek( p_demux, i64, b );

        case DEMUX_GET_LENGTH:
            pi64 = va_arg( args, int64_t * );
            if( p_sys->i_timescale > 0 )
            {
                *pi64 = MP4_rescale( i_duration,
                                     p_sys->i_timescale, CLOCK_FREQ );
            }
            else *pi64 = 0;
            return VLC_SUCCESS;

        case DEMUX_GET_FPS:
            pf = va_arg( args, double * );
            *pf = p_sys->f_fps;
            return VLC_SUCCESS;

        case DEMUX_GET_ATTACHMENTS:
        {
            input_attachment_t ***ppp_attach = va_arg( args, input_attachment_t*** );
            int *pi_int = va_arg( args, int * );

            MP4_Box_t *p_udta = NULL;
            size_t i_count = 0;
            int i_index = 0;

            /* Count number of total attachments */
            for( ; psz_meta_roots[i_index] && !p_udta; i_index++ )
            {
                p_udta = MP4_BoxGet( p_sys->p_root, psz_meta_roots[i_index] );
                if ( p_udta )
                    i_count += MP4_BoxCount( p_udta, "covr/data" );
            }

            for ( size_t i=0; i< ARRAY_SIZE(rgi_pict_atoms); i++ )
            {
                char rgsz_path[5];
                snprintf( rgsz_path, 5, "%4.4s", (char*)&rgi_pict_atoms[i] );
                i_count += MP4_BoxCount( p_sys->p_root, rgsz_path );
            }

            if ( i_count == 0 )
                return VLC_EGENERIC;

            *ppp_attach = (input_attachment_t**)
                    vlc_alloc( i_count, sizeof(input_attachment_t*) );
            if( !(*ppp_attach) ) return VLC_ENOMEM;

            /* First add cover attachments */
            i_count = 0;
            size_t i_box_count = 0;
            if ( p_udta )
            {
                const MP4_Box_t *p_data = MP4_BoxGet( p_udta, "covr/data" );
                for( ; p_data; p_data = p_data->p_next )
                {
                    char *psz_mime;
                    char *psz_filename;
                    i_box_count++;

                    if ( p_data->i_type != ATOM_data || !imageTypeCompatible( BOXDATA(p_data) ) )
                        continue;

                    switch( BOXDATA(p_data)->e_wellknowntype )
                    {
                    case DATA_WKT_PNG:
                        psz_mime = strdup( "image/png" );
                        break;
                    case DATA_WKT_JPEG:
                        psz_mime = strdup( "image/jpeg" );
                        break;
                    case DATA_WKT_BMP:
                        psz_mime = strdup( "image/bmp" );
                        break;
                    default:
                        continue;
                    }

                    if ( asprintf( &psz_filename, "%s/covr/data[%"PRIu64"]", psz_meta_roots[i_index - 1],
                                   (uint64_t) i_box_count - 1 ) >= 0 )
                    {
                        (*ppp_attach)[i_count++] =
                            vlc_input_attachment_New( psz_filename, psz_mime, "Cover picture",
                                BOXDATA(p_data)->p_blob, BOXDATA(p_data)->i_blob );
                        msg_Dbg( p_demux, "adding attachment %s", psz_filename );
                        free( psz_filename );
                    }

                    free( psz_mime );
                }
            }

            /* Then quickdraw pict ones */
            for ( size_t i=0; i< ARRAY_SIZE(rgi_pict_atoms); i++ )
            {
                char rgsz_path[5];
                snprintf( rgsz_path, 5, "%4.4s", (char*)&rgi_pict_atoms[i] );
                const MP4_Box_t *p_pict = MP4_BoxGet( p_sys->p_root, rgsz_path );
                i_box_count = 0;
                for( ; p_pict; p_pict = p_pict->p_next )
                {
                    if ( i_box_count++ == UINT16_MAX ) /* pnot only handles 2^16 */
                        break;
                    if ( p_pict->i_type != rgi_pict_atoms[i] )
                        continue;
                    char rgsz_location[12];
                    snprintf( rgsz_location, 12, "%4.4s[%"PRIu16"]", (char*)&rgi_pict_atoms[i],
                              (uint16_t) i_box_count - 1 );
                    (*ppp_attach)[i_count] = vlc_input_attachment_New( rgsz_location, "image/x-pict",
                        "Quickdraw image", p_pict->data.p_binary->p_blob, p_pict->data.p_binary->i_blob );
                    if ( !(*ppp_attach)[i_count] )
                    {
                        i_count = 0;
                        break;
                    }
                    i_count++;
                    msg_Dbg( p_demux, "adding attachment %s", rgsz_location );
                }
            }

            if ( i_count == 0 )
            {
                free( *ppp_attach );
                return VLC_EGENERIC;
            }

            *pi_int = i_count;

            return VLC_SUCCESS;
        }

        case DEMUX_GET_META:
        {
            vlc_meta_t *p_meta = va_arg( args, vlc_meta_t *);

            if( !p_sys->p_meta )
                return VLC_EGENERIC;

            vlc_meta_Merge( p_meta, p_sys->p_meta );

            return VLC_SUCCESS;
        }

        case DEMUX_GET_TITLE_INFO:
        {
            input_title_t ***ppp_title = va_arg( args, input_title_t *** );
            int *pi_int = va_arg( args, int* );
            int *pi_title_offset = va_arg( args, int* );
            int *pi_seekpoint_offset = va_arg( args, int* );

            if( !p_sys->p_title )
                return VLC_EGENERIC;

            *pi_int = 1;
            *ppp_title = malloc( sizeof( input_title_t*) );
            (*ppp_title)[0] = vlc_input_title_Duplicate( p_sys->p_title );
            *pi_title_offset = 0;
            *pi_seekpoint_offset = 0;
            return VLC_SUCCESS;
        }
        case DEMUX_SET_TITLE:
        {
            const int i_title = va_arg( args, int );
            if( !p_sys->p_title || i_title != 0 )
                return VLC_EGENERIC;
            return VLC_SUCCESS;
        }
        case DEMUX_SET_SEEKPOINT:
        {
            const int i_seekpoint = va_arg( args, int );
            if( !p_sys->p_title )
                return VLC_EGENERIC;
            return Seek( p_demux, p_sys->p_title->seekpoint[i_seekpoint]->i_time_offset, true );
        }
        case DEMUX_GET_PTS_DELAY:
        {
            for( unsigned int i = 0; i < p_sys->i_tracks; i++ )
            {
                const MP4_Box_t *p_load;
                if ( (p_load = MP4_BoxGet( p_sys->track[i].p_track, "load" )) &&
                     BOXDATA(p_load)->i_duration > 0 )
                {
                    *va_arg(args, int64_t *) =
                            MP4_rescale( BOXDATA(p_load)->i_duration,
                                         p_sys->track[i].i_timescale, CLOCK_FREQ );
                    return VLC_SUCCESS;
                }
            }
            return demux_vaControlHelper( p_demux->s, 0, -1, 0, 1, i_query, args );
        }
        case DEMUX_SET_NEXT_DEMUX_TIME:
        case DEMUX_SET_GROUP:
        case DEMUX_HAS_UNSUPPORTED_META:
        case DEMUX_CAN_RECORD:
        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close ( vlc_object_t * p_this )
{
    demux_t *  p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned int i_track;

    msg_Dbg( p_demux, "freeing all memory" );

    FragResetContext( p_sys );

    MP4_BoxFree( p_sys->p_root );

    if( p_sys->p_title )
        vlc_input_title_Delete( p_sys->p_title );

    if( p_sys->p_meta )
        vlc_meta_Delete( p_sys->p_meta );

    MP4_Fragments_Index_Delete( p_sys->p_fragsindex );

    for( i_track = 0; i_track < p_sys->i_tracks; i_track++ )
        MP4_TrackClean( p_demux->out, &p_sys->track[i_track] );
    free( p_sys->track );

    free( p_sys );
}



/****************************************************************************
 * Local functions, specific to vlc
 ****************************************************************************/
/* Chapters */
static void LoadChapterGpac( demux_t  *p_demux, MP4_Box_t *p_chpl )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( BOXDATA(p_chpl)->i_chapter == 0 )
        return;

    p_sys->p_title = vlc_input_title_New();
    for( int i = 0; i < BOXDATA(p_chpl)->i_chapter && p_sys->p_title; i++ )
    {
        seekpoint_t *s = vlc_seekpoint_New();
        if( s == NULL) continue;

        s->psz_name = strdup( BOXDATA(p_chpl)->chapter[i].psz_name );
        if( s->psz_name == NULL)
        {
            vlc_seekpoint_Delete( s );;
            continue;
        }

        EnsureUTF8( s->psz_name );
        s->i_time_offset = BOXDATA(p_chpl)->chapter[i].i_start / 10;
        TAB_APPEND( p_sys->p_title->i_seekpoint, p_sys->p_title->seekpoint, s );
    }
}
static void LoadChapterGoPro( demux_t *p_demux, MP4_Box_t *p_hmmt )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    p_sys->p_title = vlc_input_title_New();
    if( p_sys->p_title )
        for( unsigned i = 0; i < BOXDATA(p_hmmt)->i_chapter_count; i++ )
        {
            seekpoint_t *s = vlc_seekpoint_New();
            if( s )
            {
                if( asprintf( &s->psz_name, "HiLight tag #%u", i+1 ) != -1 )
                    EnsureUTF8( s->psz_name );

                /* HiLights are stored in ms so we convert them to µs */
                s->i_time_offset = BOXDATA(p_hmmt)->pi_chapter_start[i] * 1000;
                TAB_APPEND( p_sys->p_title->i_seekpoint, p_sys->p_title->seekpoint, s );
            }
        }
}
static void LoadChapterApple( demux_t  *p_demux, mp4_track_t *tk )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    for( tk->i_sample = 0; tk->i_sample < tk->i_sample_count; tk->i_sample++ )
    {
        const int64_t i_dts = MP4_TrackGetDTS( p_demux, tk );
        int64_t i_pts_delta;
        if ( !MP4_TrackGetPTSDelta( p_demux, tk, &i_pts_delta ) )
            i_pts_delta = 0;
        uint32_t i_nb_samples = 0;
        const uint32_t i_size = MP4_TrackGetReadSize( tk, &i_nb_samples );

        if( i_size > 0 && !vlc_stream_Seek( p_demux->s, MP4_TrackGetPos( tk ) ) )
        {
            char p_buffer[256];
            const uint32_t i_read = stream_ReadU32( p_demux->s, p_buffer,
                                                    __MIN( sizeof(p_buffer), i_size ) );
            if( i_read > 2 )
            {
                const uint32_t i_string = __MIN( GetWBE(p_buffer), i_read-2 );
                const char *psnz_string = &p_buffer[2];

                seekpoint_t *s = vlc_seekpoint_New();
                if( s == NULL ) continue;

                if( i_string > 1 && !memcmp( psnz_string, "\xFF\xFE", 2 ) )
                    s->psz_name = FromCharset( "UTF-16LE", psnz_string, i_string );
                else
                    s->psz_name = strndup( psnz_string, i_string );

                if( s->psz_name == NULL )
                {
                    vlc_seekpoint_Delete( s );
                    continue;
                }

                EnsureUTF8( s->psz_name );
                s->i_time_offset = i_dts + __MAX( i_pts_delta, 0 );

                if( !p_sys->p_title )
                    p_sys->p_title = vlc_input_title_New();
                TAB_APPEND( p_sys->p_title->i_seekpoint, p_sys->p_title->seekpoint, s );
            }
        }
        if( tk->i_sample+1 >= tk->chunk[tk->i_chunk].i_sample_first +
                              tk->chunk[tk->i_chunk].i_sample_count )
            tk->i_chunk++;
    }
}
static void LoadChapter( demux_t  *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    MP4_Box_t *p_chpl;
    MP4_Box_t *p_hmmt;

    if( ( p_chpl = MP4_BoxGet( p_sys->p_root, "/moov/udta/chpl" ) ) &&
          BOXDATA(p_chpl) && BOXDATA(p_chpl)->i_chapter > 0 )
    {
        LoadChapterGpac( p_demux, p_chpl );
    }
    else if( ( p_hmmt = MP4_BoxGet( p_sys->p_root, "/moov/udta/HMMT" ) ) &&
             BOXDATA(p_hmmt) && BOXDATA(p_hmmt)->pi_chapter_start && BOXDATA(p_hmmt)->i_chapter_count > 0 )
    {
        LoadChapterGoPro( p_demux, p_hmmt );
    }
    else if( p_sys->p_tref_chap )
    {
        MP4_Box_data_tref_generic_t *p_chap = p_sys->p_tref_chap->data.p_tref_generic;
        unsigned int i, j;

        /* Load the first subtitle track like quicktime */
        for( i = 0; i < p_chap->i_entry_count; i++ )
        {
            for( j = 0; j < p_sys->i_tracks; j++ )
            {
                mp4_track_t *tk = &p_sys->track[j];
                if( tk->b_ok && tk->i_track_ID == p_chap->i_track_ID[i] &&
                    tk->fmt.i_cat == SPU_ES && tk->fmt.i_codec == VLC_CODEC_TX3G )
                    break;
            }
            if( j < p_sys->i_tracks )
            {
                LoadChapterApple( p_demux, &p_sys->track[j] );
                break;
            }
        }
    }

    /* Add duration if titles are enabled */
    if( p_sys->p_title )
    {
        const uint64_t i_duration = __MAX(p_sys->i_duration, p_sys->i_cumulated_duration);
        p_sys->p_title->i_length =
                MP4_rescale( i_duration,
                             p_sys->i_timescale, CLOCK_FREQ );
    }
}

/* now create basic chunk data, the rest will be filled by MP4_CreateSamplesIndex */
static int TrackCreateChunksIndex( demux_t *p_demux,
                                   mp4_track_t *p_demux_track )
{
    MP4_Box_t *p_co64; /* give offset for each chunk, same for stco and co64 */
    MP4_Box_t *p_stsc;

    unsigned int i_chunk;
    unsigned int i_index, i_last;

    if( ( !(p_co64 = MP4_BoxGet( p_demux_track->p_stbl, "stco" ) )&&
          !(p_co64 = MP4_BoxGet( p_demux_track->p_stbl, "co64" ) ) )||
        ( !(p_stsc = MP4_BoxGet( p_demux_track->p_stbl, "stsc" ) ) ))
    {
        return( VLC_EGENERIC );
    }

    p_demux_track->i_chunk_count = BOXDATA(p_co64)->i_entry_count;
    if( !p_demux_track->i_chunk_count )
    {
        msg_Warn( p_demux, "no chunk defined" );
    }
    p_demux_track->chunk = calloc( p_demux_track->i_chunk_count,
                                   sizeof( mp4_chunk_t ) );
    if( p_demux_track->chunk == NULL )
    {
        return VLC_ENOMEM;
    }

    /* first we read chunk offset */
    for( i_chunk = 0; i_chunk < p_demux_track->i_chunk_count; i_chunk++ )
    {
        mp4_chunk_t *ck = &p_demux_track->chunk[i_chunk];

        ck->i_offset = BOXDATA(p_co64)->i_chunk_offset[i_chunk];

        ck->i_first_dts = 0;
        ck->i_entries_dts = 0;
        ck->p_sample_count_dts = NULL;
        ck->p_sample_delta_dts = NULL;
        ck->i_entries_pts = 0;
        ck->p_sample_count_pts = NULL;
        ck->p_sample_offset_pts = NULL;
    }

    /* now we read index for SampleEntry( soun vide mp4a mp4v ...)
        to be used for the sample XXX begin to 1
        We construct it begining at the end */
    i_last = p_demux_track->i_chunk_count; /* last chunk proceded */
    i_index = BOXDATA(p_stsc)->i_entry_count;

    while( i_index-- > 0 )
    {
        for( i_chunk = BOXDATA(p_stsc)->i_first_chunk[i_index] - 1;
             i_chunk < i_last; i_chunk++ )
        {
            if( i_chunk >= p_demux_track->i_chunk_count )
            {
                msg_Warn( p_demux, "corrupted chunk table" );
                return VLC_EGENERIC;
            }

            p_demux_track->chunk[i_chunk].i_sample_description_index =
                    BOXDATA(p_stsc)->i_sample_description_index[i_index];
            p_demux_track->chunk[i_chunk].i_sample_count =
                    BOXDATA(p_stsc)->i_samples_per_chunk[i_index];
        }
        i_last = BOXDATA(p_stsc)->i_first_chunk[i_index] - 1;
    }

    p_demux_track->i_sample_count = 0;
    bool b_broken = false;
    if ( p_demux_track->i_chunk_count )
    {
        p_demux_track->chunk[0].i_sample_first = 0;
        p_demux_track->i_sample_count += p_demux_track->chunk[0].i_sample_count;

        const mp4_chunk_t *prev = &p_demux_track->chunk[0];
        for( i_chunk = 1; i_chunk < p_demux_track->i_chunk_count; i_chunk++ )
        {
            mp4_chunk_t *cur = &p_demux_track->chunk[i_chunk];
            if( unlikely(UINT32_MAX - cur->i_sample_count < p_demux_track->i_sample_count) )
            {
                b_broken = true;
                break;
            }
            p_demux_track->i_sample_count += cur->i_sample_count;
            cur->i_sample_first = prev->i_sample_first + prev->i_sample_count;
            prev = cur;
        }
    }

    if( unlikely(b_broken) )
    {
        msg_Err( p_demux, "Overflow in chunks total samples count" );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_demux, "track[Id 0x%x] read %d chunk",
             p_demux_track->i_track_ID, p_demux_track->i_chunk_count );

    return VLC_SUCCESS;
}

static int xTTS_CountEntries( demux_t *p_demux, uint32_t *pi_entry /* out */,
                              const uint32_t i_index,
                              uint32_t i_index_samples_left,
                              uint32_t i_sample_count,
                              const uint32_t *pi_index_sample_count,
                              const uint32_t i_table_count )
{
    uint32_t i_array_offset;
    while( i_sample_count > 0 )
    {
        if ( likely((UINT32_MAX - i_index) >= *pi_entry) )
            i_array_offset = i_index + *pi_entry;
        else
            return VLC_EGENERIC;

        if ( i_array_offset >= i_table_count )
        {
            msg_Err( p_demux, "invalid index counting total samples %u %u", i_array_offset,  i_table_count );
            return VLC_ENOVAR;
        }

        if ( i_index_samples_left )
        {
            if ( i_index_samples_left > i_sample_count )
            {
                i_index_samples_left -= i_sample_count;
                i_sample_count = 0;
                *pi_entry +=1; /* No samples left, go copy */
                break;
            }
            else
            {
                i_sample_count -= i_index_samples_left;
                i_index_samples_left = 0;
                *pi_entry += 1;
                continue;
            }
        }
        else
        {
            i_sample_count -= __MIN( i_sample_count, pi_index_sample_count[i_array_offset] );
            *pi_entry += 1;
        }
    }

    return VLC_SUCCESS;
}

static int TrackCreateSamplesIndex( demux_t *p_demux,
                                    mp4_track_t *p_demux_track )
{
    MP4_Box_t *p_box;
    MP4_Box_data_stsz_t *stsz;
    /* TODO use also stss and stsh table for seeking */
    /* FIXME use edit table */

    /* Find stsz
     *  Gives the sample size for each samples. There is also a stz2 table
     *  (compressed form) that we need to implement TODO */
    p_box = MP4_BoxGet( p_demux_track->p_stbl, "stsz" );
    if( !p_box )
    {
        /* FIXME and stz2 */
        msg_Warn( p_demux, "cannot find STSZ box" );
        return VLC_EGENERIC;
    }
    stsz = p_box->data.p_stsz;

    /* Use stsz table to create a sample number -> sample size table */
    if( p_demux_track->i_sample_count != stsz->i_sample_count )
    {
        msg_Warn( p_demux, "Incorrect total samples stsc %" PRIu32 " <> stsz %"PRIu32 ", "
                           " expect truncated media playback",
                           p_demux_track->i_sample_count, stsz->i_sample_count );
        p_demux_track->i_sample_count = __MIN(p_demux_track->i_sample_count, stsz->i_sample_count);
    }

    if( stsz->i_sample_size )
    {
        /* 1: all sample have the same size, so no need to construct a table */
        p_demux_track->i_sample_size = stsz->i_sample_size;
        p_demux_track->p_sample_size = NULL;
    }
    else
    {
        /* 2: each sample can have a different size */
        p_demux_track->i_sample_size = 0;
        p_demux_track->p_sample_size =
            calloc( p_demux_track->i_sample_count, sizeof( uint32_t ) );
        if( p_demux_track->p_sample_size == NULL )
            return VLC_ENOMEM;

        for( uint32_t i_sample = 0; i_sample < p_demux_track->i_sample_count; i_sample++ )
        {
            p_demux_track->p_sample_size[i_sample] =
                    stsz->i_entry_size[i_sample];
        }
    }

    if ( p_demux_track->i_chunk_count && p_demux_track->i_sample_size == 0 )
    {
        const mp4_chunk_t *lastchunk = &p_demux_track->chunk[p_demux_track->i_chunk_count - 1];
        if( (uint64_t)lastchunk->i_sample_count + p_demux_track->i_chunk_count - 1 > stsz->i_sample_count )
        {
            msg_Err( p_demux, "invalid samples table: stsz table is too small" );
            return VLC_EGENERIC;
        }
    }

    /* Use stts table to create a sample number -> dts table.
     * XXX: if we don't want to waste too much memory, we can't expand
     *  the box! so each chunk will contain an "extract" of this table
     *  for fast research (problem with raw stream where a sample is sometime
     *  just channels*bits_per_sample/8 */

     /* FIXME: refactor STTS & CTTS, STTS having now only few extra lines and
      *        differing in 2/2 fields and 1 signedness */

    mtime_t i_next_dts = 0;
    /* Find stts
     *  Gives mapping between sample and decoding time
     */
    p_box = MP4_BoxGet( p_demux_track->p_stbl, "stts" );
    if( !p_box )
    {
        msg_Warn( p_demux, "cannot find STTS box" );
        return VLC_EGENERIC;
    }
    else
    {
        MP4_Box_data_stts_t *stts = p_box->data.p_stts;

        msg_Warn( p_demux, "STTS table of %"PRIu32" entries", stts->i_entry_count );

        /* Create sample -> dts table per chunk */
        uint32_t i_index = 0;
        uint32_t i_current_index_samples_left = 0;

        for( uint32_t i_chunk = 0; i_chunk < p_demux_track->i_chunk_count; i_chunk++ )
        {
            mp4_chunk_t *ck = &p_demux_track->chunk[i_chunk];
            uint32_t i_sample_count;

            /* save first dts */
            ck->i_first_dts = i_next_dts;

            /* count how many entries are needed for this chunk
             * for p_sample_delta_dts and p_sample_count_dts */
            ck->i_entries_dts = 0;

            int i_ret = xTTS_CountEntries( p_demux, &ck->i_entries_dts, i_index,
                                           i_current_index_samples_left,
                                           ck->i_sample_count,
                                           stts->pi_sample_count,
                                           stts->i_entry_count );
            if ( i_ret == VLC_EGENERIC )
                return i_ret;

            /* allocate them */
            ck->p_sample_count_dts = calloc( ck->i_entries_dts, sizeof( uint32_t ) );
            ck->p_sample_delta_dts = calloc( ck->i_entries_dts, sizeof( uint32_t ) );
            if( !ck->p_sample_count_dts || !ck->p_sample_delta_dts )
            {
                free( ck->p_sample_count_dts );
                free( ck->p_sample_delta_dts );
                msg_Err( p_demux, "can't allocate memory for i_entry=%"PRIu32, ck->i_entries_dts );
                ck->i_entries_dts = 0;
                return VLC_ENOMEM;
            }

            /* now copy */
            i_sample_count = ck->i_sample_count;

            for( uint32_t i = 0; i < ck->i_entries_dts; i++ )
            {
                if ( i_current_index_samples_left )
                {
                    if ( i_current_index_samples_left > i_sample_count )
                    {
                        ck->p_sample_count_dts[i] = i_sample_count;
                        ck->p_sample_delta_dts[i] = stts->pi_sample_delta[i_index];
                        i_next_dts += ck->p_sample_count_dts[i] * stts->pi_sample_delta[i_index];
                        if ( i_sample_count ) ck->i_duration = i_next_dts - ck->i_first_dts;
                        i_current_index_samples_left -= i_sample_count;
                        i_sample_count = 0;
                        assert( i == ck->i_entries_dts - 1 );
                        break;
                    }
                    else
                    {
                        ck->p_sample_count_dts[i] = i_current_index_samples_left;
                        ck->p_sample_delta_dts[i] = stts->pi_sample_delta[i_index];
                        i_next_dts += ck->p_sample_count_dts[i] * stts->pi_sample_delta[i_index];
                        if ( i_current_index_samples_left ) ck->i_duration = i_next_dts - ck->i_first_dts;
                        i_sample_count -= i_current_index_samples_left;
                        i_current_index_samples_left = 0;
                        i_index++;
                    }
                }
                else
                {
                    if ( stts->pi_sample_count[i_index] > i_sample_count )
                    {
                        ck->p_sample_count_dts[i] = i_sample_count;
                        ck->p_sample_delta_dts[i] = stts->pi_sample_delta[i_index];
                        i_next_dts += ck->p_sample_count_dts[i] * stts->pi_sample_delta[i_index];
                        if ( i_sample_count ) ck->i_duration = i_next_dts - ck->i_first_dts;
                        i_current_index_samples_left = stts->pi_sample_count[i_index] - i_sample_count;
                        i_sample_count = 0;
                        assert( i == ck->i_entries_dts - 1 );
                        // keep building from same index
                    }
                    else
                    {
                        ck->p_sample_count_dts[i] = stts->pi_sample_count[i_index];
                        ck->p_sample_delta_dts[i] = stts->pi_sample_delta[i_index];
                        i_next_dts += ck->p_sample_count_dts[i] * stts->pi_sample_delta[i_index];
                        if ( stts->pi_sample_count[i_index] ) ck->i_duration = i_next_dts - ck->i_first_dts;
                        i_sample_count -= stts->pi_sample_count[i_index];
                        i_index++;
                    }
                }

            }
        }
    }


    /* Find ctts
     *  Gives the delta between decoding time (dts) and composition table (pts)
     */
    p_box = MP4_BoxGet( p_demux_track->p_stbl, "ctts" );
    if( p_box && p_box->data.p_ctts )
    {
        MP4_Box_data_ctts_t *ctts = p_box->data.p_ctts;

        msg_Warn( p_demux, "CTTS table of %"PRIu32" entries", ctts->i_entry_count );

        int64_t i_cts_shift = 0;
        const MP4_Box_t *p_cslg = MP4_BoxGet( p_demux_track->p_stbl, "cslg" );
        if( p_cslg && BOXDATA(p_cslg) )
            i_cts_shift = BOXDATA(p_cslg)->ct_to_dts_shift;

        /* Create pts-dts table per chunk */
        uint32_t i_index = 0;
        uint32_t i_current_index_samples_left = 0;

        for( uint32_t i_chunk = 0; i_chunk < p_demux_track->i_chunk_count; i_chunk++ )
        {
            mp4_chunk_t *ck = &p_demux_track->chunk[i_chunk];
            uint32_t i_sample_count;

            /* count how many entries are needed for this chunk
             * for p_sample_offset_pts and p_sample_count_pts */
            ck->i_entries_pts = 0;
            int i_ret = xTTS_CountEntries( p_demux, &ck->i_entries_pts, i_index,
                                           i_current_index_samples_left,
                                           ck->i_sample_count,
                                           ctts->pi_sample_count,
                                           ctts->i_entry_count );
            if ( i_ret == VLC_EGENERIC )
                return i_ret;

            /* allocate them */
            ck->p_sample_count_pts = calloc( ck->i_entries_pts, sizeof( uint32_t ) );
            ck->p_sample_offset_pts = calloc( ck->i_entries_pts, sizeof( int32_t ) );
            if( !ck->p_sample_count_pts || !ck->p_sample_offset_pts )
            {
                free( ck->p_sample_count_pts );
                free( ck->p_sample_offset_pts );
                msg_Err( p_demux, "can't allocate memory for i_entry=%"PRIu32, ck->i_entries_pts );
                ck->i_entries_pts = 0;
                return VLC_ENOMEM;
            }

            /* now copy */
            i_sample_count = ck->i_sample_count;

            for( uint32_t i = 0; i < ck->i_entries_pts; i++ )
            {
                if ( i_current_index_samples_left )
                {
                    if ( i_current_index_samples_left > i_sample_count )
                    {
                        ck->p_sample_count_pts[i] = i_sample_count;
                        ck->p_sample_offset_pts[i] = ctts->pi_sample_offset[i_index] + i_cts_shift;
                        i_current_index_samples_left -= i_sample_count;
                        i_sample_count = 0;
                        assert( i == ck->i_entries_pts - 1 );
                        break;
                    }
                    else
                    {
                        ck->p_sample_count_pts[i] = i_current_index_samples_left;
                        ck->p_sample_offset_pts[i] = ctts->pi_sample_offset[i_index] + i_cts_shift;
                        i_sample_count -= i_current_index_samples_left;
                        i_current_index_samples_left = 0;
                        i_index++;
                    }
                }
                else
                {
                    if ( ctts->pi_sample_count[i_index] > i_sample_count )
                    {
                        ck->p_sample_count_pts[i] = i_sample_count;
                        ck->p_sample_offset_pts[i] = ctts->pi_sample_offset[i_index] + i_cts_shift;
                        i_current_index_samples_left = ctts->pi_sample_count[i_index] - i_sample_count;
                        i_sample_count = 0;
                        assert( i == ck->i_entries_pts - 1 );
                        // keep building from same index
                    }
                    else
                    {
                        ck->p_sample_count_pts[i] = ctts->pi_sample_count[i_index];
                        ck->p_sample_offset_pts[i] = ctts->pi_sample_offset[i_index] + i_cts_shift;
                        i_sample_count -= ctts->pi_sample_count[i_index];
                        i_index++;
                    }
                }


            }
        }
    }

    msg_Dbg( p_demux, "track[Id 0x%x] read %"PRIu32" samples length:%"PRId64"s",
             p_demux_track->i_track_ID, p_demux_track->i_sample_count,
             i_next_dts / p_demux_track->i_timescale );

    return VLC_SUCCESS;
}


/**
 * It computes the sample rate for a video track using the given sample
 * description index
 */
static void TrackGetESSampleRate( demux_t *p_demux,
                                  unsigned *pi_num, unsigned *pi_den,
                                  const mp4_track_t *p_track,
                                  unsigned i_sd_index,
                                  unsigned i_chunk )
{
    *pi_num = 0;
    *pi_den = 0;

    MP4_Box_t *p_trak = MP4_GetTrakByTrackID( MP4_BoxGet( p_demux->p_sys->p_root, "/moov" ),
                                              p_track->i_track_ID );
    MP4_Box_t *p_mdhd = MP4_BoxGet( p_trak, "mdia/mdhd" );
    if ( p_mdhd && BOXDATA(p_mdhd) )
    {
        vlc_ureduce( pi_num, pi_den,
                     (uint64_t) BOXDATA(p_mdhd)->i_timescale * p_track->i_sample_count,
                     (uint64_t) BOXDATA(p_mdhd)->i_duration,
                     UINT16_MAX );
        return;
    }

    if( p_track->i_chunk_count == 0 )
        return;

    /* */
    const mp4_chunk_t *p_chunk = &p_track->chunk[i_chunk];
    while( p_chunk > &p_track->chunk[0] &&
           p_chunk[-1].i_sample_description_index == i_sd_index )
    {
        p_chunk--;
    }

    uint64_t i_sample = 0;
    uint64_t i_total_duration = 0;
    do
    {
        i_sample += p_chunk->i_sample_count;
        i_total_duration += p_chunk->i_duration;
        p_chunk++;
    }
    while( p_chunk < &p_track->chunk[p_track->i_chunk_count] &&
           p_chunk->i_sample_description_index == i_sd_index );

    if( i_sample > 0 && i_total_duration )
        vlc_ureduce( pi_num, pi_den,
                     i_sample * p_track->i_timescale,
                     i_total_duration,
                     UINT16_MAX);
}

/*
 * TrackCreateES:
 * Create ES and PES to init decoder if needed, for a track starting at i_chunk
 */
static int TrackCreateES( demux_t *p_demux, mp4_track_t *p_track,
                          unsigned int i_chunk, es_out_id_t **pp_es )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned int i_sample_description_index;

    if( p_sys->b_fragmented || p_track->i_chunk_count == 0 )
        i_sample_description_index = 1; /* XXX */
    else
        i_sample_description_index =
                p_track->chunk[i_chunk].i_sample_description_index;

    if( pp_es )
        *pp_es = NULL;

    if( !i_sample_description_index )
    {
        msg_Warn( p_demux, "invalid SampleEntry index (track[Id 0x%x])",
                  p_track->i_track_ID );
        return VLC_EGENERIC;
    }

    MP4_Box_t *p_sample = MP4_BoxGet(  p_track->p_stsd, "[%d]",
                            i_sample_description_index - 1 );

    if( !p_sample ||
        ( !p_sample->data.p_payload && p_track->fmt.i_cat != SPU_ES ) )
    {
        msg_Warn( p_demux, "cannot find SampleEntry (track[Id 0x%x])",
                  p_track->i_track_ID );
        return VLC_EGENERIC;
    }

    p_track->p_sample = p_sample;

    MP4_Box_t   *p_frma;
    if( ( p_frma = MP4_BoxGet( p_track->p_sample, "sinf/frma" ) ) && p_frma->data.p_frma )
    {
        msg_Warn( p_demux, "Original Format Box: %4.4s", (char *)&p_frma->data.p_frma->i_type );

        p_sample->i_type = p_frma->data.p_frma->i_type;
    }

    /* */
    switch( p_track->fmt.i_cat )
    {
    case VIDEO_ES:
        if ( p_sample->i_handler != ATOM_vide ||
             !SetupVideoES( p_demux, p_track, p_sample ) )
            return VLC_EGENERIC;

        /* Set frame rate */
        TrackGetESSampleRate( p_demux,
                              &p_track->fmt.video.i_frame_rate,
                              &p_track->fmt.video.i_frame_rate_base,
                              p_track, i_sample_description_index, i_chunk );

        p_demux->p_sys->f_fps = (float)p_track->fmt.video.i_frame_rate /
                                (float)p_track->fmt.video.i_frame_rate_base;

        break;

    case AUDIO_ES:
        if ( p_sample->i_handler != ATOM_soun ||
             !SetupAudioES( p_demux, p_track, p_sample ) )
            return VLC_EGENERIC;
        if( p_sys->p_meta )
        {
            audio_replay_gain_t *p_arg = &p_track->fmt.audio_replay_gain;
            const char *psz_meta = vlc_meta_GetExtra( p_sys->p_meta, "replaygain_track_gain" );
            if( psz_meta )
            {
                double f_gain = us_atof( psz_meta );
                p_arg->pf_gain[AUDIO_REPLAY_GAIN_TRACK] = f_gain;
                p_arg->pb_gain[AUDIO_REPLAY_GAIN_TRACK] = f_gain != 0;
            }
            psz_meta = vlc_meta_GetExtra( p_sys->p_meta, "replaygain_track_peak" );
            if( psz_meta )
            {
                double f_gain = us_atof( psz_meta );
                p_arg->pf_peak[AUDIO_REPLAY_GAIN_TRACK] = f_gain;
                p_arg->pb_peak[AUDIO_REPLAY_GAIN_TRACK] = f_gain > 0;
            }
        }
        break;

    case SPU_ES:
        if ( p_sample->i_handler != ATOM_text ||
             !SetupSpuES( p_demux, p_track, p_sample ) )
           return VLC_EGENERIC;
        break;

    default:
        break;
    }

    if( pp_es )
        *pp_es = MP4_AddTrackES( p_demux->out, p_track );

    return ( !pp_es || *pp_es ) ? VLC_SUCCESS : VLC_EGENERIC;
}

/* *** Try to find nearest sync points *** */
static int TrackGetNearestSeekPoint( demux_t *p_demux, mp4_track_t *p_track,
                                     uint32_t i_sample, uint32_t *pi_sync_sample )
{
    int i_ret = VLC_EGENERIC;
    *pi_sync_sample = 0;

    const MP4_Box_t *p_stss;
    if( ( p_stss = MP4_BoxGet( p_track->p_stbl, "stss" ) ) )
    {
        const MP4_Box_data_stss_t *p_stss_data = BOXDATA(p_stss);
        msg_Dbg( p_demux, "track[Id 0x%x] using Sync Sample Box (stss)",
                 p_track->i_track_ID );
        for( unsigned i_index = 0; i_index < p_stss_data->i_entry_count; i_index++ )
        {
            if( i_index >= p_stss_data->i_entry_count - 1 ||
                i_sample < p_stss_data->i_sample_number[i_index+1] )
            {
                *pi_sync_sample = p_stss_data->i_sample_number[i_index];
                msg_Dbg( p_demux, "stss gives %d --> %" PRIu32 " (sample number)",
                         i_sample, *pi_sync_sample );
                i_ret = VLC_SUCCESS;
                break;
            }
        }
    }

    /* try rap samples groups */
    const MP4_Box_t *p_sbgp = MP4_BoxGet( p_track->p_stbl, "sbgp" );
    for( ; p_sbgp; p_sbgp = p_sbgp->p_next )
    {
        const MP4_Box_data_sbgp_t *p_sbgp_data = BOXDATA(p_sbgp);
        if( p_sbgp->i_type != ATOM_sbgp || !p_sbgp_data )
            continue;

        if( p_sbgp_data->i_grouping_type == SAMPLEGROUP_rap )
        {
            uint32_t i_group_sample = 0;
            for ( uint32_t i=0; i<p_sbgp_data->i_entry_count; i++ )
            {
                /* Sample belongs to rap group ? */
                if( p_sbgp_data->entries.pi_group_description_index[i] != 0 )
                {
                    if( i_sample < i_group_sample )
                    {
                        msg_Dbg( p_demux, "sbgp lookup failed %" PRIu32 " (sample number)",
                                 i_sample );
                        break;
                    }
                    else if ( i_sample >= i_group_sample &&
                              *pi_sync_sample < i_group_sample )
                    {
                        *pi_sync_sample = i_group_sample;
                        i_ret = VLC_SUCCESS;
                    }
                }
                i_group_sample += p_sbgp_data->entries.pi_sample_count[i];
            }

            if( i_ret == VLC_SUCCESS && *pi_sync_sample )
            {
                msg_Dbg( p_demux, "sbgp gives %d --> %" PRIu32 " (sample number)",
                         i_sample, *pi_sync_sample );
            }
        }
    }

    return i_ret;
}

/* given a time it return sample/chunk
 * it also update elst field of the track
 */
static int TrackTimeToSampleChunk( demux_t *p_demux, mp4_track_t *p_track,
                                   int64_t i_start, uint32_t *pi_chunk,
                                   uint32_t *pi_sample )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    uint64_t     i_dts;
    unsigned int i_sample;
    unsigned int i_chunk;
    int          i_index;

    /* FIXME see if it's needed to check p_track->i_chunk_count */
    if( p_track->i_chunk_count == 0 )
        return( VLC_EGENERIC );

    /* handle elst (find the correct one) */
    MP4_TrackSetELST( p_demux, p_track, i_start );
    if( p_track->p_elst && p_track->BOXDATA(p_elst)->i_entry_count > 0 )
    {
        MP4_Box_data_elst_t *elst = p_track->BOXDATA(p_elst);
        int64_t i_mvt= MP4_rescale( i_start, CLOCK_FREQ, p_sys->i_timescale );

        /* now calculate i_start for this elst */
        /* offset */
        i_start -= MP4_rescale( p_track->i_elst_time, p_sys->i_timescale, CLOCK_FREQ );
        if( i_start < 0 )
        {
            *pi_chunk = 0;
            *pi_sample= 0;

            return VLC_SUCCESS;
        }
        /* to track time scale */
        i_start  = MP4_rescale( i_start, CLOCK_FREQ, p_track->i_timescale );
        /* add elst offset */
        if( ( elst->i_media_rate_integer[p_track->i_elst] > 0 ||
             elst->i_media_rate_fraction[p_track->i_elst] > 0 ) &&
            elst->i_media_time[p_track->i_elst] > 0 )
        {
            i_start += elst->i_media_time[p_track->i_elst];
        }

        msg_Dbg( p_demux, "elst (%d) gives %"PRId64"ms (movie)-> %"PRId64
                 "ms (track)", p_track->i_elst,
                 MP4_rescale( i_mvt, p_sys->i_timescale, 1000 ),
                 MP4_rescale( i_start, p_track->i_timescale, 1000 ) );
    }
    else
    {
        /* convert absolute time to in timescale unit */
        i_start = MP4_rescale( i_start, CLOCK_FREQ, p_track->i_timescale );
    }

    /* we start from sample 0/chunk 0, hope it won't take too much time */
    /* *** find good chunk *** */
    for( i_chunk = 0; ; i_chunk++ )
    {
        if( i_chunk + 1 >= p_track->i_chunk_count )
        {
            /* at the end and can't check if i_start in this chunk,
               it will be check while searching i_sample */
            i_chunk = p_track->i_chunk_count - 1;
            break;
        }

        if( (uint64_t)i_start >= p_track->chunk[i_chunk].i_first_dts &&
            (uint64_t)i_start <  p_track->chunk[i_chunk + 1].i_first_dts )
        {
            break;
        }
    }

    /* *** find sample in the chunk *** */
    i_sample = p_track->chunk[i_chunk].i_sample_first;
    i_dts    = p_track->chunk[i_chunk].i_first_dts;
    for( i_index = 0; i_sample < p_track->chunk[i_chunk].i_sample_count; )
    {
        if( i_dts +
            p_track->chunk[i_chunk].p_sample_count_dts[i_index] *
            p_track->chunk[i_chunk].p_sample_delta_dts[i_index] < (uint64_t)i_start )
        {
            i_dts    +=
                p_track->chunk[i_chunk].p_sample_count_dts[i_index] *
                p_track->chunk[i_chunk].p_sample_delta_dts[i_index];

            i_sample += p_track->chunk[i_chunk].p_sample_count_dts[i_index];
            i_index++;
        }
        else
        {
            if( p_track->chunk[i_chunk].p_sample_delta_dts[i_index] <= 0 )
            {
                break;
            }
            i_sample += ( i_start - i_dts ) /
                p_track->chunk[i_chunk].p_sample_delta_dts[i_index];
            break;
        }
    }

    if( i_sample >= p_track->i_sample_count )
    {
        msg_Warn( p_demux, "track[Id 0x%x] will be disabled "
                  "(seeking too far) chunk=%d sample=%d",
                  p_track->i_track_ID, i_chunk, i_sample );
        return( VLC_EGENERIC );
    }


    /* *** Try to find nearest sync points *** */
    uint32_t i_sync_sample;
    if( VLC_SUCCESS ==
        TrackGetNearestSeekPoint( p_demux, p_track, i_sample, &i_sync_sample ) )
    {
        /* Go to chunk */
        if( i_sync_sample <= i_sample )
        {
            while( i_chunk > 0 &&
                   i_sync_sample < p_track->chunk[i_chunk].i_sample_first )
                i_chunk--;
        }
        else
        {
            while( i_chunk < p_track->i_chunk_count - 1 &&
                   i_sync_sample >= p_track->chunk[i_chunk].i_sample_first +
                                    p_track->chunk[i_chunk].i_sample_count )
                i_chunk++;
        }
        i_sample = i_sync_sample;
    }

    *pi_chunk  = i_chunk;
    *pi_sample = i_sample;

    return VLC_SUCCESS;
}

static int TrackGotoChunkSample( demux_t *p_demux, mp4_track_t *p_track,
                                 unsigned int i_chunk, unsigned int i_sample )
{
    bool b_reselect = false;

    /* now see if actual es is ok */
    if( p_track->i_chunk >= p_track->i_chunk_count ||
        p_track->chunk[p_track->i_chunk].i_sample_description_index !=
            p_track->chunk[i_chunk].i_sample_description_index )
    {
        msg_Warn( p_demux, "recreate ES for track[Id 0x%x]",
                  p_track->i_track_ID );

        es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE,
                        p_track->p_es, &b_reselect );

        es_out_Del( p_demux->out, p_track->p_es );

        p_track->p_es = NULL;

        if( TrackCreateES( p_demux, p_track, i_chunk, &p_track->p_es ) )
        {
            msg_Err( p_demux, "cannot create es for track[Id 0x%x]",
                     p_track->i_track_ID );

            p_track->b_ok       = false;
            p_track->b_selected = false;
            return VLC_EGENERIC;
        }
    }

    /* select again the new decoder */
    if( b_reselect )
    {
        es_out_Control( p_demux->out, ES_OUT_SET_ES, p_track->p_es );
    }

    p_track->i_chunk    = i_chunk;
    p_track->chunk[i_chunk].i_sample = i_sample - p_track->chunk[i_chunk].i_sample_first;
    p_track->i_sample   = i_sample;

    return p_track->b_selected ? VLC_SUCCESS : VLC_EGENERIC;
}
#if 0
static void MP4_TrackRestart( demux_t *p_demux, mp4_track_t *p_track,
                              MP4_Box_t *p_params_box )
{
    bool b_reselect = false;
    if( p_track->p_es )
    {
        es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE,
                        p_track->p_es, &b_reselect );
    }

    /* Save previous fragmented pos */
    uint32_t i_sample_pos_backup = p_track->i_sample;
    mtime_t time_backup = p_track->i_time;
    uint32_t timescale_backup = p_track->i_timescale;

    /* Save previous format and ES */
    es_format_t fmtbackup;
    es_out_id_t *p_es_backup = p_track->p_es;
    p_track->p_es = NULL;
    es_format_Copy( &fmtbackup, &p_track->fmt );
    es_format_Clean( &p_track->fmt );


    /* do the cleanup and recycle track / restart */
    MP4_TrackDestroy( p_demux, p_track );
    memset( p_track, 0, sizeof(*p_track) );

    assert(p_params_box->i_type == ATOM_trak);
    MP4_TrackCreate( p_demux, p_track, p_params_box, false, true );

    if( p_track->b_ok )
    {
        if( !es_format_IsSimilar( &fmtbackup, &p_track->fmt ) ||
            fmtbackup.i_extra != p_track->fmt.i_extra ||
            memcmp( fmtbackup.p_extra, p_track->fmt.p_extra, fmtbackup.i_extra ) )
        {
            if( p_es_backup )
                es_out_Del( p_demux->out, p_es_backup );

            if( !p_track->b_chapters_source )
            {
                p_track->p_es = MP4_AddTrackES( p_demux->out, p_track );
                p_track->b_ok = !!p_track->p_es;
            }
        }
        else
        {
            p_track->p_es = p_es_backup;
        }
    }
    else if( p_es_backup )
    {
        es_out_Del( p_demux->out, p_es_backup );
    }

    /* select again the new decoder */
    if( b_reselect && p_track->p_es )
        es_out_Control( p_demux->out, ES_OUT_SET_ES, p_track->p_es );

    es_format_Clean( &fmtbackup );

    /* Restore fragmented pos */
    p_track->i_sample = i_sample_pos_backup;
    p_track->i_time = MP4_rescale( time_backup, timescale_backup, p_track->i_timescale );
}
#endif
/****************************************************************************
 * MP4_TrackSetup:
 ****************************************************************************
 * Parse track information and create all needed data to run a track
 * If it succeed b_ok is set to 1 else to 0
 ****************************************************************************/
static void MP4_TrackSetup( demux_t *p_demux, mp4_track_t *p_track,
                             MP4_Box_t *p_box_trak,
                             bool b_create_es, bool b_force_enable )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    p_track->p_track = p_box_trak;

    char language[4] = { '\0' };
    char sdp_media_type[8] = { '\0' };

    const MP4_Box_t *p_tkhd = MP4_BoxGet( p_box_trak, "tkhd" );
    if( !p_tkhd )
    {
        return;
    }

    /* do we launch this track by default ? */
    p_track->b_enable =
        ( ( BOXDATA(p_tkhd)->i_flags&MP4_TRACK_ENABLED ) != 0 );
    if( !p_track->b_enable )
        p_track->fmt.i_priority = ES_PRIORITY_NOT_DEFAULTABLE;

    p_track->i_track_ID = BOXDATA(p_tkhd)->i_track_ID;

    p_track->i_width = BOXDATA(p_tkhd)->i_width / BLOCK16x16;
    p_track->i_height = BOXDATA(p_tkhd)->i_height / BLOCK16x16;
    p_track->f_rotation = BOXDATA(p_tkhd)->f_rotation;

    /* FIXME: unhandled box: tref */

    const MP4_Box_t *p_mdhd = MP4_BoxGet( p_box_trak, "mdia/mdhd" );
    const MP4_Box_t *p_hdlr = MP4_BoxGet( p_box_trak, "mdia/hdlr" );

    if( ( !p_mdhd )||( !p_hdlr ) )
    {
        return;
    }

    if( BOXDATA(p_mdhd)->i_timescale == 0 )
    {
        msg_Warn( p_demux, "Invalid track timescale " );
        return;
    }
    p_track->i_timescale = BOXDATA(p_mdhd)->i_timescale;

    memcpy( &language, BOXDATA(p_mdhd)->rgs_language, 3 );
    p_track->b_mac_encoding = BOXDATA(p_mdhd)->b_mac_encoding;

    switch( p_hdlr->data.p_hdlr->i_handler_type )
    {
        case( ATOM_soun ):
            if( !MP4_BoxGet( p_box_trak, "mdia/minf/smhd" ) )
            {
                return;
            }
            es_format_Change( &p_track->fmt, AUDIO_ES, 0 );
            break;

        case( ATOM_vide ):
            if( !MP4_BoxGet( p_box_trak, "mdia/minf/vmhd") )
            {
                return;
            }
            es_format_Change( &p_track->fmt, VIDEO_ES, 0 );
            break;

        case( ATOM_hint ):
            /* RTP Reception Hint tracks */
            if( !MP4_BoxGet( p_box_trak, "mdia/minf/hmhd" ) ||
                !MP4_BoxGet( p_box_trak, "mdia/minf/stbl/stsd/rrtp" ) )
            {
                break;
            }
            MP4_Box_t *p_sdp;

            /* parse the sdp message to find out whether the RTP stream contained audio or video */
            if( !( p_sdp  = MP4_BoxGet( p_box_trak, "udta/hnti/sdp " ) ) )
            {
                msg_Warn( p_demux, "Didn't find sdp box to determine stream type" );
                return;
            }

            memcpy( sdp_media_type, BOXDATA(p_sdp)->psz_text, 7 );
            if( !strcmp(sdp_media_type, "m=audio") )
            {
                msg_Dbg( p_demux, "Found audio Rtp: %s", sdp_media_type );
                es_format_Change( &p_track->fmt, AUDIO_ES, 0 );
            }
            else if( !strcmp(sdp_media_type, "m=video") )
            {
                msg_Dbg( p_demux, "Found video Rtp: %s", sdp_media_type );
                es_format_Change( &p_track->fmt, VIDEO_ES, 0 );
            }
            else
            {
                msg_Warn( p_demux, "Malformed track SDP message: %s", sdp_media_type );
                return;
            }
            p_track->p_sdp = p_sdp;
            break;

        case( ATOM_tx3g ):
        case( ATOM_text ):
        case( ATOM_subp ):
        case( ATOM_subt ): /* ttml */
        case( ATOM_sbtl ):
        case( ATOM_clcp ): /* closed captions */
            es_format_Change( &p_track->fmt, SPU_ES, 0 );
            break;

        default:
            return;
    }

    p_track->asfinfo.i_cat = p_track->fmt.i_cat;

    const MP4_Box_t *p_elst;
    p_track->i_elst = 0;
    p_track->i_elst_time = 0;
    if( ( p_track->p_elst = p_elst = MP4_BoxGet( p_box_trak, "edts/elst" ) ) )
    {
        MP4_Box_data_elst_t *elst = BOXDATA(p_elst);
        unsigned int i;

        msg_Warn( p_demux, "elst box found" );
        for( i = 0; i < elst->i_entry_count; i++ )
        {
            msg_Dbg( p_demux, "   - [%d] duration=%"PRId64"ms media time=%"PRId64
                     "ms) rate=%d.%d", i,
                     MP4_rescale( elst->i_segment_duration[i], p_sys->i_timescale, 1000 ),
                     elst->i_media_time[i] >= 0 ?
                        MP4_rescale( elst->i_media_time[i], p_track->i_timescale, 1000 ) :
                        INT64_C(-1),
                     elst->i_media_rate_integer[i],
                     elst->i_media_rate_fraction[i] );
        }
    }


/*  TODO
    add support for:
    p_dinf = MP4_BoxGet( p_minf, "dinf" );
*/
    if( !( p_track->p_stbl = MP4_BoxGet( p_box_trak,"mdia/minf/stbl" ) ) ||
        !( p_track->p_stsd = MP4_BoxGet( p_box_trak,"mdia/minf/stbl/stsd") ) )
    {
        return;
    }

    /* Set language */
    if( *language && strcmp( language, "```" ) && strcmp( language, "und" ) )
    {
        p_track->fmt.psz_language = strdup( language );
    }

    const MP4_Box_t *p_udta = MP4_BoxGet( p_box_trak, "udta" );
    if( p_udta )
    {
        const MP4_Box_t *p_box_iter;
        for( p_box_iter = p_udta->p_first; p_box_iter != NULL;
                 p_box_iter = p_box_iter->p_next )
        {
            switch( p_box_iter->i_type )
            {
                case ATOM_0xa9nam:
                case ATOM_name:
                    p_track->fmt.psz_description =
                        strndup( p_box_iter->data.p_binary->p_blob,
                                 p_box_iter->data.p_binary->i_blob );
                default:
                    break;
            }
        }
    }

    /* Create chunk index table and sample index table */
    if( TrackCreateChunksIndex( p_demux,p_track  ) ||
        TrackCreateSamplesIndex( p_demux, p_track ) )
    {
        msg_Err( p_demux, "cannot create chunks index" );
        return; /* cannot create chunks index */
    }

    p_track->i_chunk  = 0;
    p_track->i_sample = 0;

    /* Mark chapter only track */
    if( p_sys->p_tref_chap )
    {
        MP4_Box_data_tref_generic_t *p_chap = p_sys->p_tref_chap->data.p_tref_generic;
        unsigned int i;

        for( i = 0; i < p_chap->i_entry_count; i++ )
        {
            if( p_track->i_track_ID == p_chap->i_track_ID[i] &&
                p_track->fmt.i_cat == UNKNOWN_ES )
            {
                p_track->b_chapters_source = true;
                p_track->b_enable = false;
                break;
            }
        }
    }

    const MP4_Box_t *p_tsel;
    /* now create es */
    if( b_force_enable &&
        ( p_track->fmt.i_cat == VIDEO_ES || p_track->fmt.i_cat == AUDIO_ES ) )
    {
        msg_Warn( p_demux, "Enabling track[Id 0x%x] (buggy file without enabled track)",
                  p_track->i_track_ID );
        p_track->b_enable = true;
        p_track->fmt.i_priority = ES_PRIORITY_SELECTABLE_MIN;
    }
    else if ( (p_tsel = MP4_BoxGet( p_box_trak, "udta/tsel" )) )
    {
        if ( BOXDATA(p_tsel) && BOXDATA(p_tsel)->i_switch_group )
        {
            p_track->i_switch_group = BOXDATA(p_tsel)->i_switch_group;
            int i_priority = ES_PRIORITY_SELECTABLE_MIN;
            for ( unsigned int i = 0; i < p_sys->i_tracks; i++ )
            {
                const mp4_track_t *p_other = &p_sys->track[i];
                if( p_other && p_other != p_track &&
                    p_other->fmt.i_cat == p_track->fmt.i_cat &&
                    p_track->i_switch_group == p_other->i_switch_group )
                        i_priority = __MAX( i_priority, p_other->fmt.i_priority + 1 );
            }
            /* VLC only support ES priority for AUDIO_ES and SPU_ES.
               If there's another VIDEO_ES in the same group, we need to unselect it then */
            if ( p_track->fmt.i_cat == VIDEO_ES && i_priority > ES_PRIORITY_SELECTABLE_MIN )
                p_track->fmt.i_priority = ES_PRIORITY_NOT_DEFAULTABLE;
            else
                p_track->fmt.i_priority = i_priority;
        }
    }
    /* If there's no tsel, try to enable the track coming first in edit list */
    else if ( p_track->p_elst && p_track->fmt.i_priority == ES_PRIORITY_SELECTABLE_MIN )
    {
#define MAX_SELECTABLE (INT_MAX - ES_PRIORITY_SELECTABLE_MIN)
        for ( uint32_t i=0; i<p_track->BOXDATA(p_elst)->i_entry_count; i++ )
        {
            if ( p_track->BOXDATA(p_elst)->i_media_time[i] >= 0 &&
                 p_track->BOXDATA(p_elst)->i_segment_duration[i] )
            {
                /* We do selection by inverting start time into priority.
                   The track with earliest edit will have the highest prio */
                const int i_time = __MIN( MAX_SELECTABLE, p_track->BOXDATA(p_elst)->i_media_time[i] );
                p_track->fmt.i_priority = ES_PRIORITY_SELECTABLE_MIN + MAX_SELECTABLE - i_time;
                break;
            }
        }
    }

    if( p_sys->hacks.es_cat_filters && (p_sys->hacks.es_cat_filters & p_track->fmt.i_cat) == 0 )
    {
        p_track->fmt.i_priority = ES_PRIORITY_NOT_DEFAULTABLE;
    }

    if( TrackCreateES( p_demux,
                       p_track, p_track->i_chunk,
                      (p_track->b_chapters_source || !b_create_es) ? NULL : &p_track->p_es ) )
    {
        msg_Err( p_demux, "cannot create es for track[Id 0x%x]",
                 p_track->i_track_ID );
        return;
    }

    p_track->b_ok = true;
}

static void DestroyChunk( mp4_chunk_t *ck )
{
    free( ck->p_sample_count_dts );
    free( ck->p_sample_delta_dts );
    free( ck->p_sample_count_pts );
    free( ck->p_sample_offset_pts );
    free( ck->p_sample_size );
}

/****************************************************************************
 * MP4_TrackClean:
 ****************************************************************************
 * Cleans a track created by MP4_TrackCreate.
 ****************************************************************************/
static void MP4_TrackClean( es_out_t *out, mp4_track_t *p_track )
{
    es_format_Clean( &p_track->fmt );

    if( p_track->p_es )
        es_out_Del( out, p_track->p_es );

    if( p_track->chunk )
    {
        for( unsigned int i_chunk = 0; i_chunk < p_track->i_chunk_count; i_chunk++ )
            DestroyChunk( &p_track->chunk[i_chunk] );
    }
    free( p_track->chunk );

    if( !p_track->i_sample_size )
        free( p_track->p_sample_size );

    if ( p_track->asfinfo.p_frame )
        block_ChainRelease( p_track->asfinfo.p_frame );

    free( p_track->context.runs.p_array );
}

static void MP4_TrackInit( mp4_track_t *p_track )
{
    memset( p_track, 0, sizeof(mp4_track_t) );
    es_format_Init( &p_track->fmt, UNKNOWN_ES, 0 );
    p_track->i_timescale = 1;
}

static void MP4_TrackSelect( demux_t *p_demux, mp4_track_t *p_track, bool b_select )
{
    if( !p_track->b_ok || p_track->b_chapters_source )
        return;

    if( b_select == p_track->b_selected )
        return;

    if( !b_select && p_track->p_es )
    {
        es_out_Control( p_demux->out, ES_OUT_SET_ES_STATE,
                        p_track->p_es, false );
    }

    p_track->b_selected = b_select;
}

static int MP4_TrackSeek( demux_t *p_demux, mp4_track_t *p_track,
                          mtime_t i_start )
{
    uint32_t i_chunk;
    uint32_t i_sample;

    if( !p_track->b_ok || p_track->b_chapters_source )
        return VLC_EGENERIC;

    p_track->b_selected = false;

    if( TrackTimeToSampleChunk( p_demux, p_track, i_start,
                                &i_chunk, &i_sample ) )
    {
        msg_Warn( p_demux, "cannot select track[Id 0x%x]",
                  p_track->i_track_ID );
        return VLC_EGENERIC;
    }

    p_track->b_selected = true;
    if( !TrackGotoChunkSample( p_demux, p_track, i_chunk, i_sample ) )
        p_track->b_selected = true;

    p_track->i_next_block_flags |= BLOCK_FLAG_DISCONTINUITY;

    return p_track->b_selected ? VLC_SUCCESS : VLC_EGENERIC;
}


/*
 * 3 types: for audio
 *
 */
static inline uint32_t MP4_GetFixedSampleSize( const mp4_track_t *p_track,
                                               const MP4_Box_data_sample_soun_t *p_soun )
{
    uint32_t i_size = p_track->i_sample_size;

    assert( p_track->i_sample_size != 0 );

     /* QuickTime "built-in" support case fixups */
    if( p_track->fmt.i_cat == AUDIO_ES &&
        p_soun->i_compressionid == 0 && p_track->i_sample_size <= 2 )
    {
        switch( p_track->fmt.i_codec )
        {
        case VLC_CODEC_GSM:
            i_size = p_soun->i_channelcount;
            break;
        case VLC_FOURCC( 'N', 'O', 'N', 'E' ):
        case ATOM_twos:
        case ATOM_sowt:
        case ATOM_raw:
        case VLC_CODEC_S24L:
        case VLC_CODEC_S24B:
        case VLC_CODEC_S32L:
        case VLC_CODEC_S32B:
        case VLC_CODEC_F32L:
        case VLC_CODEC_F32B:
        case VLC_CODEC_F64L:
        case VLC_CODEC_F64B:
            if( p_track->i_sample_size < ((p_soun->i_samplesize+7U)/8U) * p_soun->i_channelcount )
                i_size = ((p_soun->i_samplesize+7)/8) * p_soun->i_channelcount;
            break;
        case VLC_CODEC_ALAW:
        case VLC_FOURCC( 'u', 'l', 'a', 'w' ):
            i_size = p_soun->i_channelcount;
            break;
        default:
            break;
        }
    }

    return i_size;
}

static uint32_t MP4_TrackGetReadSize( mp4_track_t *p_track, uint32_t *pi_nb_samples )
{
    uint32_t i_size = 0;
    *pi_nb_samples = 0;

    if ( p_track->i_sample == p_track->i_sample_count )
        return 0;

    if ( p_track->fmt.i_cat != AUDIO_ES )
    {
        *pi_nb_samples = 1;

        if( p_track->i_sample_size == 0 ) /* all sizes are different */
            return p_track->p_sample_size[p_track->i_sample];
        else
            return p_track->i_sample_size;
    }
    else
    {
        const MP4_Box_data_sample_soun_t *p_soun = p_track->p_sample->data.p_sample_soun;
        const mp4_chunk_t *p_chunk = &p_track->chunk[p_track->i_chunk];
        uint32_t i_max_samples = p_chunk->i_sample_count - p_chunk->i_sample;

        /* Group audio packets so we don't call demux for single sample unit */
        if( p_track->fmt.i_original_fourcc == VLC_CODEC_DVD_LPCM &&
            p_soun->i_constLPCMframesperaudiopacket &&
            p_soun->i_constbytesperaudiopacket )
        {
            /* uncompressed case */
            uint32_t i_packets = i_max_samples / p_soun->i_constLPCMframesperaudiopacket;
            if ( UINT32_MAX / p_soun->i_constbytesperaudiopacket < i_packets )
                i_packets = UINT32_MAX / p_soun->i_constbytesperaudiopacket;
            *pi_nb_samples = i_packets * p_soun->i_constLPCMframesperaudiopacket;
            return i_packets * p_soun->i_constbytesperaudiopacket;
        }

        if( p_track->fmt.i_original_fourcc == VLC_FOURCC('r','r','t','p') )
        {
            *pi_nb_samples = 1;
            return p_track->i_sample_size;
        }

        /* all samples have a different size */
        if( p_track->i_sample_size == 0 )
        {
            *pi_nb_samples = 1;
            return p_track->p_sample_size[p_track->i_sample];
        }

        if( p_soun->i_qt_version == 1 )
        {
            if ( p_soun->i_compressionid == 0xFFFE )
            {
                *pi_nb_samples = 1; /* != number of audio samples */
                if ( p_track->i_sample_size )
                    return p_track->i_sample_size;
                else
                    return p_track->p_sample_size[p_track->i_sample];
            }
            else if ( p_soun->i_compressionid != 0 || p_soun->i_bytes_per_sample > 1 ) /* compressed */
            {
                /* in this case we are dealing with compressed data
                   -2 in V1: additional fields are meaningless (VBR and such) */
                *pi_nb_samples = i_max_samples;//p_track->chunk[p_track->i_chunk].i_sample_count;
                if( p_track->fmt.audio.i_blockalign > 1 )
                    *pi_nb_samples = p_soun->i_sample_per_packet;
                i_size = *pi_nb_samples / p_soun->i_sample_per_packet * p_soun->i_bytes_per_frame;
                return i_size;
            }
            else /* uncompressed case */
            {
                uint32_t i_packets;
                if( p_track->fmt.audio.i_blockalign > 1 )
                    i_packets = 1;
                else
                    i_packets = i_max_samples / p_soun->i_sample_per_packet;

                if ( UINT32_MAX / p_soun->i_bytes_per_frame < i_packets )
                    i_packets = UINT32_MAX / p_soun->i_bytes_per_frame;

                *pi_nb_samples = i_packets * p_soun->i_sample_per_packet;
                i_size = i_packets * p_soun->i_bytes_per_frame;
                return i_size;
            }
        }

        /* uncompressed v0 (qt) or... not (ISO) */

        /* Quicktime built-in support handling */
        if( p_soun->i_compressionid == 0 && p_track->i_sample_size == 1 )
        {
            switch( p_track->fmt.i_codec )
            {
                /* sample size is not integer */
                case VLC_CODEC_GSM:
                    *pi_nb_samples = 160 * p_track->fmt.audio.i_channels;
                    return 33 * p_track->fmt.audio.i_channels;
                default:
                    break;
            }
        }

        /* More regular V0 cases */
        uint32_t i_max_v0_samples;
        switch( p_track->fmt.i_codec )
        {
            /* Compressed samples in V0 */
            case VLC_CODEC_AMR_NB:
            case VLC_CODEC_AMR_WB:
                i_max_v0_samples = 16;
                break;
            case VLC_CODEC_MPGA:
            case VLC_CODEC_MP2:
            case VLC_CODEC_MP3:
            case VLC_CODEC_DTS:
            case VLC_CODEC_MP4A:
            case VLC_CODEC_A52:
                i_max_v0_samples = 1;
                break;
                /* fixme, reverse using a list of uncompressed codecs */
            default:
                /* Read 25ms of samples (uncompressed) */
                i_max_v0_samples = p_track->fmt.audio.i_rate / 40 *
                                   p_track->fmt.audio.i_channels;
                if( i_max_v0_samples < 1 )
                    i_max_v0_samples = 1;
                break;
        }

        *pi_nb_samples = 0;
        for( uint32_t i=p_track->i_sample;
             i<p_chunk->i_sample_first+p_chunk->i_sample_count &&
             i<p_track->i_sample_count;
             i++ )
        {
            (*pi_nb_samples)++;
            if ( p_track->i_sample_size == 0 )
                i_size += p_track->p_sample_size[i];
            else
                i_size += MP4_GetFixedSampleSize( p_track, p_soun );

            /* Try to detect compression in ISO */
            if(p_soun->i_compressionid != 0)
            {
                /* Return only 1 sample */
                break;
            }

            if ( *pi_nb_samples == i_max_v0_samples )
                break;
        }
    }

    //fprintf( stderr, "size=%d\n", i_size );
    return i_size;
}

static uint64_t MP4_TrackGetPos( mp4_track_t *p_track )
{
    unsigned int i_sample;
    uint64_t i_pos;

    i_pos = p_track->chunk[p_track->i_chunk].i_offset;

    if( p_track->i_sample_size )
    {
        MP4_Box_data_sample_soun_t *p_soun =
            p_track->p_sample->data.p_sample_soun;

        /* Quicktime builtin support, _must_ ignore sample tables */
        if( p_track->fmt.i_cat == AUDIO_ES && p_soun->i_compressionid == 0 &&
            p_track->i_sample_size == 1 )
        {
            switch( p_track->fmt.i_codec )
            {
            case VLC_CODEC_GSM: /* # Samples > data size */
                i_pos += ( p_track->i_sample -
                           p_track->chunk[p_track->i_chunk].i_sample_first ) / 160 * 33;
                return i_pos;
            default:
                break;
            }
        }

        if( p_track->fmt.i_cat != AUDIO_ES || p_soun->i_qt_version == 0 ||
            p_track->fmt.audio.i_blockalign <= 1 ||
            p_soun->i_sample_per_packet * p_soun->i_bytes_per_frame == 0 )
        {
            i_pos += ( p_track->i_sample -
                       p_track->chunk[p_track->i_chunk].i_sample_first ) *
                     MP4_GetFixedSampleSize( p_track, p_soun );
        }
        else
        {
            /* we read chunk by chunk unless a blockalign is requested */
            i_pos += ( p_track->i_sample - p_track->chunk[p_track->i_chunk].i_sample_first ) /
                        p_soun->i_sample_per_packet * p_soun->i_bytes_per_frame;
        }
    }
    else
    {
        for( i_sample = p_track->chunk[p_track->i_chunk].i_sample_first;
             i_sample < p_track->i_sample; i_sample++ )
        {
            i_pos += p_track->p_sample_size[i_sample];
        }
    }

    return i_pos;
}

static int MP4_TrackNextSample( demux_t *p_demux, mp4_track_t *p_track, uint32_t i_samples )
{
    if ( UINT32_MAX - p_track->i_sample < i_samples )
    {
        p_track->i_sample = UINT32_MAX;
        return VLC_EGENERIC;
    }

    p_track->i_sample += i_samples;

    if( p_track->i_sample >= p_track->i_sample_count )
        return VLC_EGENERIC;

    /* Have we changed chunk ? */
    if( p_track->i_sample >=
            p_track->chunk[p_track->i_chunk].i_sample_first +
            p_track->chunk[p_track->i_chunk].i_sample_count )
    {
        if( TrackGotoChunkSample( p_demux, p_track, p_track->i_chunk + 1,
                                  p_track->i_sample ) )
        {
            msg_Warn( p_demux, "track[0x%x] will be disabled "
                      "(cannot restart decoder)", p_track->i_track_ID );
            MP4_TrackSelect( p_demux, p_track, false );
            return VLC_EGENERIC;
        }
    }

    /* Have we changed elst */
    if( p_track->p_elst && p_track->BOXDATA(p_elst)->i_entry_count > 0 )
    {
        demux_sys_t *p_sys = p_demux->p_sys;
        MP4_Box_data_elst_t *elst = p_track->BOXDATA(p_elst);
        uint64_t i_mvt = MP4_rescale( MP4_TrackGetDTS( p_demux, p_track ),
                                      CLOCK_FREQ, p_sys->i_timescale );
        if( (unsigned int)p_track->i_elst < elst->i_entry_count &&
            i_mvt >= p_track->i_elst_time +
                     elst->i_segment_duration[p_track->i_elst] )
        {
            MP4_TrackSetELST( p_demux, p_track,
                              MP4_TrackGetDTS( p_demux, p_track ) );
        }
    }

    return VLC_SUCCESS;
}

static void MP4_TrackSetELST( demux_t *p_demux, mp4_track_t *tk,
                              int64_t i_time )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int         i_elst_last = tk->i_elst;

    /* handle elst (find the correct one) */
    tk->i_elst      = 0;
    tk->i_elst_time = 0;
    if( tk->p_elst && tk->BOXDATA(p_elst)->i_entry_count > 0 )
    {
        MP4_Box_data_elst_t *elst = tk->BOXDATA(p_elst);
        int64_t i_mvt= MP4_rescale( i_time, CLOCK_FREQ, p_sys->i_timescale );

        for( tk->i_elst = 0; (unsigned int)tk->i_elst < elst->i_entry_count; tk->i_elst++ )
        {
            mtime_t i_dur = elst->i_segment_duration[tk->i_elst];

            if( tk->i_elst_time <= i_mvt && i_mvt < tk->i_elst_time + i_dur )
            {
                break;
            }
            tk->i_elst_time += i_dur;
        }

        if( (unsigned int)tk->i_elst >= elst->i_entry_count )
        {
            /* msg_Dbg( p_demux, "invalid number of entry in elst" ); */
            tk->i_elst = elst->i_entry_count - 1;
            tk->i_elst_time -= elst->i_segment_duration[tk->i_elst];
        }

        if( elst->i_media_time[tk->i_elst] < 0 )
        {
            /* track offset */
            tk->i_elst_time += elst->i_segment_duration[tk->i_elst];
        }
    }
    if( i_elst_last != tk->i_elst )
    {
        msg_Warn( p_demux, "elst old=%d new=%d", i_elst_last, tk->i_elst );
        tk->i_next_block_flags |= BLOCK_FLAG_DISCONTINUITY;
    }
}

/******************************************************************************
 *     Here are the functions used for fragmented MP4
 *****************************************************************************/
#if 0
/**
 * Re-init decoder.
 * \Note If we call that function too soon,
 * before the track has been selected by MP4_TrackSelect
 * (during the first execution of Demux), then the track gets disabled
 */
static int ReInitDecoder( demux_t *p_demux, const MP4_Box_t *p_root,
                          mp4_track_t *p_track )
{
    MP4_Box_t *p_paramsbox = MP4_BoxGet( p_root, "/moov/trak[0]" );
    if( !p_paramsbox )
        return VLC_EGENERIC;

    MP4_TrackRestart( p_demux, p_track, p_paramsbox );

    /* Temporary hack until we support track selection */
    p_track->b_selected = true;
    p_track->b_enable = true;

    return VLC_SUCCESS;
}
#endif

static stime_t GetCumulatedDuration( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    stime_t i_max_duration = 0;

    for ( unsigned int i=0; i<p_sys->i_tracks; i++ )
    {
        stime_t i_track_duration = 0;
        MP4_Box_t *p_trak = MP4_GetTrakByTrackID( p_sys->p_moov, p_sys->track[i].i_track_ID );
        const MP4_Box_t *p_stsz;
        const MP4_Box_t *p_tkhd;
        if ( (p_tkhd = MP4_BoxGet( p_trak, "tkhd" )) &&
             (p_stsz = MP4_BoxGet( p_trak, "mdia/minf/stbl/stsz" )) &&
             /* duration might be wrong an be set to whole duration :/ */
             BOXDATA(p_stsz)->i_sample_count > 0 )
        {
            i_max_duration = __MAX( (uint64_t)i_max_duration, BOXDATA(p_tkhd)->i_duration );
        }

        if( p_sys->p_fragsindex )
        {
            i_track_duration += MP4_Fragment_Index_GetTrackDuration( p_sys->p_fragsindex, i );
        }

        i_max_duration = __MAX( i_max_duration, i_track_duration );
    }

    return i_max_duration;
}

static int ProbeIndex( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    uint64_t i_stream_size;
    uint8_t mfro[MP4_MFRO_BOXSIZE];
    assert( p_sys->b_seekable );

    if ( MP4_BoxCount( p_sys->p_root, "/mfra" ) )
        return VLC_EGENERIC;

    i_stream_size = stream_Size( p_demux->s );
    if ( ( i_stream_size >> 62 ) ||
         ( i_stream_size < MP4_MFRO_BOXSIZE ) ||
         ( vlc_stream_Seek( p_demux->s, i_stream_size - MP4_MFRO_BOXSIZE ) != VLC_SUCCESS )
       )
    {
        msg_Dbg( p_demux, "Probing tail for mfro has failed" );
        return VLC_EGENERIC;
    }

    if ( vlc_stream_Read( p_demux->s, &mfro, MP4_MFRO_BOXSIZE ) == MP4_MFRO_BOXSIZE &&
         VLC_FOURCC(mfro[4],mfro[5],mfro[6],mfro[7]) == ATOM_mfro &&
         GetDWBE( &mfro ) == MP4_MFRO_BOXSIZE )
    {
        uint32_t i_offset = GetDWBE( &mfro[12] );
        msg_Dbg( p_demux, "will read mfra index at %"PRIu64, i_stream_size - i_offset );
        if ( i_stream_size > i_offset &&
             vlc_stream_Seek( p_demux->s, i_stream_size - i_offset ) == VLC_SUCCESS )
        {
            msg_Dbg( p_demux, "reading mfra index at %"PRIu64, i_stream_size - i_offset );
            const uint32_t stoplist[] = { ATOM_mfra, 0 };
            MP4_ReadBoxContainerChildren( p_demux->s, p_sys->p_root, stoplist );
        }
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

static stime_t GetMoovTrackDuration( demux_sys_t *p_sys, unsigned i_track_ID )
{
    MP4_Box_t *p_trak = MP4_GetTrakByTrackID( p_sys->p_moov, i_track_ID );
    const MP4_Box_t *p_stsz;
    const MP4_Box_t *p_tkhd;
    if ( (p_tkhd = MP4_BoxGet( p_trak, "tkhd" )) &&
         (p_stsz = MP4_BoxGet( p_trak, "mdia/minf/stbl/stsz" )) &&
         /* duration might be wrong an be set to whole duration :/ */
         BOXDATA(p_stsz)->i_sample_count > 0 )
    {
        return BOXDATA(p_tkhd)->i_duration; /* In movie / mvhd scale */
    }
    return 0;
}

static bool GetMoofTrackDuration( MP4_Box_t *p_moov, MP4_Box_t *p_moof,
                                  unsigned i_track_ID, stime_t *p_duration )
{
    if ( !p_moof || !p_moov )
        return false;

    MP4_Box_t *p_traf = MP4_BoxGet( p_moof, "traf" );
    while ( p_traf )
    {
        if ( p_traf->i_type != ATOM_traf )
        {
           p_traf = p_traf->p_next;
           continue;
        }

        const MP4_Box_t *p_tfhd = MP4_BoxGet( p_traf, "tfhd" );
        const MP4_Box_t *p_trun = MP4_BoxGet( p_traf, "trun" );
        if ( !p_tfhd || !p_trun || i_track_ID != BOXDATA(p_tfhd)->i_track_ID )
        {
           p_traf = p_traf->p_next;
           continue;
        }

        uint32_t i_track_timescale = 0;
        uint32_t i_track_defaultsampleduration = 0;

        /* set trex for defaults */
        MP4_Box_t *p_trex = MP4_GetTrexByTrackID( p_moov, BOXDATA(p_tfhd)->i_track_ID );
        if ( p_trex )
        {
            i_track_defaultsampleduration = BOXDATA(p_trex)->i_default_sample_duration;
        }

        MP4_Box_t *p_trak = MP4_GetTrakByTrackID( p_moov, BOXDATA(p_tfhd)->i_track_ID );
        if ( p_trak )
        {
            MP4_Box_t *p_mdhd = MP4_BoxGet( p_trak, "mdia/mdhd" );
            if ( p_mdhd )
                i_track_timescale = BOXDATA(p_mdhd)->i_timescale;
        }

        if ( !i_track_timescale )
        {
           p_traf = p_traf->p_next;
           continue;
        }

        uint64_t i_traf_duration = 0;
        while ( p_trun && p_tfhd )
        {
            if ( p_trun->i_type != ATOM_trun )
            {
               p_trun = p_trun->p_next;
               continue;
            }
            const MP4_Box_data_trun_t *p_trundata = p_trun->data.p_trun;

            /* Sum total time */
            if ( p_trundata->i_flags & MP4_TRUN_SAMPLE_DURATION )
            {
                for( uint32_t i=0; i< p_trundata->i_sample_count; i++ )
                    i_traf_duration += p_trundata->p_samples[i].i_duration;
            }
            else if ( BOXDATA(p_tfhd)->i_flags & MP4_TFHD_DFLT_SAMPLE_DURATION )
            {
                i_traf_duration += p_trundata->i_sample_count *
                        BOXDATA(p_tfhd)->i_default_sample_duration;
            }
            else
            {
                i_traf_duration += p_trundata->i_sample_count *
                        i_track_defaultsampleduration;
            }

            p_trun = p_trun->p_next;
        }

        *p_duration = i_traf_duration;
        break;
    }

    return true;
}

static int ProbeFragments( demux_t *p_demux, bool b_force, bool *pb_fragmented )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    msg_Dbg( p_demux, "probing fragments from %"PRId64, vlc_stream_Tell( p_demux->s ) );

    assert( p_sys->p_root );

    MP4_Box_t *p_vroot = MP4_BoxNew(ATOM_root);
    if( !p_vroot )
        return VLC_EGENERIC;

    if( p_sys->b_seekable && (p_sys->b_fastseekable || b_force) )
    {
        MP4_ReadBoxContainerChildren( p_demux->s, p_vroot, NULL ); /* Get the rest of the file */
        p_sys->b_fragments_probed = true;

        const unsigned i_moof = MP4_BoxCount( p_vroot, "/moof" );
        if( i_moof )
        {
            *pb_fragmented = true;
            p_sys->p_fragsindex = MP4_Fragments_Index_New( p_sys->i_tracks, i_moof );
            if( !p_sys->p_fragsindex )
            {
                MP4_BoxFree( p_vroot );
                return VLC_EGENERIC;
            }

            stime_t *pi_track_times = calloc( p_sys->i_tracks, sizeof(*pi_track_times) );
            if( !pi_track_times )
            {
                MP4_Fragments_Index_Delete( p_sys->p_fragsindex );
                p_sys->p_fragsindex = NULL;
                MP4_BoxFree( p_vroot );
                return VLC_EGENERIC;
            }

            unsigned index = 0;

            for( MP4_Box_t *p_moof = p_vroot->p_first; p_moof; p_moof = p_moof->p_next )
            {
                if( p_moof->i_type != ATOM_moof )
                    continue;

                for( unsigned i=0; i<p_sys->i_tracks; i++ )
                {
                    stime_t i_duration = 0;
                    MP4_Box_t *p_tfdt = NULL;
                    MP4_Box_t *p_traf = MP4_GetTrafByTrackID( p_moof, p_sys->track[i].i_track_ID );
                    if( p_traf )
                        p_tfdt = MP4_BoxGet( p_traf, "tfdt" );

                    /* Set first fragment time offset from moov */
                    if( index == 0 )
                        pi_track_times[i] = GetMoovTrackDuration( p_sys, p_sys->track[i].i_track_ID );

                    if( p_tfdt && BOXDATA(p_tfdt) )
                    {
                        pi_track_times[i] = p_tfdt->data.p_tfdt->i_base_media_decode_time;
                    }
                    else if( index == 0 ) /* Set first fragment time offset from moov */
                    {
                        i_duration = GetMoovTrackDuration( p_sys, p_sys->track[i].i_track_ID );
                        pi_track_times[i] = MP4_rescale( i_duration, p_sys->i_timescale, p_sys->track[i].i_timescale );
                    }

                    stime_t i_movietime = MP4_rescale( pi_track_times[i], p_sys->track[i].i_timescale, p_sys->i_timescale );
                    p_sys->p_fragsindex->p_times[index * p_sys->i_tracks + i] = i_movietime;

                    if( GetMoofTrackDuration( p_sys->p_moov, p_moof, p_sys->track[i].i_track_ID, &i_duration ) )
                        pi_track_times[i] += i_duration;
                }

                p_sys->p_fragsindex->pi_pos[index++] = p_moof->i_pos;
            }

            for( unsigned i=0; i<p_sys->i_tracks; i++ )
            {
                stime_t i_movietime = MP4_rescale( pi_track_times[i], p_sys->track[i].i_timescale, p_sys->i_timescale );
                if( p_sys->p_fragsindex->i_last_time < i_movietime )
                    p_sys->p_fragsindex->i_last_time = i_movietime;
            }

            free( pi_track_times );
#ifdef MP4_VERBOSE
            MP4_Fragments_Index_Dump( VLC_OBJECT(p_demux), p_sys->p_fragsindex, p_sys->i_timescale );
#endif
        }
    }
    else
    {
        /* We stop at first moof, which validates our fragmentation condition
         * and we'll find others while reading. */
        const uint32_t excllist[] = { ATOM_moof, 0 };
        MP4_ReadBoxContainerRestricted( p_demux->s, p_vroot, NULL, excllist );
        /* Peek since we stopped before restriction */
        const uint8_t *p_peek;
        if ( vlc_stream_Peek( p_demux->s, &p_peek, 8 ) == 8 )
            *pb_fragmented = (VLC_FOURCC( p_peek[4], p_peek[5], p_peek[6], p_peek[7] ) == ATOM_moof);
        else
            *pb_fragmented = false;
    }

    MP4_BoxFree( p_vroot );

    MP4_Box_t *p_mehd = MP4_BoxGet( p_sys->p_moov, "mvex/mehd");
    if ( !p_mehd )
           p_sys->i_cumulated_duration = GetCumulatedDuration( p_demux );

    return VLC_SUCCESS;
}

static void FragResetContext( demux_sys_t *p_sys )
{
    if( p_sys->context.p_fragment_atom )
    {
        if( p_sys->context.p_fragment_atom != p_sys->p_moov )
            MP4_BoxFree( p_sys->context.p_fragment_atom );
        p_sys->context.p_fragment_atom = NULL;
    }
    p_sys->context.i_current_box_type = 0;

    for ( uint32_t i=0; i<p_sys->i_tracks; i++ )
    {
        mp4_track_t *p_track = &p_sys->track[i];
        p_track->context.i_default_sample_size = 0;
        p_track->context.i_default_sample_duration = 0;
    }
}

static int FragDemuxTrack( demux_t *p_demux, mp4_track_t *p_track,
                           unsigned i_max_preload )
{
    if( !p_track->b_ok ||
         p_track->context.runs.i_current >= p_track->context.runs.i_count )
        return VLC_DEMUXER_EOS;

    const MP4_Box_data_trun_t *p_trun =
            p_track->context.runs.p_array[p_track->context.runs.i_current].p_trun->data.p_trun;

    if( p_track->context.i_trun_sample >= p_trun->i_sample_count )
        return VLC_DEMUXER_EOS;

    uint32_t dur = p_track->context.i_default_sample_duration,
             len = p_track->context.i_default_sample_size;

    if( vlc_stream_Tell(p_demux->s) != p_track->context.i_trun_sample_pos &&
        MP4_Seek( p_demux->s, p_track->context.i_trun_sample_pos ) != VLC_SUCCESS )
        return VLC_DEMUXER_EOF;

    const stime_t i_demux_max_dts = (i_max_preload < UINT_MAX) ?
                p_track->i_time + MP4_rescale( i_max_preload, CLOCK_FREQ, p_track->i_timescale ) :
                INT64_MAX;

    for( uint32_t i = p_track->context.i_trun_sample; i < p_trun->i_sample_count; i++ )
    {
        const stime_t i_dts = p_track->i_time;
        stime_t i_pts = i_dts;

        if( p_trun->i_flags & MP4_TRUN_SAMPLE_DURATION )
            dur = p_trun->p_samples[i].i_duration;

        if( i_dts > i_demux_max_dts )
            return VLC_DEMUXER_SUCCESS;

        p_track->i_time += dur;
        p_track->context.i_trun_sample = i + 1;

        if( p_trun->i_flags & MP4_TRUN_SAMPLE_TIME_OFFSET )
        {
            if ( p_trun->i_version == 1 )
                i_pts += p_trun->p_samples[i].i_composition_time_offset.v1;
            else if( p_trun->p_samples[i].i_composition_time_offset.v0 < 0xFF000000 )
                i_pts += p_trun->p_samples[i].i_composition_time_offset.v0;
            else /* version 0 with negative */
                i_pts += p_trun->p_samples[i].i_composition_time_offset.v1;
        }

        if( p_trun->i_flags & MP4_TRUN_SAMPLE_SIZE )
            len = p_trun->p_samples[i].i_size;

        if( !dur )
            msg_Warn(p_demux, "Zero duration sample in trun.");

        if( !len )
            msg_Warn(p_demux, "Zero length sample in trun.");

        block_t *p_block = vlc_stream_Block( p_demux->s, len );
        uint32_t i_read = ( p_block ) ? p_block->i_buffer : 0;
        p_track->context.i_trun_sample_pos += i_read;
        if( i_read < len || p_block == NULL )
        {
            if( p_block )
                block_Release( p_block );
            return VLC_DEMUXER_FATAL;
        }

#if 0
        msg_Dbg( p_demux, "tk(%i)=%"PRId64" mv=%"PRId64" pos=%"PRIu64, p_track->i_track_ID,
                 VLC_TS_0 + MP4_rescale( i_dts, p_track->i_timescale, CLOCK_FREQ ),
                 VLC_TS_0 + MP4_rescale( i_pts, p_track->i_timescale, CLOCK_FREQ ),
                 p_track->context.i_trun_sample_pos );
#endif
        if ( p_track->p_es )
        {
            p_block->i_dts = VLC_TS_0 + MP4_rescale( i_dts, p_track->i_timescale, CLOCK_FREQ );
            if( p_track->fmt.i_cat == VIDEO_ES && !( p_trun->i_flags & MP4_TRUN_SAMPLE_TIME_OFFSET ) )
                p_block->i_pts = VLC_TS_INVALID;
            else
                p_block->i_pts = VLC_TS_0 + MP4_rescale( i_pts, p_track->i_timescale, CLOCK_FREQ );
            p_block->i_length = MP4_rescale( dur, p_track->i_timescale, CLOCK_FREQ );
            MP4_Block_Send( p_demux, p_track, p_block );
        }
        else block_Release( p_block );
    }

    if( p_track->context.i_trun_sample == p_trun->i_sample_count )
    {
        p_track->context.i_trun_sample = 0;
        if( ++p_track->context.runs.i_current < p_track->context.runs.i_count )
        {
            p_track->i_time = p_track->context.runs.p_array[p_track->context.runs.i_current].i_first_dts;
            p_track->context.i_trun_sample_pos = p_track->context.runs.p_array[p_track->context.runs.i_current].i_offset;
        }
    }

    return VLC_DEMUXER_SUCCESS;
}

static int DemuxMoof( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_status;

    const unsigned i_max_preload = ( p_sys->b_fastseekable ) ? 0 : ( p_sys->b_seekable ) ? DEMUX_TRACK_MAX_PRELOAD : UINT_MAX;

    const mtime_t i_nztime = MP4_GetMoviePTS( p_sys );

    /* !important! Ensure clock is set before sending data */
    if( p_sys->i_pcr == VLC_TS_INVALID )
        es_out_SetPCR( p_demux->out, VLC_TS_0 + i_nztime );

    /* demux up to increment amount of data on every track, or just set pcr if empty data */
    for( ;; )
    {
        mp4_track_t *tk = NULL;
        i_status = VLC_DEMUXER_EOS;

        /* First pass, find any track within our target increment, ordered by position */
        for( unsigned i = 0; i < p_sys->i_tracks; i++ )
        {
            mp4_track_t *tk_tmp = &p_sys->track[i];

            if( !tk_tmp->b_ok || tk_tmp->b_chapters_source ||
               (!tk_tmp->b_selected && !p_sys->b_seekable) ||
                tk_tmp->context.runs.i_current >= tk_tmp->context.runs.i_count )
                continue;

            /* At least still have data to demux on this or next turns */
            i_status = VLC_DEMUXER_SUCCESS;

            if( MP4_rescale( tk_tmp->i_time, tk_tmp->i_timescale, CLOCK_FREQ ) <= i_nztime + DEMUX_INCREMENT )
            {
                if( tk == NULL || tk_tmp->context.i_trun_sample_pos < tk->context.i_trun_sample_pos )
                    tk = tk_tmp;
            }
        }

        if( tk )
        {
            /* Second pass, refine and find any best candidate having a chunk pos closer than
             * current candidate (avoids seeks when increment falls between the 2) from
             * current position, but within extended interleave time */
            for( unsigned i = 0; i_max_preload > 0 && i < p_sys->i_tracks; i++ )
            {
                mp4_track_t *tk_tmp = &p_sys->track[i];
                if( tk_tmp == tk ||
                    !tk_tmp->b_ok || tk_tmp->b_chapters_source ||
                   (!tk_tmp->b_selected && !p_sys->b_seekable) ||
                    tk_tmp->context.runs.i_current >= tk_tmp->context.runs.i_count )
                    continue;

                mtime_t i_nzdts = MP4_rescale( tk_tmp->i_time, tk_tmp->i_timescale, CLOCK_FREQ );
                if ( i_nzdts <= i_nztime + DEMUX_TRACK_MAX_PRELOAD )
                {
                    /* Found a better candidate to avoid seeking */
                    if( tk_tmp->context.i_trun_sample_pos < tk->context.i_trun_sample_pos )
                        tk = tk_tmp;
                    /* Note: previous candidate will be repicked on next loop */
                }
            }

            int i_ret = FragDemuxTrack( p_demux, tk, i_max_preload );

            if( i_ret == VLC_DEMUXER_SUCCESS )
                i_status = VLC_DEMUXER_SUCCESS;
            else if( i_ret == VLC_DEMUXER_FATAL )
                i_status = VLC_DEMUXER_EOF;
        }

        if( i_status != VLC_DEMUXER_SUCCESS || !tk )
            break;
    }

    if( i_status != VLC_DEMUXER_EOS )
    {
        p_sys->i_nztime += DEMUX_INCREMENT;
        p_sys->i_pcr = VLC_TS_0 + p_sys->i_nztime;
        es_out_SetPCR( p_demux->out, p_sys->i_pcr );
    }
    else
    {
        mtime_t i_segment_end = INT64_MAX;
        for( unsigned i = 0; i < p_sys->i_tracks; i++ )
        {
            mp4_track_t *tk = &p_sys->track[i];
            if( tk->b_ok || tk->b_chapters_source ||
               (!tk->b_selected && !p_sys->b_seekable) )
                continue;
            mtime_t i_track_end = MP4_rescale( tk->i_time, tk->i_timescale, CLOCK_FREQ );
            if( i_track_end < i_segment_end  )
                i_segment_end = i_track_end;
        }
        if( i_segment_end != INT64_MAX )
        {
            p_sys->i_nztime = i_segment_end;
            p_sys->i_pcr = VLC_TS_0 + p_sys->i_nztime;
            es_out_SetPCR( p_demux->out, p_sys->i_pcr );
        }
    }

    return i_status;
}

static int FragCreateTrunIndex( demux_t *p_demux, MP4_Box_t *p_moof,
                                MP4_Box_t *p_chunksidx, stime_t i_moof_time, bool b_discontinuity )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    uint64_t i_traf_base_data_offset = p_moof->i_pos;
    uint32_t i_traf = 0;
    uint64_t i_prev_traf_end = 0;

    for( unsigned i=0; i<p_sys->i_tracks; i++ )
    {
        mp4_track_t *p_track = &p_sys->track[i];
        if( p_track->context.runs.p_array )
            free( p_track->context.runs.p_array );
        p_track->context.runs.p_array = NULL;
        p_track->context.runs.i_count = 0;
        p_track->context.runs.i_current = 0;
    }

    for( MP4_Box_t *p_traf = MP4_BoxGet( p_moof, "traf" );
                    p_traf ; p_traf = p_traf->p_next )
    {
        if ( p_traf->i_type != ATOM_traf )
            continue;

        const MP4_Box_t *p_tfhd = MP4_BoxGet( p_traf, "tfhd" );
        const uint32_t i_trun_count = MP4_BoxCount( p_traf, "trun" );
        if ( !p_tfhd || !i_trun_count )
            continue;

        mp4_track_t *p_track = MP4_GetTrackByTrackID( p_demux, BOXDATA(p_tfhd)->i_track_ID );
        if( !p_track )
            continue;

        p_track->context.runs.p_array = calloc(i_trun_count, sizeof(mp4_run_t));
        if(!p_track->context.runs.p_array)
            continue;

        /* Get defaults for this/these RUN */
        uint32_t i_track_defaultsamplesize = 0;
        uint32_t i_track_defaultsampleduration = 0;
        MP4_GetDefaultSizeAndDuration( p_sys->p_moov, BOXDATA(p_tfhd),
                                       &i_track_defaultsamplesize,
                                       &i_track_defaultsampleduration );
        p_track->context.i_default_sample_size = i_track_defaultsamplesize;
        p_track->context.i_default_sample_duration = i_track_defaultsampleduration;

        stime_t  i_traf_start_time = p_track->i_time;
        bool     b_has_base_media_decode_time = false;

        if( b_discontinuity ) /* We NEED start time offset for each track */
        {
            /* Find start time */
            const MP4_Box_t *p_tfdt = MP4_BoxGet( p_traf, "tfdt" );
            if( p_tfdt )
            {
                i_traf_start_time = BOXDATA(p_tfdt)->i_base_media_decode_time;
                b_has_base_media_decode_time = true;
            }

            /* Try using Tfxd for base offset (Smooth) */
            if( !b_has_base_media_decode_time && p_sys->i_tracks == 1 )
            {
                const MP4_Box_t *p_uuid = MP4_BoxGet( p_traf, "uuid" );
                for( ; p_uuid; p_uuid = p_uuid->p_next )
                {
                    if( p_uuid->i_type == ATOM_uuid &&
                       !CmpUUID( &p_uuid->i_uuid, &TfxdBoxUUID ) && p_uuid->data.p_tfxd )
                    {
                        i_traf_start_time = p_uuid->data.p_tfxd->i_fragment_abs_time;
                        b_has_base_media_decode_time = true;
                        break;
                    }
                }
            }

            /* After seek we should have probed fragments */
            if( !b_has_base_media_decode_time && p_sys->p_fragsindex )
            {
                unsigned i_track_index = (p_track - p_sys->track);
                assert(&p_sys->track[i_track_index] == p_track);
                i_traf_start_time = MP4_Fragment_Index_GetTrackStartTime( p_sys->p_fragsindex,
                                                                          i_track_index, p_moof->i_pos );
                i_traf_start_time = MP4_rescale( i_traf_start_time,
                                                 p_sys->i_timescale, p_track->i_timescale );
                b_has_base_media_decode_time = true;
            }

            if( !b_has_base_media_decode_time && p_chunksidx )
            {
                /* Try using SIDX as base offset.
                 * This can not work for global sidx but only when sent within each fragment (dash) */
                const MP4_Box_data_sidx_t *p_data = p_chunksidx->data.p_sidx;
                if( p_data && p_data->i_timescale && p_data->i_reference_count == 1 )
                {
                    i_traf_start_time = MP4_rescale( p_data->i_earliest_presentation_time,
                                                     p_data->i_timescale, p_track->i_timescale );
                    b_has_base_media_decode_time = true;
                }
            }

            /* First contiguous segment (moov->moof) and there's no tfdt not probed index (yet) */
            if( !b_has_base_media_decode_time && FragGetMoofSequenceNumber( p_moof ) == 1 )
            {
                i_traf_start_time = MP4_rescale( GetMoovTrackDuration( p_sys, p_track->i_track_ID ),
                                                 p_sys->i_timescale, p_track->i_timescale );
                b_has_base_media_decode_time = true;
            }

            /* Use global sidx moof time, in case moof does not carry tfdt */
            if( !b_has_base_media_decode_time && i_moof_time != INT64_MAX )
                i_traf_start_time = MP4_rescale( i_moof_time, p_sys->i_timescale, p_track->i_timescale );

            /* That should not happen */
            if( !b_has_base_media_decode_time )
                i_traf_start_time = MP4_rescale( p_sys->i_nztime, CLOCK_FREQ, p_track->i_timescale );
        }

        /* Parse TRUN data */

        if ( BOXDATA(p_tfhd)->i_flags & MP4_TFHD_BASE_DATA_OFFSET )
        {
            i_traf_base_data_offset = BOXDATA(p_tfhd)->i_base_data_offset;
        }
        /* ignored if MP4_TFHD_BASE_DATA_OFFSET */
        else if ( BOXDATA(p_tfhd)->i_flags & MP4_TFHD_DEFAULT_BASE_IS_MOOF )
        {
            i_traf_base_data_offset = p_moof->i_pos /* + 8*/;
        }
        else
        {
            if ( i_traf == 0 )
                i_traf_base_data_offset = p_moof->i_pos /*+ 8*/;
            else
                i_traf_base_data_offset = i_prev_traf_end;
        }

        uint64_t i_trun_dts = i_traf_start_time;
        uint64_t i_trun_data_offset = i_traf_base_data_offset;
        uint32_t i_trun_size = 0;

        for( const MP4_Box_t *p_trun = MP4_BoxGet( p_traf, "trun" );
                              p_trun && p_tfhd;  p_trun = p_trun->p_next )
        {
            if ( p_trun->i_type != ATOM_trun )
               continue;

            const MP4_Box_data_trun_t *p_trundata = p_trun->data.p_trun;

            /* Get data offset */
            if ( p_trundata->i_flags & MP4_TRUN_DATA_OFFSET )
            {
                /* Fix for broken Trun data offset relative to tfhd instead of moof, as seen in smooth */
                if( (BOXDATA(p_tfhd)->i_flags & MP4_TFHD_BASE_DATA_OFFSET) == 0 &&
                    i_traf == 0 &&
                    i_traf_base_data_offset + p_trundata->i_data_offset < p_moof->i_pos + p_moof->i_size + 8 )
                {
                    i_trun_data_offset += p_moof->i_size + 8;
                }
                else if( (BOXDATA(p_tfhd)->i_flags & MP4_TFHD_BASE_DATA_OFFSET) )
                {
                    i_trun_data_offset = BOXDATA(p_tfhd)->i_base_data_offset + p_trundata->i_data_offset;
                }
                /* ignored if MP4_TFHD_BASE_DATA_OFFSET */
                else if ( BOXDATA(p_tfhd)->i_flags & MP4_TFHD_DEFAULT_BASE_IS_MOOF )
                {
                    i_trun_data_offset = p_moof->i_pos + p_trundata->i_data_offset;
                }
                else
                {
                    i_trun_data_offset += p_trundata->i_data_offset;
                }
            }
            else
            {
                i_trun_data_offset += i_trun_size;
            }

            i_trun_size = 0;
#ifndef NDEBUG
            msg_Dbg( p_demux,
                     "tk %u run %" PRIu32 " dflt dur %"PRIu32" size %"PRIu32" firstdts %"PRId64" offset %"PRIu64,
                     p_track->i_track_ID,
                     p_track->context.runs.i_count,
                     i_track_defaultsampleduration,
                     i_track_defaultsamplesize,
                     MP4_rescale( i_trun_dts, p_track->i_timescale, CLOCK_FREQ ), i_trun_data_offset );
#endif
            //************
            mp4_run_t *p_run = &p_track->context.runs.p_array[p_track->context.runs.i_count++];
            p_run->i_first_dts = i_trun_dts;
            p_run->i_offset = i_trun_data_offset;
            p_run->p_trun = p_trun;

            //************
            /* Sum total time */
            if ( p_trundata->i_flags & MP4_TRUN_SAMPLE_DURATION )
            {
                for( uint32_t i=0; i< p_trundata->i_sample_count; i++ )
                    i_trun_dts += p_trundata->p_samples[i].i_duration;
            }
            else
            {
                i_trun_dts += p_trundata->i_sample_count *
                        i_track_defaultsampleduration;
            }

            /* Get total traf size */
            if ( p_trundata->i_flags & MP4_TRUN_SAMPLE_SIZE )
            {
                for( uint32_t i=0; i< p_trundata->i_sample_count; i++ )
                    i_trun_size += p_trundata->p_samples[i].i_size;
            }
            else
            {
                i_trun_size += p_trundata->i_sample_count *
                        i_track_defaultsamplesize;
            }

            i_prev_traf_end = i_trun_data_offset + i_trun_size;
        }

        i_traf++;
    }

    return VLC_SUCCESS;
}

static int FragGetMoofBySidxIndex( demux_t *p_demux, mtime_t i_target_time,
                                   uint64_t *pi_moof_pos, mtime_t *pi_sampletime )
{
    const MP4_Box_t *p_sidx = MP4_BoxGet( p_demux->p_sys->p_root, "sidx" );
    const MP4_Box_data_sidx_t *p_data;
    if( !p_sidx || !((p_data = BOXDATA(p_sidx))) || !p_data->i_timescale )
        return VLC_EGENERIC;

    i_target_time = MP4_rescale( i_target_time, CLOCK_FREQ, p_data->i_timescale );

    /* sidx refers to offsets from end of sidx pos in the file + first offset */
    uint64_t i_pos = p_data->i_first_offset + p_sidx->i_pos + p_sidx->i_size;
    stime_t i_time = 0;
    for( uint16_t i=0; i<p_data->i_reference_count; i++ )
    {
        if( i_time + p_data->p_items[i].i_subsegment_duration > i_target_time )
        {
            *pi_sampletime = MP4_rescale( i_time, p_data->i_timescale, CLOCK_FREQ );
            *pi_moof_pos = i_pos;
            return VLC_SUCCESS;
        }
        i_pos += p_data->p_items[i].i_referenced_size;
        i_time += p_data->p_items[i].i_subsegment_duration;
    }

    return VLC_EGENERIC;
}

static int FragGetMoofByTfraIndex( demux_t *p_demux, const mtime_t i_target_time, unsigned i_track_ID,
                                   uint64_t *pi_moof_pos, mtime_t *pi_sampletime )
{
    MP4_Box_t *p_tfra = MP4_BoxGet( p_demux->p_sys->p_root, "mfra/tfra" );
    for( ; p_tfra; p_tfra = p_tfra->p_next )
    {
        if ( p_tfra->i_type == ATOM_tfra )
        {
            const MP4_Box_data_tfra_t *p_data = BOXDATA(p_tfra);
            if( !p_data || p_data->i_track_ID != i_track_ID )
                continue;

            uint64_t i_pos = 0;
            mp4_track_t *p_track = MP4_GetTrackByTrackID( p_demux, p_data->i_track_ID );
            if ( p_track )
            {
                stime_t i_track_target_time = MP4_rescale( i_target_time, CLOCK_FREQ, p_track->i_timescale );
                for ( uint32_t i = 0; i<p_data->i_number_of_entries; i += ( p_data->i_version == 1 ) ? 2 : 1 )
                {
                    mtime_t i_time;
                    uint64_t i_offset;
                    if ( p_data->i_version == 1 )
                    {
                        i_time = *((uint64_t *)(p_data->p_time + i));
                        i_offset = *((uint64_t *)(p_data->p_moof_offset + i));
                    }
                    else
                    {
                        i_time = p_data->p_time[i];
                        i_offset = p_data->p_moof_offset[i];
                    }

                    if ( i_time >= i_track_target_time )
                    {
                        if ( i_pos == 0 ) /* Not in this traf */
                            break;

                        *pi_moof_pos = i_pos;
                        *pi_sampletime = MP4_rescale( i_time, p_track->i_timescale, CLOCK_FREQ );
                        return VLC_SUCCESS;
                    }
                    else
                        i_pos = i_offset;
                }
            }
        }
    }
    return VLC_EGENERIC;
}

static void MP4_GetDefaultSizeAndDuration( MP4_Box_t *p_moov,
                                           const MP4_Box_data_tfhd_t *p_tfhd_data,
                                           uint32_t *pi_default_size,
                                           uint32_t *pi_default_duration )
{
    if( p_tfhd_data->i_flags & MP4_TFHD_DFLT_SAMPLE_DURATION )
        *pi_default_duration = p_tfhd_data->i_default_sample_duration;

    if( p_tfhd_data->i_flags & MP4_TFHD_DFLT_SAMPLE_SIZE )
        *pi_default_size = p_tfhd_data->i_default_sample_size;

    if( !*pi_default_duration || !*pi_default_size )
    {
        const MP4_Box_t *p_trex = MP4_GetTrexByTrackID( p_moov, p_tfhd_data->i_track_ID );
        if ( p_trex )
        {
            if ( !*pi_default_duration )
                *pi_default_duration = BOXDATA(p_trex)->i_default_sample_duration;
            if ( !*pi_default_size )
                *pi_default_size = BOXDATA(p_trex)->i_default_sample_size;
        }
    }
}

static int DemuxFrag( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned i_track_selected = 0;
    int i_status = VLC_DEMUXER_SUCCESS;

    if( unlikely(p_sys->b_error) )
    {
        msg_Warn( p_demux, "unrecoverable error" );
        i_status = VLC_DEMUXER_EOF;
        goto end;
    }

    /* check for newly selected/unselected track */
    for( unsigned i_track = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        mp4_track_t *tk = &p_sys->track[i_track];
        bool b = true;

        if( !tk->b_ok || tk->b_chapters_source )
            continue;

        if( p_sys->b_seekable )
            es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE, tk->p_es, &b );

        if(tk->b_selected != b)
        {
            msg_Dbg( p_demux, "track %u %s!", tk->i_track_ID, b ? "enabled" : "disabled" );
            MP4_TrackSelect( p_demux, tk, b );
        }

        if( tk->b_selected )
            i_track_selected++;
    }

    if( i_track_selected <= 0 )
    {
        msg_Warn( p_demux, "no track selected, exiting..." );
        i_status = VLC_DEMUXER_EOF;
        goto end;
    }

    if ( p_sys->context.i_current_box_type != ATOM_mdat )
    {
        /* Othewise mdat is skipped. FIXME: mdat reading ! */
        const uint8_t *p_peek;
        if( vlc_stream_Peek( p_demux->s, &p_peek, 8 ) != 8 )
        {
            i_status = VLC_DEMUXER_EOF;
            goto end;
        }

        p_sys->context.i_current_box_type = VLC_FOURCC( p_peek[4], p_peek[5], p_peek[6], p_peek[7] );
        if( p_sys->context.i_current_box_type == ATOM_mdat )
        {
            p_sys->context.i_post_mdat_offset = vlc_stream_Tell( p_demux->s ) + GetDWBE( p_peek );
        }
        else
        {
            MP4_Box_t *p_vroot = MP4_BoxGetNextChunk( p_demux->s );
            if(!p_vroot)
            {
                i_status = VLC_DEMUXER_EOF;
                goto end;
            }

            MP4_Box_t *p_box = NULL;
            for( p_box = p_vroot->p_first; p_box; p_box = p_box->p_next )
            {
                if( p_box->i_type == ATOM_moof ||
                    p_box->i_type == ATOM_moov )
                    break;
            }

            if( p_box )
            {
                FragResetContext( p_sys );

                if( p_box->i_type == ATOM_moov )
                {
                    p_sys->context.p_fragment_atom = p_sys->p_moov;
                }
                else
                {
                    p_sys->context.p_fragment_atom = MP4_BoxExtract( &p_vroot->p_first, p_box->i_type );

                    /* Detect and Handle Passive Seek */
                    const uint32_t i_sequence_number = FragGetMoofSequenceNumber( p_sys->context.p_fragment_atom );
                    const bool b_discontinuity = ( i_sequence_number != p_sys->context.i_lastseqnumber + 1 );
                    if( b_discontinuity )
                        msg_Info( p_demux, "Fragment sequence discontinuity detected %"PRIu32" != %"PRIu32,
                                            i_sequence_number, p_sys->context.i_lastseqnumber + 1 );
                    p_sys->context.i_lastseqnumber = i_sequence_number;

                    /* Prepare chunk */
                    if( FragPrepareChunk( p_demux, p_sys->context.p_fragment_atom,
                                          MP4_BoxGet( p_vroot, "sidx"), INT64_MAX,
                                          b_discontinuity ) != VLC_SUCCESS )
                    {
                        MP4_BoxFree( p_vroot );
                        i_status = VLC_DEMUXER_EOF;
                        goto end;
                    }

                    if( b_discontinuity )
                    {
                        p_sys->i_nztime = FragGetDemuxTimeFromTracksTime( p_sys );
                        p_sys->i_pcr = VLC_TS_INVALID;
                    }
                    /* !Prepare chunk */
                }

                p_sys->context.i_current_box_type = p_box->i_type;
            }

            MP4_BoxFree( p_vroot );

            if( p_sys->context.p_fragment_atom == NULL )
            {
                msg_Info(p_demux, "no moof or moov in current chunk");
                return VLC_DEMUXER_SUCCESS;
            }
        }
    }

    if ( p_sys->context.i_current_box_type == ATOM_mdat )
    {
        assert(p_sys->context.p_fragment_atom);

        if ( p_sys->context.p_fragment_atom )
        switch( p_sys->context.p_fragment_atom->i_type )
        {
            case ATOM_moov://[ftyp/moov, mdat]+ -> [moof, mdat]+
                i_status = DemuxMoov( p_demux );
            break;
            case ATOM_moof:
                i_status = DemuxMoof( p_demux );
              break;
        default:
             msg_Err( p_demux, "fragment type %4.4s", (char*) &p_sys->context.p_fragment_atom->i_type );
             break;
        }

        if( i_status == VLC_DEMUXER_EOS )
        {
            i_status = VLC_DEMUXER_SUCCESS;
            /* Skip if we didn't reach the end of mdat box */
            uint64_t i_pos = vlc_stream_Tell( p_demux->s );
            if( i_pos != p_sys->context.i_post_mdat_offset && i_status != VLC_DEMUXER_EOF )
            {
                if( i_pos > p_sys->context.i_post_mdat_offset )
                    msg_Err( p_demux, " Overread mdat by %" PRIu64, i_pos - p_sys->context.i_post_mdat_offset );
                else
                    msg_Warn( p_demux, "mdat had still %"PRIu64" bytes unparsed as samples",
                                        p_sys->context.i_post_mdat_offset - i_pos );
                if( MP4_Seek( p_demux->s, p_sys->context.i_post_mdat_offset ) != VLC_SUCCESS )
                    i_status = VLC_DEMUXER_EGENERIC;
            }
            p_sys->context.i_current_box_type = 0;

        }
    }

end:
    if( i_status == VLC_DEMUXER_EOF )
    {
        mtime_t i_demux_end = INT64_MIN;
        for( unsigned i = 0; i < p_sys->i_tracks; i++ )
        {
            const mp4_track_t *tk = &p_sys->track[i];
            mtime_t i_track_end = MP4_rescale( tk->i_time, tk->i_timescale, CLOCK_FREQ );
            if( i_track_end > i_demux_end  )
                i_demux_end = i_track_end;
        }
        if( i_demux_end != INT64_MIN )
            es_out_SetPCR( p_demux->out, VLC_TS_0 + i_demux_end );
    }

    return i_status;
}

/* ASF Handlers */
inline static mp4_track_t *MP4ASF_GetTrack( asf_packet_sys_t *p_packetsys,
                                            uint8_t i_stream_number )
{
    demux_sys_t *p_sys = p_packetsys->p_demux->p_sys;
    for ( unsigned int i=0; i<p_sys->i_tracks; i++ )
    {
        if ( p_sys->track[i].p_asf &&
             i_stream_number == p_sys->track[i].BOXDATA(p_asf)->i_stream_number )
        {
            return &p_sys->track[i];
        }
    }
    return NULL;
}

static asf_track_info_t * MP4ASF_GetTrackInfo( asf_packet_sys_t *p_packetsys,
                                               uint8_t i_stream_number )
{
    mp4_track_t *p_track = MP4ASF_GetTrack( p_packetsys, i_stream_number );
    if ( p_track )
        return &p_track->asfinfo;
    else
        return NULL;
}

static void MP4ASF_Send( asf_packet_sys_t *p_packetsys, uint8_t i_stream_number,
                         block_t **pp_frame )
{
    mp4_track_t *p_track = MP4ASF_GetTrack( p_packetsys, i_stream_number );
    if ( !p_track )
    {
        block_Release( *pp_frame );
    }
    else
    {
        block_t *p_gather = block_ChainGather( *pp_frame );
        p_gather->i_dts = p_track->i_dts_backup;
        p_gather->i_pts = p_track->i_pts_backup;
        es_out_Send( p_packetsys->p_demux->out, p_track->p_es, p_gather );
    }

    *pp_frame = NULL;
}

static void MP4ASF_ResetFrames( demux_sys_t *p_sys )
{
    for ( unsigned int i=0; i<p_sys->i_tracks; i++ )
    {
        mp4_track_t *p_track = &p_sys->track[i];
        if( p_track->asfinfo.p_frame )
        {
            block_ChainRelease( p_track->asfinfo.p_frame );
            p_track->asfinfo.p_frame = NULL;
        }
    }
}

#undef BOXDATA
