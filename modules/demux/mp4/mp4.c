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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "mp4.h"

#include <vlc_plugin.h>

#include <vlc_demux.h>
#include <vlc_charset.h>                           /* EnsureUTF8 */
#include <vlc_input.h>
#include <vlc_aout.h>
#include <assert.h>
#include <limits.h>
#include "../codec/cc.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_description( N_("MP4 stream demuxer") )
    set_shortname( N_("MP4") )
    set_capability( "demux", 240 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int   Demux   ( demux_t * );
static int   DemuxRef( demux_t *p_demux ){ (void)p_demux; return 0;}
static int   DemuxFrg( demux_t * );
static int   DemuxAsLeaf( demux_t * );
static int   Seek    ( demux_t *, mtime_t );
static int   Control ( demux_t *, int, va_list );

struct demux_sys_t
{
    MP4_Box_t    *p_root;      /* container for the whole file */

    mtime_t      i_pcr;

    uint64_t     i_overall_duration; /* Full duration, including all fragments */
    uint64_t     i_time;         /* time position of the presentation
                                  * in movie timescale */
    uint32_t     i_timescale;    /* movie time scale */
    unsigned int i_tracks;       /* number of tracks */
    mp4_track_t  *track;         /* array of track */
    float        f_fps;          /* number of frame per seconds */

    bool         b_fragmented;   /* fMP4 */
    bool         b_seekable;
    bool         b_fastseekable;
    bool         b_seekmode;
    bool         b_smooth;       /* Is it Smooth Streaming? */
    bool         b_dash;

    bool            b_index_probed;
    bool            b_fragments_probed;
    mp4_fragment_t  moovfragment; /* moov */
    mp4_fragment_t *p_fragments;  /* known fragments (moof following moov) */

    struct
    {
        mp4_fragment_t *p_fragment;
        uint32_t        i_current_box_type;
        uint32_t        i_mdatbytesleft;
        uint32_t        i_lastseqnumber;
    } context;

    /* */
    MP4_Box_t    *p_tref_chap;

    /* */
    input_title_t *p_title;

    /* ASF in MP4 */
    asf_packet_sys_t asfpacketsys;
    uint64_t i_preroll;         /* foobar */
    int64_t  i_preroll_start;
};

/*****************************************************************************
 * Declaration of local function
 *****************************************************************************/
static void MP4_TrackCreate ( demux_t *, mp4_track_t *, MP4_Box_t  *, bool, bool );
static int  MP4_SmoothTrackCreate( demux_t *, mp4_track_t *, const mp4_chunk_t *,
                                   const MP4_Box_t *, bool );
static void MP4_TrackDestroy( demux_t *, mp4_track_t * );

static block_t * MP4_Block_Read( demux_t *, const mp4_track_t *, int );
static void MP4_Block_Send( demux_t *, mp4_track_t *, block_t * );

static int  MP4_TrackSelect ( demux_t *, mp4_track_t *, mtime_t );
static void MP4_TrackUnselect(demux_t *, mp4_track_t * );

static int  MP4_TrackSeek   ( demux_t *, mp4_track_t *, mtime_t );

static uint64_t MP4_TrackGetPos    ( mp4_track_t * );
static uint32_t MP4_TrackGetReadSize( mp4_track_t *, uint32_t * );
static int      MP4_TrackNextSample( demux_t *, mp4_track_t *, uint32_t );
static void     MP4_TrackSetELST( demux_t *, mp4_track_t *, int64_t );
static bool     MP4_TrackIsInterleaved( const mp4_track_t * );

static void     MP4_UpdateSeekpoint( demux_t * );

static MP4_Box_t * MP4_GetTrexByTrackID( MP4_Box_t *p_moov, const uint32_t i_id );
static void MP4_GetDefaultSizeAndDuration( demux_t *p_demux,
                                           const MP4_Box_data_tfhd_t *p_tfhd_data,
                                           uint32_t *pi_default_size,
                                           uint32_t *pi_default_duration );

static bool AddFragment( demux_t *p_demux, MP4_Box_t *p_moox );
static int  ProbeFragments( demux_t *p_demux, bool b_force );
static int  ProbeIndex( demux_t *p_demux );
static mp4_fragment_t *GetFragmentByPos( demux_t *p_demux, uint64_t i_pos, bool b_exact );
static mp4_fragment_t *GetFragmentByTime( demux_t *p_demux, const mtime_t i_time );

static int LeafIndexGetMoofPosByTime( demux_t *p_demux, const mtime_t i_target_time,
                                      uint64_t *pi_pos, mtime_t *pi_mooftime );
static mtime_t LeafGetTrackFragmentTimeOffset( demux_t *p_demux, mp4_fragment_t *, unsigned int );
static int LeafGetTrackAndChunkByMOOVPos( demux_t *p_demux, uint64_t *pi_pos,
                                      mp4_track_t **pp_tk, unsigned int *pi_chunk );
static int LeafMapTrafTrunContextes( demux_t *p_demux, MP4_Box_t *p_moof );

/* ASF Handlers */
static asf_track_info_t * MP4ASF_GetTrackInfo( asf_packet_sys_t *p_packetsys, uint8_t i_stream_number );
static void MP4ASF_Send(asf_packet_sys_t *p_packetsys, uint8_t i_stream_number, block_t **pp_frame);
static void MP4ASF_ResetFrames( demux_sys_t *p_sys );

/* Helpers */

static uint32_t stream_ReadU32( stream_t *s, void *p_read, uint32_t i_toread )
{
    uint32_t i_return = 0;
    if ( i_toread > INT32_MAX )
    {
        i_return = stream_Read( s, p_read, INT32_MAX );
        if ( i_return < INT32_MAX )
            return i_return;
        else
            i_toread -= INT32_MAX;
    }
    i_return += stream_Read( s, (uint8_t *)p_read + i_return, (int32_t) i_toread );
    return i_return;
}

static bool MP4_stream_Tell( stream_t *s, uint64_t *pi_pos )
{
    int64_t i_pos = stream_Tell( s );
    if ( i_pos < 0 )
        return false;
    else
    {
        *pi_pos = (uint64_t) i_pos;
        return true;
    }
}

static mtime_t GetTrackDurationInFragment( const mp4_fragment_t *p_fragment,
                                           unsigned int i_track_ID )
{
    for( unsigned int i=0; i<p_fragment->i_durations; i++ )
    {
        if( i_track_ID == p_fragment->p_durations[i].i_track_ID )
            return p_fragment->p_durations[i].i_duration;
    }
    return 0;
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

/* Return time in microsecond of a track */
static inline int64_t MP4_TrackGetDTS( demux_t *p_demux, mp4_track_t *p_track )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const mp4_chunk_t *p_chunk;
    if( p_sys->b_fragmented )
        p_chunk = p_track->cchunk;
    else
        p_chunk = &p_track->chunk[p_track->i_chunk];

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

    /* now handle elst */
    if( p_track->p_elst )
    {
        demux_sys_t         *p_sys = p_demux->p_sys;
        MP4_Box_data_elst_t *elst = p_track->BOXDATA(p_elst);

        /* convert to offset */
        if( ( elst->i_media_rate_integer[p_track->i_elst] > 0 ||
              elst->i_media_rate_fraction[p_track->i_elst] > 0 ) &&
            elst->i_media_time[p_track->i_elst] > 0 )
        {
            i_dts -= elst->i_media_time[p_track->i_elst];
        }

        /* add i_elst_time */
        i_dts += p_track->i_elst_time * p_track->i_timescale /
            p_sys->i_timescale;

        if( i_dts < 0 ) i_dts = 0;
    }

    return CLOCK_FREQ * i_dts / p_track->i_timescale;
}

static inline bool MP4_TrackGetPTSDelta( demux_t *p_demux, mp4_track_t *p_track,
                                         int64_t *pi_delta )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    mp4_chunk_t *ck;
    if( p_sys->b_fragmented )
        ck = p_track->cchunk;
    else
        ck = &p_track->chunk[p_track->i_chunk];

    unsigned int i_index = 0;
    unsigned int i_sample = p_track->i_sample - ck->i_sample_first;

    if( ck->p_sample_count_pts == NULL || ck->p_sample_offset_pts == NULL )
        return false;

    for( i_index = 0; i_index < ck->i_entries_pts ; i_index++ )
    {
        if( i_sample < ck->p_sample_count_pts[i_index] )
        {
            *pi_delta = ck->p_sample_offset_pts[i_index] * CLOCK_FREQ /
                        (int64_t)p_track->i_timescale;
            return true;
        }

        i_sample -= ck->p_sample_count_pts[i_index];
    }
    return false;
}

static inline int64_t MP4_GetMoviePTS(demux_sys_t *p_sys )
{
    return CLOCK_FREQ * p_sys->i_time / p_sys->i_timescale;
}

static void LoadChapter( demux_t  *p_demux );

static int LoadInitFrag( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->b_smooth ) /* Smooth Streaming */
    {
        if( ( p_sys->p_root = MP4_BoxGetSmooBox( p_demux->s ) ) == NULL )
        {
            goto LoadInitFragError;
        }
        else
        {
            MP4_Box_t *p_smoo = MP4_BoxGet( p_sys->p_root, "uuid" );
            if( !p_smoo || CmpUUID( &p_smoo->i_uuid, &SmooBoxUUID ) )
                goto LoadInitFragError;
            /* Get number of tracks */
            p_sys->i_tracks = 0;
            for( int i = 0; i < 3; i++ )
            {
                MP4_Box_t *p_stra = MP4_BoxGet( p_smoo, "uuid[%d]", i );
                if( p_stra && BOXDATA(p_stra) && BOXDATA(p_stra)->i_track_ID )
                    p_sys->i_tracks++;
                /* Get timescale and duration of the video track; */
                if( p_sys->i_timescale == 0 )
                {
                    if ( p_stra && BOXDATA(p_stra) )
                    {
                        p_sys->i_timescale = BOXDATA(p_stra)->i_timescale;
                        p_sys->i_overall_duration = BOXDATA(p_stra)->i_duration;
                    }
                    if( p_sys->i_timescale == 0 )
                    {
                        msg_Err( p_demux, "bad timescale" );
                        goto LoadInitFragError;
                    }
                }
            }
        }
    }
    else
    {
        /* Load all boxes ( except raw data ) */
        if( ( p_sys->p_root = MP4_BoxGetRoot( p_demux->s ) ) == NULL )
        {
            goto LoadInitFragError;
        }
    }
    return VLC_SUCCESS;

LoadInitFragError:
    msg_Warn( p_demux, "MP4 plugin discarded (not a valid initialization chunk)" );
    return VLC_EGENERIC;
}

static int AllocateTracks( demux_t *p_demux, unsigned i_tracks )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    p_sys->track = calloc( i_tracks, sizeof( mp4_track_t ) );
    if( p_sys->track == NULL )
        return VLC_ENOMEM;
    p_sys->i_tracks = i_tracks;

    if( p_sys->b_fragmented )
    {
        for( unsigned i = 0; i < i_tracks; i++ )
        {
            mp4_track_t *p_track = &p_sys->track[i];
            p_track->cchunk = calloc( 1, sizeof( mp4_chunk_t ) );
            if( unlikely( !p_track->cchunk ) )
                return VLC_ENOMEM;
        }
    }
    return VLC_SUCCESS;
}

static int CreateTracksFromSmooBox( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    MP4_Box_t *p_smoo = MP4_BoxGet( p_sys->p_root, "uuid" );
    if( CmpUUID( &p_smoo->i_uuid, &SmooBoxUUID ) )
        return VLC_EGENERIC;

    /* Smooth tracks are stra UUID box below smooth UUID box */
    const unsigned i_tracks = MP4_BoxCount( p_smoo, "uuid" );

    if( AllocateTracks( p_demux, i_tracks ) != VLC_SUCCESS )
        return VLC_EGENERIC;

    unsigned j = 0;
    MP4_Box_t *p_stra = MP4_BoxGet( p_smoo, "uuid" );
    while( p_stra && j < p_sys->i_tracks )
    {
        if( !CmpUUID( &p_stra->i_uuid, &StraBoxUUID ) &&
            BOXDATA(p_stra) && BOXDATA(p_stra)->i_track_ID > 0 )
        {
            mp4_track_t *p_track = &p_sys->track[j++];
            MP4_SmoothTrackCreate( p_demux, p_track, p_track->cchunk, p_stra, true );
        }
        p_stra = p_stra->p_next;
    }

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
        p_write[i_copied++] = 0; /* cc1 == field 0 */
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
            p_write[i_copied++] = 0; /* cc1 == field 0 */
            p_write[i_copied++] = p_read[0];
            p_write[i_copied++] = p_read[1];
            p_read += 2;
            i_bytes -= 2;
        } while( i_bytes >= 2 );
    }

    block_Release( p_block );
    p_newblock->i_buffer = i_copied;
    p_newblock->i_flags = BLOCK_FLAG_ORDERED_CAPTIONS;
    return p_newblock;
}

static block_t * MP4_Block_Read( demux_t *p_demux, const mp4_track_t *p_track, int i_size )
{
    block_t *p_block = stream_Block( p_demux->s, i_size );
    if ( !p_block )
        return NULL;

    /* might have some encap */
    if( p_track->fmt.i_cat == SPU_ES )
    {
        switch( p_track->fmt.i_codec )
        {
            case VLC_CODEC_TX3G:
            case VLC_CODEC_SPU:
            /* accept as-is */
            break;
            case VLC_CODEC_EIA608_1:
                p_block = MP4_EIA608_Convert( p_block );
            break;
        default:
            p_block->i_buffer = 0;
            break;
        }
    }

    return p_block;
}

