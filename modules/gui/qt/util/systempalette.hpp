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
#ifndef SYSTEMPALETTE_H
#define SYSTEMPALETTE_H

#include <QObject>
#include <QPalette>

class SystemPalette : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QColor text         READ text         NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor textDisabled READ textDisabled NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor textInactive READ textInactive NOTIFY paletteChanged FINAL)


    Q_PROPERTY(QColor textBright         READ textBright         NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor textBrightDisabled READ textBrightDisabled NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor textBrightInactive READ textBrightInactive NOTIFY paletteChanged FINAL)


    Q_PROPERTY(QColor link         READ link         NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor linkDisabled READ linkDisabled NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor linkInactive READ linkInactive NOTIFY paletteChanged FINAL)

    Q_PROPERTY(QColor linkVisited         READ linkVisited         NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor linkVisitedDisabled READ linkVisitedDisabled NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor linkVisitedInactive READ linkVisitedInactive NOTIFY paletteChanged FINAL)

    Q_PROPERTY(QColor base         READ base         NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor baseDisabled READ baseDisabled NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor baseInactive READ baseInactive NOTIFY paletteChanged FINAL)

    Q_PROPERTY(QColor alternateBase         READ alternateBase         NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor alternateBaseDisabled READ alternateBaseDisabled NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor alternateBaseInactive READ alternateBaseInactive NOTIFY paletteChanged FINAL)

    Q_PROPERTY(QColor window         READ window         NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor windowDisabled READ windowDisabled NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor windowInactive READ windowInactive NOTIFY paletteChanged FINAL)

    Q_PROPERTY(QColor windowText         READ windowText         NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor windowTextDisabled READ windowTextDisabled NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor windowTextInactive READ windowTextInactive NOTIFY paletteChanged FINAL)

    Q_PROPERTY(QColor button         READ button         NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor buttonDisabled READ buttonDisabled NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor buttonInactive READ buttonInactive NOTIFY paletteChanged FINAL)

    Q_PROPERTY(QColor buttonText         READ buttonText         NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor buttonTextDisabled READ buttonTextDisabled NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor buttonTextInactive READ buttonTextInactive NOTIFY paletteChanged FINAL)

    Q_PROPERTY(QColor highlight         READ highlight         NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor highlightDisabled READ highlightDisabled NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor highlightInactive READ highlightInactive NOTIFY paletteChanged FINAL)

    Q_PROPERTY(QColor highlightText         READ highlightText         NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor highlightTextDisabled READ highlightTextDisabled NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor highlightTextInactive READ highlightTextInactive NOTIFY paletteChanged FINAL)

    Q_PROPERTY(QColor tooltip         READ tooltip         NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor tooltipDisabled READ tooltipDisabled NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor tooltipInactive READ tooltipInactive NOTIFY paletteChanged FINAL)

    Q_PROPERTY(QColor tooltipText         READ tooltipText         NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor tooltipTextDisabled READ tooltipTextDisabled NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor tooltipTextInactive READ tooltipTextInactive NOTIFY paletteChanged FINAL)

    Q_PROPERTY(QColor light     READ light    NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor midlight  READ midlight NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor dark      READ dark     NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor mid       READ mid      NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor shadow    READ shadow   NOTIFY paletteChanged FINAL)

    Q_PROPERTY(bool isDark READ isDark NOTIFY paletteChanged FINAL)

public:
    SystemPalette(QObject* parent = nullptr);

    QColor text() const;
    QColor textDisabled() const;
    QColor textInactive() const;

    QColor textBright() const;
    QColor textBrightDisabled() const;
    QColor textBrightInactive() const;

    QColor link() const;
    QColor linkDisabled() const;
    QColor linkInactive() const;

    QColor linkVisited() const;
    QColor linkVisitedDisabled() const;
    QColor linkVisitedInactive() const;

    QColor base() const;
    QColor baseDisabled() const;
    QColor baseInactive() const;

    QColor alternateBase() const;
    QColor alternateBaseDisabled() const;
    QColor alternateBaseInactive() const;

    QColor window() const;
    QColor windowDisabled() const;
    QColor windowInactive() const;

    QColor windowText() const;
    QColor windowTextDisabled() const;
    QColor windowTextInactive() const;

    QColor button() const;
    QColor buttonDisabled() const;
    QColor buttonInactive() const;

    QColor buttonText() const;
    QColor buttonTextDisabled() const;
    QColor buttonTextInactive() const;

    QColor highlight() const;
    QColor highlightDisabled() const;
    QColor highlightInactive() const;

    QColor highlightText() const;
    QColor highlightTextDisabled() const;
    QColor highlightTextInactive() const;

    QColor tooltip() const;
    QColor tooltipDisabled() const;
    QColor tooltipInactive() const;

    QColor tooltipText() const;
    QColor tooltipTextDisabled() const;
    QColor tooltipTextInactive() const;

    QColor light() const;
    QColor midlight() const;
    QColor dark() const;
    QColor mid() const;
    QColor shadow() const;

    bool isDark() const;

signals:
    void paletteChanged();

private:
    QPalette m_palette;
};

#endif // SYSTEMPALETTE_H
