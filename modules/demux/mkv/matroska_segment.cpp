/*****************************************************************************
 * matroska_segment.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2010 VLC authors and VideoLAN
 * $Id$
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

#include "matroska_segment.hpp"
#include "chapters.hpp"
#include "demux.hpp"
#include "util.hpp"
#include "Ebml_parser.hpp"

matroska_segment_c::matroska_segment_c( demux_sys_t & demuxer, EbmlStream & estream )
    :segment(NULL)
    ,es(estream)
    ,i_timescale(MKVD_TIMECODESCALE)
    ,i_duration(-1)
    ,i_start_time(0)
    ,i_seekhead_count(0)
    ,i_seekhead_position(-1)
    ,i_cues_position(-1)
    ,i_tracks_position(-1)
    ,i_info_position(-1)
    ,i_chapters_position(-1)
    ,i_tags_position(-1)
    ,i_attachments_position(-1)
    ,cluster(NULL)
    ,i_block_pos(0)
    ,i_cluster_pos(0)
    ,i_start_pos(0)
    ,p_segment_uid(NULL)
    ,p_prev_segment_uid(NULL)
    ,p_next_segment_uid(NULL)
    ,b_cues(false)
    ,i_index(0)
    ,i_index_max(1024)
    ,psz_muxing_application(NULL)
    ,psz_writing_application(NULL)
    ,psz_segment_filename(NULL)
    ,psz_title(NULL)
    ,psz_date_utc(NULL)
    ,i_default_edition(0)
    ,sys(demuxer)
    ,ep(NULL)
    ,b_preloaded(false)
    ,b_ref_external_segments(false)
{
    p_indexes = (mkv_index_t*)malloc( sizeof( mkv_index_t ) * i_index_max );
}

matroska_segment_c::~matroska_segment_c()
{
    for( size_t i_track = 0; i_track < tracks.size(); i_track++ )
    {
        delete tracks[i_track]->p_compression_data;
        es_format_Clean( &tracks[i_track]->fmt );
        delete tracks[i_track]->p_sys;
        free( tracks[i_track]->p_extra_data );
        free( tracks[i_track]->psz_codec );
        delete tracks[i_track];
    }

    free( psz_writing_application );
    free( psz_muxing_application );
    free( psz_segment_filename );
    free( psz_title );
    free( psz_date_utc );
    free( p_indexes );

    delete ep;
    delete segment;
    delete p_segment_uid;
    delete p_prev_segment_uid;
    delete p_next_segment_uid;

    vlc_delete_all( stored_editions );
    vlc_delete_all( translations );
    vlc_delete_all( families );
}


/*****************************************************************************
 * Tools                                                                     *
 *****************************************************************************
 *  * LoadCues : load the cues element and update index
 *  * LoadTags : load ... the tags element
 *  * InformationCreate : create all information, load tags if present
 *****************************************************************************/
void matroska_segment_c::LoadCues( KaxCues *cues )
{
    bool b_invalid_cue;
    EbmlParser  *ep;
    EbmlElement *el;

    if( b_cues )
    {
        msg_Err( &sys.demuxer, "There can be only 1 Cues per section." );
        return;
    }

    ep = new EbmlParser( &es, cues, &sys.demuxer );
    while( ( el = ep->Get() ) != NULL )
    {
        if( MKV_IS_ID( el, KaxCuePoint ) )
        {
            b_invalid_cue = false;
#define idx p_indexes[i_index]

            idx.i_track       = -1;
            idx.i_block_number= -1;
            idx.i_position    = -1;
            idx.i_time        = 0;
            idx.b_key         = true;

            ep->Down();
            while( ( el = ep->Get() ) != NULL )
            {
                if( MKV_IS_ID( el, KaxCueTime ) )
                {
                    KaxCueTime &ctime = *(KaxCueTime*)el;
                    try
                    {
                        ctime.ReadData( es.I_O() );
                    }
                    catch(...)
                    {
                        msg_Err( &sys.demuxer, "Error while reading CueTime" );
                        b_invalid_cue = true;
                        break;
                    }
                    idx.i_time = uint64( ctime ) * i_timescale / (mtime_t)1000;
                }
                else if( MKV_IS_ID( el, KaxCueTrackPositions ) )
                {
                    ep->Down();
                    try
                    {
                        while( ( el = ep->Get() ) != NULL )
                        {
                            if( MKV_IS_ID( el, KaxCueTrack ) )
                            {
                                KaxCueTrack &ctrack = *(KaxCueTrack*)el;

                                ctrack.ReadData( es.I_O() );
                                idx.i_track = uint16( ctrack );
                            }
                            else if( MKV_IS_ID( el, KaxCueClusterPosition ) )
                            {
                                KaxCueClusterPosition &ccpos = *(KaxCueClusterPosition*)el;

                                ccpos.ReadData( es.I_O() );
                                idx.i_position = segment->GetGlobalPosition( uint64( ccpos ) );
                            }
                            else if( MKV_IS_ID( el, KaxCueBlockNumber ) )
                            {
                                KaxCueBlockNumber &cbnum = *(KaxCueBlockNumber*)el;

                                cbnum.ReadData( es.I_O() );
                                idx.i_block_number = uint32( cbnum );
                            }
                            else
                            {
                                msg_Dbg( &sys.demuxer, "         * Unknown (%s)", typeid(*el).name() );
                            }
                        }
                    }
                    catch(...)
                    {
                        ep->Up();   
                        msg_Err( &sys.demuxer, "Error while reading %s", typeid(*el).name() );
                        b_invalid_cue = true;
                        break;
                    }
                    ep->Up();
                }
                else
                {
                    msg_Dbg( &sys.demuxer, "     * Unknown (%s)", typeid(*el).name() );
                }
            }
            ep->Up();

#if 0
            msg_Dbg( &sys.demuxer, " * added time=%"PRId64" pos=%"PRId64
                     " track=%d bnum=%d", idx.i_time, idx.i_position,
                     idx.i_track, idx.i_block_number );
#endif
            if( likely( !b_invalid_cue ) )
            {
                i_index++;
                if( i_index >= i_index_max )
                {
                    i_index_max += 1024;
                    p_indexes = (mkv_index_t*)xrealloc( p_indexes,
                                                        sizeof( mkv_index_t ) * i_index_max );
                }
            }
#undef idx
        }
        else
        {
            msg_Dbg( &sys.demuxer, " * Unknown (%s)", typeid(*el).name() );
        }
    }
    delete ep;
    b_cues = true;
    msg_Dbg( &sys.demuxer, "|   - loading cues done." );
}


