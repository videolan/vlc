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

enum vlc_qt_theme_color_state {
    VQTC_STATE_NORMAL = 0,
    VQTC_STATE_DISABLED,
    VQTC_STATE_PRESSED,
    VQTC_STATE_HOVERED,
    VQTC_STATE_FOCUSED,
    VQTC_STATE_COUNT
};


enum vlc_qt_theme_color_section {
    VQTC_SECTION_FG,
    VQTC_SECTION_BG,
    VQTC_SECTION_DECORATION,
    VQTC_SECTION_COUNT
};
enum vlc_qt_theme_color_set {
    VQTC_SET_VIEW = 0,
    VQTC_SET_WINDOW,
    VQTC_SET_ITEM,
    VQTC_SET_BADGE,
    VQTC_SET_BUTTON_STANDARD,
    VQTC_SET_BUTTON_ACCENT,
    VQTC_SET_TAB_BUTTON,
    VQTC_SET_TOOL_BUTTON,
    VQTC_SET_SWITCH,
    VQTC_SET_MENUBAR,
    VQTC_SET_TOOLTIP,
    VQTC_SET_SLIDER,
    VQTC_SET_COMBOBOX,
    VQTC_SET_SPINBOX,
    VQTC_SET_TEXTFIELD,
    VQTC_SET_COUNT
};

enum vlc_qt_theme_color_name {
    VQTC_NAME_PRIMARY = 0,
    VQTC_NAME_SECONDARY,
    VQTC_NAME_HIGHLIGHT,
    VQTC_NAME_LINK,
    VQTC_NAME_POSITIVE,
    VQTC_NAME_NEUTRAL,
    VQTC_NAME_NEGATIVE,
    VQTC_NAME_VISUAL_FOCUS,
    VQTC_NAME_BORDER,
    VQTC_NAME_ACCENT,
    VQTC_NAME_SHADOW,
    VQTC_NAME_SEPARATOR,
    VQTC_NAME_COUNT
};

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

struct vlc_qt_theme_metrics {
    union {
        struct {
            int interNavButtonSpacing;

            int csdFrameMarginLeft;
            int csdFrameMarginRight;
            int csdFrameMarginTop;
            int csdFrameMarginBottom;
        } csd;
    } u;
};


struct vlc_qt_theme_provider_t
{
    struct vlc_object_t obj;
    void* p_sys;

    //set by user while opening
    void (*paletteUpdated)(vlc_qt_theme_provider_t* obj, void* data);
    void* paletteUpdatedData;

    void (*metricsUpdated)(vlc_qt_theme_provider_t* obj, vlc_qt_theme_image_type type, void* data);
    void* metricsUpdatedData;


    void (*setColorInt)(
            vlc_qt_theme_provider_t* obj,
            vlc_qt_theme_color_set set, vlc_qt_theme_color_section section,
            vlc_qt_theme_color_name name, vlc_qt_theme_color_state state,
            int r, int g, int b, int a);
    void (*setColorF)(
            vlc_qt_theme_provider_t* obj,
            vlc_qt_theme_color_set set, vlc_qt_theme_color_section section,
            vlc_qt_theme_color_name name, vlc_qt_theme_color_state state,
            double r, double g, double b, double a);
    void* setColorData;

    //set by module
    void (*close)(vlc_qt_theme_provider_t* obj);
    bool (*isThemeDark)(vlc_qt_theme_provider_t* obj);
    /**
     * return VLC_SUCCESS if palette have been updated, VLC_EGENERIC otherwise
     */
    int (*updatePalette)(vlc_qt_theme_provider_t* obj);
    picture_t* (*getThemeImage)(vlc_qt_theme_provider_t* obj, vlc_qt_theme_image_type type, const vlc_qt_theme_image_setting* setting);
    bool (*getThemeMetrics)(vlc_qt_theme_provider_t* obj, vlc_qt_theme_image_type type, vlc_qt_theme_metrics* setting);
    bool (*supportThemeImage)(vlc_qt_theme_provider_t* obj, vlc_qt_theme_image_type type);
};

#endif // QTTHEMEPROVIDER_HPP
