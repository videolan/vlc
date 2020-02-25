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
import QtQml.Models 2.2

import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.KeyNavigableTableView {
    id: listView_id
    model: MLVideoModel {
        ml: medialib
    }
    sortModel: [
        { type: "image", criteria: "thumbnail",   width:0.2, text: i18n.qtr("Thumbnail"), showSection: "" },
        { criteria: "duration",    width:0.1, text: i18n.qtr("Duration"), showSection: "" },
        { isPrimary: true, criteria: "title",       width:0.6, text: i18n.qtr("Title"),    showSection: "title" },
        { type: "contextButton",   width:0.1, },
    ]
    section.property: "title_first_symbol"

    rowHeight: VLCStyle.video_small_height + VLCStyle.margin_normal

    property bool isFocusOnContextButton: false
    colDelegate: Item {
        id: colDel
        anchors.fill: parent
        anchors.leftMargin: VLCStyle.margin_normal
        anchors.rightMargin: VLCStyle.margin_normal

        property var rowModel: parent.rowModel
        property var model: parent.colModel
        FocusScope{
            anchors.fill: parent
            focus: isFocusOnContextButton && rowModel.index === currentIndex
            onFocusChanged: focus && contextButtonLoader.forceActiveFocus()

            Loader{
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                active: model.type === "image"
                sourceComponent: Widgets.RoundImage{
                    id: cover
                    height: VLCStyle.video_small_height
                    width: VLCStyle.video_small_width
                    source: !rowModel ? "" : rowModel[model.criteria]

                    Widgets.VideoQualityLabel {
                        id: resolutionLabel
                        anchors {
                            top: cover.top
                            left: cover.left
                            topMargin: VLCStyle.margin_xxsmall
                            leftMargin: VLCStyle.margin_xxsmall
                        }
                        text: !rowModel ? "" : rowModel.resolution_name
                    }
                    Widgets.VideoQualityLabel {
                        anchors {
                            top: cover.top
                            left: resolutionLabel.right
                            topMargin: VLCStyle.margin_xxsmall
                            leftMargin: VLCStyle.margin_xxxsmall
                        }
                        visible: !rowModel ? "" : rowModel.channel.length > 0
                        text: !rowModel ? "" : rowModel.channel
                        color: "limegreen"
                    }
                    Widgets.VideoProgressBar {
                        value: !rowModel ? 0 : rowModel.progress
                        visible: value > 0
                        anchors {
                            bottom: parent.bottom
                            left: parent.left
                            right: parent.right
                        }
                    }

                }
            }
            Loader{
                id: contextButtonLoader
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: VLCStyle.margin_xxsmall
                active: model.type === "contextButton"
                sourceComponent: Widgets.ContextButton{
                    backgroundColor: hovered || activeFocus ?
                                         VLCStyle.colors.getBgColor( root.isSelected, hovered,
                                                                    root.activeFocus) : "transparent"
                    focus: contextButtonLoader.focus
                    onClicked: listView_id.contextMenuButtonClicked(this,rowModel)
                }
            }
            Loader{
                anchors.fill:parent
                active: model.type !== "image"
                sourceComponent: Text {
                    text: !rowModel ? "" : rowModel[model.criteria] || ""
                    elide: Text.ElideRight
                    font.pixelSize: VLCStyle.fontSize_normal
                    color: (model.isPrimary)? VLCStyle.colors.text : VLCStyle.colors.textDisabled

                    anchors {
                        fill: parent
                        leftMargin: VLCStyle.margin_xsmall
                        rightMargin: VLCStyle.margin_xsmall
                    }
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignLeft
                }
            }
        }
    }

    headerColor: VLCStyle.colors.bg
    spacing: VLCStyle.margin_small

    onActionForSelection: medialib.addAndPlay(model.getIdsForIndexes( selection ))

    navigationLeft:  function(index) {
        if (isFocusOnContextButton )
            isFocusOnContextButton = false
        else
            defaultNavigationLeft(index)
    }
    navigationRight: function(index) {
        if (!isFocusOnContextButton)
            isFocusOnContextButton = true
        else
            defaultNavigationRight(index)
    }
}
