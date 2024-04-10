/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

import org.videolan.vlc 0.1

import "qrc:///util/" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///style/"

// FIXME: Maybe we could inherit from KeyNavigableListView directly.
FocusScope {
    id: root

    // Properties

    property var sortModel: []

    property Component tableHeaderDelegate: Widgets.CaptionLabel {
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter

        Accessible.ignored: true

        text: colModel.text || ""
        color: parent.colorContext.fg.secondary
    }

    // NOTE: We want edge to edge backgrounds in our delegate and header, so we implement our own
    //       margins implementation like in ExpandGridView. The default values should be the same
    //       than ExpandGridView to respect the grid parti pris.
    property int leftMargin: VLCStyle.column_margin + leftPadding
    property int rightMargin: VLCStyle.column_margin + rightPadding

    property int leftPadding: 0
    property int rightPadding: 0

    readonly property int extraMargin: Math.max(0, (width - usedRowSpace) / 2)

    // NOTE: The list margins for the item(s) horizontal positioning.
    readonly property int contentLeftMargin: extraMargin + leftMargin
    readonly property int contentRightMargin: extraMargin + rightMargin

    readonly property real usedRowSpace: {
        let size = leftMargin + rightMargin

        for (let i in sortModel)
            size += VLCStyle.colWidth(sortModel[i].size)

        return size + Math.max(VLCStyle.column_spacing * (sortModel.length - 1), 0)
    }

    property Component header: null
    property Item headerItem: view.headerItem ? view.headerItem.loadedHeader : null
    property color headerColor: colorContext.bg.primary
    property int headerTopPadding: 0


    property real rowHeight: VLCStyle.tableRow_height

    property real availableRowWidth: 0

    property Item dragItem
    property bool acceptDrop: false

    // Private

    property bool _ready: false

    property real _availabeRowWidthLastUpdateTime: Date.now()

    readonly property real _currentAvailableRowWidth: width - leftMargin - rightMargin

    // Aliases

    property alias topMargin: view.topMargin
    property alias bottomMargin: view.bottomMargin

    property alias spacing: view.spacing

    property alias model: view.model
    property alias selectionModel: view.selectionModel

    property alias delegate: view.delegate

    property alias contentY     : view.contentY
    property alias contentHeight: view.contentHeight

    property alias originX: view.originX
    property alias originY: view.originY

    property alias interactive: view.interactive

    property alias section: view.section

    property alias currentIndex: view.currentIndex
    property alias currentItem: view.currentItem

    property alias headerPositioning: view.headerPositioning

    property alias tableHeaderItem: view.headerItem

    property alias footerItem: view.footerItem
    property alias footer: view.footer

    property alias fadingEdge: view.fadingEdge

    property alias add:       view.add
    property alias displaced: view.displaced

    property alias listView: view

    property alias displayMarginEnd: view.displayMarginEnd

    property alias count: view.count

    property alias colorContext: view.colorContext

    // Signals

    //forwarded from subview
    signal actionForSelection( var selection )
    signal contextMenuButtonClicked(Item menuParent, var menuModel, point globalMousePos)
    signal rightClick(Item menuParent, var menuModel, point globalMousePos)
    signal itemDoubleClicked(var index, var model)

    signal dropUpdatePosition(Item delegate, int index, var drag, bool before)
    signal dropEntered(Item delegate, int index, var drag, bool before)
    signal dropExited(Item delegate, int index,  var drag, bool before)
    signal dropEvent(Item delegate, int index,  var drag, var drop, bool before)

    // Events

    Component.onCompleted: {
        _ready = true

        availableRowWidthUpdater.enqueueUpdate()
    }

    on_CurrentAvailableRowWidthChanged: if (_ready) availableRowWidthUpdater.enqueueUpdate()

    // Functions

    function setCurrentItem(index) {
        view.setCurrentItem(index)
    }

    function setCurrentItemFocus(reason) {
        view.setCurrentItemFocus(reason);
    }

    function positionViewAtIndex(index, mode) {
        view.positionViewAtIndex(index, mode)
    }

    function positionViewAtBeginning() {
        view.positionViewAtBeginning()
    }

    function getItemY(index) {
        let size = index * rowHeight + topMargin

        if (tableHeaderItem)
            size += tableHeaderItem.height

        return size
    }

    // Private

    // Childs

    Timer {
        id: availableRowWidthUpdater

        interval: 100
        triggeredOnStart: false
        repeat: false
        onTriggered: {
            _update()
        }

        function _update() {
            root.availableRowWidth = root._currentAvailableRowWidth
            root._availabeRowWidthLastUpdateTime = Date.now()
        }

        function enqueueUpdate() {
            // updating availableRowWidth is expensive because of property bindings in sortModel
            // and availableRowWidth is dependent on root.width which can update in a burst
            // so try to maintain a minimum time gap between subsequent availableRowWidth updates
            const sinceLastUpdate = Date.now() - root._availabeRowWidthLastUpdateTime
            if ((root.availableRowWidth === 0) || (sinceLastUpdate > 128 && !availableRowWidthUpdater.running)) {
                _update()
            } else if (!availableRowWidthUpdater.running) {
                availableRowWidthUpdater.interval = Math.max(128 - sinceLastUpdate, 32)
                availableRowWidthUpdater.start()
            }
        }
    }

    KeyNavigableListView {
        id: view

        anchors.fill: parent

        contentWidth: root.width - root.contentLeftMargin - root.contentRightMargin

        focus: true

        headerPositioning: ListView.OverlayHeader

        flickableDirection: Flickable.AutoFlickDirection

        Navigation.parentItem: root

        onActionAtIndex: (index) => { root.actionForSelection( selectionModel.selectedIndexes ) }

        onShowContextMenu: (globalPos) => {
            if (selectionModel.hasSelection)
                root.rightClick(null, null, globalPos);
        }

        header: Rectangle {
            property alias loadedHeader: headerLoader.item

            width: view.width
            height: col.height
            z: 3
            color: root.headerColor

            // with inline header positioning and for `root.header` which changes it's height after loading,
            // in such cases after `root.header` completes, the ListView will try to maintain the relative contentY,
            // and hide the completed `root.header`, try to show the `root.header` in such cases by manually
            // positiing view at beginning
            onHeightChanged: if (root.contentY < 0) root.positionViewAtBeginning()

            Widgets.ListLabel {
                // NOTE: We want the section label to be slightly shifted to the left.
                x: row.x - VLCStyle.margin_small
                y: row.y + root.headerTopPadding

                height: VLCStyle.tableHeaderText_height
                verticalAlignment: Text.AlignVCenter

                text: view.currentSection
                color: view.colorContext.accent
                visible: view.headerPositioning === ListView.OverlayHeader
                         && text !== ""
                         && view.contentY > (row.height - col.height - row.topPadding)
            }

            Column {
                id: col

                anchors.left: parent.left
                anchors.right: parent.right

                Loader {
                    id: headerLoader

                    sourceComponent: root.header
                }

                Row {
                    id: row

                    anchors.left: parent.left
                    anchors.right: parent.right

                    anchors.leftMargin: root.contentLeftMargin
                    anchors.rightMargin: root.contentRightMargin

                    topPadding: root.headerTopPadding
                    bottomPadding: VLCStyle.margin_xsmall

                    spacing: VLCStyle.column_spacing

                    Repeater {
                        model: sortModel
                        MouseArea {

                            height: VLCStyle.tableHeaderText_height
                            width: VLCStyle.colWidth(modelData.size) || 1

                            Accessible.role: Accessible.ColumnHeader
                            Accessible.name: modelData.model.text

                            Loader {
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom

                                property var colModel: modelData.model

                                readonly property ColorContext colorContext: view.colorContext

                                sourceComponent: colModel.headerDelegate || root.tableHeaderDelegate
                            }

                            Text {
                                text: (root.model.sortOrder === Qt.AscendingOrder) ? "▼" : "▲"
                                visible: root.model.sortCriteria === modelData.model.criteria
                                font.pixelSize: VLCStyle.fontSize_normal
                                color: root.colorContext.accent

                                anchors {
                                    top: parent.top
                                    bottom: parent.bottom
                                    right: parent.right
                                    leftMargin: VLCStyle.margin_xsmall
                                    rightMargin: VLCStyle.margin_xsmall
                                }
                            }
                            onClicked: {
                                if (root.model.sortCriteria !== modelData.model.criteria)
                                    root.model.sortCriteria = modelData.model.criteria
                                else
                                    root.model.sortOrder = (root.model.sortOrder === Qt.AscendingOrder) ? Qt.DescendingOrder : Qt.AscendingOrder
                            }
                        }
                    }

                    Item {
                        // placeholder for context button

                        width: VLCStyle.icon_normal

                        height: 1
                    }
                }
            }
        }

        section.delegate: Widgets.ListLabel {
            // NOTE: We want the section label to be slightly shifted to the left.
            leftPadding: root.contentLeftMargin - VLCStyle.margin_small

            topPadding: VLCStyle.margin_xsmall

            text: section
            color: root.colorContext.accent
        }

        delegate: TableViewDelegate {
            id: tableDelegate

            width: view.width
            height: root.rowHeight

            leftPadding: root.contentLeftMargin
            rightPadding: root.contentRightMargin

            dragItem: root.dragItem

            rowModel: model
            sortModel: root.sortModel

            selected: selectionModel.selectedIndexesFlat.includes(index)

            acceptDrop: root.acceptDrop

            onContextMenuButtonClicked: (menuParent, menuModel, globalMousePos) => {
                root.contextMenuButtonClicked(menuParent, menuModel, globalMousePos)
            }
            onRightClick: (menuParent, menuModel, globalMousePos) => {
                root.rightClick(menuParent, menuModel, globalMousePos)
            }
            onItemDoubleClicked: (index, model) => {
                root.itemDoubleClicked(index, model)
            }

            onDropEntered: (drag, before) => {
                root.dropEntered(tableDelegate, index, drag, before)
            }
            onDropUpdatePosition:  (drag, before) => {
                root.dropUpdatePosition(tableDelegate, index, drag, before)
            }
            onDropExited:  (drag, before) => {
                root.dropExited(tableDelegate, index, drag, before)
            }
            onDropEvent:  (drag, drop, before) => {
                root.dropEvent(tableDelegate, index, drag, drop, before)
            }

            onSelectAndFocus: (modifiers, focusReason) => {
                selectionModel.updateSelection(modifiers, view.currentIndex, index)

                view.currentIndex = index
                view.positionViewAtIndex(index, ListView.Contain)

                tableDelegate.forceActiveFocus(focusReason)
            }

            Connections {
                target: selectionModel

                function onSelectionChanged() {
                    tableDelegate.selected = Qt.binding(function() {
                      return root.selectionModel.selectedIndexesFlat.includes(index)
                    })
                }
            }
        }
    }
}
