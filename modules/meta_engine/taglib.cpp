/*****************************************************************************
 * taglib.cpp: Taglib tag parser/writer
 *****************************************************************************
 * Copyright (C) 2003-2016 VLC authors and VideoLAN
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Rafaël Carré <funman@videolanorg>
 *          Rémi Duraffort <ivoire@videolan.org>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>              /* demux_meta_t */
#include <vlc_strings.h>            /* vlc_b64_decode_binary */
#include <vlc_input.h>              /* for attachment_new */
#include <vlc_url.h>                /* vlc_uri2path */
#include <vlc_mime.h>               /* mime type */
#include <vlc_fs.h>
#include <vlc_cxx_helpers.hpp>

#include <sys/stat.h>

#ifdef _WIN32
# include <vlc_charset.h>           /* ToWide */
# include <io.h>
#else
# include <unistd.h>
#endif

// Taglib headers
#ifdef _WIN32
# define TAGLIB_STATIC
#endif
#include <taglib.h>

#define VERSION_INT(a, b, c) ((a)<<16 | (b)<<8 | (c))
#define TAGLIB_VERSION VERSION_INT(TAGLIB_MAJOR_VERSION, \
                                   TAGLIB_MINOR_VERSION, \
                                   TAGLIB_PATCH_VERSION)

#include <fileref.h>
#include <tag.h>
#include <tbytevector.h>

/* Support for stream-based metadata */
#include <vlc_access.h>
#include <tiostream.h>

#include <apefile.h>
#include <asffile.h>
#include <apetag.h>
#include <flacfile.h>
#include <mpcfile.h>
#include <mpegfile.h>
#include <mp4file.h>
#include <oggfile.h>
#include <oggflacfile.h>
#include <opusfile.h>
#include "../demux/xiph_metadata.h"

#include <aifffile.h>
#include <wavfile.h>

#include <speexfile.h>
#include <trueaudiofile.h>
#include <vorbisfile.h>
#include <wavpackfile.h>

#include <attachedpictureframe.h>
#include <textidentificationframe.h>
#include <uniquefileidentifierframe.h>

using namespace TagLib;


#include <algorithm>

namespace VLCTagLib
{
    template <class T>
    class ExtResolver : public FileRef::FileTypeResolver
    {
        public:
            ExtResolver(const std::string &);
            ~ExtResolver() {}
            virtual File *createFile(FileName, bool, AudioProperties::ReadStyle) const;

        protected:
            std::string ext;
    };
}

template <class T>
VLCTagLib::ExtResolver<T>::ExtResolver(const std::string & ext) : FileTypeResolver()
{
    this->ext = ext;
    std::transform(this->ext.begin(), this->ext.end(), this->ext.begin(), ::toupper);
}

template <class T>
File *VLCTagLib::ExtResolver<T>::createFile(FileName fileName, bool, AudioProperties::ReadStyle) const
{
    std::string filename = std::string(fileName);
    std::size_t namesize = filename.size();

    if (namesize > ext.length())
    {
        std::string fext = filename.substr(namesize - ext.length(), ext.length());
        std::transform(fext.begin(), fext.end(), fext.begin(), ::toupper);
        if(fext == ext)
            return new T(fileName, false, AudioProperties::Fast);
    }

    return 0;
}

static VLCTagLib::ExtResolver<MPEG::File> aacresolver(".aac");
static bool b_extensions_registered = false;

// taglib is not thread safe
static vlc::threads::mutex taglib_lock;

// Local functions
static int ReadMeta    ( vlc_object_t * );
static int WriteMeta   ( vlc_object_t * );

vlc_module_begin ()
    set_capability( "meta reader", 1000 )
    set_callback( ReadMeta )
    add_submodule ()
        set_capability( "meta writer", 50 )
        set_callback( WriteMeta )
vlc_module_end ()

class VlcIostream : public IOStream
{
public:
    VlcIostream(stream_t* p_stream)
        : m_stream( p_stream )
        , m_previousPos( 0 )
    {
    }

    ~VlcIostream()
    {
        vlc_stream_Delete( m_stream );
    }

    FileName name() const
    {
        // Taglib only cares about the file name part, so it doesn't matter
        // whether we include the mrl scheme or not
        return m_stream->psz_url;
    }

    ByteVector readBlock(ulong length)
    {
        ByteVector res(length, 0);
        ssize_t i_read = vlc_stream_Read( m_stream, res.data(), length);
        if (i_read < 0)
            return ByteVector::null;
        else if ((size_t)i_read != length)
            res.resize(i_read);
        return res;
    }

    void writeBlock(const ByteVector&)
    {
        // Let's stay Read-Only for now
    }

    void insert(const ByteVector&, ulong, ulong)
    {
    }

    void removeBlock(ulong, ulong)
    {
    }

    bool readOnly() const
    {
        return true;
    }

    bool isOpen() const
    {
        return true;
    }

    void seek(long offset, Position p)
    {
        uint64_t pos = 0;
        switch (p)
        {
            case Current:
                pos = m_previousPos;
                break;
            case End:
                pos = length();
                break;
            default:
                break;
        }
        if (vlc_stream_Seek( m_stream, pos + offset ) == 0)
            m_previousPos = pos + offset;
    }

    void clear()
    {
        return;
    }

    long tell() const
    {
        return m_previousPos;
    }

    long length()
    {
        uint64_t i_size;
        if (vlc_stream_GetSize( m_stream, &i_size ) != VLC_SUCCESS)
            return -1;
        return i_size;
    }

