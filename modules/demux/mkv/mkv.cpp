/*****************************************************************************
 * mkv.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Steve Lhomme <steve.lhomme@free.fr>
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

#include "mkv.hpp"
#include "util.hpp"

#include "matroska_segment.hpp"
#include "demux.hpp"

#include "chapters.hpp"
#include "Ebml_parser.hpp"

#include "stream_io_callback.hpp"

#include <vlc_fs.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_shortname( "Matroska" )
    set_description( N_("Matroska stream demuxer" ) )
    set_capability( "demux", 50 )
    set_callbacks( Open, Close )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )

    add_bool( "mkv-use-ordered-chapters", true, NULL,
            N_("Ordered chapters"),
            N_("Play ordered chapters as specified in the segment."), true );

    add_bool( "mkv-use-chapter-codec", true, NULL,
            N_("Chapter codecs"),
            N_("Use chapter codecs found in the segment."), true );

    add_bool( "mkv-preload-local-dir", false, NULL,
            N_("Preload Directory"),
            N_("Preload matroska files from the same family in the same directory (not good for broken files)."), true );

    add_bool( "mkv-seek-percent", false, NULL,
            N_("Seek based on percent not time"),
            N_("Seek based on percent not time."), true );

    add_bool( "mkv-use-dummy", false, NULL,
            N_("Dummy Elements"),
            N_("Read and discard unknown EBML elements (not good for broken files)."), true );

    add_shortcut( "mka" )
    add_shortcut( "mkv" )
vlc_module_end ()

class demux_sys_t;

static int  Demux  ( demux_t * );
static int  Control( demux_t *, int, va_list );
static void Seek   ( demux_t *, mtime_t i_date, double f_percent, chapter_item_c *psz_chapter );

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
    vlc_stream_io_callback *p_io_callback;
    EbmlStream         *p_io_stream;

    /* peek the begining */
    if( stream_Peek( p_demux->s, &p_peek, 4 ) < 4 ) return VLC_EGENERIC;

    /* is a valid file */
    if( p_peek[0] != 0x1a || p_peek[1] != 0x45 ||
        p_peek[2] != 0xdf || p_peek[3] != 0xa3 ) return VLC_EGENERIC;

    /* Set the demux function */
    p_demux->pf_demux   = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys      = p_sys = new demux_sys_t( *p_demux );

    p_io_callback = new vlc_stream_io_callback( p_demux->s, false );
    p_io_stream = new EbmlStream( *p_io_callback );

    if( p_io_stream == NULL )
    {
        msg_Err( p_demux, "failed to create EbmlStream" );
        delete p_io_callback;
        delete p_sys;
        return VLC_EGENERIC;
    }

    p_stream = p_sys->AnalyseAllSegmentsFound( p_demux, p_io_stream, true );
    if( p_stream == NULL )
    {
        msg_Err( p_demux, "cannot find KaxSegment" );
        goto error;
    }
    p_sys->streams.push_back( p_stream );

    p_stream->p_in = p_io_callback;
    p_stream->p_es = p_io_stream;

    for (size_t i=0; i<p_stream->segments.size(); i++)
    {
        p_stream->segments[i]->Preload();
    }

    p_segment = p_stream->segments[0];
    if( p_segment->cluster == NULL )
    {
        msg_Err( p_demux, "cannot find any cluster, damaged file ?" );
        goto error;
    }

    if (var_InheritInteger( p_demux, "mkv-preload-local-dir" ))
    {
        /* get the files from the same dir from the same family (based on p_demux->psz_path) */
        if (p_demux->psz_path[0] != '\0' && !strcmp(p_demux->psz_access, ""))
        {
            // assume it's a regular file
            // get the directory path
            s_path = p_demux->psz_path;
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
                char *psz_file;
                while ((psz_file = vlc_readdir(p_src_dir)) != NULL)
                {
                    if (strlen(psz_file) > 4)
                    {
                        s_filename = s_path + DIR_SEP_CHAR + psz_file;

#ifdef WIN32
                        if (!strcasecmp(s_filename.c_str(), p_demux->psz_path))
#else
                        if (!s_filename.compare(p_demux->psz_path))
#endif
                        {
                            free (psz_file);
                            continue; // don't reuse the original opened file
                        }

#if defined(__GNUC__) && (__GNUC__ < 3)
                        if (!s_filename.compare("mkv", s_filename.length() - 3, 3) ||
                            !s_filename.compare("mka", s_filename.length() - 3, 3))
#else
                        if (!s_filename.compare(s_filename.length() - 3, 3, "mkv") ||
                            !s_filename.compare(s_filename.length() - 3, 3, "mka"))
#endif
                        {
                            // test wether this file belongs to our family
                            const uint8_t *p_peek;
                            bool          file_ok = false;
                            stream_t      *p_file_stream = stream_UrlNew(
                                                            p_demux,
                                                            s_filename.c_str());
                            /* peek the begining */
                            if( p_file_stream &&
                                stream_Peek( p_file_stream, &p_peek, 4 ) >= 4
                                && p_peek[0] == 0x1a && p_peek[1] == 0x45 &&
                                p_peek[2] == 0xdf && p_peek[3] == 0xa3 ) file_ok = true;

                            if ( file_ok )
                            {
                                vlc_stream_io_callback *p_file_io = new vlc_stream_io_callback( p_file_stream, true );
                                EbmlStream *p_estream = new EbmlStream(*p_file_io);

                                p_stream = p_sys->AnalyseAllSegmentsFound( p_demux, p_estream );

                                if ( p_stream == NULL )
                                {
                                    msg_Dbg( p_demux, "the file '%s' will not be used", s_filename.c_str() );
                                    delete p_estream;
                                    delete p_file_io;
                                }
                                else
                                {
                                    p_stream->p_in = p_file_io;
                                    p_stream->p_es = p_estream;
                                    p_sys->streams.push_back( p_stream );
                                }
                            }
                            else
                            {
                                if( p_file_stream ) {
                                    stream_Delete( p_file_stream );
                                }
                                msg_Dbg( p_demux, "the file '%s' cannot be opened", s_filename.c_str() );
                            }
                        }
                    }
                    free (psz_file);
                }
                closedir( p_src_dir );
            }
        }

        p_sys->PreloadFamily( *p_segment );
    }

    p_sys->PreloadLinked( p_segment );

    if ( !p_sys->PreparePlayback( NULL ) )
    {
        msg_Err( p_demux, "cannot use the segment" );
        goto error;
    }

    p_sys->StartUiThread();
 
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
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys   = p_demux->p_sys;

    delete p_sys;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t        *p_sys = p_demux->p_sys;
    int64_t     *pi64;
    double      *pf, f;
    int         i_skp;
    size_t      i_idx;

    vlc_meta_t *p_meta;
    input_attachment_t ***ppp_attach;
    int *pi_int;

    switch( i_query )
    {
        case DEMUX_GET_ATTACHMENTS:
            ppp_attach = (input_attachment_t***)va_arg( args, input_attachment_t*** );
            pi_int = (int*)va_arg( args, int * );

            if( p_sys->stored_attachments.size() <= 0 )
                return VLC_EGENERIC;

            *pi_int = p_sys->stored_attachments.size();
            *ppp_attach = (input_attachment_t**)malloc( sizeof(input_attachment_t**) *
                                                        p_sys->stored_attachments.size() );
            if( !(*ppp_attach) )
                return VLC_ENOMEM;
            for( size_t i = 0; i < p_sys->stored_attachments.size(); i++ )
            {
                attachment_c *a = p_sys->stored_attachments[i];
                (*ppp_attach)[i] = vlc_input_attachment_New( a->psz_file_name.c_str(), a->psz_mime_type.c_str(), NULL,
                                                             a->p_data, a->i_size );
            }
            return VLC_SUCCESS;

        case DEMUX_GET_META:
            p_meta = (vlc_meta_t*)va_arg( args, vlc_meta_t* );
            vlc_meta_Merge( p_meta, p_sys->meta );
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->f_duration > 0.0 )
            {
                *pi64 = (int64_t)(p_sys->f_duration * 1000);
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double * );
            if ( p_sys->f_duration > 0.0 )
                *pf = (double)(p_sys->i_pts >= p_sys->i_start_pts ? p_sys->i_pts : p_sys->i_start_pts ) / (1000.0 * p_sys->f_duration);
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            f = (double)va_arg( args, double );
            Seek( p_demux, -1, f, NULL );
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_pts;
            return VLC_SUCCESS;

        case DEMUX_GET_TITLE_INFO:
            if( p_sys->titles.size() > 1 || ( p_sys->titles.size() == 1 && p_sys->titles[0]->i_seekpoint > 0 ) )
            {
                input_title_t ***ppp_title = (input_title_t***)va_arg( args, input_title_t*** );
                int *pi_int    = (int*)va_arg( args, int* );

                *pi_int = p_sys->titles.size();
                *ppp_title = (input_title_t**)malloc( sizeof( input_title_t**) * p_sys->titles.size() );

                for( size_t i = 0; i < p_sys->titles.size(); i++ )
                {
                    (*ppp_title)[i] = vlc_input_title_Duplicate( p_sys->titles[i] );
                }
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_SET_TITLE:
            /* TODO handle editions as titles */
            i_idx = (int)va_arg( args, int );
            if( i_idx < p_sys->used_segments.size() )
            {
                p_sys->JumpTo( *p_sys->used_segments[i_idx], NULL );
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_SET_SEEKPOINT:
            i_skp = (int)va_arg( args, int );

            // TODO change the way it works with the << & >> buttons on the UI (+1/-1 instead of a number)
            if( p_sys->titles.size() && i_skp < p_sys->titles[p_sys->i_current_title]->i_seekpoint)
            {
                Seek( p_demux, (int64_t)p_sys->titles[p_sys->i_current_title]->seekpoint[i_skp]->i_time_offset, -1, NULL);
                p_demux->info.i_seekpoint |= INPUT_UPDATE_SEEKPOINT;
                p_demux->info.i_seekpoint = i_skp;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_FPS:
            pf = (double *)va_arg( args, double * );
            *pf = 0.0;
            if( p_sys->p_current_segment && p_sys->p_current_segment->Segment() )
            {
                const matroska_segment_c *p_segment = p_sys->p_current_segment->Segment();
                for( size_t i = 0; i < p_segment->tracks.size(); i++ )
                {
                    mkv_track_t *tk = p_segment->tracks[i];
                    if( tk->fmt.i_cat == VIDEO_ES && tk->fmt.video.i_frame_rate_base > 0 )
                    {
                        *pf = (double)tk->fmt.video.i_frame_rate / tk->fmt.video.i_frame_rate_base;
                        break;
                    }
                }
            }
            return VLC_SUCCESS;

        case DEMUX_SET_TIME:
        default:
            return VLC_EGENERIC;
    }
}

/* Seek */
static void Seek( demux_t *p_demux, mtime_t i_date, double f_percent, chapter_item_c *psz_chapter )
{
    demux_sys_t        *p_sys = p_demux->p_sys;
    virtual_segment_c  *p_vsegment = p_sys->p_current_segment;
    matroska_segment_c *p_segment = p_vsegment->Segment();
    mtime_t            i_time_offset = 0;
    int64_t            i_global_position = -1;

    int         i_index;

    msg_Dbg( p_demux, "seek request to %"PRId64" (%f%%)", i_date, f_percent );
    if( i_date < 0 && f_percent < 0 )
    {
        msg_Warn( p_demux, "cannot seek nowhere !" );
        return;
    }
    if( f_percent > 1.0 )
    {
        msg_Warn( p_demux, "cannot seek so far !" );
        return;
    }

    /* seek without index or without date */
    if( f_percent >= 0 && (var_InheritInteger( p_demux, "mkv-seek-percent" ) || !p_segment->b_cues || i_date < 0 ))
    {
        if( p_sys->f_duration >= 0 && p_segment->b_cues )
        {
            i_date = int64_t( f_percent * p_sys->f_duration * 1000.0 );
        }
        else
        {
            int64_t i_pos = int64_t( f_percent * stream_Size( p_demux->s ) );

            msg_Dbg( p_demux, "inaccurate way of seeking for pos:%"PRId64, i_pos );
            for( i_index = 0; i_index < p_segment->i_index; i_index++ )
            {
                if( p_segment->b_cues && p_segment->p_indexes[i_index].i_position < i_pos )
                    break;
                if( !p_segment->b_cues && p_segment->p_indexes[i_index].i_position >= i_pos && p_segment->p_indexes[i_index].i_time > 0 )
                    break;
            }
            if( i_index == p_segment->i_index )
            {
                i_index--;
            }

            i_date = p_segment->p_indexes[i_index].i_time;

            if( !p_segment->b_cues && ( p_segment->p_indexes[i_index].i_position < i_pos || p_segment->p_indexes[i_index].i_position - i_pos > 2000000 ))
            {
                msg_Dbg( p_demux, "no cues, seek request to global pos: %"PRId64, i_pos );
                i_global_position = i_pos;
            }
        }
    }

    p_vsegment->Seek( *p_demux, i_date, i_time_offset, psz_chapter, i_global_position );
}

/* Utility function for BlockDecode */
static block_t *MemToBlock( demux_t *p_demux, uint8_t *p_mem, size_t i_mem, size_t offset)
{
    block_t *p_block;
    if( !(p_block = block_New( p_demux, i_mem + offset ) ) ) return NULL;
    memcpy( p_block->p_buffer + offset, p_mem, i_mem );
    //p_block->i_rate = p_input->stream.control.i_rate;
    return p_block;
}

/* Needed by matroska_segment::Seek() */
static void BlockDecode( demux_t *p_demux, KaxBlock *block, KaxSimpleBlock *simpleblock,
                         mtime_t i_pts, mtime_t i_duration, bool f_mandatory )
{
    demux_sys_t        *p_sys = p_demux->p_sys;
    matroska_segment_c *p_segment = p_sys->p_current_segment->Segment();

    size_t          i_track;
    unsigned int    i;
    bool            b;

    if( p_segment->BlockFindTrackIndex( &i_track, block, simpleblock ) )
    {
        msg_Err( p_demux, "invalid track number" );
        return;
    }

    mkv_track_t *tk = p_segment->tracks[i_track];

    if( tk->fmt.i_cat != NAV_ES && tk->p_es == NULL )
    {
        msg_Err( p_demux, "unknown track number" );
        return;
    }
    if( i_pts + i_duration < p_sys->i_start_pts && tk->fmt.i_cat == AUDIO_ES )
    {
        return; /* discard audio packets that shouldn't be rendered */
    }

    if ( tk->fmt.i_cat != NAV_ES )
    {
        es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE, tk->p_es, &b );

        if( !b )
        {
            tk->b_inited = false;
            return;
        }
    }


    /* First send init data */
    if( !tk->b_inited && tk->i_data_init > 0 )
    {
        block_t *p_init;

        msg_Dbg( p_demux, "sending header (%d bytes)", tk->i_data_init );
        p_init = MemToBlock( p_demux, tk->p_data_init, tk->i_data_init, 0 );
        if( p_init ) es_out_Send( p_demux->out, tk->p_es, p_init );
    }
    tk->b_inited = true;


    for( i = 0;
         (block != NULL && i < block->NumberFrames()) || (simpleblock != NULL && i < simpleblock->NumberFrames());
         i++ )
    {
        block_t *p_block;
        DataBuffer *data;
        if( simpleblock != NULL )
        {
            data = &simpleblock->GetBuffer(i);
            // condition when the DTS is correct (keyframe or B frame == NOT P frame)
            f_mandatory = simpleblock->IsDiscardable() || simpleblock->IsKeyframe();
        }
        else
        {
            data = &block->GetBuffer(i);
        }
        if( !data->Buffer() || data->Size() > SIZE_MAX )
            break;

        if( tk->i_compression_type == MATROSKA_COMPRESSION_HEADER && tk->p_compression_data != NULL )
            p_block = MemToBlock( p_demux, data->Buffer(), data->Size(), tk->p_compression_data->GetSize() );
        else
            p_block = MemToBlock( p_demux, data->Buffer(), data->Size(), 0 );

        if( p_block == NULL )
        {
            break;
        }

#if defined(HAVE_ZLIB_H)
        if( tk->i_compression_type == MATROSKA_COMPRESSION_ZLIB )
        {
            p_block = block_zlib_decompress( VLC_OBJECT(p_demux), p_block );
            if( p_block == NULL )
                break;
        }
        else
#endif
        if( tk->i_compression_type == MATROSKA_COMPRESSION_HEADER )
        {
            memcpy( p_block->p_buffer, tk->p_compression_data->GetBuffer(), tk->p_compression_data->GetSize() );
        }

        if ( tk->fmt.i_cat == NAV_ES )
        {
            // TODO handle the start/stop times of this packet
            if ( p_sys->b_ui_hooked )
            {
                vlc_mutex_lock( &p_sys->p_ev->lock );
                memcpy( &p_sys->pci_packet, &p_block->p_buffer[1], sizeof(pci_t) );
                p_sys->SwapButtons();
                p_sys->b_pci_packet_set = true;
                vlc_mutex_unlock( &p_sys->p_ev->lock );
                block_Release( p_block );
            }
            return;
        }
        // correct timestamping when B frames are used
        if( tk->fmt.i_cat != VIDEO_ES )
        {
            p_block->i_dts = p_block->i_pts = i_pts;
        }
        else
        {
            if( tk->b_dts_only )
            {
                p_block->i_pts = VLC_TS_INVALID;
                p_block->i_dts = i_pts;
            }
            else
            {
                p_block->i_pts = i_pts;
                if ( f_mandatory )
                    p_block->i_dts = p_block->i_pts;
                else
                    p_block->i_dts = min( i_pts, tk->i_last_dts + (mtime_t)(tk->i_default_duration >> 10));
                p_sys->i_pts = p_block->i_dts;
            }
        }
        tk->i_last_dts = p_block->i_dts;

#if 0
msg_Dbg( p_demux, "block i_dts: %"PRId64" / i_pts: %"PRId64, p_block->i_dts, p_block->i_pts);
#endif
        if( strcmp( tk->psz_codec, "S_VOBSUB" ) )
        {
            p_block->i_length = i_duration * 1000;
        }

        /* FIXME remove when VLC_TS_INVALID work is done */
        if( i == 0 || p_block->i_dts > VLC_TS_INVALID )
            p_block->i_dts += VLC_TS_0;
        if( !tk->b_dts_only && ( i == 0 || p_block->i_pts > VLC_TS_INVALID ) )
            p_block->i_pts += VLC_TS_0;

        es_out_Send( p_demux->out, tk->p_es, p_block );

        /* use time stamp only for first block */
        i_pts = VLC_TS_INVALID;
    }
}


