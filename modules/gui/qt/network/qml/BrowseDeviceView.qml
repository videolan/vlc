/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

import QtQuick 2.11

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///main/"    as MainInterface
import "qrc:///widgets/" as Widgets
import "qrc:///util/"    as Util

FocusScope {
    id: root

    // Properties

    /* required */ property var model

    property var parentFilter: null

    readonly property int rowHeight: (_currentView) ? _currentView.rowHeight : 0

    readonly property int contentHeight: (_currentView) ? _currentView.contentHeight : 0

    readonly property int contentMargin: (_currentView) ? _currentView.contentLeftMargin : 0

    property int displayMarginEnd: 0

    readonly property int currentIndex: (_currentView) ? _currentView.currentIndex : -1

    property int maximumRows: -1

    readonly property int maximumCount: (_currentView) ? _currentView.maximumCount : -1

    readonly property int nbItemPerRow: (_currentView) ? _currentView.nbItemPerRow : 1

    property Component header: BrowseDeviceHeader {
        view: root

        text: root.title

        button.visible: root.sourceModel.hasMoreItems

        Navigation.parentItem: root

        Navigation.downAction: function() {
            view.setCurrentItemFocus(Qt.TabFocusReason)
        }

        onClicked: root.seeAll(reason)
    }

    property string title

    // Aliases

    property alias modelFilter: modelFilter

    property alias sourceModel: modelFilter.sourceModel

    // Private

    property alias _currentView: view.currentItem

    // Signals

    signal browse(var tree, int reason)

    signal seeAll(int reason)

    // Events

    onFocusChanged: {
        if (activeFocus === false || model.count === 0 || model.currentIndex !== -1)
            return

        model.currentIndex = 0
    }

    onParentFilterChanged: {
        if (parentFilter === null || sourceModel === null)
            return

        sourceModel.searchRole = parentFilter.searchRole

        sourceModel.searchPattern = parentFilter.searchPattern

        sourceModel.sortCriteria = parentFilter.sortCriteria

        sourceModel.sortOrder = parentFilter.sortOrder
    }

    // Connections

    Connections {
        target: MainCtx

        onGridViewChanged: {
            if (MainCtx.gridView) view.replace(grid)
            else                  view.replace(list)
        }
    }

    // NOTE: If it exists, we're applying 'parentFilter' properties to fit the sorting options.
    Connections {
        target: parentFilter

        onSearchRoleChanged: sourceModel.searchRole = parentFilter.searchRole

        onSearchPatternChanged: sourceModel.searchPattern = parentFilter.searchPattern

        onSortCriteriaChanged: sourceModel.sortCriteria = parentFilter.sortCriteria

        onSortOrderChanged: sourceModel.sortOrder = parentFilter.sortOrder
    }

    // Functions

    function playAt(index) {
        model.addAndPlay(modelFilter.mapIndexToSource(index))
    }

    function setCurrentItemFocus(reason) {
        _currentView.setCurrentItemFocus(reason)
    }

    function getItemY(index) {
        if (_currentView === null)
            return 0

        return _currentView.getItemY(index)
    }

    // Events

    function onAction(index) {
        var indexes = modelSelect.selectedIndexes

        if (indexes.length > 1) {
            model.addAndPlay(modelFilter.mapIndexesToSource(indexes))

            return
        }

        var data = modelFilter.getDataAt(index)

        var type = data.type

        if (type === NetworkMediaModel.TYPE_DIRECTORY || type === NetworkMediaModel.TYPE_NODE)
            browse(data.tree, Qt.TabFocusReason)
        else
            playAt(index);
    }

    function onClicked(model, index, modifier) {
        modelSelect.updateSelection(modifier, model.currentIndex, index)

        model.currentIndex = index

        forceActiveFocus()
    }

    function onDoubleClicked(model, index) {
        var type = model.type

        if (type === NetworkMediaModel.TYPE_NODE || type === NetworkMediaModel.TYPE_DIRECTORY)
            browse(model.tree, Qt.MouseFocusReason)
        else
            playAt(index);
    }

    // Children

    Util.SelectableDelegateModel {
        id: modelSelect

        model: modelFilter
    }

    SortFilterProxyModel {
        id: modelFilter

        sourceModel: root.model

        searchRole: "name"

        // TODO: Handle the searchPattern on a partial list.
        searchPattern: (sourceModel && maximumRows === -1) ? sourceModel.searchPattern : ""
    }

    Widgets.StackViewExt {
        id: view

        anchors.fill: parent

        focus: (model.count !== 0)

        initialItem: (MainCtx.gridView) ? grid : list
    }

    Component {
        id: grid

        Widgets.ExpandGridView {
            id: gridView

            readonly property int maximumCount: (root.maximumRows === -1)
                                                ? -1
                                                : root.maximumRows * nbItemPerRow

            anchors.fill: parent

            cellWidth: VLCStyle.gridItem_network_width
            cellHeight: VLCStyle.gridItem_network_height

            displayMarginEnd: root.displayMarginEnd

            model: modelFilter

            headerDelegate: root.header

            selectionDelegateModel: modelSelect

            Navigation.parentItem: root

            Navigation.upItem: headerItem

            onActionAtIndex: root.onAction(index)

            delegate: NetworkGridItem {
                onItemClicked: root.onClicked(model, index, modifier)

                onItemDoubleClicked: root.onDoubleClicked(model, index)

                onPlayClicked: root.playAt(index)
            }
        }
    }

    Component {
        id: list

        Widgets.KeyNavigableTableView {
            id: listView

            readonly property int maximumCount: root.maximumRows

            readonly property int nbItemPerRow: 1

            readonly property int _nbCols: VLCStyle.gridColumnsForWidth(listView.availableRowWidth)

            readonly property int _nameColSpan: Math.max((_nbCols - 1) / 2, 1)

            anchors.fill: parent

            rowHeight: VLCStyle.tableCoverRow_height

            displayMarginEnd: root.displayMarginEnd

            model: modelFilter

            sortModel: [{
                criteria: "artwork",

                width: VLCStyle.colWidth(1),

                headerDelegate: artworkHeader,
                colDelegate   : artworkColumn
            }, {
                isPrimary: true,
                criteria: "name",

                width: VLCStyle.colWidth(listView._nameColSpan),

                text: I18n.qtr("Name")
            }, {
                criteria: "mrl",

                width: VLCStyle.colWidth(Math.max(listView._nbCols - listView._nameColSpan - 1), 1),

                text: I18n.qtr("Url"),

                colDelegate: mrlColumn
            }]

            header: root.header

            headerColor: VLCStyle.colors.bg

            selectionDelegateModel: modelSelect

            Navigation.parentItem: root

            Navigation.upItem: headerItem

            onActionForSelection: root.onAction(selection[0].row)

            onItemDoubleClicked: root.onDoubleClicked(model, index)

            Component {
                id: artworkHeader

                Item {
                    Widgets.IconLabel {
                        width: VLCStyle.listAlbumCover_width
                        height: VLCStyle.listAlbumCover_height

                        horizontalAlignment: Text.AlignHCenter

                        color: VLCStyle.colors.caption
                    }
                }
            }

            Component {
                id: artworkColumn

                NetworkThumbnailItem { onPlayClicked: root.playAt(index) }
            }

            Component {
                id: mrlColumn

                Widgets.ScrollingText {
                    id: itemText

                    property var rowModel: parent.rowModel
                    property var colModel: parent.colModel

                    property color foregroundColor: parent.foregroundColor

                    width: parent.width

                    clip: scrolling

                    label: itemLabel

                    forceScroll: parent.currentlyFocused

                    Widgets.ListLabel {
                        id: itemLabel

                        anchors.verticalCenter: parent.verticalCenter

                        text: {
                            if (itemText.rowModel === null)
                                return ""

                            var text = itemText.rowModel[itemText.colModel.criteria]

                            if (text.toString() === "vlc://nop")
                                return ""
                            else
                                return text
                        }

                        color: itemText.foregroundColor
                    }
                }
            }
        }
    }
}
