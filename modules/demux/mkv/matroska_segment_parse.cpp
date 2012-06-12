/*****************************************************************************
 * matroska_segment_parse.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2010 the VideoLAN team
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

#include "matroska_segment.hpp"

#include "chapters.hpp"

#include "demux.hpp"

#include "Ebml_parser.hpp"

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
    EbmlParser  *ep;
    EbmlElement *l;
    bool b_seekable;

    i_seekhead_count++;

    stream_Control( sys.demuxer.s, STREAM_CAN_SEEK, &b_seekable );
    if( !b_seekable )
        return;

    ep = new EbmlParser( &es, seekhead, &sys.demuxer );

    while( ( l = ep->Get() ) != NULL )
    {
        if( MKV_IS_ID( l, KaxSeek ) )
        {
            EbmlId id = EBML_ID(EbmlVoid);
            int64_t i_pos = -1;

#ifdef MKV_DEBUG
            msg_Dbg( &sys.demuxer, "|   |   + Seek" );
#endif
            ep->Down();
            while( ( l = ep->Get() ) != NULL )
            {
                if( MKV_IS_ID( l, KaxSeekID ) )
                {
                    KaxSeekID &sid = *(KaxSeekID*)l;
                    sid.ReadData( es.I_O() );
                    id = EbmlId( sid.GetBuffer(), sid.GetSize() );
                }
                else if( MKV_IS_ID( l, KaxSeekPosition ) )
                {
                    KaxSeekPosition &spos = *(KaxSeekPosition*)l;
                    spos.ReadData( es.I_O() );
                    i_pos = (int64_t)segment->GetGlobalPosition( uint64( spos ) );
                }
                else
                {
                    /* Many mkvmerge files hit this case. It seems to be a broken SeekHead */
                    msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid(*l).name()  );
                }
            }
            ep->Up();

            if( i_pos >= 0 )
            {
                if( id == EBML_ID(KaxCues) )
                {
                    msg_Dbg( &sys.demuxer, "|   - cues at %"PRId64, i_pos );
                    LoadSeekHeadItem( EBML_INFO(KaxCues), i_pos );
                }
                else if( id == EBML_ID(KaxInfo) )
                {
                    msg_Dbg( &sys.demuxer, "|   - info at %"PRId64, i_pos );
                    LoadSeekHeadItem( EBML_INFO(KaxInfo), i_pos );
                }
                else if( id == EBML_ID(KaxChapters) )
                {
                    msg_Dbg( &sys.demuxer, "|   - chapters at %"PRId64, i_pos );
                    LoadSeekHeadItem( EBML_INFO(KaxChapters), i_pos );
                }
                else if( id == EBML_ID(KaxTags) )
                {
                    msg_Dbg( &sys.demuxer, "|   - tags at %"PRId64, i_pos );
                    LoadSeekHeadItem( EBML_INFO(KaxTags), i_pos );
                }
                else if( id == EBML_ID(KaxSeekHead) )
                {
                    msg_Dbg( &sys.demuxer, "|   - chained seekhead at %"PRId64, i_pos );
                    LoadSeekHeadItem( EBML_INFO(KaxSeekHead), i_pos );
                }
                else if( id == EBML_ID(KaxTracks) )
                {
                    msg_Dbg( &sys.demuxer, "|   - tracks at %"PRId64, i_pos );
                    LoadSeekHeadItem( EBML_INFO(KaxTracks), i_pos );
                }
                else if( id == EBML_ID(KaxAttachments) )
                {
                    msg_Dbg( &sys.demuxer, "|   - attachments at %"PRId64, i_pos );
                    LoadSeekHeadItem( EBML_INFO(KaxAttachments), i_pos );
                }
#ifdef MKV_DEBUG
                else
                    msg_Dbg( &sys.demuxer, "|   - unknown seekhead reference at %"PRId64, i_pos );
#endif
            }
        }
        else
            msg_Dbg( &sys.demuxer, "|   |   + ParseSeekHead Unknown (%s)", typeid(*l).name() );
    }
    delete ep;
}


/**
 * Helper function to print the mkv parse tree
 */
static void MkvTree( demux_t & demuxer, int i_level, const char *psz_format, ... )
{
    va_list args;
    if( i_level > 9 )
    {
        msg_Err( &demuxer, "MKV tree is too deep" );
        return;
    }
    va_start( args, psz_format );
    static const char psz_foo[] = "|   |   |   |   |   |   |   |   |   |";
    char *psz_foo2 = (char*)malloc( i_level * 4 + 3 + strlen( psz_format ) );
    strncpy( psz_foo2, psz_foo, 4 * i_level );
    psz_foo2[ 4 * i_level ] = '+';
    psz_foo2[ 4 * i_level + 1 ] = ' ';
    strcpy( &psz_foo2[ 4 * i_level + 2 ], psz_format );
    msg_GenericVa( &demuxer,VLC_MSG_DBG, "mkv", psz_foo2, args );
    free( psz_foo2 );
    va_end( args );
}


/*****************************************************************************
 * ParseTrackEntry:
 *****************************************************************************/
