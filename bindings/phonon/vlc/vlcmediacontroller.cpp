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

#include "vlcmediacontroller.h"

#include "vlcloader.h"

namespace Phonon
{
namespace VLC {

VLCMediaController::VLCMediaController()
        : MediaController()
{
    p_vlc_media_player = 0;
}

VLCMediaController::~VLCMediaController()
{
}

void VLCMediaController::clearMediaController()
{
    current_audio_channel = Phonon::AudioChannelDescription();
    available_audio_channels.clear();

    current_subtitle = Phonon::SubtitleDescription();
    available_subtitles.clear();

    i_current_angle = 0;
    i_available_angles = 0;

//    current_chapter = Phonon::ChapterDescription();
//    available_chapters.clear();
    current_chapter = 0;
    available_chapters = 0;

//    current_title = Phonon::TitleDescription();
//    available_titles.clear();
    current_title = 0;
    available_titles = 0;

    b_autoplay_titles = false;

    emit availableAudioChannelsChanged();
    emit availableSubtitlesChanged();
    emit availableTitlesChanged(0);
    emit availableChaptersChanged(0);
}

// Add audio channel -> in libvlc it is track, it means audio in another language
void VLCMediaController::audioChannelAdded(int id, const QString & lang)
{
    QHash<QByteArray, QVariant> properties;
    properties.insert("name", lang);
    properties.insert("description", "");

    available_audio_channels << Phonon::AudioChannelDescription(id, properties);
    emit availableAudioChannelsChanged();
}

// Add subtitle
void VLCMediaController::subtitleAdded(int id, const QString & lang, const QString & type)
{
    QHash<QByteArray, QVariant> properties;
    properties.insert("name", lang);
    properties.insert("description", "");
    properties.insert("type", type);

    available_subtitles << Phonon::SubtitleDescription(id, properties);
    emit availableSubtitlesChanged();
}

// Add title
void VLCMediaController::titleAdded(int id, const QString & name)
{
//    QHash<QByteArray, QVariant> properties;
//    properties.insert("name", name);
//    properties.insert("description", "");

//    available_titles << Phonon::TitleDescription(id, properties);
    available_titles++;
    emit availableTitlesChanged(available_titles);
}

// Add chapter
void VLCMediaController::chapterAdded(int titleId, const QString & name)
{
//    QHash<QByteArray, QVariant> properties;
//    properties.insert("name", name);
//    properties.insert("description", "");

//    available_chapters << Phonon::ChapterDescription(titleId, properties);
    available_chapters++;
    emit availableChaptersChanged(available_chapters);
}

// Audio channel

void VLCMediaController::setCurrentAudioChannel(const Phonon::AudioChannelDescription & audioChannel)
{
    current_audio_channel = audioChannel;
    libvlc_audio_set_track(p_vlc_media_player, audioChannel.index(), vlc_exception);
    vlcExceptionRaised();
}

QList<Phonon::AudioChannelDescription> VLCMediaController::availableAudioChannels() const
{
    return available_audio_channels;
}

Phonon::AudioChannelDescription VLCMediaController::currentAudioChannel() const
{
    return current_audio_channel;
}

void VLCMediaController::refreshAudioChannels()
{
    current_audio_channel = Phonon::AudioChannelDescription();
    available_audio_channels.clear();

    libvlc_track_description_t * p_info = libvlc_audio_get_track_description(
                                              p_vlc_media_player, vlc_exception);
    vlcExceptionRaised();
    while (p_info) {
        audioChannelAdded(p_info->i_id, p_info->psz_name);
        p_info = p_info->p_next;
    }
    libvlc_track_description_release(p_info);
}

// Subtitle

void VLCMediaController::setCurrentSubtitle(const Phonon::SubtitleDescription & subtitle)
{
    current_subtitle = subtitle;
//    int id = current_subtitle.index();
    QString type = current_subtitle.property("type").toString();

    if (type == "file") {
        QString filename = current_subtitle.property("name").toString();
        if (!filename.isEmpty()) {
            libvlc_video_set_subtitle_file(p_vlc_media_player,
                                           filename.toAscii().data(),
                                           vlc_exception);
            vlcExceptionRaised();

            // There is no subtitle event inside libvlc so let's send our own event...
            available_subtitles << current_subtitle;
            emit availableSubtitlesChanged();
        }
    } else {
        libvlc_video_set_spu(p_vlc_media_player, subtitle.index(), vlc_exception);
        vlcExceptionRaised();
    }
}

QList<Phonon::SubtitleDescription> VLCMediaController::availableSubtitles() const
{
    return available_subtitles;
}

Phonon::SubtitleDescription VLCMediaController::currentSubtitle() const
{
    return current_subtitle;
}

void VLCMediaController::refreshSubtitles()
{
    current_subtitle = Phonon::SubtitleDescription();
    available_subtitles.clear();

    libvlc_track_description_t *p_info = libvlc_video_get_spu_description(
                                             p_vlc_media_player, vlc_exception);
    vlcExceptionRaised();
    while (p_info) {
        subtitleAdded(p_info->i_id, p_info->psz_name, "");
        p_info = p_info->p_next;
    }
    libvlc_track_description_release(p_info);
}

// Title

//void VLCMediaController::setCurrentTitle( const Phonon::TitleDescription & title )
void VLCMediaController::setCurrentTitle(int title)
{
    current_title = title;

//    libvlc_media_player_set_title(p_vlc_media_player, title.index(), vlc_exception);
    libvlc_media_player_set_title(p_vlc_media_player, title, vlc_exception);
    vlcExceptionRaised();
}

//QList<Phonon::TitleDescription> VLCMediaController::availableTitles() const
int VLCMediaController::availableTitles() const
{
    return available_titles;
}

//Phonon::TitleDescription VLCMediaController::currentTitle() const
int VLCMediaController::currentTitle() const
{
    return current_title;
}

void VLCMediaController::setAutoplayTitles(bool autoplay)
{
    b_autoplay_titles = autoplay;
}

bool VLCMediaController::autoplayTitles() const
{
    return b_autoplay_titles;
}

// Chapter

//void VLCMediaController::setCurrentChapter(const Phonon::ChapterDescription &chapter)
void VLCMediaController::setCurrentChapter(int chapter)
{
    current_chapter = chapter;
//    libvlc_media_player_set_chapter(p_vlc_media_player, chapter.index(), vlc_exception);
    libvlc_media_player_set_chapter(p_vlc_media_player, chapter, vlc_exception);
    vlcExceptionRaised();
}

//QList<Phonon::ChapterDescription> VLCMediaController::availableChapters() const
int VLCMediaController::availableChapters() const
{
    return available_chapters;
}

//Phonon::ChapterDescription VLCMediaController::currentChapter() const
int VLCMediaController::currentChapter() const
{
    return current_chapter;
}

// We need to rebuild available chapters when title is changed
void VLCMediaController::refreshChapters(int i_title)
{
//    current_chapter = Phonon::ChapterDescription();
//    available_chapters.clear();
    current_chapter = 0;
    available_chapters = 0;

    // Get the description of available chapters for specific title
    libvlc_track_description_t *p_info = libvlc_video_get_chapter_description(
                                             p_vlc_media_player, i_title, vlc_exception);
    vlcExceptionRaised();
    while (p_info) {
        chapterAdded(p_info->i_id, p_info->psz_name);
        p_info = p_info->p_next;
    }
    libvlc_track_description_release(p_info);
}

// Angle

void VLCMediaController::setCurrentAngle(int angleNumber)
{
    i_current_angle = angleNumber;
}

int VLCMediaController::availableAngles() const
{
    return i_available_angles;
}

int VLCMediaController::currentAngle() const
{
    return i_current_angle;
}

}
} // Namespace Phonon::VLC
