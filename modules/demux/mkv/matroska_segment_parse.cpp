/*****************************************************************************
 * matroska_segment_parse.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2010 VLC authors and VideoLAN
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
#include "matroska_segment.hpp"
#include "chapters.hpp"
#include "demux.hpp"
#include "Ebml_parser.hpp"
#include "Ebml_dispatcher.hpp"
#include "string_dispatcher.hpp"
#include "util.hpp"

extern "C" {
#include "../vobsub.h"
#include "../xiph.h"
#include "../windows_audio_commons.h"
#include "../mp4/libmp4.h"
}

#include "../../packetizer/iso_color_tables.h"

#include <vlc_codecs.h>
#include <stdexcept>
#include <limits>
#include <algorithm>

namespace mkv {

/* GetFourCC helper */
#define GetFOURCC( p )  __GetFOURCC( (uint8_t*)p )
static vlc_fourcc_t __GetFOURCC( uint8_t *p )
{
    return VLC_FOURCC( p[0], p[1], p[2], p[3] );
}

static inline void fill_extra_data_alac( mkv_track_t *p_tk )
{
    if( p_tk->i_extra_data <= 0 ) return;
    p_tk->fmt.p_extra = malloc( p_tk->i_extra_data + 12 );
    if( unlikely( !p_tk->fmt.p_extra ) ) return;
    p_tk->fmt.i_extra = p_tk->i_extra_data + 12;
    uint8_t *p_extra = static_cast<uint8_t*>( p_tk->fmt.p_extra );
    /* See "ALAC Specific Info (36 bytes) (required)" from
       alac.macosforge.org/trac/browser/trunk/ALACMagicCookieDescription.txt */
    SetDWBE( p_extra, p_tk->fmt.i_extra );
    memcpy( p_extra + 4, "alac", 4 );
    SetDWBE( p_extra + 8, 0 );
    memcpy( p_extra + 12, p_tk->p_extra_data, p_tk->fmt.i_extra - 12 );
}

static inline void fill_extra_data( mkv_track_t *p_tk, unsigned int offset )
{
    if(p_tk->i_extra_data <= offset) return;
    p_tk->fmt.i_extra = p_tk->i_extra_data - offset;
    p_tk->fmt.p_extra = xmalloc( p_tk->fmt.i_extra );
    if(!p_tk->fmt.p_extra) { p_tk->fmt.i_extra = 0; return; };
    memcpy( p_tk->fmt.p_extra, p_tk->p_extra_data + offset, p_tk->fmt.i_extra );
}

/*****************************************************************************
 * Some functions to manipulate memory
 *****************************************************************************/
static inline char * ToUTF8( const UTFstring &u )
{
    return strdup( u.GetUTF8().c_str() );
}

/*****************************************************************************
 * ParseSeekHead:
 *****************************************************************************/
void matroska_segment_c::ParseSeekHead( KaxSeekHead *seekhead )
{
    EbmlElement *l;

    i_seekhead_count++;

    if( !sys.b_seekable )
        return;

    EbmlParser eparser ( &es, seekhead, &sys.demuxer );

    while( ( l = eparser.Get() ) != NULL )
    {
        if( MKV_IS_ID( l, KaxSeek ) )
        {
            EbmlId id = EBML_ID(EbmlVoid);
            int64_t i_pos = -1;

#ifdef MKV_DEBUG
            msg_Dbg( &sys.demuxer, "|   |   + Seek" );
#endif
            eparser.Down();
            try
            {
                while( ( l = eparser.Get() ) != NULL )
                {
                    if( unlikely( !l->ValidateSize() ) )
                    {
                        msg_Err( &sys.demuxer,"%s too big... skipping it",  typeid(*l).name() );
                        continue;
                    }
                    if( MKV_IS_ID( l, KaxSeekID ) )
                    {
                        KaxSeekID &sid = *static_cast<KaxSeekID*>( l );
                        sid.ReadData( es.I_O() );
                        id = EbmlId( sid.GetBuffer(), sid.GetSize() );
                    }
                    else if( MKV_IS_ID( l, KaxSeekPosition ) )
                    {
                        KaxSeekPosition &spos = *static_cast<KaxSeekPosition*>( l );
                        spos.ReadData( es.I_O() );
                        i_pos = (int64_t)segment->GetGlobalPosition( static_cast<uint64>( spos ) );
                    }
                    else if ( !MKV_IS_ID( l, EbmlVoid ) && !MKV_IS_ID( l, EbmlCrc32 ))
                    {
                        /* Many mkvmerge files hit this case. It seems to be a broken SeekHead */
                        msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid(*l).name() );
                    }
                }
            }
            catch(...)
            {
                msg_Err( &sys.demuxer,"Error while reading %s",  typeid(*l).name() );
            }
            eparser.Up();

            if( i_pos >= 0 )
            {
                if( id == EBML_ID(KaxCluster) )
                {
                    _seeker.add_cluster_position( i_pos );
                }
                else if( id == EBML_ID(KaxCues) )
                {
                    msg_Dbg( &sys.demuxer, "|   - cues at %" PRId64, i_pos );
                    LoadSeekHeadItem( EBML_INFO(KaxCues), i_pos );
                }
                else if( id == EBML_ID(KaxInfo) )
                {
                    msg_Dbg( &sys.demuxer, "|   - info at %" PRId64, i_pos );
                    LoadSeekHeadItem( EBML_INFO(KaxInfo), i_pos );
                }
                else if( id == EBML_ID(KaxChapters) )
                {
                    msg_Dbg( &sys.demuxer, "|   - chapters at %" PRId64, i_pos );
                    LoadSeekHeadItem( EBML_INFO(KaxChapters), i_pos );
                }
                else if( id == EBML_ID(KaxTags) )
                {
                    msg_Dbg( &sys.demuxer, "|   - tags at %" PRId64, i_pos );
                    LoadSeekHeadItem( EBML_INFO(KaxTags), i_pos );
                }
                else if( id == EBML_ID(KaxSeekHead) )
                {
                    msg_Dbg( &sys.demuxer, "|   - chained seekhead at %" PRId64, i_pos );
                    LoadSeekHeadItem( EBML_INFO(KaxSeekHead), i_pos );
                }
                else if( id == EBML_ID(KaxTracks) )
                {
                    msg_Dbg( &sys.demuxer, "|   - tracks at %" PRId64, i_pos );
                    LoadSeekHeadItem( EBML_INFO(KaxTracks), i_pos );
                }
                else if( id == EBML_ID(KaxAttachments) )
                {
                    msg_Dbg( &sys.demuxer, "|   - attachments at %" PRId64, i_pos );
                    LoadSeekHeadItem( EBML_INFO(KaxAttachments), i_pos );
                }
#ifdef MKV_DEBUG
                else if( id != EBML_ID(KaxCluster) && id != EBML_ID(EbmlVoid) &&
                         id != EBML_ID(EbmlCrc32))
                    msg_Dbg( &sys.demuxer, "|   - unknown seekhead reference at %" PRId64, i_pos );
#endif
            }
        }
        else if ( !MKV_IS_ID( l, EbmlVoid ) && !MKV_IS_ID( l, EbmlCrc32 ))
            msg_Dbg( &sys.demuxer, "|   |   + ParseSeekHead Unknown (%s)", typeid(*l).name() );
    }
}


/*****************************************************************************
 * ParseTrackEntry:
 *****************************************************************************/
