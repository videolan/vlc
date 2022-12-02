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
#include <functional>

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

static void setGtkColor(vlc_qt_theme_provider_t* obj,
                        vlc_qt_theme_color_set set, vlc_qt_theme_color_section section,
                        vlc_qt_theme_color_name name, vlc_qt_theme_color_state state,
                        const GdkRGBA& gColor)
{
     obj->setColorF(obj, set, section, name, state, gColor.red, gColor.green, gColor.blue, gColor.alpha);
}

static void setGtkColorSet(vlc_qt_theme_provider_t* obj,
                           vlc_qt_theme_color_set set, vlc_qt_theme_color_section section,
                           vlc_qt_theme_color_name name,
                           std::function<GdkRGBA (std::string)> getter, std::string selector)
{
    setGtkColor(obj, set, section, name, VQTC_STATE_NORMAL, getter(selector));
    setGtkColor(obj, set, section, name, VQTC_STATE_DISABLED, getter(selector + ":disabled"));
    setGtkColor(obj, set, section, name, VQTC_STATE_HOVERED, getter(selector + ":hover"));
    setGtkColor(obj, set, section, name, VQTC_STATE_PRESSED, getter(selector + ":hover:active"));
    setGtkColor(obj, set, section, name, VQTC_STATE_FOCUSED, getter(selector + ":focus"));
}

static void setGtkColorSetBg(vlc_qt_theme_provider_t* obj,
                           vlc_qt_theme_color_set set,  vlc_qt_theme_color_name name,
                               std::string selector)
{
    setGtkColorSet(obj, set, VQTC_SECTION_BG, name, GetBgColor, selector);
}

static void setGtkColorSetFg(vlc_qt_theme_provider_t* obj,
                           vlc_qt_theme_color_set set,  vlc_qt_theme_color_name name,
                               std::string selector)
{
    setGtkColorSet(obj, set, VQTC_SECTION_FG, name, GetFgColor, selector);
}

static void setGtkColorSetFgFromBg(vlc_qt_theme_provider_t* obj,
                           vlc_qt_theme_color_set set,  vlc_qt_theme_color_name name,
                               std::string selector)
{
    setGtkColorSet(obj, set, VQTC_SECTION_FG, name, GetBgColor, selector);
}


static void setGtkColorSetBorder(vlc_qt_theme_provider_t* obj, vlc_qt_theme_color_set set, std::string selector)
{
    setGtkColorSet(obj, set, VQTC_SECTION_DECORATION, VQTC_NAME_BORDER, GetBorderColor, selector);
}

static void setGtkColorSetHighlight(vlc_qt_theme_provider_t* obj, vlc_qt_theme_color_set set, std::string selector)
{
    auto setFgBg = [obj, set, &selector](vlc_qt_theme_color_state state, std::string stateStr)
    {
        setGtkColor(obj, set, VQTC_SECTION_BG, VQTC_NAME_HIGHLIGHT, state, GetBgColor(selector + stateStr));
        setGtkColor(obj, set, VQTC_SECTION_FG, VQTC_NAME_HIGHLIGHT, state, GetFgColor(selector + stateStr + " GtkLabel#label"));
    };
    setFgBg(VQTC_STATE_NORMAL, ":selected");
    setFgBg(VQTC_STATE_DISABLED, ":selected:disabled");
    setFgBg(VQTC_STATE_HOVERED, ":selected:hover");
    setFgBg(VQTC_STATE_PRESSED, ":selected:hover:active");
    setFgBg(VQTC_STATE_FOCUSED, ":selected:focus");
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

static int updatePalette(vlc_qt_theme_provider_t* obj)
{
    bool isDark = isThemeDark(obj);

    GdkRGBA accent;
    if (isDark)
        gdk_rgba_parse(&accent, "#FF8800");
    else
        gdk_rgba_parse(&accent, "#FF610A");

    //IDK how to retreive the shadow color from GTK, using a black like in our theme is good enough
    GdkRGBA shadow;
    shadow.red = shadow.green = shadow.blue = 0.f;
    shadow.alpha = 0.22;

#define VIEW_SELECTOR "GtkListBox#list"
    {
        auto CS = VQTC_SET_VIEW;
        std::string seletor = "GtkListBox#list";
        setGtkColorSetBg(obj, CS, VQTC_NAME_PRIMARY, VIEW_SELECTOR);
        setGtkColorSetFg(obj, CS, VQTC_NAME_PRIMARY, VIEW_SELECTOR);

        setGtkColorSetBg(obj, CS, VQTC_NAME_SECONDARY, VIEW_SELECTOR); //use same color
        setGtkColorSetFg(obj, CS, VQTC_NAME_SECONDARY, VIEW_SELECTOR "GtkLabel#label.dim-label");

        setGtkColorSetBorder(obj, CS, VIEW_SELECTOR);

        setGtkColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_ACCENT, VQTC_STATE_NORMAL, accent);
        setGtkColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_VISUAL_FOCUS, VQTC_STATE_NORMAL, GetFocusColor(VIEW_SELECTOR " GtkLabel#label"));
        setGtkColor(obj, CS, VQTC_SECTION_DECORATION,  VQTC_NAME_SEPARATOR, VQTC_STATE_NORMAL, GetSeparatorColor(VIEW_SELECTOR " GtkSeparator#separator.horizontal"));
        setGtkColor(obj, CS, VQTC_SECTION_DECORATION,  VQTC_NAME_SHADOW, VQTC_STATE_NORMAL, shadow);
    }

