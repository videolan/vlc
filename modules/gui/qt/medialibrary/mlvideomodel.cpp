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

#include "mlvideomodel.hpp"
#include "mlhelper.hpp"

template<typename T>
QVariantList getVariantList(const QList<T> & desc)
{
    QVariantList list;

    for (const T & item : desc)
    {
        list.append(QVariant::fromValue(item));
    }

    return list;
}

QHash<QByteArray, vlc_ml_sorting_criteria_t> MLVideoModel::M_names_to_criteria = {
    {"title", VLC_ML_SORTING_ALPHA},
    {"duration", VLC_ML_SORTING_DURATION},
    {"playcount", VLC_ML_SORTING_PLAYCOUNT},
};

MLVideoModel::MLVideoModel(QObject* parent)
    : MLBaseModel(parent)
{
}

// Interface

/* Q_INVOKABLE */ void MLVideoModel::setItemPlayed(const QModelIndex & index, bool played)
{
    MLVideo * video = static_cast<MLVideo *>(item(index.row()));

    if (video == nullptr)
        return;

    assert(m_mediaLib);

    const MLItemId & itemId = video->getId();

    // ML thread
    m_mediaLib->runOnMLThread(this, [itemId, played] (vlc_medialibrary_t * ml)
    {
        vlc_ml_media_set_played(ml, itemId.id, played);
    });

    // NOTE: We want the change to be visible right away.
    video->setIsNew(played == false);

    emit dataChanged(index, index, { VIDEO_IS_NEW });
}

QVariant MLVideoModel::itemRoleData(MLItem *item, int role) const
{
    const auto video = static_cast<MLVideo *>(item);
    if ( video == nullptr )
        return {};

    switch (role)
    {
        case VIDEO_ID:
            return QVariant::fromValue( video->getId() );
        case VIDEO_IS_NEW:
            return QVariant::fromValue( video->isNew() );
        case VIDEO_FILENAME:
            return QVariant::fromValue( video->getFileName() );
        case VIDEO_TITLE:
            return QVariant::fromValue( video->getTitle() );
        case VIDEO_THUMBNAIL:
        {
            vlc_ml_thumbnail_status_t status;
            const QString thumbnail = video->getThumbnail(&status);
            if (status == VLC_ML_THUMBNAIL_STATUS_MISSING || status == VLC_ML_THUMBNAIL_STATUS_FAILURE)
            {
                generateThumbnail(item->getId().id);
            }
            return QVariant::fromValue( thumbnail );
        }
        case VIDEO_DURATION:
            return QVariant::fromValue( video->getDuration() );
        case VIDEO_PROGRESS:
            return QVariant::fromValue( video->getProgress() );
        case VIDEO_PLAYCOUNT:
            return QVariant::fromValue( video->getPlayCount() );
        case VIDEO_RESOLUTION:
            return QVariant::fromValue( video->getResolutionName() );
        case VIDEO_CHANNEL:
            return QVariant::fromValue( video->getChannel() );
        case VIDEO_MRL:
            return QVariant::fromValue( video->getMRL() );
        case VIDEO_DISPLAY_MRL:
            return QVariant::fromValue( video->getDisplayMRL() );
        case VIDEO_VIDEO_TRACK:
            return getVariantList( video->getVideoDesc() );
        case VIDEO_AUDIO_TRACK:
            return getVariantList( video->getAudioDesc() );
        case VIDEO_TITLE_FIRST_SYMBOL:
            return QVariant::fromValue( getFirstSymbol( video->getTitle() ) );

        default:
            return {};
    }
}

QHash<int, QByteArray> MLVideoModel::roleNames() const
{
    return {
        { VIDEO_ID, "id" },
        { VIDEO_IS_NEW, "isNew" },
        { VIDEO_FILENAME, "fileName" },
        { VIDEO_TITLE, "title" },
        { VIDEO_THUMBNAIL, "thumbnail" },
        { VIDEO_DURATION, "duration" },
        { VIDEO_PROGRESS, "progress" },
        { VIDEO_PLAYCOUNT, "playcount" },
        { VIDEO_RESOLUTION, "resolution_name" },
        { VIDEO_CHANNEL, "channel" },
        { VIDEO_MRL, "mrl" },
        { VIDEO_DISPLAY_MRL, "display_mrl" },
        { VIDEO_AUDIO_TRACK, "audioDesc" },
        { VIDEO_VIDEO_TRACK, "videoDesc" },
        { VIDEO_TITLE_FIRST_SYMBOL, "title_first_symbol"},
    };
}

