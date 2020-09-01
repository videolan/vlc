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
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root

    property alias parentId: albumModel.parentId
    property var artist: ({})
    readonly property var currentIndex: headerItem.albumsListView.currentIndex || view.currentItem.currentIndex
    property Item headerItem: view.currentItem.headerItem
    //the index to "go to" when the view is loaded
    property var initialIndex: 0

    property Component header: FocusScope {
        property Item albumsListView: albumsLoader.item.albumsListView
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

                active: !medialib.gridView
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
                            subtitle: model.release_year || i18n.qtr("")
                            textHorizontalAlignment: Text.AlignHCenter
                            x: selectedBorderWidth
                            y: selectedBorderWidth
                            pictureWidth: VLCStyle.gridCover_music_width
                            pictureHeight: VLCStyle.gridCover_music_height
                            playCoverBorder.width: VLCStyle.gridCover_music_border

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
            var albumsListView = medialib.gridView ? view.currentItem : headerItem.albumsListView
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
        var albumsListView = medialib.gridView ? view.currentItem : headerItem.albumsListView
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

    MLAlbumTrackModel {
        id: trackModel

        ml: medialib
        parentId: root.parentId

        onCountChanged: {
            if (trackModel.count > 0) {
                root.resetFocus()
            }
        }
    }

    Component {
        id: gridComponent

        Widgets.ExpandGridView {
            id: gridView_id

            focus: true
            activeFocusOnTab:true
            cellWidth: VLCStyle.gridItem_music_width
            cellHeight: VLCStyle.gridItem_music_height
            headerDelegate: root.header
            delegateModel: albumSelectionModel
            model: albumModel

            delegate: AudioGridItem {
                id: audioGridItem

                opacity: gridView_id.expandIndex !== -1 && gridView_id.expandIndex !== audioGridItem.index ? .7 : 1

                onItemClicked : {
                    albumSelectionModel.updateSelection( modifier , gridView_id.currentIndex, index )
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
                if (albumSelectionModel.selectedIndexes.length <= 1) {
                    gridView_id.switchExpandItem( index )
                } else {
                    root._actionAtIndex( index, albumModel, albumSelectionModel )
                }
            }

            onSelectAll: albumSelectionModel.selectAll()
            onSelectionUpdated: albumSelectionModel.updateSelection( keyModifiers, oldIndex, newIndex )
            navigationParent: root
        }
    }

    Widgets.MenuExt {
        id: contextMenu

        property var model: ({})
        closePolicy: Popup.CloseOnReleaseOutside | Popup.CloseOnEscape

        Widgets.MenuItemExt {
            id: playMenuItem
            text: i18n.qtr("Play from start")
            onTriggered: {
                medialib.addAndPlay( contextMenu.model.id )
                history.push(["player"])
            }
        }

        Widgets.MenuItemExt {
            text: i18n.qtr("Enqueue")
            onTriggered: medialib.addToPlaylist( contextMenu.model.id )
        }

        onClosed: contextMenu.parent.forceActiveFocus()
    }

    Component {
        id: tableComponent

        Widgets.KeyNavigableTableView {
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

            sortModel:  [
                { isPrimary: true, criteria: "title", width: VLCStyle.colWidth(2), text: i18n.qtr("Title"), headerDelegate: tableColumns.titleHeaderDelegate, colDelegate: tableColumns.titleDelegate },
                { criteria: "album_title", width: VLCStyle.colWidth(Math.max(tableView_id._nbCols - 3, 1)), text: i18n.qtr("Album") },
                { criteria: "duration_short", width:VLCStyle.colWidth(1), showSection: "", headerDelegate: tableColumns.timeHeaderDelegate, colDelegate: tableColumns.timeColDelegate },
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
    }
}
