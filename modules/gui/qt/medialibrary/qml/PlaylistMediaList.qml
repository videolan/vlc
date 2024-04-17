/*****************************************************************************
 * Copyright (C) 2021-23 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models

import org.videolan.medialib 0.1
import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///main/"    as MainInterface
import "qrc:///util/"    as Util
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///style/"

MainInterface.MainViewLoader {
    id: root

    //---------------------------------------------------------------------------------------------
    // Properties
    //---------------------------------------------------------------------------------------------

    property bool isMusic: false

    readonly property int currentIndex: currentItem?.currentIndex ?? -1

    property Component header: null
    
    readonly property int contentLeftMargin: currentItem?.contentLeftMargin ?? 0
    readonly property int contentRightMargin: currentItem?.contentRightMargin ?? 0

    property alias searchPattern: playlistModel.searchPattern
    property alias sortOrder: playlistModel.sortOrder
    property alias sortCriteria: playlistModel.sortCriteria

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

    // FIXME: remove this
    property var _currentView: currentItem

    //---------------------------------------------------------------------------------------------
    // Signals
    //---------------------------------------------------------------------------------------------

    signal showList(var model, int reason)

    //---------------------------------------------------------------------------------------------
    // Events
    //---------------------------------------------------------------------------------------------

    // NOTE: Define the initial position and selection. This is done on activeFocus rather than
    //       Component.onCompleted because selectionModel.selectedGroup update itself after this
    //       event.
    onActiveFocusChanged: {
        if (activeFocus == false || model.count === 0 || selectionModel.hasSelection)
            return;

        resetFocus()
    }


    //---------------------------------------------------------------------------------------------
    // Private

    grid: grid
    list: table
    emptyLabel: emptyLabel

    isSearchable: true
    sortModel: [{ text: qsTr("Alphabetic"), criteria: "title" }]


    model: MLPlaylistListModel {
        id: playlistModel
        ml: MediaLib

        playlistType: isMusic ? MLPlaylistListModel.PLAYLIST_TYPE_AUDIO
                              : MLPlaylistListModel.PLAYLIST_TYPE_VIDEO

        coverSize: (isMusic) ? Qt.size(512, 512)
                             : Qt.size(1024, 640)

        coverDefault: root._placeHolder

        coverPrefix: (isMusic) ? "playlist-music" : "playlist-video"
    }

    function _actionAtIndex() {
        if (root.selectionModel.selectedIndexes.length > 1) {
            model.addAndPlay( selectionModel.selectedIndexes );
        } else if (root.selectionModel.selectedIndexes.length === 1) {
            const index = selectionModel.selectedIndexes[0];
            showList(model.getDataAt(index), Qt.TabFocusReason);
        }
    }

    function _getCount(model) {
        const count = model.count;

        if (count < 100)
            return count;
        else
            return qsTr("99+");
    }

    //---------------------------------------------------------------------------------------------
    // Childs
    //---------------------------------------------------------------------------------------------


    Widgets.MLDragItem {
        id: dragItemPlaylist

        mlModel: model

        indexes: indexesFlat ? root.selectionModel.selectedIndexesFlat
                             : root.selectionModel.selectedIndexes
        indexesFlat: !!root.selectionModel.selectedIndexesFlat

        coverRole: "thumbnail"

        defaultCover: root._placeHolder
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

            model: root.model

            selectionModel: root.selectionModel

            headerDelegate: root.header

            Navigation.parentItem: root

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
                                    : qsTr("Unknown title")

                labels: (model.count > 1) ? [ qsTr("%1 Tracks").arg(_getCount(model)) ]
                                          : [ qsTr("%1 Track") .arg(_getCount(model)) ]

                dragItem: dragItemPlaylist


                //---------------------------------------------------------------------------------
                // Events

                onItemClicked: (_,_,modifier) => { gridView.leftClickOnItem(modifier, index) }

                onItemDoubleClicked: (_,_,modifier) => { showList(model, Qt.MouseFocusReason) }

                onPlayClicked: if (model.id) MediaLib.addAndPlay(model.id)

                onContextMenuButtonClicked: (_, globalMousePos) => {
                    gridView.rightClickOnItem(index);

                    contextMenu.popup(selectionModel.selectedRows(), globalMousePos);
                }

                //---------------------------------------------------------------------------------
                // Animations

                Behavior on opacity { NumberAnimation { duration: VLCStyle.duration_short } }
            }

            //-------------------------------------------------------------------------------------
            // Events

            // NOTE: Define the initial position and selection. This is done on activeFocus rather
            //       than Component.onCompleted because selectionModel.selectedGroup update itself
            //       after this event.
            onActiveFocusChanged: {
                if (activeFocus == false || model.count === 0 || selectionModel.hasSelection)
                    return;

                selectionModel.select(model.index(0,0), ItemSelectionModel.ClearAndSelect)
            }

            onActionAtIndex: _actionAtIndex()

            //-------------------------------------------------------------------------------------
            // Childs

        }
    }

    Component {
        id: table

        MainInterface.MainTableView {
            id: tableView

            //-------------------------------------------------------------------------------------
            // Properties

            property int _columns: Math.max(1, VLCStyle.gridColumnsForWidth(availableRowWidth) - 2)

            property var _modelSmall: [{
                size: Math.max(2, _columns),

                model: {
                    criteria: "name",

                    subCriterias: [ "count" ],

                    text: qsTr("Name"),

                    headerDelegate: columns.titleHeaderDelegate,
                    colDelegate   : columns.titleDelegate
                }
            }]

            property var _modelMedium: [{
                size: 1,

                model: {
                    criteria: "thumbnail",

                    text: qsTr("Cover"),

                    headerDelegate: columns.titleHeaderDelegate,
                    colDelegate   : columns.titleDelegate
                }
            }, {
                size: _columns,

                model: {
                    criteria: "name",

                    text: qsTr("Name")
                }
            }, {
                size: 1,

                model: {
                    criteria: "count",

                    text: qsTr("Tracks")
                }
            }]

            //-------------------------------------------------------------------------------------
            // Settings

            rowHeight: VLCStyle.tableCoverRow_height

            model: root.model

            sortModel: (availableRowWidth < VLCStyle.colWidth(4)) ? _modelSmall
                                                                  : _modelMedium

            selectionModel: root.selectionModel

            dragItem: dragItemPlaylist

            header: root.header

            Navigation.parentItem: root

            //-------------------------------------------------------------------------------------
            // Events

            onActionForSelection: _actionAtIndex()

            onItemDoubleClicked: (_, model) => showList(model, Qt.MouseFocusReason)

            onContextMenuButtonClicked: (_, _, globalMousePos) => {
                contextMenu.popup(selectionModel.selectedRows(), globalMousePos)
            }

            onRightClick: (_, _, globalMousePos) => {
                contextMenu.popup(selectionModel.selectedRows(), globalMousePos)
            }

            //-------------------------------------------------------------------------------------
            // Childs

            Widgets.TableColumns {
                id: columns

                showTitleText: (tableView.sortModel === tableView._modelSmall)
                showCriterias: showTitleText

                criteriaCover: "thumbnail"

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

    Component {
        id: emptyLabel

        Widgets.EmptyLabelHint {
            visible: (model.count === 0)

            focus: true

            text: qsTr("No playlists found")
            hint: qsTr("Right click on a media to add it to a playlist")

            cover: VLCStyle.noArtAlbumCover
        }
    }
}