    void truncate(long)
    {
    }

private:
    stream_t* m_stream;
    int64_t m_previousPos;
};

static int ExtractCoupleNumberValues( vlc_meta_t* p_meta, const char *psz_value,
        vlc_meta_type_t first, vlc_meta_type_t second)
{
    unsigned int i_trknum, i_trktot;

    int i_ret = sscanf( psz_value, "%u/%u", &i_trknum, &i_trktot );
    char psz_trck[11];
    if( i_ret >= 1 )
    {
        snprintf( psz_trck, sizeof( psz_trck ), "%u", i_trknum );
        vlc_meta_Set( p_meta, first, psz_trck );
    }
    if( i_ret == 2)
    {
        snprintf( psz_trck, sizeof( psz_trck ), "%u", i_trktot );
        vlc_meta_Set( p_meta, second, psz_trck );
    }
    return i_ret;
}

/**
 * Read meta information from APE tags
 * @param tag: the APE tag
 * @param p_demux_meta: the demuxer meta
 * @param p_meta: the meta
 */
static void ReadMetaFromAPE( APE::Tag* tag, demux_meta_t* p_demux_meta, vlc_meta_t* p_meta )
{
    APE::ItemListMap fields ( tag->itemListMap() );
    APE::ItemListMap::Iterator iter;

    iter = fields.find("COVER ART (FRONT)");
    if( iter != fields.end()
        && !iter->second.isEmpty()
        && iter->second.type() == APE::Item::Binary)
    {
        input_attachment_t *p_attachment;

        const ByteVector picture = iter->second.binaryData();
        const char *p_data = picture.data();
        unsigned i_data = picture.size();

        /* Null terminated filename followed by the image data */
        size_t desc_len = strnlen(p_data, i_data);
        if( desc_len < i_data && IsUTF8( p_data ) )
        {
            const char *psz_name = p_data;
            const char *psz_mime = vlc_mime_Ext2Mime( psz_name );
            p_data += desc_len + 1; /* '\0' */
            i_data -= desc_len + 1;

            msg_Dbg( p_demux_meta, "Found embedded art: %s (%s) is %u bytes",
                     psz_name, psz_mime, i_data );

            p_attachment = vlc_input_attachment_New( psz_name, psz_mime,
                                    psz_name, p_data, i_data );
            if( p_attachment )
            {
                TAB_APPEND_CAST( (input_attachment_t**),
                                 p_demux_meta->i_attachments, p_demux_meta->attachments,
                                 p_attachment );

                char *psz_url;
                if( asprintf( &psz_url, "attachment://%s", p_attachment->psz_name ) != -1 )
                {
                    vlc_meta_SetArtURL( p_meta, psz_url );
                    free( psz_url );
                }
            }
        }

        fields.erase(iter);
    }

#define SET( keyName, metaName ) \
    iter = fields.find(keyName); \
    if( iter != fields.end() && !iter->second.isEmpty() ) { \
        vlc_meta_Set##metaName( p_meta, iter->second.toString().toCString( true ) ); \
        fields.erase(iter); \
    }

#define SET_EXTRA( keyName, metaName ) \
    iter = fields.find( keyName ); \
    if( iter != fields.end() && !iter->second.isEmpty() ) { \
        vlc_meta_AddExtra( p_meta, metaName, iter->second.toString().toCString( true ) ); \
        fields.erase(iter); \
    }

    SET( "ALBUM", Album );
    SET( "ARTIST", Artist );
    SET( "COMMENT", Description );
    SET( "GENRE", Genre );
    SET( "TITLE", Title );
    SET( "COPYRIGHT", Copyright );
    SET( "LANGUAGE", Language );
    SET( "PUBLISHER", Publisher );
    SET( "MUSICBRAINZ_TRACKID", TrackID );

    SET_EXTRA( "MUSICBRAINZ_ALBUMID", VLC_META_EXTRA_MB_ALBUMID );

#undef SET
#undef SET_EXTRA

    /* */
    iter = fields.find( "TRACK" );
    if( iter != fields.end() && !iter->second.isEmpty() )
    {
        ExtractCoupleNumberValues( p_meta, iter->second.toString().toCString( true ),
                vlc_meta_TrackNumber, vlc_meta_TrackTotal );
        fields.erase( iter );
    }

    /* Remainings */
    for( iter = fields.begin(); iter != fields.end(); ++iter )
    {
        if( iter->second.isEmpty() )
            continue;

        if( iter->second.type() != APE::Item::Text )
            continue;

        vlc_meta_AddExtra( p_meta,
                           iter->first.toCString( true ),
                           iter->second.toString().toCString( true ) );
    }
}


/**
 * Read meta information from ASF tags
 * @param tag: the ASF tag
 * @param p_demux_meta: the demuxer meta
 * @param p_meta: the meta
 */