#define ONLY_FMT(t) if(vars.tk->fmt.i_cat != t ## _ES) return
void matroska_segment_c::ParseTrackEntry( const KaxTrackEntry *m )
{
    bool bSupported = true;

    EbmlUInteger *pTrackType = static_cast<EbmlUInteger*>(m->FindElt(EBML_INFO(KaxTrackType)));
    uint8 ttype;
    if (likely(pTrackType != NULL))
        ttype = (uint8) *pTrackType;
    else
        ttype = 0;

    enum es_format_category_e es_cat;
    switch( ttype )
    {
        case track_audio:
            es_cat = AUDIO_ES;
            break;
        case track_video:
            es_cat = VIDEO_ES;
            break;
        case track_subtitle:
        case track_buttons:
            es_cat = SPU_ES;
            break;
        default:
            es_cat = UNKNOWN_ES;
            break;
    }

    /* Init the track */
    mkv_track_t *p_track = new mkv_track_t(es_cat);

    MkvTree( sys.demuxer, 2, "Track Entry" );

    struct MetaDataCapture {
      matroska_segment_c * obj;
      mkv_track_t        * tk;
      demux_t            * p_demuxer;
      bool&                bSupported;
      int                  level;
      struct {
        unsigned int i_crop_right;
        unsigned int i_crop_left;
        unsigned int i_crop_top;
        unsigned int i_crop_bottom;
        unsigned int i_display_unit;
        unsigned int i_display_width;
        unsigned int i_display_height;

        unsigned int chroma_sit_vertical;
        unsigned int chroma_sit_horizontal;
      } track_video_info;

    } metadata_payload = {
      this, p_track, &sys.demuxer, bSupported, 3, { }
    };

    MKV_SWITCH_CREATE( EbmlTypeDispatcher, MetaDataHandlers, MetaDataCapture )
    {
        MKV_SWITCH_INIT();

        static void debug (MetaDataCapture const& vars, char const * fmt, ...)
        {
            va_list args; va_start( args, fmt );
            MkvTree_va( *vars.p_demuxer, vars.level, fmt, args);
            va_end( args );
        }
        E_CASE( KaxTrackNumber, tnum )
        {
            vars.tk->i_number = static_cast<uint32>( tnum );
            debug( vars, "Track Number=%u", vars.tk->i_number );
        }
        E_CASE( KaxTrackUID, tuid )
        {
            debug( vars, "Track UID=%x", static_cast<uint32>( tuid ) );
        }
        E_CASE( KaxTrackType, ttype )
        {
            const char *psz_type;

            switch( static_cast<uint8>( ttype ) )
            {
                case track_audio:
                    psz_type = "audio";
                    break;
                case track_video:
                    psz_type = "video";
                    break;
                case track_subtitle:
                    psz_type = "subtitle";
                    break;
                case track_buttons:
                    psz_type = "buttons";
                    break;
                default:
                    psz_type = "unknown";
                    break;
            }

            debug( vars, "Track Type=%s", psz_type ) ;
        }
        E_CASE( KaxTrackFlagEnabled, fenb ) // UNUSED
        {
            vars.tk->b_enabled = static_cast<uint32>( fenb );
            debug( vars, "Track Enabled=%u", vars.tk->b_enabled );
        }
        E_CASE( KaxTrackFlagDefault, fdef )
        {
            vars.tk->b_default = static_cast<uint32>( fdef );
            debug( vars, "Track Default=%u", vars.tk->b_default );
        }
        E_CASE( KaxTrackFlagForced, ffor ) // UNUSED
        {
            vars.tk->b_forced = static_cast<uint32>( ffor );

            debug( vars, "Track Forced=%u", vars.tk->b_forced );
        }
        E_CASE( KaxTrackFlagLacing, lac ) // UNUSED
        {
            debug( vars, "Track Lacing=%d", static_cast<uint32>( lac ) ) ;
        }
        E_CASE( KaxTrackMinCache, cmin ) // UNUSED
        {
            debug( vars, "Track MinCache=%d", static_cast<uint32>( cmin ) ) ;
        }
        E_CASE( KaxTrackMaxCache, cmax ) // UNUSED
        {
            debug( vars, "Track MaxCache=%d", static_cast<uint32>( cmax ) ) ;
        }
        E_CASE( KaxTrackDefaultDuration, defd )
        {
            vars.tk->i_default_duration = VLC_TICK_FROM_NS(static_cast<uint64>(defd));
            debug( vars, "Track Default Duration=%" PRId64, vars.tk->i_default_duration );
        }
        E_CASE( KaxTrackTimecodeScale, ttcs )
        {
            vars.tk->f_timecodescale = static_cast<float>( ttcs );
            if ( vars.tk->f_timecodescale <= 0 ) vars.tk->f_timecodescale = 1.0;
            debug( vars, "Track TimeCodeScale=%f", vars.tk->f_timecodescale ) ;
        }
        E_CASE( KaxMaxBlockAdditionID, mbl ) // UNUSED
        {
            debug( vars, "Track Max BlockAdditionID=%d", static_cast<uint32>( mbl ) ) ;
        }
        E_CASE( KaxTrackName, tname )
        {
            vars.tk->fmt.psz_description = ToUTF8( UTFstring( tname ) );
            debug( vars, "Track Name=%s", vars.tk->fmt.psz_description ? vars.tk->fmt.psz_description : "(null)" );
        }
        E_CASE( KaxTrackLanguage, lang )
        {
            free( vars.tk->fmt.psz_language );
            const std::string slang ( lang );
            size_t pos = slang.find_first_of( '-' );
            vars.tk->fmt.psz_language = pos != std::string::npos ? strndup( slang.c_str (), pos ) : strdup( slang.c_str() );
            debug( vars, "Track Language=`%s'", vars.tk->fmt.psz_language ? vars.tk->fmt.psz_language : "(null)" );
        }
        E_CASE( KaxCodecID, codecid )
        {
            vars.tk->codec = std::string( codecid );
            debug( vars, "Track CodecId=%s", std::string( codecid ).c_str() ) ;
        }
        E_CASE( KaxCodecPrivate, cpriv )
        {
            vars.tk->i_extra_data = cpriv.GetSize();
            if( vars.tk->i_extra_data > 0 )
            {
                vars.tk->p_extra_data = static_cast<uint8_t*>( malloc( vars.tk->i_extra_data ) );

                if( likely( vars.tk->p_extra_data ) )
                    memcpy( vars.tk->p_extra_data, cpriv.GetBuffer(),
                            vars.tk->i_extra_data );
            }
            debug( vars, "Track CodecPrivate size=%" PRId64, cpriv.GetSize() );
        }
        E_CASE( KaxCodecName, cname )
        {
            vars.tk->str_codec_name = static_cast<UTFstring const&>( cname ).GetUTF8();
            debug( vars, "Track Codec Name=%s", vars.tk->str_codec_name.c_str() ) ;
        }
        //AttachmentLink
        E_CASE( KaxCodecDecodeAll, cdall ) // UNUSED
        {
            debug( vars, "Track Codec Decode All=%u", static_cast<uint8>( cdall ) ) ;
        }
        E_CASE( KaxTrackOverlay, tovr ) // UNUSED
        {
            debug( vars, "Track Overlay=%u", static_cast<uint32>( tovr ) ) ;
        }
#if LIBMATROSKA_VERSION >= 0x010401
        E_CASE( KaxCodecDelay, codecdelay )
        {
            vars.tk->i_codec_delay = VLC_TICK_FROM_NS(static_cast<uint64_t>( codecdelay ));
            msg_Dbg( vars.p_demuxer, "|   |   |   + Track Codec Delay =%" PRIu64,
                     vars.tk->i_codec_delay );
        }
        E_CASE( KaxSeekPreRoll, spr )
        {
            vars.tk->i_seek_preroll = VLC_TICK_FROM_NS(static_cast<uint64_t>( spr ));
            debug( vars, "Track Seek Preroll =%" PRIu64, vars.tk->i_seek_preroll );
        }
#endif
        E_CASE( KaxContentEncodings, cencs )
        {
            debug( vars, "Content Encodings" );

            if ( cencs.ListSize () > 1 )
            {
                msg_Err( vars.p_demuxer, "Multiple Compression method not supported" );
                vars.bSupported = false;
            }

            vars.level += 1;
            dispatcher.iterate( cencs.begin(), cencs.end(), &vars );
            vars.level -= 1;
        }
        E_CASE( KaxContentEncoding, cenc )
        {
            debug( vars, "Content Encoding" );

            vars.level += 1;
            dispatcher.iterate( cenc.begin(), cenc.end(), &vars );
            vars.level -= 1;
        }
        E_CASE( KaxContentEncodingOrder, encord )
        {
            debug( vars, "Order: %i", static_cast<uint32>( encord ) );
        }
        E_CASE( KaxContentEncodingScope, encscope )
        {
            vars.tk->i_encoding_scope = static_cast<uint32>( encscope );
            debug( vars, "Scope: %i", vars.tk->i_encoding_scope );
        }
        E_CASE( KaxContentEncodingType, enctype )
        {
            debug( vars, "Type: %i", static_cast<uint32>( enctype ) );
        }
        E_CASE( KaxContentCompression, compr )
        {
            debug( vars, "Content Compression" );
            //Default compression type is 0 (Zlib)
            vars.tk->i_compression_type = MATROSKA_COMPRESSION_ZLIB;

            vars.level += 1;
            dispatcher.iterate( compr.begin(), compr.end(), &vars );
            vars.level -= 1;
        }
        E_CASE( KaxContentCompAlgo, compalg )
        {
            vars.tk->i_compression_type = static_cast<uint32>( compalg );
            debug( vars, "Compression Algorithm: %i", vars.tk->i_compression_type );
            if ( ( vars.tk->i_compression_type != MATROSKA_COMPRESSION_ZLIB ) &&
                 ( vars.tk->i_compression_type != MATROSKA_COMPRESSION_HEADER ) )
            {
                msg_Err( vars.p_demuxer, "Track Compression method %d not supported", vars.tk->i_compression_type );
                vars.bSupported = false;
            }
        }
        E_CASE( KaxContentCompSettings, kccs )
        {
            vars.tk->p_compression_data = new KaxContentCompSettings( kccs );
        }
        E_CASE( KaxTrackVideo, tkv )
        {
            ONLY_FMT(VIDEO);
            debug( vars, "Track Video");

            mkv_track_t *tk = vars.tk;

            tk->f_fps = 0.0;

            if( tk->i_default_duration > 1000 ) /* Broken ffmpeg mux info when non set fps */
            {
                tk->fmt.video.i_frame_rate_base = static_cast<unsigned>( tk->i_default_duration );
                tk->fmt.video.i_frame_rate = 1000000;
            }

            vars.level += 1;
            dispatcher.iterate (tkv.begin (), tkv.end (), &vars );
            vars.level -= 1;

            unsigned int i_crop_top    = vars.track_video_info.i_crop_top;
            unsigned int i_crop_right  = vars.track_video_info.i_crop_right;
            unsigned int i_crop_bottom = vars.track_video_info.i_crop_bottom;
            unsigned int i_crop_left   = vars.track_video_info.i_crop_left;

            unsigned int i_display_unit   = vars.track_video_info.i_display_unit; VLC_UNUSED(i_display_unit);
            unsigned int i_display_width  = vars.track_video_info.i_display_width;
            unsigned int i_display_height = vars.track_video_info.i_display_height;

            if (vars.track_video_info.chroma_sit_vertical || vars.track_video_info.chroma_sit_horizontal)
            {
                switch (vars.track_video_info.chroma_sit_vertical)
                {
                case 0: // unspecified
                case 2: // center
                    if (vars.track_video_info.chroma_sit_horizontal == 1) // left
                        tk->fmt.video.chroma_location = CHROMA_LOCATION_LEFT;
                    else if (vars.track_video_info.chroma_sit_horizontal == 2) // half
                        tk->fmt.video.chroma_location = CHROMA_LOCATION_CENTER;
                    else
                        debug( vars, "unsupported chroma Siting %u/%u",
                               vars.track_video_info.chroma_sit_horizontal,
                               vars.track_video_info.chroma_sit_vertical);
                    break;
                case 1: // top
                    if (vars.track_video_info.chroma_sit_horizontal == 1) // left
                        tk->fmt.video.chroma_location = CHROMA_LOCATION_TOP_LEFT;
                    else if (vars.track_video_info.chroma_sit_horizontal == 2) // half
                        tk->fmt.video.chroma_location = CHROMA_LOCATION_TOP_CENTER;
                    else
                        debug( vars, "unsupported chroma Siting %u/%u",
                               vars.track_video_info.chroma_sit_horizontal,
                               vars.track_video_info.chroma_sit_vertical);
                    break;
                }
            }

            if( i_display_height && i_display_width )
            {
                tk->fmt.video.i_sar_num = i_display_width  * tk->fmt.video.i_height;
                tk->fmt.video.i_sar_den = i_display_height * tk->fmt.video.i_width;
            }

            tk->fmt.video.i_visible_width   = tk->fmt.video.i_width;
            tk->fmt.video.i_visible_height  = tk->fmt.video.i_height;

            if( i_crop_left || i_crop_right || i_crop_top || i_crop_bottom )
            {
                tk->fmt.video.i_x_offset        = i_crop_left;
                tk->fmt.video.i_y_offset        = i_crop_top;
                tk->fmt.video.i_visible_width  -= i_crop_left + i_crop_right;
                tk->fmt.video.i_visible_height -= i_crop_top + i_crop_bottom;
            }
            /* FIXME: i_display_* allows you to not only set DAR, but also a zoom factor.
               we do not support this atm */
        }
#if LIBMATROSKA_VERSION >= 0x010406
        E_CASE( KaxVideoProjection, proj )
        {
            ONLY_FMT(VIDEO);
            debug( vars, "Track Video Projection" ) ;

            vars.level += 1;
            dispatcher.iterate (proj.begin (), proj.end (), &vars );
            vars.level -= 1;
        }
        E_CASE( KaxVideoProjectionType, fint )
        {
            ONLY_FMT(VIDEO);
            switch (static_cast<uint8>( fint ))
            {
            case 0:
                vars.tk->fmt.video.projection_mode = PROJECTION_MODE_RECTANGULAR;
                break;
            case 1:
                vars.tk->fmt.video.projection_mode = PROJECTION_MODE_EQUIRECTANGULAR;
                break;
            case 2:
                vars.tk->fmt.video.projection_mode = PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD;
                break;
            default:
                debug( vars, "Track Video Projection %u not supported", static_cast<uint8>( fint ) ) ;
                break;
            }
        }
        E_CASE( KaxVideoProjectionPoseYaw, pose )
        {
            ONLY_FMT(VIDEO);
            vars.tk->fmt.video.pose.yaw = static_cast<float>( pose );
        }
        E_CASE( KaxVideoProjectionPosePitch, pose )
        {
            ONLY_FMT(VIDEO);
            vars.tk->fmt.video.pose.pitch = static_cast<float>( pose );
        }
        E_CASE( KaxVideoProjectionPoseRoll, pose )
        {
            ONLY_FMT(VIDEO);
            vars.tk->fmt.video.pose.roll = static_cast<float>( pose );
        }
#endif
        E_CASE( KaxVideoFlagInterlaced, fint ) // UNUSED
        {
            ONLY_FMT(VIDEO);
            debug( vars, "Track Video Interlaced=%u", static_cast<uint8>( fint ) ) ;
        }
        E_CASE( KaxVideoStereoMode, stereo ) // UNUSED
        {
            ONLY_FMT(VIDEO);
            vars.tk->fmt.video.b_multiview_right_eye_first = false;
            switch (static_cast<uint8>( stereo ))
            {
            case 0: vars.tk->fmt.video.multiview_mode = MULTIVIEW_2D;         break;
            case 1: vars.tk->fmt.video.multiview_mode = MULTIVIEW_STEREO_SBS; break;
            case 2: vars.tk->fmt.video.multiview_mode = MULTIVIEW_STEREO_TB;  break;
            case 3: vars.tk->fmt.video.multiview_mode = MULTIVIEW_STEREO_TB;
                    vars.tk->fmt.video.b_multiview_right_eye_first = true;    break;
            case 4: vars.tk->fmt.video.multiview_mode = MULTIVIEW_STEREO_CHECKERBOARD;
                    vars.tk->fmt.video.b_multiview_right_eye_first = true;    break;
            case 5: vars.tk->fmt.video.multiview_mode = MULTIVIEW_STEREO_CHECKERBOARD; break;
            case 6: vars.tk->fmt.video.multiview_mode = MULTIVIEW_STEREO_ROW;
                    vars.tk->fmt.video.b_multiview_right_eye_first = true;    break;
            case 7: vars.tk->fmt.video.multiview_mode = MULTIVIEW_STEREO_ROW; break;
            case 8: vars.tk->fmt.video.multiview_mode = MULTIVIEW_STEREO_COL;
                    vars.tk->fmt.video.b_multiview_right_eye_first = true;    break;
            case 9: vars.tk->fmt.video.multiview_mode = MULTIVIEW_STEREO_COL; break;
            case 11: vars.tk->fmt.video.multiview_mode = MULTIVIEW_STEREO_SBS;
                     vars.tk->fmt.video.b_multiview_right_eye_first = true;    break;
            case 13: vars.tk->fmt.video.multiview_mode = MULTIVIEW_STEREO_FRAME; break;
            case 14: vars.tk->fmt.video.multiview_mode = MULTIVIEW_STEREO_FRAME;
                    vars.tk->fmt.video.b_multiview_right_eye_first = true;    break;
            default:
            case 10: case 12:
                debug( vars, " unsupported Stereo Mode=%u", static_cast<uint8>( stereo ) ) ;
            }
            debug( vars, "Track Video Stereo Mode=%u", static_cast<uint8>( stereo ) ) ;
        }
        E_CASE( KaxVideoPixelWidth, vwidth )
        {
            ONLY_FMT(VIDEO);
            vars.tk->fmt.video.i_width += static_cast<uint16>( vwidth );
            debug( vars, "width=%d", vars.tk->fmt.video.i_width );
        }
        E_CASE( KaxVideoPixelHeight, vheight )
        {
            ONLY_FMT(VIDEO);
            vars.tk->fmt.video.i_height += static_cast<uint16>( vheight );
            debug( vars, "height=%d", vars.tk->fmt.video.i_height );
        }
        E_CASE( KaxVideoDisplayWidth, vwidth )
        {
            ONLY_FMT(VIDEO);
            vars.track_video_info.i_display_width = static_cast<uint16>( vwidth );
            debug( vars, "display width=%d", vars.track_video_info.i_display_width );
        }
        E_CASE( KaxVideoDisplayHeight, vheight )
        {
            ONLY_FMT(VIDEO);
            vars.track_video_info.i_display_height = static_cast<uint16>( vheight );
            debug( vars, "display height=%d", vars.track_video_info.i_display_height );
        }
        E_CASE( KaxVideoPixelCropBottom, cropval )
        {
            ONLY_FMT(VIDEO);
            vars.track_video_info.i_crop_bottom = static_cast<uint16>( cropval );
            debug( vars, "crop pixel bottom=%d", vars.track_video_info.i_crop_bottom );
        }
        E_CASE( KaxVideoPixelCropTop, cropval )
        {
            ONLY_FMT(VIDEO);
            vars.track_video_info.i_crop_top = static_cast<uint16>( cropval );
            debug( vars, "crop pixel top=%d", vars.track_video_info.i_crop_top );
        }
        E_CASE( KaxVideoPixelCropRight, cropval )
        {
            ONLY_FMT(VIDEO);
            vars.track_video_info.i_crop_right = static_cast<uint16>( cropval );
            debug( vars, "crop pixel right=%d", vars.track_video_info.i_crop_right );
        }
        E_CASE( KaxVideoPixelCropLeft, cropval )
        {
            ONLY_FMT(VIDEO);
            vars.track_video_info.i_crop_left = static_cast<uint16>( cropval );
            debug( vars, "crop pixel left=%d", vars.track_video_info.i_crop_left );
        }
        E_CASE( KaxVideoDisplayUnit, vdmode )
        {
            ONLY_FMT(VIDEO);
            vars.track_video_info.i_display_unit = static_cast<uint8>( vdmode );
            const char *psz_unit;
            switch (vars.track_video_info.i_display_unit)
            {
            case 0:  psz_unit = "pixels"; break;
            case 1:  psz_unit = "centimeters"; break;
            case 2:  psz_unit = "inches"; break;
            case 3:  psz_unit = "dar"; break;
            default: psz_unit = "unknown"; break;
            }
            debug( vars, "Track Video Display Unit=%s", psz_unit );
        }
        E_CASE( KaxVideoAspectRatio, ratio ) // UNUSED
        {
            ONLY_FMT(VIDEO);
            debug( vars, "Track Video Aspect Ratio Type=%u", static_cast<uint8>( ratio ) ) ;
        }
        E_CASE( KaxVideoFrameRate, vfps )
        {
            ONLY_FMT(VIDEO);
            vars.tk->f_fps = __MAX( static_cast<float>( vfps ), 1 );
            debug( vars, "fps=%f", vars.tk->f_fps );
        }
        E_CASE( KaxVideoColourSpace, colourspace )
        {
            ONLY_FMT(VIDEO);
            if ( colourspace.ValidateSize() )
            {
                char clrspc[5];

                vars.tk->fmt.i_codec = GetFOURCC( colourspace.GetBuffer() );

                vlc_fourcc_to_char( vars.tk->fmt.i_codec, clrspc );
                clrspc[4]  = '\0';
                debug( vars, "Colour Space=%s", clrspc );
            }
        }
#if LIBMATROSKA_VERSION >= 0x010405
        E_CASE( KaxVideoColour, colours)
        {
            ONLY_FMT(VIDEO);
            debug( vars, "Video Colors");
            if (vars.tk->fmt.i_cat != VIDEO_ES ) {
                msg_Err( vars.p_demuxer, "Video colors elements not allowed for this track" );
            } else {
            vars.level += 1;
            dispatcher.iterate (colours.begin (), colours.end (), &vars );
            vars.level -= 1;
            }
        }
        E_CASE( KaxVideoColourRange, range )
        {
            ONLY_FMT(VIDEO);
            const char *name = nullptr;
            switch( static_cast<uint8>(range) )
            {
            case 1:
                vars.tk->fmt.video.color_range = COLOR_RANGE_LIMITED;
                name ="limited";
                break;
            case 2:
                vars.tk->fmt.video.color_range = COLOR_RANGE_FULL;
                name ="full";
                break;
            case 3: // Matrix coefficients + Transfer characteristics
            default:
                debug( vars, "Unsupported Colour Range=%d", static_cast<uint8>(range) );
            }
            if (name != nullptr) debug( vars, "Range=%s", name );
        }
        E_CASE( KaxVideoColourTransferCharacter, tranfer )
        {
            ONLY_FMT(VIDEO);
            vars.tk->fmt.video.transfer = iso_23001_8_tc_to_vlc_xfer( static_cast<uint8>(tranfer) );
            const char *name = nullptr;
            switch( static_cast<uint8>(tranfer) )
            {
            case 1: name = "BT-709";
                break;
            case 4: name = "BT.470BM";
                break;
            case 5: name = "BT.470BG";
                break;
            case 6: name = "SMPTE 170M";
                break;
            case 7: name = "SMPTE 240M";
                break;
            case 8: name = "linear";
                break;
            case 13: name = "sRGB/sYCC";
                break;
            case 16: name = "BT.2100 PQ";
                break;
            case 18: name = "HLG";
                break;
            case 9:  // Log
            case 10: // Log SQRT
            case 11: // IEC 61966-2-4
            case 12: // ITU-R BT.1361 Extended Colour Gamut
            case 14: // ITU-R BT.2020 10 bit
            case 15: // ITU-R BT.2020 12 bit
            case 17: // SMPTE ST 428-1
            default:
                break;
            }
            if (vars.tk->fmt.video.transfer == TRANSFER_FUNC_UNDEF)
                debug( vars, "Unsupported Colour Transfer=%d", static_cast<uint8>(tranfer) );
            else if (name == nullptr)
                debug( vars, "Colour Transfer=%d", static_cast<uint8>(tranfer) );
            else
                debug( vars, "Colour Transfer=%s", name );
        }
        E_CASE( KaxVideoColourPrimaries, primaries )
        {
            ONLY_FMT(VIDEO);
            vars.tk->fmt.video.primaries = iso_23001_8_cp_to_vlc_primaries( static_cast<uint8>(primaries) );
            const char *name = nullptr;
            switch( static_cast<uint8>(primaries) )
            {
            case 1: name = "BT-709";
                break;
            case 4: name = "BT.470M";
                break;
            case 5: name = "BT.470BG";
                break;
            case 6: name = "SMPTE 170M";
                break;
            case 7: name = "SMPTE 240M";
                break;
            case 9: name = "BT.2020";
                break;
            case 8:  // FILM
            case 10: // SMPTE ST 428-1
            case 22: // JEDEC P22 phosphors
            default:
                break;
            }
            if (vars.tk->fmt.video.primaries == COLOR_PRIMARIES_UNDEF)
                debug( vars, "Unsupported Colour Primaries=%d", static_cast<uint8>(primaries) );
            else if (name == nullptr)
                debug( vars, "Colour Primaries=%s", static_cast<uint8>(primaries) );
            else
                debug( vars, "Colour Primaries=%s", name );
        }
        E_CASE( KaxVideoColourMatrix, matrix )
        {
            ONLY_FMT(VIDEO);
            vars.tk->fmt.video.space = iso_23001_8_mc_to_vlc_coeffs( static_cast<uint8>(matrix) );
            const char *name = nullptr;
            switch( static_cast<uint8>(matrix) )
            {
            case 1: name = "BT-709";
                break;
            case 6: name = "SMPTE 170M";
                break;
            case 7: name = "SMPTE 240M";
                break;
            case 9: name = "BT.2020 Non-constant Luminance";
                break;
            case 10: name = "BT.2020 Constant Luminance";
                break;
            case 0: // Identity
            case 2: // unspecified
            case 3: // reserved
            case 4: // US FCC 73.682
            case 5: // ITU-R BT.470BG
            case 8: // YCoCg
            case 11: // SMPTE ST 2085
            case 12: // Chroma-derived Non-constant Luminance
            case 13: // Chroma-derived Constant Luminance
            case 14: // ITU-R BT.2100
            default:
                break;
            }
            if (vars.tk->fmt.video.space == COLOR_SPACE_UNDEF)
                debug( vars, "Unsupported Colour Matrix=%d", static_cast<uint8>(matrix) );
            else if (name == nullptr)
                debug( vars, "Colour Matrix=%d", static_cast<uint8>(matrix) );
            else
                debug( vars, "Colour Matrix=%s", name );
        }
        E_CASE( KaxVideoChromaSitHorz, chroma_hor )
        {
            ONLY_FMT(VIDEO);
            const char *name = nullptr;
            vars.track_video_info.chroma_sit_horizontal = static_cast<uint8>(chroma_hor);
            switch( static_cast<uint8>(chroma_hor) )
            {
            case 0: name = "unspecified"; break;
            case 1: name = "left";        break;
            case 2: name = "center";      break;
            default:
                debug( vars, "Unsupported Horizontal Chroma Siting=%d", static_cast<uint8>(chroma_hor) );
            }
            if (name != nullptr) debug( vars, "Chroma Siting Horizontal=%s", name);
        }
        E_CASE( KaxVideoChromaSitVert, chroma_ver )
        {
            ONLY_FMT(VIDEO);
            const char *name = nullptr;
            vars.track_video_info.chroma_sit_vertical = static_cast<uint8>(chroma_ver);
            switch( static_cast<uint8>(chroma_ver) )
            {
            case 0: name = "unspecified"; break;
            case 1: name = "left";        break;
            case 2: name = "center";      break;
            default:
                debug( vars, "Unsupported Vertical Chroma Siting=%d", static_cast<uint8>(chroma_ver) );
            }
            if (name != nullptr) debug( vars, "Chroma Siting Vertical=%s", name);
        }
        E_CASE( KaxVideoColourMaxCLL, maxCLL )
        {
            ONLY_FMT(VIDEO);
            debug( vars, "Video Max Pixel Brightness");
            vars.tk->fmt.video.lighting.MaxCLL = static_cast<uint16_t>(maxCLL);
        }
        E_CASE( KaxVideoColourMaxFALL, maxFALL )
        {
            ONLY_FMT(VIDEO);
            debug( vars, "Video Max Frame Brightness");
            vars.tk->fmt.video.lighting.MaxFALL = static_cast<uint16_t>(maxFALL);
        }
        E_CASE( KaxVideoColourMasterMeta, mastering )
        {
            ONLY_FMT(VIDEO);
            debug( vars, "Video Mastering Metadata");
            if (vars.tk->fmt.i_cat != VIDEO_ES ) {
                msg_Err( vars.p_demuxer, "Video metadata elements not allowed for this track" );
            } else {
            vars.level += 1;
            dispatcher.iterate (mastering.begin (), mastering.end (), &vars );
            vars.level -= 1;
            }
        }
        E_CASE( KaxVideoLuminanceMax, maxLum )
        {
            ONLY_FMT(VIDEO);
            debug( vars, "Video Luminance Max");
            vars.tk->fmt.video.mastering.max_luminance = static_cast<float>(maxLum) * 10000.f;
        }
        E_CASE( KaxVideoLuminanceMin, minLum )
        {
            ONLY_FMT(VIDEO);
            debug( vars, "Video Luminance Min");
            vars.tk->fmt.video.mastering.min_luminance = static_cast<float>(minLum) * 10000.f;
        }
        E_CASE( KaxVideoGChromaX, chroma )
        {
            ONLY_FMT(VIDEO);
            debug( vars, "Video Green Chroma X");
            vars.tk->fmt.video.mastering.primaries[0] = static_cast<float>(chroma) * 50000.f;
        }
        E_CASE( KaxVideoGChromaY, chroma )
        {
            ONLY_FMT(VIDEO);
            debug( vars, "Video Green Chroma Y");
            vars.tk->fmt.video.mastering.primaries[1] = static_cast<float>(chroma) * 50000.f;
        }
        E_CASE( KaxVideoBChromaX, chroma )
        {
            ONLY_FMT(VIDEO);
            debug( vars, "Video Blue Chroma X");
            vars.tk->fmt.video.mastering.primaries[2] = static_cast<float>(chroma) * 50000.f;
        }
        E_CASE( KaxVideoBChromaY, chroma )
        {
            ONLY_FMT(VIDEO);
            debug( vars, "Video Blue Chroma Y");
            vars.tk->fmt.video.mastering.primaries[3] = static_cast<float>(chroma) * 50000.f;
        }
        E_CASE( KaxVideoRChromaX, chroma )
        {
            ONLY_FMT(VIDEO);
            debug( vars, "Video Red Chroma X");
            vars.tk->fmt.video.mastering.primaries[4] = static_cast<float>(chroma) * 50000.f;
        }
        E_CASE( KaxVideoRChromaY, chroma )
        {
            ONLY_FMT(VIDEO);
            debug( vars, "Video Red Chroma Y");
            vars.tk->fmt.video.mastering.primaries[5] = static_cast<float>(chroma) * 50000.f;
        }
        E_CASE( KaxVideoWhitePointChromaX, white )
        {
            ONLY_FMT(VIDEO);
            debug( vars, "Video WhitePoint X");
            vars.tk->fmt.video.mastering.white_point[0] = static_cast<float>(white) * 50000.f;
        }
        E_CASE( KaxVideoWhitePointChromaY, white )
        {
            ONLY_FMT(VIDEO);
            debug( vars, "Video WhitePoint Y");
            vars.tk->fmt.video.mastering.white_point[1] = static_cast<float>(white) * 50000.f;
        }
#endif
        E_CASE( KaxTrackAudio, tka ) {
            ONLY_FMT(AUDIO);
            debug( vars, "Track Audio");
            vars.level += 1;
            dispatcher.iterate( tka.begin(), tka.end(), &vars );
            vars.level -= 1;
        }
        E_CASE( KaxAudioSamplingFreq, afreq )
        {
            ONLY_FMT(AUDIO);
            float const value = static_cast<float>( afreq );

            vars.tk->i_original_rate  = value;
            vars.tk->fmt.audio.i_rate = value;

            debug( vars, "afreq=%d", vars.tk->fmt.audio.i_rate ) ;
        }
        E_CASE( KaxAudioOutputSamplingFreq, afreq )
        {
            ONLY_FMT(AUDIO);
            vars.tk->fmt.audio.i_rate = static_cast<float>( afreq );
            debug( vars, "aoutfreq=%d", vars.tk->fmt.audio.i_rate ) ;
        }
        E_CASE( KaxAudioChannels, achan )
        {
            ONLY_FMT(AUDIO);
            vars.tk->fmt.audio.i_channels = static_cast<uint8>( achan );
            debug( vars, "achan=%u", vars.tk->fmt.audio.i_channels );
        }
        E_CASE( KaxAudioBitDepth, abits )
        {
            ONLY_FMT(AUDIO);
            vars.tk->fmt.audio.i_bitspersample = static_cast<uint8>( abits );
            debug( vars, "abits=%u", vars.tk->fmt.audio.i_bitspersample);
        }
        E_CASE ( EbmlVoid, ) {
          VLC_UNUSED( vars );
        }
        E_CASE_DEFAULT(element) {
            debug( vars, "Unknown (%s)", typeid(element).name() );
        }
    };

    MetaDataHandlers::Dispatcher().iterate ( m->begin(), m->end(), &metadata_payload );

    if( p_track->i_number == 0 )
    {
        msg_Warn( &sys.demuxer, "Missing KaxTrackNumber, discarding track!" );
        delete p_track;
        return;
    }

    if ( bSupported )
    {
#ifdef HAVE_ZLIB_H
        if( p_track->i_compression_type == MATROSKA_COMPRESSION_ZLIB &&
            p_track->i_encoding_scope & MATROSKA_ENCODING_SCOPE_PRIVATE &&
            p_track->i_extra_data && p_track->p_extra_data &&
            zlib_decompress_extra( &sys.demuxer, *p_track ) )
        {
            msg_Err(&sys.demuxer, "Couldn't handle the track %u compression", p_track->i_number );
            delete p_track;
            return;
        }
#endif
        if( !TrackInit( p_track ) )
        {
            msg_Err(&sys.demuxer, "Couldn't init track %u", p_track->i_number );
            delete p_track;
            return;
        }

        tracks.insert( std::make_pair( p_track->i_number, std::unique_ptr<mkv_track_t>(p_track) ) ); // TODO: add warning if two tracks have the same key
    }
    else
    {
        msg_Err( &sys.demuxer, "Track Entry %u not supported", p_track->i_number );
        delete p_track;
    }
}

