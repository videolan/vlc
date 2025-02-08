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

import VLC.Util

Item {
    implicitWidth: image.implicitWidth
    implicitHeight: image.implicitHeight

    property Item sourceItem: null

    property real viewportWidth: rectWidth + (blurRadius + Math.abs(xOffset)) * 2
    property real viewportHeight: rectHeight + (blurRadius + Math.abs(yOffset)) * 2

    property real blurRadius: 0
    property color color

    property real rectWidth: sourceItem ? Math.min((sourceItem.paintedWidth ?? Number.MAX_VALUE) - Math.ceil(sourceItem.padding ?? 0) * 2, sourceItem.width) : 0
    property real rectHeight: sourceItem ? Math.min((sourceItem.paintedHeight ?? Number.MAX_VALUE) - Math.ceil(sourceItem.padding ?? 0) * 2, sourceItem.height) : 0

    property real xOffset: 0
    property real yOffset: 0
    property real xRadius: (sourceItem ? (sourceItem.effectiveRadius ?? sourceItem.radius) : 0) ?? 0
    property real yRadius: (sourceItem ? (sourceItem.effectiveRadius ?? sourceItem.radius) : 0) ?? 0

    property alias sourceSize: image.sourceSize
    property alias cache: image.cache
    property alias asynchronous: image.asynchronous
    property alias fillMode: image.fillMode

    sourceSize: Qt.size(viewportWidth, viewportHeight)

    Image {
        id: image

        cache: true
        asynchronous: true

        visible: !visualDelegate.readyForVisibility

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
                                        "yRadius": yRadius
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
