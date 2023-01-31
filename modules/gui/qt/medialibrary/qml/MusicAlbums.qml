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
import QtQuick.Layouts 1.11
import QtQml.Models 2.2
import org.videolan.medialib 0.1
import org.videolan.vlc 0.1

import "qrc:///util/" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///main/" as MainInterface
import "qrc:///style/"

FocusScope {
    id: root

    // Properties

    //the index to "go to" when the view is loaded
    property int initialIndex: 0
    property int gridViewMarginTop: VLCStyle.margin_large
    property var gridViewRowX: MainCtx.gridView ? _currentView.rowX : undefined

    readonly property var currentIndex: _currentView.currentIndex

    property Component header: Item{}
    readonly property Item headerItem: _currentView ? _currentView.headerItem : null

    property var sortModel: [
        { text: I18n.qtr("Alphabetic"),  criteria: "title"},
        { text: I18n.qtr("Duration"),    criteria: "duration" },
        { text: I18n.qtr("Date"),        criteria: "release_year" },
        { text: I18n.qtr("Artist"),      criteria: "main_artist" },
    ]

    // Aliases

    property alias leftPadding: view.leftPadding
    property alias rightPadding: view.rightPadding

    property alias model: albumModelId
    property alias parentId: albumModelId.parentId

    readonly property int contentLeftMargin: _currentView ? _currentView.contentLeftMargin : 0
    readonly property int contentRightMargin: _currentView ? _currentView.contentRightMargin : 0

    property alias _currentView: view.currentItem

    onInitialIndexChanged:  resetFocus()
    onModelChanged: resetFocus()
    onParentIdChanged: resetFocus()

    function resetFocus() {
        if (albumModelId.count === 0) {
            return
        }
        var initialIndex = root.initialIndex
        if (initialIndex >= albumModelId.count)
            initialIndex = 0
        selectionModel.select(model.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect)
        if (_currentView)
            _currentView.positionViewAtIndex(initialIndex, ItemView.Contain)
    }

    function setCurrentItemFocus(reason) {
        _currentView.setCurrentItemFocus(reason);
    }

    function _actionAtIndex(index) {
        if (selectionModel.selectedIndexes.length > 1) {
            MediaLib.addAndPlay( model.getIdsForIndexes( selectionModel.selectedIndexes ) )
        } else {
            MediaLib.addAndPlay( model.getIdForIndex(index) )
        }
    }

    function _onNavigationCancel() {
        if (_currentView.currentIndex <= 0) {
            root.Navigation.defaultNavigationCancel()
        } else {
            _currentView.currentIndex = 0;
            _currentView.positionViewAtIndex(0, ItemView.Contain)
        }
    }


    MLAlbumModel {
        id: albumModelId
        ml: MediaLib

        onCountChanged: {
            if (albumModelId.count > 0 && !selectionModel.hasSelection) {
                root.resetFocus()
            }
        }
    }

    Util.SelectableDelegateModel {
        id: selectionModel
        model: albumModelId
    }

    Widgets.MLDragItem {
        id: albumDragItem

        mlModel: albumModelId
        indexes: selectionModel.selectedIndexes
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
            topMargin: root.gridViewMarginTop
            cellWidth: VLCStyle.gridItem_music_width
            cellHeight: VLCStyle.gridItem_music_height

            headerDelegate: root.header

            selectionDelegateModel: selectionModel
            model: albumModelId

            delegate: AudioGridItem {
                id: audioGridItem

                opacity: gridView_id.expandIndex !== -1 && gridView_id.expandIndex !== audioGridItem.index ? .7 : 1
                dragItem: albumDragItem
                onItemClicked : gridView_id.leftClickOnItem(modifier, index)

                onItemDoubleClicked: {
                    gridView_id.switchExpandItem(index)
                }

                onContextMenuButtonClicked: {
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

            onActionAtIndex: {
                if (selectionModel.selectedIndexes.length === 1) {
                    switchExpandItem(index);

                    expandItem.setCurrentItemFocus(Qt.TabFocusReason);
                } else {
                    _actionAtIndex(index);
                }
            }

            Navigation.parentItem: root
            Navigation.cancelAction: root._onNavigationCancel

            Connections {
                target: contextMenu
                onShowMediaInformation: gridView_id.switchExpandItem( index )
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

                    text: I18n.qtr("Title"),

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: tableColumns.titleDelegate,

                    placeHolder: VLCStyle.noArtAlbumCover
                })
            }]

            property var _modelMedium: [{
                size: 2,

                model: {
                    criteria: "title",

                    text: I18n.qtr("Title"),

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: tableColumns.titleDelegate,

                    placeHolder: VLCStyle.noArtAlbumCover
                }
            }, {
                size: Math.max(1, tableView_id._nbCols - 3),

                model: {
                    criteria: "main_artist",

                    text: I18n.qtr("Artist")
                }
            }, {
                size: 1,

                model: {
                    criteria: "duration",

                    showSection: "",

                    headerDelegate: tableColumns.timeHeaderDelegate,
                    colDelegate: tableColumns.timeColDelegate
                }
            }]

            model: albumModelId
            selectionDelegateModel: selectionModel
            headerColor: VLCStyle.colors.bg
            onActionForSelection: _actionAtIndex(selection[0]);
            Navigation.parentItem: root
            section.property: "title_first_symbol"
            header: root.header
            dragItem: albumDragItem
            rowHeight: VLCStyle.tableCoverRow_height
            headerTopPadding: VLCStyle.margin_normal

            sortModel: (availableRowWidth < VLCStyle.colWidth(4)) ? _modelSmall
                                                                  : _modelMedium

            Navigation.cancelAction: root._onNavigationCancel

            onContextMenuButtonClicked: contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
            onRightClick: contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
            onItemDoubleClicked: MediaLib.addAndPlay( model.id )

            Widgets.TableColumns {
                id: tableColumns

                showCriterias: (tableView_id.sortModel === tableView_id._modelSmall)
            }

            Connections {
                target: albumModelId
                onSortCriteriaChanged: {
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

    Widgets.StackViewExt {
        id: view

        anchors.fill: parent

        focus: albumModelId.count !== 0

        initialItem: MainCtx.gridView ? gridComponent : tableComponent

        Connections {
            target: MainCtx
            onGridViewChanged: {
                if (MainCtx.gridView)
                    view.replace(gridComponent)
                else
                    view.replace(tableComponent)
            }
        }
    }

    EmptyLabelButton {
        anchors.fill: parent
        visible: !albumModelId.hasContent
        focus: visible
        text: I18n.qtr("No albums found\nPlease try adding sources, by going to the Browse tab")
        Navigation.parentItem: root
        cover: VLCStyle.noArtAlbumCover
    }
}
