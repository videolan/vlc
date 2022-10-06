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
    obj->setColorInt(ptr, c.red(), c.green(), c.blue(), c.alpha());
}

static void updatePalette(vlc_qt_theme_provider_t* obj, struct vlc_qt_palette_t* p)
{
    QPalette palette = qApp->palette();

    QColor bg = palette.color(QPalette::Normal, QPalette::Base);
    QColor text = palette.color(QPalette::Normal, QPalette::Text);

    //fix with colors provided by the system
    setColor(obj, p->bg, bg);
    setColor(obj, p->bgInactive, palette.color(QPalette::Inactive, QPalette::Base));
    setColor(obj, p->bgAlt, palette.color(QPalette::Normal, QPalette::AlternateBase));
    setColor(obj, p->bgAltInactive, palette.color(QPalette::Inactive, QPalette::AlternateBase));

    setColor(obj, p->text, palette.color(QPalette::Normal, QPalette::Text));
    setColor(obj, p->textDisabled, palette.color(QPalette::Disabled, QPalette::Text));
    setColor(obj, p->textInactive, palette.color(QPalette::Inactive, QPalette::Text));

    setColor(obj, p->bgHover, palette.color(QPalette::Normal, QPalette::Highlight));
    setColor(obj, p->bgHoverInactive, palette.color(QPalette::Inactive, QPalette::Highlight));
    setColor(obj, p->bgHoverText, palette.color(QPalette::Normal, QPalette::HighlightedText));
    setColor(obj, p->bgHoverTextInactive, palette.color(QPalette::Inactive, QPalette::HighlightedText));

    QColor button = palette.color(QPalette::Normal, QPalette::Button);
    QColor buttonText = palette.color(QPalette::Normal, QPalette::ButtonText);
    setColor(obj, p->button, button);
    setColor(obj, p->buttonText, buttonText);
    setColor(obj, p->buttonBorder, blendColors(button, buttonText, 0.8));

    setColor(obj, p->topBanner, palette.color(QPalette::Normal, QPalette::Window));
    setColor(obj, p->lowerBanner, palette.color(QPalette::Normal, QPalette::AlternateBase));

    setColor(obj, p->separator, blendColors(bg, text, .95));
    setColor(obj, p->playerControlBarFg, palette.color(QPalette::Normal, QPalette::Text));
    setColor(obj, p->expandDelegate, bg);
    setColor(obj, p->tooltipColor, palette.color(QPalette::Normal, QPalette::ToolTipBase));
    setColor(obj, p->tooltipTextColor, palette.color(QPalette::Normal, QPalette::ToolTipText));

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
