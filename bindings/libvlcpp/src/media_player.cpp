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

using namespace libvlc;

MediaPlayer::MediaPlayer( libVLC &libvlcInstance )
{
    m_player = libvlc_media_player_new( libvlcInstance.m_instance );
    m_audio.setMediaPlayer( m_player );
    m_video.setMediaPlayer( m_player );
}

MediaPlayer::MediaPlayer( Media &media )
{
    m_player = libvlc_media_player_new_from_media( media.m_media );
    m_audio.setMediaPlayer( m_player );
}

MediaPlayer::~MediaPlayer()
{
    stop();
    libvlc_media_player_release( m_player );
}

void MediaPlayer::setMedia( Media &media )
{
    libvlc_media_player_set_media( m_player, media.m_media );
}

int MediaPlayer::isPlaying()
{
    return libvlc_media_player_is_playing( m_player );
}

void MediaPlayer::play()
{
    libvlc_media_player_play( m_player );
}

void MediaPlayer::pause()
{
    libvlc_media_player_pause( m_player );
}

void MediaPlayer::stop()
{
    libvlc_media_player_stop( m_player );
}

void MediaPlayer::setNSObject( void *drawable )
{
    libvlc_media_player_set_nsobject( m_player, drawable );
}

void* MediaPlayer::nsobject()
{
    return libvlc_media_player_get_nsobject( m_player );
}

void MediaPlayer::setAgl( uint32_t drawable )
{
    libvlc_media_player_set_agl( m_player, drawable );
}

uint32_t MediaPlayer::agl()
{
    return libvlc_media_player_get_agl( m_player );
}

void MediaPlayer::setXWindow( uint32_t drawable )
{
    libvlc_media_player_set_xwindow( m_player, drawable );
}

uint32_t MediaPlayer::xwindow()
{
    return libvlc_media_player_get_xwindow( m_player );
}

void MediaPlayer::setHwnd( void *drawable )
{
    libvlc_media_player_set_hwnd( m_player, drawable );
}

void *MediaPlayer::hwnd()
{
    return libvlc_media_player_get_hwnd( m_player );
}

int64_t MediaPlayer::lenght()
{
    return libvlc_media_player_get_length( m_player );
}

int64_t MediaPlayer::time()
{
    return libvlc_media_player_get_time( m_player );
}

void MediaPlayer::setTime( int64_t new_time )
{
    libvlc_media_player_set_time( m_player, new_time );
}

float MediaPlayer::position()
{
    return libvlc_media_player_get_position( m_player );
}

void MediaPlayer::setPosition( float position )
{
    libvlc_media_player_set_position( m_player, position );
}

int MediaPlayer::chapter()
{
    return libvlc_media_player_get_chapter( m_player );
}

int MediaPlayer::chapterCount()
{
    return libvlc_media_player_get_chapter_count( m_player );
}

int MediaPlayer::chapterCount( int title )
{
    return libvlc_media_player_get_chapter_count_for_title( m_player, title );
}

void MediaPlayer::setChapter( int title )
{
    libvlc_media_player_set_chapter( m_player, title );
}

int MediaPlayer::willPlay()
{
    return libvlc_media_player_will_play( m_player );
}

int MediaPlayer::title()
{
    return libvlc_media_player_get_title( m_player );
}

int MediaPlayer::titleCount()
{
    return libvlc_media_player_get_title_count( m_player );
}

void MediaPlayer::setTitle( int title )
{
    libvlc_media_player_set_title( m_player, title );
}

void MediaPlayer::previousChapter()
{
    libvlc_media_player_previous_chapter( m_player );
}

void MediaPlayer::nextChapter()
{
    libvlc_media_player_next_chapter( m_player );
}

float MediaPlayer::rate()
{
    return libvlc_media_player_get_rate( m_player );
}

void MediaPlayer::setRate( float rate )
{
    libvlc_media_player_set_rate( m_player, rate );
}

libvlc_state_t MediaPlayer::state()
{
    return libvlc_media_player_get_state( m_player );
}

float MediaPlayer::fps()
{
    return libvlc_media_player_get_fps( m_player );
}

int MediaPlayer::hasVout()
{
    return libvlc_media_player_has_vout( m_player );
}

int MediaPlayer::isSeekable()
{
    return libvlc_media_player_is_seekable( m_player );
}
int MediaPlayer::canPause()
{
    return libvlc_media_player_can_pause( m_player );
}

void MediaPlayer::nextFrame()
{
    libvlc_media_player_next_frame( m_player );
}

void MediaPlayer::toggleFullscreen()
{
    libvlc_toggle_fullscreen( m_player );
}

void MediaPlayer::enableFullscreen()
{
    libvlc_set_fullscreen( m_player, 1 );
}

void MediaPlayer::disableFullscreen()
{
    libvlc_set_fullscreen( m_player, 0 );
}

int MediaPlayer::fullscreen()
{
    return libvlc_get_fullscreen( m_player );
}

Audio &MediaPlayer::audio()
{
    return m_audio;
}

Video &MediaPlayer::video()
{
    return m_video;
}