vlc_ml_sorting_criteria_t MLVideoModel::roleToCriteria(int role) const
{
    switch(role)
    {
        case VIDEO_TITLE:
            return VLC_ML_SORTING_ALPHA;
        case VIDEO_DURATION:
            return VLC_ML_SORTING_DURATION;
        case VIDEO_PLAYCOUNT:
            return VLC_ML_SORTING_PLAYCOUNT;
        default:
            return VLC_ML_SORTING_DEFAULT;
    }
}

vlc_ml_sorting_criteria_t MLVideoModel::nameToCriteria(QByteArray name) const
{
    return M_names_to_criteria.value(name, VLC_ML_SORTING_DEFAULT);
}

QByteArray MLVideoModel::criteriaToName(vlc_ml_sorting_criteria_t criteria) const
{
    return M_names_to_criteria.key(criteria, "");
}

// Protected MLBaseModel reimplementation

void MLVideoModel::onVlcMlEvent(const MLEvent &event)
{
    switch (event.i_type)
    {
        case VLC_ML_EVENT_MEDIA_ADDED:
        {
            if (event.creation.media.i_type == VLC_ML_MEDIA_TYPE_VIDEO)
            {
                emit resetRequested();

                return;
            }

            break;
        }
        case VLC_ML_EVENT_MEDIA_UPDATED:
        {
            MLItemId itemId(event.modification.i_entity_id, VLC_ML_PARENT_UNKNOWN);
            updateItemInCache(itemId);

            return;
        }
        case VLC_ML_EVENT_MEDIA_DELETED:
        {
            MLItemId itemId(event.deletion.i_entity_id, VLC_ML_PARENT_UNKNOWN);
            deleteItemInCache(itemId);

            return;
        }
        default:
            break;
    }

    MLBaseModel::onVlcMlEvent(event);
}

void MLVideoModel::thumbnailUpdated(const QModelIndex& idx, MLItem* mlitem, const QString& mrl, vlc_ml_thumbnail_status_t status)
{
    auto videoItem = static_cast<MLVideo*>(mlitem);
    videoItem->setThumbnail(status, mrl);
    emit dataChanged(idx, idx, {VIDEO_THUMBNAIL});
}

void MLVideoModel::generateThumbnail(uint64_t id) const
{
    m_mediaLib->runOnMLThread(this,
    //ML thread
    [id](vlc_medialibrary_t* ml){
        vlc_ml_media_generate_thumbnail(ml, id, VLC_ML_THUMBNAIL_SMALL, 512, 320, .15 );
    });
}

std::unique_ptr<MLBaseModel::BaseLoader>
MLVideoModel::createLoader() const
{
    return std::make_unique<Loader>(*this);
}

size_t MLVideoModel::Loader::count(vlc_medialibrary_t* ml) const /* override */
{
    vlc_ml_query_params_t params = getParams().toCQueryParams();

    int64_t id = m_parent.id;

    if (id <= 0)
        return vlc_ml_count_video_media(ml, &params);
    else
        return vlc_ml_count_video_of(ml, &params, m_parent.type, id);
}

std::vector<std::unique_ptr<MLItem>>
MLVideoModel::Loader::load(vlc_medialibrary_t* ml, size_t index, size_t count) const /* override */
{
    vlc_ml_query_params_t params = getParams(index, count).toCQueryParams();

    ml_unique_ptr<vlc_ml_media_list_t> list;

    int64_t id = m_parent.id;

    if (id <= 0)
        list.reset(vlc_ml_list_video_media(ml, &params));
    else
        list.reset(vlc_ml_list_video_of(ml, &params, m_parent.type, id));

    if (list == nullptr)
        return {};

    std::vector<std::unique_ptr<MLItem>> result;

    for (const vlc_ml_media_t & media : ml_range_iterate<vlc_ml_media_t>(list))
    {
        result.emplace_back(std::make_unique<MLVideo>( &media));
    }

    return result;
}

std::unique_ptr<MLItem>
MLVideoModel::Loader::loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const
{
    assert(itemId.type == VLC_ML_PARENT_UNKNOWN);
    ml_unique_ptr<vlc_ml_media_t> media(vlc_ml_get_media(ml, itemId.id));
    if (!media)
        return nullptr;
    return std::make_unique<MLVideo>(media.get());
}

