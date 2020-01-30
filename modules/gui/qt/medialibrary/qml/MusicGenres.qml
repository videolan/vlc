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

import "qrc:///util/" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root
    property alias model: delegateModel.model
    property var sortModel: [
        { text: i18n.qtr("Alphabetic"), criteria: "title" }
    ]

    readonly property var currentIndex: view.currentItem.currentIndex
    //the index to "go to" when the view is loaded
    property var initialIndex: 0

    onInitialIndexChanged:  resetFocus()

    navigationCancel: function() {
        if (view.currentItem.currentIndex <= 0)
            defaultNavigationCancel()
        else
            view.currentItem.currentIndex = 0;
    }

    Component.onCompleted: loadView()

    function loadView() {
        if (medialib.gridView) {
            view.replace(gridComponent)
        } else {
            view.replace(listComponent)
        }
    }

    function showAlbumView( parent, name ) {
        history.push([ "mc", "music", "genres", "albums", { parentId: parent, genreName: name } ])
    }

    function resetFocus() {
        if (delegateModel.items.count === 0) {
            return
        }
        var initialIndex = root.initialIndex
        if (initialIndex >= delegateModel.items.count)
            initialIndex = 0
        delegateModel.selectNone()
        delegateModel.items.get(initialIndex).inSelected = true
        view.currentItem.currentIndex = initialIndex
        view.currentItem.positionViewAtIndex(initialIndex, ItemView.Contain)
    }

    Connections {
        target: medialib
        onGridViewChanged: loadView()
    }

    Component {
        id: headerComponent
        Widgets.LabelSeparator {
            text: i18n.qtr("Genres")
            width: root.width
        }
    }

    Util.SelectableDelegateModel {
        id: delegateModel
        model: MLGenreModel {
            ml: medialib
        }

        delegate: Package {
            Widgets.ListItem {
                Package.name: "list"

                width: root.width
                height: VLCStyle.icon_normal + VLCStyle.margin_small

                cover: Image {
                    id: cover_obj
                    fillMode: Image.PreserveAspectFit
                    source: model.cover || VLCStyle.noArtAlbum
                    sourceSize: Qt.size(width, height)
                }

                line1: (model.name || "Unknown genre")+" - "+model.nb_tracks+" tracks"

                onItemClicked: {
                    delegateModel.updateSelection( modifier, view.currentItem.currentIndex, index )
                    view.currentItem.currentIndex = index
                    this.forceActiveFocus()
                }
                onPlayClicked: {
                    medialib.addAndPlay( model.id )
                }
                onItemDoubleClicked: {
                    root.showAlbumView(model.id, model.name)
                }
                onAddToPlaylistClicked: {
                    medialib.addToPlaylist( model.id );
                }
            }
        }

        onCountChanged: {
            if (delegateModel.items.count > 0 && delegateModel.selectedGroup.count === 0) {
                root.resetFocus()
            }
        }

        function actionAtIndex(index) {
            if (delegateModel.selectedGroup.count > 1) {
                var list = []
                for (var i = 0; i < delegateModel.selectedGroup.count; i++)
                    list.push(delegateModel.selectedGroup.get(i).model.id)
                medialib.addAndPlay( list )
            } else if (delegateModel.selectedGroup.count === 1) {
                showAlbumView(delegateModel.selectedGroup.get(0).model.id, delegateModel.selectedGroup.get(0).model.name)
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
        Widgets.ExpandGridView {
            id: gridView_id

            model: delegateModel
            modelCount: delegateModel.items.count

            headerDelegate: headerComponent

            delegate: AudioGridItem {
                id: gridItem

                image: model.cover || VLCStyle.noArtAlbum
                title: model.name || "Unknown genre"
                subtitle: ""
                //selected: element.DelegateModel.inSelected

                onItemClicked: {
                    delegateModel.updateSelection( modifier , view.currentItem.currentIndex, index)
                    view.currentItem.currentIndex = index
                    view.currentItem.forceActiveFocus()
                }

                onItemDoubleClicked: root.showAlbumView(model.id, model.name)
            }

            focus: true

            cellWidth: VLCStyle.gridItem_music_width
            cellHeight: VLCStyle.gridItem_music_height

            onSelectAll: delegateModel.selectAll()
            onSelectionUpdated:  delegateModel.updateSelection( keyModifiers, oldIndex, newIndex )
            onActionAtIndex: {
                delegateModel.actionAtIndex(index)
            }

            navigationParent: root
        }
    }

    Component {
        id: listComponent
        /* List View */
        Widgets.KeyNavigableListView {
            id: listView_id

            model: delegateModel.parts.list
            modelCount: delegateModel.items.count

            header: headerComponent

            focus: true
            spacing: VLCStyle.margin_xxxsmall

            onSelectAll: delegateModel.selectAll()
            onSelectionUpdated: delegateModel.updateSelection( keyModifiers, oldIndex, newIndex )
            onActionAtIndex: delegateModel.actionAtIndex(index)

            navigationParent: root
        }
    }

    Widgets.StackViewExt {
        id: view

        initialItem: medialib.gridView ? gridComponent : listComponent

        anchors.fill: parent
        focus: true
    }

    Label {
        anchors.fill: parent
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        visible: delegateModel.items.count === 0
        font.pixelSize: VLCStyle.fontHeight_xxlarge
        color: root.activeFocus ? VLCStyle.colors.accent : VLCStyle.colors.text
        wrapMode: Text.WordWrap
        text: i18n.qtr("No genres found\nPlease try adding sources, by going to the Network tab")
    }
}
