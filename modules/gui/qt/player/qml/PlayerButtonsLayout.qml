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


FocusScope {
    id: playerButtonsLayout

    implicitHeight: VLCStyle.maxControlbarControlHeight

    property var parentWindow: g_root

    property var colors: undefined

    property var defaultSize: VLCStyle.icon_normal // default size for IconToolButton based controls

    property real spacing: VLCStyle.margin_normal // spacing between controls
    property real layoutSpacing: VLCStyle.margin_xlarge // spacing between layouts (left, center, and right)

    property int identifier: -1
    readonly property var model: {
        if (!!mainInterface.controlbarProfileModel.currentModel)
            mainInterface.controlbarProfileModel.currentModel.getModel(identifier)
        else
            undefined
    }

    signal requestLockUnlockAutoHide(bool lock, var source)

    Component.onCompleted: {
        console.assert(identifier >= 0)
    }

    ControlButtons {
        id: controlmodelbuttons
    }

    Loader {
        id: buttonrow_left

        anchors {
            left: parent.left
            top: parent.top
            bottom: parent.bottom

            rightMargin: layoutSpacing
        }

        active: !!playerButtonsLayout.model
                && !!playerButtonsLayout.model.left

        focus: true

        sourceComponent: ButtonsLayout {
            model: playerButtonsLayout.model.left

            extraWidth: (buttonrow_center.x - buttonrow_left.x - minimumWidth - layoutSpacing)

            visible: extraWidth < 0 ? false : true // extraWidth < 0 means there is not even available space for minimumSize

            Navigation.parentItem: playerButtonsLayout
            Navigation.rightItem: buttonrow_center.item

            focus: true

            altFocusAction: Navigation.defaultNavigationRight
        }
    }

    Loader {
        id: buttonrow_center

        anchors {
            horizontalCenter: parent.horizontalCenter
            top: parent.top
            bottom: parent.bottom
        }

        active: !!playerButtonsLayout.model
                && !!playerButtonsLayout.model.center

        sourceComponent: ButtonsLayout {
            model: playerButtonsLayout.model.center

            Navigation.parentItem: playerButtonsLayout
            Navigation.leftItem: buttonrow_left.item
            Navigation.rightItem: buttonrow_right.item

            focus: true

            altFocusAction: Navigation.defaultNavigationUp
        }
    }

    Loader {
        id: buttonrow_right

        anchors {
            right: parent.right
            top: parent.top
            bottom: parent.bottom

            leftMargin: layoutSpacing
        }

        active: !!playerButtonsLayout.model
                && !!playerButtonsLayout.model.right

        sourceComponent: ButtonsLayout {
            model: playerButtonsLayout.model.right

            extraWidth: (playerButtonsLayout.width - (buttonrow_center.x + buttonrow_center.width) - minimumWidth - (2 * layoutSpacing))

            visible: extraWidth < 0 ? false : true // extraWidth < 0 means there is not even available space for minimumSize

            Navigation.parentItem: playerButtonsLayout
            Navigation.leftItem: buttonrow_center.item

            focus: true

            altFocusAction: Navigation.defaultNavigationLeft
        }
    }
}
