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
import QtQuick.Layouts 1.4

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Item {

    property Component titleDelegate: RowLayout {
        property var rowModel: parent.rowModel
        property var model: parent.colModel
        readonly property bool containsMouse: parent.containsMouse
        readonly property bool currentlyFocused: parent.currentlyFocused

        anchors.fill: parent
        spacing: VLCStyle.margin_normal

        Image {
            source: !rowModel ? VLCStyle.noArtCover : (rowModel.cover || VLCStyle.noArtCover)
            mipmap: true // this widget can down scale the source a lot, so for better visuals we use mipmap

            Layout.preferredHeight: VLCStyle.trackListAlbumCover_heigth
            Layout.preferredWidth: VLCStyle.trackListAlbumCover_width

            Widgets.PlayCover {
                anchors.fill: parent
                iconSize: VLCStyle.play_cover_small
                visible: currentlyFocused || containsMouse

                onIconClicked: medialib.addAndPlay( rowModel.id )
            }
        }

        Widgets.ListLabel {
            text: !rowModel ? "" : (rowModel[model.criteria] || "")

            Layout.fillHeight: true
            Layout.fillWidth: true
        }
    }

    property Component titleHeaderDelegate: Row {
        spacing: VLCStyle.margin_normal

        Widgets.IconLabel {
            width: VLCStyle.heightAlbumCover_xsmall
            horizontalAlignment: Text.AlignHCenter
            text: VLCIcons.album_cover
            color: VLCStyle.colors.caption
        }

        Widgets.CaptionLabel {
            text: model.text || ""
        }
    }

    property Component timeHeaderDelegate: Widgets.IconLabel {
        width: timeTextMetric.width
        horizontalAlignment: Text.AlignHCenter
        text: VLCIcons.time
        color: VLCStyle.colors.caption
    }

    property Component timeColDelegate: Item {
        property var rowModel: parent.rowModel
        property var model: parent.colModel

        Widgets.ListLabel {
            width: timeTextMetric.width
            height: parent.height
            horizontalAlignment: Text.AlignHCenter
            text: !rowModel ? "" : rowModel[model.criteria] || ""
        }
    }

    TextMetrics {
        id: timeTextMetric

        font.pixelSize: VLCStyle.fontSize_normal
        text: "00h00"
    }

}
