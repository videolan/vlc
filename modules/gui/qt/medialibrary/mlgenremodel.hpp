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

#ifndef MLGENREMODEL_HPP
#define MLGENREMODEL_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <memory>
#include "mlbasemodel.hpp"
#include "mlgenre.hpp"

class MLGenreModel : public MLBaseModel
{
    Q_OBJECT

    Q_PROPERTY(QString coverDefault READ getCoverDefault WRITE setCoverDefault
               NOTIFY coverDefaultChanged FINAL)

public:
    enum Roles
    {
        GENRE_ID = Qt::UserRole + 1,
        GENRE_NAME,
        GENRE_NB_TRACKS,
        GENRE_ARTISTS,
        GENRE_TRACKS,
        GENRE_ALBUMS,
        GENRE_COVER
    };

public:
    explicit MLGenreModel(QObject *parent = nullptr);
    virtual ~MLGenreModel() = default;

    QHash<int, QByteArray> roleNames() const override;

    QString getCoverDefault() const;
    void setCoverDefault(const QString& defaultCover);

signals:
    void coverDefaultChanged() const;

protected:
    QVariant itemRoleData(MLItem *item, int role) const override;

    std::unique_ptr<MLBaseModel::BaseLoader> createLoader() const override;

private:
    void onVlcMlEvent(const MLEvent &event) override;
    vlc_ml_sorting_criteria_t roleToCriteria(int role) const override;
    vlc_ml_sorting_criteria_t nameToCriteria(QByteArray name) const override;

    QString getCover(MLGenre * genre) const;

    QString m_coverDefault;

private:
    struct Loader : public BaseLoader
    {
        Loader(const MLGenreModel &model) : BaseLoader(model) {}
        size_t count(vlc_medialibrary_t* ml) const override;
        std::vector<std::unique_ptr<MLItem>> load(vlc_medialibrary_t* ml, size_t index, size_t count) const override;
        std::unique_ptr<MLItem> loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const override;
    };

private: // Variables
    static QHash<QByteArray, vlc_ml_sorting_criteria_t> M_names_to_criteria;
};


#endif // MLGENREMODEL_HPP