void matroska_segment_c::ParseTrackEntry( KaxTrackEntry *m )
{
    bool bSupported = true;

    /* Init the track */
    mkv_track_t *tk = new mkv_track_t();
    memset( tk, 0, sizeof( mkv_track_t ) );

    es_format_Init( &tk->fmt, UNKNOWN_ES, 0 );
    tk->fmt.psz_language       = strdup("English");
    tk->fmt.psz_description    = NULL;

    tk->b_default              = true;
    tk->b_enabled              = true;
    tk->b_forced               = false;
    tk->b_silent               = false;
    tk->i_number               = tracks.size() - 1;
    tk->i_extra_data           = 0;
    tk->p_extra_data           = NULL;
    tk->psz_codec              = NULL;
    tk->b_dts_only             = false;
    tk->i_default_duration     = 0;
    tk->f_timecodescale        = 1.0;

    tk->b_inited               = false;
    tk->i_data_init            = 0;
    tk->p_data_init            = NULL;

    tk->psz_codec_name         = NULL;
    tk->psz_codec_settings     = NULL;
    tk->psz_codec_info_url     = NULL;
    tk->psz_codec_download_url = NULL;

    tk->i_compression_type     = MATROSKA_COMPRESSION_NONE;
    tk->p_compression_data     = NULL;

    msg_Dbg( &sys.demuxer, "|   |   + Track Entry" );

    for( size_t i = 0; i < m->ListSize(); i++ )
    {
        EbmlElement *l = (*m)[i];

        if( MKV_IS_ID( l, KaxTrackNumber ) )
        {
            KaxTrackNumber &tnum = *(KaxTrackNumber*)l;

            tk->i_number = uint32( tnum );
            msg_Dbg( &sys.demuxer, "|   |   |   + Track Number=%u", uint32( tnum ) );
        }
        else  if( MKV_IS_ID( l, KaxTrackUID ) )
        {
            KaxTrackUID &tuid = *(KaxTrackUID*)l;

            msg_Dbg( &sys.demuxer, "|   |   |   + Track UID=%u",  uint32( tuid ) );
        }
        else  if( MKV_IS_ID( l, KaxTrackType ) )
        {
            const char *psz_type;
            KaxTrackType &ttype = *(KaxTrackType*)l;

            switch( uint8(ttype) )
            {
                case track_audio:
                    psz_type = "audio";
                    tk->fmt.i_cat = AUDIO_ES;
                    break;
                case track_video:
                    psz_type = "video";
                    tk->fmt.i_cat = VIDEO_ES;
                    break;
                case track_subtitle:
                    psz_type = "subtitle";
                    tk->fmt.i_cat = SPU_ES;
                    break;
                case track_buttons:
                    psz_type = "buttons";
                    tk->fmt.i_cat = SPU_ES;
                    break;
                default:
                    psz_type = "unknown";
                    tk->fmt.i_cat = UNKNOWN_ES;
                    break;
            }

            msg_Dbg( &sys.demuxer, "|   |   |   + Track Type=%s", psz_type );
        }
        else  if( MKV_IS_ID( l, KaxTrackFlagEnabled ) ) // UNUSED
        {
            KaxTrackFlagEnabled &fenb = *(KaxTrackFlagEnabled*)l;

            tk->b_enabled = uint32( fenb );
            msg_Dbg( &sys.demuxer, "|   |   |   + Track Enabled=%u", uint32( fenb ) );
        }
        else  if( MKV_IS_ID( l, KaxTrackFlagDefault ) )
        {
            KaxTrackFlagDefault &fdef = *(KaxTrackFlagDefault*)l;

            tk->b_default = uint32( fdef );
            msg_Dbg( &sys.demuxer, "|   |   |   + Track Default=%u", uint32( fdef ) );
        }
        else  if( MKV_IS_ID( l, KaxTrackFlagForced ) ) // UNUSED
        {
            KaxTrackFlagForced &ffor = *(KaxTrackFlagForced*)l;
            tk->b_forced = uint32( ffor );

            msg_Dbg( &sys.demuxer, "|   |   |   + Track Forced=%u", uint32( ffor ) );
        }
        else  if( MKV_IS_ID( l, KaxTrackFlagLacing ) ) // UNUSED
        {
            KaxTrackFlagLacing &lac = *(KaxTrackFlagLacing*)l;

            msg_Dbg( &sys.demuxer, "|   |   |   + Track Lacing=%d", uint32( lac ) );
        }
        else  if( MKV_IS_ID( l, KaxTrackMinCache ) ) // UNUSED
        {
            KaxTrackMinCache &cmin = *(KaxTrackMinCache*)l;

            msg_Dbg( &sys.demuxer, "|   |   |   + Track MinCache=%d", uint32( cmin ) );
        }
        else  if( MKV_IS_ID( l, KaxTrackMaxCache ) ) // UNUSED
        {
            KaxTrackMaxCache &cmax = *(KaxTrackMaxCache*)l;

            msg_Dbg( &sys.demuxer, "|   |   |   + Track MaxCache=%d", uint32( cmax ) );
        }
        else  if( MKV_IS_ID( l, KaxTrackDefaultDuration ) )
        {
            KaxTrackDefaultDuration &defd = *(KaxTrackDefaultDuration*)l;

            tk->i_default_duration = uint64(defd);
            msg_Dbg( &sys.demuxer, "|   |   |   + Track Default Duration=%"PRId64, uint64(defd) );
        }
        else  if( MKV_IS_ID( l, KaxTrackTimecodeScale ) )
        {
            KaxTrackTimecodeScale &ttcs = *(KaxTrackTimecodeScale*)l;

            tk->f_timecodescale = float( ttcs );
            msg_Dbg( &sys.demuxer, "|   |   |   + Track TimeCodeScale=%f", tk->f_timecodescale );
        }
        else  if( MKV_IS_ID( l, KaxMaxBlockAdditionID ) ) // UNUSED
        {
            KaxMaxBlockAdditionID &mbl = *(KaxMaxBlockAdditionID*)l;

            msg_Dbg( &sys.demuxer, "|   |   |   + Track Max BlockAdditionID=%d", uint32( mbl ) );
        }
        else if( MKV_IS_ID( l, KaxTrackName ) )
        {
            KaxTrackName &tname = *(KaxTrackName*)l;

            tk->fmt.psz_description = ToUTF8( UTFstring( tname ) );
            msg_Dbg( &sys.demuxer, "|   |   |   + Track Name=%s", tk->fmt.psz_description );
        }
        else  if( MKV_IS_ID( l, KaxTrackLanguage ) )
        {
            KaxTrackLanguage &lang = *(KaxTrackLanguage*)l;

            free( tk->fmt.psz_language );
            tk->fmt.psz_language = strdup( string( lang ).c_str() );
            msg_Dbg( &sys.demuxer,
                     "|   |   |   + Track Language=`%s'", tk->fmt.psz_language );
        }
        else  if( MKV_IS_ID( l, KaxCodecID ) )
        {
            KaxCodecID &codecid = *(KaxCodecID*)l;

            tk->psz_codec = strdup( string( codecid ).c_str() );
            msg_Dbg( &sys.demuxer, "|   |   |   + Track CodecId=%s", string( codecid ).c_str() );
        }
        else  if( MKV_IS_ID( l, KaxCodecPrivate ) )
        {
            KaxCodecPrivate &cpriv = *(KaxCodecPrivate*)l;

            tk->i_extra_data = cpriv.GetSize();
            if( tk->i_extra_data > 0 )
            {
                tk->p_extra_data = (uint8_t*)malloc( tk->i_extra_data );
                memcpy( tk->p_extra_data, cpriv.GetBuffer(), tk->i_extra_data );
            }
            msg_Dbg( &sys.demuxer, "|   |   |   + Track CodecPrivate size=%"PRId64, cpriv.GetSize() );
        }
        else if( MKV_IS_ID( l, KaxCodecName ) )
        {
            KaxCodecName &cname = *(KaxCodecName*)l;

            tk->psz_codec_name = ToUTF8( UTFstring( cname ) );
            msg_Dbg( &sys.demuxer, "|   |   |   + Track Codec Name=%s", tk->psz_codec_name );
        }
        //AttachmentLink
        else if( MKV_IS_ID( l, KaxCodecDecodeAll ) ) // UNUSED
        {
            KaxCodecDecodeAll &cdall = *(KaxCodecDecodeAll*)l;

            msg_Dbg( &sys.demuxer, "|   |   |   + Track Codec Decode All=%u", uint8( cdall ) );
        }
        else if( MKV_IS_ID( l, KaxTrackOverlay ) ) // UNUSED
        {
            KaxTrackOverlay &tovr = *(KaxTrackOverlay*)l;

            msg_Dbg( &sys.demuxer, "|   |   |   + Track Overlay=%u", uint32( tovr ) );
        }
        else if( MKV_IS_ID( l, KaxContentEncodings ) )
        {
            EbmlMaster *cencs = static_cast<EbmlMaster*>(l);
            MkvTree( sys.demuxer, 3, "Content Encodings" );
            if ( cencs->ListSize() > 1 )
            {
                msg_Err( &sys.demuxer, "Multiple Compression method not supported" );
                bSupported = false;
            }
            for( size_t j = 0; j < cencs->ListSize(); j++ )
            {
                EbmlElement *l2 = (*cencs)[j];
                if( MKV_IS_ID( l2, KaxContentEncoding ) )
                {
                    MkvTree( sys.demuxer, 4, "Content Encoding" );
                    EbmlMaster *cenc = static_cast<EbmlMaster*>(l2);
                    for( size_t k = 0; k < cenc->ListSize(); k++ )
                    {
                        EbmlElement *l3 = (*cenc)[k];
                        if( MKV_IS_ID( l3, KaxContentEncodingOrder ) )
                        {
                            KaxContentEncodingOrder &encord = *(KaxContentEncodingOrder*)l3;
                            MkvTree( sys.demuxer, 5, "Order: %i", uint32( encord ) );
                        }
                        else if( MKV_IS_ID( l3, KaxContentEncodingScope ) )
                        {
                            KaxContentEncodingScope &encscope = *(KaxContentEncodingScope*)l3;
                            MkvTree( sys.demuxer, 5, "Scope: %i", uint32( encscope ) );
                        }
                        else if( MKV_IS_ID( l3, KaxContentEncodingType ) )
                        {
                            KaxContentEncodingType &enctype = *(KaxContentEncodingType*)l3;
                            MkvTree( sys.demuxer, 5, "Type: %i", uint32( enctype ) );
                        }
                        else if( MKV_IS_ID( l3, KaxContentCompression ) )
                        {
                            EbmlMaster *compr = static_cast<EbmlMaster*>(l3);
                            MkvTree( sys.demuxer, 5, "Content Compression" );
                            //Default compression type is 0 (Zlib)
                            tk->i_compression_type = MATROSKA_COMPRESSION_ZLIB;
                            for( size_t n = 0; n < compr->ListSize(); n++ )
                            {
                                EbmlElement *l4 = (*compr)[n];
                                if( MKV_IS_ID( l4, KaxContentCompAlgo ) )
                                {
                                    KaxContentCompAlgo &compalg = *(KaxContentCompAlgo*)l4;
                                    MkvTree( sys.demuxer, 6, "Compression Algorithm: %i", uint32(compalg) );
                                    tk->i_compression_type = uint32( compalg );
                                    if ( ( tk->i_compression_type != MATROSKA_COMPRESSION_ZLIB ) &&
                                         ( tk->i_compression_type != MATROSKA_COMPRESSION_HEADER ) )
                                    {
                                        msg_Err( &sys.demuxer, "Track Compression method %d not supported", tk->i_compression_type );
                                        bSupported = false;
                                    }
                                }
                                else if( MKV_IS_ID( l4, KaxContentCompSettings ) )
                                {
                                    tk->p_compression_data = new KaxContentCompSettings( *(KaxContentCompSettings*)l4 );
                                }
                                else
                                {
                                    MkvTree( sys.demuxer, 6, "Unknown (%s)", typeid(*l4).name() );
                                }
                            }
                        }
                        // ContentEncryption Unsupported
                        else
                        {
                            MkvTree( sys.demuxer, 5, "Unknown (%s)", typeid(*l3).name() );
                        }
                    }
                }
                else
                {
                    MkvTree( sys.demuxer, 4, "Unknown (%s)", typeid(*l2).name() );
                }
            }
        }
//        else if( MKV_IS_ID( l, KaxCodecSettings) ) DEPRECATED by matroska
//        {
//            KaxCodecSettings &cset = *(KaxCodecSettings*)l;

//            tk->psz_codec_settings = ToUTF8( UTFstring( cset ) );
//            msg_Dbg( &sys.demuxer, "|   |   |   + Track Codec Settings=%s", tk->psz_codec_settings );
//        }
//        else if( MKV_IS_ID( l, KaxCodecInfoURL) ) DEPRECATED by matroska
//        {
//            KaxCodecInfoURL &ciurl = *(KaxCodecInfoURL*)l;

//            tk->psz_codec_info_url = strdup( string( ciurl ).c_str() );
//            msg_Dbg( &sys.demuxer, "|   |   |   + Track Codec Info URL=%s", tk->psz_codec_info_url );
//        }
//        else if( MKV_IS_ID( l, KaxCodecDownloadURL) ) DEPRECATED by matroska
//        {
//            KaxCodecDownloadURL &cdurl = *(KaxCodecDownloadURL*)l;

//            tk->psz_codec_download_url = strdup( string( cdurl ).c_str() );
//            msg_Dbg( &sys.demuxer, "|   |   |   + Track Codec Info URL=%s", tk->psz_codec_download_url );
//        }
        else  if( MKV_IS_ID( l, KaxTrackVideo ) )
        {
            EbmlMaster *tkv = static_cast<EbmlMaster*>(l);
            unsigned int i_crop_right = 0, i_crop_left = 0, i_crop_top = 0, i_crop_bottom = 0;
            unsigned int i_display_unit = 0, i_display_width = 0, i_display_height = 0;

            msg_Dbg( &sys.demuxer, "|   |   |   + Track Video" );
            tk->f_fps = 0.0;

            tk->fmt.video.i_frame_rate_base = (unsigned int)(tk->i_default_duration / 1000);
            tk->fmt.video.i_frame_rate = 1000000;

            for( unsigned int j = 0; j < tkv->ListSize(); j++ )
            {
                EbmlElement *l = (*tkv)[j];
                if( MKV_IS_ID( l, KaxVideoFlagInterlaced ) ) // UNUSED
                {
                    KaxVideoFlagInterlaced &fint = *(KaxVideoFlagInterlaced*)l;

                    msg_Dbg( &sys.demuxer, "|   |   |   |   + Track Video Interlaced=%u", uint8( fint ) );
                }
                else if( MKV_IS_ID( l, KaxVideoStereoMode ) ) // UNUSED
                {
                    KaxVideoStereoMode &stereo = *(KaxVideoStereoMode*)l;

                    msg_Dbg( &sys.demuxer, "|   |   |   |   + Track Video Stereo Mode=%u", uint8( stereo ) );
                }
                else if( MKV_IS_ID( l, KaxVideoPixelWidth ) )
                {
                    KaxVideoPixelWidth &vwidth = *(KaxVideoPixelWidth*)l;

                    tk->fmt.video.i_width += uint16( vwidth );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + width=%d", uint16( vwidth ) );
                }
                else if( MKV_IS_ID( l, KaxVideoPixelHeight ) )
                {
                    KaxVideoPixelWidth &vheight = *(KaxVideoPixelWidth*)l;

                    tk->fmt.video.i_height += uint16( vheight );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + height=%d", uint16( vheight ) );
                }
                else if( MKV_IS_ID( l, KaxVideoDisplayWidth ) )
                {
                    KaxVideoDisplayWidth &vwidth = *(KaxVideoDisplayWidth*)l;

                    i_display_width = uint16( vwidth );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + display width=%d", uint16( vwidth ) );
                }
                else if( MKV_IS_ID( l, KaxVideoDisplayHeight ) )
                {
                    KaxVideoDisplayWidth &vheight = *(KaxVideoDisplayWidth*)l;

                    i_display_height = uint16( vheight );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + display height=%d", uint16( vheight ) );
                }
                else if( MKV_IS_ID( l, KaxVideoPixelCropBottom ) )
                {
                    KaxVideoPixelCropBottom &cropval = *(KaxVideoPixelCropBottom*)l;

                    i_crop_bottom = uint16( cropval );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + crop pixel bottom=%d", uint16( cropval ) );
                }
                else if( MKV_IS_ID( l, KaxVideoPixelCropTop ) )
                {
                    KaxVideoPixelCropTop &cropval = *(KaxVideoPixelCropTop*)l;

                    i_crop_top = uint16( cropval );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + crop pixel top=%d", uint16( cropval ) );
                }
                else if( MKV_IS_ID( l, KaxVideoPixelCropRight ) )
                {
                    KaxVideoPixelCropRight &cropval = *(KaxVideoPixelCropRight*)l;

                    i_crop_right = uint16( cropval );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + crop pixel right=%d", uint16( cropval ) );
                }
                else if( MKV_IS_ID( l, KaxVideoPixelCropLeft ) )
                {
                    KaxVideoPixelCropLeft &cropval = *(KaxVideoPixelCropLeft*)l;

                    i_crop_left = uint16( cropval );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + crop pixel left=%d", uint16( cropval ) );
                }
                else if( MKV_IS_ID( l, KaxVideoDisplayUnit ) )
                {
                    KaxVideoDisplayUnit &vdmode = *(KaxVideoDisplayUnit*)l;

                    i_display_unit = uint8( vdmode );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + Track Video Display Unit=%s",
                             i_display_unit == 0 ? "pixels" : ( i_display_unit == 1 ? "centimeters": "inches" ) );
                }
                else if( MKV_IS_ID( l, KaxVideoAspectRatio ) ) // UNUSED
                {
                    KaxVideoAspectRatio &ratio = *(KaxVideoAspectRatio*)l;

                    msg_Dbg( &sys.demuxer, "   |   |   |   + Track Video Aspect Ratio Type=%u", uint8( ratio ) );
                }
                // ColourSpace UNUSED
                else if( MKV_IS_ID( l, KaxVideoFrameRate ) )
                {
                    KaxVideoFrameRate &vfps = *(KaxVideoFrameRate*)l;

                    tk->f_fps = float( vfps );
                    msg_Dbg( &sys.demuxer, "   |   |   |   + fps=%f", float( vfps ) );
                }
//                else if( MKV_IS_ID( l, KaxVideoGamma) ) //DEPRECATED by Matroska
//                {
//                    KaxVideoGamma &gamma = *(KaxVideoGamma*)l;

//                    msg_Dbg( &sys.demuxer, "   |   |   |   + gamma=%f", float( gamma ) );
//                }
                else
                {
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + Unknown (%s)", typeid(*l).name() );
                }
            }
            if( i_display_height && i_display_width )
            {
                tk->fmt.video.i_sar_num = i_display_width  * tk->fmt.video.i_height;
                tk->fmt.video.i_sar_den = i_display_height * tk->fmt.video.i_width;
            }
            if( i_crop_left || i_crop_right || i_crop_top || i_crop_bottom )
            {
                tk->fmt.video.i_visible_width   = tk->fmt.video.i_width;
                tk->fmt.video.i_visible_height  = tk->fmt.video.i_height;
                tk->fmt.video.i_x_offset        = i_crop_left;
                tk->fmt.video.i_y_offset        = i_crop_top;
                tk->fmt.video.i_visible_width  -= i_crop_left + i_crop_right;
                tk->fmt.video.i_visible_height -= i_crop_top + i_crop_bottom;
            }
            /* FIXME: i_display_* allows you to not only set DAR, but also a zoom factor.
               we do not support this atm */
        }
        else  if( MKV_IS_ID( l, KaxTrackAudio ) )
        {
            EbmlMaster *tka = static_cast<EbmlMaster*>(l);

            /* Initialize default values */
            tk->fmt.audio.i_channels = 1;
            tk->fmt.audio.i_rate = 8000;

            msg_Dbg( &sys.demuxer, "|   |   |   + Track Audio" );

            for( unsigned int j = 0; j < tka->ListSize(); j++ )
            {
                EbmlElement *l = (*tka)[j];

                if( MKV_IS_ID( l, KaxAudioSamplingFreq ) )
                {
                    KaxAudioSamplingFreq &afreq = *(KaxAudioSamplingFreq*)l;

                    tk->i_original_rate = tk->fmt.audio.i_rate = (int)float( afreq );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + afreq=%d", tk->fmt.audio.i_rate );
                }
                else if( MKV_IS_ID( l, KaxAudioOutputSamplingFreq ) )
                {
                    KaxAudioOutputSamplingFreq &afreq = *(KaxAudioOutputSamplingFreq*)l;

                    tk->fmt.audio.i_rate = (int)float( afreq );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + aoutfreq=%d", tk->fmt.audio.i_rate );
                }
                else if( MKV_IS_ID( l, KaxAudioChannels ) )
                {
                    KaxAudioChannels &achan = *(KaxAudioChannels*)l;

                    tk->fmt.audio.i_channels = uint8( achan );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + achan=%u", uint8( achan ) );
                }
                else if( MKV_IS_ID( l, KaxAudioBitDepth ) )
                {
                    KaxAudioBitDepth &abits = *(KaxAudioBitDepth*)l;

                    tk->fmt.audio.i_bitspersample = uint8( abits );
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + abits=%u", uint8( abits ) );
                }
                else
                {
                    msg_Dbg( &sys.demuxer, "|   |   |   |   + Unknown (%s)", typeid(*l).name() );
                }
            }
        }
        else
        {
            msg_Dbg( &sys.demuxer, "|   |   |   + Unknown (%s)",
                     typeid(*l).name() );
        }
    }

    if ( bSupported )
    {
        tracks.push_back( tk );
    }
    else
    {
        msg_Err( &sys.demuxer, "Track Entry %d not supported", tk->i_number );
        delete tk;
    }
}