#undef ONLY_FMT

/*****************************************************************************
 * ParseTracks:
 *****************************************************************************/
void matroska_segment_c::ParseTracks( KaxTracks *tracks )
{
    EbmlElement *el;
    int i_upper_level = 0;

    /* Master elements */
    if( unlikely( tracks->IsFiniteSize() && tracks->GetSize() >= SIZE_MAX ) )
    {
        msg_Err( &sys.demuxer, "Track too big, aborting" );
        return;
    }
    try
    {
        tracks->Read( es, EBML_CONTEXT(tracks), i_upper_level, el, true );
    }
    catch(...)
    {
        msg_Err( &sys.demuxer, "Couldn't read tracks" );
        return;
    }

    struct Capture {
      matroska_segment_c * obj;
      demux_t            * p_demuxer;

    } payload = {
      this, &sys.demuxer
    };

    MKV_SWITCH_CREATE( EbmlTypeDispatcher, TrackHandlers, struct Capture )
    {
        MKV_SWITCH_INIT();

        E_CASE( KaxTrackEntry, track_number ) {
            vars.obj->ParseTrackEntry( &track_number );
        }
        E_CASE( EbmlVoid, ) {
            VLC_UNUSED( vars );
        }
        E_CASE_DEFAULT(element) {
            MkvTree( *vars.p_demuxer, 2, "Unknown (%s)", typeid(element).name() );
        }
    };

    TrackHandlers::Dispatcher().iterate( tracks->begin(), tracks->end(), &payload );
}

