/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import QtQuick.Layouts 1.11

import org.videolan.medialib 0.1
import org.videolan.vlc 0.1

import "qrc:///util/" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///main/" as MainInterface
import "qrc:///style/"

FocusScope {
    id: root

    property var artist: ({})

    //the index to "go to" when the view is loaded
    property int initialIndex: 0

    property Item headerItem: _currentView ? _currentView.headerItem : null

    // current index of album model
    readonly property int currentIndex: {
        if (!_currentView)
           return -1
        else if (MainCtx.gridView)
           return _currentView.currentIndex
        else
           return headerItem.albumsListView.currentIndex
    }

    property alias _currentView: view.currentItem

    property Component header: FocusScope {
        id: headerFs

        property Item albumsListView: albumsLoader.status === Loader.Ready ? albumsLoader.item.albumsListView: null
        property Item focusItem: albumsLoader.active ? albumsLoader.item.albumsListView : artistBanner

        focus: true
        height: col.height
        width: root.width

        function setCurrentItemFocus(reason) {
            if (albumsListView)
                albumsListView.setCurrentItemFocus(reason);
            else
                artistBanner.setCurrentItemFocus(reason);
        }

        Column {
            id: col

            height: implicitHeight
            width: root.width
            bottomPadding: VLCStyle.margin_normal

            ArtistTopBanner {
                id: artistBanner

                focus: true
                width: root.width
                artist: root.artist
                Navigation.parentItem: root
                Navigation.downAction: function() {
                    if (albumsListView)
                        albumsListView.setCurrentItemFocus(Qt.TabFocusReason);
                    else
                        _currentView.setCurrentItemFocus(Qt.TabFocusReason);

                }
            }

            Loader {
                id: albumsLoader

                active: !MainCtx.gridView
                focus: true
                sourceComponent: Column {
                    property alias albumsListView: albumsList

                    width: root.width
                    height: implicitHeight

                    Widgets.SubtitleLabel {
                        id: albumsText

                        text: I18n.qtr("Albums")
                        leftPadding: VLCStyle.margin_xlarge
                        topPadding: VLCStyle.margin_normal
                        bottomPadding: VLCStyle.margin_xsmall
                    }

                    Widgets.KeyNavigableListView {
                        id: albumsList

                        focus: true
                        height: VLCStyle.gridItem_music_height + VLCStyle.margin_normal
                        width: root.width
                        leftMargin: VLCStyle.margin_xlarge
                        topMargin: VLCStyle.margin_xsmall
                        bottomMargin: VLCStyle.margin_xsmall
                        model: albumModel
                        orientation: ListView.Horizontal
                        spacing: VLCStyle.column_spacing

                        Navigation.parentItem: root

                        Navigation.upAction: function() {
                            artistBanner.setCurrentItemFocus(Qt.TabFocusReason);
                        }

                        Navigation.downAction: function() {
                            root.setCurrentItemFocus(Qt.TabFocusReason);
                        }

                        delegate: Widgets.GridItem {
                            id: gridItem

                            image: model.cover || VLCStyle.noArtAlbumCover
                            title: model.title || I18n.qtr("Unknown title")
                            subtitle: model.release_year || ""
                            textAlignHCenter: true
                            x: selectedBorderWidth
                            y: selectedBorderWidth
                            pictureWidth: VLCStyle.gridCover_music_width
                            pictureHeight: VLCStyle.gridCover_music_height
                            playCoverBorderWidth: VLCStyle.gridCover_music_border
                            dragItem: albumDragItem

                            onPlayClicked: play()
                            onItemDoubleClicked: play()

                            onItemClicked: {
                                albumSelectionModel.updateSelection( modifier , albumsList.currentIndex, index )
                                albumsList.currentIndex = index
                                albumsList.forceActiveFocus()
                            }

                            Connections {
                                target: albumSelectionModel

                                onSelectionChanged: gridItem.selected = albumSelectionModel.isSelected(albumModel.index(index, 0))
                            }

                            function play() {
                                if ( model.id !== undefined ) {
                                    MediaLib.addAndPlay( model.id )
                                }
                            }
                        }

                        onSelectAll: albumSelectionModel.selectAll()
                        onSelectionUpdated: albumSelectionModel.updateSelection( keyModifiers, oldIndex, newIndex )
                        onActionAtIndex: MediaLib.addAndPlay( albumModel.getIdForIndex( index ) )
                    }

                    Widgets.SubtitleLabel {
                        id: tracksText

                        text: I18n.qtr("Tracks")
                        leftPadding: VLCStyle.margin_xlarge
                        topPadding: VLCStyle.margin_large
                    }
                }
            }
        }
    }

    focus: true

    onInitialIndexChanged: resetFocus()
    onActiveFocusChanged: {
        if (activeFocus && albumModel.count > 0 && !albumSelectionModel.hasSelection) {
            var initialIndex = 0
            var albumsListView = MainCtx.gridView ? _currentView : headerItem.albumsListView
            if (albumsListView.currentIndex !== -1)
                initialIndex = albumsListView.currentIndex
            albumSelectionModel.select(albumModel.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect)
            albumsListView.currentIndex = initialIndex
        }
    }

    function setCurrentItemFocus(reason) {
        view.currentItem.setCurrentItemFocus(reason);
    }

    function resetFocus() {
        if (albumModel.count === 0) {
            return
        }
        var initialIndex = root.initialIndex
        if (initialIndex >= albumModel.count)
            initialIndex = 0
        albumSelectionModel.select(albumModel.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect)
        var albumsListView = MainCtx.gridView ? _currentView : headerItem.albumsListView
        if (albumsListView) {
            albumsListView.currentIndex = initialIndex
            albumsListView.positionViewAtIndex(initialIndex, ItemView.Contain)
        }
    }

    function _actionAtIndex(index, model, selectionModel) {
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

        if (tableView_id.currentIndex <= 0)
            root.Navigation.defaultNavigationCancel()
        else
            tableView_id.currentIndex = 0;
    }

    MLAlbumModel {
        id: albumModel

        ml: MediaLib
        parentId: artist.id

        onCountChanged: {
            if (albumModel.count > 0 && !albumSelectionModel.hasSelection) {
                root.resetFocus()
            }
        }
    }

    Util.SelectableDelegateModel {
        id: albumSelectionModel
        model: albumModel
    }

    Widgets.MLDragItem {
        id: albumDragItem

        mlModel: albumModel
        indexes: albumSelectionModel.selectedIndexes
        defaultCover: VLCStyle.noArtAlbumCover
    }

    MLAlbumTrackModel {
        id: trackModel

        ml: MediaLib
        parentId: albumModel.parentId
    }

    Util.MLContextMenu {
        id: contextMenu

        model: albumModel
    }

    Util.MLContextMenu {
        id: trackContextMenu

        model: trackModel
    }

    Component {
        id: gridComponent

        MainInterface.MainGridView {
            id: gridView_id

            focus: true
            activeFocusOnTab:true
            cellWidth: VLCStyle.gridItem_music_width
            cellHeight: VLCStyle.gridItem_music_height
            headerDelegate: root.header
            selectionDelegateModel: albumSelectionModel
            model: albumModel

            Connections {
                target: albumModel
                // selectionModel updates but doesn't trigger any signal, this forces selection update in view
                onParentIdChanged: currentIndex = -1
            }

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
                    contextMenu.popup(albumSelectionModel.selectedIndexes, globalMousePos, { "information" : index})
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
                if (albumSelectionModel.selectedIndexes.length === 1) {
                    switchExpandItem(index);

                    expandItem.setCurrentItemFocus(Qt.TabFocusReason);
                } else {
                    _actionAtIndex(index, albumModel, albumSelectionModel);
                }
            }

            Navigation.parentItem: root

            Navigation.upAction: function() {
                headerItem.setCurrentItemFocus(Qt.TabFocusReason);
            }

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

            clip: true // content may overflow if not enough space is provided
            model: trackModel
            selectionDelegateModel: trackSelectionModel
            headerColor: VLCStyle.colors.bg
            onActionForSelection: {
                MediaLib.addAndPlay( model.getIdsForIndexes( selection ) )
            }

            header: root.header
            headerPositioning: ListView.InlineHeader
            rowHeight: VLCStyle.tableCoverRow_height

            sortModel:  [
                { isPrimary: true, criteria: "title", width: VLCStyle.colWidth(2), text: I18n.qtr("Title"), headerDelegate: tableColumns.titleHeaderDelegate, colDelegate: tableColumns.titleDelegate },
                { criteria: "album_title", width: VLCStyle.colWidth(Math.max(tableView_id._nbCols - 3, 1)), text: I18n.qtr("Album") },
                { criteria: "duration", width:VLCStyle.colWidth(1), showSection: "", headerDelegate: tableColumns.timeHeaderDelegate, colDelegate: tableColumns.timeColDelegate },
            ]

            Navigation.parentItem: root

            Navigation.upAction: function() {
                headerItem.setCurrentItemFocus(Qt.TabFocusReason);
            }

            Navigation.cancelAction: root._onNavigationCancel

            onItemDoubleClicked: MediaLib.addAndPlay(model.id)
            onContextMenuButtonClicked: trackContextMenu.popup(trackSelectionModel.selectedIndexes, globalMousePos)
            onRightClick: trackContextMenu.popup(trackSelectionModel.selectedIndexes, globalMousePos)

            dragItem: Widgets.MLDragItem {
                mlModel: trackModel

                indexes: trackSelectionModel.selectedIndexes

                titleRole: "name"

                defaultCover: VLCStyle.noArtArtistCover
            }

            Widgets.TableColumns {
                id: tableColumns
            }

            Util.SelectableDelegateModel {
                id: trackSelectionModel

                model: trackModel
            }
        }
    }

    Widgets.StackViewExt {
        id: view

        anchors.fill: parent
        focus: albumModel.count !== 0
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
}
