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
    required property Widgets.DragItem dragItem

    required property var contextMenu

    readonly property bool selected: (selectionModel?.selectedIndexesFlat.includes(index) ?? false)

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

    DragHandler {
        id: dragHandler

        acceptedDevices: PointerDevice.AllDevices & ~(PointerDevice.TouchScreen)

        target: null

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

        onSingleTapped: (eventPoint, button) => {
            initialAction()

            if (!(delegate.selected && button === Qt.RightButton)) {
                const view = delegate.ListView.view
                delegate.selectionModel.updateSelection(point.modifiers, view.currentIndex, index)
                view.currentIndex = index
            }

            if (button === Qt.RightButton)
                delegate.rightClick(delegate, delegate.rowModel, eventPoint.globalPosition)
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

    DelegateTouchTapHandler {
        delegate: delegate

        onSingleTapped: (eventPoint, button) => {
            // initial action is handled in `DelegateTouchTapHandler`
            delegate.itemDoubleClicked(delegate.index, delegate.rowModel)
        }

        onDoubleTapped: (eventPoint, button) => {
            MainCtx.requestShowPlayerView()
        }

        onContextMenuRequested: (index, globalPoint) => {
            delegate.rightClick(delegate, delegate.rowModel, globalPoint)
        }
    }

    background: AnimatedBackground {
        animationDuration: VLCStyle.duration_short
        enabled: theme.initialized
        color: delegate.selected ? theme.bg.highlight : theme.bg.primary
        border.color: visualFocus ? theme.visualFocus : Qt.alpha(theme.visualFocus, 0.0)
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

                onItemChanged: {
                    if (item) {
                        // `TableViewDelegateExt` uses the last sub-delegate that declares property `artworkTextureProvider`:
                        if (item.artworkTextureProvider !== undefined) {
                            delegate.artworkTextureProvider = Qt.binding(() => loader.item?.artworkTextureProvider ?? null)
                        }
                    }
                }

                width: {
                    if (!!modelData.size)
                        return modelData.size * delegate.fixedColumnWidth
                    else if (!!modelData.weight)
                        return modelData.weight * delegate.weightedColumnWidth
                    else
                        return 0
                }
                height: parent.height

                Component.onCompleted: {
                    const del = modelData.model.colDelegate || delegate.defaultDelegate
                    // https://doc.qt.io/qt-6/qqmlincubator.html#details
                    const incubator = del.incubateObject(loader, {
                            rowModel: Qt.binding(() => delegate.rowModel),
                            colModel: Qt.binding(() => loader.modelData.model),
                            index: Qt.binding(() => delegate.index),
                            currentlyFocused: Qt.binding(() => delegate.visualFocus),
                            selected: Qt.binding(() => delegate.selected),
                            containsMouse: Qt.binding(() => delegate.hovered),
                            colorContext: theme,
                            delegate: delegate,
                            width: Qt.binding(() => loader.width),
                            height: Qt.binding(() => loader.height),
                        }, 1 /* QQmlIncubator::AsynchronousIfNested */
                    )

                    // https://doc.qt.io/qt-6/qml-qtqml-component.html#incubateObject-method
                    if (incubator.status !== Component.Ready) {
                        incubator.onStatusChanged = function(status) {
                            if (status === Component.Ready) {
                                loader.item = incubator.object
                            }
                        }
                    } else {
                        loader.item = incubator.object
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

                hoverEnabled: true

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

        index: delegate.index
        view: delegate.ListView.view
    }
}
