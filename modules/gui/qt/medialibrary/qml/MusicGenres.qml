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
    property alias model: genreModel
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

    function showAlbumView( m ) {
        history.push([ "mc", "music", "genres", "albums", { parentId: m.id, genreName: m.name } ])
    }

    function resetFocus() {
        if (genreModel.count === 0) {
            return
        }
        var initialIndex = root.initialIndex
        if (initialIndex >= genreModel.count)
            initialIndex = 0
        selectionModel.select(genreModel.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect)
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
    MLGenreModel {
        id: genreModel
        ml: medialib

        onCountChanged: {
            if (genreModel.count > 0 && !selectionModel.hasSelection) {
                root.resetFocus()
            }
        }
    }

    function _actionAtIndex(index) {
        if (selectionModel.selectedIndexes.length > 1) {
            medialib.addAndPlay(model.getIdsForIndexes(selectionModel.selectedIndexes))
        } else if (selectionModel.selectedIndexes.length === 1) {
            var sel = selectionModel.selectedIndexes[0]
            showAlbumView( genreModel.getDataAt(sel) )
        }
    }

    Util.SelectableDelegateModel {
        id: selectionModel

        model: genreModel
    }

    /*
     *define the intial position/selection
     * This is done on activeFocus rather than Component.onCompleted because selectionModel.
     * selectedGroup update itself after this event
     */
    onActiveFocusChanged: {
        if (activeFocus && genreModel.count > 0 && !selectionModel.hasSelection) {
            var initialIndex = 0
            if (view.currentItem.currentIndex !== -1)
                initialIndex = view.currentItem.currentIndex
            selectionModel.select(genreModel.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect)
            view.currentItem.currentIndex = initialIndex
        }
    }

    /* Grid View */
    Component {
        id: gridComponent
        Widgets.ExpandGridView {
            id: gridView_id

            delegateModel: selectionModel
            model: genreModel

            headerDelegate: headerComponent

            delegate: AudioGridItem {
                id: gridItem

                image: model.cover || VLCStyle.noArtAlbum
                title: model.name || "Unknown genre"
                subtitle: ""

                onItemClicked: {
                    selectionModel.updateSelection( modifier , view.currentItem.currentIndex, index)
                    view.currentItem.currentIndex = index
                    view.currentItem.forceActiveFocus()
                }

                onItemDoubleClicked: root.showAlbumView(model)
            }

            focus: true

            cellWidth: VLCStyle.gridItem_music_width
            cellHeight: VLCStyle.gridItem_music_height

            onSelectAll: selectionModel.selectAll()
            onSelectionUpdated:  selectionModel.updateSelection( keyModifiers, oldIndex, newIndex )
            onActionAtIndex: _actionAtIndex(index)

            navigationParent: root
        }
    }

    Component {
        id: listComponent
        /* List View */
        Widgets.KeyNavigableListView {
            id: listView_id

            model: genreModel

            header: headerComponent

            focus: true
            spacing: VLCStyle.margin_xxxsmall

            onSelectAll: selectionModel.selectAll()
            onSelectionUpdated: selectionModel.updateSelection( keyModifiers, oldIndex, newIndex )
            onActionAtIndex: _actionAtIndex(index)

            navigationParent: root

            delegate: Widgets.ListItem {
                id: listDelegate

                width: root.width
                height: VLCStyle.icon_normal + VLCStyle.margin_small
                selected: selectionModel.isSelected(genreModel.index(index, 0))
                Connections {
                   target: selectionModel
                   onSelectionChanged: listDelegate.selected = selectionModel.isSelected(genreModel.index(index, 0))
                }


                cover: Image {
                    id: cover_obj
                    fillMode: Image.PreserveAspectFit
                    source: model.cover || VLCStyle.noArtAlbum
                    sourceSize: Qt.size(width, height)
                }

                line1: (model.name || "Unknown genre")+" - "+model.nb_tracks+" tracks"

                onItemClicked: {
                    selectionModel.updateSelection( modifier, view.currentItem.currentIndex, index )
                    view.currentItem.currentIndex = index
                    this.forceActiveFocus()
                }
                onPlayClicked: {
                    medialib.addAndPlay( model.id )
                }
                onItemDoubleClicked: {
                    root.showAlbumView(model)
                }
                onAddToPlaylistClicked: {
                    medialib.addToPlaylist( model.id );
                }
            }
        }
    }

    Widgets.StackViewExt {
        id: view

        initialItem: medialib.gridView ? gridComponent : listComponent

        anchors.fill: parent
        focus: genreModel.count !== 0
    }

    EmptyLabel {
        anchors.fill: parent
        visible: genreModel.count === 0
        focus: visible
        text: i18n.qtr("No genres found\nPlease try adding sources, by going to the Network tab")
        navigationParent: root
    }
}
