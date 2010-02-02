/*****************************************************************************
 * video.cpp: video part of the media player
 *****************************************************************************
 * Copyright (C) 2010 the VideoLAN team
 * $Id$
 *
 * Authors: Rémi Duraffort <ivoire@videolan.org>
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

#include <cstdlib>
#include "video.hpp"
#include "exception.hpp"


using namespace libvlc;

Video::Video( libvlc_media_player_t *player )
{
    m_player = player;
    libvlc_media_player_retain( m_player );
}

Video::~Video()
{
    libvlc_media_player_release( m_player );
}

float Video::scale()
{
    return libvlc_video_get_scale( m_player );
}

char *Video::aspectRatio()
{
    return libvlc_video_get_aspect_ratio( m_player );
}

void Video::setAspectRatio( const char *aspect_ratio )
{
    libvlc_video_set_aspect_ratio( m_player, aspect_ratio );
}

int Video::spu()
{
    return libvlc_video_get_spu( m_player );
}

int Video::spuCount()
{
    return libvlc_video_get_spu_count( m_player );
}

void Video::setSpu( int spu )
{
    libvlc_video_set_spu( m_player, spu );
}

void Video::setSubtitleFile( const char *subtitle_file )
{
    libvlc_video_set_subtitle_file( m_player, subtitle_file );
}

char *Video::cropGeometry()
{
    return libvlc_video_get_crop_geometry( m_player );
}

void Video::setCropGeometry( const char *geometry )
{
    libvlc_video_set_crop_geometry( m_player, geometry );
}

int Video::track()
{
    return libvlc_video_get_track( m_player );
}

int Video::trackCount()
{
    return libvlc_video_get_track_count( m_player );
}

void Video::setTrack( int track )
{
    libvlc_video_set_track( m_player, track );
}

int Video::snapshot( int num, const char *filepath, int with, int height )
{
    return libvlc_video_take_snapshot( m_player, num, filepath, with, height );
}

void Video::deinterlace( int enable, const char *mode )
{
    if( enable )
        libvlc_video_set_deinterlace( m_player, mode );
    else
        libvlc_video_set_deinterlace( m_player, NULL );
}
