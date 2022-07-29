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
import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

FocusScope {
    id: root

    implicitHeight: controlBar.implicitHeight
    height: 0

    visible: false

    property alias effectSource: effect.source
    property alias effectSourceRect: effect.sourceRect
    property alias effectAvailable: effect.effectAvailable

    state: (Player.playingState === Player.PLAYING_STATE_STOPPED) ? ""
                                                                  : "expanded"

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

    // this MouseArea prevents mouse events to be sent below miniplayer
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
    }

    Widgets.FrostedGlassEffect {
        id: effect
        anchors.fill: parent

        tint: VLCStyle.colors.lowerBanner
    }

    ControlBar {
        id: controlBar

        anchors.fill: parent

        rightPadding: VLCStyle.applicationHorizontalMargin
        leftPadding: rightPadding
        bottomPadding: VLCStyle.applicationVerticalMargin

        focus: true
        colors: VLCStyle.colors
        textPosition: ControlBar.TimeTextPosition.Hide
        sliderHeight: VLCStyle.dp(3, VLCStyle.scale)
        sliderBackgroundColor: colors.sliderBarMiniplayerBgColor
        identifier: PlayerControlbarModel.Miniplayer
        Navigation.parentItem: root

        Keys.onPressed: {
            controlBar.Navigation.defaultKeyAction(event)

            if (!event.accepted) {
                MainCtx.sendHotkey(event.key, event.modifiers)
            }
        }
    }
}
