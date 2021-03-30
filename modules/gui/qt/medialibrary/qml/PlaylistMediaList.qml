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

import QtQuick          2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts  1.3
import QtQml.Models     2.2

import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///main/"    as MainInterface
import "qrc:///util/"    as Util
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root

    //---------------------------------------------------------------------------------------------
    // Properties
    //---------------------------------------------------------------------------------------------

    readonly property int currentIndex: currentItem.currentIndex

    property int initialIndex: 0

    property var sortModel: [{ text: i18n.qtr("Alphabetic"), criteria: "title" }]

    //---------------------------------------------------------------------------------------------
    // Private

    property int _width: VLCStyle.colWidth(2)

    property int _height: Math.round(_width / 2)

    property int _widthColumn:
        Math.max(VLCStyle.gridColumnsForWidth(tableView.availableRowWidth
                                              - VLCStyle.listAlbumCover_width
                                              - VLCStyle.column_margin_width) - 1, 1)

    //---------------------------------------------------------------------------------------------
    // Alias
    //---------------------------------------------------------------------------------------------

    property alias model: model

    property alias currentItem: view.currentItem

    //---------------------------------------------------------------------------------------------
    // Signals
    //---------------------------------------------------------------------------------------------

    signal showList(variant model)

    //---------------------------------------------------------------------------------------------
    // Settings
    //---------------------------------------------------------------------------------------------

    navigationCancel: function() {
        if (currentItem.currentIndex > 0) {
            currentItem.currentIndex = 0;
        } else {
            defaultNavigationCancel();
        }
    }

    //---------------------------------------------------------------------------------------------
    // Events
    //---------------------------------------------------------------------------------------------

    // NOTE: Define the initial position and selection. This is done on activeFocus rather than
    //       Component.onCompleted because modelSelect.selectedGroup update itself after this
    //       event.
    onActiveFocusChanged: {
        if (activeFocus == false || model.count === 0 || modelSelect.hasSelection) return;

        var initialIndex = 0;

        if (currentItem.currentIndex !== -1) {
            initialIndex = currentItem.currentIndex;
        }

        modelSelect.select(model.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect);

        currentItem.currentIndex = initialIndex;
    }

    onInitialIndexChanged: resetFocus()

    //---------------------------------------------------------------------------------------------
    // Connections
    //---------------------------------------------------------------------------------------------

    Connections {
        target: mainInterface

        onGridViewChanged: {
            if (mainInterface.gridView) view.replace(grid);
            else                        view.replace(table);
        }
    }

    //---------------------------------------------------------------------------------------------
    // Functions
    //---------------------------------------------------------------------------------------------

    function resetFocus() {
        if (model.count === 0) return;

        var initialIndex = root.initialIndex;

        if (initialIndex >= model.count)
            initialIndex = 0;

        modelSelect.select(model.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect);

        if (currentItem)
            currentItem.positionViewAtIndex(initialIndex, ItemView.Contain);
    }

    //---------------------------------------------------------------------------------------------
    // Private

    function _actionAtIndex() {
        if (modelSelect.selectedIndexes.length > 1) {
            medialib.addAndPlay(model.getIdsForIndexes(modelSelect.selectedIndexes));
        } else if (modelSelect.selectedIndexes.length === 1) {
            var index = modelSelect.selectedIndexes[0];
            showList(model.getDataAt(index));
        }
    }

    //---------------------------------------------------------------------------------------------
    // Childs
    //---------------------------------------------------------------------------------------------

    MLPlaylistListModel {
        id: model

        ml: medialib

        onCountChanged: {
            if (count === 0 || modelSelect.hasSelection) return;

            resetFocus();
        }
    }

    Widgets.StackViewExt {
        id: view

        anchors.fill: parent

        initialItem: (mainInterface.gridView) ? grid : table

        focus: (model.count !== 0)
    }

    Widgets.DragItem {
        id: dragItemPlaylist

        //---------------------------------------------------------------------------------------------
        // DragItem implementation

        function updateComponents(maxCovers) {
            var items = modelSelect.selectedIndexes.slice(0, maxCovers).map(function (x){
                return model.getDataAt(x.row);
            })

            var covers = items.map(function (item) {
                return { artwork: item.cover || VLCStyle.noArtCover };
            })

            var title = items.map(function (item) {
                return item.name
            }).join(", ");

            return {
                covers: covers,
                title: title,
                count: modelSelect.selectedIndexes.length
            };
        }

        function getSelectedInputItem() {
            return model.getItemsForIndexes(modelSelect.selectedIndexes);
        }
    }

    Util.SelectableDelegateModel {
        id: modelSelect

        model: root.model
    }

    PlaylistListContextMenu {
        id: contextMenu

        model: root.model
    }

    // TBD: Refactor this with MusicGenres ?
    Component {
        id: grid

        MainInterface.MainGridView {
            id: gridView

            //-------------------------------------------------------------------------------------
            // Settings

            cellWidth : _width
            cellHeight: _height

            topMargin: VLCStyle.margin_large

            model: root.model

            delegateModel: modelSelect

            navigationParent: root

            focus: true

            //-------------------------------------------------------------------------------------
            // Events

            onSelectAll: modelSelect.selectAll()

            onSelectionUpdated: modelSelect.updateSelection(keyModifiers, oldIndex, newIndex)

            onActionAtIndex: _actionAtIndex()

            //-------------------------------------------------------------------------------------
            // Childs

            delegate: Widgets.GridItem {
                id: item

                //---------------------------------------------------------------------------------
                // Properties

                property var model: ({})

                property int index: -1

                //---------------------------------------------------------------------------------
                // Settings

                width : _width
                height: _height

                pictureWidth : width
                pictureHeight: height

                image: VLCStyle.noArtAlbum

                dragItem: dragItemPlaylist

                pictureOverlay: Item {
                    Column {
                        anchors.centerIn: parent

                        Label {
                             width: item.width

                             horizontalAlignment: Text.AlignHCenter

                             text: model.name

                             elide: Text.ElideRight

                             color: "white"

                             font.pixelSize: VLCStyle.fontSize_large
                             font.weight   : Font.DemiBold
                        }

                        Widgets.CaptionLabel {
                            width: item.width

                            horizontalAlignment: Text.AlignHCenter

                            opacity: 0.7

                            text: (model.count > 1) ? i18n.qtr("%1 Tracks").arg(model.count)
                                                    : i18n.qtr("%1 Track") .arg(model.count)

                            color: "white"
                        }
                    }
                }

                playCoverBorder.width: VLCStyle.dp(3, VLCStyle.scale)

                //---------------------------------------------------------------------------------
                // Events

                onItemDoubleClicked: showList(model)

                onItemClicked: gridView.leftClickOnItem(modifier, index)

                onPlayClicked: if (model.id) medialib.addAndPlay(model.id)

                onContextMenuButtonClicked: {
                    gridView.rightClickOnItem(index);

                    contextMenu.popup(modelSelect.selectedIndexes, globalMousePos);
                }
            }
        }
    }

    Component {
        id: table

        MainInterface.MainTableView {
            id: tableView

            //-------------------------------------------------------------------------------------
            // Properties

            property int _widthName:
                Math.max(VLCStyle.gridColumnsForWidth(tableView.availableRowWidth
                                                      - VLCStyle.listAlbumCover_width
                                                      - VLCStyle.column_margin_width) - 1, 1)

            //-------------------------------------------------------------------------------------
            // Settings

            rowHeight: VLCStyle.tableCoverRow_height

            headerTopPadding: VLCStyle.margin_normal

            model: root.model

            selectionDelegateModel: modelSelect

            navigationParent: root

            dragItem: dragItemPlaylist

            focus: true

            headerColor: VLCStyle.colors.bg

            sortModel: [{
                isPrimary: true,
                criteria: "cover",

                width: VLCStyle.listAlbumCover_width,

                headerDelegate: columns.titleHeaderDelegate,
                colDelegate   : columns.titleDelegate
            }, {
                criteria: "name",

                width: VLCStyle.colWidth(_widthName),

                text: i18n.qtr("Name")
            }, {
                criteria: "count",

                width: VLCStyle.colWidth(1),

                text: i18n.qtr("Tracks")
            }]

            //-------------------------------------------------------------------------------------
            // Events

            onActionForSelection: _actionAtIndex()

            onItemDoubleClicked: showList(model)

            onContextMenuButtonClicked: contextMenu.popup(modelSelect.selectedIndexes,
                                                          menuParent.mapToGlobal(0,0))

            onRightClick: contextMenu.popup(modelSelect.selectedIndexes, globalMousePos)

            //-------------------------------------------------------------------------------------
            // Childs

            Widgets.TableColumns {
                id: columns

                showTitleText: false

                titleCover_width : VLCStyle.listAlbumCover_width
                titleCover_height: VLCStyle.listAlbumCover_height
                titleCover_radius: VLCStyle.listAlbumCover_radius
            }
        }
    }

    EmptyLabel {
        anchors.fill: parent

        visible: (model.count === 0)

        focus: visible

        text: i18n.qtr("No playlists found")

        cover: VLCStyle.noArtAlbumCover

        navigationParent: root
    }
}
