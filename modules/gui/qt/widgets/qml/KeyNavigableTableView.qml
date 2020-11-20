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
    signal itemDoubleClicked(var model)

    property var sortModel: []
    property Component colDelegate: Widgets.ListLabel {
        property var rowModel: parent.rowModel
        property var model: parent.colModel

        anchors.fill: parent
        text: !rowModel ? "" : (rowModel[model.criteria] || "")
    }
    property Component tableHeaderDelegate: Widgets.CaptionLabel {
        text: model.text || ""
    }

    property alias model: view.model

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

    property alias footerItem: view.footerItem
    property alias footer: view.footer

    property var selectionDelegateModel
    property real rowHeight: VLCStyle.fontHeight_normal + VLCStyle.margin_large
    readonly property real availableRowWidth: width - ( VLCStyle.table_section_width * 2 )
    property alias spacing: view.spacing
    property int horizontalSpacing: VLCStyle.column_margin_width

    property alias fadeColor:             view.fadeColor
    property alias fadeRectBottomHovered: view.fadeRectBottomHovered
    property alias fadeRectTopHovered:    view.fadeRectTopHovered

    property alias add:       view.add
    property alias displaced: view.displaced

    Accessible.role: Accessible.Table

    function positionViewAtIndex(index, mode) {
        view.positionViewAtIndex(index, mode)
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

            Widgets.ListLabel {
                x: contentX - VLCStyle.table_section_width
                y: row.y
                height: row.height
                leftPadding: VLCStyle.table_section_text_margin
                text: view.currentSection
                color: VLCStyle.colors.accent
                visible: text !== "" && view.contentY > (VLCStyle.fontHeight_normal + VLCStyle.margin_xxsmall - col.height)
            }

            Column {
                id: col

                width: parent.width
                height: childrenRect.height

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
                    }
                    height: childrenRect.height + VLCStyle.margin_xxsmall
                    topPadding: VLCStyle.margin_xxsmall
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

                Rectangle {
                    width: parent.width
                    height: 1
                    color: VLCStyle.colors.textInactive
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

        delegate:Rectangle {
            id: lineView

            property var rowModel: model
            property bool selected: selectionDelegateModel.isSelected(root.model.index(index, 0))
            property alias showSeparator: separator.visible
            readonly property bool highlighted: selected || hoverArea.containsMouse || activeFocus
            readonly property int _index: index

            width: view.width
            height: root.rowHeight
            color: highlighted ? VLCStyle.colors.bgHover : "transparent"

            onHighlightedChanged: {
                showSeparator = !highlighted
                var nextItem = view.itemAtIndex(index + 1)
                if ( nextItem && lineView.ListView.nextSection === lineView.ListView.section)
                    nextItem.showSeparator = !highlighted && !nextItem.highlighted
            }

            Connections {
                target: selectionDelegateModel
                onSelectionChanged: lineView.selected = selectionDelegateModel.isSelected(root.model.index(index, 0))
            }

            MouseArea {
                id: hoverArea
                anchors.fill: parent
                hoverEnabled: true
                Keys.onMenuPressed: root.contextMenuButtonClicked(contextButton,rowModel)
                acceptedButtons: Qt.RightButton | Qt.LeftButton

                onClicked: {
                    if (mouse.button === Qt.LeftButton || !selectionDelegateModel.isSelected(root.model.index(index, 0))) {
                        selectionDelegateModel.updateSelection( mouse.modifiers , view.currentIndex, index)
                        view.currentIndex = rowModel.index
                        lineView.forceActiveFocus()
                    }

                    if (mouse.button === Qt.RightButton){
                        root.rightClick(lineView,rowModel, hoverArea.mapToGlobal(mouse.x,mouse.y) )
                    }
                }

                onDoubleClicked: {
                    actionForSelection(selectionDelegateModel.selectedIndexes)
                    root.itemDoubleClicked(model)
                }

                Rectangle {
                    id: separator

                    anchors.top: parent.top
                    anchors.right: content.right
                    width: content.width + (lineView.ListView.previousSection !== lineView.ListView.section
                                            ? VLCStyle.table_section_width : 0)
                    height: VLCStyle.heightBar_xxxsmall
                    color: VLCStyle.colors.separator
                }

                Row {
                    id: content

                    anchors {
                        topMargin: VLCStyle.margin_xxsmall
                        bottomMargin: VLCStyle.margin_xxsmall
                        leftMargin: VLCStyle.margin_xxxsmall
                        rightMargin: VLCStyle.margin_xxxsmall
                        horizontalCenter: parent.horizontalCenter
                        top: parent.top
                        bottom: parent.bottom
                    }

                    spacing: root.horizontalSpacing

                    Repeater {
                        model: sortModel

                        Item {
                            height: parent.height
                            width: modelData.width || 1
                            Layout.alignment: Qt.AlignVCenter

                            SmoothedAnimation on width {
                                duration: 256
                                easing.type: Easing.OutCubic
                            }

                            Loader{
                                property var rowModel: lineView.rowModel
                                property var colModel: modelData
                                readonly property bool currentlyFocused: lineView.activeFocus
                                readonly property bool containsMouse: hoverArea.containsMouse
                                readonly property int index: lineView._index

                                anchors.fill: parent
                                sourceComponent: colModel.colDelegate || root.colDelegate

                            }
                        }
                    }
                }

                Widgets.ContextButton {
                    anchors.right: content.right
                    anchors.top: content.top
                    anchors.bottom: content.bottom
                    backgroundColor: hovered || activeFocus ?
                                         VLCStyle.colors.getBgColor( lineView.selected, hovered,
                                                                     activeFocus ) : "transparent"

                    onClicked: root.contextMenuButtonClicked(this,  lineView.rowModel)
                    visible: hoverArea.containsMouse
                }
            }
        }

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
