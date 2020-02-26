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

import "qrc:///util/" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root
    property alias model: delegateModel.model
    property var sortModel: [
        { text: i18n.qtr("Alphabetic"),  criteria: "title" }
    ]

    property var artistId

    property alias currentIndex: artistList.currentIndex
    property int initialIndex: 0
    property int initialAlbumIndex: 0

    onInitialAlbumIndexChanged: resetFocus()
    onInitialIndexChanged: resetFocus()

    onCurrentIndexChanged: {
        history.update([ "mc", "music", "artists", {"initialIndex": currentIndex}])
    }

    function resetFocus() {
        if (delegateModel.items.count === 0) {
            return
        }
        var initialIndex = root.initialIndex
        if (initialIndex >= delegateModel.items.count)
            initialIndex = 0
        if (initialIndex !== artistList.currentIndex) {
            delegateModelId.select(initialIndex, ItemSelectionModel.ClearAndSelect)
            artistList.currentIndex = initialIndex
            artistList.positionViewAtIndex(initialIndex, ItemView.Contain)
        }
    }

    Util.SelectableDelegateModel {
        id: delegateModel
        model: MLArtistModel {
            ml: medialib
        }

        onCountChanged: {
            if (delegateModel.items.count > 0 && !delegateModel.hasSelection) {
                var initialIndex = root.initialIndex
                if (initialIndex >= delegateModel.items.count)
                    initialIndex = 0
                artistList.currentIndex = initialIndex
            }
        }

        delegate: Widgets.ListItem {
            height: VLCStyle.icon_normal + VLCStyle.margin_small
            width: artistList.width

            property bool selected: delegateModel.isSelected(index)
            Connections {
               target: delegateModel
               onSelectionChanged: selected = delegateModel.isSelected(index)
            }

            color: VLCStyle.colors.getBgColor(selected, this.hovered, this.activeFocus)

            cover: Widgets.RoundImage {
                source: model.cover || VLCStyle.noArtArtistSmall
                height: VLCStyle.icon_normal
                width: VLCStyle.icon_normal
                radius: VLCStyle.icon_normal
            }

            line1: model.name || i18n.qtr("Unknown artist")

            actionButtons: []

            onItemClicked: {
                artistId = model.id
                delegateModel.updateSelection( modifier , artistList.currentIndex, index)
                artistList.currentIndex = index
                artistList.forceActiveFocus()
            }

            onItemDoubleClicked: {
                if (keys === Qt.RightButton)
                    medialib.addAndPlay( model.id )
                else
                    view.forceActiveFocus()
            }
        }

        function actionAtIndex(index) {
            view.forceActiveFocus()
        }
    }

    FocusScope {
        visible: delegateModel.count > 0
        focus: visible
        anchors.fill: parent

    Row {
        anchors.fill: parent

        Widgets.KeyNavigableListView {
            id: artistList

            width: parent.width * 0.25
            height: parent.height

            spacing: 4
            model: delegateModel
            modelCount: delegateModel.items.count
            currentIndex: -1

            focus: true

            onSelectAll: delegateModel.selectAll()
            onSelectionUpdated: delegateModel.updateSelection( keyModifiers, oldIndex, newIndex )
            onCurrentIndexChanged: {
                if (artistList.currentIndex < delegateModel.count) {
                    root.artistId = delegateModel.items.get(artistList.currentIndex).model.id
                } else {
                    root.artistId = undefined
                }
            }

            navigationParent: root
            navigationRightItem: view
            navigationCancel: function() {
                if (artistList.currentIndex <= 0)
                    defaultNavigationCancel()
                else
                    artistList.currentIndex = 0;
            }

        }

        FocusScope {
            id: view
            width: parent.width * 0.75
            height: parent.height

            property alias currentIndex: albumSubView.currentIndex
            property alias initialIndex: albumSubView.initialIndex

            MusicAlbums {
                id: albumSubView
                anchors.fill: parent

                header: ArtistTopBanner {
                    id: artistBanner
                    width: albumSubView.width
                    artist: (artistList.currentIndex >= 0)
                            ? delegateModel.items.get(artistList.currentIndex).model
                            : ({})
                    navigationParent: root
                    navigationLeftItem: artistList
                    navigationDown: function() {
                        artistBanner.focus = false
                        view.forceActiveFocus()
                    }
                }

                focus: true
                parentId: artistId
                initialIndex: root.initialAlbumIndex

                navigationParent: root
                navigationUpItem: albumSubView.headerItem
                navigationLeftItem: artistList

                onCurrentIndexChanged: {
                    history.update(["mc", "music", "artists", {"initialIndex" : root.currentIndex, "initialAlbumIndex": albumSubView.currentIndex  }])
                }
            }

        }
    }
    }

    EmptyLabel {
        anchors.fill: parent
        visible: delegateModel.count === 0
        focus: visible
        text: i18n.qtr("No artists found\nPlease try adding sources, by going to the Network tab")
        navigationParent: root
    }
}
