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
import QtQuick.Templates 2.4 as T
import QtQuick.Layouts 1.11
import QtQml.Models 2.11

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///playlist/" as PL


Control {
    id: root

    enum TimeTextPosition {
        Hide,
        AboveSlider,
        LeftRightSlider
    }

    readonly property alias sliderY: row2.y
    property int textPosition: ControlBar.TimeTextPosition.AboveSlider
    property VLCColors colors: VLCStyle.nightColors
    property alias identifier: playerControlLayout.identifier
    property alias sliderHeight: trackPositionSlider.barHeight
    property alias sliderBackgroundColor: trackPositionSlider.backgroundColor
    property bool showRightTimeText: true

    signal requestLockUnlockAutoHide(bool lock)

    function showChapterMarks() {
        trackPositionSlider.showChapterMarks()
    }

    Keys.priority: Keys.AfterItem
    Keys.onPressed: root.Navigation.defaultKeyAction(event)
    Navigation.cancelAction: function() { History.previous() }

    onActiveFocusChanged: if (activeFocus) trackPositionSlider.forceActiveFocus(focusReason)

    Component.onCompleted: {
        // if initially textPosition = Hide, then _onTextPositionChanged isn't called
        if (textPosition === ControlBar.TimeTextPosition.Hide)
            _layout()
    }

    onTextPositionChanged: _layout()

    function _layout() {
        trackPositionSlider.visible = true
        mediaTime.visible = true

        mediaRemainingTime.visible = Qt.binding(function() { return root.showRightTimeText && textPosition !== ControlBar.TimeTextPosition.Hide; })
        mediaTime.font.pixelSize = Qt.binding(function() { return VLCStyle.fontSize_normal })
        mediaRemainingTime.font.pixelSize = Qt.binding(function() { return VLCStyle.fontSize_normal })

        row2.Layout.leftMargin = 0
        row2.Layout.rightMargin = 0

        switch (textPosition) {
        case ControlBar.TimeTextPosition.Hide:
            row1.children = []
            row2.children = [trackPositionSlider]
            mediaTime.visible = false
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
            row2.Layout.leftMargin = Qt.binding(function() { return VLCStyle.margin_xsmall })
            row2.Layout.rightMargin = Qt.binding(function() { return VLCStyle.margin_xsmall })
            mediaTime.font.pixelSize = Qt.binding(function() { return VLCStyle.fontSize_small })
            mediaRemainingTime.font.pixelSize = Qt.binding(function() { return VLCStyle.fontSize_small })
            trackPositionSlider.Layout.alignment = Qt.AlignVCenter
            break;

        default:
            console.assert(false, "invalid text position")
        }

        trackPositionSlider.Layout.fillWidth = true
        row1.visible = row1.children.length > 0
        row2.visible = row2.children.length > 0
    }

    contentItem: ColumnLayout {
        id: columnLayout

        spacing: VLCStyle.margin_xsmall

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

        PlayerControlLayout {
            id: playerControlLayout

            Layout.fillWidth: true
            Layout.leftMargin: VLCStyle.margin_large
            Layout.rightMargin: VLCStyle.margin_large
            Layout.bottomMargin: VLCStyle.margin_xsmall

            Navigation.upItem: trackPositionSlider.enabled ? trackPositionSlider : root.Navigation.upItem

            colors: root.colors

            onRequestLockUnlockAutoHide: root.requestLockUnlockAutoHide(lock)
        }
    }

    //FIXME use the right xxxLabel class
    T.Label {
        id: mediaTime

        visible: false
        text: Player.time.toString()
        color: root.colors.playerFg
        font.pixelSize: VLCStyle.fontSize_normal
    }

    //FIXME use the right xxxLabel class
    T.Label {
        id: mediaRemainingTime

        visible: false
        text: (MainCtx.showRemainingTime && Player.remainingTime.valid())
              ? "-" + Player.remainingTime.toString()
              : Player.length.toString()
        color: root.colors.playerFg
        font.pixelSize: VLCStyle.fontSize_normal

        MouseArea {
            anchors.fill: parent
            onClicked: MainCtx.showRemainingTime = !MainCtx.showRemainingTime
        }
    }

    SliderBar {
        id: trackPositionSlider

        visible: false
        backgroundColor: Qt.lighter(colors.playerBg, 1.6180)
        barHeight: VLCStyle.heightBar_xxsmall
        enabled: Player.playingState === Player.PLAYING_STATE_PLAYING || Player.playingState === Player.PLAYING_STATE_PAUSED
        colors: root.colors

        Navigation.parentItem: root
        Navigation.downItem: playerControlLayout

        Keys.onPressed: {
            Navigation.defaultKeyAction(event)
        }
    }
}
