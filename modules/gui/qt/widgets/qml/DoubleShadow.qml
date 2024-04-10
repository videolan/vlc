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

import "qrc:///style/"

// A convenience file to encapsulate two drop shadow images stacked on top
// of each other
ScaledImage {
    id: root

    property Item sourceItem: null

    property real viewportWidth: rectWidth + (Math.max(Math.abs(primaryHorizontalOffset) + primaryBlurRadius, Math.abs(secondaryHorizontalOffset) + secondaryBlurRadius)) * 2
    property real viewportHeight: rectHeight + (Math.max(Math.abs(primaryVerticalOffset) + primaryBlurRadius, Math.abs(secondaryVerticalOffset) + secondaryBlurRadius)) * 2

    property real rectWidth: sourceItem?.width ?? 0
    property real rectHeight: sourceItem?.height ?? 0
    property real xRadius: sourceItem?.radius ?? 0
    property real yRadius: sourceItem?.radius ?? 0

    property color primaryColor: Qt.rgba(0, 0, 0, .18)
    property real primaryVerticalOffset: 0
    property real primaryHorizontalOffset: 0
    property real primaryBlurRadius: 0

    property color secondaryColor: Qt.rgba(0, 0, 0, .22)
    property real secondaryVerticalOffset: 0
    property real secondaryHorizontalOffset: 0
    property real secondaryBlurRadius: 0

    //by default we request
    sourceSize: Qt.size(viewportWidth, viewportHeight)

    cache: true
    asynchronous: true

    fillMode: Image.Stretch

    visible: (width > 0 && height > 0)

    z: -1

    onSourceSizeChanged: {
        // Do not load the image when size is not valid:
        if (sourceSize.width > 0 && sourceSize.height > 0)
            source = Qt.binding(() => {
                return Effects.url(
                    Effects.DoubleRoundedRectDropShadow,
                    {
                        "viewportWidth" : viewportWidth,
                        "viewportHeight" :viewportHeight,

                        "rectWidth": rectWidth,
                        "rectHeight": rectHeight,
                        "xRadius": xRadius,
                        "yRadius": yRadius,

                        "primaryColor": primaryColor,
                        "primaryBlurRadius": primaryBlurRadius,
                        "primaryXOffset": primaryHorizontalOffset,
                        "primaryYOffset": primaryVerticalOffset,

                        "secondaryColor": secondaryColor,
                        "secondaryBlurRadius": secondaryBlurRadius,
                        "secondaryXOffset": secondaryHorizontalOffset,
                        "secondaryYOffset": secondaryVerticalOffset,
                    })
            })
        else
            source = ""
    }
}
