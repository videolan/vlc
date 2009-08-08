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

#ifndef PHONON_VLC_VLCMEDIA_CONTROLLER_H
#define PHONON_VLC_VLCMEDIA_CONTROLLER_H

#include "mediacontroller.h"
#include "vlcloader.h"

namespace Phonon
{
namespace VLC {

/**
 * MediaController specific code for VLC.
 */
class VLCMediaController : public MediaController
{
public:
    VLCMediaController();
    virtual ~VLCMediaController();

    void audioChannelAdded(int id, const QString & lang);
    void subtitleAdded(int id, const QString & lang, const QString & type);
    void titleAdded(int id, const QString & name);
    void chapterAdded(int titleId, const QString & name);

protected:
    virtual void clearMediaController();

    // AudioChannel
    void setCurrentAudioChannel(const Phonon::AudioChannelDescription & audioChannel);
    QList<Phonon::AudioChannelDescription> availableAudioChannels() const;
    Phonon::AudioChannelDescription currentAudioChannel() const;
    void refreshAudioChannels();

    // Subtitle
    void setCurrentSubtitle(const Phonon::SubtitleDescription & subtitle);
    QList<Phonon::SubtitleDescription> availableSubtitles() const;
    Phonon::SubtitleDescription currentSubtitle() const;
    void refreshSubtitles();

    // Angle
    void setCurrentAngle(int angleNumber);
    int availableAngles() const;
    int currentAngle() const;

    // Chapter
//    void setCurrentChapter( const Phonon::ChapterDescription & chapter );
//    QList<Phonon::ChapterDescription> availableChapters() const;
//    Phonon::ChapterDescription currentChapter() const;
    void setCurrentChapter(int chapterNumber);
    int availableChapters() const;
    int currentChapter() const;
    void refreshChapters(int i_title);

    // Title
//    void setCurrentTitle( const Phonon::TitleDescription & title );
//    QList<Phonon::TitleDescription> availableTitles() const;
//    Phonon::TitleDescription currentTitle() const;
    void setCurrentTitle(int titleNumber);
    int availableTitles() const;
    int currentTitle() const;

    void setAutoplayTitles(bool autoplay);
    bool autoplayTitles() const;

    // MediaPlayer
    libvlc_media_player_t *p_vlc_media_player;

private:
};

}
} // Namespace Phonon::VLC

#endif