#define POPUP_SELECTOR "GtkInfoBar#infobar"
    {
        auto CS = VQTC_SET_WINDOW;
        setGtkColorSetBg(obj, CS, VQTC_NAME_NEGATIVE, POPUP_SELECTOR ".error #revealer #box");
        setGtkColorSetFg(obj, CS, VQTC_NAME_NEGATIVE, POPUP_SELECTOR ".error #revealer #box");

        setGtkColorSetBg(obj, CS, VQTC_NAME_NEUTRAL, POPUP_SELECTOR ".warning #revealer #box");
        setGtkColorSetFg(obj, CS, VQTC_NAME_NEUTRAL, POPUP_SELECTOR ".warning #revealer #box");

        setGtkColorSetBg(obj, CS, VQTC_NAME_POSITIVE, POPUP_SELECTOR ".success #revealer #box");
        setGtkColorSetFg(obj, CS, VQTC_NAME_POSITIVE, POPUP_SELECTOR ".success #revealer #box");

    #define TITLEBAR_SELECTOR "#window.background.csd  GtkHeaderBar#headerbar.header-bar.titlebar"
        setGtkColorSetBg(obj, CS, VQTC_NAME_PRIMARY, TITLEBAR_SELECTOR);
        setGtkColorSetBg(obj, CS, VQTC_NAME_SECONDARY, TITLEBAR_SELECTOR);

        setGtkColorSetFg(obj, CS, VQTC_NAME_PRIMARY, TITLEBAR_SELECTOR);
        setGtkColorSetBorder(obj, CS, TITLEBAR_SELECTOR);
        setGtkColor(obj, CS, VQTC_SECTION_DECORATION,  VQTC_NAME_SEPARATOR, VQTC_STATE_NORMAL, GetSeparatorColor(VIEW_SELECTOR " GtkSeparator#separator.horizontal"));
    }

#define MENUBAR_SELECTOR TITLEBAR_SELECTOR " GtkMenuBar#menubar"
    {
        auto CS = VQTC_SET_MENUBAR;
        setGtkColorSetBg(obj, CS, VQTC_NAME_PRIMARY, MENUBAR_SELECTOR);
        setGtkColorSetFg(obj, CS, VQTC_NAME_PRIMARY, MENUBAR_SELECTOR " GtkLabel#menuitem");

        GdkRGBA menubarbg = GetBgColor(MENUBAR_SELECTOR);
        menubarbg.alpha = 0.0;
        setGtkColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_PRIMARY, VQTC_STATE_NORMAL, menubarbg);
    }

#define TOOLBUTTON_SELECTOR TITLEBAR_SELECTOR " GtkButton#button.toggle"
    {
        auto CS = VQTC_SET_TOOL_BUTTON;
        setGtkColorSetBg(obj, CS, VQTC_NAME_PRIMARY, TOOLBUTTON_SELECTOR);
        setGtkColorSetFg(obj, CS, VQTC_NAME_PRIMARY, TOOLBUTTON_SELECTOR);
        setGtkColorSetBorder(obj, CS, TOOLBUTTON_SELECTOR);

        setGtkColorSetBg(obj, CS, VQTC_NAME_NEGATIVE, TOOLBUTTON_SELECTOR ".error.destructive-action");
        setGtkColorSetFg(obj, CS, VQTC_NAME_NEGATIVE, TOOLBUTTON_SELECTOR ".error.destructive-action");

        setGtkColorSetBg(obj, CS, VQTC_NAME_NEUTRAL, TOOLBUTTON_SELECTOR ".warning");
        setGtkColorSetFg(obj, CS, VQTC_NAME_NEUTRAL, TOOLBUTTON_SELECTOR ".warning");

        setGtkColorSetBg(obj, CS, VQTC_NAME_POSITIVE, TOOLBUTTON_SELECTOR ".success");
        setGtkColorSetFg(obj, CS, VQTC_NAME_POSITIVE, TOOLBUTTON_SELECTOR ".success");
    }

