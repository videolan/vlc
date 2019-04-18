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
import QtQml.Models 2.2
import org.videolan.medialib 0.1


import "qrc:///utils/" as Utils
import "qrc:///style/"

Utils.NavigableFocusScope {
    id: root

    property var sortModel: ListModel {
        ListElement { text: qsTr("Alphabetic");  criteria: "title";}
        ListElement { text: qsTr("Duration");    criteria: "duration"; }
        ListElement { text: qsTr("Date");        criteria: "release_year"; }
        ListElement { text: qsTr("Artist");      criteria: "main_artist"; }
    }

    property alias model: delegateModel.model
    property alias parentId: delegateModel.parentId
    property var currentIndex: view.currentItem.currentIndex

    Utils.SelectableDelegateModel {
        id: delegateModel
        property alias parentId: albumModelId.parentId

        model: MLAlbumModel {
            id: albumModelId
            ml: medialib
        }


        delegate: Package {
            id: element

            Utils.ListItem {
                Package.name: "list"
                width: root.width
                height: VLCStyle.icon_normal

                color: VLCStyle.colors.getBgColor(element.DelegateModel.inSelected, this.hovered, this.activeFocus)

                cover: Image {
                    id: cover_obj
                    fillMode: Image.PreserveAspectFit
                    source: model.cover || VLCStyle.noArtAlbum
                    sourceSize: Qt.size(width, height)
                }
                line1: (model.title || qsTr("Unknown title"))+" ["+model.duration+"]"
                line2: model.main_artist || qsTr("Unknown artist")

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
            if (delegateModel.selectedGroup.count > 1) {
                var list = []
                for (var i = 0; i < delegateModel.selectedGroup.count; i++)
                    list.push(delegateModel.selectedGroup.get(i).model.id)
                medialib.addAndPlay( list )
            } else {
                medialib.addAndPlay( delegateModel.items.get(index).model.id )
            }
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

        Utils.ExpandGridView {
            id: gridView_id

            activeFocusOnTab:true

            cellWidth: VLCStyle.cover_normal + VLCStyle.margin_small
            cellHeight: VLCStyle.cover_normal + VLCStyle.fontHeight_normal * 2 + VLCStyle.margin_small

            gridDelegate: Utils.GridItem {
                property variant delegateModelItem: ({
                    model: ({}),
                    itemsIndex: 0,
                    inSelected: false
                })

                shiftX: view.currentItem.shiftX(delegateModelItem.itemsIndex)
                image: delegateModelItem.model.cover || VLCStyle.noArtAlbum
                title: delegateModelItem.model.title || qsTr("Unknown title")
                subtitle: delegateModelItem.model.main_artist || qsTr("Unknown artist")
                selected: delegateModelItem.inSelected
                onItemClicked : {
                    delegateModel.updateSelection( modifier , view.currentItem.currentIndex, delegateModelItem.itemsIndex)
                    view.currentItem.currentIndex = delegateModelItem.itemsIndex
                    //view.currentItem.forceActiveFocus()
                    view._switchExpandItem( delegateModelItem.itemsIndex )

                }
                onPlayClicked: medialib.addAndPlay( delegateModelItem.model.id )
                onAddToPlaylistClicked : medialib.addToPlaylist( delegateModelItem.model.id )
            }

            expandDelegate:  Rectangle {
                id: expandDelegateId
                implicitHeight: albumDetail.implicitHeight
                width: root.width
                color: VLCStyle.colors.bgAlt
                property int currentId: -1
                property alias model : albumDetail.model
                property alias currentItemY: albumDetail.currentItemY
                property alias currentItemHeight: albumDetail.currentItemHeight

                onActiveFocusChanged: {
                    if (activeFocus)
                        albumDetail.forceActiveFocus()
                }

                MusicAlbumsGridExpandDelegate {
                    id: albumDetail
                    anchors.fill: parent
                    onActionCancel:  gridView_id.retract()
                    onActionUp:  gridView_id.retract()
                    onActionDown: gridView_id.retract()
                    onActionLeft: root.actionLeft(index)
                    onActionRight: root.actionRight(index)
                }
            }

            model: delegateModel
            modelTop: delegateModel.parts.gridTop
            modelCount: delegateModel.items.count

            onActionAtIndex: {
                if (delegateModel.selectedGroup.count === 1) {
                    view._switchExpandItem(index)
                } else {
                    delegateModel.actionAtIndex(index)
                }
            }
            onSelectAll: delegateModel.selectAll()
            onSelectionUpdated: delegateModel.updateSelection( keyModifiers, oldIndex, newIndex )

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

            interactive: root.interactive

            spacing: VLCStyle.margin_xxxsmall

            model: delegateModel.parts.list
            modelCount: delegateModel.items.count

            onActionAtIndex: delegateModel.actionAtIndex(index)
            onSelectAll: delegateModel.selectAll()
            onSelectionUpdated: delegateModel.updateSelection( keyModifiers, oldIndex, newIndex )

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

        function _switchExpandItem(index) {
            view.currentItem.switchExpandItem(index)

            /*if (view.currentItem.expandIndex === index)
                view.currentItem.expandIndex = -1
            else
                view.currentItem.expandIndex = index*/
        }
    }

    Label {
        anchors.centerIn: parent
        visible: delegateModel.items.count === 0
        font.pixelSize: VLCStyle.fontHeight_xxlarge
        color: root.activeFocus ? VLCStyle.colors.accent : VLCStyle.colors.text
        text: qsTr("No albums found")
    }
}
