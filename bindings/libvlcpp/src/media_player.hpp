/*****************************************************************************
 * media_player.hpp: Media player
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

#ifndef LIBVLCPP_MEDIA_PLAYER_HPP
#define LIBVLCPP_MEDIA_PLAYER_HPP

#include <vlc/libvlc.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_player.h>

#include "libvlc.hpp"
#include "media.hpp"

namespace libvlc
{

class MediaPlayer
{
public:
    /**
     * Create a media player without a media associated
     * @param libvlcInstance: instance of the libVLC class
     */
    MediaPlayer( libVLC &libvlcInstance );

    /**
     * Create a media player with a media associated
     * @param media: the associated media (the media can be safely destroy afterward)
     */
    MediaPlayer( Media &media);

    /**
     * Destructor
     */
    ~MediaPlayer();

    /**
     * Set the media associated with the player
     * @param media: the media to associated with the player
     */
    void setMedia( Media &media );

    /**
     * Get the media associated with the player
     * @return the media
     */
    ///@todo media();

    /**
     * Get the event manager associated to the media player
     * @return the event manager
     */
    ///@todo eventManager()

    /**
     * Is the player playing
     * @return true if the player is playing, false overwise
     */
    int isPlaying();

    /**
     * Play
     */
    void play();

    /**
     * Pause
     */
    void pause();

    /**
     * Stop
     */
    void stop();


    /**
     * Set the NSView handler where the media player shoud render the video
     * @param drawable: the NSView handler
     */
    void setNSObject( void *drawable );

    /**
     * Get the NSView handler associated with the media player
     * @return the NSView handler
     */
    void* nsobject();

    /**
     * Set the agl handler where the media player shoud render the video
     * @param drawable: the agl handler
     */
    void setAgl( uint32_t drawable );

    /**
     * Get the agl handler associated with the media player
     * @return the agl handler
     */
    uint32_t agl();

    /**
     * Set the X Window drawable where the media player shoud render the video
     * @param drawable: the X Window drawable
     */
    void setXWindow( uint32_t drawable );

    /**
     * Get the X Window drawable associated with the media player
     * @return the X Window drawable
     */
    uint32_t xwindow();

    /**
     * Set the Win32/Win64 API window handle where the media player shoud
     * render the video
     * @param drawable: the windows handle
     */
    void setHwnd( void *drawable );

    /**
     * Get the  Win32/Win64 API window handle associated with the media player
     * @return the windows handle
     */
    void *hwnd();


    /**
     * Get the movie lenght (in ms)
     * @return the movie length
     */
    int64_t lenght();

    /**
     * Get the current movie time (in ms)
     * @return the current movie time
     */
    int64_t time();

    /**
      * Set the movie time (in ms)
      * @param the movie time (in ms)
      */
    void setTime( int64_t new_time );

    /**
      * Get the movie position (in percent)
      * @return the movie position
      */
    float position();

    /**
      * Set the movie position (in percent)
      * @param the movie position
      */
    void setPosition( float position );

    /**
     * Get the current movie chapter
     * @return the current chapter
     */
    int chapter();

    /**
     * Get the movie chapter count
     * @return the movie chapter count
     */
    int chapterCount();

    /**
     * Is the player going to play the media (not dead or dying)
     * @return true if the player will play
     */
    int willPlay();

protected:
    libvlc_media_player_t *m_player;
};

};

#endif // LIBVLCPP_MEDIA_PLAYER_HPP

