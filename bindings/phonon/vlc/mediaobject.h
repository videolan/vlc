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

#ifndef PHONON_VLC_MEDIAOBJECT_H
#define PHONON_VLC_MEDIAOBJECT_H

#include <phonon/mediaobjectinterface.h>

#include <QtCore/QObject>
#include <QtGui/QWidget>

namespace Phonon
{
namespace VLC {

class SeekStack;

class MediaObject : public QObject, public MediaObjectInterface
{
    Q_OBJECT
    friend class SeekStack;

public:

    MediaObject(QObject *p_parent);
    virtual ~MediaObject();

    /**
     * Widget Id where VLC will show the videos.
     */
    void setVideoWidgetId(WId i_widget_id);

    void play();
    void seek(qint64 milliseconds);

    qint32 tickInterval() const;
    void setTickInterval(qint32 tickInterval);

    qint64 currentTime() const;
    Phonon::State state() const;
    Phonon::ErrorType errorType() const;
    MediaSource source() const;
    void setSource(const MediaSource & source);
    void setNextSource(const MediaSource & source);

    qint32 prefinishMark() const;
    void setPrefinishMark(qint32 msecToEnd);

    qint32 transitionTime() const;
    void setTransitionTime(qint32);

signals:

    void aboutToFinish();
    void bufferStatus( int i_percent_filled );
    void currentSourceChanged( const MediaSource & newSource );
    void finished();
    void hasVideoChanged(bool b_has_video);
    void metaDataChanged(const QMultiMap<QString, QString> & metaData);
    void prefinishMarkReached(qint32 msecToEnd);
    void seekableChanged(bool b_is_seekable);
    void stateChanged(Phonon::State newState, Phonon::State oldState);
    void tick(qint64 time);
    void totalTimeChanged(qint64 newTotalTime);

    // Signal from VLCMediaObject
    void stateChanged(Phonon::State newState);

    void tickInternal(qint64 time);

protected:

    virtual void loadMediaInternal(const QString & filename) = 0;
    virtual void playInternal() = 0;
    virtual void seekInternal(qint64 milliseconds) = 0;

    virtual qint64 currentTimeInternal() const = 0;

    WId i_video_widget_id;

private slots:

    void stateChangedInternal(Phonon::State newState);

    void tickInternalSlot(qint64 time);

private:

    void loadMedia(const QString & filename);

    void resume();

    MediaSource mediaSource;

    Phonon::State currentState;

    qint32 i_prefinish_mark;
    bool b_prefinish_mark_reached_emitted;

    bool b_about_to_finish_emitted;

    qint32 i_tick_interval;
    qint32 i_transition_time;
};

}
} // Namespace Phonon::VLC

#endif // PHONON_VLC_MEDIAOBJECT_H
