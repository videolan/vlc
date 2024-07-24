/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Authors: Prince Gupta <guptaprince8832@gmail.com>
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

#pragma once

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include "mlqmltypes.hpp"
#include "util/vlctick.hpp"

#include <QObject>

class MLMedia : public MLItem
{
    Q_GADGET

    Q_PROPERTY(MLItemId id READ getId CONSTANT)
    Q_PROPERTY(QString title READ title CONSTANT FINAL)
    Q_PROPERTY(QString fileName READ fileName CONSTANT FINAL)
    Q_PROPERTY(QString smallCover READ smallCover CONSTANT FINAL)
    Q_PROPERTY(QString bannerCover READ bannerCover CONSTANT FINAL)
    Q_PROPERTY(VLCTick duration READ duration CONSTANT FINAL)
    Q_PROPERTY(qreal progress READ progress CONSTANT FINAL)
    Q_PROPERTY(int playCount READ playCount CONSTANT FINAL)

public:
    MLMedia() : MLItem {MLItemId()} {}

    MLMedia(const vlc_ml_media_t *media)
        : MLItem {MLItemId(media->i_id, VLC_ML_PARENT_UNKNOWN)}
    {
        const auto getThumbnail = [](const vlc_ml_thumbnail_t & thumbnail)
        {
            const QString mrl =
                    (thumbnail.i_status == VLC_ML_THUMBNAIL_STATUS_AVAILABLE)
                    ? qfu(thumbnail.psz_mrl) : QString {};

            return MLThumbnail{ mrl, thumbnail.i_status };
        };

        m_title = qfu(media->psz_title);
        m_fileName = qfu(media->psz_filename);
        m_smallCover = getThumbnail(media->thumbnails[VLC_ML_THUMBNAIL_SMALL]);
        m_bannerCover = getThumbnail(media->thumbnails[VLC_ML_THUMBNAIL_BANNER]);
        m_duration = VLCTick::fromMS(media->i_duration);
        m_progress = media->f_progress;
        m_playCount = media->i_playcount;
    }

    QString title() const { return m_title; }
    QString fileName() const { return m_fileName; }
    VLCTick duration() const {  return m_duration; }
    qreal progress() const { return m_progress; }
    int playCount() const { return m_playCount; }

    QString smallCover(vlc_ml_thumbnail_status_t* status = nullptr) const
    {
        if (status) *status = m_smallCover.status;
        return m_smallCover.mrl;
    }

    QString bannerCover(vlc_ml_thumbnail_status_t* status = nullptr) const
    {
        if (status) *status = m_bannerCover.status;
        return m_bannerCover.mrl;
    }

    VLCTick progressTime() const { return VLCTick::fromMS(m_duration * m_progress); }

    Q_INVOKABLE bool valid() const { return getId().id != INVALID_MLITEMID_ID; }


protected:
    struct MLThumbnail
    {
        QString mrl;
        vlc_ml_thumbnail_status_t status;
    };

    QString m_title;
    QString m_fileName;
    MLThumbnail m_smallCover;
    MLThumbnail m_bannerCover;
    VLCTick m_duration;
    qreal m_progress;
    int m_playCount;
};

