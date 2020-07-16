/*****************************************************************************
 * mkv.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2005, 2008, 2010 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Steve Lhomme <steve.lhomme@free.fr>
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

#include "mkv.hpp"
#include "util.hpp"

#include "matroska_segment.hpp"
#include "demux.hpp"

#include "chapters.hpp"
#include "Ebml_parser.hpp"

#include <new>

extern "C" {
    #include "../av1_unpack.h"
}

#include <vlc_fs.h>
#include <vlc_url.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
namespace mkv {
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );
} // namespace

using namespace mkv;
vlc_module_begin ()
    set_shortname( "Matroska" )
    set_description( N_("Matroska stream demuxer" ) )
    set_capability( "demux", 50 )
    set_callbacks( Open, Close )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )

    add_bool( "mkv-use-ordered-chapters", true,
            N_("Respect ordered chapters"),
            N_("Play chapters in the order specified in the segment."), false );

    add_bool( "mkv-use-chapter-codec", true,
            N_("Chapter codecs"),
            N_("Use chapter codecs found in the segment."), true );

    add_bool( "mkv-preload-local-dir", true,
            N_("Preload MKV files in the same directory"),
            N_("Preload matroska files in the same directory to find linked segments (not good for broken files)."), false );

    add_bool( "mkv-seek-percent", false,
            N_("Seek based on percent not time"),
            N_("Seek based on percent not time."), true );

    add_bool( "mkv-use-dummy", false,
            N_("Dummy Elements"),
            N_("Read and discard unknown EBML elements (not good for broken files)."), true );

    add_bool( "mkv-preload-clusters", false,
            N_("Preload clusters"),
            N_("Find all cluster positions by jumping cluster-to-cluster before playback"), true );

    add_shortcut( "mka", "mkv" )
vlc_module_end ()

namespace mkv {

struct demux_sys_t;

static int  Demux  ( demux_t * );
static int  Control( demux_t *, int, va_list );
static int  Seek   ( demux_t *, vlc_tick_t i_mk_date, double f_percent, virtual_chapter_c *p_vchapter, bool b_precise = true );

/*****************************************************************************
 * Open: initializes matroska demux structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t            *p_demux = (demux_t*)p_this;
    demux_sys_t        *p_sys;
    matroska_stream_c  *p_stream;
    matroska_segment_c *p_segment;
    const uint8_t      *p_peek;
    std::string         s_path, s_filename;
    bool                b_need_preload = false;

    /* peek the begining */
    if( vlc_stream_Peek( p_demux->s, &p_peek, 4 ) < 4 ) return VLC_EGENERIC;

    /* is a valid file */
    if( p_peek[0] != 0x1a || p_peek[1] != 0x45 ||
        p_peek[2] != 0xdf || p_peek[3] != 0xa3 ) return VLC_EGENERIC;

    /* Set the demux function */
    p_demux->pf_demux   = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys      = p_sys = new demux_sys_t( *p_demux );

    vlc_stream_Control( p_demux->s, STREAM_CAN_SEEK, &p_sys->b_seekable );
    if ( !p_sys->b_seekable || vlc_stream_Control(
          p_demux->s, STREAM_CAN_FASTSEEK, &p_sys->b_fastseekable ) )
        p_sys->b_fastseekable = false;

    es_out_Control( p_demux->out, ES_OUT_SET_ES_CAT_POLICY, VIDEO_ES,
                    ES_OUT_ES_POLICY_EXCLUSIVE );

    p_stream = new matroska_stream_c( p_demux->s, false );
    if ( unlikely(p_stream == NULL) )
    {
        msg_Err( p_demux, "failed to create matroska_stream_c" );
        delete p_sys;
        return VLC_ENOMEM;
    }
    p_sys->streams.push_back( p_stream );

    if( !p_sys->AnalyseAllSegmentsFound( p_demux, p_stream ) )
    {
        msg_Err( p_demux, "cannot find KaxSegment or missing mandatory KaxInfo" );
        goto error;
    }

    for (size_t i=0; i<p_stream->segments.size(); i++)
    {
        p_stream->segments[i]->Preload();
        b_need_preload |= p_stream->segments[i]->b_ref_external_segments;
        if ( p_stream->segments[i]->translations.size() &&
             p_stream->segments[i]->translations[0]->codec_id == MATROSKA_CHAPTER_CODEC_DVD &&
             p_stream->segments[i]->families.size() )
            b_need_preload = true;
    }

    p_segment = p_stream->segments[0];
    if( p_segment->cluster == NULL && p_segment->stored_editions.size() == 0 )
    {
        msg_Err( p_demux, "cannot find any cluster or chapter, damaged file ?" );
        goto error;
    }

    if (b_need_preload && var_InheritBool( p_demux, "mkv-preload-local-dir" ))
    {
        msg_Dbg( p_demux, "Preloading local dir" );
        /* get the files from the same dir from the same family (based on p_demux->psz_path) */
        if ( p_demux->psz_filepath && !strncasecmp( p_demux->psz_url, "file:", 5 ) )
        {
            // assume it's a regular file
            // get the directory path
            s_path = p_demux->psz_filepath;
            if (s_path.at(s_path.length() - 1) == DIR_SEP_CHAR)
            {
                s_path = s_path.substr(0,s_path.length()-1);
            }
            else
            {
                if (s_path.find_last_of(DIR_SEP_CHAR) > 0)
                {
                    s_path = s_path.substr(0,s_path.find_last_of(DIR_SEP_CHAR));
                }
            }

            DIR *p_src_dir = vlc_opendir(s_path.c_str());

            if (p_src_dir != NULL)
            {
                const char *psz_file;
                while ((psz_file = vlc_readdir(p_src_dir)) != NULL)
                {
                    if (strlen(psz_file) > 4)
                    {
                        s_filename = s_path + DIR_SEP_CHAR + psz_file;

#if defined(_WIN32) || defined(__OS2__)
                        if (!strcasecmp(s_filename.c_str(), p_demux->psz_filepath))
#else
                        if (!s_filename.compare(p_demux->psz_filepath))
#endif
                        {
                            continue; // don't reuse the original opened file
                        }

                        if (!strcasecmp(s_filename.c_str() + s_filename.length() - 4, ".mkv") ||
                            !strcasecmp(s_filename.c_str() + s_filename.length() - 4, ".mka"))
                        {
                            // test whether this file belongs to our family
                            const uint8_t *p_peek;
                            bool          file_ok = false;
                            char          *psz_url = vlc_path2uri( s_filename.c_str(), "file" );
                            stream_t      *p_file_stream = vlc_stream_NewURL(
                                                            p_demux,
                                                            psz_url );
                            /* peek the begining */
                            if( p_file_stream &&
                                vlc_stream_Peek( p_file_stream, &p_peek, 4 ) >= 4
                                && p_peek[0] == 0x1a && p_peek[1] == 0x45 &&
                                p_peek[2] == 0xdf && p_peek[3] == 0xa3 ) file_ok = true;

                            if ( file_ok )
                            {
                                matroska_stream_c *p_stream = new matroska_stream_c( p_file_stream, true );

                                if ( !p_sys->AnalyseAllSegmentsFound( p_demux, p_stream ) )
                                {
                                    msg_Dbg( p_demux, "the file '%s' will not be used", s_filename.c_str() );
                                    delete p_stream;
                                }
                                else
                                {
                                    p_sys->streams.push_back( p_stream );
                                }
                            }
                            else
                            {
                                if( p_file_stream ) {
                                    vlc_stream_Delete( p_file_stream );
                                }
                                msg_Dbg( p_demux, "the file '%s' cannot be opened", s_filename.c_str() );
                            }
                            free( psz_url );
                        }
                    }
                }
                closedir( p_src_dir );
            }
        }

        p_sys->PreloadFamily( *p_segment );
    }
    else if (b_need_preload)
        msg_Warn( p_demux, "This file references other files, you may want to enable the preload of local directory");

    if ( !p_sys->PreloadLinked() ||
         !p_sys->PreparePlayback( *p_sys->p_current_vsegment, 0 ) )
    {
        msg_Err( p_demux, "cannot use the segment" );
        goto error;
    }

    if (!p_sys->FreeUnused())
    {
        msg_Err( p_demux, "no usable segment" );
        goto error;
    }

    return VLC_SUCCESS;

