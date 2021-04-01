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
            Layout.preferredHeight: VLCStyle.heightBar_normal

            z: 1

            background: Item { }

            EditorTabButton {
                id: mainPlayerTab

                anchors.top: parent.top
                anchors.bottom: parent.bottom

                index: 0
                text: i18n.qtr("Mainplayer")
            }

            EditorTabButton {
                id: miniPlayerTab

                anchors.top: parent.top
                anchors.bottom: parent.bottom

                index: 1
                text: i18n.qtr("Miniplayer")
            }
        }

        Rectangle{
            Layout.preferredHeight: VLCStyle.heightBar_large * 1.25
            Layout.fillWidth: true

            color: VLCStyle.colors.bgAlt

            border.color: VLCStyle.colors.accent
            border.width: VLCStyle.dp(1, VLCStyle.scale)

            StackLayout{
                anchors.fill: parent
                currentIndex: bar.currentIndex

                RowLayout {
                    id: mainPlayerLayout

                    Layout.preferredHeight: VLCStyle.heightBar_large * 1.25
                    Layout.fillWidth: true

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

                    readonly property int identifier: PlayerControlbarModel.MainPlayer
                    property var model: mainInterface.controlbarProfileModel.currentModel.getModel(identifier)

                    EditorDNDView {
                        id : playerBtnDND_left
                        Layout.fillHeight: true

                        Layout.fillWidth: count > 0 || (playerBtnDND_left.count === 0 && playerBtnDND_center.count === 0 && playerBtnDND_right.count === 0)
                        Layout.minimumWidth: centerMetric.width * 1.25
                        Layout.leftMargin: VLCStyle.margin_xsmall
                        Layout.rightMargin: VLCStyle.margin_xsmall

                        model: mainPlayerLayout.model.left

                        Text {
                            anchors.fill: parent

                            text: leftMetric.text
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: VLCStyle.fontSize_xxlarge
                            color: VLCStyle.colors.menuCaption
                            horizontalAlignment: Text.AlignHCenter
                            visible: (parent.count === 0)
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: VLCStyle.margin_small

                        Layout.fillHeight: true
                        Layout.topMargin: VLCStyle.dp(1, VLCStyle.scale)
                        Layout.bottomMargin: VLCStyle.dp(1, VLCStyle.scale)
                        Layout.alignment: Qt.AlignVCenter

                        color: VLCStyle.colors.bg
                    }

                    EditorDNDView {
                        id : playerBtnDND_center
                        Layout.fillHeight: true

                        Layout.fillWidth: count > 0 || (playerBtnDND_left.count === 0 && playerBtnDND_center.count === 0 && playerBtnDND_right.count === 0)
                        Layout.minimumWidth: centerMetric.width * 1.25
                        Layout.leftMargin: VLCStyle.margin_xsmall
                        Layout.rightMargin: VLCStyle.margin_xsmall

                        model: mainPlayerLayout.model.center

                        Text {
                            anchors.fill: parent

                            text: centerMetric.text
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: VLCStyle.fontSize_xxlarge
                            color: VLCStyle.colors.menuCaption
                            horizontalAlignment: Text.AlignHCenter
                            visible: (parent.count === 0)
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: VLCStyle.margin_small

                        Layout.fillHeight: true
                        Layout.topMargin: VLCStyle.dp(1, VLCStyle.scale)
                        Layout.bottomMargin: VLCStyle.dp(1, VLCStyle.scale)
                        Layout.alignment: Qt.AlignVCenter

                        color: VLCStyle.colors.bg
                    }

                    EditorDNDView {
                        id : playerBtnDND_right
                        Layout.fillHeight: true

                        Layout.fillWidth: count > 0 || (playerBtnDND_left.count === 0 && playerBtnDND_center.count === 0 && playerBtnDND_right.count === 0)
                        Layout.minimumWidth: centerMetric.width * 1.25
                        Layout.leftMargin: VLCStyle.margin_xsmall
                        Layout.rightMargin: VLCStyle.margin_xsmall

                        model: mainPlayerLayout.model.right

                        Text {
                            anchors.fill: parent

                            text: rightMetric.text
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: VLCStyle.fontSize_xxlarge
                            color: VLCStyle.colors.menuCaption
                            horizontalAlignment: Text.AlignHCenter
                            visible: (parent.count === 0)
                        }
                    }
                }

                RowLayout {
                    id: miniPlayerLayout

                    Layout.preferredHeight: VLCStyle.heightBar_large * 1.25
                    Layout.fillWidth: true

                    readonly property int identifier: PlayerControlbarModel.MiniPlayer
                    property var model: mainInterface.controlbarProfileModel.currentModel.getModel(identifier)

                    EditorDNDView {
                        id : miniPlayerBtnDND_left
                        Layout.fillHeight: true

                        Layout.fillWidth: count > 0 || (miniPlayerBtnDND_left.count === 0 && miniPlayerBtnDND_center.count === 0 && miniPlayerBtnDND_right.count === 0)
                        Layout.minimumWidth: centerMetric.width
                        Layout.leftMargin: VLCStyle.margin_xsmall
                        Layout.rightMargin: VLCStyle.margin_xsmall

                        model: miniPlayerLayout.model.left

                        Text {
                            anchors.fill: parent

                            text: leftMetric.text
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: VLCStyle.fontSize_xxlarge
                            color: VLCStyle.colors.menuCaption
                            horizontalAlignment: Text.AlignHCenter
                            visible: (parent.count === 0)
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: VLCStyle.margin_small

                        Layout.fillHeight: true
                        Layout.topMargin: VLCStyle.dp(1, VLCStyle.scale)
                        Layout.bottomMargin: VLCStyle.dp(1, VLCStyle.scale)
                        Layout.alignment: Qt.AlignVCenter

                        color: VLCStyle.colors.bg
                    }

                    EditorDNDView {
                        id : miniPlayerBtnDND_center
                        Layout.fillHeight: true

                        Layout.fillWidth: count > 0 || (miniPlayerBtnDND_left.count === 0 && miniPlayerBtnDND_center.count === 0 && miniPlayerBtnDND_right.count === 0)
                        Layout.minimumWidth: centerMetric.width
                        Layout.leftMargin: VLCStyle.margin_xsmall
                        Layout.rightMargin: VLCStyle.margin_xsmall

                        model: miniPlayerLayout.model.center

                        Text {
                            anchors.fill: parent

                            text: centerMetric.text
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: VLCStyle.fontSize_xxlarge
                            color: VLCStyle.colors.menuCaption
                            horizontalAlignment: Text.AlignHCenter
                            visible: (parent.count === 0)
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: VLCStyle.margin_small

                        Layout.fillHeight: true
                        Layout.topMargin: VLCStyle.dp(1, VLCStyle.scale)
                        Layout.bottomMargin: VLCStyle.dp(1, VLCStyle.scale)
                        Layout.alignment: Qt.AlignVCenter

                        color: VLCStyle.colors.bg
                    }

                    EditorDNDView {
                        id : miniPlayerBtnDND_right
                        Layout.fillHeight: true

                        Layout.fillWidth: count > 0 || (miniPlayerBtnDND_left.count === 0 && miniPlayerBtnDND_center.count === 0 && miniPlayerBtnDND_right.count === 0)
                        Layout.minimumWidth: centerMetric.width
                        Layout.leftMargin: VLCStyle.margin_xsmall
                        Layout.rightMargin: VLCStyle.margin_xsmall

                        model: miniPlayerLayout.model.right

                        Text {
                            anchors.fill: parent

                            text: rightMetric.text
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: VLCStyle.fontSize_xxlarge
                            color: VLCStyle.colors.menuCaption
                            horizontalAlignment: Text.AlignHCenter
                            visible: (parent.count === 0)
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
            handleScroll(this)
        }
    }

    property int _scrollingDirection: 0

    function getHoveredListView() {
        if (playerBtnDND_left.containsDrag)
            return playerBtnDND_left
        else if (playerBtnDND_center.containsDrag)
            return playerBtnDND_center
        else if (playerBtnDND_right.containsDrag)
            return playerBtnDND_right
        else if (miniPlayerBtnDND_left.containsDrag)
            return miniPlayerBtnDND_left
        else if (miniPlayerBtnDND_center.containsDrag)
            return miniPlayerBtnDND_center
        else if (miniPlayerBtnDND_right.containsDrag)
            return miniPlayerBtnDND_right
        else
            return undefined
    }

    function handleScroll(dragItem) {
        var view = root.getHoveredListView()

        if (view === undefined) {
            upAnimation.target = null
            downAnimation.target = null

            _scrollingDirection = 0
            return
        }

        upAnimation.target = view
        downAnimation.target = view

        downAnimation.to = Qt.binding(function() { return view.contentWidth - view.width; })

        var dragItemX = root.mapToGlobal(dragItem.x, dragItem.y).x
        var viewX     = root.mapToGlobal(view.x, view.y).x

        var leftDiff  = (viewX + VLCStyle.dp(20, VLCStyle.scale)) - dragItemX
        var rightDiff = dragItemX - (viewX + view.width - VLCStyle.dp(20, VLCStyle.scale))

        if( !view.atXBeginning && leftDiff > 0 ) {
            _scrollingDirection = -1
        }
        else if( !view.atXEnd && rightDiff > 0 ) {
            _scrollingDirection = 1
        }
        else {
            _scrollingDirection = 0
        }
    }

    SmoothedAnimation {
        id: upAnimation
        property: "contentX"
        to: 0
        running: root._scrollingDirection === -1 && target !== null

        velocity: VLCStyle.dp(150, VLCStyle.scale)
    }

    SmoothedAnimation {
        id: downAnimation
        property: "contentX"
        running: root._scrollingDirection === 1 && target !== null

        velocity: VLCStyle.dp(150, VLCStyle.scale)
    }
}
