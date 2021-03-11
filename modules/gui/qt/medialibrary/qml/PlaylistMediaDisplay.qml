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

    readonly property bool isViewMultiView: false

    readonly property int currentIndex: currentItem.currentIndex

    property int     initialIndex: 0
    property variant initialId
    property string  initialName

    // NOTE: Specify an optionnal header for the view.
    property Component header: undefined

    property Item headerItem: (currentItem) ? currentItem.headerItem : undefined

    //---------------------------------------------------------------------------------------------
    // Aliases
    //---------------------------------------------------------------------------------------------

    // NOTE: This is used to determine which media(s) shall be displayed.
    property alias parentId: model.parentId

    // NOTE: The name of the playlist.
    property alias name: label.text

    property alias model: model

    property alias currentItem: view

    //---------------------------------------------------------------------------------------------

    property alias dragItem: dragItem

    //---------------------------------------------------------------------------------------------
    // Settings
    //---------------------------------------------------------------------------------------------

    navigationCancel: function() {
        if (currentItem.currentIndex <= 0) {
            defaultNavigationCancel()
        } else {
            currentItem.currentIndex = 0;

            currentItem.positionViewAtIndex(0, ItemView.Contain);
        }
    }

    //---------------------------------------------------------------------------------------------
    // Events
    //---------------------------------------------------------------------------------------------

    onModelChanged: resetFocus()

    onInitialIndexChanged: resetFocus()

    //---------------------------------------------------------------------------------------------
    // Functions
    //---------------------------------------------------------------------------------------------

    function setCurrentItemFocus() { view.currentItem.forceActiveFocus() }

    function resetFocus() {
        if (model.count === 0) return

        var initialIndex = root.initialIndex

        if (initialIndex >= model.count)
            initialIndex = 0

        modelSelect.select(model.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect);

        if (currentItem)
            currentItem.positionViewAtIndex(initialIndex, ItemView.Contain);
    }

    //---------------------------------------------------------------------------------------------
    // Private

    function _actionAtIndex(index) {
        g_mainDisplay.showPlayer();

        medialib.addAndPlay(model.getIdsForIndexes(modelSelect.selectedIndexes));
    }

    //---------------------------------------------------------------------------------------------
    // Childs
    //---------------------------------------------------------------------------------------------

    MLPlaylistModel {
        id: model

        ml: medialib

        parentId: initialId

        onCountChanged: {
            if (count === 0 || modelSelect.hasSelection) return;

            resetFocus();
        }
    }

    Widgets.SubtitleLabel {
        id: label

        anchors.top: parent.top

        anchors.topMargin: VLCStyle.margin_normal

        width: root.width

        leftPadding  : VLCStyle.margin_xlarge
        bottomPadding: VLCStyle.margin_xsmall

        text: initialName
    }

    Widgets.DragItem {
        id: dragItem

        function updateComponents(maxCovers) {
            var items = modelSelect.selectedIndexes.slice(0, maxCovers).map(function (x){
                return model.getDataAt(x.row);
            })

            var covers = items.map(function (item) {
                return { artwork: item.thumbnail || VLCStyle.noArtCover }
            });

            var title = items.map(function (item) {
                return item.title
            }).join(", ");

            return {
                covers: covers,
                title: title,
                count: modelSelect.selectedIndexes.length
            }
        }

        function getSelectedInputItem() {
            return model.getItemsForIndexes(modelSelect.selectedIndexes);
        }
    }

    Util.SelectableDelegateModel {
        id: modelSelect

        model: root.model
    }

    PlaylistMediaContextMenu {
        id: contextMenu

        model: root.model
    }

    PlaylistMedia
    {
        id: view

        //-----------------------------------------------------------------------------------------
        // Settings

        anchors.left  : parent.left
        anchors.right : parent.right
        anchors.top   : label.bottom
        anchors.bottom: parent.bottom

        clip: true

        focus: (model.count !== 0)

        model: root.model

        selectionDelegateModel: modelSelect

        dragItem: root.dragItem

        header: root.header

        headerTopPadding: VLCStyle.margin_normal

        headerPositioning: ListView.InlineHeader

        navigationParent: root
        navigationUpItem: (headerItem) ? headerItem.focus : undefined

        //-----------------------------------------------------------------------------------------
        // Events

        onContextMenuButtonClicked: contextMenu.popup(modelSelect.selectedIndexes,
                                                      menuParent.mapToGlobal(0,0))

        onRightClick: contextMenu.popup(modelSelect.selectedIndexes, globalMousePos)
    }

    EmptyLabel {
        anchors.fill: parent

        visible: (model.count === 0)

        focus: visible

        text: i18n.qtr("No media found")

        cover: VLCStyle.noArtAlbumCover

        navigationParent: root
    }
}