/*****************************************************************************
 * ParseTracks:
 *****************************************************************************/
void matroska_segment_c::ParseTracks( KaxTracks *tracks )
{
    EbmlElement *el;
    int i_upper_level = 0;

    /* Master elements */
    tracks->Read( es, EBML_CONTEXT(tracks), i_upper_level, el, true );

    for( size_t i = 0; i < tracks->ListSize(); i++ )
    {
        EbmlElement *l = (*tracks)[i];

        if( MKV_IS_ID( l, KaxTrackEntry ) )
        {
            ParseTrackEntry( static_cast<KaxTrackEntry *>(l) );
        }
        else
        {
            msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid(*l).name() );
        }
    }
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
    m->Read( es, EBML_CONTEXT(info), i_upper_level, el, true );

    for( size_t i = 0; i < m->ListSize(); i++ )
    {
        EbmlElement *l = (*m)[i];

        if( MKV_IS_ID( l, KaxSegmentUID ) )
        {
            if ( p_segment_uid == NULL )
                p_segment_uid = new KaxSegmentUID(*static_cast<KaxSegmentUID*>(l));

            msg_Dbg( &sys.demuxer, "|   |   + UID=%d", *(uint32*)p_segment_uid->GetBuffer() );
        }
        else if( MKV_IS_ID( l, KaxPrevUID ) )
        {
            if ( p_prev_segment_uid == NULL )
            {
                p_prev_segment_uid = new KaxPrevUID(*static_cast<KaxPrevUID*>(l));
                b_ref_external_segments = true;
            }

            msg_Dbg( &sys.demuxer, "|   |   + PrevUID=%d", *(uint32*)p_prev_segment_uid->GetBuffer() );
        }
        else if( MKV_IS_ID( l, KaxNextUID ) )
        {
            if ( p_next_segment_uid == NULL )
            {
                p_next_segment_uid = new KaxNextUID(*static_cast<KaxNextUID*>(l));
                b_ref_external_segments = true;
            }

            msg_Dbg( &sys.demuxer, "|   |   + NextUID=%d", *(uint32*)p_next_segment_uid->GetBuffer() );
        }
        else if( MKV_IS_ID( l, KaxTimecodeScale ) )
        {
            KaxTimecodeScale &tcs = *(KaxTimecodeScale*)l;

            i_timescale = uint64(tcs);

            msg_Dbg( &sys.demuxer, "|   |   + TimecodeScale=%"PRId64,
                     i_timescale );
        }
        else if( MKV_IS_ID( l, KaxDuration ) )
        {
            KaxDuration &dur = *(KaxDuration*)l;

            i_duration = mtime_t( double( dur ) );

            msg_Dbg( &sys.demuxer, "|   |   + Duration=%"PRId64,
                     i_duration );
        }
        else if( MKV_IS_ID( l, KaxMuxingApp ) )
        {
            KaxMuxingApp &mapp = *(KaxMuxingApp*)l;

            psz_muxing_application = ToUTF8( UTFstring( mapp ) );

            msg_Dbg( &sys.demuxer, "|   |   + Muxing Application=%s",
                     psz_muxing_application );
        }
        else if( MKV_IS_ID( l, KaxWritingApp ) )
        {
            KaxWritingApp &wapp = *(KaxWritingApp*)l;

            psz_writing_application = ToUTF8( UTFstring( wapp ) );

            msg_Dbg( &sys.demuxer, "|   |   + Writing Application=%s",
                     psz_writing_application );
        }
        else if( MKV_IS_ID( l, KaxSegmentFilename ) )
        {
            KaxSegmentFilename &sfn = *(KaxSegmentFilename*)l;

            psz_segment_filename = ToUTF8( UTFstring( sfn ) );

            msg_Dbg( &sys.demuxer, "|   |   + Segment Filename=%s",
                     psz_segment_filename );
        }
        else if( MKV_IS_ID( l, KaxTitle ) )
        {
            KaxTitle &title = *(KaxTitle*)l;

            psz_title = ToUTF8( UTFstring( title ) );

            msg_Dbg( &sys.demuxer, "|   |   + Title=%s", psz_title );
        }
        else if( MKV_IS_ID( l, KaxSegmentFamily ) )
        {
            KaxSegmentFamily *uid = static_cast<KaxSegmentFamily*>(l);

            families.push_back( new KaxSegmentFamily(*uid) );

            msg_Dbg( &sys.demuxer, "|   |   + family=%d", *(uint32*)uid->GetBuffer() );
        }
        else if( MKV_IS_ID( l, KaxDateUTC ) )
        {
            KaxDateUTC &date = *(KaxDateUTC*)l;
            time_t i_date;
            struct tm tmres;
            char   buffer[25];

            i_date = date.GetEpochDate();
            if( gmtime_r( &i_date, &tmres ) &&
                strftime( buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y",
                          &tmres ) )
            {
                psz_date_utc = strdup( buffer );
                msg_Dbg( &sys.demuxer, "|   |   + Date=%s", buffer );
            }
        }
        else if( MKV_IS_ID( l, KaxChapterTranslate ) )
        {
            KaxChapterTranslate *p_trans = static_cast<KaxChapterTranslate*>( l );
            chapter_translation_c *p_translate = new chapter_translation_c();

            p_trans->Read( es, EBML_CONTEXT(p_trans), i_upper_level, el, true );
            for( size_t j = 0; j < p_trans->ListSize(); j++ )
            {
                EbmlElement *l = (*p_trans)[j];

                if( MKV_IS_ID( l, KaxChapterTranslateEditionUID ) )
                {
                    p_translate->editions.push_back( uint64( *static_cast<KaxChapterTranslateEditionUID*>( l ) ) );
                }
                else if( MKV_IS_ID( l, KaxChapterTranslateCodec ) )
                {
                    p_translate->codec_id = uint32( *static_cast<KaxChapterTranslateCodec*>( l ) );
                }
                else if( MKV_IS_ID( l, KaxChapterTranslateID ) )
                {
                    p_translate->p_translated = new KaxChapterTranslateID( *static_cast<KaxChapterTranslateID*>( l ) );
                }
            }

            translations.push_back( p_translate );
        }
        else
        {
            msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid(*l).name() );
        }
    }

    double f_dur = double(i_duration) * double(i_timescale) / 1000000.0;
    i_duration = mtime_t(f_dur);
    if( !i_duration ) i_duration = -1;
}