error:
    delete p_sys;
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = reinterpret_cast<demux_t*>( p_this );
    demux_sys_t *p_sys = (demux_sys_t *)p_demux->p_sys;
    virtual_segment_c *p_vsegment = p_sys->p_current_vsegment;
    if( p_vsegment )
    {
        matroska_segment_c *p_segment = p_vsegment->CurrentSegment();
        if( p_segment )
            p_segment->ESDestroy();
    }

    delete p_sys;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = (demux_sys_t *)p_demux->p_sys;
    vlc_tick_t  i64;
    double      *pf, f;
    int         i_skp;
    size_t      i_idx;
    bool            b;

    vlc_meta_t *p_meta;
    input_attachment_t ***ppp_attach;
    int *pi_int;

    switch( i_query )
    {
        case DEMUX_CAN_SEEK:
            return vlc_stream_vaControl( p_demux->s, i_query, args );

        case DEMUX_GET_ATTACHMENTS:
            ppp_attach = va_arg( args, input_attachment_t*** );
            pi_int = va_arg( args, int * );

            if( p_sys->stored_attachments.size() <= 0 )
                return VLC_EGENERIC;

            *pi_int = p_sys->stored_attachments.size();
            *ppp_attach = static_cast<input_attachment_t**>( vlc_alloc( p_sys->stored_attachments.size(),
                                                        sizeof(input_attachment_t*) ) );
            if( !(*ppp_attach) )
                return VLC_ENOMEM;
            for( size_t i = 0; i < p_sys->stored_attachments.size(); i++ )
            {
                attachment_c *a = p_sys->stored_attachments[i];
                (*ppp_attach)[i] = vlc_input_attachment_New( a->fileName(), a->mimeType(), NULL,
                                                             a->p_data, a->size() );
                if( !(*ppp_attach)[i] )
                {
                    free(*ppp_attach);
                    return VLC_ENOMEM;
                }
            }
            return VLC_SUCCESS;

        case DEMUX_GET_META:
            p_meta = va_arg( args, vlc_meta_t* );
            vlc_meta_Merge( p_meta, p_sys->meta );
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
            if( p_sys->i_duration > 0 )
                *va_arg( args, vlc_tick_t * ) = p_sys->i_duration;
            else
                *va_arg( args, vlc_tick_t * ) = VLC_TICK_INVALID;
            return VLC_SUCCESS;

        case DEMUX_GET_POSITION:
            pf = va_arg( args, double * );
            if ( p_sys->i_duration > 0 )
                *pf = static_cast<double> (p_sys->i_pcr >= (p_sys->i_start_pts + p_sys->i_mk_chapter_time) ?
                                               p_sys->i_pcr :
                                               (p_sys->i_start_pts + p_sys->i_mk_chapter_time) ) / p_sys->i_duration;
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            if( p_sys->i_duration > 0)
            {
                f = va_arg( args, double );
                b = va_arg( args, int ); /* precise? */
                return Seek( p_demux, -1, f, NULL, b );
            }
            return VLC_EGENERIC;

        case DEMUX_GET_TIME:
            *va_arg( args, vlc_tick_t * ) = p_sys->i_pcr;
            return VLC_SUCCESS;

        case DEMUX_GET_TITLE_INFO:
            if( p_sys->titles.size() > 1 || ( p_sys->titles.size() == 1 && p_sys->titles[0]->i_seekpoint > 0 ) )
            {
                input_title_t ***ppp_title = va_arg( args, input_title_t*** );
                int *pi_int = va_arg( args, int* );

                *pi_int = p_sys->titles.size();
                *ppp_title = static_cast<input_title_t**>( vlc_alloc( p_sys->titles.size(), sizeof( input_title_t* ) ) );

                for( size_t i = 0; i < p_sys->titles.size(); i++ )
                    (*ppp_title)[i] = vlc_input_title_Duplicate( p_sys->titles[i] );
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_SET_TITLE:
            /* handle editions as titles */
            i_idx = va_arg( args, int );
            if(i_idx <  p_sys->titles.size() && p_sys->titles[i_idx]->i_seekpoint)
            {
                const int i_edition = p_sys->p_current_vsegment->i_current_edition;
                const int i_title = p_sys->i_current_title;
                p_sys->p_current_vsegment->i_current_edition = i_idx;
                p_sys->i_current_title = i_idx;
                if( VLC_SUCCESS ==
                    Seek( p_demux, p_sys->titles[i_idx]->seekpoint[0]->i_time_offset, -1, NULL) )
                {
                    p_sys->i_updates |= INPUT_UPDATE_SEEKPOINT|INPUT_UPDATE_TITLE;
                    p_sys->i_current_seekpoint = 0;
                    p_sys->i_duration = p_sys->titles[i_idx]->i_length;
                    return VLC_SUCCESS;
                }
                else
                {
                    p_sys->p_current_vsegment->i_current_edition = i_edition;
                    p_sys->i_current_title = i_title;
                }
            }
            return VLC_EGENERIC;

        case DEMUX_SET_SEEKPOINT:
            i_skp = va_arg( args, int );

            // TODO change the way it works with the << & >> buttons on the UI (+1/-1 instead of a number)
            if( p_sys->titles.size() && i_skp < p_sys->titles[p_sys->i_current_title]->i_seekpoint)
            {
                int i_ret = Seek( p_demux, p_sys->titles[p_sys->i_current_title]->seekpoint[i_skp]->i_time_offset, -1, NULL);
                if( i_ret == VLC_SUCCESS )
                {
                    p_sys->i_updates |= INPUT_UPDATE_SEEKPOINT;
                    p_sys->i_current_seekpoint = i_skp;
                }
                return i_ret;
            }
            return VLC_EGENERIC;

        case DEMUX_TEST_AND_CLEAR_FLAGS:
        {
            unsigned *restrict flags = va_arg( args, unsigned * );
            *flags &= p_sys->i_updates;
            p_sys->i_updates &= ~*flags;
            return VLC_SUCCESS;
        }

        case DEMUX_GET_TITLE:
            *va_arg( args, int * ) = p_sys->i_current_title;
            return VLC_SUCCESS;

        case DEMUX_GET_SEEKPOINT:
            *va_arg( args, int * ) = p_sys->i_current_seekpoint;
            return VLC_SUCCESS;

        case DEMUX_GET_FPS:
            pf = va_arg( args, double * );
            *pf = 0.0;
            if( p_sys->p_current_vsegment && p_sys->p_current_vsegment->CurrentSegment() )
            {
                typedef matroska_segment_c::tracks_map_t tracks_map_t;

                const matroska_segment_c *p_segment = p_sys->p_current_vsegment->CurrentSegment();
                for( tracks_map_t::const_iterator it = p_segment->tracks.begin(); it != p_segment->tracks.end(); ++it )
                {
                    const mkv_track_t &track = *it->second;

                    if( track.fmt.i_cat == VIDEO_ES && track.fmt.video.i_frame_rate_base > 0 )
                    {
                        *pf = (double)track.fmt.video.i_frame_rate / track.fmt.video.i_frame_rate_base;
                        break;
                    }
                }
            }
            return VLC_SUCCESS;

        case DEMUX_SET_TIME:
            i64 = va_arg( args, vlc_tick_t );
            b = va_arg( args, int ); /* precise? */
            msg_Dbg(p_demux,"SET_TIME to %" PRId64, i64 );
            return Seek( p_demux, i64, -1, NULL, b );

        case DEMUX_CAN_PAUSE:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_GET_PTS_DELAY:
            return demux_vaControlHelper( p_demux->s, 0, -1, 0, 1, i_query, args );

        default:
            return VLC_EGENERIC;
    }
}

