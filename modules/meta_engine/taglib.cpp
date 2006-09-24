/*****************************************************************************
 * taglib.cpp: Taglib tag parser/writer
 *****************************************************************************
 * Copyright (C) 2003-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
#include <vlc/input.h>
#include <vlc_meta.h>

#include <fileref.h>
#include <tag.h>
#include <id3v2tag.h>
#include <mpegfile.h>
#include <flacfile.h>

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

static bool checkID3Image( const TagLib::ID3v2::Tag *tag )
{
    TagLib::ID3v2::FrameList l = tag->frameListMap()[ "APIC" ];
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

    if( !strncmp( p_demux->psz_access, "file", 4 ) )
    {
        if( !p_demux->p_private )
            p_demux->p_private = (void*)vlc_meta_New();
        TagLib::FileRef f( p_demux->psz_path );
        if( !f.isNull() && f.tag() )
        {
            TagLib::Tag *tag = f.tag();
            vlc_meta_t *p_meta = (vlc_meta_t *)(p_demux->p_private );

#define SET( foo, bar ) vlc_meta_Set##foo( p_meta, tag->bar ().toCString(true))
            SET( Title, title );
            SET( Artist, artist );
            SET( Album, album );
//            SET( Comment, comment );
            SET( Genre, genre );
//            SET( Year, year ); Gra, this is an int, need to convert
//            SET( Tracknum , track ); Same
#undef SET
            DetectImage( f, p_meta );

            return VLC_SUCCESS;
        }
    }
    return VLC_EGENERIC;
}

static int WriteMeta( vlc_object_t *p_this )
{
    playlist_t *p_playlist = (playlist_t *)p_this;
    meta_export_t *p_export = (meta_export_t *)p_playlist->p_private;
    input_item_t *p_item = p_export->p_item;

    TagLib::FileRef f( p_export->psz_file );
    if( !f.isNull() && f.tag() )
    {
        TagLib::Tag *tag = f.tag();
        tag->setArtist( p_item->p_meta->psz_artist );
        f.save();
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

static int DownloadArt( vlc_object_t *p_this )
{
    /* We need to be passed the file name
     * Fetch the thing from the file, save it to the cache folder
     */
    return VLC_EGENERIC;
}
