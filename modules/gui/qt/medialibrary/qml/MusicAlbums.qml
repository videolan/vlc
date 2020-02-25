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
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root

    property var sortModel: [
        { text: i18n.qtr("Alphabetic"),  criteria: "title"},
        { text: i18n.qtr("Duration"),    criteria: "duration" },
        { text: i18n.qtr("Date"),        criteria: "release_year" },
        { text: i18n.qtr("Artist"),      criteria: "main_artist" },
    ]

    property alias model: delegateModelId.model
    property alias parentId: delegateModelId.parentId
    readonly property var currentIndex: view.currentItem.currentIndex
    //the index to "go to" when the view is loaded
    property var initialIndex: 0


    navigationCancel: function() {
        if (view.currentItem.currentIndex <= 0) {
            defaultNavigationCancel()
        } else {
            view.currentItem.currentIndex = 0;
            view.currentItem.positionViewAtIndex(0, ItemView.Contain)
        }
    }

    property Component header: Item{}
    readonly property var headerItem: view.currentItem ? view.currentItem.headerItem : undefined

    onInitialIndexChanged:  resetFocus()
    onModelChanged: resetFocus()
    onParentIdChanged: resetFocus()

    function resetFocus() {
        if (delegateModelId.items.count === 0) {
            return
        }
        var initialIndex = root.initialIndex
        if (initialIndex >= delegateModelId.items.count)
            initialIndex = 0
        delegateModelId.selectNone()
        delegateModelId.items.get(initialIndex).inSelected = true
        view.currentItem.currentIndex = initialIndex
        view.currentItem.positionViewAtIndex(initialIndex, ItemView.Contain)
    }

    Util.SelectableDelegateModel {
        id: delegateModelId
        property alias parentId: albumModelId.parentId

        model: MLAlbumModel {
            id: albumModelId
            ml: medialib
        }


        delegate: Package {
            id: element

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
                line1: (model.title || i18n.qtr("Unknown title"))+" ["+model.duration+"]"
                line2: model.main_artist || i18n.qtr("Unknown artist")

                onItemClicked : {
                    delegateModelId.updateSelection( modifier, view.currentItem.currentIndex, index )
                    view.currentItem.currentIndex = index
                    this.forceActiveFocus()
                }
                onPlayClicked: medialib.addAndPlay( model.id )
                onAddToPlaylistClicked : medialib.addToPlaylist( model.id )
            }
        }

        onCountChanged: {
            if (delegateModelId.items.count > 0 && !delegateModelId.hasSelection) {
                root.resetFocus()
            }
        }

        function actionAtIndex(index) {
            if (delegateModelId.selectedGroup.count > 1) {
                medialib.addAndPlay( model.getIdsForIndexes( delegateModelId.selectedIndexes() ) )
            } else {
                medialib.addAndPlay( model.getIdsForIndexes([index]) )
            }
        }
    }

    Component {
        id: gridComponent

        Widgets.ExpandGridView {
            id: gridView_id

            activeFocusOnTab:true

            cellWidth: VLCStyle.gridItem_music_width
            cellHeight: VLCStyle.gridItem_music_height

            headerDelegate: root.header

            delegateModel: delegateModelId
            model: albumModelId

            delegate: AudioGridItem {
                id: audioGridItem

                onItemClicked : {
                    delegateModelId.updateSelection( modifier , root.currentIndex, index)
                    gridView_id.currentIndex = index
                    gridView_id.forceActiveFocus()
                }

                onItemDoubleClicked: {
                    if ( model.id !== undefined ) { medialib.addAndPlay( model.id ) }
                }
            }

            expandDelegate: MusicAlbumsGridExpandDelegate {
                id: expandDelegateId
                width: root.width

                implicitHeight: gridView_id.height - gridView_id.cellHeight

                navigationParent: root
                navigationCancel:  function() {  gridView_id.retract() }
                navigationUp: function() {  gridView_id.retract() }
                navigationDown: function() {}

            }

            onActionAtIndex: {
                if (delegateModelId.selectedGroup.count === 1) {
                    view._switchExpandItem(index)
                } else {
                    delegateModelId.actionAtIndex(index)
                }
            }
            onSelectAll: delegateModelId.selectAll()
            onSelectionUpdated: delegateModelId.updateSelection( keyModifiers, oldIndex, newIndex )

            navigationParent: root
        }
    }

    Component {
        id: listComponent
        /* ListView */
        Widgets.KeyNavigableListView {
            id: listView_id

            header: root.header

            spacing: VLCStyle.margin_xxxsmall

            model: delegateModelId.parts.list
            modelCount: delegateModelId.items.count

            onActionAtIndex: delegateModelId.actionAtIndex(index)
            onSelectAll: delegateModelId.selectAll()
            onSelectionUpdated: delegateModelId.updateSelection( keyModifiers, oldIndex, newIndex )

            navigationParent: root
            navigationCancel: function() {
                if (listView_id.currentIndex <= 0)
                    defaultNavigationCancel()
                else
                    listView_id.currentIndex = 0;
            }
        }
    }

    Widgets.StackViewExt {
        id: view

        anchors.fill: parent
        focus: delegateModelId.items.count !== 0

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

    EmptyLabel {
        anchors.fill: parent
        visible: delegateModelId.items.count === 0
        focus: visible
        text: i18n.qtr("No albums found\nPlease try adding sources, by going to the Network tab")
        navigationParent: root
    }
}
