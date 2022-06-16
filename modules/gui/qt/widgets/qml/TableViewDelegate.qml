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

    property var rowModel: model

    property bool selected: selectionDelegateModel.isSelected(root.model.index(index, 0))

    readonly property int _index: index

    property int _modifiersOnLastPress: Qt.NoModifier

    readonly property bool dragActive: hoverArea.drag.active

    // Settings

    width: Math.max(view.width, content.implicitWidth) + root.sectionWidth

    height: root.rowHeight

    leftPadding: Math.max(0, view.width - root.usedRowSpace) / 2 + root.sectionWidth

    hoverEnabled: true
    
    ListView.delayRemove: dragActive

    function selectAndFocus(modifiers, focusReason) {
        selectionDelegateModel.updateSelection(modifiers, view.currentIndex, index)

        view.currentIndex = index
        view.positionViewAtIndex(index, ListView.Contain)

        delegate.forceActiveFocus(focusReason)
    }

    // Connections

    Connections {
        target: selectionDelegateModel

        onSelectionChanged: {
            delegate.selected = Qt.binding(function() {
              return  selectionDelegateModel.isSelected(root.model.index(index, 0))
            })
        }
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

            Keys.onMenuPressed: root.contextMenuButtonClicked(contextButton,rowModel)

            acceptedButtons: Qt.RightButton | Qt.LeftButton

            drag.target: root.dragItem

            drag.axis: Drag.XAndYAxis

            // Events

            onPressed: _modifiersOnLastPress = mouse.modifiers

            onClicked: {
                if ((mouse.button === Qt.LeftButton)
                        || !selectionDelegateModel.isSelected(root.model.index(index, 0))) {
                    delegate.selectAndFocus(mouse.modifiers, Qt.MouseFocusReason)
                }

                if (mouse.button === Qt.RightButton)
                    root.rightClick(delegate, rowModel, hoverArea.mapToGlobal(mouse.x, mouse.y))
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
        }
    }

    contentItem: Row {
        id: content

        leftPadding: VLCStyle.margin_xxxsmall
        rightPadding: VLCStyle.margin_xxxsmall

        spacing: root.horizontalSpacing

        Repeater {
            model: sortModel

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
                                                        : root.colDelegate
            }
        }

        Item {
            width: root._contextButtonHorizontalSpace

            height: parent.height

            Widgets.IconToolButton {
                id: contextButton

                anchors.verticalCenter: parent.verticalCenter

                iconText: VLCIcons.ellipsis

                size: root._contextButtonHorizontalSpace

                visible: delegate.hovered

                onClicked: {
                    if (!delegate.selected)
                        delegate.selectAndFocus(Qt.NoModifier, Qt.MouseFocusReason)

                    var pos = contextButton.mapToGlobal(VLCStyle.margin_xsmall, contextButton.height / 2 + VLCStyle.fontHeight_normal)
                    root.contextMenuButtonClicked(this, delegate.rowModel, pos)
                }
            }
        }
    }
}
