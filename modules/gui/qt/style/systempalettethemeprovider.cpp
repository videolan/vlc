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
#include "defaultthemeproviders.hpp"

#include "qt.hpp"

#include <QGuiApplication>
#include <QPalette>

namespace {

QColor blendColors(QColor c1, QColor c2, float blend = 0.5)
{
    return QColor(c2.redF()   + (c1.redF()   - c2.redF())   * blend,
                  c2.greenF() + (c1.greenF() - c2.greenF()) * blend,
                  c2.blueF()  + (c1.blueF()  - c2.blueF())  * blend,
                  c2.alphaF() + (c1.alphaF() - c2.alphaF()) * blend);
}

}

class SystemePaletteObserver : public QObject
{
public:
    SystemePaletteObserver(vlc_qt_theme_provider_t* obj)
        : m_obj(obj)
    {
        vlc_assert(obj);
        connect(qApp, &QGuiApplication::paletteChanged, this, &SystemePaletteObserver::paletteChanged);
    }

public slots:
    void paletteChanged()
    {
        if (m_obj->paletteUpdated)
            m_obj->paletteUpdated(m_obj, m_obj->paletteUpdatedData);
    }

private:
    vlc_qt_theme_provider_t* m_obj = nullptr;
};

static bool isThemeDark(vlc_qt_theme_provider_t*)
{
    QPalette palette = qApp->palette();
    QColor colBg = palette.color(QPalette::Normal, QPalette::Base);
    QColor colText = palette.color(QPalette::Normal, QPalette::Text);

    return colBg.lightness() < colText.lightness();
}

static void setColor(vlc_qt_theme_provider_t* obj, void* ptr, const QColor c)
{
    //temporary disabled
    //obj->setColorInt(ptr, c.red(), c.green(), c.blue(), c.alpha());
}

static int updatePalette(vlc_qt_theme_provider_t* obj)
{
    return VLC_EGENERIC;
}

static void Close(vlc_qt_theme_provider_t* obj)
{
    auto sys = static_cast<SystemePaletteObserver*>(obj->p_sys);
    delete sys;
}

//module definition is made in qt.cpp as we are statically linked to on windows
//we don't want a bloated dll just for this feature
int SystemPaletteThemeProviderOpen(vlc_object_t* p_this)
{
    vlc_qt_theme_provider_t* obj = (vlc_qt_theme_provider_t*)p_this;

    auto observer = new (std::nothrow) SystemePaletteObserver(obj);
    if (!observer)
        return VLC_EGENERIC;

    obj->p_sys = observer;
    obj->close = Close;
    obj->isThemeDark = isThemeDark;
    obj->updatePalette = updatePalette;
    return VLC_SUCCESS;
}
