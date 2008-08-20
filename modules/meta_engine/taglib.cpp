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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_playlist.h>
#include <vlc_meta.h>
#include <vlc_demux.h>
#include <vlc_strings.h>
#include <vlc_charset.h>

#ifdef WIN32
# include <io.h>
#else
# include <unistd.h>
#endif

#include <fileref.h>
#include <tag.h>
#include <tstring.h>
#include <id3v2tag.h>
#include <textidentificationframe.h>
#include <tbytevector.h>
#include <mpegfile.h>
#include <attachedpictureframe.h>
//#include <oggflacfile.h> /* ogg flac files aren't auto-casted by TagLib */
#include <flacfile.h>
#include <flacproperties.h>
#include <vorbisfile.h>
#include <vorbisproperties.h>
#include <xiphcomment.h>
#include <uniquefileidentifierframe.h>
#include <textidentificationframe.h>
//#include <relativevolumeframe.h> /* parse the tags without TagLib helpers? */

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

using namespace TagLib;

/* Try detecting embedded art */
static void DetectImage( FileRef f, demux_t *p_demux )
{
    demux_meta_t        *p_demux_meta   = (demux_meta_t *)p_demux->p_private;
    vlc_meta_t          *p_meta         = p_demux_meta->p_meta;
    int                 i_score         = -1;

    /* Preferred type of image
     * The 21 types are defined in id3v2 standard:
     * http://www.id3.org/id3v2.4.0-frames */
    static const int pi_cover_score[] = {
        0,  /* Other */
        5,  /* 32x32 PNG image that should be used as the file icon */
        4,  /* File icon of a different size or format. */
        20, /* Front cover image of the album. */
        19, /* Back cover image of the album. */
        13, /* Inside leaflet page of the album. */
        18, /* Image from the album itself. */
        17, /* Picture of the lead artist or soloist. */
        16, /* Picture of the artist or performer. */
        14, /* Picture of the conductor. */
        15, /* Picture of the band or orchestra. */
        9,  /* Picture of the composer. */
        8,  /* Picture of the lyricist or text writer. */
        7,  /* Picture of the recording location or studio. */
        10, /* Picture of the artists during recording. */
        11, /* Picture of the artists during performance. */
        6,  /* Picture from a movie or video related to the track. */
        1,  /* Picture of a large, coloured fish. */
        12, /* Illustration related to the track. */
        3,  /* Logo of the band or performer. */
        2   /* Logo of the publisher (record company). */
    };

    if( MPEG::File *mpeg = dynamic_cast<MPEG::File *>(f.file() ) )
    {
        ID3v2::Tag  *p_tag = mpeg->ID3v2Tag();
        if( !p_tag )
            return;
        ID3v2::FrameList list = p_tag->frameListMap()[ "APIC" ];
        if( list.isEmpty() )
            return;
        ID3v2::AttachedPictureFrame *p_apic;

        TAB_INIT( p_demux_meta->i_attachments, p_demux_meta->attachments );
        for( ID3v2::FrameList::Iterator iter = list.begin();
                iter != list.end(); iter++ )
        {
            p_apic = dynamic_cast<ID3v2::AttachedPictureFrame*>(*iter);
            input_attachment_t *p_attachment;

            const char *psz_name, *psz_mime, *psz_description;
            ByteVector p_data_taglib; const char *p_data; int i_data;

            psz_mime = p_apic->mimeType().toCString(true);
            psz_description = psz_name = p_apic->description().toCString(true);

            /* some old iTunes version not only sets incorrectly the mime type
             * or the description of the image,
             * but also embeds incorrectly the image.
             * Recent versions seem to behave correctly */
            if( !strncmp( psz_mime, "PNG", 3 ) ||
                !strncmp( psz_name, "\xC2\x89PNG", 5 ) )
            {
                msg_Warn( p_demux,
                    "%s: Invalid picture embedded by broken iTunes version, "
                    "you really shouldn't use this crappy software.",
                    (const char *)f.file()->name() );
                break;
            }

            p_data_taglib = p_apic->picture();
            p_data = p_data_taglib.data();
            i_data = p_data_taglib.size();

            msg_Dbg( p_demux, "Found embedded art: %s (%s) is %i bytes",
                    psz_name, psz_mime, i_data );

            p_attachment = vlc_input_attachment_New( psz_name, psz_mime,
                    psz_description, p_data, i_data );
            TAB_APPEND_CAST( (input_attachment_t**),
                    p_demux_meta->i_attachments, p_demux_meta->attachments,
                    p_attachment );

            if( pi_cover_score[p_apic->type()] > i_score )
            {
                i_score = pi_cover_score[p_apic->type()];
                char *psz_url;
                if( asprintf( &psz_url, "attachment://%s",
                        p_attachment->psz_name ) == -1 )
                    return;
                vlc_meta_SetArtURL( p_meta, psz_url );
                free( psz_url );
            }
        }
    }
    else
    if( Ogg::Vorbis::File *oggv = dynamic_cast<Ogg::Vorbis::File *>(f.file() ) )
    {
        Ogg::XiphComment *p_tag = oggv->tag();
        if( !p_tag )
            return;

        StringList mime_list = p_tag->fieldListMap()[ "COVERARTMIME" ];
        StringList art_list = p_tag->fieldListMap()[ "COVERART" ];

        /* we support only one cover in ogg/vorbis */
        if( mime_list.size() != 1 || art_list.size() != 1 )
            return;

        input_attachment_t *p_attachment;

        const char *psz_name, *psz_mime, *psz_description;
        uint8_t *p_data;
        int i_data;

        psz_name = "cover";
        psz_mime = mime_list[0].toCString(true);
        psz_description = "cover";

        i_data = vlc_b64_decode_binary( &p_data, art_list[0].toCString(true) );

        msg_Dbg( p_demux, "Found embedded art: %s (%s) is %i bytes",
                    psz_name, psz_mime, i_data );

        TAB_INIT( p_demux_meta->i_attachments, p_demux_meta->attachments );
        p_attachment = vlc_input_attachment_New( psz_name, psz_mime,
                psz_description, p_data, i_data );
        free( p_data );

        TAB_APPEND_CAST( (input_attachment_t**),
                p_demux_meta->i_attachments, p_demux_meta->attachments,
                p_attachment );

        vlc_meta_SetArtURL( p_meta, "attachment://cover" );
    }

#if 0
    //flac embedded images are extracted in the flac demuxer
    else if( FLAC::File *flac =
             dynamic_cast<FLAC::File *>(f.file() ) )
    {
        p_tag = flac->ID3v2Tag();
        if( p_tag )
            return;
        ID3v2::FrameList l = p_tag->frameListMap()[ "APIC" ];
        if( l.isEmpty() )
            return;
            vlc_meta_SetArtURL( p_meta, "APIC" );
    }
#endif
#if 0
/* TagLib doesn't support MP4 file yet */
    else if( MP4::File *mp4 =
               dynamic_cast<MP4::File *>( f.file() ) )
    {
        MP4::Tag *mp4tag =
                dynamic_cast<MP4::Tag *>( mp4->tag() );
        if( mp4tag && mp4tag->cover().size() )
            vlc_meta_SetArtURL( p_meta, "MP4C" );
    }
#endif
}

