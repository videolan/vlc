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

#include <QUrl>

#include <vlc_media_library.h>
#include "mlqmltypes.hpp"
#include "mlmedia.hpp"

#include <functional>

class VideoDescription
{
    Q_GADGET

    Q_PROPERTY(QString codec READ getCodec CONSTANT FINAL)
    Q_PROPERTY(QString language READ getLanguage CONSTANT FINAL)
    Q_PROPERTY(unsigned int fps READ getFps CONSTANT FINAL)

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

class AudioDescription
{
    Q_GADGET

    Q_PROPERTY(QString codec READ getCodec CONSTANT FINAL)
    Q_PROPERTY(QString language READ getLanguage CONSTANT FINAL)
    Q_PROPERTY(unsigned int nbchannels READ getNbChannels CONSTANT FINAL)
    Q_PROPERTY(unsigned int sampleRate READ getSampleRate CONSTANT FINAL)

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

class SubtitleDescription
{
    Q_GADGET

    Q_PROPERTY(QString codec READ getCodec CONSTANT FINAL)
    Q_PROPERTY(QString language READ getLanguage CONSTANT FINAL)
    Q_PROPERTY(QString description READ getDescription CONSTANT FINAL)
    Q_PROPERTY(QString encoding READ getEncoding CONSTANT FINAL)

public:
    SubtitleDescription() = default;
    SubtitleDescription(const QString& codec, const QString& language
                     , const QString &description, const QString &encoding);

    QString getCodec() const;
    QString getLanguage() const;
    QString getDescription() const;
    QString getEncoding() const;

private:
    QString m_codec;
    QString m_language;
    QString m_description;
    QString m_encoding;
};

class MLVideo : public MLMedia
{
public:
    MLVideo(const vlc_ml_media_t *data);

    bool isNew() const;
    void setIsNew(bool isNew);
    void setSmallCover(vlc_ml_thumbnail_status_t status, QString mrl);
    QString getResolutionName() const;
    QString getChannel() const;
    QString getDisplayMRL() const;
    QList<AudioDescription> getAudioDesc() const;
    QList<VideoDescription> getVideoDesc() const;
    QList<SubtitleDescription> getSubtitleDesc() const;

private:
    bool m_isNew;
    QString m_resolution;
    QString m_channel;
    QList<AudioDescription> m_audioDesc;
    QList<VideoDescription> m_videoDesc;
    QList<SubtitleDescription> m_subtitleDesc;
};

#endif // MLVIDEO_H
