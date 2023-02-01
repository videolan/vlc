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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include "../qtthemeprovider.hpp"
#include <mutex>

#include "nav_button_provider_gtk.h"
#include "gtk_util.h"

using namespace gtk;


class GtkPrivateThemeProvider {
public:
    bool windowMaximized = false;
    bool windowActive = false;
    int bannerHeight = -1;
    NavButtonProviderGtk navButtons;

    //Metrics
    int interNavButtonSpacing = 0;

    int csdFrameMarginLeft = 0;
    int csdFrameMarginRight = 0;
    int csdFrameMarginTop = 0;
    int csdFrameMarginBottom = 0;
};

static bool isThemeDark( vlc_qt_theme_provider_t*)
{
    GdkRGBA colBg = GetBgColor("");
    GdkRGBA colText = GetFgColor("GtkLabel#label");
    return GdkRBGALightness(colBg) < GdkRBGALightness(colText);
}

static void setGtkColor(vlc_qt_theme_provider_t* obj, void* qColor, const GdkRGBA& gColor)
{
     obj->setColorF(qColor, gColor.red, gColor.green, gColor.blue, gColor.alpha);
}

template<typename Getter>
static void setGtkColorSet(vlc_qt_theme_provider_t* obj, void* base, void* disabled, void* inactive, Getter& getter, std::string selector)
{
    if (base)
        setGtkColor(obj, base, getter(selector));
    if (disabled)
        setGtkColor(obj, disabled, getter(selector + ":disabled"));
    if (inactive)
        setGtkColor(obj, inactive, getter(selector + ":backdrop"));
}

picture_t* getThemeImage(vlc_qt_theme_provider_t* obj, vlc_qt_theme_image_type type, const vlc_qt_theme_image_setting* settings)
{
    auto sys = static_cast<GtkPrivateThemeProvider*>(obj->p_sys);
    if (type == VLC_QT_THEME_IMAGE_TYPE_CSD_BUTTON)
    {
        SetDeviceScaleFactor(settings->windowScaleFactor);

        bool windowMaximized = settings->u.csdButton.maximized;
        bool windowActive = settings->u.csdButton.active;
        int bannerHeight = settings->u.csdButton.bannerHeight;
        vlc_qt_theme_csd_button_state state = settings->u.csdButton.state;
        vlc_qt_theme_csd_button_type buttonType = settings->u.csdButton.buttonType;

        if (buttonType == VLC_QT_THEME_BUTTON_MAXIMIZE && windowMaximized)
            buttonType = VLC_QT_THEME_BUTTON_RESTORE;
        else if (buttonType == VLC_QT_THEME_BUTTON_RESTORE && !windowMaximized)
             buttonType = VLC_QT_THEME_BUTTON_MAXIMIZE;

        if (windowActive != sys->windowActive
            || windowMaximized != sys->windowMaximized
            || bannerHeight != sys->bannerHeight)
        {
            sys->navButtons.RedrawImages(bannerHeight, windowMaximized, windowActive);
            sys->bannerHeight = bannerHeight;
            sys->windowMaximized = windowMaximized;
            sys->windowActive = windowActive;

            sys->interNavButtonSpacing = sys->navButtons.GetInterNavButtonSpacing();

            MyInset frameMargin = sys->navButtons.GetTopAreaSpacing();
            sys->csdFrameMarginLeft = frameMargin.left();
            sys->csdFrameMarginRight = frameMargin.right();
            sys->csdFrameMarginTop = frameMargin.top();
            sys->csdFrameMarginBottom = frameMargin.bottom();
            if (obj->metricsUpdated)
                obj->metricsUpdated(obj, VLC_QT_THEME_IMAGE_TYPE_CSD_BUTTON, obj->metricsUpdatedData);
        }

        picture_t* pic = sys->navButtons.GetImage(buttonType, state).get();
        if (pic)
            picture_Hold(pic);
        return pic;
    }
    return nullptr;
}

static bool getThemeMetrics(vlc_qt_theme_provider_t* obj, vlc_qt_theme_image_type type, struct vlc_qt_theme_metrics* metrics)
{
    auto sys = static_cast<GtkPrivateThemeProvider*>(obj->p_sys);
    if (type != VLC_QT_THEME_IMAGE_TYPE_CSD_BUTTON)
        return false;
    metrics->u.csd.interNavButtonSpacing = sys->interNavButtonSpacing;

    metrics->u.csd.csdFrameMarginLeft = sys->csdFrameMarginLeft;
    metrics->u.csd.csdFrameMarginRight = sys->csdFrameMarginRight;
    metrics->u.csd.csdFrameMarginTop = sys->csdFrameMarginTop;
    metrics->u.csd.csdFrameMarginBottom = sys->csdFrameMarginBottom;
    return true;
}

