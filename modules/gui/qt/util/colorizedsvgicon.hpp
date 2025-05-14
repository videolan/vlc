/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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
#ifndef COLORIZEDSVGICON_HPP
#define COLORIZEDSVGICON_HPP

#include <QIcon>

class ColorizedSvgIcon : public QIcon
{
    static int hashKey(QIcon::Mode mode, QIcon::State state)
    {
        // From QSvgIconEnginePrivate:
        return ((mode << 4) | state);
    }

public:
    explicit ColorizedSvgIcon(const QString& filename,
                              const QColor color1 = {},
                              const QColor color2 = {},
                              const QColor accentColor = {},
                              const QList<QPair<QString, QString>>& otherReplacements = {});

    [[nodiscard]] static ColorizedSvgIcon colorizedIconForWidget(const QString& fileName, const QWidget *widget);

    [[nodiscard]] static QIconEngine *svgIconEngine();
};

#endif // COLORIZEDSVGICON_HPP
