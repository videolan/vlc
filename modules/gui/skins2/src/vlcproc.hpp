/*****************************************************************************
 * vlcproc.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: vlcproc.hpp,v 1.2 2004/01/11 00:21:22 asmax Exp $
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef VLCPROC_HPP
#define VLCPROC_HPP

#include "../vars/playlist.hpp"
#include "../vars/time.hpp"
#include "../vars/volume.hpp"
#include "../vars/vlcvars.hpp"

class OSTimer;


/// Singleton object handling VLC internal state and playlist
class VlcProc: public SkinObject
{
    public:
        /// Get the instance of VlcProc
        /// Returns NULL if the initialization of the object failed
        static VlcProc *instance( intf_thread_t *pIntf );

        /// Delete the instance of VlcProc
        static void destroy( intf_thread_t *pIntf );

        /// Getter for the playlist variable
        Playlist &getPlaylistVar() { return m_playlist; }

        /// Getter for the time variable
        Time &getTimeVar() { return m_varTime; }

        /// Getter for the volume variable
        Volume &getVolumeVar() { return m_varVolume; }

        /// Getter for the mute variable
        VlcIsMute &getIsMuteVar() { return m_varMute; }

        /// Getter for the playing variable
        VlcIsPlaying &getIsPlayingVar() { return m_varPlaying; }

        /// Getter for the seekable/playing variable
        VlcIsSeekablePlaying &getIsSeekablePlayingVar()
            { return m_varSeekablePlaying; }

    protected:
        // Protected because it is a singleton
        VlcProc( intf_thread_t *pIntf );
        virtual ~VlcProc();

    private:
        /// Timer to call manage() regularly (via doManage())
        OSTimer *m_pTimer;
        /// Playlist variable
        Playlist m_playlist;
        /// Variable for the position in the stream
        Time m_varTime;
        /// Variable for audio volume
        Volume m_varVolume;
        /// Variable for the "mute" state
        VlcIsMute m_varMute;
        /// Variables related to the input
        VlcIsPlaying m_varPlaying;
        VlcIsSeekablePlaying m_varSeekablePlaying;

        /// Poll VLC internals to update the status (volume, current time in
        /// the stream, current filename, play/pause/stop status, ...)
        /// This function should be called regurlarly, since there is no
        /// callback mechanism (yet?) to automatically update a variable when
        /// the internal status changes
        void manage();

        /// This function directly calls manage(), because it's boring to
        /// always write "pThis->"
        static void doManage( SkinObject *pObj );

        /// Callback for intf-change variable
        static int onIntfChange( vlc_object_t *pObj, const char *pVariable,
                                 vlc_value_t oldVal, vlc_value_t newVal,
                                 void *pParam );

        /// Callback for item-change variable
        static int onItemChange( vlc_object_t *pObj, const char *pVariable,
                                 vlc_value_t oldVal, vlc_value_t newVal,
                                 void *pParam );

        /// Callback for playlist-current variable
        static int onPlaylistChange( vlc_object_t *pObj, const char *pVariable,
                                     vlc_value_t oldVal, vlc_value_t newVal,
                                     void *pParam );
};


#endif
