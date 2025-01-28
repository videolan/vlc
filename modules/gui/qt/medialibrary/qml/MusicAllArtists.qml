/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

import QtQuick

import VLC.MediaLibrary

import VLC.Util
import VLC.Widgets as Widgets
import VLC.MainInterface
import VLC.Style

MainViewLoader {
    id: root

    // Properties

    readonly property int currentIndex: currentItem?.currentIndex ?? - 1

    property Component header: null

    readonly property int contentLeftMargin: currentItem?.contentLeftMargin ?? 0
    readonly property int contentRightMargin: currentItem?.contentRightMargin ?? 0

    property alias parentId: artistModel.parentId
    property alias searchPattern: artistModel.searchPattern
    property alias sortOrder: artistModel.sortOrder
    property alias sortCriteria: artistModel.sortCriteria

    signal artistAlbumViewRequested(var id, int reason)

    function requestArtistAlbumView(reason: int, id = null) {
        if (id !== null) {
            console.assert(id !== undefined)
            artistAlbumViewRequested(id, reason)
        } else {
            // Do not call this function if there is no current item,
            // and you are not providing an explicit id:
            console.assert(currentIndex >= 0)
            // Do not call this function if there is no model:
            console.assert(root.model)
            const data = root.model.getDataAt(currentIndex)
            artistAlbumViewRequested(data.id, reason)
        }
    }

    isSearchable: true

    model: MLArtistModel {
        id: artistModel
        ml: MediaLib
    }

    sortModel: [
        { text: qsTr("Alphabetic"), criteria: "name" },
        { text: qsTr("Tracks Count"),   criteria: "nb_tracks" }
    ]

    grid: gridComponent
    list: tableComponent
    emptyLabel: emptyLabelComponent

    MLContextMenu {
        id: contextMenu

        model: artistModel
    }

    Widgets.MLDragItem {
        id: artistsDragItem

        mlModel: artistModel
        indexes: indexesFlat ? selectionModel.selectedIndexesFlat
                             : selectionModel.selectedIndexes
        indexesFlat: !!selectionModel.selectedIndexesFlat
        defaultCover: VLCStyle.noArtArtistSmall
    }

    Component {
        id: gridComponent

        Widgets.ExpandGridItemView {
            id: artistGrid

            basePictureWidth: VLCStyle.gridCover_music_width
            basePictureHeight: VLCStyle.gridCover_music_height
            titleTopMargin: VLCStyle.gridItemTitle_topMargin + VLCStyle.margin_xxsmall

            selectionModel: root.selectionModel
            model: artistModel
            focus: true

            headerDelegate: root.header
            Navigation.parentItem: root

            onActionAtIndex: (index) => {
                if (selectionModel.selectedIndexes.length > 1) {
                    artistModel.addAndPlay( selectionModel.selectedIndexes )
                } else {
                    currentIndex = index
                    root.requestArtistAlbumView(Qt.TabFocusReason)
                }
            }

            delegate: AudioGridItem {
                id: gridItem

                width: artistGrid.cellWidth
                height: artistGrid.cellHeight

                pictureWidth: artistGrid.maxPictureWidth
                pictureHeight: artistGrid.maxPictureHeight
                pictureRadius: artistGrid.maxPictureWidth

                image: model.cover || ""
                fallbackImage: VLCStyle.noArtArtistSmall

                title: model.name || qsTr("Unknown artist")
                subtitle: model.nb_tracks > 1 ? qsTr("%1 songs").arg(model.nb_tracks) : qsTr("%1 song").arg(model.nb_tracks)
                titleTopMargin: artistGrid.titleTopMargin
                playIconSize: VLCStyle.play_cover_small
                textAlignHCenter: true
                dragItem: artistsDragItem

                onItemClicked: (modifier) => { artistGrid.leftClickOnItem(modifier, index) }

                onItemDoubleClicked: root.requestArtistAlbumView(Qt.MouseFocusReason, model.id)

                onContextMenuButtonClicked: (_, globalMousePos) => {
                    artistGrid.rightClickOnItem(index)
                    contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
                }

                selectedShadow.anchors.margins: VLCStyle.dp(1) // outside border
                unselectedShadow.anchors.margins: VLCStyle.dp(1) // outside border
            }
        }
    }


    Component {
        id: tableComponent

        MainTableView {
            id: artistTable

            property var _modelSmall: [{
                weight: 1,

                model: ({
                    criteria: "name",

                    subCriterias: [ "nb_tracks" ],

                    text: qsTr("Name"),

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: tableColumns.titleDelegate,

                    placeHolder: VLCStyle.noArtArtistSmall
                })
            }]

            property var _modelMedium: [{
                weight: 1,

                model: {
                    criteria: "name",

                    text: qsTr("Name"),

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: tableColumns.titleDelegate,

                    placeHolder: VLCStyle.noArtArtistSmall
                }
            }, {
                size: 1,

                model: {
                    criteria: "nb_tracks",

                    text: qsTr("Tracks")
                }
            }]

            selectionModel: root.selectionModel
            model: artistModel
            focus: true
            dragItem: artistsDragItem
            rowHeight: VLCStyle.tableCoverRow_height

            header: root.header
            Navigation.parentItem: root

            onActionForSelection: (selection) => {
                artistModel.addAndPlay( selection )
                if ( selection.length === 1)
                    requestArtistAlbumView(Qt.TabFocusReason)
            }

            sortModel: (availableRowWidth < VLCStyle.colWidth(4)) ? _modelSmall
                                                                  : _modelMedium

            onItemDoubleClicked: function(index, model) {
                root.requestArtistAlbumView(Qt.MouseFocusReason, model.id)
            }

            onContextMenuButtonClicked: (_,_, globalMousePos) => {
                contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
            }
            onRightClick: (_,_,globalMousePos) => {
                contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
            }

            Widgets.MLTableColumns {
                id: tableColumns

                showCriterias: (artistTable.sortModel === artistTable._modelSmall)
            }
        }
    }

    Component {
        id: emptyLabelComponent

        Widgets.EmptyLabelButton {
            text: qsTr("No artists found\nPlease try adding sources, by going to the Browse tab")
            Navigation.parentItem: root
            cover: VLCStyle.noArtArtistCover
        }
    }
}
