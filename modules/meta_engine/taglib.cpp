/*****************************************************************************
 * taglib.cpp: Taglib tag parser/writer
 *****************************************************************************
 * Copyright (C) 2003-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Rafaël Carré <funman@videolanorg>
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

#include <vlc/vlc.h>
#include <vlc_playlist.h>
#include <vlc_meta.h>
#include <vlc_demux.h>

#include <fileref.h>
#include <tag.h>
#include <tstring.h>
#include <id3v2tag.h>
#include <textidentificationframe.h>
#include <tbytevector.h>
#include <mpegfile.h>
#include <flacfile.h>
#if 0
#include <oggflacfile.h>
#endif
#include <flacfile.h>
#include <flacproperties.h>
#include <vorbisfile.h>
#include <vorbisproperties.h>
#include <uniquefileidentifierframe.h>
#include <textidentificationframe.h>
#if 0 /* parse the tags without taglib helpers? */
#include <relativevolumeframe.h>
#endif

static int  ReadMeta    ( vlc_object_t * );
static int  DownloadArt ( vlc_object_t * );
static int  WriteMeta   ( vlc_object_t * );

vlc_module_begin();
    set_capability( "meta reader", 1000 );
    set_callbacks( ReadMeta, NULL );
    add_submodule();
        set_capability( "art downloader", 50 );
        set_callbacks( DownloadArt, NULL );
    add_submodule();
        set_capability( "meta writer", 50 );
        set_callbacks( WriteMeta, NULL );
vlc_module_end();

static bool checkID3Image( const TagLib::ID3v2::Tag *p_tag )
{
    TagLib::ID3v2::FrameList l = p_tag->frameListMap()[ "APIC" ];
    return !l.isEmpty();
}

/* Try detecting embedded art */
static void DetectImage( TagLib::FileRef f, vlc_meta_t *p_meta )
{
    if( TagLib::MPEG::File *mpeg =
               dynamic_cast<TagLib::MPEG::File *>(f.file() ) )
    {
        if( mpeg->ID3v2Tag() && checkID3Image( mpeg->ID3v2Tag() ) )
            vlc_meta_SetArtURL( p_meta, "APIC" );
    }
    else if( TagLib::FLAC::File *flac =
             dynamic_cast<TagLib::FLAC::File *>(f.file() ) )
    {
        if( flac->ID3v2Tag() && checkID3Image( flac->ID3v2Tag() ) )
            vlc_meta_SetArtURL( p_meta, "APIC" );
    }
#if 0
/* This needs special additions to taglib */
 * else if( TagLib::MP4::File *mp4 =
               dynamic_cast<TagLib::MP4::File *>( f.file() ) )
    {
        TagLib::MP4::Tag *mp4tag =
                dynamic_cast<TagLib::MP4::Tag *>( mp4->tag() );
        if( mp4tag && mp4tag->cover().size() )
            vlc_meta_SetArtURL( p_meta, "MP4C" );
    }
#endif
}

