/*****************************************************************************
 * audio.hpp: audio part of the media player
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

#ifndef LIBVLCPP_AUDIO_HPP
#define LIBVLCPP_AUDIO_HPP

#include <vlc/libvlc.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_player.h>

#include "libvlc.hpp"

namespace libvlc
{

class MediaPlayer;

class Audio
{
public:
    /**
     * Toggle mute status
     */
    void toggleMute();

    /**
     * Get the mute status
     * @return true if the sound is muted
     */
    int mute();

    /**
     * Set the mute status
     * @param mute: true to mute, otherwise unmute
     */
    void setMute( int mute );

    /**
     * Get the current volume
     * @return the current volume
     */
    int volume();

    /**
     * Set the volume
     * @param volume: the new volume
     */
    void setVolume( int volume );


    /**
     * Get the current track
     * @return the current audio track
     */
    int track();

    /**
     * Get the number of audio tracks
     * @return the number of audio tracks
     */
    int trackCount();

    /**
     * Set the audio track
     * @param track: the audio track
     */
    void setTrack( int track );

    /**
     * Get the current audio channel
     * @return the current audio channel
     */
    int channel();

    /**
     * Set the audio channel
     * @param channel: the new audio channel
     */
    void setChannel( int channel );

    /** trackDescription */

private:
    /** The media player instance of libvlc */
    libvlc_media_player_t *m_player;

    /**
     * The constructor is private so only the MediaPlayer can create an instance of this class
     */
    Audio();

    /** Destructor only used by the MediaPlayer associated with this class */
    ~Audio();

    /**
     * Set the media player. This function can only be used by the MediaPlayer class
     * @param player: the media player
     */
    void setMediaPlayer( libvlc_media_player_t *player);

    /** Friend class */
    friend class MediaPlayer;
};

};

#endif // LIBVLCPP_AUDIO_HPP

