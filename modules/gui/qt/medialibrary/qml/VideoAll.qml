/*****************************************************************************
 * Copyright (C) 2021-23 VLC authors and VideoLAN
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
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///style/"

MainInterface.MainViewLoader {
    id: root

    // Properties

    readonly property int contentMargin: Helpers.get(currentItem, "contentLeftMargin", 0)

    // NOTE: Specify an optional header for the view.
    property Component header: null

    property Item headerItem: Helpers.get(currentItem, "headerItem", null)

    readonly property int currentIndex: Helpers.get(currentItem, "currentIndex", -1)

    // NOTE: The ContextMenu depends on the model so we have to provide it too.
    /* required */ property var contextMenu

    property var sortModel: [
        { text: I18n.qtr("Alphabetic"), criteria: "title"    },
        { text: I18n.qtr("Duration"),   criteria: "duration" }
    ]

    property alias dragItem: dragItem

    // function(model) -> [strings....]
    // used to get grid labels per model item
    property var gridLabels: getLabel

    list: list
    grid: grid
    emptyLabel: emptylabel


    function getLabel(model) {
        if (!model) return ""

        return [
            model.resolution_name || "",
            model.channel || ""
        ].filter(function(a) { return a !== "" })
    }

    // reimplement function to show "Info Panel" in grid view for the model index data
    function isInfoExpandPanelAvailable(modelIndexData) {
        return false
    }

    // Events

    function onAction(indexes) {
        MediaLib.addAndPlay(model.getIdsForIndexes(indexes))
        g_mainDisplay.showPlayer()
    }

    function onDoubleClick(object) {
        g_mainDisplay.play(MediaLib, object.id)
    }

    function onLabelGrid(object) { return getLabel(object) }
    function onLabelList(object) { return getLabel(object) }

    // Private events

    function _onNavigationUp() {
        // NOTE: We are calling the header focus function when we have one.
        if (headerItem && headerItem.focus)
            headerItem.forceActiveFocus(Qt.TabFocusReason)
        else
            Navigation.defaultNavigationUp()
    }

    Widgets.MLDragItem {
        id: dragItem

        mlModel: root.model

        indexes: selectionModel.selectedIndexes

        coverRole: "thumbnail"

        defaultCover: VLCStyle.noArtVideoCover
    }

    Component {
        id: grid

        MainInterface.MainGridView {
            id: gridView

            // Settings

            cellWidth : VLCStyle.gridItem_video_width
            cellHeight: VLCStyle.gridItem_video_height

            topMargin: VLCStyle.margin_normal

            model: root.model

            selectionDelegateModel: selectionModel

            headerDelegate: root.header

            activeFocusOnTab: true

            // Navigation

            Navigation.parentItem: root

            Navigation.upAction: _onNavigationUp

            // Events

            // NOTE: Define the initial position and selection. This is done on activeFocus rather
            //       than Component.onCompleted because selectionModel.selectedGroup update itself
            //       after this event.
            onActiveFocusChanged: {
                if (activeFocus == false || model.count === 0 || selectionModel.hasSelection)
                    return;

                resetFocus() // restores initialIndex
            }

            onActionAtIndex: root.onAction(selectionModel.selectedIndexes)

            // Connections

            Connections {
                target: root.contextMenu

                onShowMediaInformation: {
                    gridView.switchExpandItem(index)

                    if (gridView.focus)
                        expandItem.setCurrentItemFocus(Qt.TabFocusReason)
                }
            }

            // Children

            delegate: VideoGridItem {
                id: gridItem

                // properties required by ExpandGridView

                property var model: ({})
                property int index: -1

                // Settings

                opacity: (gridView.expandIndex !== -1
                          &&
                          gridView.expandIndex !== gridItem.index) ? 0.7 : 1

               labels: root.gridLabels(model)

                // FIXME: Sometimes MLBaseModel::getDataAt returns {} so we use 'isNew === true'.
                showNewIndicator: (model.isNew === true)

                dragItem: root.dragItem

                // Events

                onItemClicked: gridView.leftClickOnItem(modifier, index)

                onItemDoubleClicked: root.onDoubleClick(model)

                onContextMenuButtonClicked: {
                    gridView.rightClickOnItem(index);

                    var options = {}
                    if (root.isInfoExpandPanelAvailable(model))
                        options["information"] = index

                    root.contextMenu.popup(selectionModel.selectedIndexes, globalMousePos, options);
                }

                // Animations

                Behavior on opacity { NumberAnimation { duration: VLCStyle.duration_short } }
            }

            expandDelegate: VideoInfoExpandPanel {
                width: gridView.width

                x: 0

                model: root.model

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
    }

    Component {
        id: list

        VideoListDisplay {
            id: listView

            // Settings

            model: root.model

            selectionDelegateModel: selectionModel

            dragItem: root.dragItem

            header: root.header

            topMargin: VLCStyle.margin_normal

            headerPositioning: ListView.InlineHeader

            activeFocusOnTab: true

            // Navigation

            Navigation.parentItem: root

            Navigation.upAction: _onNavigationUp

            // Events

            onActionForSelection: root.onAction(selectionModel.selectedIndexes)

            onItemDoubleClicked: root.onDoubleClick(model)

            onContextMenuButtonClicked: root.contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)

            onRightClick: root.contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)

            // Functions

            function onLabels(model) { return root.onLabelList(model); }
        }
    }

    Component {
        id: emptylabel

        EmptyLabelButton {
            coverWidth : VLCStyle.dp(182, VLCStyle.scale)
            coverHeight: VLCStyle.dp(114, VLCStyle.scale)

            focus: true

            text: I18n.qtr("No video found\nPlease try adding sources, by going to the Browse tab")

            cover: VLCStyle.noArtVideoCover

            Navigation.parentItem: root
        }
    }
}