void matroska_segment_c::Seek( mtime_t i_date, mtime_t i_time_offset, int64_t i_global_position )
{
    KaxBlock    *block;
    KaxSimpleBlock *simpleblock;
    int         i_track_skipping;
    int64_t     i_block_duration;
    int64_t     i_block_ref1;
    int64_t     i_block_ref2;
    size_t      i_track;
    int64_t     i_seek_position = i_start_pos;
    int64_t     i_seek_time = i_start_time;

    if( i_global_position >= 0 )
    {
        /* Special case for seeking in files with no cues */
        EbmlElement *el = NULL;
        es.I_O().setFilePointer( i_start_pos, seek_beginning );
        delete ep;
        ep = new EbmlParser( &es, segment, &sys.demuxer );
        cluster = NULL;

        while( ( el = ep->Get() ) != NULL )
        {
            if( MKV_IS_ID( el, KaxCluster ) )
            {
                cluster = (KaxCluster *)el;
                i_cluster_pos = cluster->GetElementPosition();
                if( i_index == 0 ||
                        ( i_index > 0 && p_indexes[i_index - 1].i_position < (int64_t)cluster->GetElementPosition() ) )
                {
                    IndexAppendCluster( cluster );
                }
                if( es.I_O().getFilePointer() >= i_global_position )
                {
                    ParseCluster();
                    msg_Dbg( &sys.demuxer, "we found a cluster that is in the neighbourhood" );
                    return;
                }
            }
        }
        msg_Err( &sys.demuxer, "This file has no cues, and we were unable to seek to the requested position by parsing." );
        return;
    }

    if ( i_index > 0 )
    {
        int i_idx = 0;

        for( ; i_idx < i_index; i_idx++ )
        {
            if( p_indexes[i_idx].i_time + i_time_offset > i_date )
            {
                break;
            }
        }

        if( i_idx > 0 )
        {
            i_idx--;
        }

        i_seek_position = p_indexes[i_idx].i_position;
        i_seek_time = p_indexes[i_idx].i_time;
    }

    msg_Dbg( &sys.demuxer, "seek got %"PRId64" (%d%%)",
                i_seek_time, (int)( 100 * i_seek_position / stream_Size( sys.demuxer.s ) ) );

    es.I_O().setFilePointer( i_seek_position, seek_beginning );

    delete ep;
    ep = new EbmlParser( &es, segment, &sys.demuxer );
    cluster = NULL;

    sys.i_start_pts = i_date;

    /* now parse until key frame */
    i_track_skipping = 0;
    for( i_track = 0; i_track < tracks.size(); i_track++ )
    {
        if( tracks[i_track]->fmt.i_cat == VIDEO_ES )
        {
            tracks[i_track]->b_search_keyframe = true;
            i_track_skipping++;
        }
    }
    es_out_Control( sys.demuxer.out, ES_OUT_SET_NEXT_DISPLAY_TIME, i_date );

    while( i_track_skipping > 0 )
    {
        if( BlockGet( block, simpleblock, &i_block_ref1, &i_block_ref2, &i_block_duration ) )
        {
            msg_Warn( &sys.demuxer, "cannot get block EOF?" );

            return;
        }
        ep->Down();

        for( i_track = 0; i_track < tracks.size(); i_track++ )
        {
            if( (simpleblock && tracks[i_track]->i_number == simpleblock->TrackNum()) ||
                (block && tracks[i_track]->i_number == block->TrackNum()) )
            {
                break;
            }
        }

        if( simpleblock )
            sys.i_pts = (sys.i_chapter_time + simpleblock->GlobalTimecode()) / (mtime_t) 1000;
        else
            sys.i_pts = (sys.i_chapter_time + block->GlobalTimecode()) / (mtime_t) 1000;

        if( i_track < tracks.size() )
        {
            if( sys.i_pts >= sys.i_start_pts )
            {
                cluster = static_cast<KaxCluster*>(ep->UnGet( i_block_pos, i_cluster_pos ));
                i_track_skipping = 0;
            }
            else if( tracks[i_track]->fmt.i_cat == VIDEO_ES )
            {
                if( i_block_ref1 == 0 && tracks[i_track]->b_search_keyframe )
                {
                    tracks[i_track]->b_search_keyframe = false;
                    i_track_skipping--;
                }
                if( !tracks[i_track]->b_search_keyframe )
                {
                    BlockDecode( &sys.demuxer, block, simpleblock, sys.i_pts, 0, i_block_ref1 >= 0 || i_block_ref2 > 0 );
                }
            }
        }

        delete block;
    }

    /* FIXME current ES_OUT_SET_NEXT_DISPLAY_TIME does not work that well if
     * the delay is too high. */
    if( sys.i_pts + 500*1000 < sys.i_start_pts )
    {
        sys.i_start_pts = sys.i_pts;

        es_out_Control( sys.demuxer.out, ES_OUT_SET_NEXT_DISPLAY_TIME, sys.i_start_pts );
    }
}


