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
    QColor c;
    c.setRgbF(c2.redF()   + (c1.redF()   - c2.redF())   * blend,
              c2.greenF() + (c1.greenF() - c2.greenF()) * blend,
              c2.blueF()  + (c1.blueF()  - c2.blueF())  * blend,
              c2.alphaF() + (c1.alphaF() - c2.alphaF()) * blend);
    return c;
}

}

class SystemePaletteObserver : public QObject
{
public:
    SystemePaletteObserver(vlc_qt_theme_provider_t* obj)
        : m_obj(obj)
    {
        vlc_assert(obj);
        updatePalette();
    }

    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::ApplicationPaletteChange)
        {
            updatePalette();
            if (m_obj->paletteUpdated)
                m_obj->paletteUpdated(m_obj, m_obj->paletteUpdatedData);
        }

        return QObject::event(event);
    }

public:
    QPalette m_palette;
    bool m_isDark;

private:
    void updatePalette()
    {
        m_palette = qApp->palette();
        QColor colBg = m_palette.color(QPalette::Normal, QPalette::Base);
        QColor colText = m_palette.color(QPalette::Normal, QPalette::Text);
        m_isDark = colBg.lightness() < colText.lightness();
    }

    vlc_qt_theme_provider_t* m_obj = nullptr;
};

static bool isThemeDark(vlc_qt_theme_provider_t* obj)
{
    auto sys = static_cast<SystemePaletteObserver*>(obj->p_sys);
    return sys->m_isDark;
}
static void setQtColor(vlc_qt_theme_provider_t* obj,
                        vlc_qt_theme_color_set set, vlc_qt_theme_color_section section,
                        vlc_qt_theme_color_name name, vlc_qt_theme_color_state state,
                        const QColor& c)
{
    obj->setColorInt(obj, set, section, name, state, c.red(), c.green(), c.blue(), c.alpha());
}

static void setQtColorSet(vlc_qt_theme_provider_t* obj, const QPalette& palette,
                        vlc_qt_theme_color_set set, vlc_qt_theme_color_section section,
                        vlc_qt_theme_color_name name, QPalette::ColorRole role)
{
    auto sys = static_cast<SystemePaletteObserver*>(obj->p_sys);
    QColor normalColor = palette.color(QPalette::Normal, role);

    setQtColor(obj, set, section, name, VQTC_STATE_NORMAL, normalColor);
    setQtColor(obj, set, section, name, VQTC_STATE_DISABLED, palette.color(QPalette::Disabled, role));
    setQtColor(obj, set, section, name, VQTC_STATE_HOVERED, normalColor);
    setQtColor(obj, set, section, name, VQTC_STATE_FOCUSED, normalColor);

    if (sys->m_isDark)
        setQtColor(obj, set, section, name, VQTC_STATE_PRESSED, normalColor.lighter(150));
    else
        setQtColor(obj, set, section, name, VQTC_STATE_PRESSED, normalColor.darker(110));
}

static void setQtColorSetFg(vlc_qt_theme_provider_t* obj,
                        vlc_qt_theme_color_set set, vlc_qt_theme_color_name name, QPalette::ColorRole role)
{
    auto sys = static_cast<SystemePaletteObserver*>(obj->p_sys);
    setQtColorSet(obj, sys->m_palette, set, VQTC_SECTION_FG, name, role);
}

static void setQtColorSetBg(vlc_qt_theme_provider_t* obj,
                        vlc_qt_theme_color_set set, vlc_qt_theme_color_name name, QPalette::ColorRole role)
{
    auto sys = static_cast<SystemePaletteObserver*>(obj->p_sys);
    setQtColorSet(obj, sys->m_palette, set, VQTC_SECTION_BG, name, role);
}

static void setQtColorSetBorder(vlc_qt_theme_provider_t* obj,
    vlc_qt_theme_color_set set, QPalette::ColorRole roleBg, QPalette::ColorRole roleFg)
{
    auto sys = static_cast<SystemePaletteObserver*>(obj->p_sys);
    QColor fg = sys->m_palette.color(QPalette::Normal, roleFg);
    QColor bg = sys->m_palette.color(QPalette::Normal, roleBg);
    setQtColor(obj, set, VQTC_SECTION_DECORATION, VQTC_NAME_BORDER, VQTC_STATE_NORMAL, blendColors(bg, fg));
}