/*****************************************************************************
 * ParseChapterAtom
 *****************************************************************************/
void matroska_segment_c::ParseChapterAtom( int i_level, KaxChapterAtom *ca, chapter_item_c & chapters )
{
    msg_Dbg( &sys.demuxer, "|   |   |   + ChapterAtom (level=%d)", i_level );
    for( size_t i = 0; i < ca->ListSize(); i++ )
    {
        EbmlElement *l = (*ca)[i];

        if( MKV_IS_ID( l, KaxChapterUID ) )
        {
            chapters.i_uid = uint64_t(*(KaxChapterUID*)l);
            msg_Dbg( &sys.demuxer, "|   |   |   |   + ChapterUID: %"PRIu64"", chapters.i_uid );
        }
        else if( MKV_IS_ID( l, KaxChapterFlagHidden ) )
        {
            KaxChapterFlagHidden &flag =*(KaxChapterFlagHidden*)l;
            chapters.b_display_seekpoint = uint8( flag ) == 0;

            msg_Dbg( &sys.demuxer, "|   |   |   |   + ChapterFlagHidden: %s", chapters.b_display_seekpoint ? "no":"yes" );
        }
        else if( MKV_IS_ID( l, KaxChapterSegmentUID ) )
        {
            chapters.p_segment_uid = new KaxChapterSegmentUID( *static_cast<KaxChapterSegmentUID*>(l) );
            b_ref_external_segments = true;
            msg_Dbg( &sys.demuxer, "|   |   |   |   + ChapterSegmentUID= %u", *(uint32*)chapters.p_segment_uid->GetBuffer() );
        }
        else if( MKV_IS_ID( l, KaxChapterSegmentEditionUID ) )
        {
            chapters.p_segment_edition_uid = new KaxChapterSegmentEditionUID( *static_cast<KaxChapterSegmentEditionUID*>(l) );
            msg_Dbg( &sys.demuxer, "|   |   |   |   + ChapterSegmentEditionUID= %u",
#if LIBMATROSKA_VERSION < 0x010300
            *(uint32*)chapters.p_segment_edition_uid->GetBuffer()
#else
            *(uint32*)chapters.p_segment_edition_uid
#endif
            );
        }
        else if( MKV_IS_ID( l, KaxChapterTimeStart ) )
        {
            KaxChapterTimeStart &start =*(KaxChapterTimeStart*)l;
            chapters.i_start_time = uint64( start ) / INT64_C(1000);

            msg_Dbg( &sys.demuxer, "|   |   |   |   + ChapterTimeStart: %"PRId64"", chapters.i_start_time );
        }
        else if( MKV_IS_ID( l, KaxChapterTimeEnd ) )
        {
            KaxChapterTimeEnd &end =*(KaxChapterTimeEnd*)l;
            chapters.i_end_time = uint64( end ) / INT64_C(1000);

            msg_Dbg( &sys.demuxer, "|   |   |   |   + ChapterTimeEnd: %"PRId64"", chapters.i_end_time );
        }
        else if( MKV_IS_ID( l, KaxChapterDisplay ) )
        {
            EbmlMaster *cd = static_cast<EbmlMaster *>(l);

            msg_Dbg( &sys.demuxer, "|   |   |   |   + ChapterDisplay" );
            for( size_t j = 0; j < cd->ListSize(); j++ )
            {
                EbmlElement *l= (*cd)[j];

                if( MKV_IS_ID( l, KaxChapterString ) )
                {
                    KaxChapterString &name =*(KaxChapterString*)l;
                    for ( int k = 0; k < i_level; k++)
                        chapters.psz_name += '+';
                    chapters.psz_name += ' ';
                    char *psz_tmp_utf8 = ToUTF8( UTFstring( name ) );
                    chapters.psz_name += psz_tmp_utf8;
                    chapters.b_user_display = true;

                    msg_Dbg( &sys.demuxer, "|   |   |   |   |    + ChapterString '%s'", psz_tmp_utf8 );
                    free( psz_tmp_utf8 );
                }
                else if( MKV_IS_ID( l, KaxChapterLanguage ) )
                {
                    KaxChapterLanguage &lang =*(KaxChapterLanguage*)l;
                    const char *psz = string( lang ).c_str();

                    msg_Dbg( &sys.demuxer, "|   |   |   |   |    + ChapterLanguage '%s'", psz );
                }
                else if( MKV_IS_ID( l, KaxChapterCountry ) )
                {
                    KaxChapterCountry &ct =*(KaxChapterCountry*)l;
                    const char *psz = string( ct ).c_str();

                    msg_Dbg( &sys.demuxer, "|   |   |   |   |    + ChapterCountry '%s'", psz );
                }
            }
        }
        else if( MKV_IS_ID( l, KaxChapterProcess ) )
        {
            msg_Dbg( &sys.demuxer, "|   |   |   |   + ChapterProcess" );

            KaxChapterProcess *cp = static_cast<KaxChapterProcess *>(l);
            chapter_codec_cmds_c *p_ccodec = NULL;

            for( size_t j = 0; j < cp->ListSize(); j++ )
            {
                EbmlElement *k= (*cp)[j];

                if( MKV_IS_ID( k, KaxChapterProcessCodecID ) )
                {
                    KaxChapterProcessCodecID *p_codec_id = static_cast<KaxChapterProcessCodecID*>( k );
                    if ( uint32(*p_codec_id) == 0 )
                        p_ccodec = new matroska_script_codec_c( sys );
                    else if ( uint32(*p_codec_id) == 1 )
                        p_ccodec = new dvd_chapter_codec_c( sys );
                    break;
                }
            }

            if ( p_ccodec != NULL )
            {
                for( size_t j = 0; j < cp->ListSize(); j++ )
                {
                    EbmlElement *k= (*cp)[j];

                    if( MKV_IS_ID( k, KaxChapterProcessPrivate ) )
                    {
                        KaxChapterProcessPrivate * p_private = static_cast<KaxChapterProcessPrivate*>( k );
                        p_ccodec->SetPrivate( *p_private );
                    }
                    else if( MKV_IS_ID( k, KaxChapterProcessCommand ) )
                    {
                        p_ccodec->AddCommand( *static_cast<KaxChapterProcessCommand*>( k ) );
                    }
                }
                chapters.codecs.push_back( p_ccodec );
            }
        }
        else if( MKV_IS_ID( l, KaxChapterAtom ) )
        {
            chapter_item_c *new_sub_chapter = new chapter_item_c();
            ParseChapterAtom( i_level+1, static_cast<KaxChapterAtom *>(l), *new_sub_chapter );
            new_sub_chapter->p_parent = &chapters;
            chapters.sub_chapters.push_back( new_sub_chapter );
        }
    }
}

