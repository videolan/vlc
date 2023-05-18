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
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Templates 2.12 as T
import QtQuick.Layouts 1.12
import QtGraphicalEffects 1.12

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

T.Pane {
    id: root

    height: 0

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    visible: false

    state: (Player.playingState === Player.PLAYING_STATE_STOPPED) ? ""
                                                                  : "expanded"

    //redundant with child ControlBar
    Accessible.ignored: true

    states: State {
        name: "expanded"

        PropertyChanges {
            target: root
            visible: true
            height: implicitHeight
        }
    }

    transitions: Transition {
        from: ""; to: "expanded"
        reversible: true

        SequentialAnimation {
            // visible should change first, in order for inner layouts to calculate implicitHeight correctly
            PropertyAction { property: "visible" }
            NumberAnimation { property: "height"; easing.type: Easing.InOutSine; duration: VLCStyle.duration_long; }
        }
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    // this MouseArea prevents mouse events to be sent below miniplayer
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
    }

    background: Rectangle {
        color: theme.bg.primary
    }

    contentItem: ControlBar {
        focus: true

        // NOTE: When controls are pinned we keep the same slider in both views. Otherwise we make
        //       it more compact to fit the modern design.

        textPosition: (MainCtx.pinVideoControls) ? ControlBar.TimeTextPosition.LeftRightSlider
                                                 : ControlBar.TimeTextPosition.Hide

        sliderHeight: (MainCtx.pinVideoControls) ? VLCStyle.heightBar_xxsmall
                                                 : VLCStyle.dp(3, VLCStyle.scale)

        bookmarksHeight: (MainCtx.pinVideoControls) ? VLCStyle.controlBarBookmarksHeight
                                                    : VLCStyle.icon_xsmall * 0.7

        identifier: PlayerControlbarModel.Miniplayer

        Navigation.parentItem: root

        Keys.onPressed: {
            Navigation.defaultKeyAction(event)

            if (!event.accepted) {
                MainCtx.sendHotkey(event.key, event.modifiers)
            }
        }
    }
}