static void MP4_Block_Send( demux_t *p_demux, mp4_track_t *p_track, block_t *p_block )
{
    if ( p_track->b_chans_reorder && aout_BitsPerSample( p_track->fmt.i_codec ) )
    {
        aout_ChannelReorder( p_block->p_buffer, p_block->i_buffer,
                             p_track->fmt.audio.i_channels,
                             p_track->rgi_chans_reordering,
                             p_track->fmt.i_codec );
    }

    p_block->i_flags |= p_track->i_block_flags;

    /* ASF packets in mov */
    if( p_track->p_asf )
    {
        /* Fake a new stream from MP4 block */
        stream_t *p_stream = p_demux->s;
        p_demux->s = stream_MemoryNew( p_demux, p_block->p_buffer, p_block->i_buffer, true );
        if ( p_demux->s )
        {
            p_track->i_dts_backup = p_block->i_dts;
            p_track->i_pts_backup = p_block->i_pts;
            /* And demux it as ASF packet */
            DemuxASFPacket( &p_demux->p_sys->asfpacketsys, p_block->i_buffer, p_block->i_buffer );
            stream_Delete(p_demux->s);
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
    MP4_Box_t       *p_rmra;
    MP4_Box_t       *p_mvhd;
    MP4_Box_t       *p_trak;

    unsigned int    i;
    bool      b_enabled_es;

    /* A little test to see if it could be a mp4 */
    if( stream_Peek( p_demux->s, &p_peek, 11 ) < 11 ) return VLC_EGENERIC;

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
    stream_Control( p_demux->s, STREAM_CAN_SEEK, &p_sys->b_seekable );
    stream_Control( p_demux->s, STREAM_CAN_FASTSEEK, &p_sys->b_fastseekable );
    p_sys->b_seekmode = p_sys->b_fastseekable;

    /*Set exported functions */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    p_sys->context.i_lastseqnumber = 1;

    p_demux->p_sys = p_sys;

    if( stream_Peek( p_demux->s, &p_peek, 24 ) < 24 ) return VLC_EGENERIC;
    if( !CmpUUID( (UUID_t *)(p_peek + 8), &SmooBoxUUID ) )
    {
        p_sys->b_smooth = true;
        p_sys->b_fragmented = true;
    }

    if( LoadInitFrag( p_demux ) != VLC_SUCCESS )
        goto error;


    if( ( p_ftyp = MP4_BoxGet( p_sys->p_root, "/ftyp" ) ) )
    {
        switch( BOXDATA(p_ftyp)->i_major_brand )
        {
            case MAJOR_isom:
                msg_Dbg( p_demux,
                         "ISO Media file (isom) version %d.",
                         BOXDATA(p_ftyp)->i_minor_version );
                break;
            case MAJOR_3gp4:
            case MAJOR_3gp5:
            case MAJOR_3gp6:
            case MAJOR_3gp7:
                msg_Dbg( p_demux, "3GPP Media file Release: %c",
#ifdef WORDS_BIGENDIAN
                        BOXDATA(p_ftyp)->i_major_brand
#else
                        BOXDATA(p_ftyp)->i_major_brand >> 24
#endif
                        );
                break;
            case MAJOR_qt__:
                msg_Dbg( p_demux, "Apple QuickTime file" );
                break;
            case MAJOR_isml:
                msg_Dbg( p_demux, "PIFF (= isml = fMP4) file" );
                break;
            case MAJOR_dash:
                msg_Dbg( p_demux, "DASH Stream file" );
                p_sys->b_dash = true;
                break;
            default:
                msg_Dbg( p_demux,
                         "unrecognized major file specification (%4.4s).",
                          (char*)&BOXDATA(p_ftyp)->i_major_brand );
                break;
        }
        /* also lookup in compatibility list */
        for(uint32_t i=0; i<BOXDATA(p_ftyp)->i_compatible_brands_count; i++)
        {
            if (BOXDATA(p_ftyp)->i_compatible_brands[i] == MAJOR_dash)
            {
                msg_Dbg( p_demux, "DASH Stream file" );
                p_sys->b_dash = true;
            }
        }
    }
    else
    {
        msg_Dbg( p_demux, "file type box missing (assuming ISO Media file)" );
    }

    if( MP4_BoxCount( p_sys->p_root, "/moov/mvex" ) > 0 )
    {
        if ( p_sys->b_seekable && !p_sys->b_smooth && !p_sys->b_dash )
        {
            /* Probe remaining to check if there's really fragments
               or if that file is just ready to append fragments */
            ProbeFragments( p_demux, false );
            p_sys->b_fragmented = !!MP4_BoxCount( p_sys->p_root, "/moof" );

            if ( p_sys->b_fragmented && !p_sys->i_overall_duration )
                ProbeFragments( p_demux, true );
        }
        else
            p_sys->b_fragmented = true;
    }

    if ( !p_sys->moovfragment.p_moox )
        AddFragment( p_demux, MP4_BoxGet( p_sys->p_root, "/moov" ) );

    /* we always need a moov entry, but smooth has a workaround */
    if ( !p_sys->moovfragment.p_moox && !p_sys->b_smooth )
        goto error;

    MP4_BoxDumpStructure( p_demux->s, p_sys->p_root );

    if( p_sys->b_smooth )
    {
        if( CreateTracksFromSmooBox( p_demux ) != VLC_SUCCESS )
            goto error;

        p_demux->pf_demux = DemuxFrg;
        msg_Dbg( p_demux, "Set DemuxFrg mode" );
        return VLC_SUCCESS;
    }
    else if( p_sys->b_fragmented )
    {
        p_demux->pf_demux = DemuxAsLeaf;
        msg_Dbg( p_demux, "Set experimental DemuxLeaf mode" );
    }

    if( !p_sys->b_seekable && p_demux->pf_demux == Demux )
    {
        msg_Warn( p_demux, "MP4 plugin discarded (not seekable)" );
        goto error;
    }

    /* the file need to have one moov box */
    p_sys->moovfragment.p_moox = MP4_BoxGet( p_sys->p_root, "/moov", 0 );
    if( !p_sys->moovfragment.p_moox )
    {
        MP4_Box_t *p_foov = MP4_BoxGet( p_sys->p_root, "/foov" );

        if( !p_foov )
        {
            msg_Err( p_demux, "MP4 plugin discarded (no moov,foov,moof box)" );
            goto error;
        }
        /* we have a free box as a moov, rename it */
        p_foov->i_type = ATOM_moov;
        p_sys->moovfragment.p_moox = p_foov;
    }

    if( ( p_rmra = MP4_BoxGet( p_sys->p_root,  "/moov/rmra" ) ) )
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
                input_item_CopyOptions( p_current, p_item );
                input_item_node_AppendItem( p_subitems, p_item );
                vlc_gc_decref( p_item );
            }
            else
            {
                msg_Err( p_demux, "unknown ref type=%4.4s FIXME (send a bug report)",
                         (char*)&BOXDATA(p_rdrf)->i_ref_type );
            }
            free( psz_ref );
        }
        input_item_node_PostAndDelete( p_subitems );
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

    if( AllocateTracks( p_demux, i_tracks ) != VLC_SUCCESS )
        goto error;

    /* Search the first chap reference (like quicktime) and
     * check that at least 1 stream is enabled */
    p_sys->p_tref_chap = NULL;
    b_enabled_es = false;
    for( i = 0; i < p_sys->i_tracks; i++ )
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

    /* now process each track and extract all useful information */
    for( i = 0; i < p_sys->i_tracks; i++ )
    {
        p_trak = MP4_BoxGet( p_sys->p_root, "/moov/trak[%d]", i );
        MP4_TrackCreate( p_demux, &p_sys->track[i], p_trak, true, !b_enabled_es );

        if( p_sys->track[i].b_ok && !p_sys->track[i].b_chapter )
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
        else if( p_sys->track[i].b_ok && p_sys->track[i].b_chapter )
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

    if ( p_sys->i_overall_duration == 0 )
    {
        /* Try in mehd if fragmented */
        MP4_Box_t *p_mehd = MP4_BoxGet( p_demux->p_sys->p_root, "moov/mvex/mehd");
        if ( p_mehd && p_mehd->data.p_mehd )
            p_sys->i_overall_duration = p_mehd->data.p_mehd->i_fragment_duration;
        else
        {
            for( i = 0; i < p_sys->i_tracks; i++ )
            {
                mtime_t i_duration = GetTrackDurationInFragment( &p_sys->moovfragment,
                                                                 p_sys->track[i].i_track_ID );
                p_sys->i_overall_duration = __MAX( p_sys->i_overall_duration, (uint64_t)i_duration );
            }
        }
    }

#ifdef MP4_VERBOSE
    mtime_t i_total_duration = 0;
    mp4_fragment_t *p_fragment = &p_sys->moovfragment;
    while ( p_fragment && p_sys->i_tracks )
    {
        if ( (p_fragment != &p_sys->moovfragment || p_fragment->i_chunk_range_max_offset) &&
             p_fragment->i_durations && p_fragment->p_durations[0].i_track_ID == p_sys->track[0].i_track_ID )
            i_total_duration += CLOCK_FREQ * p_fragment->p_durations[0].i_duration / p_sys->i_timescale;
        msg_Dbg( p_demux, "fragment offset %"PRId64", data %"PRIu64"<->%"PRIu64", "
                 "duration %"PRId64" @%"PRId64,
                 p_fragment->p_moox->i_pos, p_fragment->i_chunk_range_min_offset,
                 p_fragment->i_chunk_range_max_offset,
                 ( p_fragment->i_durations && p_fragment->p_durations[0].i_track_ID == p_sys->track[0].i_track_ID )?
                 CLOCK_FREQ * p_fragment->p_durations[0].i_duration / p_sys->i_timescale : 0, i_total_duration );
        p_fragment = p_fragment->p_next;
    }
#endif

    if( !p_sys->b_fragmented && p_sys->i_tracks > 1 && p_sys->b_seekable && !p_sys->b_seekmode )
    {
        for( unsigned i = 0; i < p_sys->i_tracks; i++ )
        {
            mp4_track_t *tk = &p_sys->track[i];
            if( !MP4_TrackIsInterleaved( tk ) )
            {
                msg_Warn( p_demux, "that media doesn't look interleaved, will need to seek");
                p_sys->b_seekmode = true;
                break;
            }
        }
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
    if( stream_Tell( p_demux->s ) > 0 )
        stream_Seek( p_demux->s, 0 );

    if( p_sys->p_root )
    {
        MP4_BoxFree( p_demux->s, p_sys->p_root );
    }
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Demux: read packet and send them to decoders
 *****************************************************************************
 * TODO check for newly selected track (ie audio upt to now )
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned int i_track;


    unsigned int i_track_selected;

    /* check for newly selected/unselected track */
    for( i_track = 0, i_track_selected = 0; i_track < p_sys->i_tracks;
         i_track++ )
    {
        mp4_track_t *tk = &p_sys->track[i_track];
        bool b;

        if( !tk->b_ok || tk->b_chapter ||
            ( tk->b_selected && tk->i_sample >= tk->i_sample_count ) )
        {
            continue;
        }

        es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE, tk->p_es, &b );

        if( tk->b_selected && !b )
        {
            MP4_TrackUnselect( p_demux, tk );
        }
        else if( !tk->b_selected && b)
        {
            MP4_TrackSelect( p_demux, tk, MP4_GetMoviePTS( p_sys ) );
        }

        if( tk->b_selected )
        {
            i_track_selected++;
        }
    }

    if( i_track_selected <= 0 )
    {
        p_sys->i_time += __MAX( p_sys->i_timescale / 10 , 1 );
        if( p_sys->i_timescale > 0 )
        {
            int64_t i_length = CLOCK_FREQ *
                               (mtime_t)p_sys->i_overall_duration /
                               (mtime_t)p_sys->i_timescale;
            if( MP4_GetMoviePTS( p_sys ) >= i_length )
                return 0;
            return 1;
        }

        msg_Warn( p_demux, "no track selected, exiting..." );
        return 0;
    }

    /* */
    MP4_UpdateSeekpoint( p_demux );

    /* first wait for the good time to read a packet */
    p_sys->i_pcr = MP4_GetMoviePTS( p_sys );

    bool b_data_sent = false;

    /* Find next track matching contiguous data */
    mp4_track_t *tk = NULL;
    uint64_t i_candidate_pos = UINT64_MAX;
    mtime_t i_candidate_dts = INT64_MAX;
    for( i_track = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        mp4_track_t *tk_tmp = &p_sys->track[i_track];
        if( !tk_tmp->b_ok || tk_tmp->b_chapter || !tk_tmp->b_selected || tk_tmp->i_sample >= tk_tmp->i_sample_count )
            continue;

        if ( p_sys->b_seekmode )
        {
            mtime_t i_dts = MP4_TrackGetDTS( p_demux, tk_tmp );
            if ( i_dts <= i_candidate_dts )
            {
                tk = tk_tmp;
                i_candidate_dts = i_dts;
                i_candidate_pos = MP4_TrackGetPos( tk_tmp );
            }
        }
        else
        {
            /* Try to avoid seeking on non fastseekable. Will fail with non interleaved content */
            uint64_t i_pos = MP4_TrackGetPos( tk_tmp );
            if ( i_pos <= i_candidate_pos )
            {
                i_candidate_pos = i_pos;
                tk = tk_tmp;
            }
        }
    }

    if ( !tk )
    {
        msg_Dbg( p_demux, "Could not select track by data position" );
        goto end;
    }
    else if ( p_sys->b_seekmode )
    {
        if( stream_Seek( p_demux->s, i_candidate_pos ) )
        {
            msg_Warn( p_demux, "track[0x%x] will be disabled (eof?)",
                      tk->i_track_ID );
            MP4_TrackUnselect( p_demux, tk );
            goto end;
        }
    }

#if 0
    msg_Dbg( p_demux, "tk(%i)=%"PRId64" mv=%"PRId64" pos=%"PRIu64, tk->i_track_ID,
             MP4_TrackGetDTS( p_demux, tk ),
             MP4_GetMoviePTS( p_sys ), i_candidate_pos );
#endif

    uint32_t i_nb_samples = 0;
    uint32_t i_samplessize = MP4_TrackGetReadSize( tk, &i_nb_samples );
    if( i_samplessize > 0 )
    {
        block_t *p_block;
        int64_t i_delta;
        uint64_t i_current_pos;

        /* go,go go ! */
        if ( !MP4_stream_Tell( p_demux->s, &i_current_pos ) )
            goto end;

        if( i_current_pos != i_candidate_pos )
        {
            if( stream_Seek( p_demux->s, i_candidate_pos ) )
            {
                msg_Warn( p_demux, "track[0x%x] will be disabled (eof?)"
                          ": Failed to seek to %"PRIu64,
                          tk->i_track_ID, i_candidate_pos );
                MP4_TrackUnselect( p_demux, tk );
                goto end;
            }
            i_current_pos = i_candidate_pos;
        }

        /* now read pes */
        if( !(p_block = MP4_Block_Read( p_demux, tk, i_samplessize )) )
        {
            msg_Warn( p_demux, "track[0x%x] will be disabled (eof?)"
                      ": Failed to read %d bytes sample at %"PRIu64,
                      tk->i_track_ID, i_samplessize, i_current_pos );
            MP4_TrackUnselect( p_demux, tk );
            goto end;
        }

        /* dts */
        p_block->i_dts = VLC_TS_0 + MP4_TrackGetDTS( p_demux, tk );
        /* pts */
        if( MP4_TrackGetPTSDelta( p_demux, tk, &i_delta ) )
            p_block->i_pts = p_block->i_dts + i_delta;
        else if( tk->fmt.i_cat != VIDEO_ES )
            p_block->i_pts = p_block->i_dts;
        else
            p_block->i_pts = VLC_TS_INVALID;

        if ( !b_data_sent )
        {
            es_out_Control( p_demux->out, ES_OUT_SET_PCR, VLC_TS_0 + p_sys->i_pcr );
            b_data_sent = true;
        }
        MP4_Block_Send( p_demux, tk, p_block );
    }

    /* Next sample */
    if ( i_nb_samples ) /* sample size could be 0, need to go fwd. see return */
        MP4_TrackNextSample( p_demux, tk, i_nb_samples );

end:
    if ( b_data_sent )
    {
        p_sys->i_pcr = INT64_MAX;
        for( i_track = 0; i_track < p_sys->i_tracks; i_track++ )
        {
            mp4_track_t *tk = &p_sys->track[i_track];
            if ( !tk->b_ok || !tk->b_selected  ||
                 (tk->fmt.i_cat != AUDIO_ES && tk->fmt.i_cat != VIDEO_ES) )
                continue;

            mtime_t i_dts = MP4_TrackGetDTS( p_demux, tk );
            p_sys->i_pcr = __MIN( i_dts, p_sys->i_pcr );
            p_sys->i_time = p_sys->i_pcr * p_sys->i_timescale / CLOCK_FREQ;
        }
    }

    return b_data_sent || ( i_samplessize == 0 && i_nb_samples );
}

static void MP4_UpdateSeekpoint( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int64_t i_time;
    int i;
    if( !p_sys->p_title )
        return;
    i_time = MP4_GetMoviePTS( p_sys );
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
static int Seek( demux_t *p_demux, mtime_t i_date )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned int i_track;

    /* First update global time */
    p_sys->i_time = i_date * p_sys->i_timescale / CLOCK_FREQ;
    p_sys->i_pcr  = VLC_TS_INVALID;

    /* Now for each stream try to go to this time */
    for( i_track = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        mp4_track_t *tk = &p_sys->track[i_track];
        MP4_TrackSeek( p_demux, tk, i_date );
    }
    MP4_UpdateSeekpoint( p_demux );

    MP4ASF_ResetFrames( p_sys );
    es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME, i_date );

    return VLC_SUCCESS;
}

static int LeafSeekIntoFragment( demux_t *p_demux, mp4_fragment_t *p_fragment )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    uint64_t i64 = p_fragment->i_chunk_range_min_offset;

    if ( p_fragment->p_moox->i_type == ATOM_moov )
    {
        mp4_track_t *p_track;
        unsigned int i_chunk;
        int i_ret = LeafGetTrackAndChunkByMOOVPos( p_demux, &i64, &p_track, &i_chunk );
        if ( i_ret == VLC_EGENERIC )
        {
            msg_Dbg( p_demux, "moov seek failed to identify %"PRIu64, i64 );
            return i_ret;
        }
        msg_Dbg( p_demux, "moov seeking to %"PRIu64, i64 );
    }
    else
    {
        i64 = p_fragment->i_chunk_range_min_offset;
        msg_Dbg( p_demux, "moof seeking to %"PRIu64, i64 );
    }

    if( stream_Seek( p_demux->s, i64 ) )
    {
        msg_Err( p_demux, "seek failed to %"PRIu64, i64 );
        return VLC_EGENERIC;
    }

    /* map context */
    p_sys->context.p_fragment = p_fragment;
    p_sys->context.i_current_box_type = ATOM_mdat;
    LeafMapTrafTrunContextes( p_demux, p_fragment->p_moox );
    p_sys->context.i_mdatbytesleft = p_fragment->i_chunk_range_max_offset - i64;

    mtime_t i_time_base = 0;
    for( unsigned int i_track = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        mtime_t i_tracktime = LeafGetTrackFragmentTimeOffset( p_demux, p_fragment,
                                p_sys->track[i_track].i_track_ID );
        p_sys->track[i_track].i_time = i_tracktime *  p_sys->track[i_track].i_timescale / p_sys->i_timescale;
        i_time_base = __MIN( i_time_base, i_tracktime );
    }
    p_sys->i_time = i_time_base;
    p_sys->i_pcr  = VLC_TS_INVALID;

    return VLC_SUCCESS;
}