/* Seek */
static int Seek( demux_t *p_demux, vlc_tick_t i_mk_date, double f_percent, virtual_chapter_c *p_vchapter, bool b_precise )
{
    demux_sys_t *p_sys = (demux_sys_t *)p_demux->p_sys;
    virtual_segment_c  *p_vsegment = p_sys->p_current_vsegment;
    matroska_segment_c *p_segment = p_vsegment->CurrentSegment();

    if( f_percent < 0 ) msg_Dbg( p_demux, "seek request to i_pos = %" PRId64, i_mk_date );
    else                msg_Dbg( p_demux, "seek request to %.2f%%", f_percent * 100 );

    if( i_mk_date < 0 && f_percent < 0 )
    {
        msg_Warn( p_demux, "cannot seek nowhere!" );
        return VLC_EGENERIC;
    }
    if( f_percent > 1.0 )
    {
        msg_Warn( p_demux, "cannot seek so far!" );
        return VLC_EGENERIC;
    }
    if( p_sys->i_duration < 0 )
    {
        msg_Warn( p_demux, "cannot seek without duration!");
        return VLC_EGENERIC;
    }
    if( !p_segment )
    {
        msg_Warn( p_demux, "cannot seek without valid segment position");
        return VLC_EGENERIC;
    }

    /* seek without index or without date */
    if( f_percent >= 0 && (var_InheritBool( p_demux, "mkv-seek-percent" ) || i_mk_date < 0 ))
    {
        i_mk_date = vlc_tick_t( f_percent * p_sys->i_duration );
    }
    return p_vsegment->Seek( *p_demux, i_mk_date, p_vchapter, b_precise ) ? VLC_SUCCESS : VLC_EGENERIC;
}

