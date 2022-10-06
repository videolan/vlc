/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
#include "systempalette.hpp"
#include <QGuiApplication>
#include <QPalette>
#include <QSettings>
#include <QFile>
#include <maininterface/mainctx.hpp>
#include <vlc_modules.h>

namespace {

QColor blendColors(QColor c1, QColor c2, float blend = 0.5)
{
    return QColor::fromRgbF(c2.redF()   + (c1.redF()   - c2.redF())   * blend,
                  c2.greenF() + (c1.greenF() - c2.greenF()) * blend,
                  c2.blueF()  + (c1.blueF()  - c2.blueF())  * blend,
                  c2.alphaF() + (c1.alphaF() - c2.alphaF()) * blend);
}

}

SystemPalette::SystemPalette(QObject* parent)
    : QObject(parent)
{
    updatePalette();
}

ColorSchemeModel::ColorScheme SystemPalette::source() const
{
    return m_source;
}

void SystemPalette::setSource(ColorSchemeModel::ColorScheme source)
{
    if (m_source == source)
        return;
    m_source = source;

    updatePalette();

    emit sourceChanged();
}

void SystemPalette::setCtx(MainCtx* ctx)
{
    if (ctx == m_ctx)
        return;
    m_ctx = ctx;
    emit ctxChanged();
    updatePalette();
}

void SystemPalette::updatePalette()
{
    switch(m_source)
    {
    case ColorSchemeModel::Day:
        makeLightPalette();
        break;
    case ColorSchemeModel::Night:
        makeDarkPalette();
        break;
    default:
        makeLightPalette();
        break;
    }

    emit paletteChanged();
}

void SystemPalette::makeLightPalette()
{
    //QColor grey1 = QColor{"#9E9E9E"};
    QColor grey2 = QColor{"#666666"};

    m_isDark = false;

    m_text =  QColor{"#232627"};
    m_textInactive =  QColor{"#7f8c8d"};
    m_textDisabled = "#bdc3c7";

    m_bg =  lightGrey100;
    m_bgInactive =  QColor{"#fcfdfc"};

    m_bgAlt =  lightGrey400;
    m_bgAltInactive =  lightGrey400;

    m_bgHover =  lightGrey400;
    m_bgHoverText =  m_text;
    m_bgHoverInactive =  QColor{"#3daee9"};
    m_bgHoverTextInactive =  m_text;

    m_bgFocus =  Qt::black;

    m_button =  QColor{"#eff0f1"};
    m_buttonText =  m_text;
    m_buttonBorder =  blendColors(m_button, m_buttonText, 0.8);

    m_textActiveSource =  QColor{"#ff950d"};

    m_topBanner =  lightGrey400;
    m_lowerBanner =  Qt::white;

    m_accent =  orange800;

    m_alert = QColor{"#d70022"};

    m_separator =  lightGrey400;
    m_playerControlBarFg =  QColor{"#333333"};
    m_expandDelegate =  Qt::white;

    m_tooltipTextColor = Qt::white;
    m_tooltipColor = Qt::black;

    m_border = QColor{"#e0e0e0"};
    m_buttonHover = lightGrey300;
    m_buttonBanner = grey2;
    m_buttonPrimaryHover = QColor{"#e65609"};
    m_buttonPlayer = QColor{"#484848"};
    m_grid = lightGrey400;
    m_gridSelect = lightGrey600;
    m_listHover = lightGrey500;
    m_textField = QColor{"#999999"};
    m_textFieldHover = QColor{"#4c4c4c"};
    m_icon = QColor{"#616161"};
    m_sliderBarMiniplayerBgColor = QColor{"#FFEEEEEE"};
    m_windowCSDButtonBg = QColor{"#80DADADA"};
}

void SystemPalette::makeDarkPalette()
{
    m_isDark = true;

    //QColor grey1 = QColor{"#666666"};
    //QColor grey2 = QColor{"#AAAAAA"};

    m_text = "#eff0f1";
    m_textInactive = "#bdc3c7";
    m_textDisabled = "#bdc3c7";

    m_bg = darkGrey200;
    m_bgInactive = "#232629";

    m_bgAlt = darkGrey400;
    m_bgAltInactive = darkGrey300;

    m_bgHover = darkGrey800;
    m_bgHoverInactive = "#3daee9";

    m_bgHoverText = m_text;
    m_bgHoverTextInactive = m_text;

    m_bgFocus = Qt::white;

    m_button = "#31363b";
    m_buttonText = m_text;
    m_buttonBorder = "#575b5f";

    m_textActiveSource = "#ff950d";

    m_topBanner = darkGrey400;
    m_lowerBanner = Qt::black;

    m_accent = orange500;

    m_alert = QColor{"#d70022"};

    m_separator = darkGrey700;

    m_playerControlBarFg = Qt::white;

    m_expandDelegate = Qt::black;

    m_tooltipTextColor = Qt::black;
    m_tooltipColor = Qt::white;

    m_border = darkGrey800;
    m_buttonHover = darkGrey800;
    m_buttonBanner = QColor("#a6a6a6");
    m_buttonPrimaryHover = QColor{"#e67A00"};
    m_buttonPlayer = lightGrey600;
    m_grid = darkGrey500;
    m_gridSelect = darkGrey800;
    m_listHover = darkGrey500;
    m_textField = QColor{"#6f6f6f"};
    m_textFieldHover = QColor{"#b7b7b7"};
    m_icon = Qt::white;
    m_sliderBarMiniplayerBgColor = QColor{"#FF929292"};
    m_windowCSDButtonBg =  QColor{"#80484848"};
}