/*****************************************************************************
 * ParseInfo:
 *****************************************************************************/
void matroska_segment_c::ParseInfo( KaxInfo *info )
{
    EbmlElement *el;
    EbmlMaster  *m;
    int i_upper_level = 0;

    /* Master elements */
    m = static_cast<EbmlMaster *>(info);
    if( unlikely( m->IsFiniteSize() && m->GetSize() >= SIZE_MAX ) )
    {
        msg_Err( &sys.demuxer, "Info too big, aborting" );
        return;
    }
    try
    {
        m->Read( es, EBML_CONTEXT(info), i_upper_level, el, true );
    }
    catch(...)
    {
        msg_Err( &sys.demuxer, "Couldn't read info" );
        return;
    }

    struct InfoHandlerPayload {
        demux_t            * p_demuxer;
        matroska_segment_c * obj;
        EbmlElement       *&  el;
        EbmlMaster        *&   m;
        double             f_duration;
        int& i_upper_level;

    } captures = { &sys.demuxer, this, el, m, -1., i_upper_level };

    MKV_SWITCH_CREATE(EbmlTypeDispatcher, InfoHandlers, InfoHandlerPayload)
    {
        MKV_SWITCH_INIT();

        static void debug (InfoHandlerPayload& vars, char const * fmt, ...)
        {
            va_list args; va_start( args, fmt );
            MkvTree_va( *vars.p_demuxer, 2, fmt, args);
            va_end( args );
        }
        E_CASE( KaxSegmentUID, uid )
        {
            if ( vars.obj->p_segment_uid == NULL )
            {
                vars.obj->p_segment_uid = new KaxSegmentUID( uid );
            }
            debug( vars, "UID=%" PRIx64, *reinterpret_cast<uint64*>( vars.obj->p_segment_uid->GetBuffer() ) );
        }
        E_CASE( KaxPrevUID, uid )
        {
            if ( vars.obj->p_prev_segment_uid == NULL )
            {
                vars.obj->p_prev_segment_uid = new KaxPrevUID( uid );
                vars.obj->b_ref_external_segments = true;
            }
            debug( vars, "PrevUID=%" PRIx64, *reinterpret_cast<uint64*>( vars.obj->p_prev_segment_uid->GetBuffer() ) );
        }
        E_CASE( KaxNextUID, uid )
        {
            if ( vars.obj->p_next_segment_uid == NULL )
            {
                vars.obj->p_next_segment_uid = new KaxNextUID( uid );
                vars.obj->b_ref_external_segments = true;
            }
            debug( vars, "NextUID=%" PRIx64, *reinterpret_cast<uint64*>( vars.obj->p_next_segment_uid->GetBuffer() ) );
        }
        E_CASE( KaxTimecodeScale, tcs )
        {
            vars.obj->i_timescale = static_cast<uint64>( tcs );
            debug( vars, "TimecodeScale=%" PRId64, vars.obj->i_timescale );
        }
        E_CASE( KaxDuration, dur )
        {
            vars.f_duration = static_cast<double>( dur );
            debug( vars, "Duration=%.0f", vars.f_duration );
        }
        E_CASE( KaxMuxingApp, mapp )
        {
            vars.obj->psz_muxing_application = ToUTF8( UTFstring( mapp ) );
            debug( vars, "Muxing Application=%s", vars.obj->psz_muxing_application );
        }
        E_CASE( KaxWritingApp, wapp )
        {
            vars.obj->psz_writing_application = ToUTF8( UTFstring( wapp ) );
            debug( vars, "Writing Application=%s", vars.obj->psz_writing_application );
        }
        E_CASE( KaxSegmentFilename, sfn )
        {
            vars.obj->psz_segment_filename = ToUTF8( UTFstring( sfn ) );
            debug( vars, "Segment Filename=%s", vars.obj->psz_segment_filename );
        }
        E_CASE( KaxTitle, title )
        {
            vars.obj->psz_title = ToUTF8( UTFstring( title ) );
            debug( vars, "Title=%s", vars.obj->psz_title );
        }
        E_CASE( KaxSegmentFamily, uid )
        {
            vars.obj->families.push_back( new KaxSegmentFamily(uid) );
            debug( vars, "Family=%" PRIx64, *reinterpret_cast<uint64*>( uid.GetBuffer() ) );
        }
        E_CASE( KaxDateUTC, date )
        {
            struct tm tmres;
            char   buffer[25];
            time_t i_date = date.GetEpochDate();

            if( gmtime_r( &i_date, &tmres ) &&
                strftime( buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y",
                          &tmres ) )
            {
                vars.obj->psz_date_utc = strdup( buffer );
                debug( vars, "Date=%s", vars.obj->psz_date_utc );
            }
        }
        E_CASE( KaxChapterTranslate, trans )
        {
            MKV_SWITCH_CREATE( EbmlTypeDispatcher, TranslationHandler, chapter_translation_c* )
            {
                MKV_SWITCH_INIT();

                E_CASE( KaxChapterTranslateEditionUID, uid )
                {
                    vars->editions.push_back( static_cast<uint64>( uid ) );
                }
                E_CASE( KaxChapterTranslateCodec, codec_id )
                {
                    vars->codec_id = static_cast<uint32>( codec_id );
                }
                E_CASE( KaxChapterTranslateID, translated_id )
                {
                    vars->p_translated = new KaxChapterTranslateID( translated_id );
                }
            };
            try
            {
                if( unlikely( trans.IsFiniteSize() && trans.GetSize() >= SIZE_MAX ) )
                {
                    msg_Err( vars.p_demuxer, "Chapter translate too big, aborting" );
                    return;
                }

                trans.Read( vars.obj->es, EBML_CONTEXT(&trans), vars.i_upper_level, vars.el, true );

                chapter_translation_c *p_translate = new chapter_translation_c();

                TranslationHandler::Dispatcher().iterate(
                    trans.begin(), trans.end(), &p_translate
                );

                vars.obj->translations.push_back( p_translate );
            }
            catch(...)
            {
                msg_Err( vars.p_demuxer, "Error while reading Chapter Translate");
            }
        }
        E_CASE( EbmlVoid, )
        {
            VLC_UNUSED( vars );
        }
        E_CASE_DEFAULT(element)
        {
            debug( vars, "Unknown (%s)", typeid(element).name() );
        }
    };

    InfoHandlers::Dispatcher().iterate( m->begin(), m->end(), &captures );

    if( captures.f_duration != -1. )
        i_duration = VLC_TICK_FROM_NS( captures.f_duration * i_timescale );
}