static void ReadMetaFromASF( ASF::Tag* tag, demux_meta_t* p_demux_meta, vlc_meta_t* p_meta )
{

    ASF::AttributeList list;
#define SET( keyName, metaName )                                                     \
    if( tag->attributeListMap().contains(keyName) )                                  \
    {                                                                                \
        list = tag->attributeListMap()[keyName];                                     \
        vlc_meta_Set##metaName( p_meta, list.front().toString().toCString( true ) ); \
    }

#define SET_EXTRA( keyName, metaName )                                                     \
    if( tag->attributeListMap().contains(keyName) )                                  \
    {                                                                                \
        list = tag->attributeListMap()[keyName];                                     \
        vlc_meta_AddExtra( p_meta, metaName, list.front().toString().toCString( true ) ); \
    }

    SET("MusicBrainz/Track Id", TrackID );
    SET_EXTRA("MusicBrainz/Album Id", VLC_META_EXTRA_MB_ALBUMID );

#undef SET
#undef SET_EXTRA

    // List the pictures
    list = tag->attributeListMap()["WM/Picture"];
    ASF::AttributeList::Iterator iter;
    for( iter = list.begin(); iter != list.end(); iter++ )
    {
        const ASF::Picture asfPicture = (*iter).toPicture();
        const ByteVector picture = asfPicture.picture();
        const char *psz_mime = asfPicture.mimeType().toCString();
        const char *p_data = picture.data();
        const unsigned i_data = picture.size();
        char *psz_name;
        input_attachment_t *p_attachment;

        if( asfPicture.description().size() > 0 )
            psz_name = strdup( asfPicture.description().toCString( true ) );
        else
        {
            if( asprintf( &psz_name, "%i", asfPicture.type() ) == -1 )
                psz_name = NULL;
        }

        if( unlikely(psz_name == NULL) )
            continue;

        msg_Dbg( p_demux_meta, "Found embedded art: %s (%s) is %u bytes",
                 psz_name, psz_mime, i_data );

        p_attachment = vlc_input_attachment_New( psz_name, psz_mime,
                                psz_name, p_data, i_data );
        if( p_attachment )
            TAB_APPEND_CAST( (input_attachment_t**),
                             p_demux_meta->i_attachments, p_demux_meta->attachments,
                             p_attachment );
        char *psz_url;
        if( asprintf( &psz_url, "attachment://%s", psz_name ) != -1 )
        {
            vlc_meta_SetArtURL( p_meta, psz_url );
            free( psz_url );
        }
        free( psz_name );
    }
}

/**
 * Fills attachments list from ID3 APIC tags
 * @param tag: the APIC tags list
 * @param p_demux_meta: the demuxer meta
 * @param p_meta: the meta
 */
static void ProcessAPICListFromId3v2( const ID3v2::FrameList &list,
                                      demux_meta_t* p_demux_meta, vlc_meta_t* p_meta )
{
    /* Preferred type of image
     * The 21 types are defined in id3v2 standard:
     * http://www.id3.org/id3v2.4.0-frames */
    static const uint8_t scores[] = {
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

    const ID3v2::AttachedPictureFrame *defaultPic = nullptr;
    for( auto iter = list.begin(); iter != list.end(); ++iter )
    {
        const ID3v2::AttachedPictureFrame* p =
                dynamic_cast<const ID3v2::AttachedPictureFrame*>(*iter);
        if( !p )
            continue;
        if(defaultPic == nullptr)
        {
            defaultPic = p;
        }
        else
        {
            int scorea = defaultPic->type() >= ARRAY_SIZE(scores) ? 0 : scores[defaultPic->type()];
            int scoreb = p->type() >= ARRAY_SIZE(scores) ? 0 : scores[p->type()];
            if(scoreb > scorea)
                defaultPic = p;
        }
    }

    for( auto iter = list.begin(); iter != list.end(); ++iter )
    {
        const ID3v2::AttachedPictureFrame* p =
                dynamic_cast<const ID3v2::AttachedPictureFrame*>(*iter);
        if( !p )
            continue;
        // Get the mime and description of the image.
        String description = p->description();
        String mimeType = p->mimeType();

        /* some old iTunes version not only sets incorrectly the mime type
         * or the description of the image,
         * but also embeds incorrectly the image.
         * Recent versions seem to behave correctly */
        if( mimeType == "PNG" || description == "\xC2\x89PNG" )
        {
            msg_Warn( p_demux_meta, "Invalid picture embedded by broken iTunes version" );
            continue;
        }

        char *psz_name;
        if( asprintf( &psz_name, "%i", p_demux_meta->i_attachments ) == -1 )
            continue;

        input_attachment_t *p_attachment =
                vlc_input_attachment_New( psz_name,
                                          mimeType.toCString(),
                                          description.toCString(),
                                          p->picture().data(),
                                          p->picture().size() );
        free( psz_name );
        if( !p_attachment )
            continue;

        msg_Dbg( p_demux_meta, "Found embedded art: %s (%zu bytes)",
                 p_attachment->psz_mime, p_attachment->i_data );

        TAB_APPEND_CAST( (input_attachment_t**),
                         p_demux_meta->i_attachments, p_demux_meta->attachments,
                         p_attachment );

        if( p == defaultPic )
        {
            char *psz_url;
            if( asprintf( &psz_url, "attachment://%s",
                          p_attachment->psz_name ) == -1 )
                continue;
            vlc_meta_SetArtURL( p_meta, psz_url );
            free( psz_url );
        }
    }
}

/**
 * Read meta information from id3v2 tags
 * @param tag: the id3v2 tag
 * @param p_demux_meta: the demuxer meta
 * @param p_meta: the meta
 */
static void ReadMetaFromId3v2( ID3v2::Tag* tag, demux_meta_t* p_demux_meta, vlc_meta_t* p_meta )
{
    // Get the unique file identifier
    ID3v2::FrameList list = tag->frameListMap()["UFID"];
    ID3v2::FrameList::Iterator iter;
    for( iter = list.begin(); iter != list.end(); iter++ )
    {
        ID3v2::UniqueFileIdentifierFrame* p_ufid =
                dynamic_cast<ID3v2::UniqueFileIdentifierFrame*>(*iter);
        if( !p_ufid )
            continue;
        const char *owner = p_ufid->owner().toCString();
        if (!strcmp( owner, "http://musicbrainz.org" ))
        {
            /* ID3v2 UFID contains up to 64 bytes binary data
             * but in our case it will be a '\0'
             * terminated string */
            char psz_ufid[64];
            int max_size = __MIN( p_ufid->identifier().size(), 63);
            strncpy( psz_ufid, p_ufid->identifier().data(), max_size );
            psz_ufid[max_size] = '\0';
            vlc_meta_SetTrackID( p_meta, psz_ufid );
        }
    }

    // Get the use text
    list = tag->frameListMap()["TXXX"];
    for( iter = list.begin(); iter != list.end(); iter++ )
    {
        ID3v2::UserTextIdentificationFrame* p_txxx =
                dynamic_cast<ID3v2::UserTextIdentificationFrame*>(*iter);
        if( !p_txxx )
            continue;
        if( !strcmp( p_txxx->description().toCString( true ), "TRACKTOTAL" ) )
        {
            vlc_meta_Set( p_meta, vlc_meta_TrackTotal, p_txxx->fieldList().back().toCString( true ) );
            continue;
        }
        if( !strcmp( p_txxx->description().toCString( true ), "MusicBrainz Album Id" ) )
        {
            vlc_meta_AddExtra( p_meta, VLC_META_EXTRA_MB_ALBUMID, p_txxx->fieldList().back().toCString( true ) );
            continue;
        }
        vlc_meta_AddExtra( p_meta, p_txxx->description().toCString( true ),
                           p_txxx->fieldList().back().toCString( true ) );
    }

    // Get some more information
#define SET( tagName, metaName )                                               \
    list = tag->frameListMap()[tagName];                                       \
    if( !list.isEmpty() )                                                      \
        vlc_meta_Set##metaName( p_meta,                                        \
                                (*list.begin())->toString().toCString( true ) );

#define SET_EXTRA( tagName, metaName )\
    list = tag->frameListMap()[tagName];\
    if( !list.isEmpty() )\
        vlc_meta_AddExtra( p_meta, metaName,\
                           (*list.begin())->toString().toCString( true ) );


    SET( "TCOP", Copyright );
    SET( "TENC", EncodedBy );
    SET( "TLAN", Language );
    SET( "TPUB", Publisher );
    SET( "TPE2", AlbumArtist );
    SET_EXTRA( "USLT", "Lyrics" );

#undef SET_EXTRA
#undef SET

    /* */
    list = tag->frameListMap()["TRCK"];
    if( !list.isEmpty() )
    {
        ExtractCoupleNumberValues( p_meta, (*list.begin())->toString().toCString( true ),
                vlc_meta_TrackNumber, vlc_meta_TrackTotal );
    }

    /* */
    list = tag->frameListMap()["TPOS"];
    if( !list.isEmpty() )
    {
        ExtractCoupleNumberValues( p_meta, (*list.begin())->toString().toCString( true ),
                vlc_meta_DiscNumber, vlc_meta_DiscTotal );
    }

    // Try now to get embedded art
    list = tag->frameListMap()[ "APIC" ];
    if( !list.isEmpty() )
        ProcessAPICListFromId3v2( list, p_demux_meta, p_meta );
}

