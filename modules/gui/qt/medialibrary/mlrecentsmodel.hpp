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

#ifndef ML_RECENTS_MODEL_H
#define ML_RECENTS_MODEL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_media_library.h>

#include "mlbasemodel.hpp"

#include <QDateTime>

class MLRecentMedia : public MLItem {
public:
    MLRecentMedia( const vlc_ml_media_t *_data );

    MLRecentMedia( const MLRecentMedia& url );

    inline QUrl getUrl() const { return m_url; }
    inline QDateTime getLastPlayedDate() const { return m_lastPlayedDate; }

private:
    QUrl m_url;
    QDateTime m_lastPlayedDate;
};

class MLRecentsModel : public MLBaseModel
{
    Q_OBJECT
public:
    enum Roles {
        RECENT_MEDIA_ID = Qt::UserRole + 1,
        RECENT_MEDIA_URL,
        RECENT_MEDIA_LAST_PLAYED_DATE
    };
    Q_ENUM(Roles)

    explicit MLRecentsModel( QObject* parent = nullptr );
    virtual ~MLRecentsModel() = default;

    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void clearHistory();

protected:
    QVariant itemRoleData(MLItem *item, int role) const override;

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

#endif // ML_RECENTS_MODEL_H
