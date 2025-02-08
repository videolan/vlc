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


import VLC.Style
import VLC.Util

// A convenience file to encapsulate two drop shadow images stacked on top
// of each other
Item {
    implicitWidth: image.implicitWidth
    implicitHeight: image.implicitHeight

    property Item sourceItem: null

    readonly property real viewportHorizontalOffset: (Math.max(Math.abs(primaryHorizontalOffset) + primaryBlurRadius, Math.abs(secondaryHorizontalOffset) + secondaryBlurRadius)) * 2
    readonly property real viewportVerticalOffset: (Math.max(Math.abs(primaryVerticalOffset) + primaryBlurRadius, Math.abs(secondaryVerticalOffset) + secondaryBlurRadius)) * 2

    property real viewportWidth: rectWidth + viewportHorizontalOffset
    property real viewportHeight: rectHeight + viewportVerticalOffset

    property real rectWidth: sourceItem ? Math.min((sourceItem.paintedWidth ?? Number.MAX_VALUE) - Math.ceil(sourceItem.padding ?? 0) * 2, sourceItem.width) : 0
    property real rectHeight: sourceItem ? Math.min((sourceItem.paintedHeight ?? Number.MAX_VALUE) - Math.ceil(sourceItem.padding ?? 0) * 2, sourceItem.height) : 0
    property real xRadius: (sourceItem ? (sourceItem.effectiveRadius ?? sourceItem.radius) : 0) ?? 0
    property real yRadius: (sourceItem ? (sourceItem.effectiveRadius ?? sourceItem.radius) : 0) ?? 0

    property color primaryColor: Qt.rgba(0, 0, 0, .18)
    property real primaryVerticalOffset: 0
    property real primaryHorizontalOffset: 0
    property real primaryBlurRadius: 0

    property color secondaryColor: Qt.rgba(0, 0, 0, .22)
    property real secondaryVerticalOffset: 0
    property real secondaryHorizontalOffset: 0
    property real secondaryBlurRadius: 0

    property alias sourceSize: image.sourceSize
    property alias cache: image.cache
    property alias asynchronous: image.asynchronous
    property alias fillMode: image.fillMode

    z: -1

    visible: (width > 0 && height > 0)

    //by default we request
    sourceSize: Qt.size(viewportWidth, viewportHeight)

    ScaledImage {
        id: image

        anchors.fill: parent

        cache: true
        asynchronous: true

        visible: !visualDelegate.readyForVisibility

        fillMode: Image.Stretch

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

    ShaderEffect {
        id: visualDelegate

        anchors.centerIn: parent
        anchors.alignWhenCentered: true

        implicitWidth: image.implicitWidth
        implicitHeight: image.implicitHeight

        width: image.paintedWidth
        height: image.paintedHeight

        visible: readyForVisibility

        readonly property bool readyForVisibility: (GraphicsInfo.shaderType === GraphicsInfo.RhiShader)

        supportsAtlasTextures: true
        blending: true
        // cullMode: ShaderEffect.BackFaceCulling

        readonly property Image source: image

        // TODO: Dithered texture is not necessary if theme is not dark.
        fragmentShader: "qrc:///shaders/DitheredTexture.frag.qsb"
    }
}
