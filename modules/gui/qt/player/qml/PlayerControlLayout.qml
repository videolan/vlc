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
    id: playerControlLayout

    // Properties

    property real defaultSize: VLCStyle.icon_normal // default size for IconToolButton based controls

    property real spacing: VLCStyle.margin_normal // spacing between controls

    property real layoutSpacing: VLCStyle.margin_xxlarge // spacing between layouts (left, center, and right)

    property int identifier: -1

    readonly property PlayerControlbarModel model: {
        if (!!MainCtx.controlbarProfileModel.currentModel)
            return MainCtx.controlbarProfileModel.currentModel.getModel(identifier)
        else
            return null
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    // Signals

    signal requestLockUnlockAutoHide(bool lock)

    // Settings

    implicitWidth: loaderLeft.implicitWidth + loaderCenter.implicitWidth
                   + loaderRight.implicitWidth + 2 * layoutSpacing

    implicitHeight: VLCStyle.maxControlbarControlHeight

    // Events

    Component.onCompleted: console.assert(identifier >= 0)

    // Children

    Loader {
        id: loaderLeft

        anchors {
            right: loaderCenter.left
            left: parent.left
            top: parent.top
            bottom: parent.bottom

            // Spacing for the filler item acts as padding
            rightMargin: layoutSpacing - spacing
        }

        active: !!playerControlLayout.model && !!playerControlLayout.model.left

        focus: true

        sourceComponent: ControlLayout {
            model: ControlListFilter {
                sourceModel: playerControlLayout.model.left

                player: Player
                ctx: MainCtx
            }

            focus: true

            altFocusAction: Navigation.defaultNavigationRight

            Navigation.parentItem: playerControlLayout
            Navigation.rightItem: loaderCenter.item

            onRequestLockUnlockAutoHide: playerControlLayout.requestLockUnlockAutoHide(lock)
        }
    }

    Loader {
        id: loaderCenter

        anchors {
            horizontalCenter: parent.horizontalCenter
            top: parent.top
            bottom: parent.bottom
        }

        active: !!playerControlLayout.model && !!playerControlLayout.model.center

        width: (parent.width < implicitWidth) ? parent.width
                                              : implicitWidth

        sourceComponent: ControlLayout {
            model: ControlListFilter {
                sourceModel: playerControlLayout.model.center

                player: Player
                ctx: MainCtx
            }

            focus: true

            altFocusAction: Navigation.defaultNavigationUp

            Navigation.parentItem: playerControlLayout
            Navigation.leftItem: loaderLeft.item
            Navigation.rightItem: loaderRight.item

            onRequestLockUnlockAutoHide: playerControlLayout.requestLockUnlockAutoHide(lock)
        }
    }

    Loader {
        id: loaderRight

        anchors {
            left: loaderCenter.right
            right: parent.right
            top: parent.top
            bottom: parent.bottom

            // Spacing for the filler item acts as padding
            leftMargin: layoutSpacing - spacing
        }

        active: !!playerControlLayout.model && !!playerControlLayout.model.right

        sourceComponent: ControlLayout {
            model: ControlListFilter {
                sourceModel: playerControlLayout.model.right

                player: Player
                ctx: MainCtx
            }

            rightAligned: true

            focus: true

            altFocusAction: Navigation.defaultNavigationLeft

            Navigation.parentItem: playerControlLayout
            Navigation.leftItem: loaderCenter.item

            onRequestLockUnlockAutoHide: playerControlLayout.requestLockUnlockAutoHide(lock)
        }
    }
}
