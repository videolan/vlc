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
#include "qtthemeprovider.hpp"

#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QSettings>

#include <windows.h>

namespace
{
    const char *WIN_THEME_SETTING_PATH = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    const char *WIN_THEME_SETTING_LIGHT_THEME_KEY = "AppsUseLightTheme";
}

class WinColorSchemeObserver : public QAbstractNativeEventFilter
{
public:
    WinColorSchemeObserver(vlc_qt_theme_provider_t* obj)
        : m_obj(obj)
        , m_settings(QLatin1String {WIN_THEME_SETTING_PATH}, QSettings::NativeFormat)
    {
        vlc_assert(obj);
        qApp->installNativeEventFilter(this);
    }

    ~WinColorSchemeObserver()
    {
        qApp->removeNativeEventFilter(this);
    }

    bool isThemeDark() const
    {
        return !m_settings.value(WIN_THEME_SETTING_LIGHT_THEME_KEY).toBool();
    }

    bool nativeEventFilter(const QByteArray &, void *message, long *)
    {
        MSG* msg = static_cast<MSG*>( message );
        if ( msg->message == WM_SETTINGCHANGE
             && !lstrcmp( LPCTSTR( msg->lParam ), L"ImmersiveColorSet" ) )
        {
            if (m_obj->paletteUpdated)
                m_obj->paletteUpdated(m_obj, m_obj->paletteUpdatedData);
        }
        else if (msg->message == WM_SYSCOLORCHANGE)
        {
            if (m_obj->paletteUpdated)
                m_obj->paletteUpdated(m_obj, m_obj->paletteUpdatedData);
        }
        return false;
    }

private:
    vlc_qt_theme_provider_t* m_obj = nullptr;

    QSettings m_settings;
};

static void setQtColor(vlc_qt_theme_provider_t* obj,
                        vlc_qt_theme_color_set set, vlc_qt_theme_color_section section,
                        vlc_qt_theme_color_name name, vlc_qt_theme_color_state state,
                        int color)
{
    const DWORD c = GetSysColor(color);
    obj->setColorInt(obj, set, section, name, state, GetRValue(c), GetGValue(c), GetBValue(c), 255);
}


static void setQtColorFg(vlc_qt_theme_provider_t* obj,
                        vlc_qt_theme_color_set set, vlc_qt_theme_color_name name, vlc_qt_theme_color_state state, int role)
{
    setQtColor(obj, set, VQTC_SECTION_FG, name, state, role);
}

static void setQtColorBg(vlc_qt_theme_provider_t* obj,
                        vlc_qt_theme_color_set set, vlc_qt_theme_color_name name, vlc_qt_theme_color_state state, int role)
{
    setQtColor(obj, set, VQTC_SECTION_BG, name, state, role);
}


static void setQtColorButton(vlc_qt_theme_provider_t* obj,
                        vlc_qt_theme_color_set CS)
{
    setQtColorBg(obj, CS, VQTC_NAME_PRIMARY, VQTC_STATE_NORMAL, COLOR_3DFACE);
    setQtColorFg(obj, CS, VQTC_NAME_PRIMARY, VQTC_STATE_NORMAL, COLOR_BTNTEXT);
    setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_BORDER, VQTC_STATE_NORMAL, COLOR_BTNTEXT);

    setQtColorBg(obj, CS, VQTC_NAME_PRIMARY, VQTC_STATE_HOVERED, COLOR_HIGHLIGHT);
    setQtColorFg(obj, CS, VQTC_NAME_PRIMARY, VQTC_STATE_HOVERED, COLOR_HIGHLIGHTTEXT);
    setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_BORDER, VQTC_STATE_HOVERED, COLOR_HIGHLIGHTTEXT);

    setQtColorBg(obj, CS, VQTC_NAME_PRIMARY, VQTC_STATE_FOCUSED, COLOR_HIGHLIGHT);
    setQtColorFg(obj, CS, VQTC_NAME_PRIMARY, VQTC_STATE_FOCUSED, COLOR_HIGHLIGHTTEXT);
    setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_BORDER, VQTC_STATE_FOCUSED, COLOR_HIGHLIGHTTEXT);


}

static bool isThemeDark(vlc_qt_theme_provider_t* obj)
{
    auto sys = static_cast<WinColorSchemeObserver*>(obj->p_sys);
    return sys->isThemeDark();
}

