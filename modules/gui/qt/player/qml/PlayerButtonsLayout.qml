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

    property alias parentWindow: controlmodelbuttons.parentWindow

    property real marginLeft: VLCStyle.margin_normal
    property real marginRight: VLCStyle.margin_normal
    property real marginTop: 0
    property real marginBottom: 0

    property var colors: undefined

    property var defaultSize: VLCStyle.icon_normal // default size for IconToolButton based controls

    property real spacing: VLCStyle.margin_normal // spacing between controls
    property real layoutSpacing: VLCStyle.margin_xlarge // spacing between layouts (left, center, and right)

    property string identifier
    readonly property var model: {
        if (!!mainInterface.controlbarProfileModel.currentModel)
            mainInterface.controlbarProfileModel.currentModel.getModel(identifier)
        else
            undefined
    }

    signal requestLockUnlockAutoHide(bool lock, var source)


    Component.onCompleted: {
        console.assert(!!identifier)
        console.assert(identifier.length > 0)
    }

    ControlButtons {
        id: controlmodelbuttons

        parentWindow: g_root

        onRequestLockUnlockAutoHide: playerButtonsLayout.requestLockUnlockAutoHide(lock, source)
    }

    Loader {
        id: buttonrow_left

        anchors {
            left: parent.left
            verticalCenter: parent.verticalCenter

            leftMargin: marginLeft
            topMargin: marginTop
            bottomMargin: marginBottom
            rightMargin: layoutSpacing
        }

        active: !!playerButtonsLayout.model && !!playerButtonsLayout.model.left

        sourceComponent: ButtonsLayout {
            model: playerButtonsLayout.model.left

            extraWidth: (buttonrow_center.x - buttonrow_left.x - minimumWidth - layoutSpacing)

            visible: extraWidth < 0 ? false : true // extraWidth < 0 means there is not even available space for minimumSize

            navigationParent: playerButtonsLayout
            navigationRightItem: buttonrow_center

            focus: true
        }
    }

    Loader {
        id: buttonrow_center

        anchors {
            centerIn: parent

            topMargin: playerButtonsLayout.marginTop
            bottomMargin: playerButtonsLayout.marginBottom
        }

        active: !!playerButtonsLayout.model && !!playerButtonsLayout.model.center

        sourceComponent: ButtonsLayout {
            model: playerButtonsLayout.model.center

            navigationParent: playerButtonsLayout
            navigationLeftItem: buttonrow_left
            navigationRightItem: buttonrow_right
        }
    }

    Loader {
        id: buttonrow_right

        anchors {
            right: parent.right
            verticalCenter: parent.verticalCenter

            rightMargin: marginRight
            topMargin: marginTop
            bottomMargin: marginBottom
            leftMargin: layoutSpacing
        }

        active: !!playerButtonsLayout.model && !!playerButtonsLayout.model.right

        sourceComponent: ButtonsLayout {


            model: playerButtonsLayout.model.right

            extraWidth: (playerButtonsLayout.width - (buttonrow_center.x + buttonrow_center.width) - minimumWidth - (2 * layoutSpacing))

            visible: extraWidth < 0 ? false : true // extraWidth < 0 means there is not even available space for minimumSize

            navigationParent: playerButtonsLayout
            navigationLeftItem: buttonrow_center
        }
    }
}