/*****************************************************************************
 * ParseChapterAtom
 *****************************************************************************/
void matroska_segment_c::ParseChapterAtom( int i_level, KaxChapterAtom *ca, chapter_item_c & chapters )
{
    MkvTree( sys.demuxer, 3, "ChapterAtom (level=%d)", i_level );

    struct ChapterPayload {
        matroska_segment_c * const       obj;
        demux_t            * const p_demuxer;
        chapter_item_c     &        chapters;

        int& i_level;
        int level;

    } payload = {
        this, &sys.demuxer, chapters,
        i_level, 4
    };

    MKV_SWITCH_CREATE( EbmlTypeDispatcher, ChapterAtomHandlers, ChapterPayload )
    {
        MKV_SWITCH_INIT();

        static void debug (ChapterPayload const& vars, char const * fmt, ...)
        {
            va_list args; va_start( args, fmt );
            MkvTree_va( *vars.p_demuxer, vars.level, fmt, args);
            va_end( args );
        }
        E_CASE( KaxChapterUID, uid )
        {
            vars.chapters.i_uid = static_cast<uint64_t>( uid );
            debug( vars, "ChapterUID=%" PRIx64, vars.chapters.i_uid );
        }
        E_CASE( KaxChapterFlagHidden, flag )
        {
            vars.chapters.b_display_seekpoint = static_cast<uint8>( flag ) == 0;
            debug( vars, "ChapterFlagHidden=%s", vars.chapters.b_display_seekpoint ? "no" : "yes" );
        }
        E_CASE( KaxChapterSegmentUID, uid )
        {
            vars.chapters.p_segment_uid = new KaxChapterSegmentUID( uid );
            vars.obj->b_ref_external_segments = true;

            debug( vars, "ChapterSegmentUID=%" PRIx64, *reinterpret_cast<uint64*>( vars.chapters.p_segment_uid->GetBuffer() ) );
        }
        E_CASE( KaxChapterSegmentEditionUID, euid )
        {
            vars.chapters.p_segment_edition_uid = new KaxChapterSegmentEditionUID( euid );

            debug( vars, "ChapterSegmentEditionUID=%x",
#if LIBMATROSKA_VERSION < 0x010300
              *reinterpret_cast<uint32*>( vars.chapters.p_segment_edition_uid->GetBuffer() )
#else
              static_cast<uint32>( *vars.chapters.p_segment_edition_uid )
#endif
            );
        }
        E_CASE( KaxChapterTimeStart, start )
        {
            vars.chapters.i_start_time = VLC_TICK_FROM_NS(static_cast<uint64>( start ));
            debug( vars, "ChapterTimeStart=%" PRId64, vars.chapters.i_start_time );
        }
        E_CASE( KaxChapterTimeEnd, end )
        {
            vars.chapters.i_end_time = VLC_TICK_FROM_NS(static_cast<uint64>( end ));
            debug( vars, "ChapterTimeEnd=%" PRId64, vars.chapters.i_end_time );
        }
        E_CASE( KaxChapterDisplay, chapter_display )
        {
            debug( vars, "ChapterDisplay" );

            vars.level += 1;
            dispatcher.iterate( chapter_display.begin(), chapter_display.end(), &vars );
            vars.level -= 1;
        }
        E_CASE( KaxChapterString, name )
        {
            std::string str_name( UTFstring( name ).GetUTF8() );

            for ( int k = 0; k < vars.i_level; k++)
                vars.chapters.str_name += '+';

            vars.chapters.str_name += ' ';
            vars.chapters.str_name += str_name;
            vars.chapters.b_user_display = true;

            debug( vars, "ChapterString=%s", str_name.c_str() );
        }
        E_CASE( KaxChapterLanguage, lang )
        {
            debug( vars, "ChapterLanguage=%s", static_cast<std::string const&>( lang ).c_str() );
        }
        E_CASE( KaxChapterCountry, ct )
        {
            debug( vars, "ChapterCountry=%s", static_cast<std::string const&>( ct ).c_str() );
        }

        E_CASE( KaxChapterProcess, cp )
        {
            debug( vars, "ChapterProcess" );

            chapter_codec_cmds_c *p_ccodec = NULL;

            for( size_t j = 0; j < cp.ListSize(); j++ )
            {
                if( MKV_CHECKED_PTR_DECL( p_codec_id, KaxChapterProcessCodecID, cp[j] ) )
                {
                    if ( static_cast<uint32>(*p_codec_id) == 0 )
                        p_ccodec = new matroska_script_codec_c( vars.obj->sys );
                    else if ( static_cast<uint32>(*p_codec_id) == 1 )
                        p_ccodec = new dvd_chapter_codec_c( vars.obj->sys );
                    break;
                }
            }

            if ( p_ccodec != NULL )
            {
                for( size_t j = 0; j < cp.ListSize(); j++ )
                {
                    EbmlElement *k= cp[j];

                    if( MKV_CHECKED_PTR_DECL( p_private, KaxChapterProcessPrivate, k ) )
                    {
                        p_ccodec->SetPrivate( *p_private );
                    }
                    else if ( MKV_CHECKED_PTR_DECL( cmd, KaxChapterProcessCommand, k ) )
                    {
                        p_ccodec->AddCommand( *cmd );
                    }
                }
                vars.chapters.codecs.push_back( p_ccodec );
            }
        }
        E_CASE( KaxChapterAtom, atom )
        {
            chapter_item_c *new_sub_chapter = new chapter_item_c();
            new_sub_chapter->p_parent = &vars.chapters;

            vars.obj->ParseChapterAtom( vars.i_level+1, &atom, *new_sub_chapter );
            vars.chapters.sub_chapters.push_back( new_sub_chapter );
        }
    };

    ChapterAtomHandlers::Dispatcher().iterate( ca->begin(), ca->end(), &payload );
}

/*****************************************************************************
 * ParseAttachments:
 *****************************************************************************/
void matroska_segment_c::ParseAttachments( KaxAttachments *attachments )
{
    EbmlElement *el;
    int i_upper_level = 0;

    if( unlikely( attachments->IsFiniteSize() && attachments->GetSize() >= SIZE_MAX ) )
    {
        msg_Err( &sys.demuxer, "Attachments too big, aborting" );
        return;
    }
    try
    {
        attachments->Read( es, EBML_CONTEXT(attachments), i_upper_level, el, true );
    }
    catch(...)
    {
        msg_Err( &sys.demuxer, "Error while reading attachments" );
        return;
    }

    KaxAttached *attachedFile = FindChild<KaxAttached>( *attachments );

    while( attachedFile && ( attachedFile->GetSize() > 0 ) )
    {
        KaxFileData  &img_data     = GetChild<KaxFileData>( *attachedFile );
        std::string attached_filename( UTFstring( GetChild<KaxFileName>( *attachedFile ) ).GetUTF8() );
        attachment_c *new_attachment = new attachment_c( attached_filename,
                                                         GetChild<KaxMimeType>( *attachedFile ),
                                                         img_data.GetSize() );

        msg_Dbg( &sys.demuxer, "|   |   - %s (%s)", new_attachment->fileName(), new_attachment->mimeType() );

        if( new_attachment->init() )
        {
            memcpy( new_attachment->p_data, img_data.GetBuffer(), img_data.GetSize() );
            sys.stored_attachments.push_back( new_attachment );
            if( !strncmp( new_attachment->mimeType(), "image/", 6 ) )
            {
                char *psz_url;
                if( asprintf( &psz_url, "attachment://%s",
                              new_attachment->fileName() ) == -1 )
                    continue;
                if( !sys.meta )
                    sys.meta = vlc_meta_New();
                vlc_meta_SetArtURL( sys.meta, psz_url );
                free( psz_url );
            }
        }
        else
        {
            delete new_attachment;
        }

        attachedFile = &GetNextChild<KaxAttached>( *attachments, *attachedFile );
    }
}

/*****************************************************************************
 * ParseChapters:
 *****************************************************************************/
