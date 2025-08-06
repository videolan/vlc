/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

import VLC.MainInterface
import VLC.MediaLibrary

import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style

FocusScope {
    id: root

    // Properties

    //behave like a Page
    property var pagePrefix: []

    readonly property bool hasGridListMode: false
    readonly property bool isSearchable: true

    property int leftPadding: 0
    property int rightPadding: 0

    property alias playlistView: view

    readonly property int currentIndex: view.currentIndex
    property string name: ""

    property int initialIndex: 0

    property alias header: view.header
    property Item headerItem: view.headerItem

    property bool isMusic: true
    property string _placeHolder: isMusic ? VLCStyle.noArtAlbumCover : VLCStyle.noArtVideoCover

    property bool enableBeginningFade: true
    property bool enableEndFade: true

    // Aliases

    // NOTE: This is used to determine which media(s) shall be displayed.
    property alias parentId: model.parentId
    property alias searchPattern: model.searchPattern
    property alias sortOrder: model.sortOrder
    property alias sortCriteria: model.sortCriteria

    property alias model: model

    property alias dragItem: dragItem

    property alias displayMarginBeginning: view.displayMarginBeginning
    property alias displayMarginEnd: view.displayMarginEnd

    // Events

    onModelChanged: resetFocus()

    onInitialIndexChanged: resetFocus()

    // Functions

    function setCurrentItemFocus(reason) { view.setCurrentItemFocus(reason); }

    function resetFocus() {
        const count = model.count

        if (count === 0 || initialIndex === -1) return

        var index

        if (initialIndex < count)
            index = initialIndex
        else
            index = 0

        view.selectionModel.select(model.index(index, 0), ItemSelectionModel.ClearAndSelect);

        view.positionViewAtIndex(index, ItemView.Contain)

        view.setCurrentItem(index)
    }

    // Events

    function onDelete()
    {
        const indexes = view.selectionModel.selectedIndexes;

        if (indexes.length === 0)
            return;

        model.remove(indexes);
    }

    // Childs

    MLPlaylistModel {
        id: model

        ml: MediaLib

        onCountChanged: {
            // NOTE: We need to cancel the Drag item manually when resetting. Should this be called
            //       from 'onModelReset' only ?
            dragItem.Drag.cancel();

            if (count === 0 || view.selectionModel.hasSelection)
                return;

            resetFocus();
        }

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
            model.transactionPendingChanged.connect(model.onBusynessChanged)
            model.loadingChanged.connect(model.onBusynessChanged)
            model.onBusynessChanged()
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
        id: dragItem

        view: view

        indexes: indexesFlat ? view.selectionModel.selectedIndexesFlat
                             : view.selectionModel.selectedIndexes
        indexesFlat: !!view.selectionModel.selectedIndexesFlat

        coverRole: "thumbnail"

        defaultCover: root._placeHolder
    }


    PlaylistMediaContextMenu {
        id: contextMenu

        model: root.model
        ctx: MainCtx

        function tableView_popup(index, selectedIndexes, globalPos) {
            popup(selectedIndexes, globalPos)
        }
    }

    PlaylistMedia
    {
        id: view

        // Settings

        anchors.fill: parent

        focus: (model.count !== 0)

        model: root.model

        dragItem: root.dragItem

        rowContextMenu: contextMenu

        isMusic: root.isMusic

        header: Widgets.ViewHeader {
            view: root.playlistView

            visible: view.count > 0

            text: root.name
        }

        fadingEdge.enableBeginningFade: root.enableBeginningFade
        fadingEdge.enableEndFade: root.enableEndFade

        Navigation.parentItem: root

        Navigation.cancelAction: function () {
            if (view.currentIndex <= 0) {
                root.Navigation.defaultNavigationCancel()
            } else {
                positionViewAtIndex(0, ItemView.Contain)

                setCurrentItem(0)
            }
        }

        // Events

        onRightClick: (_,_,globalMousePos) => {
            contextMenu.popup(selectionModel.selectedRows(), globalMousePos)
        }

        // Keys

        Keys.onDeletePressed: onDelete()
    }

    Widgets.EmptyLabelButton {
        anchors.centerIn: parent

        visible: !model.loading && (model.count <= 0)

        focus: visible

        text: qsTr("No media found")

        cover: root._placeHolder

        Navigation.parentItem: root
    }
}
