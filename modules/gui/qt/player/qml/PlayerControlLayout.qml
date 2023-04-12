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

    property real layoutSpacing: VLCStyle.margin_xlarge // spacing between layouts (left, center, and right)

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

    // Private

    property int _minimumSpacing: layoutSpacing - spacing

    // Signals

    signal requestLockUnlockAutoHide(bool lock)

    // Settings

    implicitHeight: VLCStyle.maxControlbarControlHeight

    // Events

    Component.onCompleted: {
        console.assert(identifier >= 0)

        _updateLayout()
    }

    onWidthChanged: _updateLayout()

    // Functions

    function _updateLayout() {
        var item = loaderCenter.item

        // NOTE: Sometimes this gets called before the item is loaded.
        if (item === null)
            return

        if (item.count) {

            loaderCenter.width = Math.min(loaderCenter.implicitWidth, width)

            loaderLeft.width = loaderCenter.x - _minimumSpacing

            loaderRight.width = width - loaderCenter.x - loaderCenter.width - _minimumSpacing

        } else if (loaderRight.item.count) {

            var implicitLeft = loaderLeft.implicitWidth
            var implicitRight = loaderRight.implicitWidth

            var total = implicitLeft + implicitRight

            var size = total + _minimumSpacing

            if (size > width) {
                size = width - _minimumSpacing

                // NOTE: When both sizes are equals we expand on the left.
                if (implicitLeft >= implicitRight) {

                    loaderRight.width = Math.round(size * (implicitRight / total))

                    item = loaderRight.item

                    if (item === null)
                        return

                    var contentWidth = item.contentWidth

                    // NOTE: We assign the remaining width based on the contentWidth.
                    if (contentWidth)
                        loaderLeft.width = width - contentWidth - _minimumSpacing
                    else
                        loaderLeft.width = width
                } else {
                    loaderLeft.width = Math.round(size * (implicitLeft / total))

                    item = loaderLeft.item

                    if (item === null)
                        return

                    var contentWidth = item.contentWidth

                    // NOTE: We assign the remaining width based on the contentWidth.
                    if (contentWidth)
                        loaderRight.width = width - contentWidth - _minimumSpacing
                    else
                        loaderRight.width = width
                }
            } else {
                loaderLeft.width = implicitLeft
                loaderRight.width = implicitRight
            }
        } else
            loaderLeft.width = width
    }

    // Children

    Loader {
        id: loaderLeft

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        active: !!playerControlLayout.model && !!playerControlLayout.model.left

        focus: true

        sourceComponent: ControlLayout {
            model: ControlListFilter {
                sourceModel: playerControlLayout.model.left

                player: Player
                ctx: MainCtx
            }

            alignment: Qt.AlignLeft

            focus: true

            altFocusAction: Navigation.defaultNavigationRight

            Navigation.parentItem: playerControlLayout
            Navigation.rightItem: loaderCenter.item

            onImplicitWidthChanged: playerControlLayout._updateLayout()

            onCountChanged: playerControlLayout._updateLayout()

            onRequestLockUnlockAutoHide: playerControlLayout.requestLockUnlockAutoHide(lock)
        }
    }

    Loader {
        id: loaderCenter

        anchors.top: parent.top
        anchors.bottom: parent.bottom

        anchors.horizontalCenter: parent.horizontalCenter

        active: !!playerControlLayout.model && !!playerControlLayout.model.center

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

            onImplicitWidthChanged: playerControlLayout._updateLayout()

            onCountChanged: playerControlLayout._updateLayout()

            onRequestLockUnlockAutoHide: playerControlLayout.requestLockUnlockAutoHide(lock)
        }
    }

    Loader {
        id: loaderRight

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        active: !!playerControlLayout.model && !!playerControlLayout.model.right

        sourceComponent: ControlLayout {
            model: ControlListFilter {
                sourceModel: playerControlLayout.model.right

                player: Player
                ctx: MainCtx
            }

            alignment: Qt.AlignRight

            focus: true

            altFocusAction: Navigation.defaultNavigationLeft

            Navigation.parentItem: playerControlLayout
            Navigation.leftItem: loaderCenter.item

            onImplicitWidthChanged: playerControlLayout._updateLayout()

            onCountChanged: playerControlLayout._updateLayout()

            onRequestLockUnlockAutoHide: playerControlLayout.requestLockUnlockAutoHide(lock)
        }
    }
}
