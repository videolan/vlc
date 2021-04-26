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
import QtQuick.Layouts 1.3

import org.videolan.medialib 0.1

import "qrc:///util/" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///main/" as MainInterface
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root

    property var artist: ({})

    //the index to "go to" when the view is loaded
    property var initialIndex: 0

    property Item headerItem: view.currentItem ? view.currentItem.headerItem : null

    // current index of album model
    readonly property int currentIndex: {
        if (!view.currentItem)
           return -1
        else if (mainInterface.gridView)
           return view.currentItem.currentIndex
        else
           return headerItem.albumsListView.currentIndex
    }

    property Component header: FocusScope {
        property Item albumsListView: albumsLoader.status === Loader.Ready ? albumsLoader.item.albumsListView: null
        property Item focusItem: albumsLoader.active ? albumsLoader.item.albumsListView : artistBanner

        focus: true
        height: col.height
        width: root.width

        Column {
            id: col

            height: childrenRect.height + VLCStyle.margin_normal
            width: root.width

            ArtistTopBanner {
                id: artistBanner

                focus: true
                width: root.width
                artist: root.artist
                navigationParent: root.navigationParent
                navigationLeftItem: root.navigationLeftItem
                navigationDown: function() {
                    artistBanner.focus = false
                    if (albumsListView)
                        albumsListView.forceActiveFocus()
                    else
                        view.currentItem.setCurrentItemFocus()

                }
            }

            Loader {
                id: albumsLoader

                active: !mainInterface.gridView
                focus: true
                sourceComponent: Column {
                    property alias albumsListView: albumsList

                    width: root.width
                    height: childrenRect.height

                    Widgets.SubtitleLabel {
                        id: albumsText

                        text: i18n.qtr("Albums")
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
                        spacing: VLCStyle.column_margin_width
                        navigationLeftItem: root.navigationLeftItem
                        navigationUpItem: artistBanner
                        navigationDown: function() {
                            albumsList.focus = false
                            view.currentItem.setCurrentItemFocus()
                        }

                        delegate: Widgets.GridItem {
                            image: model.cover || VLCStyle.noArtAlbum
                            title: model.title || i18n.qtr("Unknown title")
                            subtitle: model.release_year || ""
                            textHorizontalAlignment: Text.AlignHCenter
                            x: selectedBorderWidth
                            y: selectedBorderWidth
                            pictureWidth: VLCStyle.gridCover_music_width
                            pictureHeight: VLCStyle.gridCover_music_height
                            playCoverBorderWidth: VLCStyle.gridCover_music_border
                            dragItem: albumDragItem
                            unselectedUnderlay: shadows.unselected
                            selectedUnderlay: shadows.selected

                            onPlayClicked: play()
                            onItemDoubleClicked: play()

                            onItemClicked: {
                                albumSelectionModel.updateSelection( modifier , albumsList.currentIndex, index )
                                albumsList.currentIndex = index
                                albumsList.forceActiveFocus()
                            }

                            Connections {
                                target: albumSelectionModel

                                onSelectionChanged: selected = albumSelectionModel.isSelected(albumModel.index(index, 0))
                            }

                            function play() {
                                if ( model.id !== undefined ) {
                                    medialib.addAndPlay( model.id )
                                }
                            }
                        }

                        onSelectAll: albumSelectionModel.selectAll()
                        onSelectionUpdated: albumSelectionModel.updateSelection( keyModifiers, oldIndex, newIndex )
                        onActionAtIndex: medialib.addAndPlay( albumModel.getIdForIndex( index ) )

                        Widgets.GridShadows {
                            id: shadows

                            coverWidth: VLCStyle.gridCover_music_width
                            coverHeight: VLCStyle.gridCover_music_height
                        }
                    }

                    Widgets.SubtitleLabel {
                        id: tracksText

                        text: i18n.qtr("Tracks")
                        leftPadding: VLCStyle.margin_xlarge
                        topPadding: VLCStyle.margin_large
                    }
                }
            }
        }
    }

    focus: true
    navigationUpItem: headerItem.focusItem

    onInitialIndexChanged: resetFocus()
    onActiveFocusChanged: {
        if (activeFocus && albumModel.count > 0 && !albumSelectionModel.hasSelection) {
            var initialIndex = 0
            var albumsListView = mainInterface.gridView ? view.currentItem : headerItem.albumsListView
            if (albumsListView.currentIndex !== -1)
                initialIndex = albumsListView.currentIndex
            albumSelectionModel.select(albumModel.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect)
            albumsListView.currentIndex = initialIndex
        }
    }

    function resetFocus() {
        if (albumModel.count === 0) {
            return
        }
        var initialIndex = root.initialIndex
        if (initialIndex >= albumModel.count)
            initialIndex = 0
        albumSelectionModel.select(albumModel.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect)
        var albumsListView = mainInterface.gridView ? view.currentItem : headerItem.albumsListView
        if (albumsListView) {
            albumsListView.currentIndex = initialIndex
            albumsListView.positionViewAtIndex(initialIndex, ItemView.Contain)
        }
    }

    function _actionAtIndex(index, model, selectionModel) {
        if (selectionModel.selectedIndexes.length > 1) {
            medialib.addAndPlay( model.getIdsForIndexes( selectionModel.selectedIndexes ) )
        } else {
            medialib.addAndPlay( model.getIdForIndex(index) )
        }
    }

    MLAlbumModel {
        id: albumModel

        ml: medialib
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

    Widgets.DragItem {
        id: albumDragItem

        function updateComponents(maxCovers) {
          var items = albumSelectionModel.selectedIndexes.slice(0, maxCovers).map(function (x){
            return albumModel.getDataAt(x.row)
          })
          var title = items.map(function (item){ return item.title}).join(", ")
          var covers = items.map(function (item) { return {artwork: item.cover || VLCStyle.noArtAlbum}})
          return {
            covers: covers,
            title: title,
            count: albumSelectionModel.selectedIndexes.length
          }
        }

        function getSelectedInputItem() {
            return albumModel.getItemsForIndexes(albumSelectionModel.selectedIndexes);
        }
    }

    MLAlbumTrackModel {
        id: trackModel

        ml: medialib
        parentId: albumModel.parentId
    }

    AlbumContextMenu {
        id: contextMenu
        model: albumModel
    }

    AlbumTrackContextMenu {
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
            delegateModel: albumSelectionModel
            model: albumModel

            Connections {
                target: root
                // selectionModel updates but doesn't trigger any signal, this forces selection update in view
                onParentIdChanged: currentIndex = -1
            }

            delegate: AudioGridItem {
                id: audioGridItem

                opacity: gridView_id.expandIndex !== -1 && gridView_id.expandIndex !== audioGridItem.index ? .7 : 1
                dragItem: albumDragItem
                unselectedUnderlay: shadows.unselected
                selectedUnderlay: shadows.selected

                onItemClicked : gridView_id.leftClickOnItem(modifier, index)

                onItemDoubleClicked: {
                    if ( model.id !== undefined ) { medialib.addAndPlay( model.id ) }
                }

                onContextMenuButtonClicked: {
                    gridView_id.rightClickOnItem(index)
                    contextMenu.popup(albumSelectionModel.selectedIndexes, globalMousePos, { "information" : index})
                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: 100
                    }
                }
            }

            expandDelegate: MusicAlbumsGridExpandDelegate {
                id: expandDelegateId

                x: 0
                width: gridView_id.width
                onRetract: gridView_id.retract()
                navigationParent: root
                navigationCancel:  function() {  gridView_id.retract() }
                navigationUp: function() {  gridView_id.retract() }
                navigationDown: function() {}
            }

            onActionAtIndex: {
                if (albumSelectionModel.selectedIndexes.length <= 1) {
                    gridView_id.switchExpandItem( index )
                } else {
                    root._actionAtIndex( index, albumModel, albumSelectionModel )
                }
            }

            onSelectAll: albumSelectionModel.selectAll()
            onSelectionUpdated: albumSelectionModel.updateSelection( keyModifiers, oldIndex, newIndex )
            navigationParent: root

            Connections {
                target: contextMenu
                onShowMediaInformation: gridView_id.switchExpandItem( index )
            }

            Widgets.GridShadows {
                id: shadows

                coverWidth: VLCStyle.gridCover_music_width
                coverHeight: VLCStyle.gridCover_music_height
            }
        }

    }

    Component {
        id: tableComponent

        MainInterface.MainTableView {
            id: tableView_id

            readonly property int _nbCols: VLCStyle.gridColumnsForWidth(tableView_id.availableRowWidth)

            model: trackModel
            selectionDelegateModel: trackSelectionModel
            headerColor: VLCStyle.colors.bg
            onActionForSelection: {
                medialib.addAndPlay( model.getIdsForIndexes( selection ) )
            }
            navigationParent: root
            header: root.header
            headerPositioning: ListView.InlineHeader
            rowHeight: VLCStyle.tableCoverRow_height

            sortModel:  [
                { isPrimary: true, criteria: "title", width: VLCStyle.colWidth(2), text: i18n.qtr("Title"), headerDelegate: tableColumns.titleHeaderDelegate, colDelegate: tableColumns.titleDelegate },
                { criteria: "album_title", width: VLCStyle.colWidth(Math.max(tableView_id._nbCols - 3, 1)), text: i18n.qtr("Album") },
                { criteria: "duration", width:VLCStyle.colWidth(1), showSection: "", headerDelegate: tableColumns.timeHeaderDelegate, colDelegate: tableColumns.timeColDelegate },
            ]

            navigationCancel: function() {
                if (tableView_id.currentIndex <= 0)
                    defaultNavigationCancel()
                else
                    tableView_id.currentIndex = 0;
            }

            onItemDoubleClicked: medialib.addAndPlay(model.id)
            onContextMenuButtonClicked: trackContextMenu.popup(trackSelectionModel.selectedIndexes, menuParent.mapToGlobal(0,0))
            onRightClick: trackContextMenu.popup(trackSelectionModel.selectedIndexes, globalMousePos)
            dragItem: Widgets.DragItem {
                function updateComponents(maxCovers) {
                  var items = trackSelectionModel.selectedIndexes.slice(0, maxCovers).map(function (x){
                    return trackModel.getDataAt(x.row)
                  })
                  var title = items.map(function (item){ return item.title}).join(", ")
                  var covers = items.map(function (item) { return {artwork: item.cover || VLCStyle.noArtCover}})
                  return {
                    covers: covers,
                    title: title,
                    count: trackSelectionModel.selectedIndexes.length
                  }
                }

                function getSelectedInputItem() {
                    return trackModel.getItemsForIndexes(trackSelectionModel.selectedIndexes);
                }
            }

            Widgets.TableColumns {
                id: tableColumns
            }

            function setCurrentItemFocus() {
                positionViewAtIndex(currentIndex, ItemView.Contain)
                currentItem.forceActiveFocus()
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
        initialItem: mainInterface.gridView ? gridComponent : tableComponent

        Connections {
            target: mainInterface
            onGridViewChanged: {
                if (mainInterface.gridView)
                    view.replace(gridComponent)
                else
                    view.replace(tableComponent)
            }
        }
    }
}
