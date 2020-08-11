/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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
import QtQuick.Layouts 1.3

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Rectangle {
    property alias text: label.text
    property alias model: plitem.model

    z: 1
    width:  plitem.visible ? plitem.implicitWidth : label.implicitWidth
    height: plitem.visible ? plitem.implicitHeight : label.implicitHeight
    color: VLCStyle.colors.button
    border.color : VLCStyle.colors.buttonBorder
    visible: false

    Drag.active: visible

    property var count: 0

    function updatePos(x, y) {
        var pos = root.mapFromGlobal(x, y)
        dragItem.x = pos.x + 10
        dragItem.y = pos.y + 10
    }

    Text {
        id: label
        font.pixelSize: VLCStyle.fontSize_normal
        color: VLCStyle.colors.text
        text: i18n.qtr("%1 tracks selected").arg(count)
        visible: count > 1 || !model
    }

    Item {
        id: plitem
        opacity: 0.7
        visible: count === 1 && model

        property var model

        RowLayout {
            id: content
            anchors.fill: parent

            Item {
                Layout.preferredHeight: VLCStyle.icon_normal
                Layout.preferredWidth: VLCStyle.icon_normal
                Layout.leftMargin: VLCStyle.margin_xsmall

                Image {
                    id: artwork
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectFit
                    source: (model.artwork && model.artwork.toString()) ? model.artwork : VLCStyle.noArtCover
                    visible: !statusIcon.visible
                }

                Widgets.IconLabel {
                    id: statusIcon
                    anchors.fill: parent
                    visible: (model.isCurrent && text !== "")
                    width: height
                    height: VLCStyle.icon_normal
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: VLCStyle.colors.accent
                    text: player.playingState === PlayerController.PLAYING_STATE_PLAYING ? VLCIcons.volume_high :
                                                    player.playingState === PlayerController.PLAYING_STATE_PAUSED ? VLCIcons.pause :
                                                        player.playingState === PlayerController.PLAYING_STATE_STOPPED ? VLCIcons.stop : ""
                }
            }

            Column {
                Widgets.ListLabel {
                    id: textInfo
                    Layout.fillWidth: true
                    Layout.leftMargin: VLCStyle.margin_small

                    font.weight: model.isCurrent ? Font.DemiBold : Font.Normal
                    text: model.title
                }

                Widgets.ListSubtitleLabel {
                    id: textArtist
                    Layout.fillWidth: true
                    Layout.leftMargin: VLCStyle.margin_small

                    font.weight: model.isCurrent ? Font.DemiBold : Font.Normal
                    text: (model.artist ? model.artist : i18n.qtr("Unknown Artist"))
                }
            }

            Widgets.ListLabel {
                id: textDuration
                Layout.rightMargin: VLCStyle.margin_xsmall

                text: model.duration
            }
        }
    }
}