/**
 * Read the meta information from XiphComments
 * @param tag: the Xiph Comment
 * @param p_demux_meta: the demuxer meta
 * @param p_meta: the meta
 */
static void ReadMetaFromXiph( Ogg::XiphComment* tag, demux_meta_t* p_demux_meta, vlc_meta_t* p_meta )
{
    bool hasTrackTotal = false;
#define SET( keyName, metaName ) \
    { \
        StringList tmp_list { tag->fieldListMap()[keyName] }; \
        if( !tmp_list.isEmpty() ) \
            vlc_meta_Set##metaName( p_meta, (*tmp_list.begin()).toCString( true ) ); \
    }

#define SET_EXTRA( keyName, metaName ) \
    { \
        StringList tmp_list = tag->fieldListMap()[keyName]; \
        if( !tmp_list.isEmpty() ) \
            vlc_meta_AddExtra( p_meta, keyName, (*tmp_list.begin()).toCString( true ) ); \
    }

    SET( "COPYRIGHT", Copyright );
    SET( "ORGANIZATION", Publisher );
    SET( "DATE", Date );
    SET( "ENCODER", EncodedBy );
    SET( "RATING", Rating );
    SET( "LANGUAGE", Language );
    SET( "MUSICBRAINZ_TRACKID", TrackID );
    SET( "ALBUMARTIST", AlbumArtist );
    SET( "DISCNUMBER", DiscNumber );

    SET_EXTRA( "MUSICBRAINZ_ALBUMID", VLC_META_EXTRA_MB_ALBUMID );
#undef SET
#undef SET_EXTRA

    StringList track_number_list = tag->fieldListMap()["TRACKNUMBER"];
    if( !track_number_list.isEmpty() )
    {
        int i_values = ExtractCoupleNumberValues( p_meta, (*track_number_list.begin()).toCString( true ),
                 vlc_meta_TrackNumber, vlc_meta_TrackTotal );
        hasTrackTotal = i_values == 2;
    }
    if( !hasTrackTotal )
    {
        StringList track_total_list { tag->fieldListMap()["TRACKTOTAL"] };
        if( track_total_list.isEmpty() )
        {
            StringList total_tracks_list { tag->fieldListMap()["TOTALTRACKS"] };
            if( !total_tracks_list.isEmpty() )
                vlc_meta_SetTrackTotal( p_meta, (*total_tracks_list.begin()).toCString( true ) );
        }
        else
        {
            vlc_meta_SetTrackTotal( p_meta, (*track_total_list.begin()).toCString( true ) );
        }
    }

    // Try now to get embedded art
    StringList mime_list { tag->fieldListMap()[ "COVERARTMIME" ] };
    StringList art_list { tag->fieldListMap()[ "COVERART" ] };

    input_attachment_t *p_attachment;

    if( mime_list.size() != 0 && art_list.size() != 0 )
    {
        // We get only the first covert art
        if( mime_list.size() > 1 || art_list.size() > 1 )
            msg_Warn( p_demux_meta, "Found %i embedded arts, so using only the first one",
                    art_list.size() );

        const char* psz_name = "cover";
        const char* psz_mime = mime_list[0].toCString(true);
        const char* psz_description = "cover";

        uint8_t *p_data;
        int i_data = vlc_b64_decode_binary( &p_data, art_list[0].toCString(true) );

        msg_Dbg( p_demux_meta, "Found embedded art: %s (%s) is %i bytes",
                psz_name, psz_mime, i_data );

        p_attachment = vlc_input_attachment_New( psz_name, psz_mime,
                psz_description, p_data, i_data );
        free( p_data );
    }
    else
    {
        StringList block_picture_list { tag->fieldListMap()[ "METADATA_BLOCK_PICTURE" ] };
        if( block_picture_list.size() == 0 )
            return;

        uint8_t *p_data;
        int i_cover_score;
        int i_cover_idx;
        int i_data = vlc_b64_decode_binary( &p_data, block_picture_list[0].toCString(true) );
        i_cover_score = i_cover_idx = 0;
        /* TODO: Use i_cover_score / i_cover_idx to select the picture. */
        p_attachment = ParseFlacPicture( p_data, i_data, 0,
            &i_cover_score, &i_cover_idx );
        free( p_data );
    }

    if (p_attachment) {
        TAB_APPEND_CAST( (input_attachment_t**),
                p_demux_meta->i_attachments, p_demux_meta->attachments,
                p_attachment );

        char *psz_url;
        if( asprintf( &psz_url, "attachment://%s", p_attachment->psz_name ) != -1 ) {
            vlc_meta_SetArtURL( p_meta, psz_url );
            free( psz_url );
        }
    }
}

