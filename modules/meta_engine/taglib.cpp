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

static int  ReadMeta ( vlc_object_t * );

vlc_module_begin();
    set_capability( "meta reader", 1000 );
    set_callbacks( ReadMeta, NULL );
vlc_module_end();

bool checkID3Image( const TagLib::ID3v2::Tag *tag )
{
    TagLib::ID3v2::FrameList l = tag->frameListMap()[ "APIC" ];
    return !l.isEmpty();
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

            vlc_meta_SetTitle( p_meta, tag->title().toCString( true ) );
            vlc_meta_SetArtist( p_meta, tag->artist().toCString( true ) );

            if( TagLib::MPEG::File *mpeg =
                           dynamic_cast<TagLib::MPEG::File *>(f.file() ) )
            {
                if( mpeg->ID3v2Tag() && checkID3Image( mpeg->ID3v2Tag() ) )
                {
                    fprintf( stderr, "%s has APIC\n", p_meta->psz_title );
                    vlc_meta_SetArtURL( p_meta, "APIC" ); /// Means that the interface will use us to actually fetch it
                }
            }
            return VLC_SUCCESS;
        }
    }
    return VLC_EGENERIC;
}
