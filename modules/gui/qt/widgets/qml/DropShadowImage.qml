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

import QtQuick 2.12

import org.videolan.vlc 0.1

ScaledImage {
    /* required */ property real blurRadius: 0
    /* required */ property color color
    /* required */ property real rectWidth: 0
    /* required */ property real rectHeight: 0
    property real xOffset: 0
    property real yOffset: 0
    property real xRadius: 0
    property real yRadius: 0

    sourceSize: Qt.size(
                    rectWidth + (blurRadius + Math.abs(xOffset)) * 2,
                    rectHeight + (blurRadius + Math.abs(yOffset)) * 2,
                    )

    cache: true
    asynchronous: true

    fillMode: Image.Stretch

    onSourceSizeChanged: {
        // Do not load the image when size is not valid:
        if (sourceSize.width > 0 && sourceSize.height > 0)
            source = Qt.binding(function() {
                return Effects.url((xRadius > 0 || yRadius > 0) ? Effects.RoundedRectDropShadow
                                                                : Effects.RectDropShadow,
                                   {"blurRadius": blurRadius,
                                    "color": color,
                                    "rectWidth": rectWidth,
                                    "rectHeight": rectHeight,
                                    "xOffset": xOffset,
                                    "yOffset": yOffset,
                                    "xRadius": xRadius,
                                    "yRadius": yRadius})
            })
        else
            source = ""
    }
}