static const struct {
    vlc_meta_type_t type;
    const char *key;
    int target_type; /* 0 is valid for all target_type */
} metadata_map[] = {
                     {vlc_meta_Album,       "TITLE",         50},
                     {vlc_meta_Title,       "TITLE",         0},
                     {vlc_meta_Artist,      "ARTIST",        0},
                     {vlc_meta_Genre,       "GENRE",         0},
                     {vlc_meta_Copyright,   "COPYRIGHT",     0},
                     {vlc_meta_TrackNumber, "PART_NUMBER",   0},
                     {vlc_meta_Description, "DESCRIPTION",   0},
                     {vlc_meta_Description, "COMMENT",       0},
                     {vlc_meta_Rating,      "RATING",        0},
                     {vlc_meta_Date,        "DATE_RELEASED", 0},
                     {vlc_meta_Date,        "DATE_RELEASE",  0},
                     {vlc_meta_Date,        "DATE_RECORDED", 0},
                     {vlc_meta_URL,         "URL",           0},
                     {vlc_meta_Publisher,   "PUBLISHER",     0},
                     {vlc_meta_EncodedBy,   "ENCODED_BY",    0},
                     {vlc_meta_TrackTotal,  "TOTAL_PARTS",   0},
                     {vlc_meta_Title,       NULL,            0},
};

SimpleTag * matroska_segment_c::ParseSimpleTags( KaxTagSimple *tag, int target_type )
{
    EbmlElement *el;
    EbmlParser *ep = new EbmlParser( &es, tag, &sys.demuxer );
    SimpleTag * p_simple = new SimpleTag;

    if( !p_simple )
    {
        msg_Err( &sys.demuxer, "Couldn't allocate memory for Simple Tag... ignoring it");
        return NULL;
    }

    if( !sys.meta )
        sys.meta = vlc_meta_New();

    msg_Dbg( &sys.demuxer, "|   + Simple Tag ");
    try
    {
        while( ( el = ep->Get() ) != NULL )
        {
            if( MKV_IS_ID( el, KaxTagName ) )
            {
                KaxTagName &key = *(KaxTagName*)el;
                key.ReadData( es.I_O(), SCOPE_ALL_DATA );
                p_simple->psz_tag_name = strdup( UTFstring( key ).GetUTF8().c_str() );
            }
            else if( MKV_IS_ID( el, KaxTagString ) )
            {
                KaxTagString &value = *(KaxTagString*)el;
                value.ReadData( es.I_O(), SCOPE_ALL_DATA );
                p_simple->p_value = strdup( UTFstring( value ).GetUTF8().c_str() );
            }
            else if(  MKV_IS_ID( el, KaxTagLangue ) )
            {
                KaxTagLangue &language = *(KaxTagLangue*) el;
                language.ReadData( es.I_O(), SCOPE_ALL_DATA );
                p_simple->psz_lang = strdup( string( language ).c_str());
            }
            else if(  MKV_IS_ID( el, KaxTagDefault ) )
            {
                KaxTagDefault & dft = *(KaxTagDefault*) el;
                dft.ReadData( es.I_O(), SCOPE_ALL_DATA );
                p_simple->b_default = (bool) uint8( dft );
            }
            /*Tags can be nested*/
            else if( MKV_IS_ID( el, KaxTagSimple) )
            {
                SimpleTag * p_st = ParseSimpleTags( (KaxTagSimple*)el, target_type );
                if( p_st )
                    p_simple->sub_tags.push_back( p_st );
            }
            /*TODO Handle binary tags*/
        }
    }
    catch(...)
    {
        msg_Err( &sys.demuxer, "Error while reading Tag ");
        delete ep;
        delete p_simple;
        return NULL;
    }
    delete ep;

    if( !p_simple->psz_tag_name || !p_simple->p_value )
    {
        msg_Warn( &sys.demuxer, "Invalid MKV SimpleTag found.");
        delete p_simple;
        return NULL;
    }

    for( int i = 0; metadata_map[i].key; i++ )
    {
        if( !strcmp( p_simple->psz_tag_name, metadata_map[i].key ) &&
            (metadata_map[i].target_type == 0 || target_type == metadata_map[i].target_type ) )
        {
            vlc_meta_Set( sys.meta, metadata_map[i].type, p_simple->p_value );
            msg_Dbg( &sys.demuxer, "|   |   + Meta %s: %s", p_simple->psz_tag_name, p_simple->p_value);
            goto done;
        }
    }
    msg_Dbg( &sys.demuxer, "|   |   + Meta %s: %s", p_simple->psz_tag_name, p_simple->p_value);
    vlc_meta_AddExtra( sys.meta, p_simple->psz_tag_name, p_simple->p_value);
done:
    return p_simple;
}

#define PARSE_TAG( type ) \
    do { \
        msg_Dbg( &sys.demuxer, "|   + " type ); \
        ep->Down();                             \
        while( ( el = ep->Get() ) != NULL )     \
        {                                       \
            msg_Dbg( &sys.demuxer, "|   |   + Unknown (%s)", typeid( *el ).name() ); \
        }                                      \
        ep->Up(); } while( 0 )