void matroska_segment_c::ParseChapters( KaxChapters *chapters )
{
    if( unlikely( chapters->IsFiniteSize() && chapters->GetSize() >= SIZE_MAX ) )
    {
        msg_Err( &sys.demuxer, "Chapters too big, aborting" );
        return;
    }
    try
    {
        EbmlElement *el;
        int i_upper_level = 0;
        chapters->Read( es, EBML_CONTEXT(chapters), i_upper_level, el, true );
    }
    catch(...)
    {
        msg_Err( &sys.demuxer, "Error while reading chapters" );
        return;
    }
    MKV_SWITCH_CREATE( EbmlTypeDispatcher, KaxChapterHandler, matroska_segment_c )
    {
        MKV_SWITCH_INIT();
        E_CASE( KaxEditionEntry, entry )
        {
            struct EditionPayload {
                matroska_segment_c * const obj;
                demux_t            * const p_demuxer;
                chapter_edition_c  * const p_edition;

            } data = { &vars, &vars.sys.demuxer, new chapter_edition_c };

            MKV_SWITCH_CREATE( EbmlTypeDispatcher, KaxEditionHandler, EditionPayload )
            {
                MKV_SWITCH_INIT();
                E_CASE( KaxChapterAtom, chapter_atom )
                {
                    chapter_item_c *new_sub_chapter = new chapter_item_c();
                    vars.obj->ParseChapterAtom( 0, &chapter_atom, *new_sub_chapter );
                    vars.p_edition->sub_chapters.push_back( new_sub_chapter );
                }
                E_CASE( KaxEditionUID, euid )
                {
                    vars.p_edition->i_uid = static_cast<uint64> ( euid );
                }
                E_CASE( KaxEditionFlagOrdered, flag_ordered )
                {
                    vars.p_edition->b_ordered = var_InheritBool(vars.p_demuxer, "mkv-use-ordered-chapters") && static_cast<uint8>( flag_ordered );
                }
                E_CASE( KaxEditionFlagDefault, flag_default )
                {
                    if( static_cast<uint8>( flag_default ) )
                        vars.obj->i_default_edition = vars.obj->stored_editions.size();
                }
                E_CASE( KaxEditionFlagHidden, flag_hidden )
                {
                    vars.p_edition->b_hidden = static_cast<uint8>( flag_hidden ) != 0;
                }
                E_CASE( EbmlVoid, el )
                {
                    VLC_UNUSED( el );
                    VLC_UNUSED( vars );
                }
                E_CASE_DEFAULT( el )
                {
                    msg_Dbg( vars.p_demuxer, "|   |   |   + Unknown (%s)", typeid(el).name() );
                }
            };
            KaxEditionHandler::Dispatcher().iterate( entry.begin(), entry.end(), &data );

            data.obj->stored_editions.push_back( data.p_edition );
        }
        E_CASE( EbmlVoid, el ) {
            VLC_UNUSED( el );
            VLC_UNUSED( vars );
        }
        E_CASE_DEFAULT( el )
        {
            msg_Dbg( &vars.sys.demuxer, "|   |   + Unknown (%s)", typeid(el).name() );
        }
    };

    KaxChapterHandler::Dispatcher().iterate( chapters->begin(), chapters->end(), this );
}

bool matroska_segment_c::ParseCluster( KaxCluster *cluster, bool b_update_start_time, ScopeMode read_fully )
{
    if( unlikely( cluster->IsFiniteSize() && cluster->GetSize() >= SIZE_MAX ) )
    {
        msg_Err( &sys.demuxer, "Cluster too big, aborting" );
        return false;
    }

    bool b_seekable;
    vlc_stream_Control( sys.demuxer.s, STREAM_CAN_SEEK, &b_seekable );
    if (!b_seekable)
        return false;

    try
    {
        EbmlElement *el;
        int i_upper_level = 0;

        cluster->Read( es, EBML_CONTEXT(cluster), i_upper_level, el, true, read_fully );
    }
    catch(...)
    {
        msg_Err( &sys.demuxer, "Error while reading cluster" );
        return false;
    }

    bool b_has_timecode = false;

    for( unsigned int i = 0; i < cluster->ListSize(); ++i )
    {
        if( MKV_CHECKED_PTR_DECL( p_ctc, KaxClusterTimecode, (*cluster)[i] ) )
        {
            cluster->InitTimecode( static_cast<uint64>( *p_ctc ), i_timescale );
            _seeker.add_cluster( cluster );
            b_has_timecode = true;
            break;
        }
    }

    if( !b_has_timecode )
    {
        msg_Err( &sys.demuxer, "Detected cluster without mandatory timecode" );
        return false;
    }

    if( b_update_start_time )
        i_mk_start_time = VLC_TICK_FROM_NS( cluster->GlobalTimecode() );

    return true;
}

#define ONLY_FMT(t) if(vars.p_tk->fmt.i_cat != t ## _ES) \
    throw std::runtime_error( "Mismatching track type" );

