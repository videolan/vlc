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
import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///utils/" as Utils
import "qrc:///style/"

Utils.NavigableFocusScope {
    id: root
    property alias model: delegateModel.model
    property var sortModel: ListModel {
        ListElement { text: qsTr("Alphabetic"); criteria: "title" }
    }

    function goToView( parent ) {
        history.push([ "mc", "music", "albums", { parentId: parent } ], History.Go)
    }

    Utils.SelectableDelegateModel {
        id: delegateModel
        model: MLGenreModel {
            ml: medialib
        }

        delegate: Package {
            id: element
            Utils.GridItem {
                Package.name: "grid"
                id: gridItem
                image: VLCStyle.noArtCover
                title: model.name || "Unknown genre"
                selected: element.DelegateModel.inSelected

                shiftX: view.currentItem.shiftX(model.index)

                onItemClicked: {
                    delegateModel.updateSelection( modifier , view.currentItem.currentIndex, index)
                    view.currentItem.currentIndex = index
                    view.currentItem.forceActiveFocus()
                }
                onPlayClicked: {
                    medialib.addAndPlay( model.id )
                }
                onItemDoubleClicked: {
                    history.push(["mc", "music", "albums", { parentId: model.id } ], History.Go)
                }
                onAddToPlaylistClicked: {
                    medialib.addToPlaylist( model.id );
                }

                //replace image with a mutlicovers preview
                Utils.MultiCoverPreview {
                    id: multicover
                    visible: false
                    width: VLCStyle.cover_normal
                    height: VLCStyle.cover_normal

                    albums: MLAlbumModel {
                        ml: medialib
                        parentId: model.id
                    }
                }

                Component.onCompleted: {
                    multicover.grabToImage(function(result) {
                        gridItem.image = result.url
                        //multicover.destroy()
                    })
                }
            }

            Utils.ListItem {
                Package.name: "list"
                width: root.width
                height: VLCStyle.icon_normal

                color: VLCStyle.colors.getBgColor(element.DelegateModel.inSelected, this.hovered, this.activeFocus)

                cover:  Utils.MultiCoverPreview {
                    albums: MLAlbumModel {
                        ml: medialib
                        parentId: model.id
                    }
                }

                line1: (model.name || "Unknown genre")+" - "+model.nb_tracks+" tracks"

                onItemClicked: {
                    console.log("Clicked on : "+model.name);
                    delegateModel.updateSelection( modifier, view.currentItem.currentIndex, index )
                    view.currentItem.currentIndex = index
                    this.forceActiveFocus()
                }
                onPlayClicked: {
                    console.log('Clicked on play : '+model.name);
                    medialib.addAndPlay( model.id )
                }
                onItemDoubleClicked: {
                    history.push([ "mc", "music", "albums", { parentId: model.id } ], History.Go)
                }
                onAddToPlaylistClicked: {
                    console.log('Clicked on addToPlaylist : '+model.name);
                    medialib.addToPlaylist( model.id );
                }
            }
        }

        function actionAtIndex(index) {
            if (delegateModel.selectedGroup.count > 1) {
                var list = []
                for (var i = 0; i < delegateModel.selectedGroup.count; i++)
                    list.push(delegateModel.selectedGroup.get(i).model.id)
                medialib.addAndPlay( list )
            } else if (delegateModel.selectedGroup.count === 1) {
                goToView(delegateModel.selectedGroup.get(0).model.id)
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

    /* Grid View */
    Component {
        id: gridComponent
        Utils.KeyNavigableGridView {
            id: gridView_id

            model: delegateModel.parts.grid
            modelCount: delegateModel.items.count

            focus: true

            cellWidth: (VLCStyle.cover_normal) + VLCStyle.margin_small
            cellHeight: (VLCStyle.cover_normal + VLCStyle.fontHeight_normal) + VLCStyle.margin_small

            onSelectAll: delegateModel.selectAll()
            onSelectionUpdated:  delegateModel.updateSelection( keyModifiers, oldIndex, newIndex )
            onActionAtIndex: delegateModel.actionAtIndex(index)

            onActionLeft: root.actionLeft(index)
            onActionRight: root.actionRight(index)
            onActionUp: root.actionUp(index)
            onActionDown: root.actionDown(index)
            onActionCancel: root.actionCancel(index)
        }
    }


    Component {
        id: listComponent
        /* List View */
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
            onActionUp: root.actionUp(index)
            onActionDown: root.actionDown(index)
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
}
