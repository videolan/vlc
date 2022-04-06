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

#include "mlhelper.hpp"

// MediaLibrary includes
#include "mlbasemodel.hpp"
#include "mlitemcover.hpp"
#include "thumbnailcollector.hpp"

namespace
{

struct ThumbnailList
{
    QSet<int64_t> toGenerate;
    QStringList existing;
};

ThumbnailList extractChildMediaThumbnailsOrIDs(vlc_medialibrary_t *p_ml, const int count, const MLItemId &itemID)
{
    ThumbnailList result;

    vlc_ml_query_params_t params {};
    params.i_nbResults = count;

    ml_unique_ptr<vlc_ml_media_list_t> list(vlc_ml_list_media_of(p_ml, &params, itemID.type, itemID.id));

    for (const auto &media : ml_range_iterate<vlc_ml_media_t>(list))
    {
        const bool isThumbnailAvailable = (media.thumbnails[VLC_ML_THUMBNAIL_SMALL].i_status == VLC_ML_THUMBNAIL_STATUS_AVAILABLE);
        if (isThumbnailAvailable)
        {
            result.existing.push_back(toValidLocalFile(media.thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl));
        } else if (media.i_type == VLC_ML_MEDIA_TYPE_VIDEO)
        {
            result.toGenerate.insert(media.i_id);
        }
    }

    return result;
}

}

QString MsToString( int64_t time , bool doShort )
{
    if (time < 0)
        return "--:--";

    int t_sec = time / 1000;
    int sec = t_sec % 60;
    int min = (t_sec / 60) % 60;
    int hour = t_sec / 3600;
    if (hour == 0)
        return QString("%1:%2")
                .arg(min, 2, 10, QChar('0'))
                .arg(sec, 2, 10, QChar('0'));
    else if ( doShort )
        return QString("%1h%2")
                .arg(hour)
                .arg(min, 2, 10, QChar('0'));
    else
        return QString("%1:%2:%3")
                .arg(hour, 2, 10, QChar('0'))
                .arg(min, 2, 10, QChar('0'))
                .arg(sec, 2, 10, QChar('0'));

}

QStringList extractMediaThumbnails(vlc_medialibrary_t *p_ml, const int count, const MLItemId &itemID)
{
    // NOTE: We retrieve twice the count to maximize our chances to get a valid thumbnail.
    return extractChildMediaThumbnailsOrIDs(p_ml, count * 2, itemID).existing;
}

QString createGroupMediaCover(const MLBaseModel* model, MLItemCover* parent
                              , int role
                              , const std::shared_ptr<CoverGenerator> generator)
{
    QString cover = parent->getCover();

    // NOTE: Making sure we're not already generating a cover.
    if (cover.isNull() == false || parent->hasGenerator())
        return cover;

    if (generator->cachedFileAvailable())
        return generator->cachedFileURL();

    MLItemId itemId = parent->getId();
    parent->setGenerator(true);

    const auto generateCover = [=](const QStringList &childCovers)
    {
        struct Context { QString cover; };

        model->ml()->runOnMLThread<Context>(model,
            //ML thread
            [generator, childCovers]
            (vlc_medialibrary_t * , Context & ctx)
            {
                ctx.cover = generator->execute(childCovers);
            },
            //UI Thread
            [model, itemId, role]
            (quint64, Context & ctx)
            {
                int row;

                // NOTE: We want to avoid calling 'MLBaseModel::item' for performance issues.
                auto item = static_cast<MLItemCover *>(model->findInCache(itemId, &row));
                if (!item)
                    return;

                item->setCover(ctx.cover);
                item->setGenerator(false);

                QModelIndex modelIndex = model->index(row);
                emit const_cast<MLBaseModel *>(model)->dataChanged(modelIndex, modelIndex, { role });
            }
        );
    };

    model->ml()->runOnMLThread<ThumbnailList>(model,
        //ML thread (get child thumbnails or ids)
        [itemId, generator](vlc_medialibrary_t *p_ml, ThumbnailList &ctx)
        {
            ctx = extractChildMediaThumbnailsOrIDs(p_ml, generator->requiredNoOfThumbnails(), itemId);
        }
        //UI Thread
        , [=](quint64, ThumbnailList & ctx)
        {
            if (ctx.toGenerate.empty())
            {
                generateCover(ctx.existing);
                return;
            }

            // request child thumbnail generation, when finished generate the cover
            auto collector = new ThumbnailCollector(const_cast<MLBaseModel *>(model));
            QObject::connect(collector, &ThumbnailCollector::finished, model, [=]()
            {
                const auto thumbnails = ctx.existing + collector->allGenerated().values();
                generateCover(thumbnails);

                collector->deleteLater();
            });

            collector->start(model->ml(), ctx.toGenerate);
        }
    );

    return cover;
}

QString toValidLocalFile(const char *mrl)
{
    QUrl url(mrl);
    return url.isLocalFile() ? url.toLocalFile() : QString {};
}
