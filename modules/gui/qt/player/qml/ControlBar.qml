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

import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Templates 2.12 as T
import QtQuick.Layouts 1.12
import QtQml.Models 2.12

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
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)

    bottomPadding: VLCStyle.margin_xsmall

    readonly property alias sliderY: row2.y
    property int textPosition: ControlBar.TimeTextPosition.AboveSlider
    property alias identifier: playerControlLayout.identifier
    property alias sliderHeight: trackPositionSlider.barHeight
    property real bookmarksHeight: VLCStyle.controlBarBookmarksHeight

    property var menu: undefined

    signal requestLockUnlockAutoHide(bool lock)

    enabled: visible

    Keys.priority: Keys.AfterItem
    Keys.onPressed: root.Navigation.defaultKeyAction(event)
    Navigation.cancelAction: function() { History.previous() }

    Accessible.name: I18n.qtr("Player controls")

    function showChapterMarks() {
        trackPositionSlider.showChapterMarks()
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
        z: 1

        RowLayout {
            id: row1

            children: {
                switch (textPosition) {
                case ControlBar.TimeTextPosition.AboveSlider:
                    return [mediaTime, spacer, mediaRemainingTime]
                case ControlBar.TimeTextPosition.Hide:
                case ControlBar.TimeTextPosition.LeftRightSlider:
                default:
                    return []
                }
            }

            visible: children.length > 0

            spacing: 0
            Layout.fillWidth: true
            Layout.leftMargin: VLCStyle.margin_normal
            Layout.rightMargin: VLCStyle.margin_normal
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

            Navigation.upItem: trackPositionSlider.enabled ? trackPositionSlider : root.Navigation.upItem

            onRequestLockUnlockAutoHide: root.requestLockUnlockAutoHide(lock)

            onMenuOpened: root.applyMenu(menu)
        }
    }

    readonly property list<Item> strayItems: [
        T.Label {
            id: mediaTime
            text: Player.time.formatHMS()
            color: theme.fg.primary
            font.pixelSize: (textPosition === ControlBar.TimeTextPosition.LeftRightSlider) ? VLCStyle.fontSize_small
                                                                                           : VLCStyle.fontSize_normal
        },
        T.Label {
            id: mediaRemainingTime

            text: (MainCtx.showRemainingTime && Player.remainingTime.valid())
                  ? "-" + Player.remainingTime.formatHMS()
                  : Player.length.formatHMS()
            color: mediaTime.color
            font.pixelSize: mediaTime.font.pixelSize

            MouseArea {
                anchors.fill: parent
                onClicked: MainCtx.showRemainingTime = !MainCtx.showRemainingTime
            }
        },
        Item {
            id: spacer

            Layout.fillWidth: true
        },
        SliderBar {
            id: trackPositionSlider

            barHeight: VLCStyle.heightBar_xxsmall
            Layout.fillWidth: true
            enabled: Player.playingState === Player.PLAYING_STATE_PLAYING || Player.playingState === Player.PLAYING_STATE_PAUSED

            Navigation.parentItem: root
            Navigation.downItem: playerControlLayout

            activeFocusOnTab: true

            focus: true

            Keys.onPressed: {
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
}