/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux)
{
    demux_sys_t        *p_sys = p_demux->p_sys;

    vlc_mutex_lock( &p_sys->lock_demuxer );

    virtual_segment_c  *p_vsegment = p_sys->p_current_segment;
    matroska_segment_c *p_segment = p_vsegment->Segment();
    if ( p_segment == NULL ) return 0;
    int                i_block_count = 0;
    int                i_return = 0;

    for( ;; )
    {
        if ( p_sys->demuxer.b_die )
            break;

        if( p_sys->i_pts >= p_sys->i_start_pts  )
            if ( p_vsegment->UpdateCurrentToChapter( *p_demux ) )
            {
                i_return = 1;
                break;
            }
 
        if ( p_vsegment->Edition() && p_vsegment->Edition()->b_ordered && p_vsegment->CurrentChapter() == NULL )
        {
            /* nothing left to read in this ordered edition */
            if ( !p_vsegment->SelectNext() )
                break;
            p_segment->UnSelect( );
 
            es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

            /* switch to the next segment */
            p_segment = p_vsegment->Segment();
            if ( !p_segment->Select( 0 ) )
            {
                msg_Err( p_demux, "Failed to select new segment" );
                break;
            }
            continue;
        }

        KaxBlock *block;
        KaxSimpleBlock *simpleblock;
        int64_t i_block_duration = 0;
        int64_t i_block_ref1;
        int64_t i_block_ref2;

        if( p_segment->BlockGet( block, simpleblock, &i_block_ref1, &i_block_ref2, &i_block_duration ) )
        {
            if ( p_vsegment->Edition() && p_vsegment->Edition()->b_ordered )
            {
                const chapter_item_c *p_chap = p_vsegment->CurrentChapter();
                // check if there are more chapters to read
                if ( p_chap != NULL )
                {
                    /* TODO handle successive chapters with the same user_start_time/user_end_time
                    if ( p_chap->i_user_start_time == p_chap->i_user_start_time )
                        p_vsegment->SelectNext();
                    */
                    p_sys->i_pts = p_chap->i_user_end_time;
                    p_sys->i_pts++; // trick to avoid staying on segments with no duration and no content

                    i_return = 1;
                }

                break;
            }
            else
            {
                msg_Warn( p_demux, "cannot get block EOF?" );
                p_segment->UnSelect( );
 
                es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

                /* switch to the next segment */
                if ( !p_vsegment->SelectNext() )
                    // no more segments in this stream
                    break;
                p_segment = p_vsegment->Segment();
                if ( !p_segment->Select( 0 ) )
                {
                    msg_Err( p_demux, "Failed to select new segment" );
                    break;
                }

                continue;
            }
        }

        if( simpleblock != NULL )
            p_sys->i_pts = (p_sys->i_chapter_time + simpleblock->GlobalTimecode()) / (mtime_t) 1000;
        else
            p_sys->i_pts = (p_sys->i_chapter_time + block->GlobalTimecode()) / (mtime_t) 1000;

        /* */
        es_out_Control( p_demux->out, ES_OUT_SET_PCR, VLC_TS_0 + p_sys->i_pts );

        if( p_sys->i_pts >= p_sys->i_start_pts  )
        {
            if ( p_vsegment->UpdateCurrentToChapter( *p_demux ) )
            {
                i_return = 1;
                delete block;
                break;
            }
        }
 
        if ( p_vsegment->Edition() && p_vsegment->Edition()->b_ordered && p_vsegment->CurrentChapter() == NULL )
        {
            /* nothing left to read in this ordered edition */
            if ( !p_vsegment->SelectNext() )
            {
                delete block;
                break;
            }
            p_segment->UnSelect( );
 
            es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

            /* switch to the next segment */
            p_segment = p_vsegment->Segment();
            if ( !p_segment->Select( 0 ) )
            {
                msg_Err( p_demux, "Failed to select new segment" );
                delete block;
                break;
            }
            delete block;
            continue;
        }

        BlockDecode( p_demux, block, simpleblock, p_sys->i_pts, i_block_duration, i_block_ref1 >= 0 || i_block_ref2 > 0 );

        delete block;
        i_block_count++;

        // TODO optimize when there is need to leave or when seeking has been called
        if( i_block_count > 5 )
        {
            i_return = 1;
            break;
        }
    }

    vlc_mutex_unlock( &p_sys->lock_demuxer );

    return i_return;
}