static int ReadMeta( vlc_object_t *p_this )
{
    demux_t         *p_demux = (demux_t *)p_this;
    demux_meta_t    *p_demux_meta = (demux_meta_t*)p_demux->p_private;
    vlc_meta_t      *p_meta;
    TagLib::FileRef  f;

    TAB_INIT( p_demux_meta->i_attachments, p_demux_meta->attachments );
    p_demux_meta->p_meta = NULL;

#if defined(WIN32) || defined (UNDER_CE)
    if(GetVersion() < 0x80000000)
    {
        wchar_t wpath[MAX_PATH + 1];
        if( !MultiByteToWideChar( CP_UTF8, 0, p_demux->psz_path, -1, wpath, MAX_PATH) )
            return VLC_EGENERIC;

        wpath[MAX_PATH] = L'0';
        f = FileRef( wpath );
    }
    else return VLC_EGENERIC;
#else
    const char *local_name = ToLocale( p_demux->psz_path );

    if( local_name == NULL )
        return VLC_EGENERIC;

    f = FileRef( local_name );
    LocaleFree( local_name );
#endif

    if( f.isNull() )
        return VLC_EGENERIC;

    if ( !f.tag() || f.tag()->isEmpty() )
        return VLC_EGENERIC;

    p_demux_meta->p_meta = p_meta = vlc_meta_New();
    Tag *p_tag = f.tag();

    if( MPEG::File *p_mpeg =
        dynamic_cast<MPEG::File *>(f.file() ) )
    {
        if( p_mpeg->ID3v2Tag() )
        {
            ID3v2::Tag *p_tag = p_mpeg->ID3v2Tag();
            ID3v2::FrameList list = p_tag->frameListMap()["UFID"];
            ID3v2::UniqueFileIdentifierFrame* p_ufid;
            for( ID3v2::FrameList::Iterator iter = list.begin();
                    iter != list.end(); iter++ )
            {
                p_ufid = dynamic_cast<ID3v2::UniqueFileIdentifierFrame*>(*iter);
                const char *owner = p_ufid->owner().toCString();
                if (!strcmp( owner, "http://musicbrainz.org" ))
                {
                    /* ID3v2 UFID contains up to 64 bytes binary data
                        * but in our case it will be a '\0'
                        * terminated string */
                    char *psz_ufid = (char*) malloc( 64 );
                    int j = 0;
                    if( psz_ufid )
                    {
                        while( ( j < 63 ) &&
                               ( j < p_ufid->identifier().size() ) )
                            psz_ufid[j] = p_ufid->identifier()[j++];
                        psz_ufid[j] = '\0';
                        vlc_meta_SetTrackID( p_meta, psz_ufid );
                        free( psz_ufid );
                    }
                }
            }

            list = p_tag->frameListMap()["TXXX"];
            ID3v2::UserTextIdentificationFrame* p_txxx;
            for( ID3v2::FrameList::Iterator iter = list.begin();
                    iter != list.end(); iter++ )
            {
                p_txxx = dynamic_cast<ID3v2::UserTextIdentificationFrame*>(*iter);
                const char *psz_desc= p_txxx->description().toCString();
                vlc_meta_AddExtra( p_meta, psz_desc,
                            p_txxx->fieldList().toString().toCString());
            }
#if 0
            list = p_tag->frameListMap()["RVA2"];
            ID3v2::RelativeVolumeFrame* p_rva2;
            for( ID3v2::FrameList::Iterator iter = list.begin();
                    iter != list.end(); iter++ )
            {
                p_rva2 = dynamic_cast<ID3v2::RelativeVolumeFrame*>(*iter);
                /* TODO: process rva2 frames */
            }
#endif
            list = p_tag->frameList();
            ID3v2::Frame* p_t;
            char psz_tag[4];
            for( ID3v2::FrameList::Iterator iter = list.begin();
                    iter != list.end(); iter++ )
            {
                p_t = dynamic_cast<ID3v2::Frame*> (*iter);
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

    else if( Ogg::Vorbis::File *p_ogg_v =
        dynamic_cast<Ogg::Vorbis::File *>(f.file() ) )
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
    else if( Ogg::FLAC::File *p_ogg_f =
        dynamic_cast<Ogg::FLAC::File *>(f.file() ) )
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
    else if( FLAC::File *p_flac =
        dynamic_cast<FLAC::File *>(f.file() ) )
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

    DetectImage( f, p_demux );

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

    FileRef f( p_export->psz_file );
    if( f.isNull() || !f.tag() || f.file()->readOnly() )
    {
        msg_Err( p_this, "File %s can't be opened for tag writing\n",
            p_export->psz_file );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_this, "Writing metadata for %s", p_export->psz_file );

    Tag *p_tag = f.tag();

    char *psz_meta;

#define SET(a,b) \
        if(b) { \
            String *psz_##a = new String( b, \
                String::UTF8 ); \
            p_tag->set##a( *psz_##a ); \
            delete psz_##a; \
        }


    psz_meta = input_item_GetArtist( p_item );
    SET( Artist, psz_meta );
    free( psz_meta );

    psz_meta = input_item_GetTitle( p_item );
    if( !psz_meta ) psz_meta = input_item_GetName( p_item );
    String *psz_title = new String( psz_meta,
        String::UTF8 );
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

    if( ID3v2::Tag *p_id3tag =
        dynamic_cast<ID3v2::Tag *>(p_tag) )
    {
#define WRITE( foo, bar ) \
        psz_meta = input_item_Get##foo( p_item ); \
        if( psz_meta ) \
        { \
            ByteVector p_byte( bar, 4 ); \
            ID3v2::TextIdentificationFrame p_frame( p_byte ); \
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

