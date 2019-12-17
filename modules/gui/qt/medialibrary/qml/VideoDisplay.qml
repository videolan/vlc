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

import "qrc:///util/" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///dialogs/" as DG
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root
    property var currentIndex: view.currentItem.currentIndex

    property alias contentModel: videosDelegate.model;

    navigationCancel: function() {
        if (view.currentItem.currentIndex <= 0)
            defaultNavigationCancel()
        else
            view.currentItem.currentIndex = 0;
    }

    DG.ModalDialog {
        id: deleteDialog
        rootWindow: root
        title: i18n.qtr("Are you sure you want to delete?")
        standardButtons: Dialog.Yes | Dialog.No

        onAccepted: console.log("Ok clicked")
        onRejected: console.log("Cancel clicked")
    }

    Widgets.MenuExt {
        id: contextMenu
        property var model: ({})
        closePolicy: Popup.CloseOnReleaseOutside | Popup.CloseOnEscape

        Widgets.MenuItemExt {
            id: playMenuItem
            text: "Play from start"
            onTriggered: medialib.addAndPlay( contextMenu.model.id )
        }
        Widgets.MenuItemExt {
            text: "Play all"
            onTriggered: console.log("not implemented")
        }
        Widgets.MenuItemExt {
            text: "Play as audio"
            onTriggered: console.log("not implemented")
        }
        Widgets.MenuItemExt {
            text: "Enqueue"
            onTriggered: medialib.addToPlaylist( contextMenu.model.id )
        }
        Widgets.MenuItemExt {
            enabled: medialib.gridView
            text: "Information"
            onTriggered: {
                view.currentItem.switchExpandItem(contextMenu.model.index)
            }
        }
        Widgets.MenuItemExt {
            text: "Download subtitles"
            onTriggered: console.log("not implemented")
        }
        Widgets.MenuItemExt {
            text: "Add to playlist"
            onTriggered: console.log("not implemented")
        }
        Widgets.MenuItemExt {
            text: "Delete"
            onTriggered: deleteDialog.open()
        }

        onClosed: contextMenu.parent.forceActiveFocus()

    }
    Util.SelectableDelegateModel {
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

        Widgets.ExpandGridView {
            id: videosGV
            property Item currentItem: Item{}

            activeFocusOnTab:true
            model: videosDelegate
            modelCount: videosDelegate.items.count

            headerDelegate: Widgets.LabelSeparator {
                id: videosSeparator
                width: videosGV.width
                text: i18n.qtr("Videos")
            }


            delegate: VideoGridItem {
                id: videoGridItem

                onContextMenuButtonClicked: {
                    contextMenu.model = videoGridItem.model
                    contextMenu.popup(menuParent)
                }

                onItemClicked : {
                    videosDelegate.updateSelection( modifier , videosGV.currentIndex, index)
                    videosGV.currentIndex = index
                    videosGV.forceActiveFocus()
                    videosGV.renderLayout()
                }
            }

            expandDelegate: VideoInfoExpandPanel {
                visible: !videosGV.isAnimating

                height: implicitHeight
                width: videosGV.width

                onRetract: videosGV.retract()
                notchPosition: videosGV.getItemPos(videosGV._expandIndex)[0] + (videosGV.cellWidth / 2)

                navigationParent: videosGV
                navigationCancel:  function() {  videosGV.retract() }
                navigationUp: function() {  videosGV.retract() }
                navigationDown: function() { videosGV.retract() }
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

            cellWidth: (VLCStyle.video_normal_width)
            cellHeight: (VLCStyle.video_normal_height) + VLCStyle.margin_xlarge + VLCStyle.margin_normal

            onSelectAll: videosGV.model.selectAll()
            onSelectionUpdated: videosGV.model.updateSelection( keyModifiers, oldIndex, newIndex )
            onActionAtIndex: switchExpandItem( index )
        }

    }


    Component {
        id: listComponent
        VideoListDisplay {
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

    Widgets.StackViewExt {
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
        text: i18n.qtr("No tracks found")
    }
}
