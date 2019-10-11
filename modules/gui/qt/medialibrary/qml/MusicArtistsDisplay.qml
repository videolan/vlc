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

    property var artistId

    Utils.SelectableDelegateModel {
        id: delegateModel
        model: MLArtistModel {
            ml: medialib
        }

        Component.onCompleted: {
            artistId = items.get(0).model.id
        }

        delegate: Utils.ListItem {
            height: VLCStyle.icon_normal + VLCStyle.margin_small
            width: artistList.width

            color: VLCStyle.colors.getBgColor(delegateModel.inSelected, this.hovered, this.activeFocus)

            cover: Rectangle {
                color: VLCStyle.colors.banner
                width: cover_obj.width

                Image {
                    id: cover_obj
                    anchors.fill: parent

                    fillMode: Image.PreserveAspectFit
                    source: model.cover || VLCStyle.noArtArtistSmall
                }
            }
            line1: model.name || qsTr("Unknown artist")

            onItemClicked: {
                artistId = model.id
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
                medialib.addToPlaylist( model.id );
            }
        }

        function actionAtIndex(index) {
            if (delegateModel.selectedGroup.count > 1) {
                var list = []
                for (var i = 0; i < delegateModel.selectedGroup.count; i++)
                    list.push(delegateModel.selectedGroup.get(i).model.id)
                medialib.addAndPlay( list )
            } else if (delegateModel.selectedGroup.count === 1) {
                root.artistId = delegateModel.items.get(artistList.currentIndex).model.id
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
            if (view.currentIndex !== -1)
                initialIndex = view.currentIndex
            delegateModel.items.get(initialIndex).inSelected = true
            view.currentIndex = initialIndex
        }
    }

    Row {
        anchors.fill: parent
        visible: delegateModel.items.count > 0

        Utils.KeyNavigableListView {
            width: parent.width * 0.25
            height: parent.height

            id: artistList
            spacing: 4
            model: delegateModel
            modelCount: delegateModel.items.count

            focus: true

            onSelectAll: delegateModel.selectAll()
            onSelectionUpdated: delegateModel.updateSelection( keyModifiers, oldIndex, newIndex )
            onCurrentIndexChanged: delegateModel.actionAtIndex(currentIndex)

            navigationParent: root
            navigationRight: function () { view.focus = true }
        }

        FocusScope {
            id: view
            width: parent.width * 0.75
            height: parent.height

            property alias currentIndex: albumSubView.currentIndex

            MusicAlbumsDisplay {
                id: albumSubView
                anchors.fill: parent

                header: ArtistTopBanner {
                    id: artistBanner
                    width: albumSubView.width
                    focus: false
                    artist: (artistList.currentIndex >= 0)
                            ? delegateModel.items.get(artistList.currentIndex).model
                            : ({})
                }

                focus: true
                parentId: artistId

                navigationParent: root
                navigationLeft: function () { artistList.focus = true }
            }

        }
    }

    Label {
        anchors.centerIn: parent
        visible: delegateModel.items.count === 0
        font.pixelSize: VLCStyle.fontHeight_xxlarge
        color: root.activeFocus ? VLCStyle.colors.accent : VLCStyle.colors.text
        text: qsTr("No artists found")
    }
}