static int ReadMeta( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;

    if( strncmp( p_demux->psz_access, "file", 4 ) )
        return VLC_EGENERIC;

    TagLib::FileRef f( p_demux->psz_path );
    if( f.isNull() )
        return VLC_EGENERIC;

    if ( !f.tag() || f.tag()->isEmpty() )
        return VLC_EGENERIC;

    if( !p_demux->p_private )
        p_demux->p_private = (void*)vlc_meta_New();
    vlc_meta_t *p_meta = (vlc_meta_t *)(p_demux->p_private );
    TagLib::Tag *p_tag = f.tag();

    if( TagLib::MPEG::File *p_mpeg =
        dynamic_cast<TagLib::MPEG::File *>(f.file() ) )
    {
        if( p_mpeg->ID3v2Tag() )
        {
            TagLib::ID3v2::Tag *p_tag = p_mpeg->ID3v2Tag();
            TagLib::ID3v2::FrameList list = p_tag->frameListMap()["UFID"];
            TagLib::ID3v2::UniqueFileIdentifierFrame* p_ufid;
            for( TagLib::ID3v2::FrameList::Iterator iter = list.begin();
                    iter != list.end(); iter++ )
            {
                p_ufid = dynamic_cast<TagLib::ID3v2::UniqueFileIdentifierFrame*>(*iter);
                const char *owner = p_ufid->owner().toCString();
                if (!strcmp( owner, "http://musicbrainz.org" ))
                {
                    /* ID3v2 UFID contains up to 64 bytes binary data
                        * but in our case it will be a '\0'
                        * terminated string */
                    char *psz_ufid = (char*) malloc( 64 );
                    int j = 0;
                    while( ( j < 63 ) &&
                            ( j < p_ufid->identifier().size() ) )
                        psz_ufid[j] = p_ufid->identifier()[j++];
                    psz_ufid[j] = '\0';
                    vlc_meta_SetTrackID( p_meta, psz_ufid );
                    free( psz_ufid );
                }
            }

            list = p_tag->frameListMap()["TXXX"];
            TagLib::ID3v2::UserTextIdentificationFrame* p_txxx;
            for( TagLib::ID3v2::FrameList::Iterator iter = list.begin();
                    iter != list.end(); iter++ )
            {
                p_txxx = dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame*>(*iter);
                const char *psz_desc= p_txxx->description().toCString();
                vlc_meta_AddExtra( p_meta, psz_desc,
                            p_txxx->fieldList().toString().toCString());
            }
#if 0
            list = p_tag->frameListMap()["RVA2"];
            TagLib::ID3v2::RelativeVolumeFrame* p_rva2;
            for( TagLib::ID3v2::FrameList::Iterator iter = list.begin();
                    iter != list.end(); iter++ )
            {
                p_rva2 = dynamic_cast<TagLib::ID3v2::RelativeVolumeFrame*>(*iter);
                /* TODO: process rva2 frames */
            }
#endif
            list = p_tag->frameList();
            TagLib::ID3v2::Frame* p_t;
            char psz_tag[4];
            for( TagLib::ID3v2::FrameList::Iterator iter = list.begin();
                    iter != list.end(); iter++ )
            {
                p_t = dynamic_cast<TagLib::ID3v2::Frame*> (*iter);
                memcpy( psz_tag, p_t->frameID().data(), 4);

#define SET( foo, bar ) if( !strncmp( psz_tag, foo, 4 ) ) \
vlc_meta_Set##bar( p_meta, p_t->toString().toCString(true))
                SET( "TPUB", Publisher );
                SET( "TCOP", Copyright );
                SET( "TENC", EncodedBy );
                SET( "TLAN", Language );
                //SET( "POPM", Rating ); /* rating needs special handling in id3v2 */
                //if( !strncmp( psz_tag, "RVA2", 4 ) )
                    /* TODO */
#undef SET
            }
        }
    }

    else if( TagLib::Ogg::Vorbis::File *p_ogg_v =
        dynamic_cast<TagLib::Ogg::Vorbis::File *>(f.file() ) )
    {
        int i_ogg_v_length = p_ogg_v->audioProperties()->length();

        input_thread_t *p_input = (input_thread_t *)
                vlc_object_find( p_demux,VLC_OBJECT_INPUT, FIND_PARENT );
        if( p_input )
        {
            input_item_t *p_item = input_GetItem( p_input );
            if( p_item )
                input_item_SetDuration( p_item,
                        (mtime_t) i_ogg_v_length * 1000000 );
            vlc_object_release( p_input );
        }
 
    }
#if 0 /* at this moment, taglib is unable to detect ogg/flac files
* becauses type detection is based on file extension:
* ogg = ogg/vorbis
* flac = flac
* ø = ogg/flac
*/
    else if( TagLib::Ogg::FLAC::File *p_ogg_f =
        dynamic_cast<TagLib::Ogg::FLAC::File *>(f.file() ) )
    {
        long i_ogg_f_length = p_ogg_f->streamLength();
        input_thread_t *p_input = (input_thread_t *)
                vlc_object_find( p_demux, VLC_OBJECT_INPUT, FIND_PARENT );
        if( p_input )
        {
            input_item_t *p_item = input_GetItem( p_input );
            if( p_item )
                input_item_SetDuration( p_item,
                        (mtime_t) i_ogg_f_length * 1000000 );
            vlc_object_release( p_input );
        }
    }
#endif
    else if( TagLib::FLAC::File *p_flac =
        dynamic_cast<TagLib::FLAC::File *>(f.file() ) )
    {
        long i_flac_length = p_flac->audioProperties()->length();
        input_thread_t *p_input = (input_thread_t *)
                vlc_object_find( p_demux, VLC_OBJECT_INPUT, FIND_PARENT );
        if( p_input )
        {
            input_item_t *p_item = input_GetItem( p_input );
            if( p_item )
                input_item_SetDuration( p_item,
                        (mtime_t) i_flac_length * 1000000 );
            vlc_object_release( p_input );
        }
    }

#define SET( foo, bar ) vlc_meta_Set##foo( p_meta, p_tag->bar ().toCString(true))
#define SETINT( foo, bar ) { \
        char psz_tmp[10]; \
        snprintf( (char*)psz_tmp, 10, "%d", p_tag->bar() ); \
        vlc_meta_Set##foo( p_meta, (char*)psz_tmp ); \
    }

    SET( Title, title );
    SET( Artist, artist );
    SET( Album, album );
    SET( Description, comment );
    SET( Genre, genre );
    SETINT( Date, year );
    SETINT( Tracknum , track );
