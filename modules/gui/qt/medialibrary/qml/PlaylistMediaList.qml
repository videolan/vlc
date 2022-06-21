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
import QtQuick.Layouts  1.11
import QtQml.Models     2.2

import org.videolan.medialib 0.1
import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///main/"    as MainInterface
import "qrc:///util/"    as Util
import "qrc:///style/"

FocusScope {
    id: root

    //---------------------------------------------------------------------------------------------
    // Properties
    //---------------------------------------------------------------------------------------------

    readonly property int currentIndex: _currentView.currentIndex

    property bool isMusic: false

    property int initialIndex: 0

    property var sortModel: [{ text: I18n.qtr("Alphabetic"), criteria: "title" }]

    //---------------------------------------------------------------------------------------------
    // Private

    property int _width: (isMusic) ? VLCStyle.gridItem_music_width
                                   : VLCStyle.gridItem_video_width

    property int _height: (isMusic) ? VLCStyle.gridItem_music_height
                                    : VLCStyle.gridItem_video_height

    property int _widthCover: (isMusic) ? VLCStyle.gridCover_music_width
                                        : VLCStyle.gridCover_video_width

    property int _heightCover: (isMusic) ? VLCStyle.gridCover_music_height
                                         : VLCStyle.gridCover_video_height

    property string _placeHolder: (isMusic) ? VLCStyle.noArtAlbumCover
                                            : VLCStyle.noArtVideoCover

    //---------------------------------------------------------------------------------------------
    // Alias
    //---------------------------------------------------------------------------------------------

    property alias model: model

    property alias _currentView: view.currentItem

    //---------------------------------------------------------------------------------------------
    // Signals
    //---------------------------------------------------------------------------------------------

    signal showList(var model, int reason)

    //---------------------------------------------------------------------------------------------
    // Events
    //---------------------------------------------------------------------------------------------

    // NOTE: Define the initial position and selection. This is done on activeFocus rather than
    //       Component.onCompleted because modelSelect.selectedGroup update itself after this
    //       event.
    onActiveFocusChanged: {
        if (activeFocus == false || model.count === 0 || modelSelect.hasSelection)
            return;

        var initialIndex = 0;

        if (_currentView.currentIndex !== -1) {
            initialIndex = _currentView.currentIndex;
        }

        modelSelect.select(model.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect);

        _currentView.currentIndex = initialIndex;
    }

    onInitialIndexChanged: resetFocus()

    //---------------------------------------------------------------------------------------------
    // Connections
    //---------------------------------------------------------------------------------------------

    Connections {
        target: MainCtx

        onGridViewChanged: {
            if (MainCtx.gridView) view.replace(grid);
            else                        view.replace(table);
        }
    }

    //---------------------------------------------------------------------------------------------
    // Functions
    //---------------------------------------------------------------------------------------------

    function resetFocus() {
        if (model.count === 0)
            return;

        var initialIndex = root.initialIndex;

        if (initialIndex >= model.count)
            initialIndex = 0;

        modelSelect.select(model.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect);

        if (_currentView)
            _currentView.positionViewAtIndex(initialIndex, ItemView.Contain);
    }

    function setCurrentItemFocus(reason) {
        _currentView.setCurrentItemFocus(reason);
    }

    //---------------------------------------------------------------------------------------------
    // Private

    function _actionAtIndex() {
        if (modelSelect.selectedIndexes.length > 1) {
            MediaLib.addAndPlay(model.getIdsForIndexes(modelSelect.selectedIndexes));
        } else if (modelSelect.selectedIndexes.length === 1) {
            var index = modelSelect.selectedIndexes[0];
            showList(model.getDataAt(index), Qt.TabFocusReason);
        }
    }

    function _getCount(model)
    {
        var count = model.count;

        if (count < 100)
            return count;
        else
            return I18n.qtr("99+");
    }

    function _onNavigationCancel() {
        if (_currentView.currentIndex <= 0) {
            Navigation.defaultNavigationCancel()
        } else {
            _currentView.currentIndex = 0;
            _currentView.positionViewAtIndex(0, ItemView.Contain);
        }
    }

    //---------------------------------------------------------------------------------------------
    // Childs
    //---------------------------------------------------------------------------------------------

    MLPlaylistListModel {
        id: model

        ml: MediaLib

        coverSize: (isMusic) ? Qt.size(512, 512)
                             : Qt.size(1024, 640)

        coverDefault: root._placeHolder

        coverPrefix: (isMusic) ? "playlist-music" : "playlist-video"

        onCountChanged: {
            if (count === 0 || modelSelect.hasSelection)
                return;

            resetFocus();
        }
    }

    Widgets.StackViewExt {
        id: view

        anchors.fill: parent

        initialItem: (MainCtx.gridView) ? grid : table

        focus: (model.count !== 0)
    }

    Widgets.MLDragItem {
        id: dragItemPlaylist

        mlModel: model

        indexes: modelSelect.selectedIndexes

        coverRole: "thumbnail"

        defaultCover: root._placeHolder

        titleRole: "name"
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

            selectionDelegateModel: modelSelect

            Navigation.parentItem: root

            Navigation.cancelAction: root._onNavigationCancel

            delegate: VideoGridItem {
                //---------------------------------------------------------------------------------
                // Properties

                property var model: ({})

                property int index: -1

                //---------------------------------------------------------------------------------
                // Settings

                pictureWidth : _widthCover
                pictureHeight: _heightCover

                title: (model.name) ? model.name
                                    : I18n.qtr("Unknown title")

                labels: (model.count > 1) ? [ I18n.qtr("%1 Tracks").arg(_getCount(model)) ]
                                          : [ I18n.qtr("%1 Track") .arg(_getCount(model)) ]

                dragItem: dragItemPlaylist

                selectedUnderlay  : shadows.selected
                unselectedUnderlay: shadows.unselected

                //---------------------------------------------------------------------------------
                // Events

                onItemClicked: gridView.leftClickOnItem(modifier, index)

                onItemDoubleClicked: showList(model, Qt.MouseFocusReason)

                onPlayClicked: if (model.id) MediaLib.addAndPlay(model.id)

                onContextMenuButtonClicked: {
                    gridView.rightClickOnItem(index);

                    contextMenu.popup(modelSelect.selectedIndexes, globalMousePos);
                }

                //---------------------------------------------------------------------------------
                // Animations

                Behavior on opacity { NumberAnimation { duration: VLCStyle.duration_short } }
            }

            //-------------------------------------------------------------------------------------
            // Events

            // NOTE: Define the initial position and selection. This is done on activeFocus rather
            //       than Component.onCompleted because modelSelect.selectedGroup update itself
            //       after this event.
            onActiveFocusChanged: {
                if (activeFocus == false || model.count === 0 || modelSelect.hasSelection)
                    return;

                modelSelect.select(model.index(0,0), ItemSelectionModel.ClearAndSelect)
            }

            onActionAtIndex: _actionAtIndex()

            //-------------------------------------------------------------------------------------
            // Childs

            Widgets.GridShadows {
                id: shadows

                coverWidth : _widthCover
                coverHeight: _heightCover
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

            dragItem: dragItemPlaylist

            headerColor: VLCStyle.colors.bg

            sortModel: [{
                isPrimary: true,
                criteria: "thumbnail",

                width: VLCStyle.listAlbumCover_width,

                headerDelegate: columns.titleHeaderDelegate,
                colDelegate   : columns.titleDelegate
            }, {
                criteria: "name",

                width: VLCStyle.colWidth(_widthName),

                text: I18n.qtr("Name")
            }, {
                criteria: "count",

                width: VLCStyle.colWidth(1),

                text: I18n.qtr("Tracks")
            }]

            Navigation.parentItem: root
            Navigation.cancelAction: root._onNavigationCancel

            //-------------------------------------------------------------------------------------
            // Events

            onActionForSelection: _actionAtIndex()

            onItemDoubleClicked: showList(model, Qt.MouseFocusReason)

            onContextMenuButtonClicked: contextMenu.popup(modelSelect.selectedIndexes,
                                                          globalMousePos)

            onRightClick: contextMenu.popup(modelSelect.selectedIndexes, globalMousePos)

            //-------------------------------------------------------------------------------------
            // Childs

            Widgets.TableColumns {
                id: columns

                showTitleText: false

                //---------------------------------------------------------------------------------
                // NOTE: When it's music we want the cover to be square

                titleCover_width: (isMusic) ? VLCStyle.trackListAlbumCover_width
                                            : VLCStyle.listAlbumCover_width

                titleCover_height: (isMusic) ? VLCStyle.trackListAlbumCover_heigth
                                             : VLCStyle.listAlbumCover_height

                titleCover_radius: (isMusic) ? VLCStyle.trackListAlbumCover_radius
                                             : VLCStyle.listAlbumCover_radius

                //---------------------------------------------------------------------------------

                // NOTE: This makes sure we display the playlist count on the item.
                function titlecoverLabels(model) {
                    return [ _getCount(model) ];
                }
            }
        }
    }

    EmptyLabelButton {
        anchors.fill: parent

        visible: (model.count === 0)

        focus: visible

        text: I18n.qtr("No playlists found")

        cover: VLCStyle.noArtAlbumCover

        Navigation.parentItem: root
    }
}
