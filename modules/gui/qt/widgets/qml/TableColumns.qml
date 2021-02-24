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
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///style/"

Item {
    id: root

    property bool showTitleText: true
    property int titleCover_width: VLCStyle.trackListAlbumCover_width
    property int titleCover_height: VLCStyle.trackListAlbumCover_heigth
    property int titleCover_radius: VLCStyle.trackListAlbumCover_radius

    function titlecoverLabels(model) {
        // implement this function to show labels in title Cover
        return []
    }

    property Component titleDelegate: RowLayout {
        property var rowModel: parent.rowModel
        property var model: parent.colModel
        readonly property bool containsMouse: parent.containsMouse
        readonly property bool currentlyFocused: parent.currentlyFocused

        anchors.fill: parent
        spacing: VLCStyle.margin_normal

        Item {
            Layout.preferredHeight: root.titleCover_height
            Layout.preferredWidth: root.titleCover_width

            ListCoverShadow {
                source: cover
                anchors.fill: cover
            }

            Widgets.MediaCover {
                id: cover

                anchors.fill: parent
                source: (rowModel ? (root.showTitleText ? rowModel.cover : rowModel[model.criteria]) : VLCStyle.noArtCover) || VLCStyle.noArtCover
                mipmap: true // this widget can down scale the source a lot, so for better visuals we use mipmap
                playCoverVisible: currentlyFocused || containsMouse
                playIconSize: VLCStyle.play_cover_small
                onPlayIconClicked: medialib.addAndPlay( rowModel.id )
                radius: root.titleCover_radius
                labels: root.titlecoverLabels(rowModel)
            }
        }

        Widgets.ListLabel {
            text: (!rowModel || !root.showTitleText) ? "" : (rowModel[model.criteria] || i18n.qtr("Unknown Title"))
            visible: root.showTitleText
            color: foregroundColor

            Layout.fillHeight: true
            Layout.fillWidth: true
        }
    }

    property Component titleHeaderDelegate: Row {
        spacing: VLCStyle.margin_normal

        Widgets.IconLabel {
            width: root.titleCover_width
            horizontalAlignment: Text.AlignHCenter
            text: VLCIcons.album_cover
            color: VLCStyle.colors.caption
        }

        Widgets.CaptionLabel {
            text: model.text || ""
            visible: root.showTitleText
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
            text: !rowModel || !rowModel[model.criteria] ? "" : Helpers.msToString(rowModel[model.criteria], true)
            color: foregroundColor
        }
    }

    TextMetrics {
        id: timeTextMetric

        font.pixelSize: VLCStyle.fontSize_normal
        text: "00h00"
    }

}
