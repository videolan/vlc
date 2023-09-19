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
import QtQuick.Templates as T
import QtQuick.Layouts
import QtQml.Models

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///playlist/" as PL


T.Pane {
    id: root

    enum TimeTextPosition {
        Hide,
        AboveSlider,
        LeftRightSlider
    }

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + bottomInset,
                             contentHeight + topPadding + bottomPadding)

    topInset: sliderY

    bottomPadding: VLCStyle.margin_xsmall

    readonly property real sliderY: mapFromItem(contentItem, 0, contentItem.sliderY).y
    property int textPosition: ControlBar.TimeTextPosition.AboveSlider
    property int identifier: -1
    property real sliderHeight: VLCStyle.heightBar_xxsmall
    property real bookmarksHeight: VLCStyle.controlBarBookmarksHeight
    property bool showRemainingTime: true

    property var menu: undefined

    signal requestLockUnlockAutoHide(bool lock)

    enabled: visible

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => root.Navigation.defaultKeyAction(event)
    Navigation.cancelAction: function() { History.previous(Qt.BacktabFocusReason) }

    Accessible.name: qsTr("Player controls")

    function showChapterMarks() {
        if (contentItem.trackPositionSlider)
            contentItem.trackPositionSlider.showChapterMarks()
    }

    function applyMenu(menu) {
        if (root.menu === menu)
            return

        // NOTE: When applying a new menu we hide the previous one.
        if (menu)
            dismiss()

        root.menu = menu
    }

    function dismiss() {
        if ((typeof menu === undefined) || !menu)
            return
        if (menu.hasOwnProperty("dismiss"))
            menu.dismiss()
        else if (menu.hasOwnProperty("close"))
            menu.close()
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    background: Rectangle {
        color: theme.bg.primary
    }

    contentItem: ColumnLayout {
        spacing: VLCStyle.margin_xsmall

        readonly property real sliderY: row2.y

        property alias trackPositionSlider: trackPositionSlider

        property alias mediaRemainingTime: mediaRemainingTime

        property alias playerControlLayout: playerControlLayout

        readonly property list<Item> strayItems: [
            T.Label {
                id: mediaTime
                text: Player.time.formatHMS()
                color: theme.fg.primary
                font.pixelSize: (textPosition === ControlBar.TimeTextPosition.LeftRightSlider) ? VLCStyle.fontSize_small
                                                                                               : VLCStyle.fontSize_normal

                anchors.left: (parent === pseudoRow) ? parent.left : undefined
                anchors.verticalCenter: (parent === pseudoRow) ? parent.verticalCenter : undefined
            },
            T.Label {
                id: mediaRemainingTime

                text: (MainCtx.showRemainingTime && Player.remainingTime.valid())
                      ? "-" + Player.remainingTime.formatHMS()
                      : Player.length.formatHMS()
                color: mediaTime.color
                font.pixelSize: mediaTime.font.pixelSize

                visible: root.showRemainingTime

                anchors.right: (parent === pseudoRow) ? parent.right : undefined
                anchors.verticalCenter: (parent === pseudoRow) ? parent.verticalCenter : undefined

                MouseArea {
                    anchors.fill: parent
                    onClicked: MainCtx.showRemainingTime = !MainCtx.showRemainingTime
                }
            },
            SliderBar {
                id: trackPositionSlider

                barHeight: root.sliderHeight
                Layout.fillWidth: true
                enabled: Player.playingState === Player.PLAYING_STATE_PLAYING || Player.playingState === Player.PLAYING_STATE_PAUSED

                Navigation.parentItem: root
                Navigation.downItem: playerControlLayout

                activeFocusOnTab: true

                focus: true

                Keys.onPressed: (event) => {
                    Navigation.defaultKeyAction(event)
                }
            },
            Loader {
                id: bookmarksLoader

                parent: root
                active: MainCtx.mediaLibraryAvailable
                source: "qrc:/player/Bookmarks.qml"

                x: root.leftPadding + trackPositionSlider.x + row2.Layout.leftMargin
                y: row2.y + row2.height + VLCStyle.margin_xxsmall
                width: trackPositionSlider.width

                onLoaded: {
                   item.barHeight = Qt.binding(function() { return bookmarksHeight })
                   item.controlBarHovered = Qt.binding(function() { return root.hovered })
                   item.yShift = Qt.binding(function() { return row2.height + VLCStyle.margin_xxsmall })
                }
            }
        ]

        Item {
            // BUG: RowLayout can not be used here
            // because of a Qt bug. (Height is
            // incorrectly determined. Could be
            // about nested layouting).

            id: pseudoRow
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.leftMargin: VLCStyle.margin_normal
            Layout.rightMargin: VLCStyle.margin_normal

            implicitHeight: visible ? Math.max(mediaTime.implicitHeight, mediaRemainingTime.implicitHeight) : 0

            visible: children.length > 0

            children: {
                switch (textPosition) {
                case ControlBar.TimeTextPosition.AboveSlider:
                    return [mediaTime, mediaRemainingTime]
                case ControlBar.TimeTextPosition.Hide:
                case ControlBar.TimeTextPosition.LeftRightSlider:
                default:
                    return []
                }
            }
        }

        RowLayout {
            id: row2

            children: {
                switch (textPosition) {
                case ControlBar.TimeTextPosition.Hide:
                case ControlBar.TimeTextPosition.AboveSlider:
                    return [trackPositionSlider]
                case ControlBar.TimeTextPosition.LeftRightSlider:
                    return [mediaTime, trackPositionSlider, mediaRemainingTime]
                default:
                    return []
                }
            }

            visible: children.length > 0

            spacing: VLCStyle.margin_xsmall

            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.leftMargin: (textPosition === ControlBar.TimeTextPosition.LeftRightSlider) ? VLCStyle.margin_xsmall
                                                                                              : 0

            Layout.rightMargin: Layout.leftMargin
        }

        PlayerControlLayout {
            id: playerControlLayout

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: VLCStyle.margin_large
            Layout.rightMargin: VLCStyle.margin_large

            identifier: root.identifier

            implicitHeight: MainCtx.pinVideoControls ? VLCStyle.controlLayoutHeightPinned
                                                     : VLCStyle.controlLayoutHeight

            Navigation.upItem: trackPositionSlider.enabled ? trackPositionSlider : root.Navigation.upItem

            onRequestLockUnlockAutoHide: (lock) => root.requestLockUnlockAutoHide(lock)

            onMenuOpened: (menu) => root.applyMenu(menu)
        }
    }
}
