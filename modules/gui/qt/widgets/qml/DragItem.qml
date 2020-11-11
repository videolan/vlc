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
import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQml.Models 2.2
import "qrc:///style/"
import "qrc:///playlist/" as Playlist

import QtGraphicalEffects 1.0

Playlist.PlaylistDroppable {
    id: dragItem

    readonly property int coverSize: VLCStyle.icon_normal
    readonly property int _maxCovers: 3
    readonly property int _displayedCoversCount: Math.min(_model.count, _maxCovers + 1)

    property var _model: {"covers": [], "title": "", "count": 0}

    // return {covers: [{artwork: <string> or cover: <component>},..maxCovers]
    //         , title: <string>, *subtitle: <string>, count: <int> /*all selected*/}
    // * - optional
    function updateComponents(maxCovers) {
        console.assert(false, "parent should reimplement this function")
    }

    Drag.onActiveChanged: {
        if (!Drag.active)
            return
        _model = updateComponents(_maxCovers)
    }

    function coversXPos(index) {
        return VLCStyle.margin_small + (coverSize / 3) * index
    }

    parent: g_mainDisplay
    width: VLCStyle.colWidth(2)
    height: coverSize + VLCStyle.margin_small * 2
    opacity: visible ? 0.90 : 0
    visible: Drag.active

    Rectangle {
        /* background */
        anchors.fill: parent
        color: VLCStyle.colors.button
        border.color: VLCStyle.colors.buttonBorder
        border.width: VLCStyle.dp(1, VLCStyle.scale)
        radius: VLCStyle.dp(6, VLCStyle.scale)
    }

    Behavior on opacity {
        NumberAnimation {
            easing.type: Easing.InOutSine
            duration: 128
        }
    }

    RectangularGlow {
        anchors.fill: parent
        glowRadius: VLCStyle.dp(8, VLCStyle.scale)
        color: VLCStyle.colors.glowColor
        spread: 0.2
    }

    Repeater {
        id: coverRepeater
        model: _model.covers

        Item {
            x: dragItem.coversXPos(index)
            y: (dragItem.height - height) / 2
            width: dragItem.coverSize
            height: dragItem.coverSize

            Rectangle {
                id: bg

                radius: coverRepeater.count > 1 ? dragItem.coverSize : VLCStyle.dp(2, VLCStyle.scale)
                anchors.fill: parent
                color: VLCStyle.colors.bg
            }

            DropShadow {
                horizontalOffset: 0
                verticalOffset: VLCStyle.dp(1, VLCStyle.scale)
                radius: VLCStyle.dp(3, VLCStyle.scale)
                samples: 2 * radius + 1
                color: Qt.rgba(0, 0, 0, .18)
                anchors.fill: bg
                source: bg
            }

            DropShadow {
                horizontalOffset: 0
                verticalOffset: VLCStyle.dp(6, VLCStyle.scale)
                radius: VLCStyle.dp(14, VLCStyle.scale)
                samples: 2 * radius + 1
                color: Qt.rgba(0, 0, 0, .18)
                anchors.fill: bg
                source: bg
            }

            Loader {
                // parent may provide extra data with covers
                property var model: modelData

                anchors.centerIn: parent
                sourceComponent: (!modelData.artwork || modelData.artwork.toString() === "") ? modelData.cover : artworkLoader
                layer.enabled: true
                layer.effect: OpacityMask {
                    maskSource: Rectangle {
                        width: bg.width
                        height: bg.height
                        radius: bg.radius
                        visible: false
                    }
                }

                onItemChanged: {
                    if (modelData.artwork && modelData.artwork.toString() !== "")
                        item.source = modelData.artwork
                }
            }

            Rectangle {
                // for cover border
                color: "transparent"
                border.width: VLCStyle.dp(1, VLCStyle.scale)
                border.color: VLCStyle.colors.buttonBorder
                anchors.fill: parent
                radius: bg.radius
            }
        }
    }

    Rectangle {
        id: extraCovers

        x: dragItem.coversXPos(_maxCovers)
        y: (dragItem.height - height) / 2
        width: dragItem.coverSize
        height: dragItem.coverSize
        radius: dragItem.coverSize
        visible: _model.count > dragItem._maxCovers
        color: VLCStyle.colors.bgAlt
        border.width: VLCStyle.dp(1, VLCStyle.scale)
        border.color: VLCStyle.colors.buttonBorder

        MenuLabel {
            anchors.centerIn: parent
            color: VLCStyle.colors.accent
            text: "+" + (_model.count - dragItem._maxCovers)
        }
    }

    DropShadow {
        horizontalOffset: 0
        verticalOffset: VLCStyle.dp(1, VLCStyle.scale)
        radius: VLCStyle.dp(3, VLCStyle.scale)
        samples: 2 * radius + 1
        color: Qt.rgba(0, 0, 0, .18)
        anchors.fill: extraCovers
        source: extraCovers
        visible: extraCovers.visible
    }

    DropShadow {
        horizontalOffset: 0
        verticalOffset: VLCStyle.dp(6, VLCStyle.scale)
        radius: VLCStyle.dp(14, VLCStyle.scale)
        samples: 2 * radius + 1
        color: Qt.rgba(0, 0, 0, .18)
        anchors.fill: extraCovers
        source: extraCovers
        visible: extraCovers.visible
    }

    Column {
        id: labelColumn

        anchors.verticalCenter: parent.verticalCenter
        x: dragItem.coversXPos(_displayedCoversCount - 1) + dragItem.coverSize + VLCStyle.margin_small
        width: parent.width - x - VLCStyle.margin_small
        spacing: VLCStyle.margin_xxxsmall

        ScrollingText {
            label: titleLabel
            scroll: true
            height: titleLabel.height
            width: parent.width

            Label {
                id: titleLabel

                text: _model.title
                visible: text && text !== ""
                width: parent.width
                elide: Text.ElideNone
                font.pixelSize: VLCStyle.fontSize_large
                color: VLCStyle.colors.buttonText
            }
        }

        MenuCaption {
            id: subtitleLabel

            visible: text && text !== ""
            width: parent.width
            text: _model.subtitle || i18n.qtr("%1 selected").arg(_model.count)
        }
    }

    Component {
        id: artworkLoader
        Image {
            mipmap: true
            fillMode: Image.PreserveAspectCrop
            width: coverSize
            height: coverSize
        }
    }
}