static int updatePalette(vlc_qt_theme_provider_t* obj)
{
    auto sys = static_cast<SystemePaletteObserver*>(obj->p_sys);

    QPalette& palette = sys->m_palette;

    QColor accent = QColor( sys->m_isDark ? "#FF8800" : "#FF610A" );
    QColor accentPressed = QColor( sys->m_isDark ? "#e67a30" : "#e65609" );
    QColor textOnAccent = Qt::white;
    QColor visualFocus =  sys->m_isDark ? Qt::white : Qt::black;
    QColor shadow = palette.color(QPalette::Normal, QPalette::Shadow);
    shadow.setAlphaF(0.22);
    QColor separator = palette.color(QPalette::Normal, QPalette::Mid);

    QColor hightlight = palette.color(QPalette::Normal, QPalette::Highlight);
    QColor textOnHightlight = palette.color(QPalette::Normal, QPalette::HighlightedText);

    QColor buttonBgNormal = palette.color(QPalette::Normal, QPalette::Button);
    buttonBgNormal.setAlpha(0);

    QColor negative("#C42B1C");
    QColor negativeHover = negative.lighter(110);
    QColor negativePressed = negative.lighter(150);
    QColor textOnNegative(Qt::white);

    QColor neutral("#c6bf00");
    QColor neutralHover = neutral.lighter(110);
    QColor neutralPressed = neutral.lighter(150);
    QColor textOnNeutral(Qt::white);

    QColor positive("#0F7B0F");
    QColor positiveHover = positive.lighter(110);
    QColor positivePressed = positive.lighter(150);
    QColor textOnPositive(Qt::white);

    QColor inputBorderNormal = blendColors(palette.color(QPalette::Normal, QPalette::Base),
                                    palette.color(QPalette::Normal, QPalette::Text));

    QColor inputBorderFocused = accent;

    QColor secondaryTextBase = blendColors(palette.color(QPalette::Normal, QPalette::Base),
                                 palette.color(QPalette::Normal, QPalette::Text), 0.2);
    //QColor secondaryTextWindow = blendColors(palette.color(QPalette::Normal, QPalette::Window),
    //                             palette.color(QPalette::Normal, QPalette::WindowText), 0.2);
    QColor secondaryTextButton = blendColors(palette.color(QPalette::Normal, QPalette::Button),
                                 palette.color(QPalette::Normal, QPalette::ButtonText), 0.2);


    //View
    {
        vlc_qt_theme_color_set CS = VQTC_SET_VIEW;
        setQtColorSetBg(obj, CS, VQTC_NAME_PRIMARY, QPalette::Base);
        setQtColorSetFg(obj, CS, VQTC_NAME_PRIMARY, QPalette::Text);

        setQtColorSetBg(obj, CS, VQTC_NAME_SECONDARY, QPalette::AlternateBase);
        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_SECONDARY, VQTC_STATE_NORMAL, secondaryTextBase);

        setQtColorSetBorder(obj, CS, QPalette::Base, QPalette::Text);

        setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_ACCENT, VQTC_STATE_NORMAL, accent);
        setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_VISUAL_FOCUS, VQTC_STATE_NORMAL, visualFocus);
        setQtColor(obj, CS, VQTC_SECTION_DECORATION,  VQTC_NAME_SEPARATOR, VQTC_STATE_NORMAL, separator);
        setQtColor(obj, CS, VQTC_SECTION_DECORATION,  VQTC_NAME_SHADOW, VQTC_STATE_NORMAL,shadow );
    }

    {
        auto CS = VQTC_SET_WINDOW;

        //Window
        setQtColorSetBg(obj, CS, VQTC_NAME_PRIMARY, QPalette::Window);
        setQtColorSetBg(obj, CS, VQTC_NAME_SECONDARY, QPalette::Window);

        setQtColorSetFg(obj, CS, VQTC_NAME_PRIMARY, QPalette::WindowText);

        setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_BORDER, VQTC_STATE_NORMAL,
                  palette.color(QPalette::Normal, sys->m_isDark ? QPalette::Light : QPalette::Dark));

        setQtColor(obj, CS, VQTC_SECTION_DECORATION,  VQTC_NAME_SEPARATOR, VQTC_STATE_NORMAL, separator);

        //Window (notifications)
        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_NEGATIVE, VQTC_STATE_NORMAL, negative);
        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_NEGATIVE, VQTC_STATE_NORMAL, textOnNegative);

        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_NEUTRAL, VQTC_STATE_NORMAL, neutral);
        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_NEUTRAL, VQTC_STATE_NORMAL, textOnNeutral);

        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_POSITIVE, VQTC_STATE_NORMAL, positive);
        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_POSITIVE, VQTC_STATE_NORMAL, textOnPositive);
    }

    // menubar
    {
        auto CS = VQTC_SET_MENUBAR;
        setQtColorSetBg(obj, CS, VQTC_NAME_PRIMARY, QPalette::Window);
        setQtColorSetFg(obj, CS, VQTC_NAME_PRIMARY, QPalette::WindowText);

        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_PRIMARY, VQTC_STATE_NORMAL, Qt::transparent);
    }

    //tool button
    {
        auto CS = VQTC_SET_TOOL_BUTTON;
        setQtColorSetBg(obj, CS, VQTC_NAME_PRIMARY, QPalette::Button);
        setQtColorSetFg(obj, CS, VQTC_NAME_PRIMARY, QPalette::ButtonText);

        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_SECONDARY, VQTC_STATE_NORMAL, secondaryTextButton);

        setQtColorSetBorder(obj, CS, QPalette::Button, QPalette::ButtonText);

        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_NEGATIVE, VQTC_STATE_NORMAL, negative);
        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_NEGATIVE, VQTC_STATE_HOVERED, negativeHover);
        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_NEGATIVE, VQTC_STATE_FOCUSED, negativeHover);
        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_NEGATIVE, VQTC_STATE_PRESSED, negativePressed);

        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_NEGATIVE, VQTC_STATE_NORMAL, textOnNegative);

        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_NEUTRAL, VQTC_STATE_NORMAL, neutral);
        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_NEUTRAL, VQTC_STATE_HOVERED, neutralHover);
        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_NEUTRAL, VQTC_STATE_FOCUSED, neutralHover);
        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_NEUTRAL, VQTC_STATE_PRESSED, neutralPressed);

        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_NEUTRAL, VQTC_STATE_NORMAL, textOnNeutral);

        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_POSITIVE, VQTC_STATE_NORMAL, positive);
        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_POSITIVE, VQTC_STATE_HOVERED, positiveHover);
        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_POSITIVE, VQTC_STATE_FOCUSED, positiveHover);
        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_POSITIVE, VQTC_STATE_PRESSED, positivePressed);

        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_POSITIVE, VQTC_STATE_NORMAL, textOnPositive);
    }

    //tab button
    {
        auto CS = VQTC_SET_TAB_BUTTON;
        setQtColorSetBg(obj, CS, VQTC_NAME_PRIMARY, QPalette::Button);
        setQtColorSetFg(obj, CS, VQTC_NAME_PRIMARY, QPalette::ButtonText);
        setQtColorSetBorder(obj, CS, QPalette::Button, QPalette::ButtonText);
        //keep the background transparent in the tabbar in normal state
        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_PRIMARY, VQTC_STATE_NORMAL, buttonBgNormal);

        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_SECONDARY, VQTC_STATE_NORMAL, secondaryTextButton);
    }

    //Primary action
    {
        auto CS = VQTC_SET_BUTTON_ACCENT;
        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_PRIMARY, VQTC_STATE_NORMAL, accent);
        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_PRIMARY, VQTC_STATE_PRESSED, accentPressed);
        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_PRIMARY, VQTC_STATE_DISABLED, palette.color(QPalette::Disabled, QPalette::Button));

        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_PRIMARY, VQTC_STATE_NORMAL, textOnAccent);
        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_PRIMARY, VQTC_STATE_DISABLED, palette.color(QPalette::Disabled, QPalette::ButtonText));

        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_SECONDARY, VQTC_STATE_NORMAL, secondaryTextButton);

        //always no border
        setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_BORDER, VQTC_STATE_NORMAL, Qt::transparent);
    }

    //Secondary Action
    {
        auto CS = VQTC_SET_BUTTON_STANDARD;
        setQtColorSetBg(obj, CS, VQTC_NAME_PRIMARY, QPalette::Button);
        setQtColorSetFg(obj, CS, VQTC_NAME_PRIMARY, QPalette::ButtonText);
        //keep the background transparent in the tabbar in normal state
        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_PRIMARY, VQTC_STATE_NORMAL, buttonBgNormal);

        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_SECONDARY, VQTC_STATE_NORMAL, secondaryTextButton);

        //always no border
        setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_BORDER, VQTC_STATE_NORMAL, Qt::transparent);
    }

    //Tooltip
    {
        auto CS = VQTC_SET_TOOLTIP;
        setQtColorSetBg(obj, CS, VQTC_NAME_PRIMARY, QPalette::ToolTipBase);
        setQtColorSetFg(obj, CS, VQTC_NAME_PRIMARY, QPalette::ToolTipText);
        setQtColorSetBorder(obj, CS, QPalette::ToolTipBase, QPalette::ToolTipText);
    }

    //Item
    {
        auto CS = VQTC_SET_ITEM;
        setQtColorSetBg(obj, CS, VQTC_NAME_PRIMARY, QPalette::AlternateBase);

        setQtColorSetFg(obj, CS, VQTC_NAME_PRIMARY, QPalette::Text);
        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_SECONDARY, VQTC_STATE_NORMAL, secondaryTextBase);

        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_PRIMARY, VQTC_STATE_NORMAL, Qt::transparent);
        QColor itemBgHover = hightlight;
        itemBgHover.setAlphaF(0.1);
        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_PRIMARY, VQTC_STATE_FOCUSED, itemBgHover);
        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_PRIMARY, VQTC_STATE_HOVERED, itemBgHover);

        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_HIGHLIGHT, VQTC_STATE_NORMAL, hightlight);
        QColor hightlightHover = sys->m_isDark ? hightlight.lighter(110) : hightlight.darker(110);
        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_HIGHLIGHT, VQTC_STATE_FOCUSED, hightlightHover);
        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_HIGHLIGHT, VQTC_STATE_HOVERED, hightlightHover);
        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_HIGHLIGHT, VQTC_STATE_NORMAL, textOnHightlight);


        const auto bg = palette.color(QPalette::AlternateBase);
        const auto indicator = sys->m_isDark ? bg.lighter() : bg.darker();
        setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_INDICATOR
                   , VQTC_STATE_NORMAL, indicator);

    }

    //Badge
    {
        auto CS = VQTC_SET_BADGE;
        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_PRIMARY, VQTC_STATE_NORMAL, Qt::black);
        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_PRIMARY, VQTC_STATE_NORMAL, Qt::white);
    }

    //TextField
    {
        auto CS = VQTC_SET_TEXTFIELD;
        setQtColorSetBg(obj, CS, VQTC_NAME_PRIMARY, QPalette::AlternateBase);
        setQtColorSetFg(obj, CS, VQTC_NAME_PRIMARY, QPalette::Text);

        QColor textfieldBorder;
        setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_BORDER, VQTC_STATE_NORMAL, inputBorderNormal);
        setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_BORDER, VQTC_STATE_HOVERED, inputBorderNormal);
        setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_BORDER, VQTC_STATE_FOCUSED, inputBorderFocused);

        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_HIGHLIGHT, VQTC_STATE_NORMAL, hightlight);

        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_HIGHLIGHT, VQTC_STATE_NORMAL, hightlight);
        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_HIGHLIGHT, VQTC_STATE_NORMAL, textOnHightlight);

        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_NEGATIVE, VQTC_STATE_NORMAL, negative);
        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_NEGATIVE, VQTC_STATE_NORMAL, textOnNegative);

        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_NEUTRAL, VQTC_STATE_NORMAL, neutral);
        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_NEUTRAL, VQTC_STATE_NORMAL, textOnNeutral);

        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_POSITIVE, VQTC_STATE_NORMAL, positive);
        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_POSITIVE, VQTC_STATE_NORMAL, textOnPositive);
    }

    //Combobox
    {
        auto CS = VQTC_SET_COMBOBOX;
        setQtColorSetBg(obj, CS, VQTC_NAME_PRIMARY, QPalette::AlternateBase);
        setQtColorSetFg(obj, CS, VQTC_NAME_PRIMARY, QPalette::Text);

        QColor textfieldBorder;
        setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_BORDER, VQTC_STATE_NORMAL, inputBorderNormal);
        setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_BORDER, VQTC_STATE_HOVERED, inputBorderNormal);
        setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_BORDER, VQTC_STATE_FOCUSED, inputBorderFocused);
    }

    //Spinbox
    {
        auto CS = VQTC_SET_SPINBOX;
        setQtColorSetBg(obj, CS, VQTC_NAME_PRIMARY, QPalette::AlternateBase);
        setQtColorSetFg(obj, CS, VQTC_NAME_PRIMARY, QPalette::Text);

        QColor textfieldBorder;
        setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_BORDER, VQTC_STATE_NORMAL, inputBorderNormal);
        setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_BORDER, VQTC_STATE_HOVERED, inputBorderNormal);
        setQtColor(obj, CS, VQTC_SECTION_DECORATION, VQTC_NAME_BORDER, VQTC_STATE_FOCUSED, inputBorderFocused);
    }

    //Slider
    {
        auto CS = VQTC_SET_SLIDER;
        setQtColorSetBg(obj, CS, VQTC_NAME_PRIMARY, QPalette::Button);

        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_PRIMARY, VQTC_STATE_NORMAL, accent);
        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_NEUTRAL, VQTC_STATE_NORMAL, neutral);
        setQtColor(obj, CS, VQTC_SECTION_FG, VQTC_NAME_NEGATIVE, VQTC_STATE_NORMAL, negative);
    }

    {
        auto CS = VQTC_SET_SWITCH;
        setQtColorSetBg(obj, CS, VQTC_NAME_PRIMARY, QPalette::AlternateBase);
        setQtColorSetFg(obj, CS, VQTC_NAME_PRIMARY, QPalette::Text);
        setQtColorSetBorder(obj, CS, QPalette::AlternateBase, QPalette::Text);

        setQtColor(obj, CS, VQTC_SECTION_BG, VQTC_NAME_SECONDARY, VQTC_STATE_NORMAL, accent);
        setQtColorSetFg(obj, CS, VQTC_NAME_SECONDARY, QPalette::Button);
    }

    return VLC_SUCCESS;
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
