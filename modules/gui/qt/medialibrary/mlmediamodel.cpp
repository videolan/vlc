/******************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Author: Ash <ashutoshv191@gmail.com>
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
 ******************************************************************************/
#include "util/vlctick.hpp"

#include "mlmediamodel.hpp"
#include "mlhelper.hpp"
#include <QFile>

MLMediaModel::MLMediaModel(QObject *par)
    : MLBaseModel(par)
{
}

void MLMediaModel::setMediaIsFavorite(const QModelIndex &index, bool isFavorite)
{
    assert(m_mediaLib != nullptr);

    const auto media = static_cast<MLMedia *>(item(index.row()));
    assert(media != nullptr);

    // on ML thread
    m_mediaLib->runOnMLThread(this, [id = media->getId().id, isFavorite](vlc_medialibrary_t *ml)
    {
        vlc_ml_media_set_favorite(ml, id, isFavorite);
    });

    media->setIsFavorite(isFavorite);

    emit dataChanged(index, index, { MEDIA_IS_FAVORITE });
}

QUrl MLMediaModel::getParentURL(const QModelIndex &index) {
    const auto media = static_cast<MLMedia *>(item(index.row()));
    assert(media != nullptr);
    return getParentURL(media);
}

QUrl MLMediaModel::getParentURL(const MLMedia *media) const{
    assert(media != nullptr);
    return getParentURLFromURL(media->mrl());
}

bool MLMediaModel::getPermissions(const MLMedia* media) const
{
    QString mrl = media->mrl();

    QUrl fileUrl(mrl);
    QString localPath = fileUrl.toLocalFile();

    if (localPath.isEmpty())
        return false;

    QFileDevice::Permissions filePermissions = QFile::permissions(localPath);

    if (!(filePermissions & QFileDevice::WriteUser))
        return false;

    QUrl parentUrl = getParentURL(media);
    QString parentPath = parentUrl.toLocalFile();

    if (parentPath.isEmpty())
        return false;

    QFileDevice::Permissions folderPermissions = QFile::permissions(parentPath);

    return folderPermissions & QFileDevice::WriteUser;
}

void MLMediaModel::deleteFileFromSource(const QModelIndex &index)
{
    const auto media = static_cast<MLMedia *>(item(index.row()));
    QString mrl = media->mrl();

    QUrl fileUrl(mrl);
    QString localPath = fileUrl.toLocalFile();

    QFile file(localPath);
    bool removed = file.remove();
    if (!removed)
        return;

    QUrl parentUrl = getParentURL(index);
    QString parentUrlString = parentUrl.toString();

    m_mediaLib->runOnMLThread(this,
                              [parentUrlString](vlc_medialibrary_t *ml)
                              {
                                  vlc_ml_reload_folder(ml, qtu(parentUrlString));
                              });
}

vlc_ml_sorting_criteria_t MLMediaModel::nameToCriteria(QByteArray name) const
{
    return QHash<QByteArray, vlc_ml_sorting_criteria_t> {
        {"title", VLC_ML_SORTING_ALPHA},
        {"duration", VLC_ML_SORTING_DURATION},
        {"insertion", VLC_ML_SORTING_INSERTIONDATE},
    }.value(name, VLC_ML_SORTING_DEFAULT);
}

QHash<int, QByteArray> MLMediaModel::roleNames() const
{
    return {
        {MEDIA_ID, "id"},
        {MEDIA_TITLE, "title"},
        {MEDIA_TITLE_FIRST_SYMBOL, "title_first_symbol"},
        {MEDIA_FILENAME, "fileName"},
        {MEDIA_SMALL_COVER, "small_cover"},
        {MEDIA_DURATION, "duration"},
        {MEDIA_MRL, "mrl"},
        {MEDIA_IS_VIDEO, "isVideo"},
        {MEDIA_IS_FAVORITE, "isFavorite"},
        {MEDIA_IS_LOCAL, "isLocal"},
        {MEDIA_PROGRESS, "progress"},
        {MEDIA_PLAYCOUNT, "playcount"},
        {MEDIA_LAST_PLAYED_DATE, "last_played_date"},
        {MEDIA_IS_DELETABLE_FILE, "isDeletableFile"},
    };
}

QVariant MLMediaModel::itemRoleData(const MLItem *item, int role) const
{
    const auto media = static_cast<const MLMedia *>(item);
    assert(media != nullptr);

    switch(role)
    {
    case MEDIA_ID:
        return QVariant::fromValue(media->getId());
    case MEDIA_TITLE:
        return QVariant::fromValue(media->title());
    case MEDIA_TITLE_FIRST_SYMBOL:
        return QVariant::fromValue(getFirstSymbol(media->title()));
    case MEDIA_FILENAME:
        return QVariant::fromValue(media->fileName());
    case MEDIA_SMALL_COVER: // depends on the type of media
        if (const auto video = dynamic_cast<const MLVideo *>(media))
        {
            vlc_ml_thumbnail_status_t status;
            const QString thumbnail(video->smallCover(&status));
            if (status == VLC_ML_THUMBNAIL_STATUS_MISSING || status == VLC_ML_THUMBNAIL_STATUS_FAILURE)
                generateThumbnail(video);
            return QVariant::fromValue(thumbnail);
        }
        else if (const auto audio = dynamic_cast<const MLAudio *>(media))
            return QVariant::fromValue(audio->smallCover());
        else
            Q_UNREACHABLE();
    case MEDIA_DURATION:
        return QVariant::fromValue(media->duration());
    case MEDIA_MRL:
        return QVariant::fromValue(media->mrl());
    case MEDIA_IS_VIDEO:
        return QVariant::fromValue(media->type() != VLC_ML_MEDIA_TYPE_AUDIO);
    case MEDIA_IS_FAVORITE:
        return QVariant::fromValue(media->isFavorite());
    case MEDIA_IS_LOCAL:
    {
        QUrl mediaUrl(media->mrl());
        return QVariant::fromValue(mediaUrl.isLocalFile());
    }
    case MEDIA_PROGRESS:
        return QVariant::fromValue(media->progress());
    case MEDIA_PLAYCOUNT:
        return QVariant::fromValue(media->playCount());
    case MEDIA_LAST_PLAYED_DATE:
        return QVariant::fromValue( media->lastPlayedDate().toString( QLocale::system().dateFormat( QLocale::ShortFormat ) ) );
    case MEDIA_IS_DELETABLE_FILE:
        return QVariant::fromValue(getPermissions(media));
    }
    return {};
}

