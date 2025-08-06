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

import VLC.MediaLibrary

import VLC.Widgets as Widgets
import VLC.MainInterface
import VLC.Util
import VLC.Style
import VLC.Dialogs

MainViewLoader {
    id: root

    //---------------------------------------------------------------------------------------------
    // Properties
    //---------------------------------------------------------------------------------------------

    property bool isMusic: false

    readonly property int currentIndex: currentItem?.currentIndex ?? -1

    property Component header: null

    readonly property int contentLeftMargin: currentItem?.contentLeftMargin ?? 0
    readonly property int contentRightMargin: currentItem?.contentRightMargin ?? 0

    property int displayMarginBeginning: 0
    property int displayMarginEnd: 0

    // Currently only respected by the list view:
    property bool enableBeginningFade: true
    property bool enableEndFade: true

    property alias searchPattern: playlistModel.searchPattern
    property alias sortOrder: playlistModel.sortOrder
    property alias sortCriteria: playlistModel.sortCriteria

    //---------------------------------------------------------------------------------------------
    // Private

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

    Component.onCompleted: {
        root.Keys.deletePressed.connect(root.deleteSelectedPlaylists)
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

        function onBusynessChanged() {
            if (transactionPending || loading) {
                MainCtx.setCursor(root, Qt.BusyCursor)
                visibilityTimer.start()
            } else {
                visibilityTimer.stop()
                progressIndicator.visible = false
                MainCtx.unsetCursor(root)
            }
        }

        Component.onCompleted: {
            playlistModel.transactionPendingChanged.connect(playlistModel.onBusynessChanged)
            playlistModel.loadingChanged.connect(playlistModel.onBusynessChanged)
            playlistModel.onBusynessChanged()
        }
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

    function _adjustDragAccepted(drag) {
        if (!root.model || root.model.transactionPending)
        {
            drag.accepted = false
            return
        }

        if (drag.source !== dragItemPlaylist && Helpers.isValidInstanceOf(drag.source, Widgets.DragItem))
            drag.accepted = true
        else if (drag.hasUrls)
            drag.accepted = true
        else {
            drag.accepted = false
        }
    }

    function _dropAction(drop, index) {
        const item = drop.source
        if (Helpers.isValidInstanceOf(item, Widgets.DragItem)) {
            drop.accepted = true
            return item.getSelectedInputItem().then(inputItems => {
                if (index === undefined)
                    DialogsProvider.playlistsDialog(inputItems, (root.model as MLPlaylistListModel).playlistType)
                else
                    root.model.append(root.model.getItemId(index), inputItems)
            })
        } else if (drop.hasUrls) {
            drop.accepted = true
            const urlList = []
            for (let url in drop.urls)
                urlList.push(drop.urls[url])
            if (index === undefined)
                DialogsProvider.playlistsDialog(inputItems, (root.model as MLPlaylistListModel).playlistType)
            else
                root.model.append(root.model.getItemId(index), urlList)
        } else {
            drop.accepted = false
        }
        return Promise.resolve()
    }

    function deleteSelectedPlaylists() {
        console.assert(root.model)
        console.assert(root.selectionModel)
        if (root.selectionModel.hasSelection) {
            const items = []
            for (const i of root.selectionModel.selectedIndexes) {
                items.push(root.model.data(i, MLPlaylistListModel.PLAYLIST_ID))
            }
            console.assert(items.length > 0)
            if (DialogsProvider.questionDialog(qsTr("Do you really want to delete the selected playlist(s)?"),
                                               qsTr("Delete playlist(s)")))
                root.model.deletePlaylists(items)
        } else {
            console.warn(root, ": Nothing to delete")
        }
    }

    //---------------------------------------------------------------------------------------------
    // Childs
    //---------------------------------------------------------------------------------------------

    DropArea {
        anchors.fill: parent

        onEntered: function(drag) {
            root._adjustDragAccepted(drag)
        }

        onDropped: function(drop) {
            root._dropAction(drop)
        }
    }

    Widgets.ProgressIndicator {
        id: progressIndicator
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: VLCStyle.margin_small

        visible: false

        z: 99

        text: root.model?.transactionPending ? qsTr("Processing...") : ""

        Timer {
            id: visibilityTimer

            interval: VLCStyle.duration_humanMoment

            onTriggered: {
                progressIndicator.visible = true
            }
        }
    }

    Widgets.MLDragItem {
        id: dragItemPlaylist

        objectName: "PlaylistMediaListDragItem"

        view: root.currentItem

        indexes: indexesFlat ? root.selectionModel.selectedIndexesFlat
                             : root.selectionModel.selectedIndexes
        indexesFlat: !!root.selectionModel.selectedIndexesFlat

        coverRole: "thumbnail"

        defaultCover: root._placeHolder
    }

    PlaylistListContextMenu {
        id: contextMenu

        model: root.model

        ctx: MainCtx

        function tableView_popup(index, selectedIndexes, globalPos) {
            popup(selectedIndexes, globalPos)
        }

        Component.onCompleted: {
            contextMenu.requestDeleteSelectedPlaylists.connect(root.deleteSelectedPlaylists)
        }
    }

    // TBD: Refactor this with MusicGenres ?
    Component {
        id: grid

        Widgets.ExpandGridItemView {
            id: gridView

            //-------------------------------------------------------------------------------------
            // Settings

            basePictureWidth: isMusic ? VLCStyle.gridCover_music_width : VLCStyle.gridCover_video_width
            basePictureHeight: isMusic ? VLCStyle.gridCover_music_height : VLCStyle.gridCover_video_height

            model: root.model

            selectionModel: root.selectionModel

            headerDelegate: root.header

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            Navigation.parentItem: root

            delegate: VideoGridItem {
                //---------------------------------------------------------------------------------
                // Properties

                property var model: ({})

                property int index: -1

                //---------------------------------------------------------------------------------
                // Settings

                width: gridView.cellWidth;
                height: gridView.cellHeight;

                pictureWidth : gridView.maxPictureWidth
                pictureHeight: gridView.maxPictureHeight

                title: (model.name) ? model.name
                                    : qsTr("Unknown title")

                labels: (model.count > 1) ? [ qsTr("%1 Tracks").arg(_getCount(model)) ]
                                          : [ qsTr("%1 Track") .arg(_getCount(model)) ]

                dragItem: dragItemPlaylist


                //---------------------------------------------------------------------------------
                // Events

                onItemClicked: (modifier) => { gridView.leftClickOnItem(modifier, index) }

                onItemDoubleClicked: showList(model, Qt.MouseFocusReason)

                onPlayClicked: if (model.id) MediaLib.addAndPlay(model.id)

                onContextMenuButtonClicked: (_, globalMousePos) => {
                    gridView.rightClickOnItem(index);

                    contextMenu.popup(selectionModel.selectedRows(), globalMousePos);
                }

                //---------------------------------------------------------------------------------
                // Animations

                Behavior on opacity { NumberAnimation { duration: VLCStyle.duration_short } }

                DropArea {
                    anchors.fill: parent

                    onEntered: function(drag) {
                        root._adjustDragAccepted(drag)
                    }

                    onDropped: function(drop) {
                        MainCtx.setCursor(gridView, Qt.BusyCursor)
                        root._dropAction(drop, index).then(() => {
                            if (gridView)
                                MainCtx.unsetCursor(gridView)
                        })
                    }
                }
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

        Widgets.TableViewExt {
            id: tableView

            //-------------------------------------------------------------------------------------
            // Properties

            property var _modelSmall: [{
                weight: 1,

                model: {
                    criteria: "name",

                    subCriterias: [ "count" ],

                    text: qsTr("Name"),

                    headerDelegate: columns.titleHeaderDelegate,
                    colDelegate   : columns.titleDelegate
                }
            }]

            property var _modelMedium: [{
                weight: 1,

                model: {
                    criteria: "name",

                    text: qsTr("Name"),

                    headerDelegate: columns.titleHeaderDelegate,
                    colDelegate   : columns.titleDelegate
                }
            }, {
                size: 1,

                model: {
                    criteria: "count",

                    text: qsTr("Tracks"),

                    isSortable: false
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

            rowContextMenu: contextMenu

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            fadingEdge.enableBeginningFade: root.enableBeginningFade
            fadingEdge.enableEndFade: root.enableEndFade

            listView.isDropAcceptableFunc: function(drag, index) {
                root._adjustDragAccepted(drag)
                return drag.accepted
            }

            listView.acceptDropFunc: function(index, drop) {
                return root._dropAction(drop, listView.itemContainsDrag?.index)
            }

            listView.dropIndicator: null

            Navigation.parentItem: root

            //-------------------------------------------------------------------------------------
            // Events

            onActionForSelection: _actionAtIndex()

            onItemDoubleClicked: (_, model) => showList(model, Qt.MouseFocusReason)

            onRightClick: (_, _, globalMousePos) => {
                contextMenu.popup(selectionModel.selectedRows(), globalMousePos)
            }

            //-------------------------------------------------------------------------------------
            // Childs

            Widgets.TableColumns {
                id: columns

                showCriterias: (tableView.sortModel === tableView._modelSmall)

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
            hint: qsTr("Right click on a media\nto add it to a playlist")

            cover: VLCStyle.noArtAlbumCover
        }
    }
}