static int LeafSeekToTime( demux_t *p_demux, mtime_t i_nztime )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    mp4_fragment_t *p_fragment;
    uint64_t i64 = 0;
    if ( !p_sys->i_timescale || !p_sys->i_overall_duration || !p_sys->b_seekable )
         return VLC_EGENERIC;

    if ( !p_sys->b_fragments_probed && !p_sys->b_index_probed && p_sys->b_seekable )
    {
        ProbeIndex( p_demux );
        p_sys->b_index_probed = true;
    }

    p_fragment = GetFragmentByTime( p_demux, i_nztime );
    if ( !p_fragment )
    {
        mtime_t i_mooftime;
        msg_Dbg( p_demux, "seek can't find matching fragment for %"PRId64", trying index", i_nztime );
        if ( LeafIndexGetMoofPosByTime( p_demux, i_nztime, &i64, &i_mooftime ) == VLC_SUCCESS )
        {
            msg_Dbg( p_demux, "seek trying to go to unknown but indexed fragment at %"PRId64, i64 );
            if( stream_Seek( p_demux->s, i64 ) )
            {
                msg_Err( p_demux, "seek to moof failed %"PRId64, i64 );
                return VLC_EGENERIC;
            }
            p_sys->context.i_current_box_type = 0;
            p_sys->context.i_mdatbytesleft = 0;
            p_sys->context.p_fragment = NULL;
            for( unsigned int i_track = 0; i_track < p_sys->i_tracks; i_track++ )
            {
                p_sys->track[i_track].i_time = i_mooftime / CLOCK_FREQ * p_sys->track[i_track].i_timescale;
            }
            p_sys->i_time = i_mooftime / CLOCK_FREQ * p_sys->i_timescale;
            p_sys->i_pcr  = VLC_TS_INVALID;
        }
        else
        {
            msg_Warn( p_demux, "seek by index failed" );
            return VLC_EGENERIC;
        }
    }
    else
    {
        msg_Dbg( p_demux, "seeking to fragment data starting at %"PRIu64" for time %"PRId64,
                           p_fragment->i_chunk_range_min_offset, i_nztime );
        if ( LeafSeekIntoFragment( p_demux, p_fragment ) != VLC_SUCCESS )
            return VLC_EGENERIC;
    }

    MP4ASF_ResetFrames( p_sys );
    /* And set next display time in that trun/fragment */
    es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME, VLC_TS_0 + i_nztime );
    return VLC_SUCCESS;
}

static int LeafSeekToPos( demux_t *p_demux, double f )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    if ( !p_sys->b_seekable )
        return VLC_EGENERIC;

    if ( p_sys->i_timescale && p_sys->i_overall_duration )
    {
        return LeafSeekToTime( p_demux,
                    (mtime_t)( f * CLOCK_FREQ * p_sys->i_overall_duration /
                               p_sys->i_timescale ) );
    }

    if ( !p_sys->b_fragments_probed && !p_sys->b_index_probed && p_sys->b_seekable )
    {
        ProbeIndex( p_demux );
        p_sys->b_index_probed = true;
    }

    /* Blind seek to pos only */
    uint64_t i64 = (uint64_t) stream_Size( p_demux->s ) * f;
    mp4_fragment_t *p_fragment = GetFragmentByPos( p_demux, i64, false );
    if ( p_fragment )
    {
        msg_Dbg( p_demux, "Seeking to fragment data starting at %"PRIu64" for pos %"PRIu64,
                 p_fragment->i_chunk_range_min_offset, i64 );
        return LeafSeekIntoFragment( p_demux, p_fragment );
    }
    else
    {
        msg_Dbg( p_demux, "Cant get fragment for data starting at %"PRIu64, i64 );
        return VLC_EGENERIC;
    }
}

static int MP4_frg_Seek( demux_t *p_demux, double f )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    int64_t i64 = stream_Size( p_demux->s );
    if( stream_Seek( p_demux->s, (int64_t)(i64 * f) ) )
    {
        return VLC_EGENERIC;
    }
    else
    {
        /* update global time */
        p_sys->i_time = (uint64_t)(f * (double)p_sys->i_overall_duration);
        p_sys->i_pcr  = MP4_GetMoviePTS( p_sys );

        for( unsigned i_track = 0; i_track < p_sys->i_tracks; i_track++ )
        {
            mp4_track_t *tk = &p_sys->track[i_track];

            /* We don't want the current chunk to be flushed */
            tk->cchunk->i_sample = tk->cchunk->i_sample_count;

            /* reset/update some values */
            tk->i_sample = tk->i_sample_first = 0;
            tk->i_first_dts = p_sys->i_time;

            /* We want to discard the current chunk and get the next one at once */
            tk->b_has_non_empty_cchunk = false;
        }
        es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME, p_sys->i_pcr );
        return VLC_SUCCESS;
    }
}

