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

class VideoDescription
{
    Q_GADGET

    Q_PROPERTY(QString codec READ getCodec CONSTANT)
    Q_PROPERTY(QString language READ getLanguage CONSTANT)
    Q_PROPERTY(unsigned int fps READ getFps CONSTANT)

public:
    VideoDescription() = default;
    VideoDescription(const QString& codec, const QString& language, unsigned int fps);

    QString getCodec() const;
    QString getLanguage() const;
    unsigned int getFps() const;

private:

    QString m_codec;
    QString m_language;
    unsigned int m_fps;
};

Q_DECLARE_METATYPE(VideoDescription)

class AudioDescription
{
    Q_GADGET

    Q_PROPERTY(QString codec READ getCodec CONSTANT)
    Q_PROPERTY(QString language READ getLanguage CONSTANT)
    Q_PROPERTY(unsigned int nbchannels READ getNbChannels CONSTANT)
    Q_PROPERTY(unsigned int sampleRate READ getSampleRate CONSTANT)

public:
    AudioDescription() = default;
    AudioDescription(const QString& codec, const QString& language, unsigned int nbChannels, unsigned int sampleRate);

    QString getCodec() const;
    QString getLanguage() const;
    unsigned int getNbChannels() const;
    unsigned int getSampleRate() const;

private:

    QString m_codec;
    QString m_language;
    unsigned int m_nbchannels;
    unsigned int m_sampleRate;
};

Q_DECLARE_METATYPE(AudioDescription)

class MLVideo : public QObject, public MLItem
{
    Q_OBJECT

public:
    MLVideo(vlc_medialibrary_t *ml, const vlc_ml_media_t *data, QObject *parent = nullptr);

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
    QList<AudioDescription> getAudioDesc() const;
    QList<VideoDescription> getVideoDesc() const;

private:
    static void onMlEvent( void* data, const vlc_ml_event_t* event );
    void onMlEvent( const vlc_ml_event_t* event );

    vlc_medialibrary_t* m_ml;
    QString m_title;
    QString m_thumbnail;
    int64_t m_duration;
    QUrl m_mrl;
    QString m_resolution;
    QString m_channel;
    float m_progress;
    QString m_progressTime;
    unsigned int m_playCount;
    vlc_ml_thumbnail_status_t m_thumbnailStatus;
    QList<AudioDescription> m_audioDesc;
    QList<VideoDescription> m_videoDesc;

    std::unique_ptr<vlc_ml_event_callback_t,
                    std::function<void(vlc_ml_event_callback_t*)>> m_ml_event_handle;
};

#endif // MLVIDEO_H
