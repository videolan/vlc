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

import QtQuick
import QtQuick.Controls
import QtQml.Models

import VLC.Widgets as Widgets
import VLC.Style
import VLC.Network
import VLC.Util

Widgets.TableRowDelegate {
    id: root

    readonly property string artworkSource: rowModel?.artwork ?? ""

    readonly property bool _showPlayCover: (currentlyFocused || containsMouse)
                                           && !!rowModel
                                           && (rowModel.type !== NetworkMediaModel.TYPE_NODE)
                                           && (rowModel.type !== NetworkMediaModel.TYPE_DIRECTORY)

    readonly property bool _showCustomCover: (!artworkSource) || (artwork.status !== Image.Ready)

    signal playClicked(int index)

    // Functions

    function getCriterias(colModel, rowModel) {
        if (colModel === null || rowModel === null)
            return ""

        const criterias = colModel.subCriterias

        if (criterias === undefined || criterias.length === 0)
            return ""

        let string = ""

        for (let i = 0; i < criterias.length; i++) {
            const criteria = criterias[i]

            const value = rowModel[criteria]

            if (value.toString() === "vlc://nop")
                continue

            if (i) string += " • "

            string += value
        }

        return string
    }

    Row {
        anchors.fill: parent
        spacing: VLCStyle.margin_normal

        Item {
            id: itemCover

            anchors.verticalCenter: parent.verticalCenter

            width: artwork.width
            height: artwork.height

            //FIXME: implement fillMode in RoundImage and use MediaCover here instead
            //or directly TableCollumns.titleHeaderDelegate in place of NetworkThumbnailItem
            NetworkCustomCover {
                id: artwork

                width: VLCStyle.listAlbumCover_width
                height: VLCStyle.listAlbumCover_height

                // artworks can have anysize, we try to fit it using PreserveAspectFit
                // in the provided size and place it in the center of itemCover
                fillMode: Image.PreserveAspectFit
                horizontalAlignment: Image.AlignHCenter
                verticalAlignment: Image.AlignVCenter

                networkModel: root.rowModel

                bgColor: root.colorContext.bg.secondary
                color1: root.colorContext.fg.primary
                accent: root.colorContext.accent

                Widgets.DefaultShadow {

                }

                Widgets.PlayCover {
                    x: Math.round((artwork.width - width) / 2)
                    y: Math.round((artwork.height - height) / 2)

                    width: VLCStyle.play_cover_small

                    visible: root._showPlayCover

                    onTapped: playClicked(root.index)
                }
            }
        }

        Column {
            anchors.top: parent.top
            anchors.bottom: parent.bottom

            anchors.topMargin: VLCStyle.margin_xxsmall
            anchors.bottomMargin: VLCStyle.margin_xxsmall

            width: Math.max(0, parent.width - x)

            Widgets.TextAutoScroller {
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

                    text: root.rowModel?.[root.colModel.title] ?? ""

                    color: root.selected
                        ? root.colorContext.fg.highlight
                        : root.colorContext.fg.primary
                }
            }

            Widgets.MenuCaption {
                id: itemCriterias

                anchors.left: parent.left
                anchors.right: parent.right

                height: itemText.height

                visible: (text)

                color: root.selected
                    ? root.colorContext.fg.highlight
                    : root.colorContext.fg.secondary

                text: root.getCriterias(root.colModel, root.rowModel)
            }
        }
    }
}