/**
 * Read the meta information from mp4 specific tags
 * @param tag: the mp4 tag
 * @param p_demux_meta: the demuxer meta
 * @param p_meta: the meta
 */
static void ReadMetaFromMP4( MP4::Tag* tag, demux_meta_t *p_demux_meta, vlc_meta_t* p_meta )
{
    MP4::Item list;
#define SET( keyName, metaName )                                                             \
    if( tag->itemListMap().contains(keyName) )                                               \
    {                                                                                        \
        list = tag->itemListMap()[keyName];                                                  \
        vlc_meta_Set##metaName( p_meta, list.toStringList().front().toCString( true ) );     \
    }
#define SET_EXTRA( keyName, metaName )                                                   \
    if( tag->itemListMap().contains(keyName) )                                  \
    {                                                                                \
        list = tag->itemListMap()[keyName];                                     \
        vlc_meta_AddExtra( p_meta, metaName, list.toStringList().front().toCString( true ) ); \
    }

    SET("----:com.apple.iTunes:MusicBrainz Track Id", TrackID );
    SET_EXTRA("----:com.apple.iTunes:MusicBrainz Album Id", VLC_META_EXTRA_MB_ALBUMID );

#undef SET
#undef SET_EXTRA

    if( tag->itemListMap().contains("covr") )
    {
        MP4::CoverArtList list = tag->itemListMap()["covr"].toCoverArtList();
        const char *psz_format = list[0].format() == MP4::CoverArt::PNG ? "image/png" : "image/jpeg";

        msg_Dbg( p_demux_meta, "Found embedded art (%s) is %i bytes",
                 psz_format, list[0].data().size() );

        input_attachment_t *p_attachment =
                vlc_input_attachment_New( "cover", psz_format, "cover",
                                          list[0].data().data(), list[0].data().size() );
        if( p_attachment )
        {
            TAB_APPEND_CAST( (input_attachment_t**),
                             p_demux_meta->i_attachments, p_demux_meta->attachments,
                             p_attachment );
            vlc_meta_SetArtURL( p_meta, "attachment://cover" );
        }
    }
}

/**
 * Get the tags from the file using TagLib
 * @param p_this: the demux object
 * @return VLC_SUCCESS if the operation success
 */
