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

import VLC.MediaLibrary

import VLC.Util
import VLC.Widgets as Widgets
import VLC.MainInterface
import VLC.Style

MainViewLoader {
    id: root

    // Properties

    readonly property var currentIndex: currentItem?.currentIndex ?? - 1

    property Component header: null
    readonly property Item headerItem: currentItem?.headerItem ?? null

    readonly property int contentLeftMargin: currentItem?.contentLeftMargin ?? 0
    readonly property int contentRightMargin: currentItem?.contentRightMargin ?? 0

    property int displayMarginBeginning: 0
    property int displayMarginEnd: 0

    // Currently only respected by the list view:
    property bool enableBeginningFade: true
    property bool enableEndFade: true

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

        view: root.currentItem
        indexes: indexesFlat ? selectionModel.selectedIndexesFlat
                             : selectionModel.selectedIndexes
        indexesFlat: !!selectionModel.selectedIndexesFlat
        defaultCover: VLCStyle.noArtAlbumCover
    }

    MLContextMenu {
        id: contextMenu

        model: albumModelId
    }

    Component {
        id: gridComponent

        Widgets.ExpandGridItemView {
            id: gridView_id

            basePictureWidth: VLCStyle.gridCover_music_width
            basePictureHeight: VLCStyle.gridCover_music_height

            activeFocusOnTab:true

            headerDelegate: root.header

            selectionModel: root.selectionModel
            model: albumModelId

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            delegate: AudioGridItem {
                id: audioGridItem

                width: gridView_id.cellWidth;
                height: gridView_id.cellHeight;

                pictureWidth: gridView_id.maxPictureWidth
                pictureHeight: gridView_id.maxPictureHeight

                opacity: gridView_id.expandIndex !== -1 && gridView_id.expandIndex !== audioGridItem.index ? .7 : 1
                dragItem: albumDragItem
                onItemClicked : (modifier) => { gridView_id.leftClickOnItem(modifier, index) }

                onItemDoubleClicked: {
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

        Widgets.TableViewExt {
            id: tableView_id

            property var _modelSmall: [{
                weight: 1,

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
                weight: 1,

                model: {
                    criteria: "title",

                    text: qsTr("Title"),

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: tableColumns.titleDelegate,

                    placeHolder: VLCStyle.noArtAlbumCover
                }
            }, {
                weight: 1,

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
            onActionForSelection: (selection) => _actionAtIndex(selection[0])
            Navigation.parentItem: root
            section.property: "title_first_symbol"
            header: root.header
            dragItem: albumDragItem
            rowHeight: VLCStyle.tableCoverRow_height

            rowContextMenu: contextMenu

            sortModel: (availableRowWidth < VLCStyle.colWidth(4)) ? _modelSmall
                                                                  : _modelMedium

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            fadingEdge.enableBeginningFade: root.enableBeginningFade
            fadingEdge.enableEndFade: root.enableEndFade

            onRightClick: (_,_,globalMousePos) => {
                contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
            }
            onItemDoubleClicked: (index, model) => MediaLib.addAndPlay( model.id )

            Widgets.MLTableColumns {
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
            text: qsTr("No albums found\nPlease try adding sources")
            Navigation.parentItem: root
            cover: VLCStyle.noArtAlbumCover
        }
    }
}
