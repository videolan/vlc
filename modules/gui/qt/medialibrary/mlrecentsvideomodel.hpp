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

#include "mlvideomodel.hpp"

class MLRecentsVideoModel : public MLVideoModel
{
    Q_OBJECT

public:
    explicit MLRecentsVideoModel(QObject * parent = nullptr);

    virtual ~MLRecentsVideoModel() = default;

protected: // MLBaseModel implementation
    std::unique_ptr<MLBaseModel::BaseLoader> createLoader() const override;

protected: // MLVideoModel reimplementation
    void onVlcMlEvent(const MLEvent & event) override;

private:
    struct Loader : public BaseLoader
    {
        Loader(const MLRecentsVideoModel & model);

        size_t count(vlc_medialibrary_t* ml) const override;

        std::vector<std::unique_ptr<MLItem>> load(vlc_medialibrary_t* ml, size_t index, size_t count) const override;

        std::unique_ptr<MLItem> loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const override;
    };
};

#endif // ML_RECENTS_VIDEO_MODEL_H
