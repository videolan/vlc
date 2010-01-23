/*****************************************************************************
 * media_player.cpp: Represent a media player
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

#include "media_player.hpp"
#include "exception.hpp"

using namespace libvlc;

MediaPlayer::MediaPlayer( libVLC &libvlcInstance )
{
    Exception ex;
    m_player = libvlc_media_player_new( libvlcInstance.instance(), &ex.ex );
}

MediaPlayer::MediaPlayer( Media &media )
{
    Exception ex;
    m_player = libvlc_media_player_new_from_media( media.instance(), &ex.ex );
}

MediaPlayer::~MediaPlayer()
{
    libvlc_media_player_release( m_player );
}

void MediaPlayer::setMedia( Media &media )
{
    Exception ex;
    libvlc_media_player_set_media( m_player, media.instance(), &ex.ex );
}

int MediaPlayer::isPlaying()
{
    Exception ex;
    return libvlc_media_player_is_playing( m_player, &ex.ex );
}

void MediaPlayer::play()
{
    Exception ex;
    libvlc_media_player_play( m_player, &ex.ex );
}

void MediaPlayer::pause()
{
    Exception ex;
    libvlc_media_player_pause( m_player, &ex.ex );
}

void MediaPlayer::stop()
{
    Exception ex;
    libvlc_media_player_stop( m_player, &ex.ex );
}

void MediaPlayer::setNSObject( void *drawable )
{
    Exception ex;
    libvlc_media_player_set_nsobject( m_player, drawable, &ex.ex );
}

void* MediaPlayer::nsobject()
{
    return libvlc_media_player_get_nsobject( m_player );
}

void MediaPlayer::setAgl( uint32_t drawable )
{
    Exception ex;
    libvlc_media_player_set_agl( m_player, drawable, &ex.ex );
}

uint32_t MediaPlayer::agl()
{
    return libvlc_media_player_get_agl( m_player );
}

void MediaPlayer::setXWindow( uint32_t drawable )
{
    Exception ex;
    libvlc_media_player_set_xwindow( m_player, drawable, &ex.ex );
}

uint32_t MediaPlayer::xwindow()
{
    return libvlc_media_player_get_xwindow( m_player );
}

void MediaPlayer::setHwnd( void *drawable )
{
    Exception ex;
    libvlc_media_player_set_hwnd( m_player, drawable, &ex.ex );
}

void *MediaPlayer::hwnd()
{
    return libvlc_media_player_get_hwnd( m_player );
}

int64_t MediaPlayer::lenght()
{
    Exception ex;
    return libvlc_media_player_get_length( m_player, &ex.ex );
}

int64_t MediaPlayer::time()
{
    Exception ex;
    return libvlc_media_player_get_time( m_player, &ex.ex );
}

void MediaPlayer::setTime( int64_t new_time )
{
    Exception ex;
    libvlc_media_player_set_time( m_player, new_time, &ex.ex );
}

float MediaPlayer::position()
{
    Exception ex;
    return libvlc_media_player_get_position( m_player, &ex.ex );
}

void MediaPlayer::setPosition( float position )
{
    Exception ex;
    libvlc_media_player_set_position( m_player, position, &ex.ex );
}

int MediaPlayer::chapter()
{
    Exception ex;
    return libvlc_media_player_get_chapter( m_player, &ex.ex );
}

int MediaPlayer::chapterCount()
{
    Exception ex;
    return libvlc_media_player_get_chapter_count( m_player, &ex.ex );
}

int MediaPlayer::willPlay()
{
    Exception ex;
    return libvlc_media_player_will_play( m_player, &ex.ex );
}