static bool imageTypeCompatible( const MP4_Box_data_data_t *p_data )
{
    return p_data && (
    p_data->e_wellknowntype == DATA_WKT_PNG ||
    p_data->e_wellknowntype == DATA_WKT_JPEG ||
    p_data->e_wellknowntype == DATA_WKT_BMP );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    double f, *pf;
    int64_t i64, *pi64;

    const char *psz_roots[] = { "/moov/udta/meta/ilst",
                                "/moov/meta/ilst",
                                "/moov/udta/meta",
                                "/moov/udta",
                                "/meta/ilst",
                                "/udta",
                                NULL };
    const uint32_t rgi_pict_atoms[2] = { ATOM_PICT, ATOM_pict };

    switch( i_query )
    {
        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double * );
            if( p_sys->i_overall_duration > 0 )
            {
                *pf = (double)p_sys->i_time / (double)p_sys->i_overall_duration;
            }
            else
            {
                *pf = 0.0;
            }
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            f = (double)va_arg( args, double );
            if ( p_demux->pf_demux == DemuxAsLeaf )
                return LeafSeekToPos( p_demux, f );
            else if ( p_demux->pf_demux == DemuxFrg )
                return MP4_frg_Seek( p_demux, f );
            else if( p_sys->i_timescale > 0 )
            {
                i64 = (int64_t)( f * CLOCK_FREQ *
                                 (double)p_sys->i_overall_duration /
                                 (double)p_sys->i_timescale );
                return Seek( p_demux, i64 );
            }
            else return VLC_EGENERIC;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->i_timescale > 0 )
            {
                *pi64 = CLOCK_FREQ *
                        (mtime_t)p_sys->i_time /
                        (mtime_t)p_sys->i_timescale;
            }
            else *pi64 = 0;
            return VLC_SUCCESS;

        case DEMUX_SET_TIME:
            i64 = (int64_t)va_arg( args, int64_t );
            if ( p_demux->pf_demux == DemuxAsLeaf )
                return LeafSeekToTime( p_demux, i64 );
            else
                return Seek( p_demux, i64 );

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->i_timescale > 0 )
            {
                *pi64 = CLOCK_FREQ *
                        (mtime_t)p_sys->i_overall_duration /
                        (mtime_t)p_sys->i_timescale;
            }
            else *pi64 = 0;
            return VLC_SUCCESS;

        case DEMUX_GET_FPS:
            pf = (double*)va_arg( args, double* );
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
            for( ; psz_roots[i_index] && !p_udta; i_index++ )
            {
                p_udta = MP4_BoxGet( p_sys->p_root, psz_roots[i_index] );
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
                    malloc( sizeof(input_attachment_t*) * i_count );
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

                    if ( asprintf( &psz_filename, "%s/covr/data[%"PRIu64"]", psz_roots[i_index - 1],
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
            vlc_meta_t *p_meta = (vlc_meta_t *)va_arg( args, vlc_meta_t*);

            MP4_Box_t *p_data = NULL;
            MP4_Box_t *p_udta = NULL;
            bool b_attachment_set = false;

            for( int i_index = 0; psz_roots[i_index] && !p_udta; i_index++ )
            {
                p_udta = MP4_BoxGet( p_sys->p_root, psz_roots[i_index] );
                if ( p_udta )
                {
                    p_data = MP4_BoxGet( p_udta, "covr/data" );
                    if ( p_data && imageTypeCompatible( BOXDATA(p_data) ) )
                    {
                        char *psz_attachment;
                        if ( -1 != asprintf( &psz_attachment, "attachment://%s/covr/data[0]",
                                             psz_roots[i_index] ) )
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

            if( p_udta == NULL && !b_attachment_set )
                return VLC_EGENERIC;

            SetupMeta( p_meta, p_udta );

            return VLC_SUCCESS;
        }

        case DEMUX_GET_TITLE_INFO:
        {
            input_title_t ***ppp_title = (input_title_t***)va_arg( args, input_title_t*** );
            int *pi_int    = (int*)va_arg( args, int* );
            int *pi_title_offset = (int*)va_arg( args, int* );
            int *pi_seekpoint_offset = (int*)va_arg( args, int* );

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
            const int i_title = (int)va_arg( args, int );
            if( !p_sys->p_title || i_title != 0 )
                return VLC_EGENERIC;
            return VLC_SUCCESS;
        }
        case DEMUX_SET_SEEKPOINT:
        {
            const int i_seekpoint = (int)va_arg( args, int );
            if( !p_sys->p_title )
                return VLC_EGENERIC;
            return Seek( p_demux, p_sys->p_title->seekpoint[i_seekpoint]->i_time_offset );
        }
        case DEMUX_GET_PTS_DELAY:
        {
            for( unsigned int i = 0; i < p_sys->i_tracks; i++ )
            {
                const MP4_Box_t *p_load;
                if ( (p_load = MP4_BoxGet( p_sys->track[i].p_track, "load" )) &&
                     BOXDATA(p_load)->i_duration > 0 )
                {
                    *va_arg(args, int64_t *) = BOXDATA(p_load)->i_duration *
                            CLOCK_FREQ / p_sys->track[i].i_timescale;
                    return VLC_SUCCESS;
                }
            }
            return VLC_EGENERIC;
        }
        case DEMUX_SET_NEXT_DEMUX_TIME:
        case DEMUX_SET_GROUP:
        case DEMUX_HAS_UNSUPPORTED_META:
        case DEMUX_CAN_RECORD:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_demux, "control query %u unimplemented", i_query );
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

    MP4_BoxFree( p_demux->s, p_sys->p_root );
    for( i_track = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        MP4_TrackDestroy( p_demux, &p_sys->track[i_track] );
    }
    free( p_sys->track );

    if( p_sys->p_title )
        vlc_input_title_Delete( p_sys->p_title );

    while( p_sys->moovfragment.p_next )
    {
        mp4_fragment_t *p_fragment = p_sys->moovfragment.p_next->p_next;
        free( p_sys->moovfragment.p_next->p_durations );
        free( p_sys->moovfragment.p_next );
        p_sys->moovfragment.p_next = p_fragment;
    }
    free( p_sys->moovfragment.p_durations );

    free( p_sys );
}



/****************************************************************************
 * Local functions, specific to vlc
 ****************************************************************************/
/* Chapters */
static void LoadChapterGpac( demux_t  *p_demux, MP4_Box_t *p_chpl )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i;

    p_sys->p_title = vlc_input_title_New();
    for( i = 0; i < BOXDATA(p_chpl)->i_chapter; i++ )
    {
        seekpoint_t *s = vlc_seekpoint_New();

        s->psz_name = strdup( BOXDATA(p_chpl)->chapter[i].psz_name );
        EnsureUTF8( s->psz_name );
        s->i_time_offset = BOXDATA(p_chpl)->chapter[i].i_start / 10;
        TAB_APPEND( p_sys->p_title->i_seekpoint, p_sys->p_title->seekpoint, s );
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

        if( i_size > 0 && !stream_Seek( p_demux->s, MP4_TrackGetPos( tk ) ) )
        {
            char p_buffer[256];
            const uint32_t i_read = stream_ReadU32( p_demux->s, p_buffer,
                                                    __MIN( sizeof(p_buffer), i_size ) );
            const uint32_t i_len = __MIN( GetWBE(p_buffer), i_read-2 );

            if( i_len > 0 )
            {
                seekpoint_t *s = vlc_seekpoint_New();

                s->psz_name = strndup( &p_buffer[2], i_len );
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

    if( ( p_chpl = MP4_BoxGet( p_sys->p_root, "/moov/udta/chpl" ) ) &&
          BOXDATA(p_chpl) && BOXDATA(p_chpl)->i_chapter > 0 )
    {
        LoadChapterGpac( p_demux, p_chpl );
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
        p_sys->p_title->i_length = CLOCK_FREQ *
                       (uint64_t)p_sys->i_overall_duration / (uint64_t)p_sys->i_timescale;
    }
}

static bool MP4_TrackIsInterleaved( const mp4_track_t *p_track )
{
    const MP4_Box_t *p_stsc = MP4_BoxGet( p_track->p_stbl, "stsc" );
    const MP4_Box_t *p_stsz = MP4_BoxGet( p_track->p_stbl, "stsz" );
    if( p_stsc && BOXDATA(p_stsc) && p_stsz && BOXDATA(p_stsz) )
    {
        if( BOXDATA(p_stsc)->i_entry_count == 1 &&
            BOXDATA(p_stsz)->i_sample_count > 1 &&
            BOXDATA(p_stsc)->i_samples_per_chunk[0] == BOXDATA(p_stsz)->i_sample_count )
            return false;
    }

    return true;
}

/* now create basic chunk data, the rest will be filled by MP4_CreateSamplesIndex */
static int TrackCreateChunksIndex( demux_t *p_demux,
                                   mp4_track_t *p_demux_track )
{
    demux_sys_t *p_sys = p_demux->p_sys;

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

    if ( p_demux_track->i_chunk_count )
    {
        p_demux_track->chunk[0].i_sample_first = 0;
        for( i_chunk = 1; i_chunk < p_demux_track->i_chunk_count; i_chunk++ )
        {
            p_demux_track->chunk[i_chunk].i_sample_first =
                p_demux_track->chunk[i_chunk-1].i_sample_first +
                    p_demux_track->chunk[i_chunk-1].i_sample_count;
        }
    }

    msg_Dbg( p_demux, "track[Id 0x%x] read %d chunk",
             p_demux_track->i_track_ID, p_demux_track->i_chunk_count );

    if ( p_demux_track->i_chunk_count && (
             p_sys->moovfragment.i_chunk_range_min_offset == 0 ||
             p_sys->moovfragment.i_chunk_range_min_offset > p_demux_track->chunk[0].i_offset
             ) )
        p_sys->moovfragment.i_chunk_range_min_offset = p_demux_track->chunk[0].i_offset;

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
    demux_sys_t *p_sys = p_demux->p_sys;

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
    p_demux_track->i_sample_count = stsz->i_sample_count;
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

    if ( p_demux_track->i_chunk_count )
    {
        mp4_chunk_t *lastchunk = &p_demux_track->chunk[p_demux_track->i_chunk_count - 1];
        uint64_t i_total_size = lastchunk->i_offset;

        if ( p_demux_track->i_sample_size != 0 ) /* all samples have same size */
        {
            i_total_size += (uint64_t)p_demux_track->i_sample_size * lastchunk->i_sample_count;
        }
        else
        {
            if( (uint64_t)lastchunk->i_sample_count + p_demux_track->i_chunk_count - 1 > stsz->i_sample_count )
            {
                msg_Err( p_demux, "invalid samples table: stsz table is too small" );
                return VLC_EGENERIC;
            }

            for( uint32_t i=stsz->i_sample_count - lastchunk->i_sample_count;
                 i<stsz->i_sample_count; i++)
            {
                i_total_size += stsz->i_entry_size[i];
            }
        }

        if ( i_total_size > p_sys->moovfragment.i_chunk_range_max_offset )
            p_sys->moovfragment.i_chunk_range_max_offset = i_total_size;
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
            ck->i_last_dts  = i_next_dts;

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
                        if ( i_sample_count ) ck->i_last_dts = i_next_dts;
                        ck->p_sample_count_dts[i] = i_sample_count;
                        ck->p_sample_delta_dts[i] = stts->pi_sample_delta[i_index];
                        i_next_dts += ck->p_sample_count_dts[i] * stts->pi_sample_delta[i_index];
                        i_current_index_samples_left -= i_sample_count;
                        i_sample_count = 0;
                        assert( i == ck->i_entries_dts - 1 );
                        break;
                    }
                    else
                    {
                        if ( i_current_index_samples_left ) ck->i_last_dts = i_next_dts;
                        ck->p_sample_count_dts[i] = i_current_index_samples_left;
                        ck->p_sample_delta_dts[i] = stts->pi_sample_delta[i_index];
                        i_next_dts += ck->p_sample_count_dts[i] * stts->pi_sample_delta[i_index];
                        i_sample_count -= i_current_index_samples_left;
                        i_current_index_samples_left = 0;
                        i_index++;
                    }
                }
                else
                {
                    if ( stts->pi_sample_count[i_index] > i_sample_count )
                    {
                        if ( i_sample_count ) ck->i_last_dts = i_next_dts;
                        ck->p_sample_count_dts[i] = i_sample_count;
                        ck->p_sample_delta_dts[i] = stts->pi_sample_delta[i_index];
                        i_next_dts += ck->p_sample_count_dts[i] * stts->pi_sample_delta[i_index];
                        i_current_index_samples_left = stts->pi_sample_count[i_index] - i_sample_count;
                        i_sample_count = 0;
                        assert( i == ck->i_entries_dts - 1 );
                        // keep building from same index
                    }
                    else
                    {
                        if ( stts->pi_sample_count[i_index] ) ck->i_last_dts = i_next_dts;
                        ck->p_sample_count_dts[i] = stts->pi_sample_count[i_index];
                        ck->p_sample_delta_dts[i] = stts->pi_sample_delta[i_index];
                        i_next_dts += ck->p_sample_count_dts[i] * stts->pi_sample_delta[i_index];
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
                        ck->p_sample_offset_pts[i] = ctts->pi_sample_offset[i_index];
                        i_current_index_samples_left -= i_sample_count;
                        i_sample_count = 0;
                        assert( i == ck->i_entries_pts - 1 );
                        break;
                    }
                    else
                    {
                        ck->p_sample_count_pts[i] = i_current_index_samples_left;
                        ck->p_sample_offset_pts[i] = ctts->pi_sample_offset[i_index];
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
                        ck->p_sample_offset_pts[i] = ctts->pi_sample_offset[i_index];
                        i_current_index_samples_left = ctts->pi_sample_count[i_index] - i_sample_count;
                        i_sample_count = 0;
                        assert( i == ck->i_entries_pts - 1 );
                        // keep building from same index
                    }
                    else
                    {
                        ck->p_sample_count_pts[i] = ctts->pi_sample_count[i_index];
                        ck->p_sample_offset_pts[i] = ctts->pi_sample_offset[i_index];
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
    uint64_t i_first_dts = p_chunk->i_first_dts;
    uint64_t i_last_dts;
    do
    {
        i_sample += p_chunk->i_sample_count;
        i_last_dts = p_chunk->i_last_dts;
        p_chunk++;
    }
    while( p_chunk < &p_track->chunk[p_track->i_chunk_count] &&
           p_chunk->i_sample_description_index == i_sd_index );

    if( i_sample > 1 && i_first_dts < i_last_dts )
        vlc_ureduce( pi_num, pi_den,
                     ( i_sample - 1) *  p_track->i_timescale,
                     i_last_dts - i_first_dts,
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
        if ( !SetupVideoES( p_demux, p_track, p_sample ) )
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
        if ( !SetupAudioES( p_demux, p_track, p_sample ) )
            return VLC_EGENERIC;
        break;

    case SPU_ES:
        switch( p_sample->i_handler )
        {
            case ATOM_clcp:
                if ( !SetupCCES( p_demux, p_track, p_sample ) )
                    return VLC_EGENERIC;
                break;
            default:
                if ( !SetupSpuES( p_demux, p_track, p_sample ) )
                 return VLC_EGENERIC;
        }

    default:
        break;
    }

    if( pp_es )
        *pp_es = es_out_Add( p_demux->out, &p_track->fmt );

    return VLC_SUCCESS;
}

/* given a time it return sample/chunk
 * it also update elst field of the track
 */
static int TrackTimeToSampleChunk( demux_t *p_demux, mp4_track_t *p_track,
                                   int64_t i_start, uint32_t *pi_chunk,
                                   uint32_t *pi_sample )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    MP4_Box_t   *p_box_stss;
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
        int64_t i_mvt= i_start * p_sys->i_timescale / CLOCK_FREQ;

        /* now calculate i_start for this elst */
        /* offset */
        i_start -= p_track->i_elst_time * CLOCK_FREQ / p_sys->i_timescale;
        if( i_start < 0 )
        {
            *pi_chunk = 0;
            *pi_sample= 0;

            return VLC_SUCCESS;
        }
        /* to track time scale */
        i_start  = i_start * p_track->i_timescale / CLOCK_FREQ;
        /* add elst offset */
        if( ( elst->i_media_rate_integer[p_track->i_elst] > 0 ||
             elst->i_media_rate_fraction[p_track->i_elst] > 0 ) &&
            elst->i_media_time[p_track->i_elst] > 0 )
        {
            i_start += elst->i_media_time[p_track->i_elst];
        }

        msg_Dbg( p_demux, "elst (%d) gives %"PRId64"ms (movie)-> %"PRId64
                 "ms (track)", p_track->i_elst,
                 i_mvt * 1000 / p_sys->i_timescale,
                 i_start * 1000 / p_track->i_timescale );
    }
    else
    {
        /* convert absolute time to in timescale unit */
        i_start = i_start * p_track->i_timescale / CLOCK_FREQ;
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
    if( ( p_box_stss = MP4_BoxGet( p_track->p_stbl, "stss" ) ) )
    {
        MP4_Box_data_stss_t *p_stss = p_box_stss->data.p_stss;
        msg_Dbg( p_demux, "track[Id 0x%x] using Sync Sample Box (stss)",
                 p_track->i_track_ID );
        for( unsigned i_index = 0; i_index < p_stss->i_entry_count; i_index++ )
        {
            if( i_index >= p_stss->i_entry_count - 1 ||
                i_sample < p_stss->i_sample_number[i_index+1] )
            {
                unsigned i_sync_sample = p_stss->i_sample_number[i_index];
                msg_Dbg( p_demux, "stss gives %d --> %d (sample number)",
                         i_sample, i_sync_sample );

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
                break;
            }
        }
    }
    else
    {
        msg_Dbg( p_demux, "track[Id 0x%x] does not provide Sync "
                 "Sample Box (stss)", p_track->i_track_ID );
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
    mp4_chunk_t *cchunk_backup = p_track->cchunk;
    p_track->cchunk = NULL;
    mtime_t time_backup = p_track->i_time * p_track->i_timescale;

    /* Save previous format and ES */
    es_format_t fmtbackup;
    es_out_id_t *p_es_backup = p_track->p_es;
    p_track->p_es = NULL;
    es_format_Init( &fmtbackup, UNKNOWN_ES, 0 );
    es_format_Copy( &fmtbackup, &p_track->fmt );
    es_format_Clean( &p_track->fmt );


    /* do the cleanup and recycle track / restart */
    MP4_TrackDestroy( p_demux, p_track );
    memset( p_track, 0, sizeof(*p_track) );

    if( p_demux->p_sys->b_smooth )
    {
        assert(p_params_box->i_type == ATOM_uuid && !CmpUUID( &p_params_box->i_uuid, &StraBoxUUID ) );
        MP4_SmoothTrackCreate( p_demux, p_track, cchunk_backup, p_params_box, false );
    }
    else
    {
        assert(p_params_box->i_type == ATOM_trak);
        MP4_TrackCreate( p_demux, p_track, p_params_box, false, true );
    }

    if( p_track->b_ok )
    {
        if( !es_format_IsSimilar( &fmtbackup, &p_track->fmt ) ||
            fmtbackup.i_extra != p_track->fmt.i_extra ||
            memcmp( &fmtbackup.p_extra, &p_track->fmt.p_extra, fmtbackup.i_extra ) )
        {
            if( p_es_backup )
                es_out_Del( p_demux->out, p_es_backup );

            if( !p_track->b_chapter )
            {
                p_track->p_es = es_out_Add( p_demux->out, &p_track->fmt );
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
    p_track->cchunk = cchunk_backup;
    p_track->i_sample = i_sample_pos_backup;
    if( p_track->i_timescale )
        p_track->i_time = time_backup / p_track->i_timescale;
}

/****************************************************************************
 * MP4_TrackCreate:
 ****************************************************************************
 * Parse track information and create all needed data to run a track
 * If it succeed b_ok is set to 1 else to 0
 ****************************************************************************/
static void MP4_TrackCreate( demux_t *p_demux, mp4_track_t *p_track,
                             MP4_Box_t *p_box_trak,
                             bool b_create_es, bool b_force_enable )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    p_track->p_track = p_box_trak;

    char language[4] = { '\0' };

    es_format_Init( &p_track->fmt, UNKNOWN_ES, 0 );

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

    p_track->i_timescale = BOXDATA(p_mdhd)->i_timescale;
    if( p_track->i_timescale == 0 )
        return;

    memcpy( &language, BOXDATA(p_mdhd)->rgs_language, 3 );
    p_track->b_mac_encoding = BOXDATA(p_mdhd)->b_mac_encoding;

    switch( p_hdlr->data.p_hdlr->i_handler_type )
    {
        case( ATOM_soun ):
            if( !MP4_BoxGet( p_box_trak, "mdia/minf/smhd" ) )
            {
                return;
            }
            p_track->fmt.i_cat = AUDIO_ES;
            break;

        case( ATOM_vide ):
            if( !MP4_BoxGet( p_box_trak, "mdia/minf/vmhd") )
            {
                return;
            }
            p_track->fmt.i_cat = VIDEO_ES;
            break;

        case( ATOM_tx3g ):
        case( ATOM_text ):
        case( ATOM_subp ):
        case( ATOM_sbtl ):
            p_track->fmt.i_cat = SPU_ES;
            break;

        /* closed captions */
        case( ATOM_clcp ):
            p_track->fmt.i_cat = SPU_ES;
            break;

        default:
            return;
    }

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
                     elst->i_segment_duration[i] * 1000 / p_sys->i_timescale,
                     elst->i_media_time[i] >= 0 ?
                     (int64_t)(elst->i_media_time[i] * 1000 / p_track->i_timescale) :
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
                        strdup( p_box_iter->data.p_string->psz_text );
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
                p_track->b_chapter = true;
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

    if( TrackCreateES( p_demux,
                       p_track, p_track->i_chunk,
                      (p_track->b_chapter || !b_create_es) ? NULL : &p_track->p_es ) )
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

    if( ck->p_sample_data )
    {
        for( uint32_t i = 0; i < ck->i_sample_count; i++ )
            free( ck->p_sample_data[i] );
        free( ck->p_sample_data );
    }
    free( ck->p_sample_size );
}

/****************************************************************************
 * MP4_TrackDestroy:
 ****************************************************************************
 * Destroy a track created by MP4_TrackCreate.
 ****************************************************************************/
static void MP4_TrackDestroy( demux_t *p_demux, mp4_track_t *p_track )
{
    es_format_Clean( &p_track->fmt );

    if( p_track->p_es )
        es_out_Del( p_demux->out, p_track->p_es );

    if( p_track->chunk )
    {
        for( unsigned int i_chunk = 0; i_chunk < p_track->i_chunk_count; i_chunk++ )
            DestroyChunk( &p_track->chunk[i_chunk] );
    }
    free( p_track->chunk );

    if( p_track->cchunk )
    {
        assert( p_demux->p_sys->b_fragmented );
        DestroyChunk( p_track->cchunk );
        free( p_track->cchunk );
    }

    if( !p_track->i_sample_size )
        free( p_track->p_sample_size );

    if ( p_track->asfinfo.p_frame )
        block_ChainRelease( p_track->asfinfo.p_frame );
}

static int MP4_TrackSelect( demux_t *p_demux, mp4_track_t *p_track,
                            mtime_t i_start )
{
    if( !p_track->b_ok || p_track->b_chapter )
    {
        return VLC_EGENERIC;
    }

    if( p_track->b_selected )
    {
        msg_Warn( p_demux, "track[Id 0x%x] already selected",
                  p_track->i_track_ID );
        return VLC_SUCCESS;
    }

    return MP4_TrackSeek( p_demux, p_track, i_start );
}

static void MP4_TrackUnselect( demux_t *p_demux, mp4_track_t *p_track )
{
    if( !p_track->b_ok || p_track->b_chapter )
    {
        return;
    }

    if( !p_track->b_selected )
    {
        msg_Warn( p_demux, "track[Id 0x%x] already unselected",
                  p_track->i_track_ID );
        return;
    }
    if( p_track->p_es )
    {
        es_out_Control( p_demux->out, ES_OUT_SET_ES_STATE,
                        p_track->p_es, false );
    }

    p_track->b_selected = false;
}

static int MP4_TrackSeek( demux_t *p_demux, mp4_track_t *p_track,
                          mtime_t i_start )
{
    uint32_t i_chunk;
    uint32_t i_sample;

    if( !p_track->b_ok || p_track->b_chapter )
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
        p_soun->i_compressionid == 0 && (p_track->i_sample_size == 1 || p_track->i_sample_size == 1) )
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
            MP4_TrackUnselect( p_demux, p_track );
            return VLC_EGENERIC;
        }
    }

    /* Have we changed elst */
    if( p_track->p_elst && p_track->BOXDATA(p_elst)->i_entry_count > 0 )
    {
        demux_sys_t *p_sys = p_demux->p_sys;
        MP4_Box_data_elst_t *elst = p_track->BOXDATA(p_elst);
        uint64_t i_mvt = MP4_TrackGetDTS( p_demux, p_track ) *
                        p_sys->i_timescale / CLOCK_FREQ;

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
        int64_t i_mvt= i_time * p_sys->i_timescale / CLOCK_FREQ;

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
    }
}

/******************************************************************************
 *     Here are the functions used for fragmented MP4
 *****************************************************************************/

/**
 * It computes the sample rate for a video track using current video chunk
 */
static void ChunkGetESSampleRate( unsigned *pi_num, unsigned *pi_den,
                                  const mp4_chunk_t *p_cchunk,
                                  uint32_t i_timescale )
{
    if( !p_cchunk || p_cchunk->i_last_dts == 0 || i_timescale == 0 )
        return;

    *pi_num = 0;
    *pi_den = 0;

    uint32_t i_sample = p_cchunk->i_sample_count;
    uint64_t i_first_dts = p_cchunk->i_first_dts;
    uint64_t i_last_dts =  p_cchunk->i_last_dts;

    if( i_sample > 1 && i_first_dts < i_last_dts )
        vlc_ureduce( pi_num, pi_den,
                     ( i_sample - 1) * i_timescale,
                     i_last_dts - i_first_dts,
                     UINT16_MAX);
}

/**
 * Build raw avcC box (without the 8 bytes header)
 * \return The size of the box.
 */
static int build_raw_avcC( uint8_t **p_extra, const uint8_t *CodecPrivateData,
                                                       const unsigned cpd_len )
{
    uint8_t *avcC;
    unsigned sps_len = 0, pps_len = 0;
    const uint32_t mark = 0x00000001;

    assert( CodecPrivateData[0] == 0 );
    assert( CodecPrivateData[1] == 0 );
    assert( CodecPrivateData[2] == 0 );
    assert( CodecPrivateData[3] == 1 );

    uint32_t length = cpd_len + 3;
    avcC = calloc( length, 1 );
    if( unlikely( avcC == NULL ) )
        return 0;

    uint8_t *sps = avcC + 8;

    uint32_t candidate = ~mark;
    CodecPrivateData += 4;
    for( unsigned i = 0; i < cpd_len - 4; i++ )
    {
        sps[i] = CodecPrivateData[i];
        candidate = (candidate << 8) | CodecPrivateData[i];
        if( candidate == mark )
        {
            sps_len = i - 3;
            break;
        }
    }
    if( sps_len == 0 )
    {
        free( avcC );
        return 0;
    }
    uint8_t *pps = sps + sps_len + 3;
    pps_len = cpd_len - sps_len - 4 * 2;
    memcpy( pps, CodecPrivateData + sps_len + 4, pps_len );

    /* XXX */
    uint8_t AVCProfileIndication = 0x64;
    uint8_t profile_compatibility = 0x40;
    uint8_t AVCLevelIndication = 0x1f;
    uint8_t lengthSizeMinusOne = 0x03;

    avcC[0] = 1;
    avcC[1] = AVCProfileIndication;
    avcC[2] = profile_compatibility;
    avcC[3] = AVCLevelIndication;
    avcC[4] = 0xfc + lengthSizeMinusOne;
    avcC[5] = 0xe0 + 1;
    avcC[6] = (sps_len & 0xff00)>>8;
    avcC[7] = sps_len & 0xff;

    avcC[8+sps_len] = 1;
    avcC[9+sps_len] = (pps_len & 0xff00) >> 8;
    avcC[10+sps_len] = pps_len & 0xff;

    *p_extra = avcC;
    return length;
}

/**
 * Build a mp4_track_t from a StraBox
 */

static inline int MP4_SetCodecExtraData( es_format_t *fmt, const MP4_Box_data_stra_t *p_data )
{
    fmt->p_extra = malloc( p_data->cpd_len );
    if( unlikely( !fmt->p_extra ) )
        return VLC_ENOMEM;
    fmt->i_extra = p_data->cpd_len;
    memcpy( fmt->p_extra, p_data->CodecPrivateData, p_data->cpd_len );
    return VLC_SUCCESS;
  }

static int MP4_SmoothFormatFill( const MP4_Box_data_stra_t *p_data,
                                 es_format_t *fmt )
{
    /* Set language FIXME */
    fmt->psz_language = strdup( "en" );

    fmt->i_cat = p_data->i_es_cat;
    fmt->i_original_fourcc = p_data->FourCC;
    fmt->i_codec = vlc_fourcc_GetCodec( fmt->i_cat, p_data->FourCC );

    uint8_t **p_extra = (uint8_t **)&fmt->p_extra;
    /* See http://msdn.microsoft.com/en-us/library/ff728116%28v=vs.90%29.aspx
     * for MS weird use of FourCC*/
    switch( fmt->i_cat )
    {
        case VIDEO_ES:
            if( p_data->FourCC == VLC_FOURCC( 'A', 'V', 'C', '1' ) ||
                p_data->FourCC == VLC_FOURCC( 'A', 'V', 'C', 'B' ) ||
                p_data->FourCC == VLC_FOURCC( 'H', '2', '6', '4' ) )
            {
                fmt->i_extra = build_raw_avcC( p_extra,
                        p_data->CodecPrivateData, p_data->cpd_len );
            }
            else
            {
                if ( MP4_SetCodecExtraData( fmt, p_data ) != VLC_SUCCESS )
                    return VLC_EGENERIC;
            }

            fmt->video.i_width = p_data->MaxWidth;
            fmt->video.i_height = p_data->MaxHeight;
            fmt->video.i_bits_per_pixel = 0x18;
            fmt->video.i_visible_width = p_data->MaxWidth;
            fmt->video.i_visible_height = p_data->MaxHeight;
            break;

        case AUDIO_ES:
            fmt->audio.i_channels = p_data->Channels;
            fmt->audio.i_rate = p_data->SamplingRate;
            fmt->audio.i_bitspersample = p_data->BitsPerSample;
            fmt->audio.i_blockalign = p_data->nBlockAlign;

            fmt->i_bitrate = p_data->Bitrate;

            if ( MP4_SetCodecExtraData( fmt, p_data ) != VLC_SUCCESS )
                return VLC_EGENERIC;

            break;

        default:
            break;
    }

    return VLC_SUCCESS;
}

static int MP4_SmoothTrackCreate( demux_t *p_demux, mp4_track_t *p_track,
                                  const mp4_chunk_t *p_cchunk,
                                  const MP4_Box_t *p_stra, bool b_create_es )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    es_format_t *fmt = &p_track->fmt;

    MP4_Box_data_stra_t *p_data = BOXDATA(p_stra);
    if( !p_data )
        return VLC_EGENERIC;

    p_track->i_sample_count = UINT32_MAX;

    p_track->i_timescale = p_sys->i_timescale;
    p_track->i_track_ID = p_data->i_track_ID;
    p_track->i_width = 0;
    p_track->i_height = 0;

    es_format_Init( fmt, p_data->i_es_cat, 0 );

    if( MP4_SmoothFormatFill( p_data, fmt ) != VLC_SUCCESS )
        return VLC_EGENERIC;

    if( fmt->i_cat == VIDEO_ES )
    {
        p_track->i_width = fmt->video.i_visible_width;
        p_track->i_height = fmt->video.i_visible_height;

        /* Frame rate */
        ChunkGetESSampleRate( &fmt->video.i_frame_rate,
                              &fmt->video.i_frame_rate_base,
                              p_cchunk, p_track->i_timescale );

        if( fmt->video.i_frame_rate_base != 0 )
        {
            p_demux->p_sys->f_fps = (float)fmt->video.i_frame_rate /
                                    (float)fmt->video.i_frame_rate_base;
        }
        else
            p_demux->p_sys->f_fps = 24;
    }

    if( b_create_es )
    {
        p_track->p_es = es_out_Add( p_demux->out, fmt );
        if( !p_track->p_es )
            return VLC_EGENERIC;
    }

    p_track->b_ok = true;

    return VLC_SUCCESS;
}

/**
 * Return the track identified by tid
 */
static mp4_track_t *MP4_frg_GetTrackByID( demux_t *p_demux, const uint32_t tid )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    mp4_track_t *ret = NULL;
    for( unsigned i = 0; i < p_sys->i_tracks; i++ )
    {
        ret = &p_sys->track[i];
        if( ret->i_track_ID == tid )
            return ret;
    }
    msg_Err( p_demux, "MP4_frg_GetTrack: track %"PRIu32" not found!", tid );
    return NULL;
}

static void FlushChunk( demux_t *p_demux, mp4_track_t *tk )
{
    msg_Dbg( p_demux, "Flushing chunk for track id %u", tk->i_track_ID );
    mp4_chunk_t *ck = tk->cchunk;
    while( ck->i_sample < ck->i_sample_count )
    {
        block_t *p_block;
        int64_t i_delta;

        if( ck->p_sample_size == NULL || ck->p_sample_data == NULL )
            return;

        uint32_t sample_size = ck->p_sample_size[ck->i_sample];
        assert( sample_size > 0 );
        p_block = block_Alloc( sample_size );
        if( unlikely( !p_block ) )
            return;

        uint8_t *src = ck->p_sample_data[ck->i_sample];
        memcpy( p_block->p_buffer, src, sample_size );
        ck->i_sample++;

        /* dts */
        p_block->i_dts = VLC_TS_0 + MP4_TrackGetDTS( p_demux, tk );
        /* pts */
        if( MP4_TrackGetPTSDelta( p_demux, tk, &i_delta ) )
            p_block->i_pts = p_block->i_dts + i_delta;
        else if( tk->fmt.i_cat != VIDEO_ES )
            p_block->i_pts = p_block->i_dts;
        else
            p_block->i_pts = VLC_TS_INVALID;

        MP4_Block_Send( p_demux, tk, p_block );

        tk->i_sample++;
    }
}

/**
 * Re-init decoder.
 * \Note If we call that function too soon,
 * before the track has been selected by MP4_TrackSelect
 * (during the first execution of Demux), then the track gets disabled
 */
static int ReInitDecoder( demux_t *p_demux, const MP4_Box_t *p_root,
                          mp4_track_t *p_track )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    MP4_Box_t *p_paramsbox; /* trak or stra */
    if( p_sys->b_smooth )
    {
        p_paramsbox = MP4_BoxGet( p_root, "uuid/uuid[0]" );
        if( !p_paramsbox || CmpUUID( &p_paramsbox->i_uuid, &StraBoxUUID ) )
            return VLC_EGENERIC;
    }
    else /* DASH */
    {
        p_paramsbox = MP4_BoxGet( p_root, "/moov/trak[0]" );
        if( !p_paramsbox )
            return VLC_EGENERIC;
    }

    MP4_TrackRestart( p_demux, p_track, p_paramsbox );

    /* Temporary hack until we support track selection */
    p_track->b_selected = true;
    p_track->b_enable = true;

    return VLC_SUCCESS;
}

/**
 * This function fills a mp4_chunk_t structure from a MP4_Box_t (p_chunk).
 * The 'i_tk_id' argument returns the ID of the track the chunk belongs to.
 * \note p_chunk usually contains a 'moof' and a 'mdat', and might contain a 'sidx'.
 * \return VLC_SUCCESS, VLC_EGENERIC or VLC_ENOMEM.
 */
static int MP4_frg_GetChunk( demux_t *p_demux, MP4_Box_t *p_chunk, unsigned *i_tk_id )
{
    MP4_Box_t *p_traf = MP4_BoxGet( p_chunk, "moof/traf" );
    if( p_traf == NULL)
    {
        msg_Warn( p_demux, "no traf box found!" );
        return VLC_EGENERIC;
    }

    MP4_Box_t *p_tfhd = MP4_BoxGet( p_traf, "tfhd" );
    if( p_tfhd == NULL)
    {
        msg_Warn( p_demux, "no tfhd box found!" );
        return VLC_EGENERIC;
    }

    uint32_t i_track_ID = BOXDATA(p_tfhd)->i_track_ID;
    *i_tk_id = i_track_ID;
    assert( i_track_ID > 0 );
    msg_Dbg( p_demux, "GetChunk: track ID is %"PRIu32"", i_track_ID );

    mp4_track_t *p_track = MP4_frg_GetTrackByID( p_demux, i_track_ID );
    if( !p_track )
        return VLC_EGENERIC;

    mp4_chunk_t *ret = p_track->cchunk;

    if( BOXDATA(p_tfhd)->b_empty )
        msg_Warn( p_demux, "No samples in this chunk!" );

    /* Usually we read 100 ms of each track. However, suppose we have two tracks,
     * Ta and Tv (audio and video). Suppose also that Ta is the first track to be
     * read, i.e. we read 100 ms of Ta, then 100 ms of Tv, then 100 ms of Ta,
     * and so on. Finally, suppose that we get the chunks the other way around,
     * i.e. first a chunk of Tv, then a chunk of Ta, then a chunk of Tv, and so on.
     * In that case, it is very likely that at some point, Ta->cchunk or Tv->cchunk
     * is not emptied when MP4_frg_GetChunks is called. It is therefore necessary to
     * flush it, i.e. send to the decoder the samples not yet sent.
     * Note that all the samples to be flushed should worth less than 100 ms,
     * (though I did not do the formal proof) and thus this flushing mechanism
     * should not cause A/V sync issues, or delays or whatever.
     */
    if( ret->i_sample < ret->i_sample_count )
        FlushChunk( p_demux, p_track );

    if( ret->i_sample_count )
    {
        DestroyChunk( ret );
        memset( ret, 0, sizeof(*ret) );
    }

    MP4_Box_t *p_trun = MP4_BoxGet( p_traf, "trun");
    if( p_trun == NULL)
    {
        msg_Warn( p_demux, "no trun box found!" );
        return VLC_EGENERIC;
    }
    MP4_Box_data_trun_t *p_trun_data = p_trun->data.p_trun;

    ret->i_sample_count = p_trun_data->i_sample_count;
    assert( ret->i_sample_count > 0 );
    ret->i_sample_description_index = 1; /* seems to be always 1, is it? */
    ret->i_sample_first = p_track->i_sample_first;
    p_track->i_sample_first += ret->i_sample_count;

    ret->i_first_dts = p_track->i_first_dts;

    uint32_t default_duration = 0;
    uint32_t default_size = 0;

    MP4_GetDefaultSizeAndDuration( p_demux, BOXDATA(p_tfhd),
                                   &default_size, &default_duration );

    ret->p_sample_count_dts = calloc( ret->i_sample_count, sizeof( uint32_t ) );
    ret->p_sample_delta_dts = calloc( ret->i_sample_count, sizeof( uint32_t ) );

    if( !ret->p_sample_count_dts || !ret->p_sample_delta_dts )
    {
        free( ret->p_sample_count_dts );
        free( ret->p_sample_delta_dts );
        return VLC_ENOMEM;
    }
    ret->i_entries_dts = ret->i_sample_count;

    ret->p_sample_count_pts = calloc( ret->i_sample_count, sizeof( uint32_t ) );
    if( !ret->p_sample_count_pts )
        return VLC_ENOMEM;
    ret->i_entries_pts = ret->i_sample_count;

    if( p_trun_data->i_flags & MP4_TRUN_SAMPLE_TIME_OFFSET )
    {
        ret->p_sample_offset_pts = calloc( ret->i_sample_count, sizeof( int32_t ) );
        if( !ret->p_sample_offset_pts )
            return VLC_ENOMEM;
    }

    ret->p_sample_size = calloc( ret->i_sample_count, sizeof( uint32_t ) );
    if( !ret->p_sample_size )
        return VLC_ENOMEM;

    ret->p_sample_data = calloc( ret->i_sample_count, sizeof( uint8_t * ) );
    if( !ret->p_sample_data )
        return VLC_ENOMEM;

    uint32_t dur = 0, i_mdatlen = 0, len;
    uint32_t chunk_duration = 0, chunk_size = 0;

    /* Skip header of mdat */
    uint8_t mdat[8];
    int i_read = stream_Read( p_demux->s, &mdat, 8 );
    i_mdatlen = GetDWBE( mdat );
    if ( i_read < 8 || i_mdatlen < 8 ||
         VLC_FOURCC( mdat[4], mdat[5], mdat[6], mdat[7] ) != ATOM_mdat )
        return VLC_EGENERIC;

    for( uint32_t i = 0; i < ret->i_sample_count; i++)
    {
        if( p_trun_data->i_flags & MP4_TRUN_SAMPLE_DURATION )
            dur = p_trun_data->p_samples[i].i_duration;
        else
            dur = default_duration;
        ret->p_sample_delta_dts[i] = dur;
        chunk_duration += dur;

        ret->p_sample_count_dts[i] = ret->p_sample_count_pts[i] = 1;

        if( ret->p_sample_offset_pts )
        {
            if ( p_trun_data->i_version == 0 )
                ret->p_sample_offset_pts[i] = (int32_t) p_trun_data->p_samples[i].i_composition_time_offset;
            else
                ret->p_sample_offset_pts[i] = __MIN( INT32_MAX, p_trun_data->p_samples[i].i_composition_time_offset );
        }

        if( p_trun_data->i_flags & MP4_TRUN_SAMPLE_SIZE )
            len = ret->p_sample_size[i] = p_trun_data->p_samples[i].i_size;
        else
            len = ret->p_sample_size[i] = default_size;

        if ( chunk_size + len > ( i_mdatlen - 8 ) )
            return VLC_EGENERIC;

        ret->p_sample_data[i] = malloc( len );
        if( ret->p_sample_data[i] == NULL )
            return VLC_ENOMEM;
        uint32_t i_read = stream_ReadU32( p_demux->s, ret->p_sample_data[i], len );
        if( i_read < len )
            return VLC_EGENERIC;
        chunk_size += len;
    }
    ret->i_last_dts = ret->i_first_dts + chunk_duration - dur;
    p_track->i_first_dts = chunk_duration + ret->i_first_dts;

    if( p_track->b_codec_need_restart )
    {
        ReInitDecoder( p_demux, p_demux->p_sys->p_root, p_track );
        p_track->b_codec_need_restart = false;
    }

    /* Skip if we didn't reach the end of mdat box */
    if ( chunk_size < (i_mdatlen - 8) )
        stream_ReadU32( p_demux->s, NULL, i_mdatlen - chunk_size - 8 );

    p_track->b_has_non_empty_cchunk = true;
    return VLC_SUCCESS;
}


/**
 * Get the next chunk of the track identified by i_tk_id.
 * \Note We don't want to seek all the time, so if the first chunk given by the
 * input method doesn't belong to the right track, we don't throw it away,
 * and so, in general, this function fetch more than one chunk.
 * Not to mention that a new init fragment might be put everywhere
 * between two chunks by the input method.
 */
static int MP4_frg_GetChunks( demux_t *p_demux, const unsigned i_tk_id )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    mp4_track_t *p_track;

    for( unsigned i = 0; i < p_sys->i_tracks; i++ )
    {
        MP4_Box_t *p_chunk = MP4_BoxGetNextChunk( p_demux->s );
        if( !p_chunk )
            return VLC_EGENERIC;

        if( !p_chunk->p_first )
            goto MP4_frg_GetChunks_Error;
        uint32_t i_type = p_chunk->p_first->i_type;
        uint32_t tid = 0;
        if( i_type == ATOM_uuid || i_type == ATOM_ftyp )
        {
            MP4_BoxFree( p_demux->s, p_sys->p_root );
            p_sys->p_root = p_chunk;

            if( i_type == ATOM_ftyp ) /* DASH */
            {
                MP4_Box_t *p_tkhd = MP4_BoxGet( p_chunk, "/moov/trak[0]/tkhd" );
                if( !p_tkhd )
                {
                    msg_Warn( p_demux, "No tkhd found!" );
                    goto MP4_frg_GetChunks_Error;
                }
                tid = p_tkhd->data.p_tkhd->i_track_ID;
            }
            else                      /* Smooth Streaming */
            {
                assert( !CmpUUID( &p_chunk->p_first->i_uuid, &SmooBoxUUID ) );
                MP4_Box_t *p_stra = MP4_BoxGet( p_chunk, "/uuid/uuid[0]" );
                if( !p_stra || CmpUUID( &p_stra->i_uuid, &StraBoxUUID ) )
                {
                    msg_Warn( p_demux, "No StraBox found!" );
                    goto MP4_frg_GetChunks_Error;
                }
                tid = p_stra->data.p_stra->i_track_ID;
            }

            p_track = MP4_frg_GetTrackByID( p_demux, tid );
            if( !p_track )
                goto MP4_frg_GetChunks_Error;
            p_track->b_codec_need_restart = true;

            return MP4_frg_GetChunks( p_demux, i_tk_id );
        }

        if( MP4_frg_GetChunk( p_demux, p_chunk, &tid ) != VLC_SUCCESS )
            goto MP4_frg_GetChunks_Error;

        MP4_BoxFree( p_demux->s, p_chunk );

        if( tid == i_tk_id )
            break;
        else
            continue;

MP4_frg_GetChunks_Error:
        MP4_BoxFree( p_demux->s, p_chunk );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int MP4_frg_TrackSelect( demux_t *p_demux, mp4_track_t *p_track )
{
    if( !p_track->b_ok || p_track->b_chapter )
    {
        return VLC_EGENERIC;
    }

    if( p_track->b_selected )
    {
        msg_Warn( p_demux, "track[Id 0x%x] already selected", p_track->i_track_ID );
        return VLC_SUCCESS;
    }

    msg_Dbg( p_demux, "Select track id %u", p_track->i_track_ID );
    p_track->b_selected = true;
    return VLC_SUCCESS;
}

/**
 * DemuxFrg: read packet and send them to decoders
 * \return 1 on success, 0 on error.
 * TODO check for newly selected track
 */
int DemuxFrg( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned i_track;
    unsigned i_track_selected;

    /* check for newly selected/unselected track */
    for( i_track = 0, i_track_selected = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        mp4_track_t *tk = &p_sys->track[i_track];
        bool b;

        if( !tk->b_ok || tk->b_chapter )
            continue;

        es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE, tk->p_es, &b );
        msg_Dbg( p_demux, "track %u %s!", tk->i_track_ID, b ? "enabled" : "disabled" );

        if( tk->b_selected && !b )
        {
            MP4_TrackUnselect( p_demux, tk );
        }
        else if( !tk->b_selected && b)
        {
            MP4_frg_TrackSelect( p_demux, tk );
        }

        if( tk->b_selected )
            i_track_selected++;
    }

    if( i_track_selected <= 0 )
    {
        p_sys->i_time += __MAX( p_sys->i_timescale / 10 , 1 );
        if( p_sys->i_timescale > 0 )
        {
            int64_t i_length = CLOCK_FREQ *
                               (mtime_t)p_sys->i_overall_duration /
                               (mtime_t)p_sys->i_timescale;
            if( MP4_GetMoviePTS( p_sys ) >= i_length )
                return 0;
            return 1;
        }

        msg_Warn( p_demux, "no track selected, exiting..." );
        return 0;
    }

    /* first wait for the good time to read a packet */
    es_out_Control( p_demux->out, ES_OUT_SET_PCR, VLC_TS_0 + p_sys->i_pcr );

    p_sys->i_pcr = MP4_GetMoviePTS( p_sys );

    /* we will read 100ms for each stream so ...*/
    p_sys->i_time += __MAX( p_sys->i_timescale / 10, 1 );

    for( i_track = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        mp4_track_t *tk = &p_sys->track[i_track];

        if( !tk->b_ok || tk->b_chapter || !tk->b_selected )
        {
            msg_Warn( p_demux, "Skipping track id %u", tk->i_track_ID );
            continue;
        }

        if( !tk->b_has_non_empty_cchunk )
        {
            if( MP4_frg_GetChunks( p_demux, tk->i_track_ID ) != VLC_SUCCESS )
            {
                msg_Info( p_demux, "MP4_frg_GetChunks returned error. End of stream?" );
                return 0;
            }
        }

        while( MP4_TrackGetDTS( p_demux, tk ) < MP4_GetMoviePTS( p_sys ) )
        {
            block_t *p_block;
            int64_t i_delta;

            mp4_chunk_t *ck = tk->cchunk;
            if( ck->i_sample >= ck->i_sample_count )
            {
                msg_Err( p_demux, "sample %"PRIu32" of %"PRIu32"",
                                    ck->i_sample, ck->i_sample_count );
                return 0;
            }

            uint32_t sample_size = ck->p_sample_size[ck->i_sample];
            p_block = block_Alloc( sample_size );
            uint8_t *src = ck->p_sample_data[ck->i_sample];
            memcpy( p_block->p_buffer, src, sample_size );

            ck->i_sample++;
            if( ck->i_sample == ck->i_sample_count )
                tk->b_has_non_empty_cchunk = false;

            /* dts */
            p_block->i_dts = VLC_TS_0 + MP4_TrackGetDTS( p_demux, tk );
            /* pts */
            if( MP4_TrackGetPTSDelta( p_demux, tk, &i_delta ) )
                p_block->i_pts = p_block->i_dts + i_delta;
            else if( tk->fmt.i_cat != VIDEO_ES )
                p_block->i_pts = p_block->i_dts;
            else
                p_block->i_pts = VLC_TS_INVALID;

            MP4_Block_Send( p_demux, tk, p_block );

            tk->i_sample++;

            if( !tk->b_has_non_empty_cchunk )
                break;
        }
    }
    return 1;
}

static MP4_Box_t * LoadNextChunk( demux_t *p_demux )
{
    /* Read Next Chunk */
    MP4_Box_t *p_chunk = MP4_BoxGetNextChunk( p_demux->s );
    if( !p_chunk )
    {
        msg_Warn( p_demux, "no next chunk" );
        return NULL;
    }

    if( !p_chunk->p_first )
    {
        msg_Warn( p_demux, "no next chunk child" );
        return NULL;
    }

    return p_chunk;
}

static bool BoxExistsInRootTree( MP4_Box_t *p_root, uint32_t i_type, uint64_t i_pos )
{
    while ( p_root )
    {
        if ( p_root->i_pos == i_pos )
        {
            assert( i_type == p_root->i_type );
            break;
        }
        p_root = p_root->p_next;
    }
    return (p_root != NULL);
}

static mtime_t SumFragmentsDurations( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    mtime_t i_max_duration = 0;

    for ( unsigned int i=0; i<p_sys->i_tracks; i++ )
    {
        mtime_t i_duration = 0;
        const mp4_fragment_t *p_fragment = &p_sys->moovfragment;
        while ( p_fragment && p_fragment->p_durations )
        {
            i_duration += GetTrackDurationInFragment( p_fragment,
                                                      p_sys->track[i].i_track_ID );
            p_fragment = p_fragment->p_next;
        }
        i_max_duration = __MAX( i_duration, i_max_duration );
    }

    return i_max_duration;
}

/* Keeps an ordered chain of all fragments */
static bool AddFragment( demux_t *p_demux, MP4_Box_t *p_moox )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    mp4_fragment_t *p_base_fragment = & p_sys->moovfragment;
    if ( !p_moox )
        return false;

    if( p_moox->i_type == ATOM_moov )
    {
        if ( !p_sys->moovfragment.p_moox )
        {
            p_sys->moovfragment.p_moox = p_moox;
            MP4_Box_t *p_mvhd;
            if( (p_mvhd = MP4_BoxGet( p_moox, "mvhd" )) )
            {
                p_sys->i_timescale = BOXDATA(p_mvhd)->i_timescale;
                p_sys->i_overall_duration = BOXDATA(p_mvhd)->i_duration;
            }

            if ( MP4_BoxCount( p_moox, "mvex" ) || !p_mvhd )
            { /* duration might be wrong an be set to whole duration :/ */
               MP4_Box_t *p_tkhd;
               MP4_Box_t *p_trak = MP4_BoxGet( p_moox, "trak" );
               unsigned int i_trakcount = MP4_BoxCount( p_moox, "trak" );
               p_sys->moovfragment.p_durations = calloc( i_trakcount, sizeof(*p_sys->moovfragment.p_durations) );
               if ( i_trakcount && !p_sys->moovfragment.p_durations )
                   return 0;
               p_sys->moovfragment.i_durations = i_trakcount;
               i_trakcount = 0;
               while( p_trak )
               {
                   if ( p_trak->i_type == ATOM_trak && (p_tkhd = MP4_BoxGet( p_trak, "tkhd" )) )
                   {
                       p_sys->moovfragment.p_durations[i_trakcount].i_duration = BOXDATA(p_tkhd)->i_duration;
                       p_sys->moovfragment.p_durations[i_trakcount++].i_track_ID = BOXDATA(p_tkhd)->i_track_ID;
                   }
                   p_trak = p_trak->p_next;
               }
            }

            msg_Dbg( p_demux, "added fragment %4.4s", (char*) &p_moox->i_type );
            return true;
        }
        return false;
    }
    else // p_moox->i_type == ATOM_moof
    {
        assert(p_moox->i_type == ATOM_moof);
        mp4_fragment_t *p_fragment = p_sys->moovfragment.p_next;
        while ( p_fragment )
        {
            if ( !p_base_fragment->p_moox || p_moox->i_pos > p_base_fragment->p_moox->i_pos )
            {
                p_base_fragment = p_fragment;
                p_fragment = p_fragment->p_next;
            }
            else if ( p_moox->i_pos == p_base_fragment->p_moox->i_pos )
            {
                /* already exists */
                return false;
            }
        }
    }

    /* Add the moof fragment */
    mp4_fragment_t *p_new = malloc(sizeof(mp4_fragment_t));
    if ( !p_new ) return false;
    p_new->p_moox = p_moox;
    p_new->i_durations = MP4_BoxCount( p_new->p_moox, "traf" );
    p_new->p_durations = calloc( p_new->i_durations, sizeof(*p_new->p_durations) );
    if ( p_new->i_durations && !p_new->p_durations )
    {
        free( p_new );
        return false;
    }
    p_new->p_next = p_base_fragment->p_next;
    p_base_fragment->p_next = p_new;
    msg_Dbg( p_demux, "added fragment %4.4s", (char*) &p_moox->i_type );

    /* we have to probe all fragments :/ */
    uint64_t i_traf_base_data_offset = 0;
    uint64_t i_traf_min_offset = 0;
    uint32_t i_traf = 0;
    uint32_t i_traf_total_size = 0;
    uint32_t i_trafs_total_size = 0;

    MP4_Box_t *p_traf = MP4_BoxGet( p_new->p_moox, "traf" );
    unsigned int i_durationindex = 0;
    while ( p_traf )
    {
        if ( p_traf->i_type != ATOM_traf )
        {
           p_traf = p_traf->p_next;
           continue;
        }
        const MP4_Box_t *p_tfhd = MP4_BoxGet( p_traf, "tfhd" );
        const MP4_Box_t *p_trun = MP4_BoxGet( p_traf, "trun" );
        if ( !p_tfhd || !p_trun )
        {
           p_traf = p_traf->p_next;
           continue;
        }

        uint32_t i_track_timescale = 0;
        uint32_t i_track_defaultsamplesize = 0;
        uint32_t i_track_defaultsampleduration = 0;
        if ( p_sys->b_smooth )
        {
            /* stra sets identical timescales */
            i_track_timescale = p_sys->i_timescale;
            i_track_defaultsamplesize = 1;
            i_track_defaultsampleduration = 1;
        }
        else
        {
            /* set trex for defaults */
             MP4_Box_t *p_trex = MP4_GetTrexByTrackID( p_sys->moovfragment.p_moox, BOXDATA(p_tfhd)->i_track_ID );
             if ( p_trex )
             {
                i_track_defaultsamplesize = BOXDATA(p_trex)->i_default_sample_size;
                i_track_defaultsampleduration = BOXDATA(p_trex)->i_default_sample_duration;
             }

             MP4_Box_t *p_trak = MP4_GetTrakByTrackID( p_sys->moovfragment.p_moox, BOXDATA(p_tfhd)->i_track_ID );
             if ( p_trak )
             {
                MP4_Box_t *p_mdhd = MP4_BoxGet( p_trak, "mdia/mdhd" );
                if ( p_mdhd ) i_track_timescale = BOXDATA(p_mdhd)->i_timescale;
             }
        }

        if ( !i_track_timescale )
        {
           p_traf = p_traf->p_next;
           continue;
        }

        if ( BOXDATA(p_tfhd)->i_flags & MP4_TFHD_BASE_DATA_OFFSET )
        {
            i_traf_base_data_offset = BOXDATA(p_tfhd)->i_base_data_offset;
        }
        else if ( BOXDATA(p_tfhd)->i_flags & MP4_TFHD_DEFAULT_BASE_IS_MOOF )
        {
            i_traf_base_data_offset = p_new->p_moox->i_pos /* + 8*/;
        }
        else
        {
            if ( i_traf == 0 )
                i_traf_base_data_offset = p_new->p_moox->i_pos /*+ 8*/;
            else
                i_traf_base_data_offset += i_traf_total_size;
        }

        i_traf_total_size = 0;

        uint64_t i_trun_data_offset = i_traf_base_data_offset;
        uint64_t i_traf_duration = 0;
        uint32_t i_trun_size = 0;
        while ( p_trun && p_tfhd )
        {
            if ( p_trun->i_type != ATOM_trun )
            {
               p_trun = p_trun->p_next;
               continue;
            }
            const MP4_Box_data_trun_t *p_trundata = p_trun->data.p_trun;

            /* Get data offset */
            if ( p_trundata->i_flags & MP4_TRUN_DATA_OFFSET )
                i_trun_data_offset += __MAX( p_trundata->i_data_offset, 0 );
            else
                i_trun_data_offset += i_trun_size;

            i_trun_size = 0;

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

            /* Get total traf size */
            if ( p_trundata->i_flags & MP4_TRUN_SAMPLE_SIZE )
            {
                for( uint32_t i=0; i< p_trundata->i_sample_count; i++ )
                    i_trun_size += p_trundata->p_samples[i].i_size;
            }
            else if ( BOXDATA(p_tfhd)->i_flags & MP4_TFHD_DFLT_SAMPLE_SIZE )
            {
                i_trun_size += p_trundata->i_sample_count *
                        BOXDATA(p_tfhd)->i_default_sample_size;
            }
            else
            {
                i_trun_size += p_trundata->i_sample_count *
                        i_track_defaultsamplesize;
            }

            i_traf_total_size += i_trun_size;

            if ( i_traf_min_offset )
                i_traf_min_offset = __MIN( i_trun_data_offset, i_traf_min_offset );
            else
                i_traf_min_offset = i_trun_data_offset;

            p_trun = p_trun->p_next;
        }

        i_trafs_total_size += i_traf_total_size;

        p_new->p_durations[i_durationindex].i_track_ID = BOXDATA(p_tfhd)->i_track_ID;
        p_new->p_durations[i_durationindex++].i_duration = i_traf_duration * p_sys->i_timescale / i_track_timescale;

        p_traf = p_traf->p_next;
        i_traf++;
    }

    p_new->i_chunk_range_min_offset = i_traf_min_offset;
    p_new->i_chunk_range_max_offset = i_traf_min_offset + i_trafs_total_size;

    msg_Dbg( p_demux, "new fragment is %"PRId64" %"PRId64, p_new->i_chunk_range_min_offset, p_new->i_chunk_range_max_offset );

    /* compute total duration with that new fragment if no overall provided */
    MP4_Box_t *p_mehd = MP4_BoxGet( p_sys->moovfragment.p_moox, "mvex/mehd");
    if ( !p_mehd )
    {
        if ( p_sys->b_fragments_probed )
           p_sys->i_overall_duration = SumFragmentsDurations( p_demux );
    }
    else
        p_sys->i_overall_duration = BOXDATA(p_mehd)->i_fragment_duration;

    msg_Dbg( p_demux, "total fragments duration %"PRId64, CLOCK_FREQ * p_sys->i_overall_duration / p_sys->i_timescale );
    return true;
}

static int ProbeIndex( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    uint64_t i_backup_pos, i_stream_size;
    uint8_t mfro[MP4_MFRO_BOXSIZE];
    assert( p_sys->b_seekable );

    if ( MP4_BoxCount( p_sys->p_root, "/mfra" ) )
        return VLC_SUCCESS;

    i_stream_size = stream_Size( p_demux->s );
    if ( ( i_stream_size >> 62 ) ||
         ( i_stream_size < MP4_MFRO_BOXSIZE ) ||
         ( !MP4_stream_Tell( p_demux->s, &i_backup_pos ) ) ||
         ( stream_Seek( p_demux->s, i_stream_size - MP4_MFRO_BOXSIZE ) != VLC_SUCCESS )
       )
    {
        msg_Dbg( p_demux, "Probing tail for mfro has failed" );
        return VLC_EGENERIC;
    }

    if ( stream_Read( p_demux->s, &mfro, MP4_MFRO_BOXSIZE ) == MP4_MFRO_BOXSIZE &&
         VLC_FOURCC(mfro[4],mfro[5],mfro[6],mfro[7]) == ATOM_mfro &&
         GetDWBE( &mfro ) == MP4_MFRO_BOXSIZE )
    {
        uint32_t i_offset = GetDWBE( &mfro[12] );
        msg_Dbg( p_demux, "will read mfra index at %"PRIu64, i_stream_size - i_offset );
        if ( i_stream_size > i_offset &&
             stream_Seek( p_demux->s, i_stream_size - i_offset ) == VLC_SUCCESS )
        {
            msg_Dbg( p_demux, "reading mfra index at %"PRIu64, i_stream_size - i_offset );
            MP4_ReadBoxContainerChildren( p_demux->s, p_sys->p_root, ATOM_mfra );
        }
    }

    return stream_Seek( p_demux->s, i_backup_pos );
}

static int ProbeFragments( demux_t *p_demux, bool b_force )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    uint64_t i_current_pos;

    if ( MP4_stream_Tell( p_demux->s, &i_current_pos ) )
        msg_Dbg( p_demux, "probing fragments from %"PRId64, i_current_pos );

    assert( p_sys->p_root );

    if ( p_sys->b_fastseekable || b_force )
    {
        MP4_ReadBoxContainerChildren( p_demux->s, p_sys->p_root, 0 ); /* Get the rest of the file */
        p_sys->b_fragments_probed = true;
    }
    else
    {
        /* We stop at first moof, which validates our fragmentation condition
         * and we'll find others while reading. */
        MP4_ReadBoxContainerChildren( p_demux->s, p_sys->p_root, ATOM_moof );
    }

    if ( !p_sys->moovfragment.p_moox )
    {
        MP4_Box_t *p_moov = MP4_BoxGet( p_sys->p_root, "/moov" );
        if ( !p_moov )
        {
            /* moov/mvex before probing should be present anyway */
            MP4_BoxDumpStructure( p_demux->s, p_sys->p_root );
            return VLC_EGENERIC;
        }
        AddFragment( p_demux, p_moov );
    }

    MP4_Box_t *p_moof = MP4_BoxGet( p_sys->p_root, "moof" );
    while ( p_moof )
    {
        if ( p_moof->i_type == ATOM_moof )
            AddFragment( p_demux, p_moof );
        p_moof = p_moof->p_next;
    }

    MP4_Box_t *p_mdat = MP4_BoxGet( p_sys->p_root, "mdat" );
    if ( p_mdat )
    {
        stream_Seek( p_demux->s, p_mdat->i_pos );
        msg_Dbg( p_demux, "rewinding to mdat %"PRId64, p_mdat->i_pos );
    }

    return VLC_SUCCESS;
}

