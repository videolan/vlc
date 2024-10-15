/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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
#ifndef WINDOWEFFECTSMODULE_HPP
#define WINDOWEFFECTSMODULE_HPP

#include <vlc_common.h>

#include <QRegion>

class QWindow;

struct WindowEffectsModule
{
    enum Effect {
        BlurBehind
    };

    vlc_object_t obj;

    module_t *p_module = nullptr;
    void *p_sys = nullptr;

    bool (*isEffectAvailable)(const QWindow* window, Effect effect);

    // NOTE: Region is not guaranteed to be respected.
    // WARNING: Care must be taken for Qt library ABI compatibility.
    //          For example, KWindowSystem may be built with Qt that are not
    //          binary compatible with the version the application uses.
    void (*setBlurBehind)(QWindow* window, bool enable, const QRegion& region);
};

#endif // WINDOWEFFECTSMODULE_HPP
