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

    implicitHeight: childrenRect.height
    implicitWidth: childrenRect.width

    property alias isMiniplayer: controlmodelbuttons.isMiniplayer
    property alias parentWindow: controlmodelbuttons.parentWindow

    property real marginLeft: VLCStyle.margin_normal
    property real marginRight: VLCStyle.margin_normal
    property real marginTop: 0
    property real marginBottom: 0

    property bool forceColors: false
    
    property var models: [] // 0: left, 1: center, 2: right

    Connections {
        target: mainInterface

        onToolBarConfUpdated: {
            models[0].reloadModel()
            models[1].reloadModel()
            models[2].reloadModel()
        }
    }

    ControlButtons {
        id: controlmodelbuttons

        isMiniplayer: false
        parentWindow: mainInterfaceRect
    }

    ButtonsLayout {
        id: buttonrow_left

        model: models[0]

        implicitHeight: buttonrow.implicitHeight

        anchors {
            left: parent.left
            top: parent.top
            bottom: parent.bottom

            leftMargin: playerButtonsLayout.marginLeft

            topMargin: playerButtonsLayout.marginTop
            bottomMargin: playerButtonsLayout.marginBottom
        }

        forceColors: playerButtonsLayout.forceColors
        
        visible: model.count > 0 && (buttonrow_center.model.count > 0 ? ((x+width) < buttonrow_center.x) : true)

        navigationParent: playerButtonsLayout
        navigationRightItem: buttonrow_center

        focus: true
    }

    ButtonsLayout {
        id: buttonrow_center

        model: models[1]

        anchors {
            centerIn: parent

            topMargin: playerButtonsLayout.marginTop
            bottomMargin: playerButtonsLayout.marginBottom
        }

        forceColors: playerButtonsLayout.forceColors

        navigationParent: playerButtonsLayout
        navigationLeftItem: buttonrow_left
        navigationRightItem: buttonrow_right
    }

    ButtonsLayout {
        id: buttonrow_right

        model: models[2]

        anchors {
            right: parent.right
            top: parent.top
            bottom: parent.bottom

            rightMargin: playerButtonsLayout.marginRight

            topMargin: playerButtonsLayout.marginTop
            bottomMargin: playerButtonsLayout.marginBottom
        }

        forceColors: playerButtonsLayout.forceColors
        
        visible: model.count > 0 && (buttonrow_center.model.count > 0 ? ((buttonrow_center.x + buttonrow_center.width) < x)
                                                                      : !(((buttonrow_left.x + buttonrow_left.width) > x) && buttonrow_center.left.count > 0))

        navigationParent: playerButtonsLayout
        navigationLeftItem: buttonrow_center
    }
}