bool matroska_segment_c::TrackInit( mkv_track_t * p_tk )
{
    if( p_tk->codec.empty() )
    {
        msg_Err( &sys.demuxer, "Empty codec id" );
        p_tk->fmt.i_codec = VLC_CODEC_UNKNOWN;
        return true;
    }

    struct HandlerPayload {
        matroska_segment_c * obj;
        mkv_track_t        * p_tk;
        es_format_t        * p_fmt;
        demux_t            * p_demuxer;
    } captures = {
        this, p_tk, &p_tk->fmt, &sys.demuxer
    };

    MKV_SWITCH_CREATE( StringDispatcher, TrackCodecHandlers, HandlerPayload )
    {
        MKV_SWITCH_INIT();

        S_CASE("V_MS/VFW/FOURCC") {
            if( vars.p_tk->i_extra_data < (int)sizeof( VLC_BITMAPINFOHEADER ) )
            {
                msg_Err(vars.p_demuxer, "missing/invalid VLC_BITMAPINFOHEADER" );
                vars.p_fmt->i_codec = VLC_CODEC_UNKNOWN;
            }
            else
            {
                ONLY_FMT(VIDEO);
                VLC_BITMAPINFOHEADER *p_bih = (VLC_BITMAPINFOHEADER*)vars.p_tk->p_extra_data;

                vars.p_fmt->video.i_width = GetDWLE( &p_bih->biWidth );
                vars.p_fmt->video.i_height= GetDWLE( &p_bih->biHeight );
                vars.p_fmt->i_codec       = GetFOURCC( &p_bih->biCompression );

                /* Very unlikely yet possible: bug #5659*/
                const unsigned int min_extra = std::min(GetDWLE( &p_bih->biSize ), vars.p_tk->i_extra_data);
                if ( min_extra > sizeof( VLC_BITMAPINFOHEADER ))
                {
                    vars.p_fmt->i_extra = min_extra - sizeof( VLC_BITMAPINFOHEADER );
                    vars.p_fmt->p_extra = xmalloc( vars.p_fmt->i_extra );
                    if (likely(vars.p_fmt->p_extra != NULL))
                        memcpy( vars.p_fmt->p_extra, &p_bih[1], vars.p_fmt->i_extra );
                    else
                        vars.p_fmt->i_extra = 0;
                }
                else if( vars.p_fmt->i_codec == VLC_FOURCC('W','V','C','1') )
                {
                    vars.p_fmt->video.i_width = 0;
                    vars.p_fmt->video.i_height = 0;
                    vars.p_fmt->b_packetized = false;
                }
            }
            vars.p_tk->b_dts_only = true;
        }
        S_CASE("V_MPEG1") {
            vars.p_fmt->i_codec = VLC_CODEC_MPGV;
            fill_extra_data( vars.p_tk, 0 );
        }
        S_CASE("V_MPEG2") {
            vars.p_fmt->i_codec = VLC_CODEC_MPGV;
            if (vars.obj->psz_muxing_application != NULL &&
                    strstr(vars.obj->psz_muxing_application,"libmakemkv"))
                vars.p_fmt->b_packetized = false;
            fill_extra_data( vars.p_tk, 0 );
        }
        S_CASE("V_THEORA") {
            vars.p_fmt->i_codec = VLC_CODEC_THEORA;
            vars.p_tk->b_pts_only = true;
            fill_extra_data( vars.p_tk, 0 );
        }
        static void v_real_helper (vlc_fourcc_t codec, HandlerPayload& vars)
        {
            vars.p_fmt->i_codec   = codec;

            /* Extract the framerate from the header */
            uint8_t *p = vars.p_tk->p_extra_data;

            if (
                vars.p_tk->i_extra_data >= 26 && !memcmp(p+4, "VIDORV", 6) && strchr("34", p[10]) && p[11] == '0')
            {
                ONLY_FMT(VIDEO);
                vars.p_tk->fmt.video.i_frame_rate      = p[22] << 24 | p[23] << 16 | p[24] << 8 | p[25] << 0;
                vars.p_tk->fmt.video.i_frame_rate_base = 65536;
            }

            fill_extra_data( vars.p_tk, 26 );
        }
        S_CASE("V_REAL/RV10") { v_real_helper (VLC_CODEC_RV10, vars ); }
        S_CASE("V_REAL/RV20") { v_real_helper (VLC_CODEC_RV20, vars ); }
        S_CASE("V_REAL/RV30") { v_real_helper (VLC_CODEC_RV30, vars ); }
        S_CASE("V_REAL/RV40") { v_real_helper (VLC_CODEC_RV40, vars ); }
        S_CASE("V_DIRAC")     {
            vars.p_fmt->i_codec = VLC_CODEC_DIRAC;
        }
        S_CASE("V_VP8") {
            vars.p_fmt->i_codec = VLC_CODEC_VP8;
            vars.p_tk->b_pts_only = true;
        }
        S_CASE("V_VP9") {
            vars.p_fmt->i_codec = VLC_CODEC_VP9;
            vars.p_fmt->b_packetized = false;
            vars.p_tk->b_pts_only = true;

            fill_extra_data( vars.p_tk, 0 );
        }
        S_CASE("V_AV1") {
            vars.p_fmt->i_codec = VLC_CODEC_AV1;
            vars.p_tk->b_pts_only = true;

            fill_extra_data( vars.p_tk, 0 );
        }
        S_CASE("V_MPEG4/MS/V3") {
            vars.p_fmt->i_codec = VLC_CODEC_DIV3;
        }
        S_CASE("V_MPEG4/ISO/AVC") {
            vars.p_fmt->i_codec = VLC_FOURCC( 'a','v','c','1' );
            fill_extra_data( vars.p_tk, 0 );
        }
        S_CASE_GLOB("V_MPEG4/ISO*") {
            vars.p_fmt->i_codec = VLC_CODEC_MP4V;
            fill_extra_data( vars.p_tk, 0 );
        }
        S_CASE("V_MPEGH/ISO/HEVC") {
            vars.p_tk->fmt.i_codec = VLC_CODEC_HEVC;

            uint8_t* p_extra = (uint8_t*) vars.p_tk->p_extra_data;

            /* HACK: if we found invalid format, made by mkvmerge < 16.0.0,
             *       we try to fix it. They fixed it in 16.0.0. */
            const char* app = vars.obj->psz_writing_application;
            if( p_extra && vars.p_tk->i_extra_data >= 3 &&
                    p_extra[0] == 0 && (p_extra[1] != 0 || p_extra[2] > 1) )
            {
                msg_Warn(vars.p_demuxer,
                        "Invalid HEVC reserved bits in mkv file "
                        "made by %s, fixing it", app ? app : "unknown app");
                p_extra[0] = 0x01;
            }

            fill_extra_data( vars.p_tk, 0 );
        }
        S_CASE("V_QUICKTIME") {
            ONLY_FMT(VIDEO);
            if( vars.p_tk->i_extra_data > 4 )
            {
                MP4_Box_t *p_box = MP4_BoxNew(ATOM_root);
                if( p_box )
                {
                    stream_t *p_mp4_stream = vlc_stream_MemoryNew( VLC_OBJECT(vars.p_demuxer),
                                                                   vars.p_tk->p_extra_data,
                                                                   vars.p_tk->i_extra_data,
                                                                   true );
                    if( p_mp4_stream )
                    {
                        p_box->i_type = GetFOURCC( vars.p_tk->p_extra_data );
                        p_box->i_size = p_box->i_shortsize = vars.p_tk->i_extra_data;
                        if( MP4_ReadBox_sample_vide( p_mp4_stream, p_box ) )
                        {
                            const MP4_Box_data_sample_vide_t *p_sample = p_box->data.p_sample_vide;
                            vars.p_fmt->i_codec = p_box->i_type;
                            if( p_sample->i_width && p_sample->i_height )
                            {
                                vars.p_tk->fmt.video.i_width = p_sample->i_width;
                                vars.p_tk->fmt.video.i_height = p_sample->i_height;
                            }
                            vars.p_fmt->p_extra = malloc( p_sample->i_qt_image_description );
                            if( vars.p_fmt->p_extra )
                            {
                                vars.p_fmt->i_extra = p_sample->i_qt_image_description;
                                memcpy( vars.p_fmt->p_extra,
                                        p_sample->p_qt_image_description, vars.p_fmt->i_extra );
                            }
                        }
                        vlc_stream_Delete( p_mp4_stream );
                    }
                    MP4_BoxFree( p_box );
                }
            }
            else throw std::runtime_error ("invalid extradata when handling V_QUICKTIME/*");
        }
        S_CASE("V_MJPEG") {
            vars.p_fmt->i_codec = VLC_CODEC_MJPG;
            vars.p_tk->b_pts_only = true;
        }
        S_CASE("V_UNCOMPRESSED") {
            msg_Dbg( vars.p_demuxer, "uncompressed format detected");
        }
        S_CASE("V_FFV1") {
            vars.p_fmt->i_codec = VLC_CODEC_FFV1;
            fill_extra_data( vars.p_tk, 0 );
        }
        S_CASE("V_PRORES") {
            vars.p_fmt->i_codec = VLC_CODEC_PRORES;
            fill_extra_data( vars.p_tk, 0 );
        }
        S_CASE("A_MS/ACM") {
            mkv_track_t * p_tk = vars.p_tk;
            es_format_t * p_fmt = &vars.p_tk->fmt;

            if( p_tk->i_extra_data < (int)sizeof( WAVEFORMATEX ) )
            {
                msg_Err( vars.p_demuxer, "missing/invalid WAVEFORMATEX" );
                p_tk->fmt.i_codec = VLC_CODEC_UNKNOWN;
            }
            else
            {
                ONLY_FMT(AUDIO);
                WAVEFORMATEX *p_wf = (WAVEFORMATEX*)p_tk->p_extra_data;

                p_tk->fmt.audio.i_channels   = GetWLE( &p_wf->nChannels );
                p_tk->fmt.audio.i_rate = GetDWLE( &p_wf->nSamplesPerSec );
                p_tk->fmt.i_bitrate    = GetDWLE( &p_wf->nAvgBytesPerSec ) * 8;
                p_tk->fmt.audio.i_blockalign = GetWLE( &p_wf->nBlockAlign );;
                p_tk->fmt.audio.i_bitspersample = GetWLE( &p_wf->wBitsPerSample );

                p_tk->fmt.i_extra            = GetWLE( &p_wf->cbSize );
                if ( (size_t)p_tk->fmt.i_extra > p_tk->i_extra_data - sizeof( WAVEFORMATEX ) )
                    p_tk->fmt.i_extra = 0;
                if( p_tk->fmt.i_extra != 0 )
                {
                    p_tk->fmt.p_extra = xmalloc( p_tk->fmt.i_extra );
                    if( p_tk->fmt.p_extra )
                        memcpy( p_tk->fmt.p_extra, &p_wf[1], p_tk->fmt.i_extra );
                    else
                        p_tk->fmt.i_extra = 0;
                }

                if( p_wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                    p_tk->i_extra_data >= sizeof(WAVEFORMATEXTENSIBLE) )
                {
                    WAVEFORMATEXTENSIBLE *p_wext = (WAVEFORMATEXTENSIBLE*)p_wf;
                    GUID subFormat = p_wext->SubFormat;

                    sf_tag_to_fourcc( &subFormat,  &p_tk->fmt.i_codec, NULL);
                    /* FIXME should we use Samples */

                    if( p_tk->fmt.audio.i_channels > 2 &&
                        ( p_tk->fmt.i_codec != VLC_CODEC_UNKNOWN ) )
                    {
                        uint32_t wfextcm = GetDWLE( &p_wext->dwChannelMask );
                        int match;
                        unsigned i_channel_mask = getChannelMask( &wfextcm,
                                                                  p_tk->fmt.audio.i_channels,
                                                                  &match );
                        p_tk->fmt.i_codec = vlc_fourcc_GetCodecAudio( p_tk->fmt.i_codec,
                                                                      p_tk->fmt.audio.i_bitspersample );
                        if( i_channel_mask )
                        {
                            p_tk->i_chans_to_reorder = aout_CheckChannelReorder(
                                pi_channels_aout, NULL,
                                i_channel_mask,
                                p_tk->pi_chan_table );

                            p_tk->fmt.audio.i_physical_channels = i_channel_mask;
                        }
                    }
                }
                else
                {
                    wf_tag_to_fourcc( GetWLE( &p_wf->wFormatTag ), &p_tk->fmt.i_codec, NULL );
                    if( p_wf->wFormatTag == WAVE_FORMAT_AAC_LATM )
                    {
                        p_tk->fmt.i_original_fourcc = VLC_FOURCC('L','A','T','M');
                    }
                    else if( p_wf->wFormatTag == WAVE_FORMAT_AAC_ADTS )
                    {
                        p_tk->fmt.i_original_fourcc = VLC_FOURCC('A','D','T','S');
                    }
                }

                if( p_tk->fmt.i_codec == VLC_CODEC_UNKNOWN )
                    msg_Err( vars.p_demuxer, "Unrecognized wf tag: 0x%x", GetWLE( &p_wf->wFormatTag ) );
            }
            p_fmt->b_packetized = !p_fmt->audio.i_blockalign;
        }
        static void A_MPEG_helper_ (HandlerPayload& vars) {
            vars.p_tk->fmt.i_codec = VLC_CODEC_MPGA;
            vars.p_fmt->b_packetized = false;
        }
        S_CASE("A_MPEG/L3") { A_MPEG_helper_(vars); }
        S_CASE("A_MPEG/L2") { A_MPEG_helper_(vars); }
        S_CASE("A_MPEG/L1") { A_MPEG_helper_(vars); }
        S_CASE("A_AC3") {
            ONLY_FMT(AUDIO);
            // the AC-3 default duration cannot be trusted, see #8512
            if ( vars.p_tk->fmt.audio.i_rate == 8000 )
            {
                vars.p_tk->b_no_duration = true;
                vars.p_tk->i_default_duration = 0;
            }

            vars.p_fmt->i_codec = VLC_CODEC_A52;
            vars.p_fmt->b_packetized = false;
        }
        S_CASE("A_EAC3") {
            vars.p_fmt->i_codec = VLC_CODEC_EAC3;
            vars.p_fmt->b_packetized = false;
        }
        S_CASE("A_DTS")  {
            vars.p_fmt->i_codec = VLC_CODEC_DTS;
            vars.p_fmt->b_packetized = false;
        }
        S_CASE("A_MLP")  { vars.p_fmt->i_codec = VLC_CODEC_MLP; }
        S_CASE("A_TRUEHD") { /* FIXME when more samples arrive */
            vars.p_fmt->i_codec = VLC_CODEC_TRUEHD;
            vars.p_fmt->b_packetized = false;
        }
        S_CASE("A_FLAC") {
            vars.p_fmt->i_codec = VLC_CODEC_FLAC;
            fill_extra_data( vars.p_tk, 0 );
        }
        S_CASE("A_VORBIS") {
            vars.p_fmt->i_codec = VLC_CODEC_VORBIS;
            fill_extra_data( vars.p_tk, 0 );
        }
        static void A_OPUS__helper(HandlerPayload& vars) {
            ONLY_FMT(AUDIO);
            vars.p_fmt->i_codec = VLC_CODEC_OPUS;
            vars.p_tk->b_no_duration = true;
            if( !vars.p_tk->fmt.audio.i_rate )
            {
                msg_Err( vars.p_demuxer,"No sampling rate, defaulting to 48kHz");
                vars.p_fmt->audio.i_rate = 48000;
            }
            const uint8_t tags[16] = {'O','p','u','s','T','a','g','s',
                                       0, 0, 0, 0, 0, 0, 0, 0};
            unsigned ps[2] = { vars.p_tk->i_extra_data, 16 };
            const void *pkt[2] = { static_cast<const void *>( vars.p_tk->p_extra_data ),
                                   static_cast<const void *>( tags ) };

            if( xiph_PackHeaders( &vars.p_fmt->i_extra,
                &vars.p_fmt->p_extra,
                ps, pkt, 2 ) )
                msg_Err( vars.p_demuxer, "Couldn't pack OPUS headers");
        }
        S_CASE("A_OPUS")                { A_OPUS__helper( vars ); }
        S_CASE("A_OPUS/EXPERIMENTAL")   { A_OPUS__helper( vars ); }
        static void A_AAC_MPEG__helper(HandlerPayload& vars, int i_profile, bool sbr = false) {
            ONLY_FMT(AUDIO);
            int i_srate;

            mkv_track_t * p_tk = vars.p_tk;

            static const unsigned int i_sample_rates[] =
            {
                96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                16000, 12000, 11025,  8000,  7350,     0,     0,     0
            };

            p_tk->fmt.i_codec = VLC_CODEC_MP4A;

            for( i_srate = 0; i_srate < 13; i_srate++ )
            {
                if( i_sample_rates[i_srate] == p_tk->i_original_rate )
                    break;
            }

            msg_Dbg (vars.p_demuxer, "profile=%d srate=%d", i_profile, i_srate );

            p_tk->fmt.i_extra = sbr ? 5 : 2;
            p_tk->fmt.p_extra = xmalloc( p_tk->fmt.i_extra );
            ((uint8_t*)p_tk->fmt.p_extra)[0] = ((i_profile + 1) << 3) | ((i_srate&0xe) >> 1);
            ((uint8_t*)p_tk->fmt.p_extra)[1] = ((i_srate & 0x1) << 7) | (p_tk->fmt.audio.i_channels << 3);

            if (sbr) {
                int syncExtensionType = 0x2B7;
                int iDSRI;
                for (iDSRI=0; iDSRI<13; iDSRI++)
                    if( i_sample_rates[iDSRI] == p_tk->fmt.audio.i_rate )
                        break;
                ((uint8_t*)p_tk->fmt.p_extra)[2] = (syncExtensionType >> 3) & 0xFF;
                ((uint8_t*)p_tk->fmt.p_extra)[3] = ((syncExtensionType & 0x7) << 5) | 5;
                ((uint8_t*)p_tk->fmt.p_extra)[4] = ((1 & 0x1) << 7) | (iDSRI << 3);
            }

        }
        S_CASE("A_AAC/MPEG2/MAIN")   { A_AAC_MPEG__helper( vars, 0 ); }
        S_CASE("A_AAC/MPEG4/MAIN")   { A_AAC_MPEG__helper( vars, 0 ); }
        S_CASE("A_AAC/MPEG2/LC")     { A_AAC_MPEG__helper( vars, 1 ); }
        S_CASE("A_AAC/MPEG4/LC")     { A_AAC_MPEG__helper( vars, 1 ); }
        S_CASE("A_AAC/MPEG2/SSR")    { A_AAC_MPEG__helper( vars, 2 ); }
        S_CASE("A_AAC/MPEG4/SSR")    { A_AAC_MPEG__helper( vars, 2 ); }
        S_CASE("A_AAC/MPEG4/LTP")    { A_AAC_MPEG__helper( vars, 3 ); }
        S_CASE("A_AAC/MPEG2/LC/SBR") { A_AAC_MPEG__helper( vars, 1, true ); }
        S_CASE("A_AAC/MPEG4/LC/SBR") { A_AAC_MPEG__helper( vars, 1, true ); }
        S_CASE("A_AAC/MPEG4/") { A_AAC_MPEG__helper( vars, 3 ); } // see #4250
        S_CASE("A_AAC/MPEG2/") { A_AAC_MPEG__helper( vars, 3 ); } // backward compatibility
        S_CASE("A_AAC") {
            vars.p_tk->fmt.i_codec = VLC_CODEC_MP4A;
            fill_extra_data( vars.p_tk, 0 );
        }
        S_CASE("A_ALAC") {
            vars.p_tk->fmt.i_codec = VLC_CODEC_ALAC;
            fill_extra_data_alac( vars.p_tk );
        }
        S_CASE("A_WAVPACK4") {
            vars.p_tk->fmt.i_codec = VLC_CODEC_WAVPACK;
            fill_extra_data( vars.p_tk, 0);
        }
        S_CASE("A_TTA1") {
            ONLY_FMT(AUDIO);
            mkv_track_t * p_tk  = vars.p_tk;
            es_format_t * p_fmt = vars.p_fmt;

            p_fmt->i_codec = VLC_CODEC_TTA;
            if( p_tk->i_extra_data > 0 )
            {
                fill_extra_data( p_tk, 0 );
            }
            else
            {
                p_fmt->i_extra = 30;
                p_fmt->p_extra = xmalloc( p_fmt->i_extra );
                uint8_t *p_extra = (uint8_t*)p_fmt->p_extra;
                memcpy( &p_extra[ 0], "TTA1", 4 );
                SetWLE( &p_extra[ 4], 1 );
                SetWLE( &p_extra[ 6], p_fmt->audio.i_channels );
                SetWLE( &p_extra[ 8], p_fmt->audio.i_bitspersample );
                SetDWLE( &p_extra[10], p_fmt->audio.i_rate );
                SetDWLE( &p_extra[14], 0xffffffff );
                memset( &p_extra[18], 0, 30  - 18 );
            }
        }
        static void A_PCM__helper (HandlerPayload& vars, uint32_t i_codec) {
            ONLY_FMT(AUDIO);
            vars.p_fmt->i_codec = i_codec;
            vars.p_fmt->audio.i_blockalign = ( vars.p_fmt->audio.i_bitspersample + 7 ) / 8 * vars.p_fmt->audio.i_channels;

        }
        S_CASE("A_PCM/INT/BIG")    { A_PCM__helper ( vars, VLC_FOURCC( 't','w','o','s' ) ); }
        S_CASE("A_PCM/INT/LIT")    { A_PCM__helper ( vars, VLC_FOURCC( 'a','r','a','w' ) ); }
        S_CASE("A_PCM/FLOAT/IEEE") { A_PCM__helper ( vars, VLC_FOURCC( 'a','f','l','t' ) ) ;}
        S_CASE("A_REAL/14_4") {
            ONLY_FMT(AUDIO);
            vars.p_fmt->i_codec = VLC_CODEC_RA_144;
            vars.p_fmt->audio.i_channels = 1;
            vars.p_fmt->audio.i_rate = 8000;
            vars.p_fmt->audio.i_blockalign = 0x14;
        }
        static bool A_REAL__is_valid (HandlerPayload& vars) {
            ONLY_FMT(AUDIO);
            uint8_t *p = vars.p_tk->p_extra_data;

            if (vars.p_tk->i_extra_data <= sizeof(real_audio_private))
                return false;

            if( memcmp( p, ".ra", 3 ) ) {
                msg_Err( vars.p_demuxer, "Invalid Real ExtraData 0x%4.4s", (char *)p );
                vars.p_tk->fmt.i_codec = VLC_CODEC_UNKNOWN;
                return false;
            }

            return true;
        }
        static void A_REAL__helper (HandlerPayload& vars, uint32_t i_codec) {
            mkv_track_t        * p_tk = vars.p_tk;
            real_audio_private * priv = (real_audio_private*) p_tk->p_extra_data;

            p_tk->fmt.i_codec = i_codec;

            /* FIXME RALF and SIPR */
            uint16_t version = (uint16_t) hton16(priv->version);

            p_tk->p_sys = new Cook_PrivateTrackData(
                  hton16( priv->sub_packet_h ),
                  hton16( priv->frame_size ),
                  hton16( priv->sub_packet_size )
            );

            if( unlikely( !p_tk->p_sys ) )
                throw std::runtime_error ("p_tk->p_sys is NULL when handling A_REAL/28_8");

            if( unlikely( p_tk->p_sys->Init() ) )
                throw std::runtime_error ("p_tk->p_sys->Init() failed when handling A_REAL/28_8");

            if( version == 4 )
            {
                real_audio_private_v4 * v4 = (real_audio_private_v4*) priv;
                p_tk->fmt.audio.i_channels = hton16(v4->channels);
                p_tk->fmt.audio.i_bitspersample = hton16(v4->sample_size);
                p_tk->fmt.audio.i_rate = hton16(v4->sample_rate);
            }
            else if( version == 5 )
            {
                real_audio_private_v5 * v5 = (real_audio_private_v5*) priv;
                p_tk->fmt.audio.i_channels = hton16(v5->channels);
                p_tk->fmt.audio.i_bitspersample = hton16(v5->sample_size);
                p_tk->fmt.audio.i_rate = hton16(v5->sample_rate);
            }
            msg_Dbg(vars.p_demuxer, "%d channels %d bits %d Hz",p_tk->fmt.audio.i_channels, p_tk->fmt.audio.i_bitspersample, p_tk->fmt.audio.i_rate);

            fill_extra_data( p_tk, p_tk->fmt.i_codec == VLC_CODEC_RA_288 ? 0 : 78);
        }
        S_CASE("A_REAL/COOK") {
            if (!A_REAL__is_valid (vars))
                return;

            real_audio_private * priv = (real_audio_private*) vars.p_tk->p_extra_data;
            vars.p_tk->fmt.audio.i_blockalign = hton16(priv->sub_packet_size);

            A_REAL__helper (vars, VLC_CODEC_COOK);
        }
        S_CASE("A_REAL/ATRC") {
            if (!A_REAL__is_valid (vars))
                return;

            real_audio_private * priv = (real_audio_private*) vars.p_tk->p_extra_data;
            vars.p_tk->fmt.audio.i_blockalign = hton16(priv->sub_packet_size);

            A_REAL__helper (vars, VLC_CODEC_ATRAC3);
        }
        S_CASE("A_REAL/28_8") {
            if (!A_REAL__is_valid (vars))
                return;

            A_REAL__helper (vars, VLC_CODEC_RA_288);
        }
        S_CASE("A_QUICKTIME/QDM2") {
            vars.p_fmt->i_cat   = AUDIO_ES;
            vars.p_fmt->i_codec = VLC_CODEC_QDM2;

            fill_extra_data( vars.p_tk, 0 );
        }
        S_CASE("A_QUICKTIME/QDMC") {
            vars.p_fmt->i_cat   = AUDIO_ES;
            vars.p_fmt->i_codec = VLC_FOURCC('Q','D','M','C');

            fill_extra_data( vars.p_tk, 0 );
        }
        S_CASE_GLOB("A_QUICKTIME/*") {
            if (vars.p_tk->i_extra_data < 4)
                throw std::runtime_error ("invalid extradata when handling A_QUICKTIME/*");

            vars.p_fmt->i_cat = AUDIO_ES;
            vars.p_fmt->i_codec = GetFOURCC(vars.p_tk->p_extra_data);

            fill_extra_data( vars.p_tk, 0 );
        }
        S_CASE("S_KATE") {
            ONLY_FMT(SPU);
            vars.p_fmt->i_codec = VLC_CODEC_KATE;
            vars.p_fmt->subs.psz_encoding = strdup( "UTF-8" );

            fill_extra_data( vars.p_tk, 0 );
        }
        S_CASE("S_TEXT/ASCII") {
            ONLY_FMT(SPU);
            vars.p_fmt->i_codec = VLC_CODEC_SUBT;
            vars.p_fmt->subs.psz_encoding = strdup( "ASCII" );
        }
        S_CASE("S_TEXT/UTF8") {
            ONLY_FMT(SPU);
            vars.p_tk->fmt.i_codec = VLC_CODEC_SUBT;
            vars.p_tk->fmt.subs.psz_encoding = strdup( "UTF-8" );
        }
        S_CASE("S_TEXT/USF") {
            ONLY_FMT(SPU);
            vars.p_tk->fmt.i_codec = VLC_FOURCC( 'u', 's', 'f', ' ' );
            vars.p_tk->fmt.subs.psz_encoding = strdup( "UTF-8" );
            fill_extra_data( vars.p_tk, 0 );
        }
        static void SSA__helper (HandlerPayload& vars) {
            ONLY_FMT(SPU);
            vars.p_tk->fmt.i_codec = VLC_CODEC_SSA;
            vars.p_tk->fmt.subs.psz_encoding = strdup( "UTF-8" );
            fill_extra_data( vars.p_tk, 0 );
        }
        S_CASE("S_TEXT/SSA") { SSA__helper( vars ); }
        S_CASE("S_TEXT/ASS") { SSA__helper( vars ); }
        S_CASE("S_SSA")      { SSA__helper( vars ); }
        S_CASE("S_ASS")      { SSA__helper( vars ); }
        S_CASE("S_VOBSUB") {
            ONLY_FMT(SPU);
            mkv_track_t * p_tk = vars.p_tk;

            p_tk->fmt.i_codec = VLC_CODEC_SPU;
            p_tk->b_no_duration = true;
            if( p_tk->i_extra_data )
            {
                char *psz_start;
                char *psz_buf = (char *)malloc( p_tk->i_extra_data + 1);
                if( psz_buf != NULL )
                {
                    memcpy( psz_buf, p_tk->p_extra_data , p_tk->i_extra_data );
                    psz_buf[p_tk->i_extra_data] = '\0';

                    if (p_tk->fmt.i_cat == SPU_ES)
                    {
                        psz_start = strstr( psz_buf, "size:" );
                        if( psz_start &&
                            vobsub_size_parse( psz_start,
                                               &p_tk->fmt.subs.spu.i_original_frame_width,
                                               &p_tk->fmt.subs.spu.i_original_frame_height ) == VLC_SUCCESS )
                        {
                            msg_Dbg( vars.p_demuxer, "original frame size vobsubs: %dx%d",
                                     p_tk->fmt.subs.spu.i_original_frame_width,
                                     p_tk->fmt.subs.spu.i_original_frame_height );
                        }
                        else
                        {
                            msg_Warn( vars.p_demuxer, "reading original frame size for vobsub failed" );
                        }

                        psz_start = strstr( psz_buf, "palette:" );
                        if( psz_start &&
                            vobsub_palette_parse( psz_start, &p_tk->fmt.subs.spu.palette[1] ) == VLC_SUCCESS )
                        {
                            p_tk->fmt.subs.spu.palette[0] = SPU_PALETTE_DEFINED;
                            msg_Dbg( vars.p_demuxer, "vobsub palette read" );
                        }
                        else
                        {
                            msg_Warn( vars.p_demuxer, "reading original palette failed" );
                        }
                    }
                    free( psz_buf );
                }
            }
        }
        S_CASE("S_DVBSUB")
        {
            ONLY_FMT(SPU);
            vars.p_fmt->i_codec = VLC_CODEC_DVBS;

            if( vars.p_tk->i_extra_data < 4 )
                throw std::runtime_error( "not enough codec data for S_DVBSUB" );

            uint16_t page_id = GetWBE( &vars.p_tk->p_extra_data[0] );
            uint16_t ancillary_id = GetWBE( &vars.p_tk->p_extra_data[2] );

            vars.p_fmt->subs.dvb.i_id = ( ancillary_id << 16 ) | page_id;
        }
        S_CASE("S_HDMV/PGS") {
            vars.p_fmt->i_codec = VLC_CODEC_BD_PG;
        }
        S_CASE("S_HDMV/TEXTST") {
            vars.p_fmt->i_codec = VLC_CODEC_BD_TEXT;
        }
        S_CASE("D_WEBVTT/SUBTITLES") {
            ONLY_FMT(SPU);
            vars.p_fmt->i_codec = VLC_CODEC_WEBVTT;
            vars.p_fmt->subs.psz_encoding = strdup( "UTF-8");
        }
        S_CASE("S_TEXT/WEBVTT") {
            ONLY_FMT(SPU);
            vars.p_fmt->i_codec = VLC_CODEC_WEBVTT;
            vars.p_fmt->subs.psz_encoding = strdup( "UTF-8");
            fill_extra_data( vars.p_tk, 0 );
        }
        S_CASE("B_VOBBTN") {
            vars.p_fmt->i_cat = DATA_ES;
        }
        S_CASE_DEFAULT(str) {
            msg_Err( vars.p_demuxer, "unknown codec id=`%s'", str );
            vars.p_tk->fmt.i_codec = VLC_CODEC_UNKNOWN;
        }
    };

    try {
        TrackCodecHandlers::Dispatcher().send( p_tk->codec.c_str(), &captures );
    }
    catch (std::exception const& e)
    {
        msg_Err( &sys.demuxer, "Error when trying to initiate track (codec: %s): %s",
          p_tk->codec.c_str(), e.what () );
        return false;
    }

    return true;
}

} // namespace
