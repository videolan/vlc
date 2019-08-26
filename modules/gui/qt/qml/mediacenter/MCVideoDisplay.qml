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
import "qrc:///dialogs/" as DG
import "qrc:///style/"

Utils.NavigableFocusScope {
    id: root
    property Item videosGridView: Item{}
    property Item currentGridView: Item{}
    DG.ModalDialog {
        id: deleteDialog
        rootWindow: root
        title: qsTr("Are you sure you want to delete?")
        standardButtons: Dialog.Yes | Dialog.No

        onAccepted: console.log("Ok clicked")
        onRejected: console.log("Cancel clicked")
    }

    Utils.MenuExt {
           id: contextMenu
           property var model: ({})
           closePolicy: Popup.CloseOnReleaseOutside | Popup.CloseOnEscape

           Utils.MenuItemExt {
               id: playMenuItem
               text: "Play from start"
               onTriggered: medialib.addAndPlay( contextMenu.model.id )
           }
           Utils.MenuItemExt {
               text: "Play all"
               onTriggered: console.log("not implemented")
           }
           Utils.MenuItemExt {
               text: "Play as audio"
               onTriggered: console.log("not implemented")
           }
           Utils.MenuItemExt {
               text: "Enqueue"
               onTriggered: medialib.addToPlaylist( contextMenu.model.id )
           }
           Utils.MenuItemExt {
               enabled: root.currentGridView && root.currentGridView.switchExpandItem !== undefined
               text: "Information"
               onTriggered: {
                   root.currentGridView.switchExpandItem(contextMenu.model.index,root.currentGridView.currentItem)
               }
           }
           Utils.MenuItemExt {
               text: "Download subtitles"
               onTriggered: console.log("not implemented")
           }
           Utils.MenuItemExt {
               text: "Add to playlist"
               onTriggered: console.log("not implemented")
           }
           Utils.MenuItemExt {
               text: "Delete"
               onTriggered: deleteDialog.open()
           }

           onClosed: contextMenu.parent.forceActiveFocus()

       }
    Utils.SelectableDelegateModel {
        id: videosDelegate

        model: MLVideoModel {
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
                    fillMode: Image.PreserveAspectCrop
                    source: model.thumbnail || VLCStyle.noArtCover
                }
                line1: (model.title || qsTr("Unknown title"))+" ["+model.duration+"]"

                onItemClicked : {
                    videosDelegate.updateSelection( modifier, view.currentItem.currentIndexVideos, index )
                    view.currentItem.currentIndexVideos = index
                    this.forceActiveFocus()
                }
                onPlayClicked: medialib.addAndPlay( model.id )
                onAddToPlaylistClicked : medialib.addToPlaylist( model.id )
            }
        }
        function actionAtIndex(index) {
            var list = []
            for (var i = 0; i < videosDelegate.selectedGroup.count; i++)
                list.push(videosDelegate.selectedGroup.get(i).model.id)
            medialib.addAndPlay( list )
        }
    }

    Component {
        id: gridComponent
        Flickable{
            id: flickable
            height: view.height
            width: view.width
            contentHeight: allSections.implicitHeight
            ScrollBar.vertical: ScrollBar{}
            onActiveFocusChanged: {
            if(activeFocus)
                videosGV.forceActiveFocus()
            }


            property int currentIndexRecents: -1
            property int currentIndexVideos: -1

            Rectangle {
                id: allSections
                color: "transparent"
                implicitHeight: childrenRect.height
                implicitWidth: view.width
                anchors {
                    left: parent.left
                    right: parent.right
                }

            Rectangle {
                id: recentsSection
                anchors {
                    left: parent.left
                    right: parent.right
                }
                implicitHeight: visible ? childrenRect.height: 0
                color: "transparent"
                enabled: visible
                visible: recentsDelegate.items.count > 0

                Utils.LabelSeparator {
                    id: recentsSeparator
                    text: qsTr("Recents")
                }

                Rectangle {
                    color: "transparent"
                    anchors.top: recentsSeparator.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: recentsGV.contentHeight

                    Utils.SelectableDelegateModel {
                        id: recentsDelegate
                       model: MLRecentsVideoModel {
                           ml: medialib
                       }

                        delegate: Package {
                            id: recentsElement
                            Utils.GridItem {
                                Package.name: "grid"
                                image: model.thumbnail || VLCStyle.noArtCover
                                title: model.title || qsTr("Unknown title")
                                selected: recentsGV.activeFocus && recentsElement.DelegateModel.inSelected
                                infoLeft: model.duration
                                resolution: model.resolution_name
                                channel: model.channel
                                isVideo: true
                                isNew: model.playcount < 1
                                progress: model.saved_position > 0 ? model.saved_position : 0
                                pictureWidth: VLCStyle.video_large_width
                                pictureHeight: VLCStyle.video_large_height
                                onItemClicked : {
                                    if (key == Qt.RightButton){
                                        contextMenu.model = model
                                        contextMenu.popup(menuParent)
                                    }
                                    recentsDelegate.updateSelection( modifier , view.currentItem.currentIndexRecents, index)
                                    view.currentItem.currentIndexRecents = index
                                    root.currentGridView = recentsGV
                                    root.currentGridView.currentIndex = index
                                    root.currentGridView.forceActiveFocus()
                                }
                                onPlayClicked: medialib.addAndPlay( model.id )
                                onAddToPlaylistClicked : medialib.addToPlaylist( model.id )
                                onContextMenuButtonClicked:{
                                    contextMenu.model = model;
                                    contextMenu.popup(menuParent,contextMenu.width,0,playMenuItem)
                                }
                                onSelectedChanged:{
                                    if(selected)
                                        root.currentGridView = recentsGV
                                }

                            }

                        }
                        function actionAtIndex(index) {
                            var list = []
                            for (var i = 0; i < recentsDelegate.selectedGroup.count; i++)
                                list.push(recentsDelegate.selectedGroup.get(i).model.id)
                            medialib.addAndPlay( list )
                        }
                    }

                    Utils.KeyNavigableListView {
                        id: recentsGV
                        anchors.fill:parent
                        anchors.leftMargin: leftBtn.width/2
                        anchors.rightMargin: rightBtn.width/2

                        model: recentsDelegate.parts.grid
                        modelCount: recentsDelegate.items.count
                        orientation: ListView.Horizontal

                        onSelectAll: recentsDelegate.selectAll()
                        onSelectionUpdated: recentsDelegate.updateSelection( keyModifiers, oldIndex, newIndex )
                        onActionAtIndex: recentsDelegate.actionAtIndex(index)

                        onActionLeft: root.actionLeft(index)
                        onActionRight: root.actionRight(index)
                        onActionDown: videosGV.forceActiveFocus()
                        onActionUp: root.actionUp(index)
                        onActionCancel: root.actionCancel(index)

                        /*
                         *define the intial position/selection
                         * This is done on activeFocus rather than Component.onCompleted because recentsDelegate.
                         * selectedGroup update itself after this event
                         */
                        onActiveFocusChanged: {
                            if (activeFocus && recentsDelegate.items.count > 0 && recentsDelegate.selectedGroup.count === 0) {
                                var initialIndex = 0
                                if (view.currentItem.currentIndexRecents !== -1)
                                    initialIndex = view.currentItem.currentIndexRecents
                                recentsDelegate.items.get(initialIndex).inSelected = true
                                view.currentItem.currentIndexRecents = initialIndex
                            }
                            if (activeFocus)
                                flickable.ScrollBar.vertical.position = 0

                        }

                    }


                    Utils.RoundButton{
                        id: leftBtn
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        text:"<"
                        onClicked: recentsGV.prevPage()
                    }


                    Utils.RoundButton{
                        id: rightBtn
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        text:">"
                        onClicked: recentsGV.nextPage()
                    }
               }
            }


            Rectangle {
                id: videosSection
                anchors {
                    left: parent.left
                    right: parent.right
                    top: recentsSection.bottom
                }
                implicitHeight: childrenRect.height
                color: "transparent"

                Utils.LabelSeparator {
                    id: videosSeparator
                    text: qsTr("Videos")
                }
                Rectangle {
                    color: VLCStyle.colors.bg
                    anchors.top: videosSeparator.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: videosGV.contentHeight
                    VideoExpandableGrid {
                        id: videosGV
                        Component.onCompleted: root.videosGridView = videosGV
                        property Item currentItem: Item{}

                        activeFocusOnTab:true
                        anchors.fill: parent
                        flickableDirection:  Flickable.VerticalFlick
                        model: videosDelegate
                        modelCount: videosDelegate.items.count

                        expandDelegateImplicitHeight: view.height/3
                        expandDelegateWidth: view.width


                        onGridItemClicked: {
                            if (key == Qt.RightButton){
                                contextMenu.model = delegateModelItem.model
                                contextMenu.popup(menuParent)
                            }
                            videosDelegate.updateSelection( modifier , view.currentItem.currentIndexVideos, delegateModelItem.itemsIndex)
                            view.currentItem.currentIndexVideos = delegateModelItem.itemsIndex
                            root.currentGridView = videosGV
                            root.currentGridView.currentIndex = delegateModelItem.itemsIndex
                            root.currentGridView.forceActiveFocus()

                            videosGV.renderLayout()
                        }
                        onGridItemContextButtonClicked: {
                            contextMenu.model = delegateModelItem.model;
                            contextMenu.popup(menuParent,contextMenu.width,0,contextMenu.playMenuItem)
                        }
                        onGridItemSelectedChanged: {
                            if(selected){
                                root.currentGridView = videosGV
                                videosGV.currentItem = item

                                if (videosSection.y + videosGV.currentItem.y + videosGV.currentItem.height > flickable.contentY + flickable.height - videosSection.y ||
                                       videosSection.y + videosGV.currentItem.y < flickable.contentY)

                                    flickable.contentY = ((view.height + videosGV.currentItem.y) > flickable.contentHeight) ?
                                                flickable.contentHeight-view.height : videosSection.y + videosGV.currentItem.y
                            }
                    }

                        onActionLeft: root.actionLeft(index)
                        onActionRight: root.actionRight(index)
                        onActionDown: root.actionDown(index)
                        onActionUp: {
                            if (recentsSection.visible)
                                recentsGV.forceActiveFocus()
                            else
                                root.actionUp(index)
                        }
                        onActionCancel: root.actionCancel(index)

                        /*
                         *define the intial position/selection
                         * This is done on activeFocus rather than Component.onCompleted because videosDelegate.
                         * selectedGroup update itself after this event
                         */
                        onActiveFocusChanged: {
                            if (activeFocus && videosDelegate.items.count > 0 && videosDelegate.selectedGroup.count === 0) {
                                var initialIndex = 0
                                if (view.currentItem.currentIndexVideos !== -1)
                                    initialIndex = view.currentItem.currentIndexVideos
                                videosDelegate.items.get(initialIndex).inSelected = true
                                view.currentItem.currentIndexVideos = initialIndex
                            }
                        }

                    }

                }
            }
        }
        }
    }

    Component {
        id: listComponent
        MCVideoListDisplay {
            height: view.height
            width: view.width
            onContextMenuButtonClicked:{
                contextMenu.model = menuModel
                contextMenu.popup(menuParent,contextMenu.width,0)
            }
            onRightClick:{
                contextMenu.model = menuModel
                contextMenu.popup(menuParent)
            }
        }
    }

        Utils.StackViewExt {
            id: view
            anchors.fill:parent
            clip: true
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
        visible: videosDelegate.items.count === 0
        font.pixelSize: VLCStyle.fontHeight_xxlarge
        color: root.activeFocus ? VLCStyle.colors.accent : VLCStyle.colors.text
        text: qsTr("No tracks found")
    }
}
