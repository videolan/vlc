/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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
import QtQuick.Templates 2.4 as Templates

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Templates.Pane {
    id: root

    // Properties

    property int size: VLCStyle.icon_toolbar

    property VLCColors colors: VLCStyle.colors

    property bool paintOnly: false

    // Private

    readonly property string _controlPath : "qrc:///player/controlbarcontrols/"

    // Signals

    signal requestLockUnlockAutoHide(bool lock)

    // Settings

    implicitWidth: contentWidth + leftPadding + rightPadding
    implicitHeight: contentHeight + topPadding + bottomPadding

    contentWidth: row.implicitWidth
    contentHeight: row.implicitHeight

    // Keys

    Keys.priority: Keys.AfterItem
    Keys.onPressed: Navigation.defaultKeyAction(event)

    // Functions private

    function _applyItem(loader, item) {
        item.focus = true

        item.colors    = Qt.binding(function() { return colors })
        item.paintOnly = Qt.binding(function() { return paintOnly })

        item.Navigation.parentItem = Qt.binding(function() { return loader })
    }

    function _applyItemLock(loader, item) {
        if (item === null) return

        _applyItem(loader, item)

        item.requestLockUnlockAutoHide.connect(function(lock) {
            controlLayout.requestLockUnlockAutoHide(lock)
        })
    }

    // Children

    Row {
        id: row

        anchors.fill: parent

        Loader {
            id: loaderA

            anchors.verticalCenter: parent.verticalCenter

            focus: (item && item.enabled)

            // NOTE: We display the 'menu button' as a placeholder for the customize dialog.
            source: (Player.hasMenu || root.paintOnly) ? root._controlPath + "DvdMenuButton.qml"
                                                       : ""

            Navigation.parentItem: root

            Navigation.rightItem: (loaderB.item) ? loaderB.item
                                                 : loaderC.item

            onLoaded: {
                if (item === null) return

                _applyItem(loaderA, item)

                item.size = Qt.binding(function() { return root.size })
            }
        }

        Loader {
            id: loaderB

            anchors.verticalCenter: parent.verticalCenter

            focus: (item && item.enabled && loaderA.focus === false)

            source: (Player.hasPrograms
                     &&
                     root.paintOnly === false) ? root._controlPath + "ProgramButton.qml" : ""

            Navigation.parentItem: root

            Navigation.leftItem: loaderA.item
            Navigation.rightItem: loaderC.item

            onLoaded: _applyItemLock(loaderB, item)
        }

        Loader {
            id: loaderC

            anchors.verticalCenter: parent.verticalCenter

            focus: (item && item.enabled && (loaderA.focus === false && loaderB.focus === false))

            source: (Player.isTeletextAvailable
                     &&
                     root.paintOnly == false) ? _controlPath + "TeletextButton.qml" : ""

            Navigation.parentItem: root

            Navigation.leftItem: (loaderB.item) ? loaderB.item
                                                : loaderA.item

            onLoaded: _applyItemLock(loaderC, item)
        }
    }
}
