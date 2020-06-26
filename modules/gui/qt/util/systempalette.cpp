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

SystemPalette::SystemPalette(QObject* parent)
    : QObject(parent)
    , m_palette(qApp->palette())
{

    connect(qApp, &QGuiApplication::paletteChanged, this, &SystemPalette::paletteChanged);

}

#define COLOR_GETTER(getter, role) \
    QColor SystemPalette::getter() const{ \
        return m_palette.color(QPalette::Normal, role); \
    } \
    QColor SystemPalette::getter##Disabled() const { \
        return m_palette.color(QPalette::Disabled, role); \
    } \
    QColor SystemPalette::getter##Inactive() const { \
        return m_palette.color(QPalette::Inactive, role); \
    }

COLOR_GETTER(text, QPalette::Text)
COLOR_GETTER(textBright, QPalette::BrightText)
COLOR_GETTER(link, QPalette::Link)
COLOR_GETTER(linkVisited, QPalette::LinkVisited)
COLOR_GETTER(base, QPalette::Base)
COLOR_GETTER(alternateBase, QPalette::AlternateBase)
COLOR_GETTER(window, QPalette::Window)
COLOR_GETTER(windowText, QPalette::WindowText)
COLOR_GETTER(button, QPalette::Button)
COLOR_GETTER(buttonText, QPalette::ButtonText)
COLOR_GETTER(highlight, QPalette::Highlight)
COLOR_GETTER(highlightText, QPalette::HighlightedText)
COLOR_GETTER(tooltip, QPalette::ToolTipBase)
COLOR_GETTER(tooltipText, QPalette::ToolTipText)

QColor SystemPalette::light() const
{
    return m_palette.color(QPalette::Normal, QPalette::Light);
}

QColor SystemPalette::midlight() const
{
    return m_palette.color(QPalette::Normal, QPalette::Midlight);
}

QColor SystemPalette::mid() const
{
    return m_palette.color(QPalette::Normal, QPalette::Mid);
}

QColor SystemPalette::dark() const
{
    return m_palette.color(QPalette::Normal, QPalette::Dark);
}

QColor SystemPalette::shadow() const
{
    return m_palette.color(QPalette::Normal, QPalette::Shadow);
}

bool SystemPalette::isDark() const
{
    return base().lightness() < text().lightness();
}
