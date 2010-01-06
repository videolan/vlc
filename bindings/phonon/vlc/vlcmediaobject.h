/*****************************************************************************
 * VLC backend for the Phonon library                                        *
 * Copyright (C) 2007-2008 Tanguy Krotoff <tkrotoff@gmail.com>               *
 * Copyright (C) 2008 Lukas Durfina <lukas.durfina@gmail.com>                *
 * Copyright (C) 2009 Fathi Boudra <fabo@kde.org>                            *
 *                                                                           *
 * This program is free software; you can redistribute it and/or             *
 * modify it under the terms of the GNU Lesser General Public                *
 * License as published by the Free Software Foundation; either              *
 * version 3 of the License, or (at your option) any later version.          *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU         *
 * Lesser General Public License for more details.                           *
 *                                                                           *
 * You should have received a copy of the GNU Lesser General Public          *
 * License along with this package; if not, write to the Free Software       *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA *
 *****************************************************************************/

#ifndef PHONON_VLC_VLCMEDIAOBJECT_H
#define PHONON_VLC_VLCMEDIAOBJECT_H

#include "vlcmediacontroller.h"

#include "mediaobject.h"

#include <phonon/mediaobjectinterface.h>
#include <phonon/addoninterface.h>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QMultiMap>

namespace Phonon
{
namespace VLC {

/**
 * VLC MediaObject.
 *
 * This is the "brain" of the VLC backend.
 * VLCMediaObject uses libvlc in order to send commands and receive events from the VLC.
 *
 * Encapsulates VLC specific code.
 * Take care of libvlc events via libvlc_callback()
 *
 * @see MediaObject
 */
class VLCMediaObject : public MediaObject, public VLCMediaController
{
    Q_OBJECT
    Q_INTERFACES(Phonon::MediaObjectInterface  Phonon::AddonInterface)

public:

    VLCMediaObject(QObject * parent);
    ~VLCMediaObject();

    void pause();
    void stop();

    bool hasVideo() const;
    bool isSeekable() const;

    qint64 totalTime() const;

    QString errorString() const;

signals:

    // MediaController signals
    void availableSubtitlesChanged();
    void availableAudioChannelsChanged();

//    void availableChaptersChanged();
//    void availableTitlesChanged();
    void availableChaptersChanged(int);
    void availableTitlesChanged(int);

    void availableAnglesChanged(int availableAngles);
    void angleChanged(int angleNumber);
    void chapterChanged(int chapterNumber);
    void titleChanged(int titleNumber);

    /**
     * New widget size computed by VLC.
     *
     * It should be applied to the widget that contains the VLC video.
     */
    void videoWidgetSizeChanged(int i_width, int i_height);

protected:

    void loadMediaInternal(const QString & filename);
    void playInternal();
    void seekInternal(qint64 milliseconds);

    qint64 currentTimeInternal() const;

private:

    /**
     * Connect libvlc_callback() to all vlc events.
     *
     * @see libvlc_callback()
     */
    void connectToAllVLCEvents();

    /**
     * Retrieve meta data of a file (i.e ARTIST, TITLE, ALBUM, etc...).
     */
    void updateMetaData();

    /**
     * Libvlc callback.
     *
     * Receive all vlc events.
     *
     * Warning: owned by libvlc thread.
     *
     * @see connectToAllVLCEvents()
     * @see libvlc_event_attach()
     */
    static void libvlc_callback(const libvlc_event_t *p_event, void *p_user_data);

    void unloadMedia();

    void setVLCWidgetId();

    // MediaPlayer
//    libvlc_media_player_t * p_vlc_media_player;
    libvlc_event_manager_t * p_vlc_media_player_event_manager;

    // Media
    libvlc_media_t * p_vlc_media;
    libvlc_event_manager_t * p_vlc_media_event_manager;

    // MediaDiscoverer
    libvlc_media_discoverer_t * p_vlc_media_discoverer;
    libvlc_event_manager_t * p_vlc_media_discoverer_event_manager;

    bool b_play_request_reached;

    qint64 i_total_time;

    bool b_has_video;

    bool b_seekable;
};

}
} // Namespace Phonon::VLC

#endif
