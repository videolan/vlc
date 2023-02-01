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
        return false;
    }

private:
    vlc_qt_theme_provider_t* m_obj = nullptr;

    QSettings m_settings;
};

static bool isThemeDark(vlc_qt_theme_provider_t* obj)
{
    auto sys = static_cast<WinColorSchemeObserver*>(obj->p_sys);
    return sys->isThemeDark();
}

static int updatePalette(vlc_qt_theme_provider_t* obj, struct vlc_qt_palette_t*)
{
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
