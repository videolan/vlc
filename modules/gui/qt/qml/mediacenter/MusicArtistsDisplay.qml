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
import QtQuick.Controls 2.4
import QtQuick 2.11
import QtQml.Models 2.2
import QtQuick.Layouts 1.3

import org.videolan.medialib 0.1

import "qrc:///utils/" as Utils
import "qrc:///style/"

Utils.NavigableFocusScope {
    id: root
    property alias model: delegateModel.model
    property var sortModel: ListModel {
        ListElement { text: qsTr("Alphabetic");  criteria: "title" }
    }

    property int currentArtistIndex: -1
    onCurrentArtistIndexChanged: {
        if (currentArtistIndex == -1)
            view.replace(artistGridComponent)
        else
            view.replace(albumComponent)
    }
    property var artistId: null

    Utils.SelectableDelegateModel {
        id: delegateModel
        model: MLArtistModel {
            ml: medialib
        }
        delegate: Package {
            id: element
            Utils.ListItem {
                Package.name: "list"
                height: VLCStyle.icon_normal
                width: parent.width

                color: VLCStyle.colors.getBgColor(element.DelegateModel.inSelected, this.hovered, this.activeFocus)

                cover: Image {
                    id: cover_obj
                    fillMode: Image.PreserveAspectFit
                    source: model.cover || VLCStyle.noArtCover
                }
                line1: model.name || qsTr("Unknown artist")

                onItemClicked: {
                    currentArtistIndex = index
                    artistId = model.id
                    delegateModel.updateSelection( modifier , artistList.currentIndex, index)
                    artistList.currentIndex = index
                    artistList.forceActiveFocus()
                }

                onItemDoubleClicked: {
                    delegateModel.actionAtIndex(index)
                }

                onPlayClicked: {
                    console.log('Clicked on play : '+model.name);
                    medialib.addAndPlay( model.id )
                }
                onAddToPlaylistClicked: {
                    console.log('Clicked on addToPlaylist : '+model.name);
                    medialib.addToPlaylist( model.id );
                }
            }

            Utils.GridItem {
                Package.name: "grid"
                id: gridItem

                image: VLCStyle.noArtCover
                title: model.name || "Unknown Artist"
                selected: element.DelegateModel.inSelected

                //shiftX: view.currentItem.shiftX(index)

                onItemClicked: {
                    delegateModel.updateSelection( modifier , artistList.currentIndex, index)
                    artistList.currentIndex = index
                    artistList.forceActiveFocus()
                }

                onItemDoubleClicked: {
                    delegateModel.actionAtIndex(index)
                }

                onPlayClicked: {
                    medialib.addAndPlay( model.id )
                }
                onAddToPlaylistClicked: {
                    console.log('Clicked on addToPlaylist : '+model.name);
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
                        maxItems: 4
                    }
                }
                Component.onCompleted: {
                    multicover.grabToImage(function(result) {
                        gridItem.image = result.url
                        //multicover.destroy()
                    })
                }
            }
        }

        function actionAtIndex(index) {
            console.log("actionAtIndex", index)
            if (delegateModel.selectedGroup.count > 1) {
                var list = []
                for (var i = 0; i < delegateModel.selectedGroup.count; i++)
                    list.push(delegateModel.selectedGroup.get(i).model.id)
                medialib.addAndPlay( list )
            } else if (delegateModel.selectedGroup.count === 1) {
                root.artistId =  delegateModel.selectedGroup.get(0).model.id
                root.currentArtistIndex = index
                artistList.currentIndex = index
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
        id: artistGridComponent
        Utils.KeyNavigableGridView {
            cellWidth: (VLCStyle.cover_normal) + VLCStyle.margin_small
            cellHeight: (VLCStyle.cover_normal + VLCStyle.fontHeight_normal)  + VLCStyle.margin_small

            model: delegateModel.parts.grid
            modelCount: delegateModel.items.count

            onSelectAll: delegateModel.selectAll()
            onActionAtIndex: delegateModel.actionAtIndex(index)
            onSelectionUpdated: delegateModel.updateSelection( keyModifiers, oldIndex, newIndex )

            onActionLeft: artistList.focus = true
            onActionRight: root.actionRight(index)
            onActionUp: root.actionUp(index)
            onActionDown: root.actionDown(index)
            onActionCancel: root.actionCancel(index)
        }
    }

    Component {
        id: albumComponent
        // Display selected artist albums

        FocusScope {
            property alias currentIndex: albumSubView.currentIndex
            ColumnLayout {
                anchors.fill: parent
                ArtistTopBanner {
                    id: artistBanner
                    Layout.fillWidth: true
                    focus: false
                    //contentY: albumsView.contentY
                    contentY: 0
                    artist: delegateModel.items.get(currentArtistIndex).model
                }
                MusicAlbumsDisplay {
                    id: albumSubView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    focus: true
                    parentId: artistId
                    onActionLeft: artistList.focus = true

                    onActionRight: root.actionRight(index)
                    onActionUp: root.actionUp(index)
                    onActionDown: root.actionDown(index)
                    onActionCancel: root.actionCancel(index)
                }
            }
        }
    }

    Row {
        anchors.fill: parent
        Utils.KeyNavigableListView {
            width: parent.width * 0.25
            height: parent.height

            id: artistList
            spacing: 2
            model: delegateModel.parts.list
            modelCount: delegateModel.items.count

            onSelectAll: delegateModel.selectAll()
            onSelectionUpdated: delegateModel.updateSelection( keyModifiers, oldIndex, newIndex )
            onActionAtIndex: delegateModel.actionAtIndex(index)

            onActionRight:  view.focus = true
            onActionLeft: root.actionLeft(index)
            onActionUp: root.actionUp(index)
            onActionDown: root.actionDown(index)
            onActionCancel: root.actionCancel(index)
        }

        Utils.StackViewExt {
            id: view
            width: parent.width * 0.75
            height: parent.height
            focus: true

            initialItem: artistGridComponent
        }
    }
}
