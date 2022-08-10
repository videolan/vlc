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

import QtQuick.Controls 2.4
import QtQuick 2.11
import QtQml.Models 2.2
import QtQuick.Layouts 1.3

import org.videolan.medialib 0.1
import org.videolan.vlc 0.1

import "qrc:///util/" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///main/" as MainInterface
import "qrc:///style/"

FocusScope {
    id: root

    readonly property int currentIndex: _currentView.currentIndex
    property int initialIndex: 0
    property alias model: artistModel

    property alias _currentView: view.currentItem

    onInitialIndexChanged: resetFocus()

    signal requestArtistAlbumView(int reason)

    function resetFocus() {
        if (artistModel.count === 0)
            return

        var initialIndex = root.initialIndex
        if (initialIndex >= artistModel.count)
            initialIndex = 0
        selectionModel.select(artistModel.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect)
        if (_currentView) {
            _currentView.currentIndex = initialIndex
            _currentView.positionViewAtIndex(initialIndex, ItemView.Contain)
        }
    }

    function setCurrentItemFocus(reason) {
        _currentView.setCurrentItemFocus(reason);
    }

    function _onNavigationCancel() {
        if (_currentView.currentIndex <= 0) {
            root.Navigation.defaultNavigationCancel()
        } else {
            _currentView.currentIndex = 0;
            _currentView.positionViewAtIndex(0, ItemView.Contain);
        }
    }

    MLArtistModel {
        id: artistModel
        ml: MediaLib

        onCountChanged: {
            if (artistModel.count > 0 && !selectionModel.hasSelection) {
                root.resetFocus()
            }
        }
    }

    Util.SelectableDelegateModel {
        id: selectionModel
        model: artistModel
    }

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

            anchors.fill: parent
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
                    _currentView.currentIndex = index
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

            anchors.fill: parent
            selectionDelegateModel: selectionModel
            model: artistModel
            focus: true
            headerColor: VLCStyle.colors.bg
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
                    MediaLib.addAndPlay( artistModel.getIdForIndex( selection[0] ) )
                }
            }

            sortModel:  [
                { isPrimary: true, criteria: "name", width: VLCStyle.colWidth(Math.max(artistTable._nbCols - 1, 1)), text: I18n.qtr("Name"), headerDelegate: tableColumns.titleHeaderDelegate, colDelegate: tableColumns.titleDelegate, placeHolder: VLCStyle.noArtArtistSmall },
                { criteria: "nb_tracks", width: VLCStyle.colWidth(1), text: I18n.qtr("Tracks") }
            ]

            onItemDoubleClicked: root.requestArtistAlbumView(Qt.MouseFocusReason)

            onContextMenuButtonClicked: contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
            onRightClick: contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)

            Widgets.TableColumns {
                id: tableColumns
            }
        }
    }

    Widgets.StackViewExt {
        id: view

        anchors.fill: parent
        visible: artistModel.count > 0
        focus: artistModel.count > 0
        initialItem: MainCtx.gridView ? gridComponent : tableComponent
    }

    Connections {
        target: MainCtx
        onGridViewChanged: {
            if (MainCtx.gridView) {
                view.replace(gridComponent)
            } else {
                view.replace(tableComponent)
            }
        }
    }

    EmptyLabelButton {
        anchors.fill: parent
        visible: artistModel.count === 0
        focus: artistModel.count === 0
        text: I18n.qtr("No artists found\nPlease try adding sources, by going to the Browse tab")
        Navigation.parentItem: root
        cover: VLCStyle.noArtArtistCover
    }
}