void MLMediaModel::onVlcMlEvent(const MLEvent &event)
{
    switch(event.i_type)
    {
    // common
    case VLC_ML_EVENT_MEDIA_ADDED:
        emit resetRequested();
        return;
    case VLC_ML_EVENT_MEDIA_UPDATED:
    {
        MLItemId itemId(event.modification.i_entity_id, VLC_ML_PARENT_UNKNOWN);
        updateItemInCache(itemId);
        return;
    }
    case VLC_ML_EVENT_MEDIA_DELETED:
    {
        MLItemId itemId(event.modification.i_entity_id, VLC_ML_PARENT_UNKNOWN);
        deleteItemInCache(itemId);
        return;
    }
    // audio specific
    case VLC_ML_EVENT_ALBUM_UPDATED:
        if (m_parent.id != 0 && m_parent.type == VLC_ML_PARENT_ALBUM && m_parent.id == event.modification.i_entity_id)
            emit resetRequested();
        return;
    case VLC_ML_EVENT_ALBUM_DELETED:
        if (m_parent.id != 0 && m_parent.type == VLC_ML_PARENT_ALBUM && m_parent.id == event.deletion.i_entity_id)
            emit resetRequested();
        return;
    case VLC_ML_EVENT_GENRE_DELETED:
        if (m_parent.id != 0 && m_parent.type == VLC_ML_PARENT_GENRE && m_parent.id == event.deletion.i_entity_id)
            emit resetRequested();
        return;
    default:
        MLBaseModel::onVlcMlEvent(event);
    }
}

void MLMediaModel::generateThumbnail(const MLVideo *video) const
{
    assert(video != nullptr);

    // on ML thread
    m_mediaLib->runOnMLThread(this, [id = video->getId().id](vlc_medialibrary_t *ml)
    {
        vlc_ml_media_generate_thumbnail(ml, id, VLC_ML_THUMBNAIL_SMALL, 512, 320, .15);
    });
}

// Loader related member functions
std::unique_ptr<MLListCacheLoader>
MLMediaModel::createMLLoader() const
{
    return std::make_unique<MLListCacheLoader>(m_mediaLib, std::make_shared<MLMediaModel::Loader>(*this));
}

size_t MLMediaModel::Loader::count(vlc_medialibrary_t *ml, const vlc_ml_query_params_t *queryParams) const
{
    if (m_parent.id <= 0)
        return vlc_ml_count_media(ml, queryParams);
    return vlc_ml_count_media_of(ml, queryParams, m_parent.type, m_parent.id);
}

std::vector<std::unique_ptr<MLItem>>
MLMediaModel::Loader::load(vlc_medialibrary_t *ml, const vlc_ml_query_params_t *queryParams) const
{
    ml_unique_ptr<vlc_ml_media_list_t> mediaList;

    if (m_parent.id <= 0)
        mediaList.reset(vlc_ml_list_media(ml, queryParams));
    else
        mediaList.reset(vlc_ml_list_media_of(ml, queryParams, m_parent.type, m_parent.id));

    if (mediaList == nullptr)
        return {};

    std::vector<std::unique_ptr<MLItem>> res;

    for (vlc_ml_media_t &media : ml_range_iterate<vlc_ml_media_t>(mediaList))
        if (media.i_type != VLC_ML_MEDIA_TYPE_AUDIO)
            res.emplace_back(std::make_unique<MLVideo>(&media));
        else if (media.i_type == VLC_ML_MEDIA_TYPE_AUDIO)
            res.emplace_back(std::make_unique<MLAudio>(ml, &media));
        else
            Q_UNREACHABLE();
    return res;
}

std::unique_ptr<MLItem>
MLMediaModel::Loader::loadItemById(vlc_medialibrary_t *ml, MLItemId itemId) const
{
    assert(itemId.type == VLC_ML_PARENT_UNKNOWN);
    ml_unique_ptr<vlc_ml_media_t> media(vlc_ml_get_media(ml, itemId.id));

    if (media == nullptr)
        return nullptr;

    if (media->i_type != VLC_ML_MEDIA_TYPE_AUDIO)
        return std::make_unique<MLVideo>(media.get());
    else if (media->i_type == VLC_ML_MEDIA_TYPE_AUDIO)
        return std::make_unique<MLAudio>(ml, media.get());
    else
        Q_UNREACHABLE();
    return nullptr;
}
