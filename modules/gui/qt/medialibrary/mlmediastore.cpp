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


#include "mlmediastore.hpp"

#include "mlhelper.hpp"
#include "medialib.hpp"
#include "mlmedia.hpp"

static const char *UPDATE_QUEUE = "MLMEDIASTORE_UPDATEQUEUE";

MLMediaStore::~MLMediaStore() = default;

MLMediaStore::MLMediaStore(MediaLib *ml, QObject *parent)
    : QObject(parent)
    , m_ml {ml}
    , m_ml_event_handle( nullptr, [this](vlc_ml_event_callback_t* cb )
    {
        assert( m_ml != nullptr );
        m_ml->unregisterEventListener( cb );
    })
{
    m_ml_event_handle.reset
    (
        m_ml->registerEventListener(MLMediaStore::onVlcMlEvent, this)
    );
}

void MLMediaStore::insert(const QString &mrl)
{
    struct Ctx
    {
        MLMedia media;
    };

    m_files.insert(mrl);

    m_ml->runOnMLThread<Ctx>(this,
    //ML thread
    [mrl](vlc_medialibrary_t *ml, Ctx &ctx)
    {
        ml_unique_ptr<vlc_ml_media_t> media {vlc_ml_get_media_by_mrl(ml, qtu(mrl))};
        if (media)
            ctx.media = MLMedia(media.get());
    },
    //UI thread
    [this, mrl](quint64, Ctx &ctx)
    {
        if (!ctx.media.valid())
            return; // failed to get media, TODO: notify??

        setMedia(mrl, std::move(ctx.media));
    });
}

void MLMediaStore::remove(const MLItemId &id)
{
    if (!m_mrls.contains(id))
        return;

    const QString mrl = m_mrls[id];

    m_mrls.remove(id);
    m_files.remove(mrl);
}

void MLMediaStore::clear()
{
    m_mrls.clear();
}

void MLMediaStore::onVlcMlEvent(void *data, const vlc_ml_event_t *event)
{
    auto self = static_cast<MLMediaStore*>(data);
    switch (event->i_type)
    {
    case VLC_ML_EVENT_MEDIA_ADDED:
    {
        const vlc_ml_media_t *media = event->creation.p_media;
        QString mrl;
        for (const vlc_ml_file_t &file : ml_range_iterate<vlc_ml_file_t>(media->p_files))
        {
            if (file.i_type == VLC_ML_FILE_TYPE_MAIN)
            {
                mrl = QString::fromUtf8(file.psz_mrl);
                break;
            }
        }

        if (mrl.isEmpty())
            break;

        MLMedia mlMedia (media);
        QMetaObject::invokeMethod(self, [self, mrl, mlMedia]()
        {
            self->setMedia(mrl, mlMedia);
        });

        break;
    }

    case VLC_ML_EVENT_MEDIA_UPDATED:
    {
        const MLItemId id(event->modification.i_entity_id, VLC_ML_PARENT_UNKNOWN);
        QMetaObject::invokeMethod(self, [self, id] () mutable
        {
            self->update(id);
        });

        break;
    }

    case VLC_ML_EVENT_MEDIA_DELETED:
    {
        const MLItemId id{ event->deletion.i_entity_id, VLC_ML_PARENT_UNKNOWN };
        QMetaObject::invokeMethod(self, [self, id]()
        {
            self->remove(id);
        });

        break;
    }

    default:
        break;
    }
}

void MLMediaStore::update(const MLItemId &id)
{
    if (!m_mrls.contains(id))
        return;

    struct Ctx
    {
        MLMedia media;
    };

    m_ml->runOnMLThread<Ctx>(this,
    //ML thread
    [id](vlc_medialibrary_t *ml, Ctx &ctx)
    {
        ml_unique_ptr<vlc_ml_media_t> media {vlc_ml_get_media(ml, id.id)};
        ctx.media = media ? MLMedia(media.get()) : MLMedia {};
    },
    //UI thread
    [this, id](quint64, Ctx &ctx)
    {
        if (!m_mrls.contains(id))
            return; // item was removed?

        const QString mrl = m_mrls[id];
        emit updated(mrl, ctx.media);
    }, UPDATE_QUEUE); // update in a single queue in case there
                      // is a overlap of same media update
}

void MLMediaStore::setMedia(const QString &mrl, MLMedia media)
{
    if (!m_files.contains(mrl))
        return;

    m_mrls[media.getId()] = mrl;
    emit updated(mrl, media);
}
