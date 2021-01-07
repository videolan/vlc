/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets


Widgets.NavigableFocusScope {
    id: playerButtonsLayout

    implicitHeight: Math.max(buttonrow_left.implicitHeight, buttonrow_center.implicitHeight, buttonrow_right.implicitHeight)

    property alias isMiniplayer: controlmodelbuttons.isMiniplayer
    property alias parentWindow: controlmodelbuttons.parentWindow

    property real marginLeft: VLCStyle.margin_normal
    property real marginRight: VLCStyle.margin_normal
    property real marginTop: 0
    property real marginBottom: 0

    property var colors: undefined

    property var defaultSize: VLCStyle.icon_normal // default size for IconToolButton based controls

    property real spacing: VLCStyle.margin_normal // spacing between controls
    property real layoutSpacing: VLCStyle.margin_xlarge // spacing between layouts (left, center, and right)

    enum Alignment {
        Left = 0,
        Center = 1,
        Right = 2
    }

    property var models: []

    Connections {
        target: mainInterface

        onToolBarConfUpdated: {
            models[PlayerButtonsLayout.Alignment.Left].reloadModel()
            models[PlayerButtonsLayout.Alignment.Center].reloadModel()
            models[PlayerButtonsLayout.Alignment.Right].reloadModel()
        }
    }

    ControlButtons {
        id: controlmodelbuttons

        isMiniplayer: false
        parentWindow: g_root
    }

    ButtonsLayout {
        id: buttonrow_left

        model: models[PlayerButtonsLayout.Alignment.Left]

        extraWidth: (buttonrow_center.x - buttonrow_left.x - minimumWidth - layoutSpacing)

        anchors {
            left: parent.left
            verticalCenter: parent.verticalCenter

            leftMargin: marginLeft
            topMargin: marginTop
            bottomMargin: marginBottom
            rightMargin: layoutSpacing
        }
        
        visible: extraWidth < 0 ? false : true // extraWidth < 0 means there is not even available space for minimumSize

        navigationParent: playerButtonsLayout
        navigationRightItem: buttonrow_center

        focus: true
    }

    ButtonsLayout {
        id: buttonrow_center

        model: models[PlayerButtonsLayout.Alignment.Center]

        anchors {
            centerIn: parent

            topMargin: playerButtonsLayout.marginTop
            bottomMargin: playerButtonsLayout.marginBottom
        }

        navigationParent: playerButtonsLayout
        navigationLeftItem: buttonrow_left
        navigationRightItem: buttonrow_right
    }

    ButtonsLayout {
        id: buttonrow_right

        model: models[PlayerButtonsLayout.Alignment.Right]

        extraWidth: (playerButtonsLayout.width - (buttonrow_center.x + buttonrow_center.width) - minimumWidth - (2 * layoutSpacing))

        anchors {
            right: parent.right
            verticalCenter: parent.verticalCenter

            rightMargin: marginRight
            topMargin: marginTop
            bottomMargin: marginBottom
            leftMargin: layoutSpacing
        }

        visible: extraWidth < 0 ? false : true // extraWidth < 0 means there is not even available space for minimumSize

        navigationParent: playerButtonsLayout
        navigationLeftItem: buttonrow_center
    }
}
