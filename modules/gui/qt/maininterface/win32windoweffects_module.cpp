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
#include <QRegion>

#include <cassert>

#include <dwmapi.h>
#include <winuser.h>

static bool isEffectAvailable(const QWindow* window, const WindowEffectsModule::Effect effect)
{
    // Version check is done on module open, no need to re-do it here.
    switch (effect)
    {
    case WindowEffectsModule::BlurBehind:
        // NOTE: Qt does not officially support translucent window with frame.
        //       The documentation states that `Qt::FramelessWindowHint` is
        //       required on certain platforms, such as Windows. Otherwise,
        //       The window starts with a white background, which is a widely
        //       known Windows issue regardless of translucency, but the white
        //       background is never cleared if the window clear color is
        //       translucent. In this case, minimizing and restoring the window
        //       makes the background cleared, but this still does not make
        //       it a portable solution.
        // NOTE: See QTBUG-56201, QTBUG-120691. From the reports, it appears
        //       that Nvidia graphics is "fine" with translucent framed window
        //       while Intel graphics is not. However, the said issue above
        //       is still a concern with Nvidia graphics according to my own
        //       experience.
        // TODO: Ideally, we should at least use the frameless window hint
        //       when CSD is in use and use native backdrop effect since
        //       the custom solution has more chance to cause issues.
        if (!window->flags().testFlag(Qt::FramelessWindowHint))
        {
            const auto extendedStyle = GetWindowLong((HWND)window->winId(), GWL_EXSTYLE);
            if (!(extendedStyle & 0x00200000L /* WS_EX_NOREDIRECTIONBITMAP */))
            {
                qDebug("Target window is not frameless and does not use WS_EX_NOREDIRECTIONBITMAP, " \
                       "window can not be translucent for the Windows 11 22H2 native acrylic backdrop effect.");
                return false;
            }
        }
        return true;
    default:
        return false;
    };
}

static void setBlurBehind(QWindow* const window, const bool enable = true, const QRegion& region = {})
{
    Q_UNUSED(region);

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
