/*****************************************************************************
 * video.cpp: video part of an media player
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
    Exception ex;
    return libvlc_video_get_scale( m_player, &ex.ex );
}

char *Video::aspectRatio()
{
    Exception ex;
    return libvlc_video_get_aspect_ratio( m_player, &ex.ex );
}

void Video::setAspectRatio( const char *aspect_ratio )
{
    Exception ex;
    libvlc_video_set_aspect_ratio( m_player, aspect_ratio, &ex.ex );
}

int Video::spu()
{
    Exception ex;
    return libvlc_video_get_spu( m_player, &ex.ex );
}

int Video::spuCount()
{
    Exception ex;
    return libvlc_video_get_spu_count( m_player, &ex.ex );
}

void Video::setSpu( int spu )
{
    Exception ex;
    libvlc_video_set_spu( m_player, spu, &ex.ex );
}

void Video::setSubtitleFile( const char *subtitle_file )
{
    Exception ex;
    libvlc_video_set_subtitle_file( m_player, subtitle_file, &ex.ex );
}

char *Video::cropGeometry()
{
    Exception ex;
    return libvlc_video_get_crop_geometry( m_player, &ex.ex );
}

void Video::setCropGeometry( const char *geometry )
{
    Exception ex;
    libvlc_video_set_crop_geometry( m_player, geometry, &ex.ex );
}

int Video::track()
{
    Exception ex;
    return libvlc_video_get_track( m_player, &ex.ex );
}

int Video::trackCount()
{
    Exception ex;
    return libvlc_video_get_track_count( m_player, &ex.ex );
}

void Video::setTrack( int track )
{
    Exception ex;
    libvlc_video_set_track( m_player, track, &ex.ex );
}

void Video::snapshot( const char *filepath, int with, int height )
{
    Exception ex;
    libvlc_video_take_snapshot( m_player, filepath, with, height, &ex.ex );
}

void Video::deinterlace( int enable, const char *mode )
{
    Exception ex;
    libvlc_video_set_deinterlace( m_player, enable, mode, &ex.ex );
}