static int ReadMeta( vlc_object_t* p_this)
{
    vlc::threads::mutex_locker locker(taglib_lock);
    demux_meta_t*   p_demux_meta = (demux_meta_t *)p_this;
    vlc_meta_t*     p_meta;
    FileRef f;

    p_demux_meta->p_meta = NULL;

    char *psz_uri = input_item_GetURI( p_demux_meta->p_item );
    if( unlikely(psz_uri == NULL) )
        return VLC_ENOMEM;

    if( !b_extensions_registered )
    {
        FileRef::addFileTypeResolver( &aacresolver );
        b_extensions_registered = true;
    }

    stream_t *p_stream = vlc_access_NewMRL( p_this, psz_uri );
    free( psz_uri );
    if( p_stream == NULL )
        return VLC_EGENERIC;
    stream_t* p_filter = vlc_stream_FilterNew( p_stream, "prefetch,cache" );
    if( p_filter )
        p_stream = p_filter;

    VlcIostream s( p_stream );
    f = FileRef( &s );

    if( f.isNull() )
        return VLC_EGENERIC;
    if( !f.tag() || f.tag()->isEmpty() )
        return VLC_EGENERIC;

    p_demux_meta->p_meta = p_meta = vlc_meta_New();
    if( !p_meta )
        return VLC_ENOMEM;


    // Read the tags from the file
    Tag* p_tag = f.tag();

#define SET( tag, meta )                                                       \
    if( !p_tag->tag().isNull() && !p_tag->tag().isEmpty() )                    \
        vlc_meta_Set##meta( p_meta, p_tag->tag().toCString(true) )
#define SETINT( tag, meta )                                                    \
    if( p_tag->tag() )                                                         \
    {                                                                          \
        char psz_tmp[10];                                                      \
        snprintf( psz_tmp, 10, "%d", p_tag->tag() );                           \
        vlc_meta_Set##meta( p_meta, psz_tmp );                                 \
    }

    SET( title, Title );
    SET( artist, Artist );
    SET( album, Album );
    SET( comment, Description );
    SET( genre, Genre );
    SETINT( year, Date );
    SETINT( track, TrackNum );

#undef SETINT
#undef SET

    TAB_INIT( p_demux_meta->i_attachments, p_demux_meta->attachments );

    if( APE::File* ape = dynamic_cast<APE::File*>(f.file()) )
    {
        if( ape->APETag() )
            ReadMetaFromAPE( ape->APETag(), p_demux_meta, p_meta );
    }
    else
    if( ASF::File* asf = dynamic_cast<ASF::File*>(f.file()) )
    {
        if( asf->tag() )
            ReadMetaFromASF( asf->tag(), p_demux_meta, p_meta );
    }
    else
    if( FLAC::File* flac = dynamic_cast<FLAC::File*>(f.file()) )
    {
        if( flac->ID3v2Tag() )
            ReadMetaFromId3v2( flac->ID3v2Tag(), p_demux_meta, p_meta );
        else if( flac->xiphComment() )
            ReadMetaFromXiph( flac->xiphComment(), p_demux_meta, p_meta );
    }
    else if( MP4::File *mp4 = dynamic_cast<MP4::File*>(f.file()) )
    {
        if( mp4->tag() )
            ReadMetaFromMP4( mp4->tag(), p_demux_meta, p_meta );
    }
    else if( MPC::File* mpc = dynamic_cast<MPC::File*>(f.file()) )
    {
        if( mpc->APETag() )
            ReadMetaFromAPE( mpc->APETag(), p_demux_meta, p_meta );
    }
    else if( MPEG::File* mpeg = dynamic_cast<MPEG::File*>(f.file()) )
    {
        if( mpeg->APETag() )
            ReadMetaFromAPE( mpeg->APETag(), p_demux_meta, p_meta );
        if( mpeg->ID3v2Tag() )
            ReadMetaFromId3v2( mpeg->ID3v2Tag(), p_demux_meta, p_meta );
    }
    else if( dynamic_cast<Ogg::File*>(f.file()) )
    {
        if( Ogg::FLAC::File* ogg_flac = dynamic_cast<Ogg::FLAC::File*>(f.file()))
            ReadMetaFromXiph( ogg_flac->tag(), p_demux_meta, p_meta );
        else if( Ogg::Speex::File* ogg_speex = dynamic_cast<Ogg::Speex::File*>(f.file()) )
            ReadMetaFromXiph( ogg_speex->tag(), p_demux_meta, p_meta );
        else if( Ogg::Vorbis::File* ogg_vorbis = dynamic_cast<Ogg::Vorbis::File*>(f.file()) )
            ReadMetaFromXiph( ogg_vorbis->tag(), p_demux_meta, p_meta );
#if defined(TAGLIB_OPUSFILE_H)
        else if( Ogg::Opus::File* ogg_opus = dynamic_cast<Ogg::Opus::File*>(f.file()) )
            ReadMetaFromXiph( ogg_opus->tag(), p_demux_meta, p_meta );
#endif
    }
    else if( dynamic_cast<RIFF::File*>(f.file()) )
    {
        if( RIFF::AIFF::File* riff_aiff = dynamic_cast<RIFF::AIFF::File*>(f.file()) )
            ReadMetaFromId3v2( riff_aiff->tag(), p_demux_meta, p_meta );
        else if( RIFF::WAV::File* riff_wav = dynamic_cast<RIFF::WAV::File*>(f.file()) )
            ReadMetaFromId3v2( riff_wav->tag(), p_demux_meta, p_meta );
    }
    else if( TrueAudio::File* trueaudio = dynamic_cast<TrueAudio::File*>(f.file()) )
    {
        if( trueaudio->ID3v2Tag() )
            ReadMetaFromId3v2( trueaudio->ID3v2Tag(), p_demux_meta, p_meta );
    }
    else if( WavPack::File* wavpack = dynamic_cast<WavPack::File*>(f.file()) )
    {
        if( wavpack->APETag() )
            ReadMetaFromAPE( wavpack->APETag(), p_demux_meta, p_meta );
    }

    return VLC_SUCCESS;
}


/**
 * Write meta information to APE tags
 * @param tag: the APE tag
 * @param p_item: the input item
 */
