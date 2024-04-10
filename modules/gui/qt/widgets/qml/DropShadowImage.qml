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

import QtQuick

import org.videolan.vlc 0.1

ScaledImage {

    property Item sourceItem: null

    property real viewportWidth: rectWidth + (blurRadius + Math.abs(xOffset)) * 2
    property real viewportHeight: rectHeight + (blurRadius + Math.abs(yOffset)) * 2

    property real blurRadius: 0
    property color color

    property real rectWidth: sourceItem?.width ?? 0
    property real rectHeight: sourceItem?.height ?? 0

    property real xOffset: 0
    property real yOffset: 0
    property real xRadius: sourceItem?.radius ?? 0
    property real yRadius: sourceItem?.radius ?? 0

    sourceSize: Qt.size(viewportWidth, viewportHeight)

    cache: true
    asynchronous: true

    fillMode: Image.Stretch

    onSourceSizeChanged: {
        // Do not load the image when size is not valid:
        if (sourceSize.width > 0 && sourceSize.height > 0)
            source = Qt.binding(function() {
                return Effects.url((xRadius > 0 || yRadius > 0) ? Effects.RoundedRectDropShadow
                                                                : Effects.RectDropShadow,
                                   {
                                    "viewportWidth" : viewportWidth,
                                    "viewportHeight" :viewportHeight,

                                    "blurRadius": blurRadius,
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