void matroska_segment_c::LoadTags( KaxTags *tags )
{
    /* Master elements */
    EbmlParser *ep = new EbmlParser( &es, tags, &sys.demuxer );
    EbmlElement *el;

    while( ( el = ep->Get() ) != NULL )
    {
        if( MKV_IS_ID( el, KaxTag ) )
        {
            Tag * p_tag = new Tag;
            if(!p_tag)
            {
                msg_Err( &sys.demuxer,"Couldn't allocate memory for tag... ignoring it");
                continue;
            }
            msg_Dbg( &sys.demuxer, "+ Tag" );
            ep->Down();
            int target_type = 50;
            while( ( el = ep->Get() ) != NULL )
            {
                if( MKV_IS_ID( el, KaxTagTargets ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Targets" );
                    ep->Down();
                    while( ( el = ep->Get() ) != NULL )
                    {
                        try
                        {
                            if( MKV_IS_ID( el, KaxTagTargetTypeValue ) )
                            {
                                KaxTagTargetTypeValue &value = *(KaxTagTargetTypeValue*)el;
                                value.ReadData( es.I_O() );

                                msg_Dbg( &sys.demuxer, "|   |   + TargetTypeValue: %u", uint32(value));
                                target_type = uint32(value);
                            }
                            if( MKV_IS_ID( el, KaxTagTrackUID ) )
                            {
                                p_tag->i_tag_type = TRACK_UID;
                                KaxTagTrackUID &uid = *(KaxTagTrackUID*) el;
                                uid.ReadData( es.I_O() );
                                p_tag->i_uid = uint64( uid );
                                msg_Dbg( &sys.demuxer, "|   |   + TrackUID: %"PRIu64, p_tag->i_uid);

                            }
                            if( MKV_IS_ID( el, KaxTagEditionUID ) )
                            {
                                p_tag->i_tag_type = EDITION_UID;
                                KaxTagEditionUID &uid = *(KaxTagEditionUID*) el;
                                uid.ReadData( es.I_O() );
                                p_tag->i_uid = uint64( uid );
                                msg_Dbg( &sys.demuxer, "|   |   + EditionUID: %"PRIu64, p_tag->i_uid);
                            }
                            if( MKV_IS_ID( el, KaxTagChapterUID ) )
                            {
                                p_tag->i_tag_type = CHAPTER_UID;
                                KaxTagChapterUID &uid = *(KaxTagChapterUID*) el;
                                uid.ReadData( es.I_O() );
                                p_tag->i_uid = uint64( uid );
                                msg_Dbg( &sys.demuxer, "|   |   + ChapterUID: %"PRIu64, p_tag->i_uid);
                            }
                            if( MKV_IS_ID( el, KaxTagAttachmentUID ) )
                            {
                                p_tag->i_tag_type = ATTACHMENT_UID;
                                KaxTagAttachmentUID &uid = *(KaxTagAttachmentUID*) el;
                                uid.ReadData( es.I_O() );
                                p_tag->i_uid = uint64( uid );
                                msg_Dbg( &sys.demuxer, "|   |   + AttachmentUID: %"PRIu64, p_tag->i_uid);
                            }
                        }
                        catch(...)
                        {
                            msg_Err( &sys.demuxer, "Error while reading tag");
                            ep->Up();
                            break;
                        }
                        ep->Up();
                    }
                }
                else if( MKV_IS_ID( el, KaxTagSimple ) )
                {
                    SimpleTag * p_simple =
                        ParseSimpleTags( static_cast<KaxTagSimple*>( el ),
                                         target_type );
                    if( p_simple )
                        p_tag->simple_tags.push_back( p_simple );
                }
#if 0 // not valid anymore
                else if( MKV_IS_ID( el, KaxTagGeneral ) )
                    PARSE_TAG( "General" );
                else if( MKV_IS_ID( el, KaxTagGenres ) )
                    PARSE_TAG( "Genres" );
                else if( MKV_IS_ID( el, KaxTagAudioSpecific ) )
                    PARSE_TAG( "Audio Specific" );
                else if( MKV_IS_ID( el, KaxTagImageSpecific ) )
                    PARSE_TAG( "Images Specific" );
                else if( MKV_IS_ID( el, KaxTagMultiComment ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Multi Comment" );
                }
                else if( MKV_IS_ID( el, KaxTagMultiCommercial ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Multi Commercial" );
                }
                else if( MKV_IS_ID( el, KaxTagMultiDate ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Multi Date" );
                }
                else if( MKV_IS_ID( el, KaxTagMultiEntity ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Multi Entity" );
                }
                else if( MKV_IS_ID( el, KaxTagMultiIdentifier ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Multi Identifier" );
                }
                else if( MKV_IS_ID( el, KaxTagMultiLegal ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Multi Legal" );
                }
                else if( MKV_IS_ID( el, KaxTagMultiTitle ) )
                {
                    msg_Dbg( &sys.demuxer, "|   + Multi Title" );
                }
#endif
                else
                {
                    msg_Dbg( &sys.demuxer, "|   + LoadTag Unknown (%s)", typeid( *el ).name() );
                }
            }
            ep->Up();
            this->tags.push_back(p_tag);
        }
        else
        {
            msg_Dbg( &sys.demuxer, "+ Unknown (%s)", typeid( *el ).name() );
        }
    }
    delete ep;

    msg_Dbg( &sys.demuxer, "loading tags done." );
}
#undef PARSE_TAG

/*****************************************************************************
 * InformationCreate:
 *****************************************************************************/
void matroska_segment_c::InformationCreate( )
{
    if( !sys.meta )
        sys.meta = vlc_meta_New();

    if( psz_title )
    {
        vlc_meta_SetTitle( sys.meta, psz_title );
    }
#if 0
    if( psz_date_utc )
    {
        vlc_meta_SetDate( sys.meta, psz_date_utc );
    }

    if( psz_segment_filename )
    {
        fprintf( stderr, "***** WARNING: Unhandled meta - Use custom\n" );
    }
    if( psz_muxing_application )
    {
        fprintf( stderr, "***** WARNING: Unhandled meta - Use custom\n" );
    }
    if( psz_writing_application )
    {
        fprintf( stderr, "***** WARNING: Unhandled meta - Use custom\n" );
    }

    for( size_t i_track = 0; i_track < tracks.size(); i_track++ )
    {
//        mkv_track_t *tk = tracks[i_track];
//        vlc_meta_t *mtk = vlc_meta_New();
        fprintf( stderr, "***** WARNING: Unhandled child meta\n");
    }
#endif
#if 0
    if( i_tags_position >= 0 )
    {
        bool b_seekable;

        stream_Control( sys.demuxer.s, STREAM_CAN_FASTSEEK, &b_seekable );
        if( b_seekable )
        {
            LoadTags( );
        }
    }
#endif
}


/*****************************************************************************
 * Misc
 *****************************************************************************/

void matroska_segment_c::IndexAppendCluster( KaxCluster *cluster )
{
#define idx p_indexes[i_index]
    idx.i_track       = -1;
    idx.i_block_number= -1;
    idx.i_position    = cluster->GetElementPosition();
    idx.i_time        = cluster->GlobalTimecode()/ (mtime_t) 1000;
    idx.b_key         = true;

    i_index++;
    if( i_index >= i_index_max )
    {
        i_index_max += 1024;
        p_indexes = (mkv_index_t*)xrealloc( p_indexes,
                                        sizeof( mkv_index_t ) * i_index_max );
    }
#undef idx
}

bool matroska_segment_c::PreloadFamily( const matroska_segment_c & of_segment )
{
    if ( b_preloaded )
        return false;

    for (size_t i=0; i<families.size(); i++)
    {
        for (size_t j=0; j<of_segment.families.size(); j++)
        {
            if ( *(families[i]) == *(of_segment.families[j]) )
                return Preload( );
        }
    }

    return false;
}

bool matroska_segment_c::CompareSegmentUIDs( const matroska_segment_c * p_item_a, const matroska_segment_c * p_item_b )
{
    EbmlBinary *p_tmp;

    if ( p_item_a == NULL || p_item_b == NULL )
        return false;

    p_tmp = (EbmlBinary *)p_item_a->p_segment_uid;
    if ( p_item_b->p_prev_segment_uid != NULL
          && *p_tmp == *p_item_b->p_prev_segment_uid )
        return true;

    p_tmp = (EbmlBinary *)p_item_a->p_next_segment_uid;
    if ( !p_tmp )
        return false;

    if ( p_item_b->p_segment_uid != NULL
          && *p_tmp == *p_item_b->p_segment_uid )
        return true;

    if ( p_item_b->p_prev_segment_uid != NULL
          && *p_tmp == *p_item_b->p_prev_segment_uid )
        return true;

    return false;
}

bool matroska_segment_c::Preload( )
{
    if ( b_preloaded )
        return false;

    EbmlElement *el = NULL;

    ep->Reset( &sys.demuxer );

    while( ( el = ep->Get() ) != NULL )
    {
        if( MKV_IS_ID( el, KaxSeekHead ) )
        {
            /* Multiple allowed */
            /* We bail at 10, to prevent possible recursion */
            msg_Dbg(  &sys.demuxer, "|   + Seek head" );
            if( i_seekhead_count < 10 )
            {
                i_seekhead_position = (int64_t) es.I_O().getFilePointer();
                ParseSeekHead( static_cast<KaxSeekHead*>( el ) );
            }
        }
        else if( MKV_IS_ID( el, KaxInfo ) )
        {
            /* Multiple allowed, mandatory */
            msg_Dbg(  &sys.demuxer, "|   + Information" );
            if( i_info_position < 0 ) // FIXME
                ParseInfo( static_cast<KaxInfo*>( el ) );
            i_info_position = (int64_t) es.I_O().getFilePointer();
        }
        else if( MKV_IS_ID( el, KaxTracks ) )
        {
            /* Multiple allowed */
            msg_Dbg(  &sys.demuxer, "|   + Tracks" );
            if( i_tracks_position < 0 ) // FIXME
                ParseTracks( static_cast<KaxTracks*>( el ) );
            if ( tracks.size() == 0 )
            {
                msg_Err( &sys.demuxer, "No tracks supported" );
                return false;
            }
            i_tracks_position = (int64_t) es.I_O().getFilePointer();
        }
        else if( MKV_IS_ID( el, KaxCues ) )
        {
            msg_Dbg(  &sys.demuxer, "|   + Cues" );
            if( i_cues_position < 0 )
                LoadCues( static_cast<KaxCues*>( el ) );
            i_cues_position = (int64_t) es.I_O().getFilePointer();
        }
        else if( MKV_IS_ID( el, KaxCluster ) )
        {
            msg_Dbg( &sys.demuxer, "|   + Cluster" );

            cluster = (KaxCluster*)el;

            i_cluster_pos = i_start_pos = cluster->GetElementPosition();
            ParseCluster( );

            ep->Down();
            /* stop pre-parsing the stream */
            break;
        }
        else if( MKV_IS_ID( el, KaxAttachments ) )
        {
            msg_Dbg( &sys.demuxer, "|   + Attachments" );
            if( i_attachments_position < 0 )
                ParseAttachments( static_cast<KaxAttachments*>( el ) );
            i_attachments_position = (int64_t) es.I_O().getFilePointer();
        }
        else if( MKV_IS_ID( el, KaxChapters ) )
        {
            msg_Dbg( &sys.demuxer, "|   + Chapters" );
            if( i_chapters_position < 0 )
                ParseChapters( static_cast<KaxChapters*>( el ) );
            i_chapters_position = (int64_t) es.I_O().getFilePointer();
        }
        else if( MKV_IS_ID( el, KaxTag ) )
        {
            msg_Dbg( &sys.demuxer, "|   + Tags" );
            /*FIXME if( i_tags_position < 0)
                LoadTags( static_cast<KaxTags*>( el ) );*/
            i_tags_position = (int64_t) es.I_O().getFilePointer();
        }
        else if( MKV_IS_ID( el, EbmlVoid ) )
            msg_Dbg( &sys.demuxer, "|   + Void" );
        else
            msg_Dbg( &sys.demuxer, "|   + Preload Unknown (%s)", typeid(*el).name() );
    }

    ComputeTrackPriority();

    b_preloaded = true;

    return true;
}

/* Here we try to load elements that were found in Seek Heads, but not yet parsed */
bool matroska_segment_c::LoadSeekHeadItem( const EbmlCallbacks & ClassInfos, int64_t i_element_position )
{
    int64_t     i_sav_position = (int64_t)es.I_O().getFilePointer();
    EbmlElement *el;

    es.I_O().setFilePointer( i_element_position, seek_beginning );
    el = es.FindNextID( ClassInfos, 0xFFFFFFFFL);

    if( el == NULL )
    {
        msg_Err( &sys.demuxer, "cannot load some cues/chapters/tags etc. (broken seekhead or file)" );
        es.I_O().setFilePointer( i_sav_position, seek_beginning );
        return false;
    }

    if( MKV_IS_ID( el, KaxSeekHead ) )
    {
        /* Multiple allowed */
        msg_Dbg( &sys.demuxer, "|   + Seek head" );
        if( i_seekhead_count < 10 )
        {
            i_seekhead_position = i_element_position;
            ParseSeekHead( static_cast<KaxSeekHead*>( el ) );
        }
    }
    else if( MKV_IS_ID( el, KaxInfo ) ) // FIXME
    {
        /* Multiple allowed, mandatory */
        msg_Dbg( &sys.demuxer, "|   + Information" );
        if( i_info_position < 0 )
            ParseInfo( static_cast<KaxInfo*>( el ) );
        i_info_position = i_element_position;
    }
    else if( MKV_IS_ID( el, KaxTracks ) ) // FIXME
    {
        /* Multiple allowed */
        msg_Dbg( &sys.demuxer, "|   + Tracks" );
        if( i_tracks_position < 0 )
            ParseTracks( static_cast<KaxTracks*>( el ) );
        if ( tracks.size() == 0 )
        {
            msg_Err( &sys.demuxer, "No tracks supported" );
            delete el;
            es.I_O().setFilePointer( i_sav_position, seek_beginning );
            return false;
        }
        i_tracks_position = i_element_position;
    }
    else if( MKV_IS_ID( el, KaxCues ) )
    {
        msg_Dbg( &sys.demuxer, "|   + Cues" );
        if( i_cues_position < 0 )
            LoadCues( static_cast<KaxCues*>( el ) );
        i_cues_position = i_element_position;
    }
    else if( MKV_IS_ID( el, KaxAttachments ) )
    {
        msg_Dbg( &sys.demuxer, "|   + Attachments" );
        if( i_attachments_position < 0 )
            ParseAttachments( static_cast<KaxAttachments*>( el ) );
        i_attachments_position = i_element_position;
    }
    else if( MKV_IS_ID( el, KaxChapters ) )
    {
        msg_Dbg( &sys.demuxer, "|   + Chapters" );
        if( i_chapters_position < 0 )
            ParseChapters( static_cast<KaxChapters*>( el ) );
        i_chapters_position = i_element_position;
    }
    else if( MKV_IS_ID( el, KaxTags ) )
    {
        msg_Dbg( &sys.demuxer, "|   + Tags" );
        if( i_tags_position < 0 )
            LoadTags( static_cast<KaxTags*>( el ) );
        i_tags_position = i_element_position;
    }
    else
    {
        msg_Dbg( &sys.demuxer, "|   + LoadSeekHeadItem Unknown (%s)", typeid(*el).name() );
    }
    delete el;

    es.I_O().setFilePointer( i_sav_position, seek_beginning );
    return true;
}

struct spoint
{
    spoint(unsigned int tk, mtime_t date, int64_t pos, int64_t cpos):
        i_track(tk),i_date(date), i_seek_pos(pos),
        i_cluster_pos(cpos), p_next(NULL){}
    unsigned int     i_track;
    mtime_t i_date;
    int64_t i_seek_pos;
    int64_t i_cluster_pos;
    spoint * p_next;
};

void matroska_segment_c::Seek( mtime_t i_date, mtime_t i_time_offset, int64_t i_global_position )
{
    KaxBlock    *block;
    KaxSimpleBlock *simpleblock;
    int64_t     i_block_duration;
    size_t      i_track;
    int64_t     i_seek_position = i_start_pos;
    int64_t     i_seek_time = i_start_time;
    mtime_t     i_pts = 0;
    spoint *p_first = NULL;
    spoint *p_last = NULL;
    int i_cat;
    bool b_has_key = false;

    for( size_t i = 0; i < tracks.size(); i++)
        tracks[i]->i_last_dts = VLC_TS_INVALID;

    if( i_global_position >= 0 )
    {
        /* Special case for seeking in files with no cues */
        EbmlElement *el = NULL;

        /* Start from the last known index instead of the beginning eachtime */
        if( i_index == 0)
            es.I_O().setFilePointer( i_start_pos, seek_beginning );
        else
            es.I_O().setFilePointer( p_indexes[ i_index - 1 ].i_position,
                                     seek_beginning );
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
                    ( i_index > 0 &&
                      p_indexes[i_index - 1].i_position < (int64_t)cluster->GetElementPosition() ) )
                {
                    ParseCluster(false);
                    IndexAppendCluster( cluster );
                }
                if( es.I_O().getFilePointer() >= (unsigned) i_global_position )
                    break;
            }
        }
    }

    /* Don't try complex seek if we seek to 0 */
    if( i_date == 0 && i_time_offset == 0 )
    {
        es_out_Control( sys.demuxer.out, ES_OUT_SET_PCR, VLC_TS_0 );
        es_out_Control( sys.demuxer.out, ES_OUT_SET_NEXT_DISPLAY_TIME,
                        INT64_C(0) );
        es.I_O().setFilePointer( i_start_pos );

        delete ep;
        ep = new EbmlParser( &es, segment, &sys.demuxer );
        cluster = NULL;
        sys.i_start_pts = 0;
        sys.i_pts = 0;
        sys.i_pcr = 0;
        return;
    }

    int i_idx = 0;
    if ( i_index > 0 )
    {

        for( ; i_idx < i_index; i_idx++ )
            if( p_indexes[i_idx].i_time + i_time_offset > i_date )
                break;

        if( i_idx > 0 )
            i_idx--;

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
    const int es_types[3] = { VIDEO_ES, AUDIO_ES, SPU_ES };
    i_cat = es_types[0];
    for( int i = 0; i < 2; i_cat = es_types[++i] )
    {
        for( i_track = 0; i_track < tracks.size(); i_track++ )
        {
            if( tracks[i_track]->fmt.i_cat == i_cat )
            {
                spoint * seekpoint = new spoint(i_track, i_seek_time, i_seek_position, i_seek_position);
                if( unlikely( !seekpoint ) )
                {
                    for( spoint * sp = p_first; sp; )
                    {
                        spoint * tmp = sp;
                        sp = sp->p_next;
                        delete tmp;
                    }
                    return;
                }
                if( unlikely( !p_first ) )
                {
                    p_first = seekpoint;
                    p_last = seekpoint;
                }
                else
                {
                    p_last->p_next = seekpoint;
                    p_last = seekpoint;
                }
            }
        }
        if( likely( p_first ) )
            break;
    }
    /*Neither video nor audio track... no seek further*/
    if( unlikely( !p_first ) )
    {
        es_out_Control( sys.demuxer.out, ES_OUT_SET_PCR, i_date );
        es_out_Control( sys.demuxer.out, ES_OUT_SET_NEXT_DISPLAY_TIME, i_date );
        return;
    }

    for(;;)
    {
        do
        {
            bool b_key_picture;
            bool b_discardable_picture;
            if( BlockGet( block, simpleblock, &b_key_picture, &b_discardable_picture, &i_block_duration ) )
            {
                msg_Warn( &sys.demuxer, "cannot get block EOF?" );
                return;
            }

            /* check if block's track is in our list */
            for( i_track = 0; i_track < tracks.size(); i_track++ )
            {
                if( (simpleblock && tracks[i_track]->i_number == simpleblock->TrackNum()) ||
                    (block && tracks[i_track]->i_number == block->TrackNum()) )
                    break;
            }

            if( simpleblock )
                i_pts = sys.i_chapter_time + simpleblock->GlobalTimecode() / (mtime_t) 1000;
            else
                i_pts = sys.i_chapter_time + block->GlobalTimecode() / (mtime_t) 1000;
            if( i_track < tracks.size() )
            {
                if( tracks[i_track]->fmt.i_cat == i_cat && b_key_picture )
                {
                    /* get the seekpoint */
                    spoint * sp;
                    for( sp =  p_first; sp; sp = sp->p_next )
                        if( sp->i_track == i_track )
                            break;

                    sp->i_date = i_pts;
                    if( simpleblock )
                        sp->i_seek_pos = simpleblock->GetElementPosition();
                    else
                        sp->i_seek_pos = i_block_pos;
                    sp->i_cluster_pos = i_cluster_pos;
                    b_has_key = true;
                }
            }

            delete block;
        } while( i_pts < i_date );
        if( b_has_key || !i_idx )
            break;

        /* No key picture was found in the cluster seek to previous seekpoint */
        i_date = i_time_offset + p_indexes[i_idx].i_time;
        i_idx--;
        i_pts = 0;
        es.I_O().setFilePointer( p_indexes[i_idx].i_position );
        delete ep;
        ep = new EbmlParser( &es, segment, &sys.demuxer );
        cluster = NULL;
    }

    /* rewind to the last I img */
    spoint * p_min;
    for( p_min  = p_first, p_last = p_first; p_last; p_last = p_last->p_next )
        if( p_last->i_date < p_min->i_date )
            p_min = p_last;

    sys.i_pcr = sys.i_pts = p_min->i_date;
    es_out_Control( sys.demuxer.out, ES_OUT_SET_PCR, VLC_TS_0 + sys.i_pcr );
    es_out_Control( sys.demuxer.out, ES_OUT_SET_NEXT_DISPLAY_TIME, i_date );
    cluster = (KaxCluster *) ep->UnGet( p_min->i_seek_pos, p_min->i_cluster_pos );

    /* hack use BlockGet to get the cluster then goto the wanted block */
    if ( !cluster )
    {
        bool b_key_picture;
        bool b_discardable_picture;
        BlockGet( block, simpleblock, &b_key_picture, &b_discardable_picture, &i_block_duration );
        delete block;
        cluster = (KaxCluster *) ep->UnGet( p_min->i_seek_pos, p_min->i_cluster_pos );
    }

    while( p_first )
    {
        p_min = p_first;
        p_first = p_first->p_next;
        delete p_min;
    }
}

int matroska_segment_c::BlockFindTrackIndex( size_t *pi_track,
                                             const KaxBlock *p_block, const KaxSimpleBlock *p_simpleblock )
{
    size_t i_track;
    for( i_track = 0; i_track < tracks.size(); i_track++ )
    {
        const mkv_track_t *tk = tracks[i_track];

        if( ( p_block != NULL && tk->i_number == p_block->TrackNum() ) ||
            ( p_simpleblock != NULL && tk->i_number == p_simpleblock->TrackNum() ) )
        {
            break;
        }
    }

    if( i_track >= tracks.size() )
        return VLC_EGENERIC;

    if( pi_track )
        *pi_track = i_track;
    return VLC_SUCCESS;
}

void matroska_segment_c::ComputeTrackPriority()
{
    bool b_has_default_video = false;
    bool b_has_default_audio = false;
    /* check for default */
    for(size_t i_track = 0; i_track < tracks.size(); i_track++)
    {
        mkv_track_t *p_tk = tracks[i_track];
        es_format_t *p_fmt = &p_tk->fmt;
        if( p_fmt->i_cat == VIDEO_ES )
            b_has_default_video |=
                p_tk->b_enabled && ( p_tk->b_default || p_tk->b_forced );
        else if( p_fmt->i_cat == AUDIO_ES )
            b_has_default_audio |=
                p_tk->b_enabled && ( p_tk->b_default || p_tk->b_forced );
    }

    for( size_t i_track = 0; i_track < tracks.size(); i_track++ )
    {
        mkv_track_t *p_tk = tracks[i_track];
        es_format_t *p_fmt = &p_tk->fmt;

        if( unlikely( p_fmt->i_cat == UNKNOWN_ES || !p_tk->psz_codec ) )
        {
            msg_Warn( &sys.demuxer, "invalid track[%d, n=%d]", (int)i_track, p_tk->i_number );
            p_tk->p_es = NULL;
            continue;
        }
        else if( unlikely( !b_has_default_video && p_fmt->i_cat == VIDEO_ES ) )
        {
            p_tk->b_default = true;
            b_has_default_video = true;
        }
        else if( unlikely( !b_has_default_audio &&  p_fmt->i_cat == AUDIO_ES ) )
        {
            p_tk->b_default = true;
            b_has_default_audio = true;
        }
        if( unlikely( !p_tk->b_enabled ) )
            p_tk->fmt.i_priority = -2;
        else if( p_tk->b_forced )
            p_tk->fmt.i_priority = 2;
        else if( p_tk->b_default )
            p_tk->fmt.i_priority = 1;
        else
            p_tk->fmt.i_priority = 0;

        /* Avoid multivideo tracks when unnecessary */
        if( p_tk->fmt.i_cat == VIDEO_ES )
            p_tk->fmt.i_priority--;
    } 
}

bool matroska_segment_c::Select( mtime_t i_start_time )
{
    /* add all es */
    msg_Dbg( &sys.demuxer, "found %d es", (int)tracks.size() );

    for( size_t i_track = 0; i_track < tracks.size(); i_track++ )
    {
        mkv_track_t *p_tk = tracks[i_track];
        es_format_t *p_fmt = &p_tk->fmt;

        if( unlikely( p_fmt->i_cat == UNKNOWN_ES || !p_tk->psz_codec ) )
        {
            msg_Warn( &sys.demuxer, "invalid track[%d, n=%d]", (int)i_track, p_tk->i_number );
            p_tk->p_es = NULL;
            continue;
        }

        if( !p_tk->p_es )
            p_tk->p_es = es_out_Add( sys.demuxer.out, &p_tk->fmt );

        /* Turn on a subtitles track if it has been flagged as default -
         * but only do this if no subtitles track has already been engaged,
         * either by an earlier 'default track' (??) or by default
         * language choice behaviour.
         */
        if( p_tk->b_default || p_tk->b_forced )
        {
            es_out_Control( sys.demuxer.out,
                            ES_OUT_SET_ES_DEFAULT,
                            p_tk->p_es );
        }
    }
    es_out_Control( sys.demuxer.out, ES_OUT_SET_NEXT_DISPLAY_TIME, i_start_time );

    sys.i_start_pts = i_start_time;
    // reset the stream reading to the first cluster of the segment used
    es.I_O().setFilePointer( i_start_pos );

    delete ep;
    ep = new EbmlParser( &es, segment, &sys.demuxer );

    return true;
}

void matroska_segment_c::UnSelect( )
{
    sys.p_ev->ResetPci();
    for( size_t i_track = 0; i_track < tracks.size(); i_track++ )
    {
        if ( tracks[i_track]->p_es != NULL )
        {
//            es_format_Clean( &tracks[i_track]->fmt );
            es_out_Del( sys.demuxer.out, tracks[i_track]->p_es );
            tracks[i_track]->p_es = NULL;
        }
    }
    delete ep;
    ep = NULL;
}

int matroska_segment_c::BlockGet( KaxBlock * & pp_block, KaxSimpleBlock * & pp_simpleblock, bool *pb_key_picture, bool *pb_discardable_picture, int64_t *pi_duration )
{
    pp_simpleblock = NULL;
    pp_block = NULL;

    *pb_key_picture         = true;
    *pb_discardable_picture = false;
    size_t i_tk;

    for( ;; )
    {
        EbmlElement *el = NULL;
        int         i_level;

        if ( ep == NULL )
            return VLC_EGENERIC;

        if( pp_simpleblock != NULL || ((el = ep->Get()) == NULL && pp_block != NULL) )
        {
            /* Check blocks validity to protect againts broken files */
            if( BlockFindTrackIndex( &i_tk, pp_block , pp_simpleblock ) )
            {
                delete pp_block;
                pp_simpleblock = NULL;
                pp_block = NULL;
                continue;
            }
            if( pp_simpleblock != NULL )
            {
                *pb_key_picture         = pp_simpleblock->IsKeyframe();
                *pb_discardable_picture = pp_simpleblock->IsDiscardable();
            }
            /* We have block group let's check if the picture is a keyframe */
            else if( *pb_key_picture )
            {
                switch(tracks[i_tk]->fmt.i_codec)
                {
                    case VLC_CODEC_THEORA:
                        {
                            DataBuffer *p_data = &pp_block->GetBuffer(0);
                            size_t sz = p_data->Size();
                            const uint8_t * p_buff = p_data->Buffer();
                            /* if the second bit of a Theora frame is 1 
                               it's not a keyframe */
                            if( sz && p_buff )
                            {
                                if( p_buff[0] & 0x40 )
                                    *pb_key_picture = false;
                            }
                            else
                                *pb_key_picture = false;
                            break;
                        }
                }
            }

            /* update the index */
#define idx p_indexes[i_index - 1]
            if( i_index > 0 && idx.i_time == -1 )
            {
                if ( pp_simpleblock != NULL )
                    idx.i_time        = pp_simpleblock->GlobalTimecode() / (mtime_t)1000;
                else
                    idx.i_time        = (*pp_block).GlobalTimecode() / (mtime_t)1000;
                idx.b_key         = *pb_key_picture;
            }
#undef idx
            return VLC_SUCCESS;
        }

        i_level = ep->GetLevel();

        if( el == NULL )
        {
            if( i_level > 1 )
            {
                ep->Up();
                continue;
            }
            msg_Warn( &sys.demuxer, "EOF" );
            return VLC_EGENERIC;
        }

        /* Verify that we are still inside our cluster
         * It can happens whith broken files and when seeking
         * without index */
        if( i_level > 1 )
        {
            if( cluster && !ep->IsTopPresent( cluster ) )
            {
                msg_Warn( &sys.demuxer, "Unexpected escape from current cluster" );
                cluster = NULL;
            }
            if( !cluster )
                continue;
        }

        /* do parsing */
        try
        {
            switch ( i_level )
            {
                case 1:
                    if( MKV_IS_ID( el, KaxCluster ) )
                    {
                        cluster = (KaxCluster*)el;
                        i_cluster_pos = cluster->GetElementPosition();

                        // reset silent tracks
                        for (size_t i=0; i<tracks.size(); i++)
                        {
                            tracks[i]->b_silent = false;
                        }

                        ep->Down();
                    }
                    else if( MKV_IS_ID( el, KaxCues ) )
                    {
                        msg_Warn( &sys.demuxer, "find KaxCues FIXME" );
                        return VLC_EGENERIC;
                    }
                    else
                    {
                        msg_Dbg( &sys.demuxer, "unknown (%s)", typeid( el ).name() );
                    }
                    break;
                case 2:
                    if( MKV_IS_ID( el, KaxClusterTimecode ) )
                    {
                        KaxClusterTimecode &ctc = *(KaxClusterTimecode*)el;

                        ctc.ReadData( es.I_O(), SCOPE_ALL_DATA );
                        cluster->InitTimecode( uint64( ctc ), i_timescale );

                        /* add it to the index */
                        if( i_index == 0 ||
                            ( i_index > 0 &&
                              p_indexes[i_index - 1].i_position < (int64_t)cluster->GetElementPosition() ) )
                            IndexAppendCluster( cluster );
                    }
                    else if( MKV_IS_ID( el, KaxClusterSilentTracks ) )
                    {
                        ep->Down();
                    }
                    else if( MKV_IS_ID( el, KaxBlockGroup ) )
                    {
                        i_block_pos = el->GetElementPosition();
                        ep->Down();
                    }
                    else if( MKV_IS_ID( el, KaxSimpleBlock ) )
                    {
                        pp_simpleblock = (KaxSimpleBlock*)el;

                        pp_simpleblock->ReadData( es.I_O() );
                        pp_simpleblock->SetParent( *cluster );
                    }
                    break;
                case 3:
                    if( MKV_IS_ID( el, KaxBlock ) )
                    {
                        pp_block = (KaxBlock*)el;

                        pp_block->ReadData( es.I_O() );
                        pp_block->SetParent( *cluster );

                        ep->Keep();
                    }
                    else if( MKV_IS_ID( el, KaxBlockDuration ) )
                    {
                        KaxBlockDuration &dur = *(KaxBlockDuration*)el;

                        dur.ReadData( es.I_O() );
                        *pi_duration = uint64( dur );
                    }
                    else if( MKV_IS_ID( el, KaxReferenceBlock ) )
                    {
                        KaxReferenceBlock &ref = *(KaxReferenceBlock*)el;

                        ref.ReadData( es.I_O() );

                        if( *pb_key_picture )
                            *pb_key_picture = false;
                        else if( int64( ref ) > 0 )
                            *pb_discardable_picture = true;
                    }
                    else if( MKV_IS_ID( el, KaxClusterSilentTrackNumber ) )
                    {
                        KaxClusterSilentTrackNumber &track_num = *(KaxClusterSilentTrackNumber*)el;
                        track_num.ReadData( es.I_O() );
                        // find the track
                        for (size_t i=0; i<tracks.size(); i++)
                        {
                            if ( tracks[i]->i_number == uint32(track_num))
                            {
                                tracks[i]->b_silent = true;
                                break;
                            }
                        }
                    }
                    break;
                default:
                    msg_Err( &sys.demuxer, "invalid level = %d", i_level );
                    return VLC_EGENERIC;
            }
        }
        catch(...)
        {
            msg_Err( &sys.demuxer, "Error while reading %s... upping level", typeid(*el).name());
            ep->Up();
        }
    }
}

SimpleTag::~SimpleTag()
{
    free(psz_tag_name);
    free(psz_lang);
    free(p_value);
    for(size_t i = 0; i < sub_tags.size(); i++)
        delete sub_tags[i];
}

Tag::~Tag()
{
    for(size_t i = 0; i < simple_tags.size(); i++)
        delete simple_tags[i];
}
