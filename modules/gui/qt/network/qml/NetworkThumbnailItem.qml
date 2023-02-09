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
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Row {
    id: root

    // Properties

    readonly property bool containsMouse: parent.containsMouse
    readonly property bool currentlyFocused: parent.currentlyFocused

    property var rowModel: parent.rowModel
    property var model: parent.colModel

    readonly property int index: parent.index

    readonly property string artworkSource: !!rowModel ? rowModel.artwork : ""

    readonly property color foregroundColor: parent.foregroundColor

    // Private

    readonly property bool _showPlayCover: (currentlyFocused || containsMouse)
                                           && !!rowModel
                                           && (rowModel.type !== NetworkMediaModel.TYPE_NODE)
                                           && (rowModel.type !== NetworkMediaModel.TYPE_DIRECTORY)

    readonly property bool _showCustomCover: (!artworkSource) || (artwork.status !== Image.Ready)

    // Signals

    signal playClicked(int index)

    // Settings

    spacing: VLCStyle.margin_normal

    // Functions

    function getCriterias(colModel, rowModel) {
        if (colModel === null || rowModel === null)
            return ""

        var criterias = colModel.subCriterias

        if (criterias === undefined || criterias.length === 0)
            return ""

        var string = ""

        for (var i = 0; i < criterias.length; i++) {
            var criteria = criterias[i]

            var value = rowModel[criteria]

            if (value.toString() === "vlc://nop")
                continue

            if (i) string += " • "

            string += value
        }

        return string
    }

    // Children

    Item {
        id: itemCover

        anchors.verticalCenter: parent.verticalCenter

        width: artwork.width
        height: artwork.height

        Widgets.ListCoverShadow { anchors.fill: parent }

        NetworkCustomCover {
            id: artwork

            width: VLCStyle.listAlbumCover_width
            height: VLCStyle.listAlbumCover_height

            //radius: VLCStyle.listAlbumCover_radius

            networkModel: rowModel

            bgColor: VLCStyle.colors.bg

            Widgets.PlayCover {
                x: Math.round((artwork.width - width) / 2)
                y: Math.round((artwork.height - height) / 2)

                width: VLCStyle.play_cover_small

                visible: root._showPlayCover

                onClicked: playClicked(root.index)
            }
        }
    }

    Column {
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        anchors.topMargin: VLCStyle.margin_xxsmall
        anchors.bottomMargin: VLCStyle.margin_xxsmall

        width: Math.max(0, parent.width - x)

        Widgets.ScrollingText {
            id: itemText

            anchors.left: parent.left
            anchors.right: parent.right

            height: (itemCriterias.visible) ? Math.round(parent.height / 2)
                                            : parent.height

            visible: (listLabel.text)

            clip: scrolling

            label: listLabel

            forceScroll: root.currentlyFocused

            Widgets.ListLabel {
                id: listLabel

                anchors.verticalCenter: parent.verticalCenter

                text: (root.rowModel && model.title) ? root.rowModel[model.title] : ""

                color: root.foregroundColor
            }
        }

        Widgets.MenuCaption {
            id: itemCriterias

            anchors.left: parent.left
            anchors.right: parent.right

            height: itemText.height

            visible: (text)

            text: root.getCriterias(root.model, root.rowModel)
        }
    }
}