#define TABBUTTON_SELECTOR TITLEBAR_SELECTOR " GtkBox#box.linked.stack-switcher.horizontal GtkButton#button.text-button"
    {
        auto CS = VQTC_SET_TAB_BUTTON;
        setGtkColorSetBg(obj, CS, VQTC_NAME_PRIMARY, TABBUTTON_SELECTOR);
        setGtkColorSetFg(obj, CS, VQTC_NAME_SECONDARY, TABBUTTON_SELECTOR ":checked");
        setGtkColorSetBorder(obj, CS, TABBUTTON_SELECTOR);
        //keep the background transparent in the tabbar in normal state, they look weird otherwise
        GdkRGBA tabButtonNormalBg = GetBgColor(TITLEBAR_SELECTOR);
        tabButtonNormalBg.alpha = 1.f;
        setGtkColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_PRIMARY, VQTC_STATE_NORMAL, tabButtonNormalBg);
    }

#define BUTTON_STANDARD_SELECTOR VIEW_SELECTOR " GtkButton#button.flat"
    {
        auto CS = VQTC_SET_BUTTON_STANDARD;
        setGtkColorSetBg(obj, CS, VQTC_NAME_PRIMARY, BUTTON_STANDARD_SELECTOR);
        setGtkColorSetFg(obj, CS, VQTC_NAME_PRIMARY, BUTTON_STANDARD_SELECTOR);
        setGtkColorSetBorder(obj, CS, BUTTON_STANDARD_SELECTOR);
    }

#define BUTTON_ACCENT_SELECTOR VIEW_SELECTOR " GtkButton#button.suggested-action"
    {
        auto CS = VQTC_SET_BUTTON_ACCENT;
        setGtkColorSetBg(obj, CS, VQTC_NAME_PRIMARY, BUTTON_ACCENT_SELECTOR);
        setGtkColorSetFg(obj, CS, VQTC_NAME_PRIMARY, BUTTON_ACCENT_SELECTOR);
        setGtkColorSetBorder(obj, CS, BUTTON_ACCENT_SELECTOR);
    }

#define TEXTFIELD_SELECTOR TITLEBAR_SELECTOR " GtkEntry#entry"
    {
        auto CS = VQTC_SET_TEXTFIELD;
        setGtkColorSetBg(obj, CS, VQTC_NAME_PRIMARY, TEXTFIELD_SELECTOR);
        setGtkColorSetFg(obj, CS, VQTC_NAME_PRIMARY, TEXTFIELD_SELECTOR);
        setGtkColorSetBorder(obj, CS, TEXTFIELD_SELECTOR);

        std::string textfield_hightlight_selector = std::string{TEXTFIELD_SELECTOR}
                + (GtkCheckVersion(3, 20) ? " GtkLabel#label #selection" : " GtkLabel#label:selected");
        setGtkColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_HIGHLIGHT, VQTC_STATE_NORMAL,
                    GetSelectionBgColor(textfield_hightlight_selector));
        setGtkColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_HIGHLIGHT, VQTC_STATE_NORMAL,
                    GetFgColor(textfield_hightlight_selector));

        setGtkColorSetBg(obj, CS, VQTC_NAME_NEGATIVE, TEXTFIELD_SELECTOR ".error");
        setGtkColorSetFg(obj, CS, VQTC_NAME_NEGATIVE, TEXTFIELD_SELECTOR ".error");

        setGtkColorSetBg(obj, CS, VQTC_NAME_NEUTRAL, TEXTFIELD_SELECTOR ".warning");
        setGtkColorSetFg(obj, CS, VQTC_NAME_NEUTRAL, TEXTFIELD_SELECTOR ".warning");

        setGtkColorSetBg(obj, CS, VQTC_NAME_POSITIVE, TEXTFIELD_SELECTOR ".success");
        setGtkColorSetFg(obj, CS, VQTC_NAME_POSITIVE, TEXTFIELD_SELECTOR ".success");
    }

    //use the .osd class as it will remove the transparency effect on the slider
