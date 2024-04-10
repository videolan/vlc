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
import QtQuick.Controls
import QtQuick
import QtQml.Models
import QtQuick.Layouts

import org.videolan.medialib 0.1
import org.videolan.vlc 0.1

import "qrc:///util/" as Util
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///widgets/" as Widgets
import "qrc:///main/" as MainInterface
import "qrc:///style/"

FocusScope {
    id: root

    property var artist: ({})

    //the index to "go to" when the view is loaded
    property int initialIndex: 0

    property Item headerItem: _currentView ? _currentView.headerItem : null

    property bool isSearchable: true

    property alias searchPattern: albumModel.searchPattern
    property alias sortOrder: albumModel.sortOrder
    property alias sortCriteria: albumModel.sortCriteria

    // current index of album model
    readonly property int currentIndex: {
        if (!_currentView)
           return -1
        else if (MainCtx.gridView)
           return _currentView.currentIndex
        else
           return headerItem.albumsListView.currentIndex
    }

    property alias rightPadding: view.rightPadding

    property alias _currentView: view.currentItem

    function navigationShowHeader(y, height) {
        const newContentY = Helpers.flickablePositionContaining(_currentView, y, height, 0, 0)

        if (newContentY !== _currentView.contentY)
            _currentView.contentY = newContentY
    }

    property Component header: FocusScope {
        id: headerFs

        property Item albumsListView: albumsLoader.status === Loader.Ready ? albumsLoader.item.albumsListView: null

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
            width: headerFs.width

            ArtistTopBanner {
                id: artistBanner

                focus: true
                width: headerFs.width

                rightPadding: root.rightPadding

                artist: root.artist

                onActiveFocusChanged: {
                    // make sure content is visible with activeFocus
                    if (activeFocus)
                        root.navigationShowHeader(0, height)
                }

                Navigation.parentItem: root
                Navigation.downAction: function() {
                    if (albumsListView)
                        albumsListView.setCurrentItemFocus(Qt.TabFocusReason);
                    else
                        _currentView.setCurrentItemFocus(Qt.TabFocusReason);

                }
            }

            Widgets.ViewHeader {
                view: root

                leftPadding: VLCStyle.margin_xlarge
                bottomPadding: VLCStyle.layoutTitle_bottom_padding -
                               (MainCtx.gridView ? 0 : VLCStyle.gridItemSelectedBorder)

                text: qsTr("Albums")
            }

            Loader {
                id: albumsLoader

                active: !MainCtx.gridView
                focus: true

                onActiveFocusChanged: {
                    // make sure content is visible with activeFocus
                    if (activeFocus)
                        root.navigationShowHeader(y, height)
                }

                sourceComponent: Column {
                    property alias albumsListView: albumsList

                    width: albumsList.width
                    height: implicitHeight

                    spacing: VLCStyle.tableView_spacing

                    Widgets.KeyNavigableListView {
                        id: albumsList

                        focus: true

                        height: VLCStyle.gridItem_music_height + topMargin + bottomMargin
                        width: root.width - root.rightPadding

                        leftMargin: VLCStyle.margin_xlarge
                        topMargin: VLCStyle.gridItemSelectedBorder
                        bottomMargin: VLCStyle.gridItemSelectedBorder
                        model: albumModel
                        selectionModel: albumSelectionModel
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

                            image: model.cover || ""
                            fallbackImage: VLCStyle.noArtAlbumCover

                            title: model.title || qsTr("Unknown title")
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

                            onItemClicked: (_,_, modifier) => {
                                albumsList.selectionModel.updateSelection( modifier , albumsList.currentIndex, index )
                                albumsList.currentIndex = index
                                albumsList.forceActiveFocus()
                            }

                            Connections {
                                target: albumsList.selectionModel

                                function onSelectionChanged() {
                                    gridItem.selected = albumsList.selectionModel.isSelected(index)
                                }
                            }

                            function play() {
                                if ( model.id !== undefined ) {
                                    MediaLib.addAndPlay( model.id )
                                }
                            }
                        }

                        onActionAtIndex: (index) => { albumModel.addAndPlay( new Array(index) ) }
                    }

                    Widgets.ViewHeader {
                        view: root

                        leftPadding: VLCStyle.margin_xlarge
                        topPadding: 0

                        text: qsTr("Tracks")
                    }
                }
            }
        }
    }

    focus: true

    onInitialIndexChanged: resetFocus()

    function setCurrentItemFocus(reason) {
        if (view.currentItem === null) {
            Qt.callLater(setCurrentItemFocus, reason)
            return
        }
        view.currentItem.setCurrentItemFocus(reason);
    }

    function resetFocus() {
        if (albumModel.count === 0) {
            return
        }
        let initialIndex = root.initialIndex
        if (initialIndex >= albumModel.count)
            initialIndex = 0
        albumSelectionModel.select(initialIndex, ItemSelectionModel.ClearAndSelect)
        const albumsListView = MainCtx.gridView ? _currentView : headerItem.albumsListView
        if (albumsListView) {
            albumsListView.currentIndex = initialIndex
            albumsListView.positionViewAtIndex(initialIndex, ItemView.Contain)
        }
    }

    function _actionAtIndex(index, model, selectionModel) {
        if (selectionModel.selectedIndexes.length > 1) {
            model.addAndPlay( selectionModel.selectedIndexes )
        } else {
            model.addAndPlay( new Array(index) )
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

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
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

    ListSelectionModel {
        id: albumSelectionModel
        model: albumModel
    }

    Widgets.MLDragItem {
        id: albumDragItem

        mlModel: albumModel
        indexes: indexesFlat ? albumSelectionModel.selectedIndexesFlat
                             : albumSelectionModel.selectedIndexes
        indexesFlat: !!albumSelectionModel.selectedIndexesFlat
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
            selectionModel: albumSelectionModel
            model: albumModel

            Connections {
                target: albumModel
                // selectionModel updates but doesn't trigger any signal, this forces selection update in view
                function onParentIdChanged() {
                    currentIndex = -1
                }
            }

            delegate: AudioGridItem {
                id: audioGridItem

                opacity: gridView_id.expandIndex !== -1 && gridView_id.expandIndex !== audioGridItem.index ? .7 : 1
                dragItem: albumDragItem

                onItemClicked : (_,_,modifier) => {
                    gridView_id.leftClickOnItem(modifier, index)
                }

                onItemDoubleClicked: {
                    gridView_id.switchExpandItem(index)
                }

                onContextMenuButtonClicked: (_, globalMousePos) => {
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

            onActionAtIndex: (index) => {
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

            clip: true // content may overflow if not enough space is provided
            model: trackModel

            onActionForSelection: {
                model.addAndPlay(selection)
            }

            header: root.header
            headerPositioning: ListView.InlineHeader
            rowHeight: VLCStyle.tableCoverRow_height

            property var _modelSmall: [{
                size: Math.max(2, tableView_id._nbCols),

                model: {
                    criteria: "title",

                    subCriterias: [ "duration", "album_title" ],

                    text: qsTr("Title"),

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: tableColumns.titleDelegate
                }
            }]

            property var _modelMedium: [{
                size: 2,

                model: {
                    criteria: "title",

                    text: qsTr("Title"),

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: tableColumns.titleDelegate
                }
            }, {
                size: Math.max(1, tableView_id._nbCols - 3),

                model: {
                    criteria: "album_title",

                    text: qsTr("Album")
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

            sortModel: (availableRowWidth < VLCStyle.colWidth(4)) ? _modelSmall
                                                                  : _modelMedium

            dragItem: tableDragItem

            Navigation.parentItem: root

            Navigation.upAction: function() {
                headerItem.setCurrentItemFocus(Qt.TabFocusReason);
            }

            Navigation.cancelAction: root._onNavigationCancel

            onItemDoubleClicked: MediaLib.addAndPlay(model.id)
            onContextMenuButtonClicked: trackContextMenu.popup(tableView_id.selectionModel.selectedIndexes, globalMousePos)
            onRightClick: trackContextMenu.popup(tableView_id.selectionModel.selectedIndexes, globalMousePos)

            onDragItemChanged: console.assert(tableView_id.dragItem === tableDragItem)

            Widgets.MLDragItem {
                id: tableDragItem

                mlModel: trackModel

                indexes: indexesFlat ? tableView_id.selectionModel.selectedIndexesFlat
                                     : tableView_id.selectionModel.selectedIndexes
                indexesFlat: !!tableView_id.selectionModel.selectedIndexesFlat

                defaultCover: VLCStyle.noArtArtistCover
            }

            Widgets.TableColumns {
                id: tableColumns

                showCriterias: (tableView_id.sortModel === tableView_id._modelSmall)
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
            function onGridViewChanged() {
                if (MainCtx.gridView)
                    view.replace(gridComponent)
                else
                    view.replace(tableComponent)
            }
        }
    }
}