/* Needed by matroska_segment::Seek() and Seek */
void BlockDecode( demux_t *p_demux, KaxBlock *block, KaxSimpleBlock *simpleblock,
                  KaxBlockAdditions *additions,
                  vlc_tick_t i_pts, int64_t i_duration, bool b_key_picture,
                  bool b_discardable_picture )
{
    demux_sys_t *p_sys = (demux_sys_t *)p_demux->p_sys;
    matroska_segment_c *p_segment = p_sys->p_current_vsegment->CurrentSegment();

    KaxInternalBlock& internal_block = simpleblock
        ? static_cast<KaxInternalBlock&>( *simpleblock )
        : static_cast<KaxInternalBlock&>( *block );

    if( !p_segment ) return;

    mkv_track_t *p_track = p_segment->FindTrackByBlock( block, simpleblock );
    if( p_track == NULL )
    {
        msg_Err( p_demux, "invalid track number" );
        return;
    }

    mkv_track_t &track = *p_track;

    if( track.fmt.i_cat != DATA_ES && track.p_es == NULL )
    {
        msg_Err( p_demux, "unknown track number" );
        return;
    }

    i_pts -= track.i_codec_delay;

    if ( track.fmt.i_cat != DATA_ES )
    {
        bool b;
        es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE, track.p_es, &b );

        if( !b )
        {
            if( track.fmt.i_cat == VIDEO_ES || track.fmt.i_cat == AUDIO_ES )
                track.i_last_dts = VLC_TICK_INVALID;
            return;
        }
    }

    size_t frame_size = 0;
    size_t block_size = internal_block.GetSize();
    const unsigned i_number_frames = internal_block.NumberFrames();

    for( unsigned int i_frame = 0; i_frame < i_number_frames; i_frame++ )
    {
        block_t *p_block;
        DataBuffer *data = &internal_block.GetBuffer(i_frame);

        frame_size += data->Size();
        if( !data->Buffer() || data->Size() > frame_size || frame_size > block_size  )
        {
            msg_Warn( p_demux, "Cannot read frame (too long or no frame)" );
            break;
        }
        size_t extra_data = track.fmt.i_codec == VLC_CODEC_PRORES ? 8 : 0;

        if( track.i_compression_type == MATROSKA_COMPRESSION_HEADER &&
            track.p_compression_data != NULL &&
            track.i_encoding_scope & MATROSKA_ENCODING_SCOPE_ALL_FRAMES )
            p_block = MemToBlock( data->Buffer(), data->Size(), track.p_compression_data->GetSize() + extra_data );
        else if( unlikely( track.fmt.i_codec == VLC_CODEC_WAVPACK ) )
            p_block = packetize_wavpack( track, data->Buffer(), data->Size() );
        else
            p_block = MemToBlock( data->Buffer(), data->Size(), extra_data );

        if( p_block == NULL )
        {
            break;
        }

#if defined(HAVE_ZLIB_H)
        if( track.i_compression_type == MATROSKA_COMPRESSION_ZLIB &&
            track.i_encoding_scope & MATROSKA_ENCODING_SCOPE_ALL_FRAMES )
        {
            p_block = block_zlib_decompress( VLC_OBJECT(p_demux), p_block );
            if( p_block == NULL )
                break;
        }
        else
#endif
        if( track.i_compression_type == MATROSKA_COMPRESSION_HEADER &&
            track.i_encoding_scope & MATROSKA_ENCODING_SCOPE_ALL_FRAMES )
        {
            memcpy( p_block->p_buffer, track.p_compression_data->GetBuffer(), track.p_compression_data->GetSize() );
        }
        if ( track.fmt.i_codec == VLC_CODEC_PRORES )
            memcpy( p_block->p_buffer + 4, "icpf", 4 );

        if ( b_key_picture )
            p_block->i_flags |= BLOCK_FLAG_TYPE_I;

        switch( track.fmt.i_codec )
        {
        case VLC_CODEC_COOK:
        case VLC_CODEC_ATRAC3:
        {
            handle_real_audio(p_demux, &track, p_block, i_pts);
            block_Release(p_block);
            i_pts = ( track.i_default_duration )?
                i_pts + track.i_default_duration:
                VLC_TICK_INVALID;
            continue;
         }

         case VLC_CODEC_WEBVTT:
            {
                const uint8_t *p_addition = NULL;
                size_t i_addition = 0;
                if(additions)
                {
                    KaxBlockMore *blockmore = FindChild<KaxBlockMore>(*additions);
                    if(blockmore)
                    {
                        KaxBlockAdditional *addition = FindChild<KaxBlockAdditional>(*blockmore);
                        if(addition)
                        {
                            i_addition = static_cast<std::string::size_type>(addition->GetSize());
                            p_addition = reinterpret_cast<const uint8_t *>(addition->GetBuffer());
                        }
                    }
                }
                p_block = WEBVTT_Repack_Sample( p_block, /* D_WEBVTT -> webm */
                                                !p_track->codec.compare( 0, 1, "D" ),
                                                p_addition, i_addition );
                if( !p_block )
                    continue;
            }
            break;

         case VLC_CODEC_OPUS:
            {
                vlc_tick_t i_length = VLC_TICK_FROM_NS(i_duration * track.f_timecodescale *
                                                       p_segment->i_timescale);
                if ( i_length < 0 ) i_length = 0;
                p_block->i_nb_samples = samples_from_vlc_tick(i_length, track.fmt.audio.i_rate);
            }
            break;

         case VLC_CODEC_DVBS:
            {
                p_block = block_Realloc( p_block, 2, p_block->i_buffer + 1);

                if( unlikely( !p_block ) )
                    continue;

                p_block->p_buffer[0] = 0x20; // data identifier
                p_block->p_buffer[1] = 0x00; // subtitle stream id
                p_block->p_buffer[ p_block->i_buffer - 1 ] = 0x3f; // end marker
            }
            break;

          case VLC_CODEC_AV1:
            p_block = AV1_Unpack_Sample( p_block );
            if( unlikely( !p_block ) )
                continue;
            break;
        }

        if( track.fmt.i_cat != VIDEO_ES )
        {
            if ( track.fmt.i_cat == DATA_ES )
            {
                // TODO handle the start/stop times of this packet
                if( p_block->i_size >= sizeof(pci_t))
                    p_sys->ev.SetPci( (const pci_t *)&p_block->p_buffer[1]);
                block_Release( p_block );
                return;
            }
            p_block->i_dts = p_block->i_pts = i_pts;
        }
        else
        {
            // correct timestamping when B frames are used
            if( track.b_dts_only )
            {
                p_block->i_pts = VLC_TICK_INVALID;
                p_block->i_dts = i_pts;
            }
            else if( track.b_pts_only )
            {
                p_block->i_pts = i_pts;
                p_block->i_dts = i_pts;
            }
            else
            {
                p_block->i_pts = i_pts;
                // condition when the DTS is correct (keyframe or B frame == NOT P frame)
                if ( b_key_picture || b_discardable_picture )
                        p_block->i_dts = p_block->i_pts;
                else if ( track.i_last_dts == VLC_TICK_INVALID )
                    p_block->i_dts = i_pts;
                else
                    p_block->i_dts = std::min( i_pts, track.i_last_dts + track.i_default_duration );
            }
        }

        send_Block( p_demux, &track, p_block, i_number_frames, i_duration );

        /* use time stamp only for first block */
        i_pts = ( track.i_default_duration )?
                 i_pts + track.i_default_duration:
                 ( track.fmt.b_packetized ) ? VLC_TICK_INVALID : i_pts + 1;
    }
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux)
{
    demux_sys_t *p_sys = (demux_sys_t *)p_demux->p_sys;

    vlc_mutex_locker demux_lock ( &p_sys->lock_demuxer );

    virtual_segment_c  *p_vsegment = p_sys->p_current_vsegment;

    if( p_sys->i_pts >= p_sys->i_start_pts )
    {
        if ( p_vsegment->UpdateCurrentToChapter( *p_demux ) )
            return VLC_DEMUXER_SUCCESS;
        p_vsegment = p_sys->p_current_vsegment;
    }

    matroska_segment_c *p_segment = p_vsegment->CurrentSegment();
    if ( p_segment == NULL )
        return VLC_DEMUXER_EOF;

    KaxBlock *block;
    KaxSimpleBlock *simpleblock;
    KaxBlockAdditions *additions;
    int64_t i_block_duration = 0;
    bool b_key_picture;
    bool b_discardable_picture;

    if( p_segment->BlockGet( block, simpleblock, additions,
                             &b_key_picture, &b_discardable_picture, &i_block_duration ) )
    {
        if ( p_vsegment->CurrentEdition() && p_vsegment->CurrentEdition()->b_ordered )
        {
            const virtual_chapter_c *p_chap = p_vsegment->CurrentChapter();
            // check if there are more chapters to read
            if ( p_chap != NULL )
            {
                /* TODO handle successive chapters with the same user_start_time/user_end_time
                */
                p_sys->i_pts = p_chap->i_mk_virtual_stop_time + VLC_TICK_0;
                p_sys->i_pts++; // trick to avoid staying on segments with no duration and no content

                return VLC_DEMUXER_SUCCESS;
            }
        }

        msg_Warn( p_demux, "cannot get block EOF?" );
        return VLC_DEMUXER_EOF;
    }

    KaxInternalBlock& internal_block = block
        ? static_cast<KaxInternalBlock&>( *block )
        : static_cast<KaxInternalBlock&>( *simpleblock );

    {
        mkv_track_t *p_track = p_segment->FindTrackByBlock( block, simpleblock );

        if( p_track == NULL )
        {
            msg_Err( p_demux, "invalid track number" );
            delete block;
            delete additions;
            return VLC_DEMUXER_EGENERIC;
        }

        mkv_track_t &track = *p_track;


        if( track.i_skip_until_fpos != std::numeric_limits<uint64_t>::max() ) {

            uint64_t block_fpos = internal_block.GetElementPosition();

            if ( track.i_skip_until_fpos > block_fpos )
            {
                delete block;
                delete additions;
                return VLC_DEMUXER_SUCCESS; // this block shall be ignored
            }
        }
    }

    /* update pcr */
    {
        vlc_tick_t i_pcr = VLC_TICK_INVALID;

        typedef matroska_segment_c::tracks_map_t tracks_map_t;

        for( tracks_map_t::const_iterator it = p_segment->tracks.begin(); it != p_segment->tracks.end(); ++it )
        {
            mkv_track_t &track = *it->second;

            if( track.i_last_dts == VLC_TICK_INVALID )
                continue;

            if( track.fmt.i_cat != VIDEO_ES && track.fmt.i_cat != AUDIO_ES )
                continue;

            if( track.i_last_dts < i_pcr || i_pcr == VLC_TICK_INVALID )
            {
                i_pcr = track.i_last_dts;
            }
        }

        if( i_pcr != VLC_TICK_INVALID && i_pcr > p_sys->i_pcr )
        {
            if( es_out_SetPCR( p_demux->out, i_pcr ) )
            {
                msg_Err( p_demux, "ES_OUT_SET_PCR failed, aborting." );
                delete block;
                delete additions;
                return VLC_DEMUXER_EGENERIC;
            }

            p_sys->i_pcr = i_pcr;
        }
    }

    /* set pts */
    {
        p_sys->i_pts = p_sys->i_mk_chapter_time + VLC_TICK_0;
        p_sys->i_pts += VLC_TICK_FROM_NS(internal_block.GlobalTimecode());
    }

    if ( p_vsegment->CurrentEdition() &&
         p_vsegment->CurrentEdition()->b_ordered &&
         p_vsegment->CurrentChapter() == NULL )
    {
        /* nothing left to read in this ordered edition */
        delete block;
        delete additions;
        return VLC_DEMUXER_EOF;
    }

    BlockDecode( p_demux, block, simpleblock, additions,
                 p_sys->i_pts, i_block_duration, b_key_picture, b_discardable_picture );

    delete block;
    delete additions;

    return VLC_DEMUXER_SUCCESS;
}

