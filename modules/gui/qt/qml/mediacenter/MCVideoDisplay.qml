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

import "qrc:///utils/" as Utils
import "qrc:///style/"

Utils.NavigableFocusScope {
    id: root

    property alias model: delegateModel.model

    Utils.SelectableDelegateModel {
        id: delegateModel
        model: MLVideoModel {
            ml: medialib
        }

        delegate: Package {
            id: element
            Utils.GridItem {
                Package.name: "grid"
                focus: true
                image: model.thumbnail || VLCStyle.noArtCover
                title: model.title || qsTr("Unknown title")
                selected: element.DelegateModel.inSelected || view.currentItem.currentIndex === index
                shiftX: view.currentItem.shiftX(model.index)

                onItemClicked : {
                    delegateModel.updateSelection( modifier , view.currentItem.currentIndex, index)
                    view.currentItem.currentIndex = index
                    view.currentItem.forceActiveFocus()
                }
                onPlayClicked: medialib.addAndPlay( model.id )
                onAddToPlaylistClicked : medialib.addToPlaylist( model.id )
            }

            Utils.ListItem {
                Package.name: "list"
                width: root.width
                height: VLCStyle.icon_normal
                focus: true

                color: VLCStyle.colors.getBgColor(element.DelegateModel.inSelected, this.hovered, this.activeFocus)

                cover: Image {
                    id: cover_obj
                    fillMode: Image.PreserveAspectCrop
                    source: model.thumbnail || VLCStyle.noArtCover
                }
                line1: (model.title || qsTr("Unknown title"))+" ["+model.duration+"]"

                onItemClicked : {
                    delegateModel.updateSelection( modifier, view.currentItem.currentIndex, index )
                    view.currentItem.currentIndex = index
                    this.forceActiveFocus()
                }
                onPlayClicked: medialib.addAndPlay( model.id )
                onAddToPlaylistClicked : medialib.addToPlaylist( model.id )
            }
        }
        function actionAtIndex(index) {
            var list = []
            for (var i = 0; i < delegateModel.selectedGroup.count; i++)
                list.push(delegateModel.selectedGroup.get(i).model.id)
            medialib.addAndPlay( list )
        }
    }

    /*
     *define the intial position/selection
     * This is done on activeFocus rather than Component.onCompleted because delegateModel.
     * selectedGroup update itself after this event
     */
    onActiveFocusChanged: {
        if (activeFocus && delegateModel.items.count > 0 && delegateModel.selectedGroup.count === 0) {
            var initialIndex = 0
            if (view.currentItem.currentIndex !== -1)
                initialIndex = view.currentItem.currentIndex
            delegateModel.items.get(initialIndex).inSelected = true
            view.currentItem.currentIndex = initialIndex
        }
    }

    Component {
        id: gridComponent

        Utils.KeyNavigableGridView {
            id: gridView_id

            model: delegateModel.parts.grid
            modelCount: delegateModel.items.count

            focus: true

            cellWidth: VLCStyle.cover_normal + VLCStyle.margin_small
            cellHeight: VLCStyle.cover_normal + VLCStyle.fontHeight_normal + VLCStyle.margin_small

            onSelectAll: delegateModel.selectAll()
            onSelectionUpdated: delegateModel.updateSelection( keyModifiers, oldIndex, newIndex )
            onActionAtIndex: delegateModel.actionAtIndex(index)

            onActionLeft: root.actionLeft(index)
            onActionRight: root.actionRight(index)
            onActionDown: root.actionDown(index)
            onActionUp: root.actionUp(index)
            onActionCancel: root.actionCancel(index)
        }
    }

    Component {
        id: listComponent
        /* ListView */
        Utils.KeyNavigableListView {
            id: listView_id

            model: delegateModel.parts.list
            modelCount: delegateModel.items.count

            focus: true
            spacing: VLCStyle.margin_xxxsmall

            onSelectAll: delegateModel.selectAll()
            onSelectionUpdated: delegateModel.updateSelection( keyModifiers, oldIndex, newIndex )
            onActionAtIndex: delegateModel.actionAtIndex(index)

            onActionLeft: root.actionLeft(index)
            onActionRight: root.actionRight(index)
            onActionDown: root.actionDown(index)
            onActionUp: root.actionUp(index)
            onActionCancel: root.actionCancel(index)
        }
    }

    Utils.StackViewExt {
        id: view

        anchors.fill: parent
        focus: true

        initialItem: medialib.gridView ? gridComponent : listComponent

        Connections {
            target: medialib
            onGridViewChanged: {
                if (medialib.gridView)
                    view.replace(gridComponent)
                else
                    view.replace(listComponent)
            }
        }
    }

    Label {
        anchors.centerIn: parent
        visible: delegateModel.items.count === 0
        font.pixelSize: VLCStyle.fontHeight_xxlarge
        color: root.activeFocus ? VLCStyle.colors.accent : VLCStyle.colors.text
        text: qsTr("No tracks found")
    }
}
