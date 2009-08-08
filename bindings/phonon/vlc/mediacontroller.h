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

#ifndef PHONON_VLC_MEDIACONTROLLER_H
#define PHONON_VLC_MEDIACONTROLLER_H

#include <phonon/addoninterface.h>
#include <phonon/objectdescription.h>

namespace Phonon
{
namespace VLC {

/**
 * Interface for AddonInterface.
 *
 * This class cannot inherit from QObject has MediaObject already inherit from QObject.
 * This is a Qt limitation: there is no possibility to inherit virtual Qobject :/
 * See http://doc.trolltech.com/qq/qq15-academic.html
 * Phonon implementation got the same problem.
 *
 * @see VLCMediaController
 * @see VLCMediaObject
 * @see MediaObject
 */
class MediaController : public AddonInterface
{
public:

    MediaController();
    virtual ~MediaController();

    bool hasInterface(Interface iface) const;

    QVariant interfaceCall(Interface iface, int i_command, const QList<QVariant> & arguments = QList<QVariant>());

    // MediaController signals
    virtual void availableSubtitlesChanged() = 0;
    virtual void availableAudioChannelsChanged() = 0;

//    virtual void availableChaptersChanged() = 0;
//    virtual void availableTitlesChanged() = 0;
    virtual void availableChaptersChanged(int) = 0;
    virtual void availableTitlesChanged(int) = 0;

    virtual void availableAnglesChanged(int i_available_angles) = 0;
    virtual void angleChanged(int i_angle_number) = 0;
    virtual void chapterChanged(int i_chapter_number) = 0;
    virtual void titleChanged(int i_title_number) = 0;

protected:

    // AudioChannel
    virtual void setCurrentAudioChannel(const Phonon::AudioChannelDescription & audioChannel) = 0;
    virtual QList<Phonon::AudioChannelDescription> availableAudioChannels() const = 0;
    virtual Phonon::AudioChannelDescription currentAudioChannel() const = 0;

    // Subtitle
    virtual void setCurrentSubtitle(const Phonon::SubtitleDescription & subtitle) = 0;
    virtual QList<Phonon::SubtitleDescription> availableSubtitles() const = 0;
    virtual Phonon::SubtitleDescription currentSubtitle() const = 0;

    // Angle
    virtual void setCurrentAngle(int i_angle_number) = 0;
    virtual int availableAngles() const = 0;
    virtual int currentAngle() const = 0;

    // Chapter
//    virtual void setCurrentChapter( const Phonon::ChapterDescription & chapter ) = 0;
//    virtual QList<Phonon::ChapterDescription> availableChapters() const = 0;
//    virtual Phonon::ChapterDescription currentChapter() const = 0;
    virtual void setCurrentChapter(int chapterNumber) = 0;
    virtual int availableChapters() const = 0;
    virtual int currentChapter() const = 0;

    // Title
//    virtual void setCurrentTitle( const Phonon::TitleDescription & title ) = 0;
//    virtual QList<Phonon::TitleDescription> availableTitles() const = 0;
//    virtual Phonon::TitleDescription currentTitle() const = 0;
    virtual void setCurrentTitle(int titleNumber) = 0;
    virtual int availableTitles() const = 0;
    virtual int currentTitle() const = 0;

    virtual void setAutoplayTitles(bool b_autoplay) = 0;
    virtual bool autoplayTitles() const = 0;

    /**
     * Clear all (i.e availableSubtitles, availableChapters...).
     *
     * This is used each time we restart the video.
     */
    virtual void clearMediaController();

    Phonon::AudioChannelDescription current_audio_channel;
    QList<Phonon::AudioChannelDescription> available_audio_channels;

    Phonon::SubtitleDescription current_subtitle;
    QList<Phonon::SubtitleDescription> available_subtitles;

//    Phonon::ChapterDescription current_chapter;
//    QList<Phonon::ChapterDescription> available_chapters;
    int current_chapter;
    int available_chapters;

//    Phonon::TitleDescription current_title;
//    QList<Phonon::TitleDescription> available_titles;
    int current_title;
    int available_titles;

    int i_current_angle;
    int i_available_angles;

    bool b_autoplay_titles;

private:
};

}
} // Namespace Phonon::VLC

#endif // PHONON_VLC_MEDIACONTROLLER_H
