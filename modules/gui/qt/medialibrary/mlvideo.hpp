/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef MLVIDEO_H
#define MLVIDEO_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "qt.hpp"

#include <QObject>
#include <vlc_media_library.h>
#include "mlhelper.hpp"
#include "mlqmltypes.hpp"

#include <functional>

class MLVideo : public QObject
{
    Q_OBJECT

    Q_PROPERTY(MLParentId id READ getId CONSTANT);
    Q_PROPERTY(QString title READ getTitle CONSTANT);
    Q_PROPERTY(QString thumbnail READ getThumbnail NOTIFY onThumbnailChanged);
    Q_PROPERTY(QString duration READ getDuration CONSTANT);
    Q_PROPERTY(QString durationShort READ getDurationShort CONSTANT);
    Q_PROPERTY(QString mrl READ getMRL CONSTANT);
    Q_PROPERTY(QString displayMrl READ getDisplayMRL CONSTANT)
    Q_PROPERTY(float progress READ getProgress CONSTANT);
    Q_PROPERTY(unsigned int playCount READ getPlayCount CONSTANT);
    Q_PROPERTY(QString resolution_name READ getResolutionName CONSTANT);
    Q_PROPERTY(QString channel READ getChannel CONSTANT);
    Q_PROPERTY(QString progressTime READ getProgressTime CONSTANT);
    Q_PROPERTY(QString audioDesc READ getAudioDesc CONSTANT);
    Q_PROPERTY(QString videoDesc READ getVideoDesc CONSTANT);

public:
    MLVideo(vlc_medialibrary_t *ml, const vlc_ml_media_t *data, QObject *parent = nullptr);

    MLParentId getId() const;
    QString getTitle() const;
    QString getThumbnail();
    QString getDuration() const;
    QString getDurationShort() const;
    QString getResolutionName() const;
    QString getChannel() const;
    QString getMRL() const;
    QString getDisplayMRL() const;
    float getProgress() const;
    unsigned int getPlayCount() const;
    QString getProgressTime() const;
    QString getAudioDesc() const;
    QString getVideoDesc() const;

    MLVideo* clone(QObject* parent = nullptr) const;

signals:
    void onThumbnailChanged( QString );

private:
    MLVideo(const MLVideo& video, QObject* parent = nullptr);

    static void onMlEvent( void* data, const vlc_ml_event_t* event );
    void onMlEvent( const vlc_ml_event_t* event );


    vlc_medialibrary_t* m_ml;
    MLParentId m_id;
    QString m_title;
    QString m_thumbnail;
    int64_t m_duration;
    QUrl m_mrl;
    QString m_resolution;
    QString m_channel;
    float m_progress;
    QString m_progressTime;
    unsigned int m_playCount;
    bool m_thumbnailGenerated;
    QString audioDesc,videoDesc;

    std::unique_ptr<vlc_ml_event_callback_t,
                    std::function<void(vlc_ml_event_callback_t*)>> m_ml_event_handle;
};

#endif // MLVIDEO_H