bool supportThemeImage(vlc_qt_theme_provider_t*, vlc_qt_theme_image_type type)
{
    if (type == VLC_QT_THEME_IMAGE_TYPE_CSD_BUTTON)
        return true;
    return false;
}

static int updatePalette(vlc_qt_theme_provider_t* obj, struct vlc_qt_palette_t* p)
{
    GdkRGBA bg = GetBgColor("GtkTreeView#treeview.view GtkTreeView#treeview.view.cell");
    GdkRGBA text = GetFgColor("GtkLabel#label");

    setGtkColorSet(obj, p->bg, nullptr, p->bgInactive, GetBgColor,  "GtkTreeView#treeview.view GtkTreeView#treeview.view.cell");
    setGtkColorSet(obj, p->bgAlt, nullptr, p->bgAltInactive, GetBgColor, "");
    setGtkColorSet(obj, p->text, p->textDisabled, p->textInactive, GetFgColor, "GtkLabel#label");

    setGtkColorSet(obj, p->bgHover, nullptr, p->bgHoverInactive, GetSelectionBgColor, "GtkLabel#label #selection");
    setGtkColorSet(obj, p->bgHoverText, nullptr, p->bgHoverTextInactive, GetFgColor, "GtkLabel#setGtkColorSet");

    setGtkColorSet(obj, p->button, nullptr, nullptr, GetBgColor, "GtkButton#button");
    setGtkColorSet(obj, p->buttonText, nullptr, nullptr, GetFgColor, "GtkButton#button GtkLabel#label");
    setGtkColorSet(obj, p->buttonBorder, nullptr, nullptr, GetBorderColor, "GtkButton#button");

    setGtkColor(obj, p->topBanner, GetBgColor("#headerbar.header-bar.titlebar"));
    setGtkColor(obj, p->lowerBanner, GetBgColor(""));
    setGtkColor(obj, p->expandDelegate, bg);

    setGtkColor(obj, p->separator, GdkRBGABlend(bg, text, .95));
    setGtkColor(obj, p->playerControlBarFg, text);

    const auto tooltip_context = AppendCssNodeToStyleContext({}, "#tooltip.background");

    setGtkColor(obj, p->tooltipColor, GetBgColorFromStyleContext(tooltip_context));
    setGtkColor(obj, p->tooltipTextColor, GtkStyleContextGetColor(AppendCssNodeToStyleContext(tooltip_context, "GtkLabel#label")));

    return VLC_SUCCESS;
}

static void Close(vlc_qt_theme_provider_t* obj)
{
    auto sys = static_cast<GtkPrivateThemeProvider*>(obj->p_sys);
    delete sys;
}

static int Open(vlc_object_t* p_this)
{
    vlc_qt_theme_provider_t* obj = (vlc_qt_theme_provider_t*)p_this;


    std::once_flag flag;
    std::call_once(flag, [](){
        int argc = 0;
        char** argv = nullptr;
        gtk_init(&argc, &argv);
        gdk_init(&argc, &argv);

        //register types needed by g_type_from_name
        g_type_class_unref(g_type_class_ref(gtk_button_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_entry_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_frame_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_header_bar_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_image_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_info_bar_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_label_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_menu_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_menu_bar_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_menu_item_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_range_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_scrollbar_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_scrolled_window_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_separator_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_spinner_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_text_view_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_toggle_button_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_tree_view_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_window_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_combo_box_text_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_cell_view_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_scale_get_type()));
    });

    auto sys = new (std::nothrow) GtkPrivateThemeProvider();
    if (!sys)
        return VLC_EGENERIC;

    obj->p_sys = sys;
    obj->close = Close;
    obj->isThemeDark = isThemeDark;
    obj->updatePalette = updatePalette;
    obj->supportThemeImage  = supportThemeImage;
    obj->getThemeImage = getThemeImage;
    obj->getThemeMetrics = getThemeMetrics;
    return VLC_SUCCESS;
}

vlc_module_begin()
    add_shortcut("qt-themeprovider-gtk")
    set_description( "Qt GTK system theme" )
    set_capability("qt theme provider", 0)
    set_callback(Open)
vlc_module_end()
