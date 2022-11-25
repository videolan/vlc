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

import QtQuick 2.11
import QtQuick.Templates 2.4 as T
import QtQuick.Layouts 1.3

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

T.Control {
    id: delegate

    // Properties

    property var rowModel
    property var sortModel

    property bool selected: false

    readonly property int _index: index

    property int _modifiersOnLastPress: Qt.NoModifier

    readonly property bool dragActive: hoverArea.drag.active

    property int horizontalSpacing: 0

    property var dragItem

    property bool acceptDrop: false

    signal contextMenuButtonClicked(Item menuParent, var menuModel, point globalMousePos)
    signal rightClick(Item menuParent, var menuModel, point globalMousePos)
    signal itemDoubleClicked(var index, var model)

    signal selectAndFocus(int modifiers, int focusReason)

    signal dropEntered(var drag, bool before)
    signal dropUpdatePosition(var drag, bool before)
    signal dropExited(var drag, bool before)
    signal dropEvent(var drag, var drop, bool before)

    property Component defaultDelegate: Widgets.ScrollingText {
        id: defaultDelId
        property var rowModel: parent.rowModel
        property var colModel: parent.colModel
        property color foregroundColor: parent.foregroundColor

        label: text
        forceScroll: parent.currentlyFocused
        width: parent.width
        clip: scrolling

        Widgets.ListLabel {
            id: text

            anchors.verticalCenter: parent.verticalCenter
            text: defaultDelId.rowModel
                    ? (defaultDelId.rowModel[defaultDelId.colModel.criteria] || "")
                    : ""
            color: defaultDelId.foregroundColor
        }
    }
    // Settings

    hoverEnabled: true
    
    ListView.delayRemove: dragActive

    Component.onCompleted: {
        Keys.menuPressed.connect(contextButton.clicked)
    }

    // Childs

    background: AnimatedBackground {
        id: background

        active: visualFocus

        animationDuration: VLCStyle.duration_short

        backgroundColor: {
            if (delegate.selected)
                return VLCStyle.colors.gridSelect;
            else if (delegate.hovered)
                return VLCStyle.colors.listHover;
            else
                return VLCStyle.colors.setColorAlpha(VLCStyle.colors.listHover, 0);
        }

        MouseArea {
            id: hoverArea

            // Settings

            anchors.fill: parent

            hoverEnabled: false

            acceptedButtons: Qt.RightButton | Qt.LeftButton

            drag.target: delegate.dragItem

            drag.axis: Drag.XAndYAxis

            // Events

            onPressed: _modifiersOnLastPress = mouse.modifiers

            onClicked: {
                if ((mouse.button === Qt.LeftButton) || !delegate.selected) {
                    delegate.selectAndFocus(mouse.modifiers, Qt.MouseFocusReason)
                }

                if (mouse.button === Qt.RightButton)
                    delegate.rightClick(delegate, delegate.rowModel, hoverArea.mapToGlobal(mouse.x, mouse.y))
            }

            onPositionChanged: {
                if (drag.active == false)
                    return;

                var pos = drag.target.parent.mapFromItem(hoverArea, mouseX, mouseY);

                drag.target.x = pos.x + VLCStyle.dragDelta;
                drag.target.y = pos.y + VLCStyle.dragDelta;
            }

            onDoubleClicked: {
                if (mouse.button === Qt.LeftButton)
                    delegate.itemDoubleClicked(delegate._index, delegate.rowModel)
            }

            drag.onActiveChanged: {
                // NOTE: Perform the "click" action because the click action is only executed on mouse
                //       release (we are in the pressed state) but we will need the updated list on drop.
                if (drag.active && !delegate.selected) {
                    delegate.selectAndFocus(_modifiersOnLastPress, index)
                } else if (delegate.dragItem) {
                    delegate.dragItem.Drag.drop()
                }

                delegate.dragItem.Drag.active = drag.active
            }
        }
    }

    contentItem: Row {
        id: content

        spacing: delegate.horizontalSpacing

        Repeater {
            model: delegate.sortModel

            Loader{
                property var rowModel: delegate.rowModel

                property var colModel: modelData

                readonly property int index: delegate._index

                readonly property bool currentlyFocused: delegate.activeFocus

                readonly property bool containsMouse: hoverArea.containsMouse

                readonly property color foregroundColor: background.foregroundColor

                width: (modelData.width) ? modelData.width : 0

                height: parent.height

                sourceComponent: (colModel.colDelegate) ? colModel.colDelegate
                                                        : delegate.defaultDelegate
            }
        }

        Item {
            width: VLCStyle.icon_normal

            height: parent.height

            Widgets.IconToolButton {
                id: contextButton

                anchors.verticalCenter: parent.verticalCenter

                iconText: VLCIcons.ellipsis

                size: VLCStyle.icon_normal

                visible: delegate.hovered

                onClicked: {
                    if (!delegate.selected)
                        delegate.selectAndFocus(Qt.NoModifier, Qt.MouseFocusReason)

                    var pos = contextButton.mapToGlobal(VLCStyle.margin_xsmall, contextButton.height / 2 + VLCStyle.fontHeight_normal)
                    delegate.contextMenuButtonClicked(this, delegate.rowModel, pos)
                }
            }
        }
    }

    DropArea {
        enabled: delegate.acceptDrop

        anchors.fill: parent

        function isBefore(drag) {
            return drag.y < height/2
        }

        onEntered: delegate.dropEntered(drag, isBefore(drag))

        onPositionChanged: delegate.dropUpdatePosition(drag, isBefore(drag))

        onExited:delegate.dropExited(drag, isBefore(drag))

        onDropped: delegate.dropEvent(drag, drop, isBefore(drag))

    }
}
