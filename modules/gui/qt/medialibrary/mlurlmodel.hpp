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

#include <QObject>
#include "mlbasemodel.hpp"

#include <vlc_media_library.h>
#include "mlhelper.hpp"
#include "mlqmltypes.hpp"

class MLUrl {
public:
    MLUrl( const vlc_ml_media_t *_data );

    MLUrl( const MLUrl& url );

    QString getUrl() const;
    QString getLastPlayedDate() const;
    MLParentId getId() const { return m_id; }

    MLUrl *clone() const;

private:
    MLParentId m_id;
    QString m_url;
    QString m_lastPlayedDate;
};

class MLUrlModel : public MLSlidingWindowModel<MLUrl>
{
    Q_OBJECT

public:
    enum Roles {
        URL_ID = Qt::UserRole + 1,
        URL_URL,
        URL_LAST_PLAYED_DATE
    };
    Q_ENUM(Roles);

    explicit MLUrlModel(QObject *parent = nullptr);

    virtual ~MLUrlModel() = default;

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addAndPlay( const QString& url );

protected:
    ListCacheLoader<std::unique_ptr<MLUrl>> *createLoader() const override;

private:
    vlc_ml_sorting_criteria_t roleToCriteria(int role) const override;
    virtual void onVlcMlEvent( const MLEvent &event ) override;

    struct Loader : public BaseLoader
    {
        Loader(const MLUrlModel &model) : BaseLoader(model) {}
        size_t count() const override;
        std::vector<std::unique_ptr<MLUrl>> load(size_t index, size_t count) const override;
    };
};

#endif // MLURLMODEL_H
