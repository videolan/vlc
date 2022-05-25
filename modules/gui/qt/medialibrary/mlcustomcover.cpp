/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

#include "mlcustomcover.hpp"

#include "medialibrary/medialib.hpp"
#include "medialibrary/mlhelper.hpp"
#include "medialibrary/thumbnailcollector.hpp"
#include "util/asynctask.hpp"
#include "util/covergenerator.hpp"

#include <qhashfunctions.h>

#include <QCache>
#include <QMutex>
#include <QUrl>
#include <QUrlQuery>

namespace
{

const QString ID_KEY = QStringLiteral("id");
const QString TYPE_KEY = QStringLiteral("type");
const QString WIDTH_KEY = QStringLiteral("width");
const QString HEIGHT_KEY = QStringLiteral("height");
const QString COUNTX_KEY = QStringLiteral("countX");
const QString COUNTY_KEY = QStringLiteral("countY");
const QString BLUR_KEY = QStringLiteral("blur");
const QString SPLIT_KEY = QStringLiteral("split");
const QString DEFAULT_COVER_KEY = QStringLiteral("default_cover");

struct CoverData
{
    MLItemId id;
    QSize size;
    int countX;
    int countY;
    int blur;
    int split;
    QString defaultCover;
};

QUrlQuery toQuery(const CoverData &data)
{
    QUrlQuery query;
    query.addQueryItem(ID_KEY, QString::number(data.id.id));
    query.addQueryItem(TYPE_KEY, QString::number(data.id.type));
    query.addQueryItem(WIDTH_KEY, QString::number(data.size.width()));
    query.addQueryItem(HEIGHT_KEY, QString::number(data.size.height()));
    query.addQueryItem(COUNTX_KEY, QString::number(data.countX));
    query.addQueryItem(COUNTY_KEY, QString::number(data.countY));
    query.addQueryItem(BLUR_KEY, QString::number(data.blur));
    query.addQueryItem(SPLIT_KEY, QString::number(data.split));
    query.addQueryItem(DEFAULT_COVER_KEY, data.defaultCover);
    return query;
}

CoverData fromQuery(const QUrlQuery &query, QString *error)
{
    try
    {
        const auto getValue = [&](const QString &key)
        {
            if (!query.hasQueryItem(key))
                throw QString("key '%1' doesn't exist").arg(key);

            return query.queryItemValue(key);
        };

        const auto intValue = [&](const QString &key)
        {
            auto value = getValue(key);
            bool ok;
            int iValue = value.toInt(&ok);
            if (!ok)
                throw QString("invalid value for key '%1'").arg(key);

            return iValue;
        };

        CoverData data;
        data.id.id = intValue(ID_KEY);
        data.id.type = static_cast<vlc_ml_parent_type>(intValue(TYPE_KEY));
        data.size.setWidth(intValue(WIDTH_KEY));
        data.size.setHeight(intValue(HEIGHT_KEY));
        data.countX = intValue(COUNTX_KEY);
        data.countY = intValue(COUNTY_KEY);
        data.blur = intValue(BLUR_KEY);
        data.split = intValue(SPLIT_KEY);
        data.defaultCover = getValue(DEFAULT_COVER_KEY);

        return data;
    }
    catch (const QString &e)
    {
        if (error)
            *error = e;
        return {};
    }
}


struct ThumbnailList
{
    QSet<int64_t> toGenerate;
    QStringList existing;
};

QStringList getGenreMediaThumbnails(vlc_medialibrary_t* p_ml, const int count, const int64_t id)
{
    QStringList thumbnails;

    vlc_ml_query_params_t params {};

    // NOTE: We retrieve twice the count to maximize our chances to get a valid thumbnail.
    params.i_nbResults = count * 2;

    ml_unique_ptr<vlc_ml_album_list_t> list(vlc_ml_list_genre_albums(p_ml, &params, id));

    thumbnailCopy(ml_range_iterate<vlc_ml_album_t>(list), std::back_inserter(thumbnails), count);

    return thumbnails;
}

ThumbnailList extractChildMediaThumbnailsOrIDs(vlc_medialibrary_t *p_ml, const int count, const MLItemId &itemID)
{
    ThumbnailList result;

    vlc_ml_query_params_t params {};

    // NOTE: We retrieve twice the count to maximize our chances to get a valid thumbnail.
    params.i_nbResults = count * 2;

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

    if (result.existing.size() > count)
    {
        const auto removeStart = result.existing.end() - (result.existing.size() - count);
        result.existing.erase(removeStart, result.existing.end());
    }

    while (result.toGenerate.size() + result.existing.size() > count)
    {
        result.toGenerate.erase(result.toGenerate.begin());
    }

    return result;
}

} // anonymous namespace

