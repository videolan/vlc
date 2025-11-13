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

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util

T.Control {
    id: delegate

    // Properties
    required property int index
    required property var rowModel
    required property var sortModel
    required property bool selected
    required property Widgets.DragItem dragItem

    required property var contextMenu

    readonly property bool topContainsDrag: dropAreaLayout.higherDropArea.containsDrag
    readonly property bool bottomContainsDrag: dropAreaLayout.lowerDropArea.containsDrag
    readonly property bool containsDrag: (topContainsDrag || bottomContainsDrag)

    required property real fixedColumnWidth
    required property real weightedColumnWidth

    readonly property point dragPosition: mapFromItem(dropAreaLayout,
                                                      dropAreaLayout.dragPosition.x,
                                                      dropAreaLayout.dragPosition.y)

    readonly property ItemSelectionModel selectionModel: delegate.ListView.view.selectionModel

    // Optional, used to show the drop indicator
    property alias isDropAcceptable: dropAreaLayout.isDropAcceptable

    // Optional, but required to drop a drag
    property alias acceptDrop: dropAreaLayout.acceptDrop

    property int _modifiersOnLastPress: Qt.NoModifier

    readonly property int _defaultContextMenuRequestID: index

    property int _contextMenuRequestID: -1

    property Item artworkTextureProvider

    signal rightClick(Item menuParent, var menuModel, point globalMousePos)
    signal itemDoubleClicked(var index, var model)

    signal selectAndFocus(int modifiers, int focusReason)

    Connections {
        target: contextMenu

        function onVisibleChanged() {
            if (!contextMenu.visible) _contextMenuRequestID = -1
        }
    }

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

                width: parent.width

                horizontalAlignment: defaultDelId.colModel.hCenterText ? Text.AlignHCenter : Text.AlignLeft

                text: defaultDelId.rowModel[defaultDelId.colModel.criteria] ?? ""

                color: defaultDelId.selected
                    ? defaultDelId.colorContext.fg.highlight
                    : defaultDelId.colorContext.fg.primary
            }
        }
    }
    // Settings

    hoverEnabled: true

    ListView.delayRemove: dragHandler.active

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

    // TODO: Qt bug 6.2: QTBUG-103604
    DoubleClickIgnoringItem {
        anchors.fill: parent

        z: -1

        DragHandler {
            id: dragHandler

            acceptedDevices: PointerDevice.AllDevices & ~(PointerDevice.TouchScreen)

            target: null

            grabPermissions: PointerHandler.CanTakeOverFromHandlersOfDifferentType | PointerHandler.ApprovesTakeOverByAnything

            onActiveChanged: {
                if (dragItem) {
                    if (active) {
                        if (!selected) {
                            delegate.selectionModel.select(index, ItemSelectionModel.ClearAndSelect)
                        }

                        dragItem.Drag.active = true
                    } else {
                        dragItem.Drag.drop()
                    }
                }
            }
        }

        TapHandler {
            acceptedDevices: PointerDevice.AllDevices & ~(PointerDevice.TouchScreen)

            acceptedButtons: Qt.LeftButton | Qt.RightButton

            gesturePolicy: TapHandler.ReleaseWithinBounds // TODO: Qt 6.2 bug: Use TapHandler.DragThreshold

            grabPermissions: TapHandler.CanTakeOverFromHandlersOfDifferentType | TapHandler.ApprovesTakeOverByAnything

            onSingleTapped: (eventPoint, button) => {
                initialAction()

                if (!(delegate.selected && button === Qt.RightButton)) {
                    const view = delegate.ListView.view
                    delegate.selectionModel.updateSelection(point.modifiers, view.currentIndex, index)
                    view.currentIndex = index
                }

                if (button === Qt.RightButton)
                    delegate.rightClick(delegate, delegate.rowModel, parent.mapToGlobal(eventPoint.position.x, eventPoint.position.y))
            }

            onDoubleTapped: (point, button) => {
                if (button === Qt.LeftButton)
                    delegate.itemDoubleClicked(delegate.index, delegate.rowModel)
            }

            Component.onCompleted: {
                canceled.connect(initialAction)
            }

            function initialAction() {
                delegate.forceActiveFocus(Qt.MouseFocusReason)
            }
        }

        TapHandler {
            acceptedDevices: PointerDevice.TouchScreen

            grabPermissions: TapHandler.CanTakeOverFromHandlersOfDifferentType | TapHandler.ApprovesTakeOverByAnything

            onTapped: (eventPoint, button) => {
                delegate.selectAndFocus(Qt.NoModifier, Qt.MouseFocusReason)
                delegate.itemDoubleClicked(delegate.index, delegate.rowModel)
            }

            onLongPressed: {
                delegate.rightClick(delegate, delegate.rowModel, parent.mapToGlobal(point.position.x, point.position.y))
            }
        }
    }

    background: AnimatedBackground {
        animationDuration: VLCStyle.duration_short
        enabled: theme.initialized
        color: delegate.selected ? theme.bg.highlight : theme.bg.primary
        border.color: visualFocus ? theme.visualFocus : "transparent"
    }

    contentItem: Row {
        id: contentItemRow

        spacing: VLCStyle.column_spacing

        Repeater {
            model: delegate.sortModel

            //manually load the component as Loader is unable to pass initial/required properties
            Item {
                id: loader
                required property var modelData
                property TableRowDelegate item: null
                width: {
                    if (!!modelData.size)
                        return modelData.size * delegate.fixedColumnWidth
                    else if (!!modelData.weight)
                        return modelData.weight * delegate.weightedColumnWidth
                    else
                        return 0
                }
                height: parent.height

                TableRowDelegate.CellModel {
                    id: cellModel
                    rowModel: delegate.rowModel
                    colModel: loader.modelData.model
                    index: delegate.index
                    currentlyFocused: delegate.visualFocus
                    selected: delegate.selected
                    containsMouse: delegate.hovered
                    colorContext: theme
                    delegateItem: delegate
                }

                Component.onCompleted: {
                    const del = modelData.model.colDelegate || delegate.defaultDelegate
                    item = del.createObject(loader, {
                            cellModel: cellModel,
                            width: Qt.binding(() => loader.width),
                            height: Qt.binding(() => loader.height),
                        }
                    )
                    if (item.artworkTextureProvider) {
                        delegate.artworkTextureProvider = Qt.binding(() => item.artworkTextureProvider)
                    }
                }
                Component.onDestruction: {
                    item?.destroy()
                }
            }
        }

        Item {
            width: VLCStyle.contextButton_width

            height: parent.height

            visible: !!delegate.contextMenu

            Widgets.IconToolButton {
                id: contextButton

                // place it at the end of the last column, we don't want spacing for this column
                x: - contentItemRow.spacing + VLCStyle.contextButton_margin

                anchors.verticalCenter: parent.verticalCenter

                text: VLCIcons.ellipsis

                font.pixelSize: VLCStyle.icon_normal

                description: qsTr("Menu")

                visible: delegate.hovered
                         || ((delegate.contextMenu?.currentRequest ?? delegate._defaultContextMenuRequestID)
                                    === delegate._contextMenuRequestID
                                && delegate.contextMenu.visible)

                // NOTE: QTBUG-100543
                // Hover handling in controls is blocking in Qt 6.2, meaning if this
                // control handles the hover, delegate itself won't have its `hovered`
                // set. Since this control is visible when delegate is hovered, there
                // becomes an infinite loop of visibility when this control is hovered.

                // 1) When delegate is hovered, delegate's hovered property becomes set.
                // 2) This control becomes visible.
                // 3) When this control is hovered, delegate's hovered property becomes unset.
                // 4) This control becomes invisible. Delegate's hovered property becomes set.
                // * Infinite loop *

                // Disable hovering in this control to prevent twitching due to infinite loop:
                hoverEnabled: MainCtx.qtQuickControlRejectsHoverEvents()

                onClicked: {
                    if (!delegate.selected)
                        delegate.selectAndFocus(Qt.NoModifier, Qt.MouseFocusReason)

                    const pos = contextButton.mapToGlobal(VLCStyle.margin_xsmall, contextButton.height / 2 + VLCStyle.fontHeight_normal)
                    const selectionIndexes = delegate.ListView.view.selectionModel.selectedIndexes

                    delegate._contextMenuRequestID
                            = delegate.contextMenu.tableView_popup(delegate.index, selectionIndexes, pos)
                                ?? delegate._defaultContextMenuRequestID
                }

                activeFocusOnTab: false
            }
        }
    }

    Widgets.ListViewExt.VerticalDropAreaLayout {
        id: dropAreaLayout
        anchors.fill: parent

        view: delegate.ListView.view
    }
}
