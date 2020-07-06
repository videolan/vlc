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

    property alias model: albumModelId
    property alias parentId: albumModelId.parentId
    readonly property var currentIndex: view.currentItem.currentIndex
    //the index to "go to" when the view is loaded
    property var initialIndex: 0
    property int gridViewMarginTop: VLCStyle.margin_large

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
        if (albumModelId.count === 0) {
            return
        }
        var initialIndex = root.initialIndex
        if (initialIndex >= albumModelId.count)
            initialIndex = 0
        selectionModel.select(model.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect)
        view.currentItem.positionViewAtIndex(initialIndex, ItemView.Contain)
    }

    function _actionAtIndex(index) {
        if (selectionModel.selectedGroup.count > 1) {
            medialib.addAndPlay( model.getIdsForIndexes( selectionModel.selectedIndexes ) )
        } else {
            medialib.addAndPlay( model.getIdForIndex(index) )
        }
    }

    MLAlbumModel {
        id: albumModelId
        ml: medialib

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

    Component {
        id: gridComponent

        Widgets.ExpandGridView {
            id: gridView_id

            activeFocusOnTab:true
            marginTop: root.gridViewMarginTop
            cellWidth: VLCStyle.gridItem_music_width
            cellHeight: VLCStyle.gridItem_music_height

            headerDelegate: root.header

            delegateModel: selectionModel
            model: albumModelId

            delegate: AudioGridItem {
                id: audioGridItem

                opacity: gridView_id.expandIndex !== -1 && gridView_id.expandIndex !== audioGridItem.index ? .7 : 1

                onItemClicked : {
                    selectionModel.updateSelection( modifier , root.currentIndex, index)
                    gridView_id.currentIndex = index
                    gridView_id.forceActiveFocus()
                }

                onItemDoubleClicked: {
                    if ( model.id !== undefined ) { medialib.addAndPlay( model.id ) }
                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: 100
                    }
                }
            }

            expandDelegate: MusicAlbumsGridExpandDelegate {
                id: expandDelegateId

                onRetract: gridView_id.retract()
                navigationParent: root
                navigationCancel:  function() {  gridView_id.retract() }
                navigationUp: function() {  gridView_id.retract() }
                navigationDown: function() {}
            }

            onActionAtIndex: {
                if (selectionModel.selectedIndexes.length === 1) {
                    view._switchExpandItem(index)
                } else {
                    _actionAtIndex(index)
                }
            }
            onSelectAll: selectionModel.selectAll()
            onSelectionUpdated: selectionModel.updateSelection( keyModifiers, oldIndex, newIndex )

            navigationParent: root
        }
    }

    Widgets.MenuExt {
        id: contextMenu
        property var model: ({})
        closePolicy: Popup.CloseOnReleaseOutside | Popup.CloseOnEscape

        Widgets.MenuItemExt {
            id: playMenuItem
            text: "Play from start"
            onTriggered: {
                medialib.addAndPlay( contextMenu.model.id )
                history.push(["player"])
            }
        }

        Widgets.MenuItemExt {
            text: "Enqueue"
            onTriggered: medialib.addToPlaylist( contextMenu.model.id )
        }

        onClosed: contextMenu.parent.forceActiveFocus()

    }

    Component {
        id: tableComponent

        Widgets.KeyNavigableTableView {
            id: tableView_id

            readonly property int _nbCols: VLCStyle.gridColumnsForWidth(tableView_id.availableRowWidth)

            model: albumModelId
            headerColor: VLCStyle.colors.bg
            onActionForSelection: _actionAtIndex(index)
            navigationParent: root
            section.property: "title_first_symbol"
            header: root.header

            sortModel:  [
                { isPrimary: true, criteria: "title", width: VLCStyle.colWidth(2), text: i18n.qtr("Title"), headerDelegate: tableColumns.titleHeaderDelegate, colDelegate: tableColumns.titleDelegate },
                { criteria: "main_artist", width: VLCStyle.colWidth(Math.max(tableView_id._nbCols - 3, 1)), text: i18n.qtr("Artist") },
                { criteria: "durationShort", width:VLCStyle.colWidth(1), showSection: "", headerDelegate: tableColumns.timeHeaderDelegate, colDelegate: tableColumns.timeColDelegate },
            ]

            navigationCancel: function() {
                if (tableView_id.currentIndex <= 0)
                    defaultNavigationCancel()
                else
                    tableView_id.currentIndex = 0;
            }

            onContextMenuButtonClicked: {
                contextMenu.model = menuModel
                contextMenu.popup(menuParent)
            }

            Widgets.TableColumns {
                id: tableColumns
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

        initialItem: medialib.gridView ? gridComponent : tableComponent

        Connections {
            target: medialib
            onGridViewChanged: {
                if (medialib.gridView)
                    view.replace(gridComponent)
                else
                    view.replace(tableComponent)
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
        visible: albumModelId.count === 0
        focus: visible
        text: i18n.qtr("No albums found\nPlease try adding sources, by going to the Network tab")
        navigationParent: root
    }
}