static void WriteMetaToAPE( APE::Tag* tag, input_item_t* p_item )
{
    char* psz_meta;
#define WRITE( metaName, keyName )                      \
    psz_meta = input_item_Get##metaName( p_item );      \
    if( psz_meta )                                      \
    {                                                   \
        String key( keyName, String::UTF8 );            \
        String value( psz_meta, String::UTF8 );         \
        tag->addValue( key, value, true );              \
    }                                                   \
    free( psz_meta );

    WRITE( Copyright, "COPYRIGHT" );
    WRITE( Language, "LANGUAGE" );
    WRITE( Publisher, "PUBLISHER" );
    WRITE( TrackID, "MUSICBRAINZ_TRACKID" );
#undef WRITE
}


/**
 * Write meta information to id3v2 tags
 * @param tag: the id3v2 tag
 * @param p_input: the input item
 */
static void WriteMetaToId3v2( ID3v2::Tag* tag, input_item_t* p_item )
{
    char* psz_meta;
#define WRITE( metaName, tagName )                                            \
    psz_meta = input_item_Get##metaName( p_item );                            \
    if( psz_meta )                                                            \
    {                                                                         \
        ByteVector p_byte( tagName, 4 );                                      \
        tag->removeFrames( p_byte );                                         \
        ID3v2::TextIdentificationFrame* p_frame =                             \
            new ID3v2::TextIdentificationFrame( p_byte, String::UTF8 );       \
        p_frame->setText( psz_meta );                                         \
        tag->addFrame( p_frame );                                             \
    }                                                                         \
    free( psz_meta );

    WRITE( Copyright, "TCOP" );
    WRITE( EncodedBy, "TENC" );
    WRITE( Language,  "TLAN" );
    WRITE( Publisher, "TPUB" );

#undef WRITE
    /* Known TXXX frames */
    ID3v2::FrameList list = tag->frameListMap()["TXXX"];

#define WRITETXXX( metaName, txxName )\
    psz_meta = input_item_Get##metaName( p_item );                                       \
    if ( psz_meta )                                                                      \
    {                                                                                    \
        ID3v2::UserTextIdentificationFrame *p_txxx;                                      \
        for( ID3v2::FrameList::Iterator iter = list.begin(); iter != list.end(); iter++ )\
        {                                                                                \
            p_txxx = dynamic_cast<ID3v2::UserTextIdentificationFrame*>(*iter);           \
            if( !p_txxx )                                                                \
                continue;                                                                \
            if( !strcmp( p_txxx->description().toCString( true ), txxName ) )            \
            {                                                                            \
                p_txxx->setText( psz_meta );                                             \
                FREENULL( psz_meta );                                                    \
                break;                                                                   \
            }                                                                            \
        }                                                                                \
        if( psz_meta ) /* not found in existing custom fields */                         \
        {                                                                                \
            ByteVector p_byte( "TXXX", 4 );                                              \
            p_txxx = new ID3v2::UserTextIdentificationFrame( p_byte );                   \
            p_txxx->setDescription( txxName );                                           \
            p_txxx->setText( psz_meta );                                                 \
            free( psz_meta );                                                            \
            tag->addFrame( p_txxx );                                                     \
        }                                                                                \
    }

    WRITETXXX( TrackTotal, "TRACKTOTAL" );

#undef WRITETXXX

    /* Write album art */
    char *psz_url = input_item_GetArtworkURL( p_item );
    if( psz_url == NULL )
        return;

    char *psz_path = vlc_uri2path( psz_url );
    free( psz_url );
    if( psz_path == NULL )
        return;

    const char *psz_mime = vlc_mime_Ext2Mime( psz_path );

    FILE *p_file = vlc_fopen( psz_path, "rb" );
    if( p_file == NULL )
    {
        free( psz_path );
        return;
    }

    struct stat st;
    if( vlc_stat( psz_path, &st ) == -1 )
    {
        free( psz_path );
        fclose( p_file );
        return;
    }
    off_t file_size = st.st_size;

    free( psz_path );

    /* Limit picture size to 10MiB */
    if( file_size > 10485760 )
    {
      fclose( p_file );
      return;
    }

    char *p_buffer = new (std::nothrow) char[file_size];
    if( p_buffer == NULL )
    {
        fclose( p_file );
        return;
    }

    if( fread( p_buffer, 1, file_size, p_file ) != (unsigned)file_size )
    {
        fclose( p_file );
        delete[] p_buffer;
        return;
    }
    fclose( p_file );

    ByteVector data( p_buffer, file_size );
    delete[] p_buffer;

    ID3v2::FrameList frames = tag->frameList( "APIC" );
    ID3v2::AttachedPictureFrame *frame = NULL;
    if( frames.isEmpty() )
    {
        frame = new TagLib::ID3v2::AttachedPictureFrame;
        tag->addFrame( frame );
    }
    else
    {
        frame = static_cast<ID3v2::AttachedPictureFrame *>( frames.back() );
    }

    frame->setPicture( data );
    frame->setMimeType( psz_mime );
}


/**
 * Write the meta information to XiphComments
 * @param tag: the Xiph Comment
 * @param p_input: the input item
 */
static void WriteMetaToXiph( Ogg::XiphComment* tag, input_item_t* p_item )
{
    char* psz_meta;
#define WRITE( metaName, keyName )                      \
    psz_meta = input_item_Get##metaName( p_item );      \
    if( psz_meta )                                      \
    {                                                   \
        String key( keyName, String::UTF8 );            \
        String value( psz_meta, String::UTF8 );         \
        tag->addField( key, value, true );              \
    }                                                   \
    free( psz_meta );

    WRITE( TrackNum, "TRACKNUMBER" );
    WRITE( TrackTotal, "TRACKTOTAL" );
    WRITE( Copyright, "COPYRIGHT" );
    WRITE( Publisher, "ORGANIZATION" );
    WRITE( Date, "DATE" );
    WRITE( EncodedBy, "ENCODER" );
    WRITE( Rating, "RATING" );
    WRITE( Language, "LANGUAGE" );
    WRITE( TrackID, "MUSICBRAINZ_TRACKID" );
#undef WRITE
}


