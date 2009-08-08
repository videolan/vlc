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

#include "mediacontroller.h"

namespace Phonon
{
namespace VLC {

MediaController::MediaController()
{
    clearMediaController();
}

MediaController::~MediaController()
{
}

void MediaController::clearMediaController()
{
    current_audio_channel = Phonon::AudioChannelDescription();
    available_audio_channels.clear();

    current_subtitle = Phonon::SubtitleDescription();
    available_subtitles.clear();

//    current_chapter = Phonon::ChapterDescription();
//    available_chapters.clear();
    current_chapter = 0;
    available_chapters = 0;

//    current_title = Phonon::TitleDescription();
//    available_titles.clear();
    current_title = 0;
    available_titles = 0;

    i_current_angle = 0;
    i_available_angles = 0;

    b_autoplay_titles = false;
}

bool MediaController::hasInterface(Interface iface) const
{
    switch (iface) {
    case AddonInterface::NavigationInterface:
        return true;
        break;
    case AddonInterface::ChapterInterface:
        return true;
        break;
    case AddonInterface::AngleInterface:
        return true;
        break;
    case AddonInterface::TitleInterface:
        return true;
        break;
    case AddonInterface::SubtitleInterface:
        return true;
        break;
    case AddonInterface::AudioChannelInterface:
        return true;
        break;
    default:
        qCritical() << __FUNCTION__
        << "Error: unsupported AddonInterface::Interface"
        << iface;
    }

    return false;
}

QVariant MediaController::interfaceCall(Interface iface, int i_command, const QList<QVariant> & arguments)
{
    switch (iface) {
    case AddonInterface::ChapterInterface:
        switch (static_cast<AddonInterface::ChapterCommand>(i_command)) {
//        case AddonInterface::availableChapters:
//            return QVariant::fromValue(availableChapters());
        case AddonInterface::availableChapters:
            return availableChapters();
//        case AddonInterface::currentChapter:
//            return QVariant::fromValue(currentChapter());
        case AddonInterface::chapter:
            return currentChapter();
//        case AddonInterface::setCurrentChapter:
//            if( arguments.isEmpty() || !arguments.first().canConvert<ChapterDescription>()) {
//                    qCritical() << __FUNCTION__ << "Error: arguments invalid";
//                    return false;
//                }
//            setCurrentChapter(arguments.first().value<ChapterDescription>());
//            return true;
        case AddonInterface::setChapter:
            if (arguments.isEmpty() || !arguments.first().canConvert(QVariant::Int)) {
                qCritical() << __FUNCTION__ << "Error: arguments invalid";
                return false;
            }
            setCurrentChapter(arguments.first().toInt());
            return true;
        default:
            qCritical() << __FUNCTION__
            << "Error: unsupported AddonInterface::ChapterInterface command:"
            << i_command;
        }
        break;
    case AddonInterface::TitleInterface:
        switch (static_cast<AddonInterface::TitleCommand>(i_command)) {
//        case AddonInterface::availableTitles:
//            return QVariant::fromValue(availableTitles());
        case AddonInterface::availableTitles:
            return availableTitles();
//        case AddonInterface::currentTitle:
//            return QVariant::fromValue(currentTitle());
        case AddonInterface::title:
            return currentTitle();
//        case AddonInterface::setCurrentTitle:
//            if( arguments.isEmpty() || !arguments.first().canConvert<TitleDescription>()) {
//                    qCritical() << __FUNCTION__ << "Error: arguments invalid";
//                    return false;
//            }
//            setCurrentTitle(arguments.first().value<TitleDescription>());
//            return true;
        case AddonInterface::setTitle:
            if (arguments.isEmpty() || !arguments.first().canConvert(QVariant::Int)) {
                qCritical() << __FUNCTION__ << "Error: arguments invalid";
                return false;
            }
            setCurrentTitle(arguments.first().toInt());
            return true;
        case AddonInterface::autoplayTitles:
            return autoplayTitles();
        case AddonInterface::setAutoplayTitles:
            if (arguments.isEmpty() || !arguments.first().canConvert(QVariant::Bool)) {
                qCritical() << __FUNCTION__ << "Error: arguments invalid";
                return false;
            }
            setAutoplayTitles(arguments.first().toBool());
            return true;
        default:
            qCritical() << __FUNCTION__
            << "Error: unsupported AddonInterface::TitleInterface command:"
            << i_command;
        }
        break;
    case AddonInterface::AngleInterface:
        switch (static_cast<AddonInterface::AngleCommand>(i_command)) {
        case AddonInterface::availableAngles:
        case AddonInterface::angle:
        case AddonInterface::setAngle:
            break;
        default:
            qCritical() << __FUNCTION__
            << "Error: unsupported AddonInterface::AngleInterface command:"
            << i_command;
        }
        break;
    case AddonInterface::SubtitleInterface:
        switch (static_cast<AddonInterface::SubtitleCommand>(i_command)) {
        case AddonInterface::availableSubtitles:
            return QVariant::fromValue(availableSubtitles());
        case AddonInterface::currentSubtitle:
            return QVariant::fromValue(currentSubtitle());
        case AddonInterface::setCurrentSubtitle:
            if (arguments.isEmpty() || !arguments.first().canConvert<SubtitleDescription>()) {
                qCritical() << __FUNCTION__ << "Error: arguments invalid";
                return false;
            }
            setCurrentSubtitle(arguments.first().value<SubtitleDescription>());
            return true;
        default:
            qCritical() << __FUNCTION__
            << "Error: unsupported AddonInterface::SubtitleInterface command:"
            << i_command;
        }
        break;
    case AddonInterface::AudioChannelInterface:
        switch (static_cast<AddonInterface::AudioChannelCommand>(i_command)) {
        case AddonInterface::availableAudioChannels:
            return QVariant::fromValue(availableAudioChannels());
        case AddonInterface::currentAudioChannel:
            return QVariant::fromValue(currentAudioChannel());
        case AddonInterface::setCurrentAudioChannel:
            if (arguments.isEmpty() || !arguments.first().canConvert<AudioChannelDescription>()) {
                qCritical() << __FUNCTION__ << "Error: arguments invalid";
                return false;
            }
            setCurrentAudioChannel(arguments.first().value<AudioChannelDescription>());
            return true;
        default:
            qCritical() << __FUNCTION__
            << "Error: unsupported AddonInterface::AudioChannelInterface command:"
            << i_command;
        }
        break;
    default:
        qCritical() << __FUNCTION__
        << "Error: unsupported AddonInterface::Interface:"
        << iface;
    }

    return QVariant();
}

}
} // Namespace Phonon::VLC