#undef SET
#undef SETINT

    DetectImage( f, p_meta );

    return VLC_SUCCESS;
}

static int WriteMeta( vlc_object_t *p_this )
{
    playlist_t *p_playlist = (playlist_t *)p_this;
    meta_export_t *p_export = (meta_export_t *)p_playlist->p_private;
    input_item_t *p_item = p_export->p_item;
 
    if( p_item == NULL )
    {
        msg_Err( p_this, "Can't save meta data of an empty input" );
        return VLC_EGENERIC;
    }

    TagLib::FileRef f( p_export->psz_file );
    if( f.isNull() || !f.tag() || f.file()->readOnly() )
    {
        msg_Err( p_this, "File %s can't be opened for tag writing\n",
            p_export->psz_file );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_this, "Writing metadata for %s", p_export->psz_file );

    TagLib::Tag *p_tag = f.tag();

    char *psz_meta;

#define SET(a,b) \
        if(b) { \
            TagLib::String *psz_##a = new TagLib::String( b, \
                TagLib::String::UTF8 ); \
            p_tag->set##a( *psz_##a ); \
            delete psz_##a; \
        }


    psz_meta = input_item_GetArtist( p_item );
    SET( Artist, psz_meta );
    free( psz_meta );

    psz_meta = input_item_GetTitle( p_item );
    if( !psz_meta ) psz_meta = input_item_GetName( p_item );
    TagLib::String *psz_title = new TagLib::String( psz_meta,
        TagLib::String::UTF8 );
    p_tag->setTitle( *psz_title );
    delete psz_title;
    free( psz_meta );

    psz_meta = input_item_GetAlbum( p_item );
    SET( Album, psz_meta );
    free( psz_meta );

    psz_meta = input_item_GetGenre( p_item );
    SET( Genre, psz_meta );
    free( psz_meta );

#undef SET

    psz_meta = input_item_GetDate( p_item );
    if( psz_meta ) p_tag->setYear( atoi( psz_meta ) );
    free( psz_meta );

    psz_meta = input_item_GetTrackNum( p_item );
    if( psz_meta ) p_tag->setTrack( atoi( psz_meta ) );
    free( psz_meta );

    if( TagLib::ID3v2::Tag *p_id3tag =
        dynamic_cast<TagLib::ID3v2::Tag *>(p_tag) )
    {
#define WRITE( foo, bar ) \
        psz_meta = input_item_Get##foo( p_item ); \
        if( psz_meta ) \
        { \
            TagLib::ByteVector p_byte( bar, 4 ); \
            TagLib::ID3v2::TextIdentificationFrame p_frame( p_byte ); \
            p_frame.setText( psz_meta ); \
            p_id3tag->addFrame( &p_frame ); \
            free( psz_meta ); \
        } \

        WRITE( Publisher, "TPUB" );
        WRITE( Copyright, "TCOP" );
        WRITE( EncodedBy, "TENC" );
        WRITE( Language, "TLAN" );
 
#undef WRITE
    }

    f.save();
    return VLC_SUCCESS;
}

static int DownloadArt( vlc_object_t *p_this )
{
    /* We need to be passed the file name
     * Fetch the thing from the file, save it to the cache folder
     */
    return VLC_EGENERIC;
}

