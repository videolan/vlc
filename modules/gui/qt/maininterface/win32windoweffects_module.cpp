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
#include "win32windoweffects_module.hpp"
#include "windoweffects_module.hpp"

#include <QOperatingSystemVersion>
#include <QWindow>

#include <cassert>

#include <dwmapi.h>

static bool isEffectAvailable(const WindowEffectsModule::Effect effect)
{
    // Version check is done on module open, no need to re-do it here.
    switch (effect)
    {
    case WindowEffectsModule::BlurBehind:
        return true;
    default:
        return false;
    };
}

static void setBlurBehind(QWindow* const window, const bool enable = true)
{
    assert(window);
    assert(window->winId()); // use QWindow::create() before calling this function

    enum BackdropType
    {
        DWMSBT_NONE = 1,
        DWMSBT_TRANSIENTWINDOW = 3
    } backdropType = enable ? DWMSBT_TRANSIENTWINDOW : DWMSBT_NONE;

    DwmSetWindowAttribute(reinterpret_cast<HWND>(window->winId()),
                          38 /* DWMWA_SYSTEMBACKDROP_TYPE */,
                          &backdropType,
                          sizeof(backdropType));
}

int QtWin32WindowEffectsOpen(vlc_object_t* p_this)
{
    assert(p_this);

    if (QOperatingSystemVersion::current()
        < QOperatingSystemVersion(QOperatingSystemVersion::Windows, 10, 0, 22621))
        return VLC_EGENERIC;

    const auto obj = reinterpret_cast<WindowEffectsModule*>(p_this);

    obj->setBlurBehind = setBlurBehind;
    obj->isEffectAvailable = isEffectAvailable;

    return VLC_SUCCESS;
}