static mp4_fragment_t * GetFragmentByPos( demux_t *p_demux, uint64_t i_pos, bool b_exact )
{
    mp4_fragment_t *p_fragment = &p_demux->p_sys->moovfragment;
    while ( p_fragment )
    {
        if ( i_pos <= p_fragment->i_chunk_range_max_offset &&
             ( !b_exact || i_pos >= p_fragment->i_chunk_range_min_offset ) )
        {
            msg_Dbg( p_demux, "fragment matched %"PRIu64" << %"PRIu64" << %"PRIu64,
                     p_fragment->i_chunk_range_min_offset, i_pos,
                     p_fragment->i_chunk_range_max_offset );
            return p_fragment;
        }
        else
        {
            p_fragment = p_fragment->p_next;
        }
    }
    return NULL;
}

/* Get a matching fragment data start by clock time */
static mp4_fragment_t * GetFragmentByTime( demux_t *p_demux, const mtime_t i_time )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    mp4_fragment_t *p_fragment = &p_sys->moovfragment;
    mtime_t i_base_time = 0;
    mtime_t *pi_tracks_duration_total = calloc( p_sys->i_tracks, sizeof(mtime_t) );
    while ( p_fragment && pi_tracks_duration_total )
    {
        if ( p_fragment == &p_sys->moovfragment &&
             p_fragment->i_chunk_range_max_offset == 0 )
        {
            p_fragment = p_fragment->p_next;
            continue;
        }

        mtime_t i_length = 0;
        for( unsigned int i=0; i<p_sys->i_tracks; i++ )
        {
            mtime_t i_track_duration = GetTrackDurationInFragment( p_fragment,
                                                                   p_sys->track[i].i_track_ID );
            pi_tracks_duration_total[i] += i_track_duration;
            i_length = __MAX(i_length, i_track_duration);
        }

        i_length = i_length * CLOCK_FREQ / p_demux->p_sys->i_timescale; /* movie scale to time */
        if ( i_time >= i_base_time &&
             i_time <= i_base_time + i_length )
        {
            free( pi_tracks_duration_total );
            return p_fragment;
        }
        else
        {
            i_base_time += i_length;
            p_fragment = p_fragment->p_next;
        }

        if ( !p_demux->p_sys->b_fragments_probed )
            break; /* We have no way to guess missing fragments time */
    }
    free( pi_tracks_duration_total );
    return NULL;
}