/*****************************************************************************
 * ParseAttachments:
 *****************************************************************************/
void matroska_segment_c::ParseAttachments( KaxAttachments *attachments )
{
    EbmlElement *el;
    int i_upper_level = 0;

    attachments->Read( es, EBML_CONTEXT(attachments), i_upper_level, el, true );

    KaxAttached *attachedFile = FindChild<KaxAttached>( *attachments );

    while( attachedFile && ( attachedFile->GetSize() > 0 ) )
    {
        KaxFileData  &img_data     = GetChild<KaxFileData>( *attachedFile );
        attachment_c *new_attachment = new attachment_c( ToUTF8( UTFstring( GetChild<KaxFileName>( *attachedFile ) ) ),
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
    EbmlElement *el;
    int i_upper_level = 0;

    /* Master elements */
    chapters->Read( es, EBML_CONTEXT(chapters), i_upper_level, el, true );

    for( size_t i = 0; i < chapters->ListSize(); i++ )
    {
        EbmlElement *l = (*chapters)[i];

        if( MKV_IS_ID( l, KaxEditionEntry ) )
        {
            chapter_edition_c *p_edition = new chapter_edition_c();

            EbmlMaster *E = static_cast<EbmlMaster *>(l );
            msg_Dbg( &sys.demuxer, "|   |   + EditionEntry" );
            for( size_t j = 0; j < E->ListSize(); j++ )
            {
                EbmlElement *l = (*E)[j];

                if( MKV_IS_ID( l, KaxChapterAtom ) )
                {
                    chapter_item_c *new_sub_chapter = new chapter_item_c();
                    ParseChapterAtom( 0, static_cast<KaxChapterAtom *>(l), *new_sub_chapter );
                    p_edition->sub_chapters.push_back( new_sub_chapter );
                }
                else if( MKV_IS_ID( l, KaxEditionUID ) )
                {
                    p_edition->i_uid = uint64(*static_cast<KaxEditionUID *>( l ));
                }
                else if( MKV_IS_ID( l, KaxEditionFlagOrdered ) )
                {
                    p_edition->b_ordered = var_InheritBool( &sys.demuxer, "mkv-use-ordered-chapters" ) ? (uint8(*static_cast<KaxEditionFlagOrdered *>( l )) != 0) : 0;
                }
                else if( MKV_IS_ID( l, KaxEditionFlagDefault ) )
                {
                    if (uint8(*static_cast<KaxEditionFlagDefault *>( l )) != 0)
                        i_default_edition = stored_editions.size();
                }
                else if( MKV_IS_ID( l, KaxEditionFlagHidden ) )
                {
                    // FIXME to implement
                }
                else
                {
                    msg_Dbg( &sys.demuxer, "|   |   |   + Unknown (%s)", typeid(*l).name() );
                }
            }
            stored_editions.push_back( p_edition );
        }
        else
        {
            msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid(*l).name() );
        }
    }
}

void matroska_segment_c::ParseCluster( bool b_update_start_time )
{
    EbmlElement *el;
    EbmlMaster  *m;
    int i_upper_level = 0;

    /* Master elements */
    m = static_cast<EbmlMaster *>( cluster );
    m->Read( es, EBML_CONTEXT(cluster), i_upper_level, el, true );

    for( unsigned int i = 0; i < m->ListSize(); i++ )
    {
        EbmlElement *l = (*m)[i];

        if( MKV_IS_ID( l, KaxClusterTimecode ) )
        {
            KaxClusterTimecode &ctc = *(KaxClusterTimecode*)l;

            cluster->InitTimecode( uint64( ctc ), i_timescale );
            break;
        }
    }

    if( b_update_start_time )
        i_start_time = cluster->GlobalTimecode() / 1000;
}

