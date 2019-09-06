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
import "qrc:///dialogs/" as DG
import "qrc:///style/"

Utils.NavigableFocusScope {
    id: root
    property var currentIndex: view.currentItem.currentIndex

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
            enabled: medialib.gridView
            text: "Information"
            onTriggered: {
                view.currentItem.switchExpandItem(contextMenu.model.index, view.currentItem.currentItem)
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
        delegate: Package{
            Item { Package.name: "grid" }
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

        VideoExpandableGrid {
            id: videosGV
            property Item currentItem: Item{}

            activeFocusOnTab:true
            model: videosDelegate
            modelCount: videosDelegate.items.count

            headerDelegate: Utils.LabelSeparator {
                id: videosSeparator
                width: videosGV.width
                text: qsTr("Videos")
            }


            expandDelegateImplicitHeight: view.height/3
            expandDelegateWidth: view.width

            delegate: VideoGridItem {
                id: videoGridItem

                onItemClicked : {
                    if (key == Qt.RightButton){
                        contextMenu.model = videoGridItem.model
                        contextMenu.popup(menuParent)
                    }
                    videosDelegate.updateSelection( modifier , videosGV.currentIndex, index)
                    videosGV.currentIndex = index
                    videosGV.forceActiveFocus()
                    videosGV.renderLayout()
                }
            }

            navigationParent: root

            /*
                         *define the intial position/selection
                         * This is done on activeFocus rather than Component.onCompleted because videosDelegate.
                         * selectedGroup update itself after this event
                         */
            onActiveFocusChanged: {
                if (activeFocus && videosDelegate.items.count > 0 && videosDelegate.selectedGroup.count === 0) {
                    videosDelegate.items.get(0).inSelected = true
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

            navigationParent: root
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