static int updatePalette(vlc_qt_theme_provider_t* obj)
{
    HIGHCONTRAST constrastInfo;
    constrastInfo.cbSize = sizeof(HIGHCONTRAST);

    bool ret = SystemParametersInfoA(SPI_GETHIGHCONTRAST, constrastInfo.cbSize, &constrastInfo, 0);

    if (ret && ((constrastInfo.dwFlags & HCF_HIGHCONTRASTON) == HCF_HIGHCONTRASTON))
    {
        //we must comply with the high contrast palette, only few color can be used
        //see https://learn.microsoft.com/en-us/windows/apps/design/accessibility/high-contrast-themes#contrast-colors

        //most things will fallback to the default view colors, as we mainly have a foreground and a backgroud color

        //View
        {
            vlc_qt_theme_color_set CS = VQTC_SET_VIEW;
            setQtColorBg(obj, CS, VQTC_NAME_PRIMARY, VQTC_STATE_NORMAL, COLOR_WINDOW);
            setQtColorFg(obj, CS, VQTC_NAME_PRIMARY, VQTC_STATE_NORMAL, COLOR_WINDOWTEXT);
            setQtColorFg(obj, CS, VQTC_NAME_PRIMARY, VQTC_STATE_DISABLED, COLOR_GRAYTEXT);

            setQtColorBg(obj, CS, VQTC_NAME_SECONDARY, VQTC_STATE_NORMAL, COLOR_WINDOW);
            setQtColorFg(obj, CS, VQTC_NAME_SECONDARY, VQTC_STATE_NORMAL, COLOR_WINDOWTEXT);
            setQtColorFg(obj, CS, VQTC_NAME_SECONDARY, VQTC_STATE_DISABLED, COLOR_GRAYTEXT);

            setQtColorFg(obj, CS, VQTC_NAME_LINK, VQTC_STATE_NORMAL, COLOR_HOTLIGHT);

            setQtColorBg(obj, CS, VQTC_NAME_HIGHLIGHT, VQTC_STATE_NORMAL, COLOR_HIGHLIGHT);
            setQtColorFg(obj, CS, VQTC_NAME_HIGHLIGHT, VQTC_STATE_NORMAL, COLOR_HIGHLIGHTTEXT);

            //we can't really have color here
            setQtColorBg(obj, CS, VQTC_NAME_NEGATIVE, VQTC_STATE_NORMAL, COLOR_HIGHLIGHT);
            setQtColorBg(obj, CS, VQTC_NAME_NEUTRAL, VQTC_STATE_NORMAL, COLOR_HIGHLIGHT);
            setQtColorBg(obj, CS, VQTC_NAME_POSITIVE, VQTC_STATE_NORMAL, COLOR_HIGHLIGHT);

            setQtColorFg(obj, CS, VQTC_NAME_NEGATIVE, VQTC_STATE_NORMAL, COLOR_HIGHLIGHTTEXT);
            setQtColorFg(obj, CS, VQTC_NAME_NEUTRAL, VQTC_STATE_NORMAL, COLOR_HIGHLIGHTTEXT);
            setQtColorFg(obj, CS, VQTC_NAME_POSITIVE, VQTC_STATE_NORMAL, COLOR_HIGHLIGHTTEXT);

            //branding color,
            //NOTE: can we keep our branding orange accent here?
            //obj->setColorInt(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_ACCENT, VQTC_STATE_NORMAL, 0xFF, 0x80, 0x00, 0xFF);
            setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_ACCENT, VQTC_STATE_NORMAL, COLOR_HIGHLIGHT);

            setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_VISUAL_FOCUS, VQTC_STATE_NORMAL, COLOR_WINDOWTEXT);
            setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_SEPARATOR, VQTC_STATE_NORMAL, COLOR_BTNTEXT);
            setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_SHADOW, VQTC_STATE_NORMAL, COLOR_BTNTEXT);

            setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_BORDER, VQTC_STATE_NORMAL, COLOR_BTNTEXT);
            setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_BORDER, VQTC_STATE_FOCUSED, COLOR_HIGHLIGHT);
            setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_BORDER, VQTC_STATE_HOVERED, COLOR_HIGHLIGHT);
        }

        //set colors on all buttons
        setQtColorButton(obj, VQTC_SET_TOOL_BUTTON);
        setQtColorButton(obj, VQTC_SET_TAB_BUTTON);
        setQtColorButton(obj, VQTC_SET_BUTTON_ACCENT);
        setQtColorButton(obj, VQTC_SET_BUTTON_STANDARD);
        setQtColorButton(obj, VQTC_SET_SWITCH);

        return VLC_SUCCESS;
    }

    //use VLC palette, choose the one maching the dark/light settings
    return VLC_EGENERIC;
}

static void Close(vlc_qt_theme_provider_t* obj)
{
    auto sys = static_cast<WinColorSchemeObserver*>(obj->p_sys);
    delete sys;
}

//module definition is made in qt.cpp as we are statically linked to on windows
//we don't want a bloated dll just for this feature
int WindowsThemeProviderOpen(vlc_object_t* p_this)
{
    vlc_qt_theme_provider_t* obj = (vlc_qt_theme_provider_t*)p_this;

    QSettings settings(QLatin1String {WIN_THEME_SETTING_PATH}, QSettings::NativeFormat);
    if (! settings.contains(WIN_THEME_SETTING_LIGHT_THEME_KEY))
    {
        //can't detect theme change, module is useless
        return VLC_EGENERIC;
    }
    auto observer = new (std::nothrow) WinColorSchemeObserver(obj);
    if (!observer)
        return VLC_EGENERIC;

    obj->p_sys = observer;
    obj->close = Close;
    obj->isThemeDark = isThemeDark;
    obj->updatePalette = updatePalette;
    return VLC_SUCCESS;
}
