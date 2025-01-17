/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

#ifndef MLURLMODEL_H
#define MLURLMODEL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mlbasemodel.hpp"

#include "mlhelper.hpp"
#include "mlqmltypes.hpp"

class MLUrl : public MLItem {
public:
    MLUrl( const vlc_ml_media_t *_data );

    MLUrl( const MLUrl& url );

    QString getUrl() const;
    QString getLastPlayedDate() const;

private:
    QString m_url;
    QString m_lastPlayedDate;
};

class MLUrlModel : public MLBaseModel
{
    Q_OBJECT

public:
    enum Roles {
        URL_ID = Qt::UserRole + 1,
        URL_URL,
        URL_LAST_PLAYED_DATE,
        URL_IS_DELETABLE
    };
    Q_ENUM(Roles)

    explicit MLUrlModel(QObject *parent = nullptr);

    virtual ~MLUrlModel() = default;

    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addAndPlay( const QString& url );

    Q_INVOKABLE void deleteStream( const MLItemId itemId );

protected:
    QVariant itemRoleData(const MLItem *item, int role) const override;

    std::unique_ptr<MLListCacheLoader> createMLLoader() const override;

private:
    void onVlcMlEvent( const MLEvent &event ) override;

    struct Loader : public MLListCacheLoader::MLOp
    {
        using MLListCacheLoader::MLOp::MLOp;
        size_t count(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const override;
        std::vector<std::unique_ptr<MLItem>> load(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const override;
        std::unique_ptr<MLItem> loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const override;
    };
};

#endif // MLURLMODEL_H
