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
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.0

import "qrc:///util/" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///style/"

NavigableFocusScope {
    id: root

    //forwarded from subview
    signal actionForSelection( var selection )
    signal contextMenuButtonClicked(Item menuParent, var menuModel)
    signal rightClick(Item menuParent, var menuModel, var globalMousePos)
    signal itemDoubleClicked(var index, var model)

    property var sortModel: []
    property Component colDelegate: Widgets.ListLabel {
        property var rowModel: parent.rowModel
        property var model: parent.colModel

        anchors.fill: parent
        text: !rowModel ? "" : (rowModel[model.criteria] || "")
        color: parent.foregroundColor
    }
    property Component tableHeaderDelegate: Widgets.CaptionLabel {
        text: model.text || ""
    }

    property alias model: view.model

    property alias delegate: view.delegate

    property alias contentY     : view.contentY
    property alias contentHeight: view.contentHeight

    property alias interactive: view.interactive

    property alias section: view.section

    property alias currentIndex: view.currentIndex
    property alias currentItem: view.currentItem

    property alias headerPositioning: view.headerPositioning
    property Component header: Item{}
    property var headerItem: view.headerItem.loadedHeader
    property alias tableHeaderItem: view.headerItem
    property color headerColor
    property int headerTopPadding: 0

    property alias footerItem: view.footerItem
    property alias footer: view.footer

    property var selectionDelegateModel
    property real rowHeight: VLCStyle.tableRow_height
    readonly property int _contextButtonHorizontalSpace: VLCStyle.icon_normal + VLCStyle.margin_xxsmall * 2
    property alias spacing: view.spacing
    property int horizontalSpacing: VLCStyle.column_margin_width

    property real availableRowWidth: 0
    property real _availabeRowWidthLastUpdateTime: Date.now()

    property alias fadeColor:             view.fadeColor
    property alias fadeRectBottomHovered: view.fadeRectBottomHovered
    property alias fadeRectTopHovered:    view.fadeRectTopHovered

    property alias add:       view.add
    property alias displaced: view.displaced
    property Item dragItem

    property alias listScrollBar: view.listScrollBar

    property alias displayMarginEnd: view.displayMarginEnd

    Accessible.role: Accessible.Table

    function positionViewAtIndex(index, mode) {
        view.positionViewAtIndex(index, mode)
    }

    function positionViewAtBeginning() {
        view.listView.positionViewAtBeginning()
    }

    Timer {
        id: availableRowWidthUpdater

        interval: 100
        triggeredOnStart: false
        repeat: false
        onTriggered: {
            _update()
        }

        function _update() {
            root.availableRowWidth = root.width
                    - ( !!section.property ? VLCStyle.table_section_width * 2 : 0 )
                    - _contextButtonHorizontalSpace
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

    onWidthChanged: {
        availableRowWidthUpdater.enqueueUpdate()
    }

    onSectionChanged: {
        availableRowWidthUpdater.enqueueUpdate()
    }

    KeyNavigableListView {
        id: view

        anchors.fill: parent

        focus: true

        headerPositioning: ListView.OverlayHeader

        header: Rectangle {

            readonly property alias contentX: row.x
            readonly property alias contentWidth: row.width
            property alias loadedHeader: headerLoader.item

            width: parent.width
            height: col.height
            color: headerColor
            visible: view.modelCount > 0
            z: 3

            // with inline header positioning and for `root.header` which changes it's height after loading,
            // in such cases after `root.header` completes, the ListView will try to maintain the relative contentY,
            // and hide the completed `root.header`, try to show the `root.header` in such cases by manually
            // positiing view at beginning
            onHeightChanged: if (root.contentY < 0) root.positionViewAtBeginning()

            Widgets.ListLabel {
                x: contentX - VLCStyle.table_section_width
                y: row.y
                height: row.height
                topPadding: root.headerTopPadding
                leftPadding: VLCStyle.table_section_text_margin
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

                    anchors {
                        leftMargin: VLCStyle.margin_xxxsmall
                        rightMargin: VLCStyle.margin_xxxsmall
                        horizontalCenter: parent.horizontalCenter
                        horizontalCenterOffset: - root._contextButtonHorizontalSpace / 2
                    }
                    height: implicitHeight
                    topPadding: root.headerTopPadding
                    bottomPadding: VLCStyle.margin_xsmall
                    spacing: root.horizontalSpacing

                    Repeater {
                        model: sortModel
                        MouseArea {
                            height: childrenRect.height
                            width: modelData.width || 1
                            //Layout.alignment: Qt.AlignVCenter

                            Loader {
                                property var model: modelData

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
                }
            }
        }

        section.delegate: Widgets.ListLabel {
            x: view.headerItem.contentX - VLCStyle.table_section_width
            topPadding: VLCStyle.margin_xsmall
            bottomPadding: VLCStyle.margin_xxsmall
            leftPadding: VLCStyle.table_section_text_margin
            text: section
            color: VLCStyle.colors.accent
        }

        delegate: TableViewDelegate {}

        onSelectAll: selectionDelegateModel.selectAll()
        onSelectionUpdated: selectionDelegateModel.updateSelection( keyModifiers, oldIndex, newIndex )
        onActionAtIndex: root.actionForSelection( selectionDelegateModel.selectedIndexes )

        navigationParent: root
    }

    /*
     *define the intial position/selection
     * This is done on activeFocus rather than Component.onCompleted because delegateModel.
     * selectedGroup update itself after this event
     */
    onActiveFocusChanged: {
        if (activeFocus && view.count > 0 && !selectionDelegateModel.hasSelection) {
            var initialIndex = 0
            if (view.currentIndex !== -1)
                initialIndex = view.currentIndex
            selectionDelegateModel.select(model.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect)
            view.currentIndex = initialIndex
        }
    }

}