/* Returns fragment scaled time offset */
static mtime_t LeafGetTrackFragmentTimeOffset( demux_t *p_demux, mp4_fragment_t *p_fragment,
                                               unsigned int i_track_ID )
{
    mtime_t i_base_scaledtime = 0;
    mp4_fragment_t *p_current = &p_demux->p_sys->moovfragment;
    while ( p_current != p_fragment )
    {
        if ( p_current != &p_demux->p_sys->moovfragment ||
             p_current->i_chunk_range_max_offset )
        {
            i_base_scaledtime += GetTrackDurationInFragment( p_current, i_track_ID );
        }
        p_current = p_current->p_next;
    }
    return i_base_scaledtime;
}

static int LeafParseTRUN( demux_t *p_demux, mp4_track_t *p_track,
                      const uint32_t i_defaultduration, const uint32_t i_defaultsize,
                      const MP4_Box_data_trun_t *p_trun, uint32_t * const pi_mdatlen )
{
    assert( p_trun->i_sample_count );
    msg_Dbg( p_demux, "Default sample duration %"PRIu32" size %"PRIu32" firstdts %"PRIu64,
             i_defaultduration, i_defaultsize, p_track->i_first_dts );

    uint32_t dur = 0, len;
    uint32_t chunk_size = 0;
    mtime_t i_nzdts = CLOCK_FREQ * p_track->i_time / p_track->i_timescale;

    mtime_t i_nzpts;

    for( uint32_t i = 0; i < p_trun->i_sample_count; i++)
    {
        i_nzdts += CLOCK_FREQ * dur / p_track->i_timescale;
        i_nzpts = i_nzdts;

        if( p_trun->i_flags & MP4_TRUN_SAMPLE_DURATION )
            dur = p_trun->p_samples[i].i_duration;
        else
            dur = i_defaultduration;

        p_track->i_time += dur;

        if( p_trun->i_flags & MP4_TRUN_SAMPLE_TIME_OFFSET )
        {
            if ( p_trun->i_version == 1 )
                i_nzpts += CLOCK_FREQ * (int32_t) p_trun->p_samples[i].i_composition_time_offset / p_track->i_timescale;
            else
                i_nzpts += CLOCK_FREQ * p_trun->p_samples[i].i_composition_time_offset / p_track->i_timescale;
        }

        if( p_trun->i_flags & MP4_TRUN_SAMPLE_SIZE )
            len = p_trun->p_samples[i].i_size;
        else
            len = i_defaultsize;

        if(!dur)
            msg_Warn(p_demux, "Zero duration sample in trun.");

        if(!len)
            msg_Warn(p_demux, "Zero length sample in trun.");

        if ( chunk_size + len > *pi_mdatlen )
        {
            /* update data left in mdat */
            *pi_mdatlen -= chunk_size;
            return VLC_EGENERIC;
        }

        block_t *p_block = MP4_Block_Read( p_demux, p_track, __MIN( len, INT32_MAX ) );
        uint32_t i_read = ( p_block ) ? p_block->i_buffer : 0;
        if( i_read < len )
        {
            /* update data left in mdat */
            *pi_mdatlen -= chunk_size;
            *pi_mdatlen -= i_read;
            free( p_block );
            return VLC_EGENERIC;
        }

        if ( p_demux->p_sys->i_pcr < VLC_TS_0 )
        {
            p_demux->p_sys->i_pcr = i_nzdts;
            es_out_Control( p_demux->out, ES_OUT_SET_PCR, VLC_TS_0 + i_nzdts );
        }

        if ( p_block )
        {
            if ( p_track->p_es )
            {
                p_block->i_dts = VLC_TS_0 + i_nzdts;
                p_block->i_pts = VLC_TS_0 + i_nzpts;
                p_block->i_length = CLOCK_FREQ * dur / p_track->i_timescale;
                MP4_Block_Send( p_demux, p_track, p_block );
            }
            else block_Release( p_block );
        }

        chunk_size += len;
    }

    /* update data left in mdat */
    *pi_mdatlen -= chunk_size;
    return VLC_SUCCESS;
}

