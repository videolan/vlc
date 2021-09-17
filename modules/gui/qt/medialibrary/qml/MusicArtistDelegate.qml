/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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
import QtQuick.Layouts 1.11

import org.videolan.controls 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Control {
    id: root

    // Properties

    /* required */ property var mlModel

    property bool isCurrent: false

    // Aliases
    // Private

    property alias _isHover: mouseArea.containsMouse

    // Signals

    signal itemClicked(variant mouse)

    signal itemDoubleClicked(variant mouse)

    // Settings

    height: VLCStyle.play_cover_small + (VLCStyle.margin_xsmall * 2)

    // Childs

    background: Widgets.AnimatedBackground {
        id: background

        active: visualFocus

        backgroundColor: {
            if (isCurrent)
                return VLCStyle.colors.gridSelect;
            else if (_isHover)
                return VLCStyle.colors.listHover;
            else
                return VLCStyle.colors.setColorAlpha(VLCStyle.colors.listHover, 0);
        }
    }

    contentItem: MouseArea {
        id: mouseArea

        hoverEnabled: true

        drag.axis: Drag.XAndYAxis

        drag.target: Widgets.DragItem {
            function updateComponents(maxCovers) {
                return {
                    covers: [{ artwork: (model.cover) ? model.cover
                                                      : VLCStyle.noArtArtistSmall }],

                    title: (model.name) ? model.name
                                        : i18n.qtr("Unknown artist"),

                    count: 1
                }
            }

            function getSelectedInputItem() {
                return mlModel.getItemsForIndexes([mlModel.index(index, 0)]);
            }
        }

        drag.onActiveChanged: {
            var dragItem = drag.target;

            if (drag.active == false)
                dragItem.Drag.drop();

            dragItem.Drag.active = drag.active;
        }

        onPositionChanged: {
            if (drag.active == false) return;

            var pos = drag.target.parent.mapFromItem(root, mouseX, mouseY);

            drag.target.x = pos.x + VLCStyle.dragDelta;
            drag.target.y = pos.y + VLCStyle.dragDelta;
        }

        onClicked: itemClicked(mouse)

        onDoubleClicked: itemDoubleClicked(mouse)

        Widgets.CurrentIndicator {
            height: parent.height - (margin * 2)

            margin: VLCStyle.dp(4, VLCStyle.scale)

            visible: isCurrent
        }

        RowLayout {
            anchors.fill: parent

            anchors.leftMargin: VLCStyle.margin_normal
            anchors.rightMargin: VLCStyle.margin_normal
            anchors.topMargin: VLCStyle.margin_xsmall
            anchors.bottomMargin: VLCStyle.margin_xsmall

            spacing: VLCStyle.margin_xsmall

            RoundImage {
                width: VLCStyle.play_cover_small
                height: width

                radius: width

                source: (model.cover) ? model.cover
                                      : VLCStyle.noArtArtistSmall

                Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

                Rectangle {
                    anchors.fill: parent

                    radius: VLCStyle.play_cover_small

                    color: "transparent"

                    border.width: VLCStyle.dp(1, VLCStyle.scale)

                    border.color: (isCurrent || _isHover) ? VLCStyle.colors.accent
                                                          : VLCStyle.colors.roundPlayCoverBorder
                }
            }

            Widgets.ListLabel {
                text: (model.name) ? model.name
                                   : i18n.qtr("Unknown artist")

                color: background.foregroundColor

                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }
}
