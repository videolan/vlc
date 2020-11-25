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

#ifndef ML_RECENTS_VIDEO_MODEL_H
#define ML_RECENTS_VIDEO_MODEL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_media_library.h>

#include "mlbasemodel.hpp"
#include "mlvideo.hpp"

#include <QObject>

class MLRecentsVideoModel : public MLSlidingWindowModel
{
    Q_OBJECT
    Q_PROPERTY(int numberOfItemsToShow READ getNumberOfItemsToShow WRITE setNumberOfItemsToShow)

public:
    explicit MLRecentsVideoModel( QObject* parent = nullptr );
    virtual ~MLRecentsVideoModel() = default;

    QVariant data( const QModelIndex& index , int role ) const override;
    QHash<int, QByteArray> roleNames() const override;
    int numberOfItemsToShow = 10;

protected:
    ListCacheLoader<std::unique_ptr<MLItem>> *createLoader() const override;

private:
    vlc_ml_sorting_criteria_t roleToCriteria( int /* role */ ) const override{
        return VLC_ML_SORTING_DEFAULT;
    }
    vlc_ml_sorting_criteria_t nameToCriteria( QByteArray /* name */ ) const override{
        return VLC_ML_SORTING_DEFAULT;
    }
    virtual void onVlcMlEvent( const MLEvent &event ) override;
    void setNumberOfItemsToShow(int);
    int getNumberOfItemsToShow();

    struct Loader : public BaseLoader
    {
        Loader(const MLRecentsVideoModel &model, int numberOfItemsToShow)
            : BaseLoader(model)
            , m_numberOfItemsToShow(numberOfItemsToShow)
        {
        }

        size_t count() const override;
        std::vector<std::unique_ptr<MLItem>> load(size_t index, size_t count) const override;

    private:
        int m_numberOfItemsToShow;
        // FIXME: count() may not depend on load(), since the call to load()
        // depends on count()
        mutable int m_video_count;
    };
};

#endif // ML_RECENTS_VIDEO_MODEL_H
