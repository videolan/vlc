/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
 *
 * Authors: Leon Vitanos <leon.vitanos@gmail.com>
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
import QtQml.Models

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///main/" as MainInterface
import "qrc:///util" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///style/"

MainInterface.MainGridView {
    id: gridView

    // Properties

    property Widgets.MLDragItem dragItem
    property Util.MLContextMenu contextMenu
    property var labels

    // Signals

    signal itemDoubleClicked(var model)

    // Settings

    cellWidth : VLCStyle.gridItem_video_width
    cellHeight: VLCStyle.gridItem_video_height

    activeFocusOnTab: true

    // Functions

    // reimplement function to show "Info Panel" in grid view for the model index data
    function isInfoExpandPanelAvailable(modelIndexData) {
        return false
    }

    // Events

    // NOTE: Define the initial position and selection. This is done on activeFocus rather
    //       than Component.onCompleted because selectionModel.selectedGroup update itself
    //       after this event.
    onActiveFocusChanged: {
        if (!activeFocus || model.count === 0 || selectionModel.hasSelection)
            return;

        resetFocus() // restores initialIndex
    }

    // Connections

    Connections {
        target: gridView.contextMenu

        function onShowMediaInformation(index) {
            gridView.switchExpandItem(index)

            if (gridView.focus)
                expandItem.setCurrentItemFocus(Qt.TabFocusReason)
        }
    }

    // Children

    delegate: VideoGridItem {
        id: gridItem

        // Properties

        /* required */ property var model: ({})
        /* required */ property int index: -1

        // Settings

        opacity: (gridView.expandIndex !== -1
                  &&
                  gridView.expandIndex !== gridItem.index) ? 0.7 : 1

        // FIXME: Sometimes MLBaseModel::getDataAt returns {} so we use 'isNew === true'.
        showNewIndicator: (model.isNew === true)

        dragItem: gridView.dragItem

        labels: gridView.labels(model)

        // Events

        onItemClicked: (_,_, modifier) => { gridView.leftClickOnItem(modifier, index) }

        onItemDoubleClicked: gridView.itemDoubleClicked(model)

        onContextMenuButtonClicked: (_, globalMousePos) => {
            gridView.rightClickOnItem(index);

            const options = {}
            if (gridView.isInfoExpandPanelAvailable(model))
                options["information"] = index

            gridView.contextMenu.popup(selectionModel.selectedIndexes, globalMousePos, options);
        }

        // Animations

        Behavior on opacity { NumberAnimation { duration: VLCStyle.duration_short } }
    }

    expandDelegate: VideoInfoExpandPanel {
        x: 0

        width: gridView.width

        model: gridView.model

        Navigation.parentItem: gridView

        Navigation.cancelAction: gridView.forceFocus
        Navigation.upAction: gridView.forceFocus
        Navigation.downAction: gridView.forceFocus

        onRetract: gridView.retract()
    }

    function forceFocus() {
        setCurrentItemFocus(Qt.TabFocus)
    }
}