mkv_track_t::mkv_track_t(enum es_format_category_e es_cat) :
    b_default(true)
  ,b_enabled(true)
  ,b_forced(false)
  ,i_number(0)
  ,i_extra_data(0)
  ,p_extra_data(NULL)
  ,b_dts_only(false)
  ,b_pts_only(false)
  ,b_no_duration(false)
  ,i_default_duration(0)
  ,f_timecodescale(1.0)
  ,i_last_dts(VLC_TICK_INVALID)
  ,i_skip_until_fpos(std::numeric_limits<uint64_t>::max())
  ,f_fps(0)
  ,p_es(NULL)
  ,i_original_rate(0)
  ,i_chans_to_reorder(0)
  ,p_sys(NULL)
  ,b_discontinuity(false)
  ,i_compression_type(MATROSKA_COMPRESSION_NONE)
  ,i_encoding_scope(MATROSKA_ENCODING_SCOPE_ALL_FRAMES)
  ,p_compression_data(NULL)
  ,i_seek_preroll(0)
  ,i_codec_delay(0)
{
    std::memset( &pi_chan_table, 0, sizeof( pi_chan_table ) );

    es_format_Init(&fmt, es_cat, 0);

    switch( es_cat )
    {
        case AUDIO_ES:
            fmt.audio.i_channels = 1;
            fmt.audio.i_rate = 8000;
            /* fall through */
        case VIDEO_ES:
        case SPU_ES:
            fmt.psz_language = strdup("English");
            break;
        default:
            // no language needed
            break;
    }
}

mkv_track_t::~mkv_track_t()
{
    es_format_Clean( &fmt );
    assert(p_es == NULL); // did we leak an ES ?

    free(p_extra_data);

    delete p_compression_data;
    delete p_sys;
}

matroska_stream_c::matroska_stream_c( stream_t *s, bool owner )
    :io_callback( s, owner )
    ,estream( io_callback )
{}

bool matroska_stream_c::isUsed() const
{
    for( size_t j = 0; j < segments.size(); j++ )
    {
        if( segments[j]->b_preloaded )
            return true;
    }
    return false;
}

} // namespace
