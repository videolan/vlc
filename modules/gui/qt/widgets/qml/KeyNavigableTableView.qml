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
import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQml.Models 2.2
import QtQuick.Layouts 1.11
import QtGraphicalEffects 1.0
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
        text: model.text || ""
    }

    // NOTE: We want edge to edge backgrounds in our delegate and header, so we implement our own
    //       margins implementation like in ExpandGridView. The default values should be the same
    //       than ExpandGridView to respect the grid parti pris.
    property int leftMargin: VLCStyle.column_margin
    property int rightMargin: VLCStyle.column_margin

    readonly property int extraMargin: Math.max(0, (width - usedRowSpace) / 2)

    // NOTE: The list margins for the item(s) horizontal positioning.
    readonly property int contentLeftMargin: extraMargin + leftMargin
    readonly property int contentRightMargin: extraMargin + rightMargin

    readonly property real usedRowSpace: {
        var size = leftMargin + rightMargin

        for (var i in sortModel)
            size += sortModel[i].width

        return size + Math.max(VLCStyle.column_spacing * (sortModel.length - 1), 0)
    }

    property Component header: Item{}
    property Item headerItem: view.headerItem ? view.headerItem.loadedHeader : null
    property color headerColor
    property int headerTopPadding: 0

    property Util.SelectableDelegateModel selectionDelegateModel
    property real rowHeight: VLCStyle.tableRow_height

    property real availableRowWidth: 0
    property real _availabeRowWidthLastUpdateTime: Date.now()

    readonly property real _currentAvailableRowWidth: width - leftMargin - rightMargin

    property Item dragItem
    property bool acceptDrop: false

    // Aliases

    property alias topMargin: view.topMargin
    property alias bottomMargin: view.bottomMargin

    property alias spacing: view.spacing

    property alias model: view.model

    property alias delegate: view.delegate

    property alias contentY     : view.contentY
    property alias contentHeight: view.contentHeight

    property alias interactive: view.interactive

    property alias section: view.section

    property alias currentIndex: view.currentIndex
    property alias currentItem: view.currentItem

    property alias headerPositioning: view.headerPositioning

    property alias tableHeaderItem: view.headerItem

    property alias footerItem: view.footerItem
    property alias footer: view.footer

    property alias backgroundColor: view.backgroundColor
    property alias fadeSize: view.fadeSize

    property alias add:       view.add
    property alias displaced: view.displaced

    property alias listScrollBar: view.listScrollBar
    property alias listView: view

    property alias displayMarginEnd: view.displayMarginEnd

    property alias count: view.count

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

    // Settings

    Accessible.role: Accessible.Table

    // Events

    Component.onDestruction: {
        _qtAvoidSectionUpdate()
    }

    on_CurrentAvailableRowWidthChanged: availableRowWidthUpdater.enqueueUpdate()

    // Functions

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
        var size = index * rowHeight + topMargin

        if (tableHeaderItem)
            size += tableHeaderItem.height

        return size
    }

    // Private

    function _qtAvoidSectionUpdate() {
        // Qt SEG. FAULT WORKAROUND

        // There exists a Qt bug that tries to access null
        // pointer while updating sections. Qt does not
        // check if `QQmlEngine::contextForObject(sectionItem)->parentContext()`
        // is null and when it's null which might be the case for
        // views during destruction it causes segmentation fault.

        // As a workaround, when section delegate is set to null
        // during destruction, Qt does not proceed with updating
        // the sections so null pointer access is avoided. Updating
        // sections during destruction should not make sense anyway.

        // Setting section delegate to null seems to has no
        // negative impact and safely could be used as a fix.
        // However, the problem lying beneath prevails and
        // should be taken care of sooner than later.

        // Affected Qt versions are 5.11.3, and 5.15.2 (not
        // limited).

        section.delegate = null
    }

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
            var sinceLastUpdate = Date.now() - root._availabeRowWidthLastUpdateTime
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

        onSelectAll: selectionDelegateModel.selectAll()
        onSelectionUpdated: selectionDelegateModel.updateSelection( keyModifiers, oldIndex, newIndex )
        onActionAtIndex: root.actionForSelection( selectionDelegateModel.selectedIndexes )

        onDeselectAll: {
            if (selectionDelegateModel) {
                selectionDelegateModel.clear()
            }
        }

        onShowContextMenu: {
            if (selectionDelegateModel.hasSelection)
                root.rightClick(null, null, globalPos);
        }

        header: Rectangle {
            property alias loadedHeader: headerLoader.item

            width: view.width
            height: col.height
            color: headerColor
            visible: view.count > 0
            z: 3

            // with inline header positioning and for `root.header` which changes it's height after loading,
            // in such cases after `root.header` completes, the ListView will try to maintain the relative contentY,
            // and hide the completed `root.header`, try to show the `root.header` in such cases by manually
            // positiing view at beginning
            onHeightChanged: if (root.contentY < 0) root.positionViewAtBeginning()

            Widgets.ListLabel {
                height: row.height

                // NOTE: We want the section label to be slightly shifted to the left.
                x: row.x - VLCStyle.margin_small
                y: row.y

                topPadding: root.headerTopPadding

                text: view.currentSection
                color: VLCStyle.colors.accent
                verticalAlignment: Text.AlignTop
                visible: view.headerPositioning === ListView.OverlayHeader
                         && text !== ""
                         && view.contentY > (row.height - col.height - row.topPadding)
            }

            Column {
                id: col

                width: parent.width
                height: implicitHeight

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
                            height: childrenRect.height
                            width: modelData.width || 1
                            //Layout.alignment: Qt.AlignVCenter

                            Loader {
                                property var model: modelData.model

                                sourceComponent: model.headerDelegate || root.tableHeaderDelegate
                            }

                            Text {
                                text: (root.model.sortOrder === Qt.AscendingOrder) ? "▼" : "▲"
                                visible: root.model.sortCriteria === modelData.criteria
                                font.pixelSize: VLCStyle.fontSize_normal
                                color: VLCStyle.colors.accent
                                anchors {
                                    right: parent.right
                                    leftMargin: VLCStyle.margin_xsmall
                                    rightMargin: VLCStyle.margin_xsmall
                                }
                            }
                            onClicked: {
                                if (root.model.sortCriteria !== modelData.criteria)
                                    root.model.sortCriteria = modelData.criteria
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

            color: VLCStyle.colors.accent
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

            selected: selectionDelegateModel.isSelected(root.model.index(index, 0))

            acceptDrop: root.acceptDrop

            onContextMenuButtonClicked: root.contextMenuButtonClicked(menuParent, menuModel, globalMousePos)
            onRightClick: root.rightClick(menuParent, menuModel, globalMousePos)
            onItemDoubleClicked: root.itemDoubleClicked(index, model)

            onDropEntered: root.dropEntered(tableDelegate, index, drag, before)
            onDropUpdatePosition: root.dropUpdatePosition(tableDelegate, index, drag, before)
            onDropExited: root.dropExited(tableDelegate, index, drag, before)
            onDropEvent: root.dropEvent(tableDelegate, index, drag, drop, before)

            onSelectAndFocus:  {
                selectionDelegateModel.updateSelection(modifiers, view.currentIndex, index)

                view.currentIndex = index
                view.positionViewAtIndex(index, ListView.Contain)

                tableDelegate.forceActiveFocus(focusReason)
            }

            Connections {
                target: selectionDelegateModel

                onSelectionChanged: {
                    tableDelegate.selected = Qt.binding(function() {
                      return  selectionDelegateModel.isSelected(root.model.index(index, 0))
                    })
                }
            }
        }
    }
}
