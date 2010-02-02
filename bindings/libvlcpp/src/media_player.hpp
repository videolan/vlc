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
#include "audio.hpp"

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
    MediaPlayer( Media &media );

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
      * @param new_time the movie time (in ms)
      */
    void setTime( int64_t new_time );

    /**
      * Get the movie position (in percent)
      * @return the movie position
      */
    float position();

    /**
      * Set the movie position (in percent)
      * @param position the movie position
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
     * Get the number of chapter in the given title
     * @param title: the title
     * @return the number of chapter in title
     */
    int chapterCount( int title );

    /**
     * Set the movie chapter
     * @param chapter: the chapter to play
     */
    void setChapter( int chapter );

    /**
     * Is the player going to play the media (not dead or dying)
     * @return true if the player will play
     */
    int willPlay();

    /**
     * Get the current title
     * @return the title
     */
    int title();

    /**
     * Get the title count
     * @return the number of title
     */
    int titleCount();

    /**
     * Set the title
     * @param title: the title
     */
    void setTitle( int title );


    /**
     * Move to the previous chapter
     */
    void previousChapter();

    /**
     * Move to the next chapter
     */
    void nextChapter();

    /**
     * Get the movie play rate
     * @return the play rate
     */
    float rate();

    /**
     * Set the movie rate
     * @param rate: the rate
     */
    void setRate( float rate );

    /**
     * Get the movie state
     * @return the state
     */
    libvlc_state_t state();

    /**
     * Get the movie fps
     * @return the movie fps
     */
    float fps();


    /**
     * Does the media player have a video output
     * @return true if the media player has a video output
     */
    int hasVout();

    /**
     * Is the media player able to seek ?
     * @return true if the media player can seek
     */
    int isSeekable();

    /**
     * Can this media player be paused ?
     * @return true if the media player can pause
     */
    int canPause();

    /**
     * Display the next frame
     */
    void nextFrame();


    /**
     * Toggle fullscreen status on a non-embedded video output
     */
    void toggleFullscreen();

    /**
     * Enable fullscreen on a non-embedded video output
     */
    void enableFullscreen();

    /**
     * Disable fullscreen on a non-embedded video output
     */
    void disableFullscreen();

    /**
     * Get the fullscreen status
     * @return true if the fullscreen is enable, false overwise
     */
    int fullscreen();

    /**
     * Get the class that handle the Audio
     * @return the instance of Audio associated with this MediaPlayer
     */
    Audio &audio();

private:
    /** The media player instance of libvlc */
    libvlc_media_player_t *m_player;

    /** The Audio part of the media player */
    Audio m_audio;
};

};

#endif // LIBVLCPP_MEDIA_PLAYER_HPP

