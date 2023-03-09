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

import QtQuick 2.11

import org.videolan.medialib 0.1
import org.videolan.vlc 0.1

import "qrc:///util/" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///main/" as MainInterface
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///style/"

MainInterface.MainViewLoader {
    id: root

    // Properties

    readonly property int currentIndex: Helpers.get(currentItem, "currentIndex", - 1)

    signal requestArtistAlbumView(int reason)

    function _onNavigationCancel() {
        if (currentIndex <= 0) {
            root.Navigation.defaultNavigationCancel()
        } else if (model.count > 0) {
            currentItem.currentIndex = 0
            currentItem.positionViewAtIndex(0, ItemView.Contain)
        }
    }

    model: MLArtistModel {
        id: artistModel
        ml: MediaLib
    }

    grid: gridComponent
    list: tableComponent
    emptyLabel: emptyLabelComponent

    Util.MLContextMenu {
        id: contextMenu

        model: artistModel
    }

    Widgets.MLDragItem {
        id: artistsDragItem

        mlModel: artistModel
        indexes: selectionModel.selectedIndexes
        titleRole: "name"
        defaultCover: VLCStyle.noArtArtistSmall
    }

    Component {
        id: gridComponent

        MainInterface.MainGridView {
            id: artistGrid

            topMargin: VLCStyle.margin_large
            selectionDelegateModel: selectionModel
            model: artistModel
            focus: true
            cellWidth: VLCStyle.colWidth(1)
            cellHeight: VLCStyle.gridItem_music_height

            Navigation.parentItem: root
            Navigation.cancelAction: root._onNavigationCancel

            onActionAtIndex: {
                if (selectionModel.selectedIndexes.length > 1) {
                    MediaLib.addAndPlay( artistModel.getIdsForIndexes( selectionModel.selectedIndexes ) )
                } else {
                    currentIndex = index
                    requestArtistAlbumView(Qt.TabFocusReason)
                }
            }

            delegate: AudioGridItem {
                id: gridItem

                image: model.cover || VLCStyle.noArtArtistSmall
                title: model.name || I18n.qtr("Unknown artist")
                subtitle: model.nb_tracks > 1 ? I18n.qtr("%1 songs").arg(model.nb_tracks) : I18n.qtr("%1 song").arg(model.nb_tracks)
                pictureRadius: VLCStyle.artistGridCover_radius
                pictureHeight: VLCStyle.artistGridCover_radius
                pictureWidth: VLCStyle.artistGridCover_radius
                playCoverBorderWidth: VLCStyle.dp(3, VLCStyle.scale)
                titleMargin: VLCStyle.margin_xlarge
                playIconSize: VLCStyle.play_cover_small
                textAlignHCenter: true
                width: VLCStyle.colWidth(1)
                dragItem: artistsDragItem

                onItemClicked: artistGrid.leftClickOnItem(modifier, index)

                onItemDoubleClicked: root.requestArtistAlbumView(Qt.MouseFocusReason)

                onContextMenuButtonClicked: {
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

        MainInterface.MainTableView {
            id: artistTable

            readonly property int _nbCols: VLCStyle.gridColumnsForWidth(artistTable.availableRowWidth)

            property var _modelSmall: [{
                size: Math.max(2, artistTable._nbCols),

                model: ({
                    criteria: "name",

                    subCriterias: [ "nb_tracks" ],

                    text: I18n.qtr("Name"),

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: tableColumns.titleDelegate,

                    placeHolder: VLCStyle.noArtArtistSmall
                })
            }]

            property var _modelMedium: [{
                size: Math.max(1, artistTable._nbCols - 1),

                model: {
                    criteria: "name",

                    text: I18n.qtr("Name"),

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: tableColumns.titleDelegate,

                    placeHolder: VLCStyle.noArtArtistSmall
                }
            }, {
                size: 1,

                model: {
                    criteria: "nb_tracks",

                    text: I18n.qtr("Tracks")
                }
            }]

            selectionDelegateModel: selectionModel
            model: artistModel
            focus: true
            dragItem: artistsDragItem
            rowHeight: VLCStyle.tableCoverRow_height
            headerTopPadding: VLCStyle.margin_normal

            Navigation.parentItem: root
            Navigation.cancelAction: root._onNavigationCancel

            onActionForSelection: {
                if (selection.length > 1) {
                    MediaLib.addAndPlay( artistModel.getIdsForIndexes( selection ) )
                } else if ( selection.length === 1) {
                    requestArtistAlbumView(Qt.TabFocusReason)
                    // FIX ME - requestArtistAlbumView will destroy this view
                    MediaLib.addAndPlay( artistModel.getIdForIndex( selection[0] ) )
                }
            }

            sortModel: (availableRowWidth < VLCStyle.colWidth(4)) ? _modelSmall
                                                                  : _modelMedium

            onItemDoubleClicked: root.requestArtistAlbumView(Qt.MouseFocusReason)

            onContextMenuButtonClicked: contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
            onRightClick: contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)

            Widgets.TableColumns {
                id: tableColumns

                showCriterias: (artistTable.sortModel === artistTable._modelSmall)
            }
        }
    }

    Component {
        id: emptyLabelComponent

        EmptyLabelButton {
            text: I18n.qtr("No artists found\nPlease try adding sources, by going to the Browse tab")
            Navigation.parentItem: root
            cover: VLCStyle.noArtArtistCover
        }
    }
}
