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

#ifndef MLABLUMMODEL_H
#define MLABLUMMODEL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QObject>
#include "mlbasemodel.hpp"
#include "mlalbum.hpp"
#include "medialib.hpp"

class MLAlbumModel : public MLSlidingWindowModel<MLAlbum>
{
    Q_OBJECT

public:
    enum Roles
    {
        ALBUM_ID = Qt::UserRole + 1,
        ALBUM_TITLE,
        ALBUM_RELEASE_YEAR,
        ALBUM_SHORT_SUMMARY,
        ALBUM_COVER,
        ALBUM_MAIN_ARTIST,
        ALBUM_NB_TRACKS,
        ALBUM_DURATION,
        ALBUM_DURATION_SHORT,

        ALBUM_TITLE_FIRST_SYMBOL,
        ALBUM_MAIN_ARTIST_FIRST_SYMBOL
    };
    Q_ENUM(Roles)

public:
    explicit MLAlbumModel(QObject *parent = nullptr);
    virtual ~MLAlbumModel() = default;

    Q_INVOKABLE QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Q_INVOKABLE QHash<int, QByteArray> roleNames() const override;

private:
    std::vector<std::unique_ptr<MLAlbum>> fetch() override;
    size_t countTotalElements() const override;
    vlc_ml_sorting_criteria_t roleToCriteria(int role) const override;
    vlc_ml_sorting_criteria_t nameToCriteria(QByteArray name) const override;
    QByteArray criteriaToName(vlc_ml_sorting_criteria_t criteria) const override;
    virtual void onVlcMlEvent( const vlc_ml_event_t* event ) override;
    void thumbnailUpdated(int idx) override;

    static  QHash<QByteArray, vlc_ml_sorting_criteria_t> M_names_to_criteria;
};


#endif // MLABLUMMODEL_H
