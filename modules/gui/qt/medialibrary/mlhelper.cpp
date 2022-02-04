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

QString getVideoListCover( const MLBaseModel* model, MLItemCover* item, int width, int height,
                           int role )
{
    QString cover = item->getCover();

    // NOTE: Making sure we're not already generating a cover.
    if (cover.isNull() == false || item->hasGenerator())
        return cover;

    MLItemId itemId = item->getId();

    struct Context { QString cover; };

    item->setGenerator(true);

    model->ml()->runOnMLThread<Context>(model,
    //ML thread
    [itemId, width, height]
    (vlc_medialibrary_t * ml, Context & ctx)
    {
        CoverGenerator generator{ ml, itemId };

        generator.setSize(QSize(width, height));

        generator.setDefaultThumbnail(":/noart_videoCover.svg");

        if (generator.cachedFileAvailable())
            ctx.cover = generator.cachedFileURL();
        else
            ctx.cover = generator.execute();
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

        //we're running in a callback
        emit const_cast<MLBaseModel *>(model)->dataChanged(modelIndex, modelIndex, { role });
    });

    return cover;
}
