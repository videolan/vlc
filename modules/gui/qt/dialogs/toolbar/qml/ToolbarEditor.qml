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
import QtQuick.Layouts
import QtQml.Models

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///util/" as Util

Item {
    id: root

    property bool dragActive: !!_viewThatContainsDrag || buttonDragItem.Drag.active

    property alias removeInfoRectVisible: buttonList.removeInfoRectVisible

    property EditorDNDView _viewThatContainsDrag: null

    signal dragStarted(int controlId)
    signal dragStopped(int controlId)

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    ColumnLayout{
        anchors.fill: parent
        spacing: 0

        TabBar {
            id: bar
            z: 1

            background: Rectangle {
                color: theme.bg.primary
            }

            readonly property int currentIdentifier: currentItem.identifier

            Repeater {
                model: PlayerListModel.model

                delegate: EditorTabButton {
                    readonly property int identifier: modelData.identifier

                    selected: index === bar.currentIndex

                    implicitWidth: VLCStyle.button_width_large

                    text: {
                        const text = modelData.name

                        if (MainCtx.controlbarProfileModel.currentModel?.getModel(identifier).dirty)
                            return _markDirty(text)
                        else
                            return text
                    }

                    onDropEnterred: {
                        bar.currentIndex = index
                    }
                }
            }
        }

        Item {
            Layout.preferredHeight: VLCStyle.controlLayoutHeight * 1.5
            Layout.fillWidth: true

            TextMetrics {
                id: leftMetric
                text: qsTr("L   E   F   T")
                font.pixelSize: VLCStyle.fontSize_xxlarge
            }

            TextMetrics {
                id: centerMetric
                text: qsTr("C   E   N   T   E   R")
                font.pixelSize: VLCStyle.fontSize_xxlarge
            }

            TextMetrics {
                id: rightMetric
                text: qsTr("R   I   G   H   T")
                font.pixelSize: VLCStyle.fontSize_xxlarge
            }

            Repeater {
                model: PlayerListModel.model

                delegate: RowLayout {
                    id: layout

                    // can't use StackLayout or change visibility
                    // because there is a bug with the dragging
                    // that it doesn't work when the visibility
                    // is set to false, so instead use stacking
                    // and width/height to show the current view
                    clip: true
                    z: bar.currentIdentifier === identifier ? 0 : -1
                    width: bar.currentIdentifier === identifier ? parent.width : 0
                    height: bar.currentIdentifier === identifier ? parent.height : 0
                    visible: root.dragActive || (bar.currentIdentifier === identifier)

                    readonly property int identifier: modelData.identifier
                    readonly property PlayerControlbarModel model: {
                        if (!!MainCtx.controlbarProfileModel.currentModel)
                            return MainCtx.controlbarProfileModel.currentModel.getModel(identifier)
                        else
                            return null
                    }

                    spacing: VLCStyle.margin_small

                    Repeater {
                        id: repeater

                        model: 3 // left, center, and right

                        function getModel(index) {
                            if (!!layout.model) {
                                switch (index) {
                                case 0:
                                    return layout.model.left
                                case 1:
                                    return layout.model.center
                                case 2:
                                    return layout.model.right
                                default:
                                    return undefined
                                }
                            } else {
                                return undefined
                            }
                        }

                        function getMetric(index) {
                            switch (index) {
                            case 0:
                                return leftMetric
                            case 1:
                                return centerMetric
                            case 2:
                                return rightMetric
                            }
                        }

                        Loader {
                            id : playerBtnDND
                            active: !!repeater.getModel(index)

                            Layout.fillHeight: true
                            Layout.fillWidth: {
                                if (count === 0) {
                                    for (let i = 0; i < repeater.count; ++i) {
                                        const item = repeater.itemAt(i)
                                        if (!!item && item.count > 0)
                                            return false
                                    }
                                }

                                return true
                            }

                            Layout.minimumWidth: (!!item && item.visible && item.count <= 0) ? Math.max(leftMetric.width,
                                                                                                      centerMetric.width,
                                                                                                      rightMetric.width) * 1.25
                                                                                             : 0

                            readonly property int count: item?.count ?? 0

                            sourceComponent: Rectangle {
                                color: theme.bg.primary

                                border.color: theme.border
                                border.width: VLCStyle.border

                                property alias count: dndView.count

                                Connections {
                                    target: root
                                    enabled: dndView.model === layout.model.center

                                    function onDragStarted(controlId) {
                                        // extending spacer widget should not be placed in the
                                        // central alignment view
                                        if (controlId === ControlListModel.WIDGET_SPACER_EXTEND)
                                            visible = false
                                    }

                                    function onDragStopped(controlId) {
                                        if (controlId === ControlListModel.WIDGET_SPACER_EXTEND)
                                            visible = true
                                    }
                                }

                                EditorDNDView {
                                    id: dndView
                                    anchors.fill: parent
                                    anchors.leftMargin: spacing
                                    anchors.rightMargin: spacing

                                    model: repeater.getModel(index)

                                    // controls in the center view can not have
                                    // extra width
                                    extraWidthAvailable: model !== layout.model.center

                                    onContainsDragChanged: {
                                        if (containsDrag)
                                            _viewThatContainsDrag = this
                                        else if (_viewThatContainsDrag === this)
                                            _viewThatContainsDrag = null
                                    }

                                    Text {
                                        anchors.fill: parent

                                        text: repeater.getMetric(index).text
                                        verticalAlignment: Text.AlignVCenter
                                        font.pixelSize: VLCStyle.fontSize_xxlarge
                                        color: theme.fg.secondary
                                        horizontalAlignment: Text.AlignHCenter
                                        visible: (count === 0)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.topMargin: VLCStyle.margin_xxsmall

            color: buttonList.colorContext.bg.primary
            border.color: theme.border
            border.width: VLCStyle.border

            ColumnLayout {
                anchors.fill: parent

                Widgets.MenuCaption {
                    Layout.margins: VLCStyle.margin_xxsmall
                    text: qsTr("Drag items below to add them above: ")
                    color: buttonList.colorContext.fg.primary
                }

                ToolbarEditorButtonList {
                    id: buttonList

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: VLCStyle.margin_xxsmall

                    Component.onCompleted: {
                        dragStarted.connect(root.dragStarted)
                        dragStopped.connect(root.dragStopped)
                    }
                }
            }
        }
    }

    EditorDummyButton {
        id: buttonDragItem

        visible: Drag.active
        color: theme.fg.primary
        opacity: 0.75

        x: -1
        y: -1

        Drag.onActiveChanged: {
            dragAutoScrollHandler.dragItem = Drag.active ? this : null
        }
    }

    Util.ViewDragAutoScrollHandler {
        id: dragAutoScrollHandler

        view: _viewThatContainsDrag ?? null
    }
}
