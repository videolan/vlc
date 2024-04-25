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

import QtQuick
import QtQuick.Templates as T
import QtQuick.Layouts

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

T.Control {
    id: delegate

    // Properties
    required property int index
    required property var rowModel
    required property var sortModel
    required property bool selected
    required property Widgets.DragItem dragItem
    required property bool acceptDrop

    readonly property bool dragActive: hoverArea.drag.active

    property int _modifiersOnLastPress: Qt.NoModifier

    signal contextMenuButtonClicked(Item menuParent, var menuModel, point globalMousePos)
    signal rightClick(Item menuParent, var menuModel, point globalMousePos)
    signal itemDoubleClicked(var index, var model)

    signal selectAndFocus(int modifiers, int focusReason)

    signal dropEntered(var drag, bool before)
    signal dropUpdatePosition(var drag, bool before)
    signal dropExited(var drag, bool before)
    signal dropEvent(var drag, var drop, bool before)

    property Component defaultDelegate: TableRowDelegate {
        id: defaultDelId
        Widgets.TextAutoScroller {

            anchors.fill: parent

            label: text
            forceScroll: defaultDelId.currentlyFocused
            clip: scrolling

            Widgets.ListLabel {
                id: text

                anchors.verticalCenter: parent.verticalCenter
                text: defaultDelId.rowModel[defaultDelId.colModel.criteria] ?? ""

                color: defaultDelId.selected
                    ? defaultDelId.colorContext.fg.highlight
                    : defaultDelId.colorContext.fg.primary
            }
        }
    }
    // Settings

    hoverEnabled: true

    ListView.delayRemove: dragActive

    Component.onCompleted: {
        Keys.menuPressed.connect(contextButton.clicked)
    }

    // Childs

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Item

        focused: delegate.visualFocus
        hovered: delegate.hovered
    }

    background: AnimatedBackground {
        animationDuration: VLCStyle.duration_short
        enabled: theme.initialized
        color: delegate.selected ? theme.bg.highlight : theme.bg.primary
        border.color: visualFocus ? theme.visualFocus : "transparent"

        MouseArea {
            id: hoverArea

            // Settings

            anchors.fill: parent

            hoverEnabled: false

            acceptedButtons: Qt.RightButton | Qt.LeftButton

            drag.target: delegate.dragItem

            drag.axis: Drag.XAndYAxis

            drag.smoothed: false

            // Events

            onPressed: (mouse) => {
                _modifiersOnLastPress = mouse.modifiers
            }

            onClicked: (mouse) => {
                if ((mouse.button === Qt.LeftButton) || !delegate.selected) {
                    delegate.selectAndFocus(mouse.modifiers, Qt.MouseFocusReason)
                }

                if (mouse.button === Qt.RightButton)
                    delegate.rightClick(delegate, delegate.rowModel, hoverArea.mapToGlobal(mouse.x, mouse.y))
            }

            onDoubleClicked: (mouse) => {
                if (mouse.button === Qt.LeftButton)
                    delegate.itemDoubleClicked(delegate.index, delegate.rowModel)
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

            TapHandler {
                acceptedDevices: PointerDevice.TouchScreen

                onTapped: {
                    delegate.selectAndFocus(Qt.NoModifier, Qt.MouseFocusReason)
                    delegate.itemDoubleClicked(delegate.index, delegate.rowModel)
                }

                onLongPressed: {
                    delegate.rightClick(delegate, delegate.rowModel, point.scenePosition)
                }
            }
        }
    }

    contentItem: Row {
        leftPadding: VLCStyle.margin_xxxsmall
        rightPadding: VLCStyle.margin_xxxsmall

        spacing: VLCStyle.column_spacing

        Repeater {
            model: delegate.sortModel

            //manually load the component as Loader is unable to pass initial/required properties
            Item {
                id: loader
                required property var modelData
                property TableRowDelegate item: null
                width: (modelData.size) ? VLCStyle.colWidth(modelData.size) : 0
                height: parent.height

                Component.onCompleted: {
                    const del = modelData.model.colDelegate || delegate.defaultDelegate
                    item = del.createObject(loader, {
                            width: Qt.binding(() => loader.width),
                            height: Qt.binding(() => loader.height),
                            rowModel: Qt.binding(() => delegate.rowModel),
                            colModel: Qt.binding(() => loader.modelData.model),
                            index: Qt.binding(() => delegate.index),
                            currentlyFocused: Qt.binding(() => delegate.activeFocus),
                            selected: Qt.binding(() => delegate.selected),
                            containsMouse: Qt.binding(() => hoverArea.containsMouse),
                            colorContext: Qt.binding(() => theme),
                        }
                    )
                }
                Component.onDestruction: {
                    item?.destroy()
                }
            }
        }

        Item {
            width: VLCStyle.icon_normal

            height: parent.height

            Widgets.IconToolButton {
                id: contextButton

                anchors.left: parent.left

                // NOTE: We want the contextButton to be contained inside the trailing
                //       column_spacing.
                anchors.leftMargin: -width

                anchors.verticalCenter: parent.verticalCenter

                text: VLCIcons.ellipsis

                font.pixelSize: VLCStyle.icon_normal

                description: qsTr("Menu")

                visible: delegate.hovered

                onClicked: {
                    if (!delegate.selected)
                        delegate.selectAndFocus(Qt.NoModifier, Qt.MouseFocusReason)

                    const pos = contextButton.mapToGlobal(VLCStyle.margin_xsmall, contextButton.height / 2 + VLCStyle.fontHeight_normal)
                    delegate.contextMenuButtonClicked(this, delegate.rowModel, pos)
                }

                activeFocusOnTab: false
            }
        }
    }

    DropArea {
        enabled: delegate.acceptDrop

        anchors.fill: parent

        function isBefore(drag) {
            return drag.y < height/2
        }

        onEntered: (drag) => { delegate.dropEntered(drag, isBefore(drag)) }

        onPositionChanged: (drag) => { delegate.dropUpdatePosition(drag, isBefore(drag)) }

        onExited:delegate.dropExited(drag, isBefore(drag))

        onDropped: (drop) => { delegate.dropEvent(drag, drop, isBefore(drag)) }

    }
}