static int LeafGetTrackAndChunkByMOOVPos( demux_t *p_demux, uint64_t *pi_pos,
                                      mp4_track_t **pp_tk, unsigned int *pi_chunk )
{
    const demux_sys_t *p_sys = p_demux->p_sys;

    mp4_track_t *p_tk_closest = NULL;
    uint64_t i_closest = UINT64_MAX;
    unsigned int i_chunk_closest;

    *pp_tk = NULL;

    for ( unsigned int i_track = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        for( unsigned int i_chunk = 0; i_chunk < p_sys->track[i_track].i_chunk_count; i_chunk++ )
        {
            if ( p_sys->track[i_track].chunk[i_chunk].i_offset > *pi_pos )
            {
                i_closest = __MIN( i_closest, p_sys->track[i_track].chunk[i_chunk].i_offset );
                p_tk_closest = &p_sys->track[i_track];
                i_chunk_closest = i_chunk;
            }

            if ( *pi_pos == p_sys->track[i_track].chunk[i_chunk].i_offset )
            {
                *pp_tk = &p_sys->track[i_track];
                *pi_chunk = i_chunk;
                return VLC_SUCCESS;
            }
        }
    }

    if ( i_closest != UINT64_MAX )
    {
        *pi_pos = i_closest;
        *pp_tk = p_tk_closest;
        *pi_chunk = i_chunk_closest;
        return VLC_ENOOBJ;
    }
    else return VLC_EGENERIC;
}

static int LeafMOOVGetSamplesSize( const mp4_track_t *p_track, const uint32_t i_sample,
                           uint32_t *pi_samplestoread, uint32_t *pi_samplessize,
                           const uint32_t i_maxbytes, const uint32_t i_maxsamples )
{
    MP4_Box_t *p_stsz = MP4_BoxGet( p_track->p_stbl, "stsz" );
    if ( !p_stsz )
        return VLC_EGENERIC;

    if ( BOXDATA(p_stsz)->i_sample_size == 0 )
    {
        uint32_t i_entry = i_sample;
        uint32_t i_totalbytes = 0;
        *pi_samplestoread = 1;

        if ( i_sample >= BOXDATA(p_stsz)->i_sample_count )
            return VLC_EGENERIC;

        *pi_samplessize = BOXDATA(p_stsz)->i_entry_size[i_sample];
        i_totalbytes += *pi_samplessize;

        if ( *pi_samplessize > i_maxbytes )
            return VLC_EGENERIC;

        i_entry++;
        while( i_entry < BOXDATA(p_stsz)->i_sample_count &&
               *pi_samplessize == BOXDATA(p_stsz)->i_entry_size[i_entry] &&
               i_totalbytes + *pi_samplessize < i_maxbytes &&
               *pi_samplestoread < i_maxsamples
              )
        {
            i_totalbytes += *pi_samplessize;
            (*pi_samplestoread)++;
        }

        *pi_samplessize = i_totalbytes;
    }
    else
    {
        /* all samples have same size */
        *pi_samplessize = BOXDATA(p_stsz)->i_sample_size;
        *pi_samplestoread = __MIN( i_maxsamples, BOXDATA(p_stsz)->i_sample_count );
        *pi_samplestoread = __MIN( i_maxbytes / *pi_samplessize, *pi_samplestoread );
        *pi_samplessize = *pi_samplessize * *pi_samplestoread;
    }

    return VLC_SUCCESS;
}

static inline mtime_t LeafGetMOOVTimeInChunk( const mp4_chunk_t *p_chunk, uint32_t i_sample )
{
    mtime_t i_time = 0;
    uint32_t i_index = 0;

    while( i_sample > 0 )
    {
        if( i_sample > p_chunk->p_sample_count_dts[i_index] )
        {
            i_time += p_chunk->p_sample_count_dts[i_index] *
                p_chunk->p_sample_delta_dts[i_index];
            i_sample -= p_chunk->p_sample_count_dts[i_index];
            i_index++;
        }
        else
        {
            i_time += i_sample * p_chunk->p_sample_delta_dts[i_index];
            break;
        }
    }

    return i_time;
}

static int LeafParseMDATwithMOOV( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    assert( p_sys->context.i_current_box_type == ATOM_mdat );
    assert( p_sys->context.p_fragment->p_moox->i_type == ATOM_moov );

    uint64_t i_current_pos;
    if ( !MP4_stream_Tell( p_demux->s, &i_current_pos ) )
        return VLC_EGENERIC;

    if ( p_sys->context.i_mdatbytesleft == 0 ) /* Start parsing new mdat */
    {
        /* Ready mdat section */
        uint8_t mdat[8];
        int i_read = stream_Read( p_demux->s, &mdat, 8 );
        p_sys->context.i_mdatbytesleft = GetDWBE( mdat );
        if ( i_read < 8 || p_sys->context.i_mdatbytesleft < 8 ||
             VLC_FOURCC( mdat[4], mdat[5], mdat[6], mdat[7] ) != ATOM_mdat )
        {
            uint64_t i_pos;
            if ( !MP4_stream_Tell( p_demux->s, &i_pos ) )
                msg_Err( p_demux, "No mdat atom at %"PRIu64, i_pos - __MAX( 0, i_read ) );
            return VLC_EGENERIC;
        }
        i_current_pos += 8;
        p_sys->context.i_mdatbytesleft -= 8;
    }

    while( p_sys->context.i_mdatbytesleft > 0 )
    {
        mp4_track_t *p_track;
        unsigned int i_chunk;

        /**/
        uint64_t i_pos = i_current_pos;
        int i_ret = LeafGetTrackAndChunkByMOOVPos( p_demux, &i_pos, &p_track, &i_chunk );
        if ( i_ret == VLC_EGENERIC )
        {
            msg_Err(p_demux, "can't find referenced chunk for start at %"PRIu64, i_current_pos );
            goto error;
        }
        else if( i_ret == VLC_ENOOBJ )
        {
            assert( i_pos - i_current_pos > p_sys->context.i_mdatbytesleft );
            int i_read = stream_Read( p_demux->s, NULL, i_pos - i_current_pos );
            i_current_pos += i_read;
            p_sys->context.i_mdatbytesleft -= i_read;
            if ( i_read == 0 ) goto error;
            continue;
        }
        /**/

        mp4_chunk_t *p_chunk = &p_track->chunk[i_chunk];

        uint32_t i_nb_samples_at_chunk_start = p_chunk->i_sample_first;
        uint32_t i_nb_samples_in_chunk = p_chunk->i_sample_count;

        assert(i_nb_samples_in_chunk);

        uint32_t i_nb_samples = 0;
        while ( i_nb_samples < i_nb_samples_in_chunk )
        {
            uint32_t i_samplessize = 0;
            uint32_t i_samplescounttoread = 0;
            i_ret = LeafMOOVGetSamplesSize( p_track,
                            i_nb_samples_at_chunk_start + i_nb_samples,
                            &i_samplescounttoread, &i_samplessize,
                            p_sys->context.i_mdatbytesleft,
                            /*i_nb_samples_in_chunk - i_nb_samples*/ 1 );
            if ( i_ret != VLC_SUCCESS )
                goto error;

            if( p_sys->context.i_mdatbytesleft &&
                p_sys->context.i_mdatbytesleft  >= i_samplessize )
            {
                block_t *p_block;

                /* now read pes */

                if( !(p_block = MP4_Block_Read( p_demux, p_track, i_samplessize )) )
                {
                    uint64_t i_pos;
                    if ( MP4_stream_Tell( p_demux->s, &i_pos ) )
                    {
                        p_sys->context.i_mdatbytesleft -= ( i_pos - i_current_pos );
                        msg_Err( p_demux, "stream block error %"PRId64" %"PRId64, i_pos, i_pos - i_current_pos );
                    }
                    goto error;
                }

                i_nb_samples += i_samplescounttoread;
                i_current_pos += i_samplessize;
                p_sys->context.i_mdatbytesleft -= i_samplessize;

                /* dts */
                mtime_t i_time = LeafGetMOOVTimeInChunk( p_chunk, i_nb_samples );
                i_time += p_chunk->i_first_dts;
                p_track->i_time = i_time;
                p_block->i_dts = VLC_TS_0 + CLOCK_FREQ * i_time / p_track->i_timescale;

                /* pts */
                int64_t i_delta;
                if( MP4_TrackGetPTSDelta( p_demux, p_track, &i_delta ) )
                    p_block->i_pts = p_block->i_dts + i_delta;
                else if( p_track->fmt.i_cat != VIDEO_ES )
                    p_block->i_pts = p_block->i_dts;
                else
                    p_block->i_pts = VLC_TS_INVALID;

                MP4_Block_Send( p_demux, p_track, p_block );

                if ( p_demux->p_sys->i_pcr < VLC_TS_0 )
                {
                    p_sys->i_time = p_track->i_time * p_sys->i_timescale / p_track->i_timescale;
                    p_demux->p_sys->i_pcr = MP4_GetMoviePTS( p_demux->p_sys );
                    es_out_Control( p_demux->out, ES_OUT_SET_PCR, VLC_TS_0 + p_demux->p_sys->i_pcr );
                }

            }
            else
            {
                // sample size > left bytes
                break;
            }

            p_track->i_sample += i_samplescounttoread;
        }

        /* flag end of mdat section if needed */
        if ( p_sys->context.i_mdatbytesleft == 0 )
            p_sys->context.i_current_box_type = 0;

        // next chunk
        return VLC_SUCCESS;
    }

error:
    /* Skip if we didn't reach the end of mdat box */
    if ( p_sys->context.i_mdatbytesleft  > 0 )
    {
        msg_Err( p_demux, "mdat had still %"PRIu32" bytes unparsed as samples",
                 p_sys->context.i_mdatbytesleft );
        stream_ReadU32( p_demux->s, NULL, p_sys->context.i_mdatbytesleft );
    }
    p_sys->context.i_current_box_type = 0;

    return VLC_SUCCESS;
}

