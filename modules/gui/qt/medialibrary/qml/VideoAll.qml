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

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models

import VLC.MediaLibrary

import VLC.Widgets as Widgets
import VLC.MainInterface
import VLC.Util
import VLC.Style

MainViewLoader {
    id: root

    // Properties

    readonly property int contentLeftMargin: currentItem?.contentLeftMargin ?? 0
    readonly property int contentRightMargin: currentItem?.contentRightMargin ?? 0

    // Currently only respected by the list view:
    property bool enableBeginningFade: true
    property bool enableEndFade: true

    // NOTE: Specify an optional header for the view.
    property Component header: null

    property Item headerItem: currentItem?.headerItem ?? null

    property int headerPositioning: ListView.OverlayHeader

    property int displayMarginBeginning: 0
    property int displayMarginEnd: 0

    property bool interactive: true // false, when as a row

    property bool reuseItems: true

    readonly property int currentIndex: currentItem?.currentIndex ?? -1

    readonly property int rowHeight: currentItem?.rowHeight ?? 0

    // 'role' used for tableview's section text
    required  property string sectionProperty

    // NOTE: The ContextMenu depends on the model so we have to provide it too.
    required property MLContextMenu contextMenu

    // function(model) -> [strings....]
    // used to get grid labels per model item
    property var gridLabels: getLabel
    property var listLabels: getLabel

    // Aliases

    property alias dragItem: dragItem

    // Settings

    isSearchable: true
    list: list
    grid: grid
    emptyLabel: emptylabel

    sortModel: [
        { text: qsTr("Alphabetic"), criteria: "title"    },
        { text: qsTr("Duration"),   criteria: "duration" }
    ]


    signal seeAllButtonClicked(int reason)


    // Functions

    function getLabel(model) {
        if (!model) return ""

        return [
            model.resolution_name || "",
            model.channel || ""
        ].filter(function(a) { return a !== "" })
    }

    function getItemY(index) {
        return currentItem?.getItemY(index) ?? 0
    }

    // reimplement function to show "Info Panel" in grid view for the model index data
    function isInfoExpandPanelAvailable(modelIndexData) {
        return false
    }

    // Events

    function onAction(indexes) {
        model.addAndPlay( indexes )
        MainCtx.requestShowPlayerView()
    }

    function onDoubleClick(object) {
        MediaLib.addAndPlay(object.id)
        MainCtx.requestShowPlayerView()
    }


    Widgets.MLDragItem {
        id: dragItem

        view: root.currentItem

        indexes: indexesFlat ? selectionModel.selectedIndexesFlat
                             : selectionModel.selectedIndexes
        indexesFlat: !!selectionModel.selectedIndexesFlat

        coverRole: "thumbnail"

        defaultCover: VLCStyle.noArtVideoCover
    }

    Component {
        id: grid

        VideoGridDisplay {
            id: gridView

            // Settings

            model: root.model

            selectionModel: root.selectionModel

            headerDelegate: root.header

            dragItem: root.dragItem

            contextMenu: root.contextMenu

            labels: root.gridLabels

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            interactive: root.interactive

            reuseItems: root.reuseItems

            // Navigation

            Navigation.parentItem: root

            Navigation.upItem: headerItem

            // Functions

            function isInfoExpandPanelAvailable(modelIndexData) {
                return root.isInfoExpandPanelAvailable(modelIndexData)
            }

            // Events

            onActionAtIndex: root.onAction(selectionModel.selectedIndexes)

            onItemDoubleClicked: model => root.onDoubleClick(model)
        }
    }

    Component {
        id: list

        VideoListDisplay {
            id: listView

            // Settings

            model: root.model

            selectionModel: root.selectionModel

            rowContextMenu: root.contextMenu

            dragItem: root.dragItem

            header: root.header

            headerPositioning: root.headerPositioning

            activeFocusOnTab: true

            section.property: root.sectionProperty

            fadingEdge.enableBeginningFade: root.enableBeginningFade
            fadingEdge.enableEndFade: root.enableEndFade

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            interactive: root.interactive

            reuseItems: root.reuseItems

            // Navigation

            Navigation.parentItem: root

            Navigation.upItem: headerItem

            // Events

            onActionForSelection: (selection) => {
                root.onAction(selectionModel.selectedIndexes)
            }

            onItemDoubleClicked: (index, model) => root.onDoubleClick(model)

            onRightClick: (_,_,globalMousePos) => {
                root.contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
            }

            coverLabels: root.listLabels
        }
    }

    Component {
        id: emptylabel

        Widgets.EmptyLabelButton {
            coverWidth : VLCStyle.dp(182, VLCStyle.scale)
            coverHeight: VLCStyle.dp(114, VLCStyle.scale)

            focus: true

            text: qsTr("No video found\nPlease try adding sources")

            cover: VLCStyle.noArtVideoCover

            Navigation.parentItem: root
        }
    }
}
