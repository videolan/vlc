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
import QtQuick.Layouts 1.3
import QtQml.Models 2.11

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///playlist/" as PL


Widgets.NavigableFocusScope {
    id: root

    enum TimeTextPosition {
        Hide,
        AboveSlider,
        LeftRightSlider
    }

    readonly property alias sliderY: row2.y
    property int textPosition: ControlBar.TimeTextPosition.AboveSlider
    property VLCColors colors: VLCStyle.nightColors
    property alias identifier: playerButtonsLayout.identifier
    property alias sliderHeight: trackPositionSlider.barHeight
    property alias sliderBackgroundColor: trackPositionSlider.backgroundColor
    property alias sliderProgressColor: trackPositionSlider.progressBarColor

    signal requestLockUnlockAutoHide(bool lock, var source)

    Keys.priority: Keys.AfterItem
    Keys.onPressed: defaultKeyAction(event, 0)
    onActionCancel: history.previous()

    onActiveFocusChanged: if (activeFocus) trackPositionSlider.forceActiveFocus()

    implicitHeight: columnLayout.implicitHeight

    Component.onCompleted: {
        // if initially textPosition = Hide, then _onTextPositionChanged isn't called
        if (textPosition === ControlBar.TimeTextPosition.Hide)
            _layout()
    }

    onTextPositionChanged: _layout()

    function _layout() {
        trackPositionSlider.visible = true
        mediaTime.visible = true
        mediaRemainingTime.visible = true
        mediaTime.font.pixelSize = VLCStyle.fontSize_normal
        mediaRemainingTime.font.pixelSize = VLCStyle.fontSize_normal
        row2.Layout.leftMargin = 0
        row2.Layout.rightMargin = 0

        switch (textPosition) {
        case ControlBar.TimeTextPosition.Hide:
            row1.children = []
            row2.children = [trackPositionSlider]
            mediaTime.visible = false
            mediaRemainingTime.visible = false
            break;

        case ControlBar.TimeTextPosition.AboveSlider:
            var spacer = Qt.createQmlObject("import QtQuick 2.11; Item {}", row1, "ControlBar")
            row1.children = [mediaTime, spacer, mediaRemainingTime]
            spacer.Layout.fillWidth = true
            row2.children = [trackPositionSlider]
            break;

        case ControlBar.TimeTextPosition.LeftRightSlider:
            row1.children = []
            row2.children = [mediaTime, trackPositionSlider, mediaRemainingTime]
            row2.Layout.leftMargin = VLCStyle.margin_xsmall
            row2.Layout.rightMargin = VLCStyle.margin_xsmall
            mediaTime.font.pixelSize = VLCStyle.fontSize_small
            mediaRemainingTime.font.pixelSize = VLCStyle.fontSize_small
            trackPositionSlider.Layout.alignment = Qt.AlignVCenter
            break;

        default:
            console.assert(false, "invalid text position")
        }

        trackPositionSlider.Layout.fillWidth = true
        row1.visible = row1.children.length > 0
        row2.visible = row2.children.length > 0
    }

    ColumnLayout {
        id: columnLayout
        anchors.fill: parent
        spacing: VLCStyle.margin_small

        RowLayout {
            id: row1

            spacing: 0
            Layout.fillWidth: true
            Layout.leftMargin: VLCStyle.margin_normal
            Layout.rightMargin: VLCStyle.margin_normal
        }

        RowLayout {
            id: row2

            spacing: VLCStyle.margin_xsmall
            Layout.fillWidth: true
        }

        Item {
            Layout.fillWidth: true
            Layout.leftMargin: VLCStyle.margin_normal
            Layout.rightMargin: VLCStyle.margin_normal
            Layout.bottomMargin: VLCStyle.margin_xsmall
            Layout.preferredHeight: playerButtonsLayout.implicitHeight

            PlayerButtonsLayout {
                id: playerButtonsLayout

                anchors {
                    fill: parent

                    leftMargin: VLCStyle.applicationHorizontalMargin
                    rightMargin: VLCStyle.applicationHorizontalMargin
                    bottomMargin: VLCStyle.applicationVerticalMargin
                }

                navigationUpItem: trackPositionSlider.enabled ? trackPositionSlider : root.navigationUpItem

                colors: root.colors

                onRequestLockUnlockAutoHide: root.requestLockUnlockAutoHide(lock, source)
            }
        }
    }

    Label {
        id: mediaTime

        visible: false
        text: player.time.toString()
        color: root.colors.playerFg
        font.pixelSize: VLCStyle.fontSize_normal
    }

    Label {
        id: mediaRemainingTime

        visible: false
        text: (mainInterface.showRemainingTime && player.remainingTime.valid())
              ? "-" + player.remainingTime.toString()
              : player.length.toString()
        color: root.colors.playerFg
        font.pixelSize: VLCStyle.fontSize_normal

        MouseArea {
            anchors.fill: parent
            onClicked: mainInterface.showRemainingTime = !mainInterface.showRemainingTime
        }
    }

    SliderBar {
        id: trackPositionSlider

        visible: false
        backgroundColor: Qt.lighter(colors.playerBg, 1.6180)
        progressBarColor: activeFocus ? colors.accent : colors.playerControlBarFg
        barHeight: VLCStyle.heightBar_xxsmall
        enabled: player.playingState == PlayerController.PLAYING_STATE_PLAYING || player.playingState == PlayerController.PLAYING_STATE_PAUSED
        parentWindow: g_root
        colors: root.colors

        Keys.onDownPressed: playerButtonsLayout.focus = true
    }
}
