/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

#include "thumbnailcollector.hpp"

#include "medialibrary/medialib.hpp"
#include "medialibrary/mlhelper.hpp"


ThumbnailCollector::ThumbnailCollector(QObject *parent)
    : QObject(parent)
    , m_ml_event_handle(nullptr, [this](vlc_ml_event_callback_t *cb)
    {
        assert(m_ml);
        QMutexLocker lock(&m_mut);
        m_ml->unregisterEventListener(cb);
    })
{
}

void ThumbnailCollector::start(MediaLib *ml, const QSet<int64_t> &mlIds)
{
    assert(!m_ml); // class object is only one time usable
    m_ml = ml;
    m_ml_event_handle.reset(ml->registerEventListener(&onVlcMLEvent, this));

    m_pending = mlIds;
    m_ml->runOnMLThread(this, [ids = mlIds](vlc_medialibrary_t* ml)
    {
        for (const auto id : ids)
            vlc_ml_media_generate_thumbnail(ml, id, VLC_ML_THUMBNAIL_SMALL, 512, 320, .15);
    });
}

void ThumbnailCollector::onVlcMLEvent(void *data, const vlc_ml_event_t *event)
{
    static const auto mediaID = [](const vlc_ml_event_t *event)
    {
        return (event->i_type == VLC_ML_EVENT_MEDIA_DELETED)
                   ? event->deletion.i_entity_id
                   : event->media_thumbnail_generated.p_media->i_id;
    };

    if ((event->i_type != VLC_ML_EVENT_MEDIA_DELETED)
        && (event->i_type != VLC_ML_EVENT_MEDIA_THUMBNAIL_GENERATED))
        return;

    const auto self = reinterpret_cast<ThumbnailCollector *>(data);
    QMutexLocker lock(&self->m_mut);

    const auto id = mediaID(event);
    if (!self->m_pending.contains(id))
        return;

    self->m_pending.remove(id);

    if (event->i_type == VLC_ML_EVENT_MEDIA_THUMBNAIL_GENERATED)
    {
        const auto media = event->media_thumbnail_generated.p_media;
        const auto url = toValidLocalFile(media->thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl);

        if (event->media_thumbnail_generated.b_success && !url.isEmpty()) {
            self->m_thumbnails.insert(id, url);
        } else {
            qDebug("thumbnail generation failed, id: %" PRId64 ", url: '%s'", id, qUtf8Printable(url));
        }
    }

    if (self->m_pending.empty())
        emit self->finished();
}
