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

#include "mediaobject.h"

#include "seekstack.h"

#include <QtCore/QUrl>
#include <QtCore/QMetaType>
#include <QtCore/QTimer>

//Time in milliseconds before sending aboutToFinish() signal
//2 seconds
static const int ABOUT_TO_FINISH_TIME = 2000;

namespace Phonon
{
namespace VLC {

MediaObject::MediaObject(QObject *p_parent)
        : QObject(p_parent)
{
    currentState = Phonon::LoadingState;
    i_video_widget_id = 0;
    b_prefinish_mark_reached_emitted = false;
    b_about_to_finish_emitted = false;
    i_transition_time = 0;

    // By default, no tick() signal
    // FIXME: Not implemented yet
    i_tick_interval = 0;

    qRegisterMetaType<QMultiMap<QString, QString> >("QMultiMap<QString, QString>");

    connect(this, SIGNAL(stateChanged(Phonon::State)),
            SLOT(stateChangedInternal(Phonon::State)));

    connect(this, SIGNAL(tickInternal(qint64)),
            SLOT(tickInternalSlot(qint64)));
}

MediaObject::~MediaObject()
{
}

void MediaObject::setVideoWidgetId(int i_widget_id)
{
    i_video_widget_id = i_widget_id;
}

void MediaObject::play()
{
    qDebug() << __FUNCTION__;

    if (currentState == Phonon::PausedState) {
        resume();
    } else {
        // Play the file
        playInternal();
    }
}

void MediaObject::seek(qint64 milliseconds)
{
    static SeekStack *p_stack = new SeekStack(this);

    p_stack->pushSeek(milliseconds);

    qint64 currentTime = this->currentTime();
    qint64 totalTime = this->totalTime();

    if (currentTime < totalTime - i_prefinish_mark) {
        b_prefinish_mark_reached_emitted = false;
    }
    if (currentTime < totalTime - ABOUT_TO_FINISH_TIME) {
        b_about_to_finish_emitted = false;
    }
}

void MediaObject::tickInternalSlot(qint64 currentTime)
{
    qint64 totalTime = this->totalTime();

    if (i_tick_interval > 0) {
        // If _tickInternal == 0 means tick() signal is disabled
        // Default is _tickInternal = 0
        emit tick(currentTime);
    }

    if (currentState == Phonon::PlayingState) {
        if (currentTime >= totalTime - i_prefinish_mark) {
            if (!b_prefinish_mark_reached_emitted) {
                b_prefinish_mark_reached_emitted = true;
                emit prefinishMarkReached(totalTime - currentTime);
            }
        }
        if (currentTime >= totalTime - ABOUT_TO_FINISH_TIME) {
            if (!b_about_to_finish_emitted) {
                // Track is about to finish
                b_about_to_finish_emitted = true;
                emit aboutToFinish();
            }
        }
    }
}

void MediaObject::loadMedia(const QString & filename)
{
    // Default MediaObject state is Phonon::LoadingState
    currentState = Phonon::LoadingState;

    // Load the media
    loadMediaInternal(filename);
}

void MediaObject::resume()
{
    pause();
}

qint32 MediaObject::tickInterval() const
{
    return i_tick_interval;
}

void MediaObject::setTickInterval(qint32 tickInterval)
{
    i_tick_interval = tickInterval;
//    if (_tickInterval <= 0) {
//        _tickTimer->setInterval(50);
//    } else {
//        _tickTimer->setInterval(_tickInterval);
//    }
}

qint64 MediaObject::currentTime() const
{
    qint64 time = -1;
    Phonon::State st = state();

    switch (st) {
    case Phonon::PausedState:
        time = currentTimeInternal();
        break;
    case Phonon::BufferingState:
        time = currentTimeInternal();
        break;
    case Phonon::PlayingState:
        time = currentTimeInternal();
        break;
    case Phonon::StoppedState:
        time = 0;
        break;
    case Phonon::LoadingState:
        time = 0;
        break;
    case Phonon::ErrorState:
        time = -1;
        break;
    default:
        qCritical() << __FUNCTION__ << "Error: unsupported Phonon::State:" << st;
    }

    return time;
}

Phonon::State MediaObject::state() const
{
    return currentState;
}

Phonon::ErrorType MediaObject::errorType() const
{
    return Phonon::NormalError;
}

MediaSource MediaObject::source() const
{
    return mediaSource;
}

void MediaObject::setSource(const MediaSource & source)
{
    qDebug() << __FUNCTION__;

    mediaSource = source;

    switch (source.type()) {
    case MediaSource::Invalid:
        break;
    case MediaSource::LocalFile:
        loadMedia(mediaSource.fileName());
        break;
    case MediaSource::Url:
        loadMedia(mediaSource.url().toString());
        break;
    case MediaSource::Disc:
        switch (source.discType()) {
        case Phonon::NoDisc:
            qCritical() << __FUNCTION__
            << "Error: the MediaSource::Disc doesn't specify which one (Phonon::NoDisc)";
            return;
        case Phonon::Cd:
            loadMedia(mediaSource.deviceName());
            break;
        case Phonon::Dvd:
            loadMedia("dvd://" + mediaSource.deviceName());
            break;
        case Phonon::Vcd:
            loadMedia(mediaSource.deviceName());
            break;
        default:
            qCritical() << __FUNCTION__ << "Error: unsupported MediaSource::Disc:" << source.discType();
            break;
        }
        break;
    case MediaSource::Stream:
        break;
    default:
        qCritical() << __FUNCTION__
        << "Error: unsupported MediaSource:"
        << source.type();
        break;
    }
}

void MediaObject::setNextSource(const MediaSource & source)
{
    setSource(source);
}

qint32 MediaObject::prefinishMark() const
{
    return i_prefinish_mark;
}

void MediaObject::setPrefinishMark(qint32 msecToEnd)
{
    i_prefinish_mark = msecToEnd;
    if (currentTime() < totalTime() - i_prefinish_mark) {
        // Not about to finish
        b_prefinish_mark_reached_emitted = false;
    }
}

qint32 MediaObject::transitionTime() const
{
    return i_transition_time;
}

void MediaObject::setTransitionTime(qint32 time)
{
    i_transition_time = time;
}

void MediaObject::stateChangedInternal(Phonon::State newState)
{
    qDebug() << __FUNCTION__ << "newState:" << newState
    << "previousState:" << currentState ;

    if (newState == currentState) {
        // State not changed
        return;
    }

    // State changed
    Phonon::State previousState = currentState;
    currentState = newState;
    emit stateChanged(currentState, previousState);
}

}
} // Namespace Phonon::VLC
