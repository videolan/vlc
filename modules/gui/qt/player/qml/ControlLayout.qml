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
import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T
import QtQuick.Layouts
import QtQml.Models

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

FocusScope {
    id: controlLayout

    // Properties

    // These delayed bindings are necessary
    // because the size of the items
    // may not be ready immediately.
    // The wise thing to do would be to not
    // delay if the sizes are ready.

    Binding on Layout.minimumWidth {
        delayed: true
        when: controlLayout._componentCompleted
        value: {
            const count = repeater.count

            if (count === 0)
                return 0

            let size = 0

            for (let i = 0; i < count; ++i) {
                const item = repeater.itemAt(i)

                if (item.Layout.minimumWidth < 0)
                    size += item.implicitWidth
                else
                    size += item.Layout.minimumWidth
            }

            return size + ((count - 1 + ((controlLayout.alignment & (Qt.AlignLeft | Qt.AlignRight)) ? 1 : 0)) * playerControlLayout.spacing)
        }
    }

    Binding on Layout.maximumWidth {
        delayed: true
        when: controlLayout._componentCompleted
        value: {
            let maximumWidth = 0
            const count = repeater.count

            for (let i = 0; i < count; ++i) {
                const item = repeater.itemAt(i)
                maximumWidth += item.implicitWidth
            }

            maximumWidth += ((count - 1 + ((alignment & (Qt.AlignLeft | Qt.AlignRight)) ? 1 : 0)) * playerControlLayout.spacing)

            return maximumWidth
        }
    }

    property alias alignment: repeater.alignment

    //function
    /* required */ property var altFocusAction

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    property bool _componentCompleted: false

    // Aliases

    property alias model: repeater.model

    property alias spacing: rowLayout.spacing

    // Signals

    signal requestLockUnlockAutoHide(bool lock)

    signal menuOpened(var menu)

    // Settings

    implicitWidth: Layout.maximumWidth
    implicitHeight: rowLayout.implicitHeight

    Navigation.navigable: {
        for (let i = 0; i < repeater.count; ++i) {
            const item = repeater.itemAt(i).item

            if (item && item.focus) {
                return true
            }
        }
        return false
    }

    // Events

    Component.onCompleted: {
        visibleChanged.connect(_handleFocus)
        activeFocusChanged.connect(_handleFocus)

        _componentCompleted = true
    }

    // Functions

    function _handleFocus() {
        if (typeof activeFocus === "undefined")
            return

        if (activeFocus && (!visible || model.count === 0))
            controlLayout.altFocusAction()
    }

    // Children

    RowLayout {
        id: rowLayout

        anchors.fill: parent

        Item {
            Layout.fillWidth: visible
            visible: (controlLayout.alignment & Qt.AlignRight)
        }

        ControlRepeater {
            id: repeater

            Navigation.parentItem: controlLayout

            availableWidth: rowLayout.width
            availableHeight: rowLayout.height

            Component.onCompleted: {
                requestLockUnlockAutoHide.connect(controlLayout.requestLockUnlockAutoHide)
                menuOpened.connect(controlLayout.menuOpened)
            }
        }

        Item {
            Layout.fillWidth: visible
            visible: (controlLayout.alignment & Qt.AlignLeft)
        }
    }
}
