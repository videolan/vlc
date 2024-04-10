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
import QtQuick

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

    property var gridViewRowX: Helpers.get(currentItem, "rowX", 0)

    readonly property var currentIndex: Helpers.get(currentItem, "currentIndex", - 1)

    property Component header: null
    readonly property Item headerItem: Helpers.get(currentItem, "headerItem", null)

    readonly property int contentLeftMargin: Helpers.get(currentItem, "contentLeftMargin", 0)
    readonly property int contentRightMargin: Helpers.get(currentItem, "contentRightMargin", 0)

    property alias parentId: albumModelId.parentId
    property alias searchPattern: albumModelId.searchPattern
    property alias sortOrder: albumModelId.sortOrder
    property alias sortCriteria: albumModelId.sortCriteria

    isSearchable: true
    model: albumModelId

    sortModel: [
        { text: qsTr("Alphabetic"),  criteria: "title"},
        { text: qsTr("Duration"),    criteria: "duration" },
        { text: qsTr("Date"),        criteria: "release_year" },
        { text: qsTr("Artist"),      criteria: "main_artist" },
    ]

    grid: gridComponent
    list: tableComponent
    emptyLabel: emptyLabelComponent

    onParentIdChanged: resetFocus()

    function _actionAtIndex(index) {
        if (selectionModel.selectedIndexes.length > 1) {
            model.addAndPlay( selectionModel.selectedIndexes )
        } else {
            model.addAndPlay( new Array(index) )
        }
    }

    MLAlbumModel {
        id: albumModelId

        ml: MediaLib
    }

    Widgets.MLDragItem {
        id: albumDragItem

        mlModel: albumModelId
        indexes: indexesFlat ? selectionModel.selectedIndexesFlat
                             : selectionModel.selectedIndexes
        indexesFlat: !!selectionModel.selectedIndexesFlat
        defaultCover: VLCStyle.noArtAlbumCover
    }

    Util.MLContextMenu {
        id: contextMenu

        model: albumModelId
    }

    Component {
        id: gridComponent

        MainInterface.MainGridView {
            id: gridView_id

            activeFocusOnTab:true
            cellWidth: VLCStyle.gridItem_music_width
            cellHeight: VLCStyle.gridItem_music_height

            headerDelegate: root.header

            selectionModel: root.selectionModel
            model: albumModelId

            delegate: AudioGridItem {
                id: audioGridItem

                opacity: gridView_id.expandIndex !== -1 && gridView_id.expandIndex !== audioGridItem.index ? .7 : 1
                dragItem: albumDragItem
                onItemClicked : (_,_,modifier) => { gridView_id.leftClickOnItem(modifier, index) }

                onItemDoubleClicked: (_,_,modifier) => {
                    gridView_id.switchExpandItem(index)
                }

                onContextMenuButtonClicked: (_, globalMousePos) => {
                    gridView_id.rightClickOnItem(index)
                    contextMenu.popup(selectionModel.selectedIndexes, globalMousePos, {
                        "information": index
                    })
                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: VLCStyle.duration_short
                    }
                }
            }

            expandDelegate: MusicAlbumsGridExpandDelegate {
                id: expandDelegateId

                x: 0
                width: gridView_id.width
                onRetract: gridView_id.retract()
                Navigation.parentItem: root

                Navigation.cancelAction: function() {
                    gridView_id.setCurrentItemFocus(Qt.TabFocusReason);
                }

                Navigation.upAction: function() {
                    gridView_id.setCurrentItemFocus(Qt.TabFocusReason);
                }

                Navigation.downAction: function() {}
            }

            onActionAtIndex: (index) => {
                if (selectionModel.selectedIndexes.length === 1) {
                    switchExpandItem(index);

                    expandItem.setCurrentItemFocus(Qt.TabFocusReason);
                } else {
                    _actionAtIndex(index);
                }
            }

            Navigation.parentItem: root

            Connections {
                target: contextMenu
                function onShowMediaInformation(index) {
                    gridView_id.switchExpandItem( index )
                }
            }
        }
    }

    Component {
        id: tableComponent

        MainInterface.MainTableView {
            id: tableView_id

            readonly property int _nbCols: VLCStyle.gridColumnsForWidth(tableView_id.availableRowWidth)

            property var _modelSmall: [{
                size: Math.max(2, tableView_id._nbCols),

                model: ({
                    criteria: "title",

                    subCriterias: [ "main_artist", "duration" ],

                    text: qsTr("Title"),

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: tableColumns.titleDelegate,

                    placeHolder: VLCStyle.noArtAlbumCover
                })
            }]

            property var _modelMedium: [{
                size: 2,

                model: {
                    criteria: "title",

                    text: qsTr("Title"),

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: tableColumns.titleDelegate,

                    placeHolder: VLCStyle.noArtAlbumCover
                }
            }, {
                size: Math.max(1, tableView_id._nbCols - 3),

                model: {
                    criteria: "main_artist",

                    text: qsTr("Artist")
                }
            }, {
                size: 1,

                model: {
                    criteria: "duration",

                    text: qsTr("Duration"),

                    showSection: "",

                    headerDelegate: tableColumns.timeHeaderDelegate,
                    colDelegate: tableColumns.timeColDelegate
                }
            }]

            model: albumModelId
            selectionModel: root.selectionModel
            onActionForSelection: _actionAtIndex(selection[0]);
            Navigation.parentItem: root
            section.property: "title_first_symbol"
            header: root.header
            dragItem: albumDragItem
            rowHeight: VLCStyle.tableCoverRow_height

            sortModel: (availableRowWidth < VLCStyle.colWidth(4)) ? _modelSmall
                                                                  : _modelMedium

            onContextMenuButtonClicked: (_,_,globalMousePos) => {
                contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
            }
            onRightClick: (_,_,globalMousePos) => {
                contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
            }
            onItemDoubleClicked: (index, model) => MediaLib.addAndPlay( model.id )

            Widgets.TableColumns {
                id: tableColumns

                showCriterias: (tableView_id.sortModel === tableView_id._modelSmall)
            }

            Connections {
                target: albumModelId
                function onSortCriteriaChanged() {
                    switch (albumModelId.sortCriteria) {
                    case "title":
                    case "main_artist":
                        tableView_id.section.property = albumModelId.sortCriteria + "_first_symbol"
                        break;
                    default:
                        tableView_id.section.property = ""
                    }
                }
            }
        }
    }

    Component {
        id: emptyLabelComponent

        Widgets.EmptyLabelButton {
            text: qsTr("No albums found\nPlease try adding sources, by going to the Browse tab")
            Navigation.parentItem: root
            cover: VLCStyle.noArtAlbumCover
        }
    }
}
