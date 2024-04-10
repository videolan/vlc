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

import QtQuick

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///main/"    as MainInterface
import "qrc:///widgets/" as Widgets
import "qrc:///util/"    as Util

FocusScope {
    id: root

    // Properties

    /* required */ property var model

    readonly property int rowHeight: (_currentView) ? _currentView.rowHeight : 0

    readonly property int contentHeight: (_currentView) ? _currentView.contentHeight : 0

    readonly property int contentLeftMargin: (_currentView) ? _currentView.contentLeftMargin : 0
    readonly property int contentRightMargin: (_currentView) ? _currentView.contentRightMargin : 0

    property int displayMarginEnd: 0

    readonly property int currentIndex: (_currentView) ? _currentView.currentIndex : -1

    property int maximumRows: -1

    readonly property int maximumCount: (_currentView) ? _currentView.maximumCount : -1

    readonly property int nbItemPerRow: (_currentView) ? _currentView.nbItemPerRow : 1

    property bool isSearchable: true

    property Component header: Widgets.ViewHeader {
        view: root

        text: root.title

        seeAllButton.visible: root.model.count < root.model.maximumCount

        Navigation.parentItem: root

        Navigation.downAction: function() {
            view.setCurrentItemFocus(Qt.TabFocusReason)
        }

        onSeeAllButtonClicked: root.seeAll(reason)
    }

    property string title

    // Aliases

    property alias leftPadding: view.leftPadding
    property alias rightPadding: view.rightPadding

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

    // Connections

    Connections {
        target: MainCtx

        function onGridViewChanged() {
            if (MainCtx.gridView) view.replace(grid)
            else                  view.replace(list)
        }
    }

    // Functions

    function playAt(index) {
        model.addAndPlay(index)
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
        const indexes = modelSelect.selectedIndexes

        if (indexes.length > 1) {
            model.addAndPlay(indexes)

            return
        }

        const data = model.getDataAt(index)

        const type = data.type

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
        const type = model.type

        if (type === NetworkMediaModel.TYPE_NODE || type === NetworkMediaModel.TYPE_DIRECTORY)
            browse(model.tree, Qt.MouseFocusReason)
        else
            playAt(index);
    }

    // Children

    ListSelectionModel {
        id: modelSelect

        model: root.model
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

            cellWidth: VLCStyle.gridItem_network_width
            cellHeight: VLCStyle.gridItem_network_height

            displayMarginEnd: root.displayMarginEnd

            model: root.model

            headerDelegate: root.header

            selectionModel: modelSelect

            Navigation.parentItem: root

            Navigation.upItem: headerItem

            onActionAtIndex: (index) => { root.onAction(index) }

            delegate: NetworkGridItem {
                onItemClicked: (_, _, modifier) => { root.onClicked(model, index, modifier) }

                onItemDoubleClicked: (_, _, modifier) => { root.onDoubleClicked(model, index) }

                onPlayClicked: root.playAt(index)
            }
        }
    }

    Component {
        id: list

        Widgets.KeyNavigableTableView {
            id: listView

            // Properties

            readonly property int maximumCount: root.maximumRows

            readonly property int nbItemPerRow: 1

            readonly property int _nbCols: VLCStyle.gridColumnsForWidth(availableRowWidth)

            readonly property int _size: (_nbCols - 1) / 2

            property var _modelSmall: [{
                size: Math.max(2, _nbCols),

                model: ({
                    criteria: "name",

                    title: "name",

                    subCriterias: [ "mrl" ],

                    text: qsTr("Name"),

                    headerDelegate: artworkHeader,
                    colDelegate: artworkColumn
                })
            }]

            property var _modelMedium: [{
                size: 1,

                model: {
                    criteria: "artwork",

                    text: qsTr("Cover"),

                    headerDelegate: artworkHeader,
                    colDelegate: artworkColumn
                }
            }, {
                size: _size,

                model: {
                    criteria: "name",

                    text: qsTr("Name")
                }
            }, {
                size: Math.max(_nbCols - _size - 1, 1),

                model: {
                    criteria: "mrl",

                    text: qsTr("Url"),

                    colDelegate: mrlColumn
                }
            }]

            // Settings

            rowHeight: VLCStyle.tableCoverRow_height

            displayMarginEnd: root.displayMarginEnd

            model: root.model

            sortModel: (availableRowWidth < VLCStyle.colWidth(4)) ? _modelSmall
                                                                  : _modelMedium

            header: root.header

            selectionModel: modelSelect

            Navigation.parentItem: root

            Navigation.upItem: headerItem

            onActionForSelection: root.onAction(selection[0].row)

            onItemDoubleClicked: root.onDoubleClicked(model, index)

            Component {
                id: artworkHeader

                Widgets.IconLabel {
                    text: VLCIcons.album_cover

                    height: VLCStyle.listAlbumCover_height
                    width: VLCStyle.listAlbumCover_width
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: VLCStyle.icon_tableHeader

                    color: parent.colorContext.fg.secondary
                }
            }

            Component {
                id: artworkColumn

                NetworkThumbnailItem { onPlayClicked: root.playAt(index) }
            }

            Component {
                id: mrlColumn

                Widgets.TextAutoScroller {
                    id: itemText

                    property var rowModel: parent.rowModel
                    property var colModel: parent.colModel

                    readonly property ColorContext colorContext: parent.colorContext
                    readonly property bool selected: parent.selected

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

                            const text = itemText.rowModel[itemText.colModel.criteria]

                            if (text.toString() === "vlc://nop")
                                return ""
                            else
                                return text
                        }

                        color: itemText.selected
                            ? itemText.colorContext.fg.highlight
                            : itemText.colorContext.fg.primary
                    }
                }
            }
        }
    }
}
