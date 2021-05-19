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

import QtQuick         2.11
import QtQuick.Layouts 1.3

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Rectangle {
    id: delegate

    //---------------------------------------------------------------------------------------------
    // Properties
    //---------------------------------------------------------------------------------------------

    property var rowModel: model

    property bool selected: selectionDelegateModel.isSelected(root.model.index(index, 0))

    readonly property bool highlighted: (selected || hoverArea.containsMouse || activeFocus)

    readonly property int _index: index

    property int _modifiersOnLastPress: Qt.NoModifier

    readonly property color foregroundColor: (highlighted) ? VLCStyle.colors.bgHoverText
                                                           : VLCStyle.colors.text

    //---------------------------------------------------------------------------------------------
    // Settings
    //---------------------------------------------------------------------------------------------

    width: view.width

    height: root.rowHeight

    color: (highlighted) ? VLCStyle.colors.bgHover
                         : "transparent"

    //---------------------------------------------------------------------------------------------
    // Connections
    //---------------------------------------------------------------------------------------------

    Connections {
        target: selectionDelegateModel

        onSelectionChanged: {
            delegate.selected = selectionDelegateModel.isSelected(root.model.index(index, 0));
        }
    }

    //---------------------------------------------------------------------------------------------
    // Childs
    //---------------------------------------------------------------------------------------------

    MouseArea {
        id: hoverArea

        //-----------------------------------------------------------------------------------------
        // Settings

        anchors.fill: parent

        hoverEnabled: true

        Keys.onMenuPressed: root.contextMenuButtonClicked(contextButton,rowModel)

        acceptedButtons: Qt.RightButton | Qt.LeftButton

        drag.target: root.dragItem

        drag.axis: Drag.XAndYAxis

        //-----------------------------------------------------------------------------------------
        // Events

        onPressed: _modifiersOnLastPress = mouse.modifiers

        onClicked: {
            if (mouse.button === Qt.LeftButton
                ||
                selectionDelegateModel.isSelected(root.model.index(index, 0)) == false) {

                selectionDelegateModel.updateSelection(mouse.modifiers, view.currentIndex, index);

                view.currentIndex = index;

                delegate.forceActiveFocus();
            }

            if (mouse.button === Qt.RightButton)
                root.rightClick(delegate, rowModel, hoverArea.mapToGlobal(mouse.x, mouse.y));
        }

        onPositionChanged: {
            if (drag.active == false)
                return;

            var pos = drag.target.parent.mapFromItem(hoverArea, mouseX, mouseY);

            // FIXME: Shouldn't this be specified in VLCStyle ?
            var delta = VLCStyle.dp(12);

            drag.target.x = pos.x + delta;
            drag.target.y = pos.y + delta;
        }

        onDoubleClicked: {
            if (mouse.button === Qt.LeftButton)
                root.itemDoubleClicked(delegate._index, rowModel)
        }

        drag.onActiveChanged: {
            // NOTE: Perform the "click" action because the click action is only executed on mouse
            //       release (we are in the pressed state) but we will need the updated list on drop.
            if (drag.active
                &&
                selectionDelegateModel.isSelected(root.model.index(index, 0)) == false) {

                selectionDelegateModel.updateSelection(_modifiersOnLastPress, view.currentIndex,
                                                       index);
            } else if (root.dragItem) {
                root.dragItem.Drag.drop();
            }

            root.dragItem.Drag.active = drag.active;
        }

        //-----------------------------------------------------------------------------------------
        // Childs

        Row {
            id: content

            anchors.top   : parent.top
            anchors.bottom: parent.bottom

            anchors.leftMargin  : VLCStyle.margin_xxxsmall
            anchors.rightMargin : VLCStyle.margin_xxxsmall
            anchors.topMargin   : VLCStyle.margin_xxsmall
            anchors.bottomMargin: VLCStyle.margin_xxsmall

            anchors.horizontalCenter: parent.horizontalCenter

            anchors.horizontalCenterOffset: Math.round(-(root._contextButtonHorizontalSpace) / 2)

            spacing: root.horizontalSpacing

            Repeater {
                model: sortModel

                Item {
                    height: parent.height

                    width: (modelData.width) ? modelData.width
                                             : 1

                    Layout.alignment: Qt.AlignVCenter

                    SmoothedAnimation on width {
                        duration: 256

                        easing.type: Easing.OutCubic
                    }

                    Loader{
                        property var rowModel: delegate.rowModel
                        property var colModel: modelData

                        readonly property int index: delegate._index

                        readonly property bool currentlyFocused: delegate.activeFocus

                        readonly property bool containsMouse: hoverArea.containsMouse

                        readonly property color foregroundColor: delegate.foregroundColor

                        anchors.fill: parent

                        sourceComponent: (colModel.colDelegate) ? colModel.colDelegate
                                                                : root.colDelegate
                    }
                }
            }
        }

        Widgets.ContextButton {
            anchors.left: content.right

            anchors.leftMargin: VLCStyle.margin_xxsmall

            anchors.verticalCenter: content.verticalCenter

            color: delegate.foregroundColor

            backgroundColor: (hovered || activeFocus)
                             ? VLCStyle.colors.getBgColor(delegate.selected, hovered, activeFocus)
                             : "transparent"

            visible: hoverArea.containsMouse

            onClicked: root.contextMenuButtonClicked(this, delegate.rowModel)
        }

        BackgroundFocus {
            anchors.fill: parent

            visible: delegate.activeFocus
        }
    }
}
