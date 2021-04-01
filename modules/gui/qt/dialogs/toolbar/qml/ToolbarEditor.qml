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
import QtQuick.Layouts 1.11
import QtQml.Models 2.11

import org.videolan.vlc 0.1

import "qrc:///player/" as Player
import "qrc:///style/"
import "qrc:///widgets/" as Widgets

Rectangle{
    id: root
    color: VLCStyle.colors.bg

    property bool _held: false

    property alias removeInfoRectVisible: buttonList.removeInfoRectVisible

    property var _viewThatContainsDrag: undefined

    MouseArea {
        anchors.fill: parent
        z: -1

        visible: _held

        cursorShape: visible ? Qt.ForbiddenCursor : Qt.ArrowCursor
    }

    ColumnLayout{
        anchors.fill: parent
        spacing: 0

        TabBar {
            id: bar
            z: 1

            background: Item { }

            Repeater {
                model: PlayerListModel.model

                delegate: EditorTabButton {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom

                    text: modelData.name
                    readonly property int identifier: modelData.identifier
                }
            }
        }

        Rectangle{
            id: parentRectangle

            Layout.preferredHeight: VLCStyle.heightBar_large * 1.25
            Layout.fillWidth: true

            color: "transparent"

            border.color: VLCStyle.colors.accent
            border.width: VLCStyle.dp(1, VLCStyle.scale)

            TextMetrics {
                id: leftMetric
                text: i18n.qtr("L   E   F   T")
                font.pixelSize: VLCStyle.fontSize_xxlarge
            }

            TextMetrics {
                id: centerMetric
                text: i18n.qtr("C   E   N   T   E   R")
                font.pixelSize: VLCStyle.fontSize_xxlarge
            }

            TextMetrics {
                id: rightMetric
                text: i18n.qtr("R   I   G   H   T")
                font.pixelSize: VLCStyle.fontSize_xxlarge
            }

            StackLayout{
                anchors.fill: parent
                currentIndex: bar.currentIndex

                Repeater {
                    model: PlayerListModel.model

                    delegate: RowLayout {
                        id: layout

                        readonly property int identifier: modelData.identifier
                        readonly property var model: {
                            if (!!mainInterface.controlbarProfileModel.currentModel)
                                return mainInterface.controlbarProfileModel.currentModel.getModel(identifier)
                            else
                                return undefined
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
                                Layout.fillWidth: count > 0 ||
                                                  (repeater.itemAt(0).count === 0 &&
                                                   repeater.itemAt(1).count === 0 &&
                                                   repeater.itemAt(2).count === 0)

                                Layout.minimumWidth: Math.max(leftMetric.width,
                                                              centerMetric.width,
                                                              rightMetric.width) * 1.25
                                Layout.margins: parentRectangle.border.width

                                readonly property int count: {
                                    if (status === Loader.Ready)
                                        return item.count
                                    else
                                        return 0
                                }

                                sourceComponent: Rectangle {
                                    color: VLCStyle.colors.bgAlt

                                    property alias count: dndView.count

                                    EditorDNDView {
                                        id: dndView
                                        anchors.fill: parent

                                        model: repeater.getModel(index)

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
                                            color: VLCStyle.colors.menuCaption
                                            horizontalAlignment: Text.AlignHCenter
                                            visible: (playerBtnDND.count === 0)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle{
            id : allBtnsGrid
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.margins: VLCStyle.margin_xxsmall
            color: VLCStyle.colors.bgAlt

            ColumnLayout{
                anchors.fill: parent

                Widgets.MenuCaption {
                    Layout.margins: VLCStyle.margin_xxsmall
                    text: i18n.qtr("Drag items below to add them above: ")
                }

                ToolbarEditorButtonList {
                    id: buttonList

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: VLCStyle.margin_xxsmall
                }
            }
        }
    }

    Player.ControlButtons{
        id: controlButtons
    }

    EditorDummyButton{
        id: buttonDragItem
        visible: false
        Drag.active: visible
        color: VLCStyle.colors.buttonText

        opacity: 0.75

        function updatePos(x, y) {
            var pos = root.mapFromGlobal(x, y)
            this.x = pos.x
            this.y = pos.y
        }

        onXChanged: {
            if (buttonDragItem.Drag.active)
                handleScroll(this)
        }
    }

    function handleScroll(dragItem) {
        var view = _viewThatContainsDrag

        if (view === undefined || view === null) {
            if (!!scrollAnimation.target)
                view = scrollAnimation.target
            else
                return
        }

        var dragItemX = dragItem.x
        var viewX     = view.mapToItem(root, view.x, view.y).x

        var leftMark  = (viewX + VLCStyle.dp(20, VLCStyle.scale))
        var rightMark = (viewX + view.width - VLCStyle.dp(20, VLCStyle.scale))

        scrollAnimation.target = view
        scrollAnimation.dragItem = dragItem

        if (!view.atXBeginning && dragItemX <= leftMark) {
            scrollAnimation.direction = -1
        } else if (!view.atXEnd && dragItemX >= rightMark) {
            scrollAnimation.direction = 1
        } else {
            scrollAnimation.direction = 0
        }
    }

    SmoothedAnimation {
        id: scrollAnimation

        property var dragItem
        property int direction: 0 // -1: left, 0: stop, 1: right

        to: {
            if (direction === -1)
                0
            else if (direction === 1 && !!target)
                target.contentWidth - target.width
            else
                0
        }

        running: {
            if (!!dragItem && (direction === -1 || direction === 1))
                dragItem.Drag.active
            else
                false
        }

        property: "contentX"
        velocity: VLCStyle.dp(150, VLCStyle.scale)
        reversingMode: SmoothedAnimation.Immediate
    }
}