class CustomCoverImageResponse : public QQuickImageResponse
{
public:
    CustomCoverImageResponse(CoverData data, MediaLib *ml)
        : ml {ml}
        , data{data}
    {
        // uses Qt::QueuedConnection to give the receiver time to connect to finish()
        QMetaObject::invokeMethod(this, &CustomCoverImageResponse::start, Qt::QueuedConnection);
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return !image.isNull() ? QQuickTextureFactory::textureFactoryForImage(image) : nullptr;
    }

private:
    void start()
    {
        const int thumbnailCount = data.countX * data.countY;

        ml->runOnMLThread<ThumbnailList>(this,
            //ML thread (get child thumbnails or ids)
            [itemId = data.id, thumbnailCount](vlc_medialibrary_t *p_ml, ThumbnailList &ctx)
            {
                if (itemId.type == VLC_ML_PARENT_GENRE)
                    ctx.existing = getGenreMediaThumbnails(p_ml, thumbnailCount, itemId.id);
                else
                    ctx = extractChildMediaThumbnailsOrIDs(p_ml, thumbnailCount, itemId);
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
                auto collector = new ThumbnailCollector(this);
                QObject::connect(collector, &ThumbnailCollector::finished, this, [=]()
                {
                    const auto thumbnails = ctx.existing + collector->allGenerated().values();
                    generateCover(thumbnails);

                    collector->deleteLater();
                });

                collector->start(ml, ctx.toGenerate);
            }
            );
    }

    void generateCover(const QStringList &thumbnails)
    {
        struct Context { QImage img; };

        ml->runOnMLThread<Context>(this,
            //ML thread
            [data = this->data, thumbnails]
            (vlc_medialibrary_t * , Context & ctx)
            {
                CoverGenerator generator;
                generator.setCountX(data.countX);
                generator.setCountY(data.countY);
                generator.setSize(data.size);
                generator.setSplit((CoverGenerator::Split)data.split);
                generator.setBlur(data.blur);

                if (!data.defaultCover.isEmpty())
                    generator.setDefaultThumbnail(data.defaultCover);

                ctx.img = generator.execute(thumbnails);
            },
            //UI Thread
            [this]
            (quint64, Context & ctx)
            {
                doFinish(ctx.img);
            }
            );
    }

    void doFinish(const QImage &result)
    {
        image = result;
        emit finished();
    }

    MediaLib *ml;
    CoverData data;
    QImage image;
};


MLCustomCover::MLCustomCover(const QString &providerId, MediaLib *ml)
    : m_providerId {providerId}
    , m_ml {ml}
{
}

QString MLCustomCover::get(const MLItemId &parentId, const QSize &size, const QString &defaultCover
                           , const int countX, const int countY, const int blur, const bool split_duplicate)
{
    QUrl url;
    url.setScheme(QStringLiteral("image"));
    url.setHost(m_providerId);
    url.setQuery(toQuery({parentId, size, countX, countY, blur, split_duplicate, defaultCover}));
    return url.toString();
}

QQuickImageResponse *MLCustomCover::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    QString error;
    CoverData data = fromQuery(QUrlQuery(id), &error);
    if (!error.isEmpty())
    {
        qDebug("failed to parse url %s, error %s", qUtf8Printable(id), qUtf8Printable(error));
        return nullptr;
    }

    if (requestedSize.isValid())
        data.size = requestedSize;

    return new CustomCoverImageResponse(data, m_ml);
}
