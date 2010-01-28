/*****************************************************************************
 * audio.cpp: audio part of the media player
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

#include "audio.hpp"
#include "exception.hpp"


using namespace libvlc;

Audio::Audio( libvlc_instance_t *libvlcInstance, libvlc_media_player_t *player )
{
    m_libvlcInstance = libvlcInstance;
    libvlc_retain( m_libvlcInstance );

    m_player = player;
    libvlc_media_player_retain( m_player );
}

Audio::~Audio()
{
    libvlc_media_player_release( m_player );
    libvlc_release( m_libvlcInstance );
}

void Audio::toggleMute()
{
    libvlc_audio_toggle_mute( m_libvlcInstance );
}

int Audio::mute()
{
    return libvlc_audio_get_mute( m_libvlcInstance );
}

void Audio::setMute( int mute )
{
    libvlc_audio_set_mute( m_libvlcInstance, mute );
}

int Audio::volume()
{
    return libvlc_audio_get_volume( m_libvlcInstance );
}

void Audio::setVolume( int volume )
{
    Exception ex;
    libvlc_audio_set_volume( m_libvlcInstance, volume, &ex.ex );
}

int Audio::track()
{
    Exception ex;
    return libvlc_audio_get_track( m_player, &ex.ex );
}

int Audio::trackCount()
{
    Exception ex;
    return libvlc_audio_get_track_count( m_player, &ex.ex );
}

void Audio::setTrack( int track )
{
    Exception ex;
    libvlc_audio_set_track( m_player, track, &ex.ex );
}