#define SLIDER_SELECTOR "GtkProgressBar#progressbar.osd"
    {
        auto CS = VQTC_SET_SLIDER;
        setGtkColorSetBg(obj, CS, VQTC_NAME_PRIMARY, SLIDER_SELECTOR);
#define SLIDER_PROGRESS_SELECTOR SLIDER_SELECTOR " #trough #progress"
        setGtkColorSetFgFromBg(obj, CS, VQTC_NAME_PRIMARY, SLIDER_PROGRESS_SELECTOR);

        setGtkColorSetBorder(obj, CS, SLIDER_SELECTOR);
        setGtkColorSetFg(obj, CS, VQTC_NAME_NEUTRAL, SLIDER_PROGRESS_SELECTOR ".pulse ");

        //use the destructive action button background color which should always be red
        setGtkColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_NEGATIVE, VQTC_STATE_NORMAL, GetBgColor(TOOLBUTTON_SELECTOR ".error.destructive-action"));
    }

    //we may ask for accent variant but as far as I can tell, themes won't provide the variant
#define BADGE_SELECTOR VIEW_SELECTOR " GtkFrame#frame.osd"
    {
        auto CS = VQTC_SET_BADGE;
        setGtkColorSetBg(obj, CS, VQTC_NAME_PRIMARY, BADGE_SELECTOR);
        setGtkColorSetFg(obj, CS, VQTC_NAME_PRIMARY, BADGE_SELECTOR);
        setGtkColorSetBorder(obj, CS, BADGE_SELECTOR);
    }

    {
        auto CS = VQTC_SET_TOOLTIP;
        std::string tooltipSelector = GtkCheckVersion(3, 20) ? "#tooltip.background"
                : "GtkWindow#window.background.tooltip";

        setGtkColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_PRIMARY, VQTC_STATE_NORMAL, GetBgColor(tooltipSelector));
        setGtkColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_BORDER, VQTC_STATE_NORMAL, GetBorderColor(tooltipSelector));
        tooltipSelector += " GtkLabel#label";
        setGtkColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_PRIMARY, VQTC_STATE_NORMAL,  GetFgColor(tooltipSelector));
    }

#define ITEM_SELECTOR VIEW_SELECTOR " GtkListBoxRow#row.activatable"
    {
        auto CS = VQTC_SET_ITEM;
        setGtkColorSetBg(obj, CS, VQTC_NAME_PRIMARY, ITEM_SELECTOR);
        setGtkColorSetFg(obj, CS, VQTC_NAME_PRIMARY, ITEM_SELECTOR);
        setGtkColorSetBorder(obj, CS, ITEM_SELECTOR);
        setGtkColorSetHighlight(obj, CS, ITEM_SELECTOR);
    }

#define COMBOBOX_SELECTOR VIEW_SELECTOR " GtkComboBoxText#combobox #box.linked #entry.combo"
    {
        auto CS = VQTC_SET_COMBOBOX;
        setGtkColorSetBg(obj, CS, VQTC_NAME_PRIMARY, COMBOBOX_SELECTOR);
        setGtkColorSetFg(obj, CS, VQTC_NAME_PRIMARY, COMBOBOX_SELECTOR);
        setGtkColorSetBorder(obj, CS, COMBOBOX_SELECTOR);
    }

#define SPINBOX_SELECTOR VIEW_SELECTOR " GtkSpinButton#spinbutton.vertical"
    {
        auto CS = VQTC_SET_COMBOBOX;
        setGtkColorSetBg(obj, CS, VQTC_NAME_PRIMARY, SPINBOX_SELECTOR);
        setGtkColorSetFg(obj, CS, VQTC_NAME_PRIMARY, SPINBOX_SELECTOR);
        setGtkColorSetBorder(obj, CS, SPINBOX_SELECTOR);
    }

#define SWITCH_SELECTOR VIEW_SELECTOR " GtkSwitch#switch"
    {
        auto CS = VQTC_SET_SWITCH;
        setGtkColorSetBg(obj, CS, VQTC_NAME_PRIMARY, SWITCH_SELECTOR);
        setGtkColorSetFgFromBg(obj, CS, VQTC_NAME_PRIMARY, SWITCH_SELECTOR " #slider");

        setGtkColorSetBg(obj, CS, VQTC_NAME_SECONDARY, SWITCH_SELECTOR ":checked");
        setGtkColorSetFgFromBg(obj, CS, VQTC_NAME_SECONDARY, SWITCH_SELECTOR ":checked #slider");
        setGtkColorSetBorder(obj, CS, SWITCH_SELECTOR);
    }

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
        g_type_class_unref(g_type_class_ref(gtk_progress_bar_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_overlay_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_list_box_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_list_box_row_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_spin_button_get_type()));
        g_type_class_unref(g_type_class_ref(gtk_switch_get_type()));

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