/**
 * Set the tags to the file using TagLib
 * @param p_this: the demux object
 * @return VLC_SUCCESS if the operation success
 */

static int WriteMeta( vlc_object_t *p_this )
{
    vlc::threads::mutex_locker locker(taglib_lock);
    meta_export_t *p_export = (meta_export_t *)p_this;
    input_item_t *p_item = p_export->p_item;
    FileRef f;

    if( !p_item )
    {
        msg_Err( p_this, "Can't save meta data of an empty input" );
        return VLC_EGENERIC;
    }

#if defined(_WIN32)
    wchar_t *wpath = ToWide( p_export->psz_file );
    if( wpath == NULL )
        return VLC_EGENERIC;
    f = FileRef( wpath, false );
    free( wpath );
#else
    f = FileRef( p_export->psz_file, false );
#endif

    if( f.isNull() || !f.tag() || f.file()->readOnly() )
    {
        msg_Err( p_this, "File %s can't be opened for tag writing",
                 p_export->psz_file );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_this, "Writing metadata for %s", p_export->psz_file );

    Tag *p_tag = f.tag();

    char *psz_meta;

#define SET( a, b )                                             \
    psz_meta = input_item_Get ## a( p_item );                   \
    if( psz_meta )                                              \
    {                                                           \
        String tmp( psz_meta, String::UTF8 );                   \
        p_tag->set##b( tmp );                                   \
    }                                                           \
    free( psz_meta );

    // Saving all common fields
    // If the title is empty, use the name
    SET( TitleFbName, Title );
    SET( Artist, Artist );
    SET( Album, Album );
    SET( Description, Comment );
    SET( Genre, Genre );

#undef SET

    psz_meta = input_item_GetDate( p_item );
    if( !EMPTY_STR(psz_meta) ) p_tag->setYear( atoi( psz_meta ) );
    else p_tag->setYear( 0 );
    free( psz_meta );

    psz_meta = input_item_GetTrackNum( p_item );
    if( !EMPTY_STR(psz_meta) ) p_tag->setTrack( atoi( psz_meta ) );
    else p_tag->setTrack( 0 );
    free( psz_meta );


    // Try now to write special tags
    if( APE::File* ape = dynamic_cast<APE::File*>(f.file()) )
    {
        if( ape->APETag() )
            WriteMetaToAPE( ape->APETag(), p_item );
    }
    else
    if( FLAC::File* flac = dynamic_cast<FLAC::File*>(f.file()) )
    {
        if( flac->ID3v2Tag() )
            WriteMetaToId3v2( flac->ID3v2Tag(), p_item );
        else if( flac->xiphComment() )
            WriteMetaToXiph( flac->xiphComment(), p_item );
    }
    else if( MPC::File* mpc = dynamic_cast<MPC::File*>(f.file()) )
    {
        if( mpc->APETag() )
            WriteMetaToAPE( mpc->APETag(), p_item );
    }
    else if( MPEG::File* mpeg = dynamic_cast<MPEG::File*>(f.file()) )
    {
        if( mpeg->ID3v2Tag() )
            WriteMetaToId3v2( mpeg->ID3v2Tag(), p_item );
        else if( mpeg->APETag() )
            WriteMetaToAPE( mpeg->APETag(), p_item );
    }
    else if( dynamic_cast<Ogg::File*>(f.file()) )
    {
        if( Ogg::FLAC::File* ogg_flac = dynamic_cast<Ogg::FLAC::File*>(f.file()))
            WriteMetaToXiph( ogg_flac->tag(), p_item );
        else if( Ogg::Speex::File* ogg_speex = dynamic_cast<Ogg::Speex::File*>(f.file()) )
            WriteMetaToXiph( ogg_speex->tag(), p_item );
        else if( Ogg::Vorbis::File* ogg_vorbis = dynamic_cast<Ogg::Vorbis::File*>(f.file()) )
            WriteMetaToXiph( ogg_vorbis->tag(), p_item );
#if defined(TAGLIB_OPUSFILE_H)
        else if( Ogg::Opus::File* ogg_opus = dynamic_cast<Ogg::Opus::File*>(f.file()) )
            WriteMetaToXiph( ogg_opus->tag(), p_item );
#endif
    }
    else if( dynamic_cast<RIFF::File*>(f.file()) )
    {
        if( RIFF::AIFF::File* riff_aiff = dynamic_cast<RIFF::AIFF::File*>(f.file()) )
            WriteMetaToId3v2( riff_aiff->tag(), p_item );
        else if( RIFF::WAV::File* riff_wav = dynamic_cast<RIFF::WAV::File*>(f.file()) )
            WriteMetaToId3v2( riff_wav->tag(), p_item );
    }
    else if( TrueAudio::File* trueaudio = dynamic_cast<TrueAudio::File*>(f.file()) )
    {
        if( trueaudio->ID3v2Tag() )
            WriteMetaToId3v2( trueaudio->ID3v2Tag(), p_item );
    }
    else if( WavPack::File* wavpack = dynamic_cast<WavPack::File*>(f.file()) )
    {
        if( wavpack->APETag() )
            WriteMetaToAPE( wavpack->APETag(), p_item );
    }

    // Save the meta data
    f.save();

    return VLC_SUCCESS;
}
