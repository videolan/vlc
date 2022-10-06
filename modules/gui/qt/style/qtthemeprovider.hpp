/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

#ifndef QTTHEMEPROVIDER_HPP
#define QTTHEMEPROVIDER_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#define VLC_QT_INTF_PUBLIC_COLORS(X) \
    X(text) \
    X(textInactive) \
    X(textDisabled) \
    X(bg) \
    X(bgInactive) \
    X(bgAlt) \
    X(bgAltInactive) \
    X(bgHover) \
    X(bgHoverText) \
    X(bgHoverInactive) \
    X(bgHoverTextInactive) \
    X(bgFocus) \
    X(button) \
    X(buttonText) \
    X(buttonBorder) \
    X(textActiveSource) \
    X(topBanner) \
    X(lowerBanner) \
    X(accent) \
    X(alert) \
    X(separator) \
    X(playerControlBarFg) \
    X(expandDelegate) \
    X(tooltipTextColor) \
    X(tooltipColor) \
    X(border) \
    X(buttonHover) \
    X(buttonBanner) \
    X(buttonPrimaryHover) \
    X(buttonPlayer) \
    X(grid) \
    X(gridSelect) \
    X(listHover) \
    X(textField) \
    X(textFieldHover) \
    X(icon) \
    X(sliderBarMiniplayerBgColor) \
    X(windowCSDButtonBg)

#define DEFINE_QCOLOR_STRUCT(x) void* x;



struct vlc_qt_palette_t
{
    VLC_QT_INTF_PUBLIC_COLORS(DEFINE_QCOLOR_STRUCT)
};

#undef DEFINE_QCOLOR_STRUCT

enum vlc_qt_theme_image_type
{
    VLC_QT_THEME_IMAGE_TYPE_CSD_BUTTON
};

enum vlc_qt_theme_csd_button_type {
    VLC_QT_THEME_BUTTON_MAXIMIZE = 0,
    VLC_QT_THEME_BUTTON_MINIMIZE,
    VLC_QT_THEME_BUTTON_RESTORE,
    VLC_QT_THEME_BUTTON_CLOSE,
    VLC_QT_THEME_BUTTON_TYPE_COUNT
};

enum vlc_qt_theme_csd_button_state {
    VLC_QT_THEME_BUTTON_STATE_DISABLED = 0,
    VLC_QT_THEME_BUTTON_STATE_HOVERED,
    VLC_QT_THEME_BUTTON_STATE_NORMAL,
    VLC_QT_THEME_BUTTON_STATE_PRESSED,
    VLC_QT_THEME_BUTTON_STATE_COUNT
};

struct vlc_qt_theme_image_setting {
    float windowScaleFactor;
    float userScaleFacor;
    union {
        struct {
            vlc_qt_theme_csd_button_type buttonType;
            vlc_qt_theme_csd_button_state state;
            bool maximized;
            bool active;
            int bannerHeight;
        } csdButton;
    } u;
};


struct vlc_qt_theme_provider_t
{
    struct vlc_object_t obj;
    void* p_sys;

    //set by user while opening
    void (*paletteUpdated)(vlc_qt_theme_provider_t* obj, void* data);
    void* paletteUpdatedData;

    void (*setColorInt)(void* color, int r, int g, int b, int a);
    void (*setColorF)(void* color, double r, double g, double b, double a);

    //set by module
    void (*close)(vlc_qt_theme_provider_t* obj);
    bool (*isThemeDark)(vlc_qt_theme_provider_t* obj);
    void (*updatePalette)(vlc_qt_theme_provider_t* obj, struct vlc_qt_palette_t*);
    picture_t* (*getThemeImage)(vlc_qt_theme_provider_t* obj, vlc_qt_theme_image_type type, const vlc_qt_theme_image_setting* setting);
    bool (*supportThemeImage)(vlc_qt_theme_provider_t* obj, vlc_qt_theme_image_type type);
};

#endif // QTTHEMEPROVIDER_HPP