static mp4_track_t * LeafGetTrackByTrunPos( demux_t *p_demux, const uint64_t i_pos, const uint64_t i_moofpos )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    for ( uint32_t i=0; i<p_sys->i_tracks; i++ )
    {
        mp4_track_t *p_track = &p_sys->track[i];
        if ( !p_track->context.p_trun || !p_track->context.p_tfhd )
            continue;
        const MP4_Box_data_trun_t *p_trun_data = p_track->context.BOXDATA(p_trun);
        uint64_t i_offset = 0;

        if ( p_track->context.BOXDATA(p_tfhd)->i_flags & MP4_TFHD_BASE_DATA_OFFSET )
            i_offset = p_track->context.BOXDATA(p_tfhd)->i_base_data_offset;
        else
            i_offset = i_moofpos;

        //if ( p_track->context.BOXDATA(p_tfhd)->i_flags & MP4_TFHD_DEFAULT_BASE_IS_MOOF )
          //  i_offset += i_moofpos;

        if (p_trun_data->i_flags & MP4_TRUN_DATA_OFFSET)
            i_offset += p_trun_data->i_data_offset;
        else
            return p_track;

        if ( i_offset == i_pos )
            return p_track;
    }

    return NULL;
}

static int LeafMapTrafTrunContextes( demux_t *p_demux, MP4_Box_t *p_moof )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    /* reset */
    for ( uint32_t i=0; i<p_sys->i_tracks; i++ )
    {
        mp4_track_t *p_track = &p_sys->track[i];
        p_track->context.p_tfhd = NULL;
        p_track->context.p_traf = NULL;
        p_track->context.p_trun = NULL;
    }

    if ( p_moof->i_type == ATOM_moov )
        return VLC_SUCCESS;

    MP4_Box_t *p_traf = MP4_BoxGet( p_moof, "traf" );
    if( !p_traf )
    {
        msg_Warn( p_demux, "no traf box found!" );
        return VLC_EGENERIC;
    }

    /* map contexts */
    while ( p_traf )
    {
        if ( p_traf->i_type == ATOM_traf )
        {
            MP4_Box_t *p_tfhd = MP4_BoxGet( p_traf, "tfhd" );
            for ( uint32_t i=0; p_tfhd && i<p_sys->i_tracks; i++ )
            {
                mp4_track_t *p_track = &p_sys->track[i];
                if ( BOXDATA(p_tfhd)->i_track_ID == p_track->i_track_ID )
                {
                    MP4_Box_t *p_trun = MP4_BoxGet( p_traf, "trun" );
                    if ( p_trun )
                    {
                        p_track->context.p_tfhd = p_tfhd;
                        p_track->context.p_traf = p_traf;
                        p_track->context.p_trun = p_trun;
                    }
                    p_tfhd = NULL; /* break loop */
                }
            }
        }
        p_traf = p_traf->p_next;
    }

    return VLC_SUCCESS;
}

static int LeafIndexGetMoofPosByTime( demux_t *p_demux, const mtime_t i_target_time,
                                      uint64_t *pi_pos, mtime_t *pi_mooftime )
{
    MP4_Box_t *p_tfra = MP4_BoxGet( p_demux->p_sys->p_root, "mfra/tfra" );
    while ( p_tfra )
    {
        if ( p_tfra->i_type == ATOM_tfra )
        {
            int64_t i_pos = -1;
            const MP4_Box_data_tfra_t *p_data = BOXDATA(p_tfra);
            mp4_track_t *p_track = MP4_frg_GetTrackByID( p_demux, p_data->i_track_ID );
            if ( p_track && (p_track->fmt.i_cat == AUDIO_ES || p_track->fmt.i_cat == VIDEO_ES) )
            {
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

                    if ( CLOCK_FREQ * i_time / p_track->i_timescale >= i_target_time )
                    {
                        if ( i_pos == -1 ) /* Not in this traf */
                            break;

                        *pi_pos = (uint64_t) i_pos;
                        *pi_mooftime = CLOCK_FREQ * i_time / p_track->i_timescale;
                        if ( p_track->fmt.i_cat == AUDIO_ES )
                            *pi_mooftime -= CLOCK_FREQ / p_track->fmt.audio.i_rate * p_data->p_sample_number[i];
                        else
                            *pi_mooftime -= CLOCK_FREQ / p_demux->p_sys->f_fps * p_data->p_sample_number[i];
                        return VLC_SUCCESS;
                    }
                    else
                        i_pos = i_offset;
                }
            }
        }
        p_tfra = p_tfra->p_next;
    }
    return VLC_EGENERIC;
}

static void MP4_GetDefaultSizeAndDuration( demux_t *p_demux,
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
        const MP4_Box_t *p_trex = MP4_GetTrexByTrackID(
                    MP4_BoxGet( p_demux->p_sys->p_root, "moov" ),
                    p_tfhd_data->i_track_ID );
        if ( p_trex )
        {
            if ( !*pi_default_duration )
                *pi_default_duration = BOXDATA(p_trex)->i_default_sample_duration;
            if ( !*pi_default_size )
                *pi_default_size = BOXDATA(p_trex)->i_default_sample_size;
        }
    }
}

static int LeafParseMDATwithMOOF( demux_t *p_demux, MP4_Box_t *p_moof )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    uint64_t i_pos;
    assert( p_moof->i_type == ATOM_moof );
    assert( p_sys->context.i_current_box_type == ATOM_mdat );

    if ( p_sys->context.i_mdatbytesleft == 0 )
    {
        int i_ret = LeafMapTrafTrunContextes( p_demux, p_moof );
        if ( i_ret != VLC_SUCCESS )
            return i_ret;

        /* Ready mdat section */
        uint8_t mdat[8];
        int i_read = stream_Read( p_demux->s, &mdat, 8 );
        p_sys->context.i_mdatbytesleft = GetDWBE( mdat );
        if ( i_read < 8 || p_sys->context.i_mdatbytesleft < 8 ||
             VLC_FOURCC( mdat[4], mdat[5], mdat[6], mdat[7] ) != ATOM_mdat )
        {
            if ( MP4_stream_Tell( p_demux->s, &i_pos ) )
                msg_Err(p_demux, "No mdat atom at %"PRIu64, i_pos - i_read );
            return VLC_EGENERIC;
        }
        p_sys->context.i_mdatbytesleft -= 8;
    }

    if ( !MP4_stream_Tell( p_demux->s, &i_pos ) )
        return VLC_EGENERIC;
    if ( p_sys->b_smooth )
        i_pos -= p_moof->i_pos;
    mp4_track_t *p_track = LeafGetTrackByTrunPos( p_demux, i_pos, p_moof->i_pos );
    if( p_track )
    {
        uint32_t i_trun_sample_default_duration = 0;
        uint32_t i_trun_sample_default_size = 0;

        if ( p_track->context.p_trun )
        {
            /* Get defaults for this/these RUN */
            const MP4_Box_data_tfhd_t *p_tfhd_data = p_track->context.BOXDATA(p_tfhd);
            MP4_GetDefaultSizeAndDuration( p_demux, p_tfhd_data,
                                           &i_trun_sample_default_size,
                                           &i_trun_sample_default_duration );

            const MP4_Box_data_trun_t *p_trun_data = p_track->context.BOXDATA(p_trun);

           /* NOW PARSE TRUN WITH MDAT */
            int i_ret = LeafParseTRUN( p_demux, p_track,
                                   i_trun_sample_default_duration, i_trun_sample_default_size,
                                   p_trun_data, & p_sys->context.i_mdatbytesleft );
            if ( i_ret != VLC_SUCCESS )
                goto end;

            p_track->context.p_trun = p_track->context.p_trun->p_next;
        }

        if ( p_sys->context.i_mdatbytesleft == 0 )
            p_sys->context.i_current_box_type = 0;
        return VLC_SUCCESS;
    }

end:
    /* Skip if we didn't reach the end of mdat box */
    if ( p_sys->context.i_mdatbytesleft > 0 )
    {
        msg_Warn( p_demux, "mdat had still %"PRIu32" bytes unparsed as samples", p_sys->context.i_mdatbytesleft - 8 );
        stream_ReadU32( p_demux->s, NULL, p_sys->context.i_mdatbytesleft );
    }

    p_sys->context.i_current_box_type = 0;

    return VLC_SUCCESS;
}

static void RestartAllTracks( demux_t *p_demux, const MP4_Box_t *p_root )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    for( unsigned i_track = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        mp4_track_t *tk = &p_sys->track[i_track];
        if( !tk->b_ok || tk->b_chapter )
            continue;

        ReInitDecoder( p_demux, p_root, tk );
    }
}

static int DemuxAsLeaf( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned i_track_selected = 0;

    /* check for newly selected/unselected track */
    for( unsigned i_track = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        mp4_track_t *tk = &p_sys->track[i_track];
        bool b;

        if( !tk->b_ok || tk->b_chapter )
            continue;

        es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE, tk->p_es, &b );
        msg_Dbg( p_demux, "track %u %s!", tk->i_track_ID, b ? "enabled" : "disabled" );

        if( tk->b_selected && !b )
            MP4_TrackUnselect( p_demux, tk );
        else if( !tk->b_selected && b)
            MP4_frg_TrackSelect( p_demux, tk );

        if( tk->b_selected )
            i_track_selected++;
    }

    if( i_track_selected <= 0 )
    {
        msg_Warn( p_demux, "no track selected, exiting..." );
        return 0;
    }

    if ( p_sys->context.i_current_box_type != ATOM_mdat )
    {
        /* Othewise mdat is skipped. FIXME: mdat reading ! */
        const uint8_t *p_peek;
        int i_read  = stream_Peek( p_demux->s, &p_peek, 8 );
        if ( i_read < 8 )
            return 0;

        p_sys->context.i_current_box_type = VLC_FOURCC( p_peek[4], p_peek[5], p_peek[6], p_peek[7] );

        if ( p_sys->context.i_current_box_type != ATOM_mdat )
        {
            const int i_tell = stream_Tell( p_demux->s );
            if ( i_tell >= 0 && ! BoxExistsInRootTree( p_sys->p_root, p_sys->context.i_current_box_type, (uint64_t)i_tell ) )
            {// only if !b_probed ??
                MP4_Box_t *p_vroot = LoadNextChunk( p_demux );

                MP4_Box_t *p_fragbox = MP4_BoxGet( p_vroot, "moof" );
                if( !p_fragbox )
                {
                    p_fragbox = MP4_BoxGet( p_vroot, "moov" );
                    RestartAllTracks( p_demux, p_vroot );
                }

                if(!p_fragbox)
                {
                    MP4_BoxFree( p_demux->s, p_vroot );
                    msg_Err(p_demux, "no moof or moov in current chunk");
                    return 1;
                }

                MP4_Box_t *p_mfhd = MP4_BoxGet( p_fragbox, "mfhd" );
                if( p_mfhd && BOXDATA(p_mfhd) )
                {
                    /* Detect and Handle Passive Seek */
                    if( p_sys->context.i_lastseqnumber + 1 != BOXDATA(p_mfhd)->i_sequence_number )
                    {
                        msg_Info( p_demux, "Fragment sequence discontinuity detected %"PRIu32" != %"PRIu32,
                                  BOXDATA(p_mfhd)->i_sequence_number, p_sys->context.i_lastseqnumber + 1 );
                        MP4_Box_t *p_sidx = MP4_BoxGet( p_vroot, "sidx" );
                        if( p_sidx && BOXDATA(p_sidx) && BOXDATA(p_sidx)->i_timescale )
                        {
                            mtime_t i_time_base = BOXDATA(p_sidx)->i_earliest_presentation_time;

                            for( unsigned int i_track = 0; i_track < p_sys->i_tracks; i_track++ )
                            {
                                p_sys->track[i_track].i_time = i_time_base * p_sys->track[i_track].i_timescale
                                                                           / BOXDATA(p_sidx)->i_timescale;
                            }

                            p_sys->i_time = i_time_base * p_sys->i_timescale / BOXDATA(p_sidx)->i_timescale;
                            p_sys->i_pcr  = VLC_TS_INVALID;
                        }
                    }
                    p_sys->context.i_lastseqnumber = BOXDATA(p_mfhd)->i_sequence_number;
                }

                /* detach */
                while( p_vroot->p_first )
                {
                    if( p_vroot->p_first == p_fragbox )
                    {
                        p_vroot->p_first = p_fragbox->p_next;
                    }
                    else
                    {
                        MP4_Box_t *p_cur = p_vroot->p_first;
                        p_vroot->p_first = p_cur->p_next;
                        p_cur->p_next = NULL;
                        msg_Dbg(p_demux, "ignoring box %4.4s", (char*)&p_cur->i_type);
                        MP4_BoxFree( p_demux->s, p_cur );
                    }
                }
                p_fragbox->p_next = NULL;

                /* create fragment */
                AddFragment( p_demux, p_fragbox );

                /* Append to root */
                p_sys->p_root->p_last->p_next = p_fragbox;
                p_sys->p_root->p_last = p_fragbox;
                MP4_BoxFree( p_demux->s, p_vroot );
            }
            else
            {
                /* Skip */
                msg_Err( p_demux, "skipping known chunk type %4.4s size %"PRIu32, (char*)& p_sys->context.i_current_box_type, GetDWBE( p_peek ) );
                stream_Read( p_demux->s, NULL, GetDWBE( p_peek ) );
            }
        }
        else
        {
            /* skip mdat header */
            p_sys->context.p_fragment = GetFragmentByPos( p_demux,
                                       stream_Tell( p_demux->s ) + 8, true );
        }

    }

    if ( p_sys->context.i_current_box_type == ATOM_mdat )
    {
        assert(p_sys->context.p_fragment);
        if ( p_sys->context.p_fragment )
        switch( p_sys->context.p_fragment->p_moox->i_type )
        {
            case ATOM_moov://[ftyp/moov, mdat]+ -> [moof, mdat]+
                LeafParseMDATwithMOOV( p_demux );
            break;
            case ATOM_moof:
                LeafParseMDATwithMOOF( p_demux, p_sys->context.p_fragment->p_moox ); // BACKUP CHUNK!
            break;
        default:
             msg_Err( p_demux, "fragment type %4.4s", (char*) &p_sys->context.p_fragment->p_moox->i_type );
             break;
        }
    }

    /* Get current time */
    mtime_t i_lowest_dts = VLC_TS_INVALID;
    mtime_t i_lowest_time = INT64_MAX;
    for( unsigned int i_track = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        const mp4_track_t *p_track = &p_sys->track[i_track];
        if( !p_track->b_selected || ( p_track->fmt.i_cat != VIDEO_ES && p_track->fmt.i_cat != AUDIO_ES ) )
            continue;

        i_lowest_time = __MIN( i_lowest_time, p_track->i_time * p_sys->i_timescale / p_track->i_timescale );

        if ( i_lowest_dts == VLC_TS_INVALID )
            i_lowest_dts = CLOCK_FREQ * p_track->i_time / p_track->i_timescale;
        else
            i_lowest_dts = __MIN( i_lowest_dts, CLOCK_FREQ * p_track->i_time / p_track->i_timescale );
    }

    p_sys->i_time = i_lowest_time;
    p_sys->i_pcr = i_lowest_dts;
    es_out_Control( p_demux->out, ES_OUT_SET_PCR, VLC_TS_0 + p_sys->i_pcr );

    return 1;
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
