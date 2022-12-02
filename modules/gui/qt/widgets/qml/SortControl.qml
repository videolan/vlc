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

import QtQuick 2.11

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

FocusScope {
    id: root

    // Properties

    property var model: []

    property string textRole
    property string criteriaRole

    property bool popupAbove: false

    property real listWidth: VLCStyle.widthSortBox

    // NOTE: This allows us provide a custom menu and override sortMenu.
    property SortMenu menu

    // properties that should be handled by parent
    // if they are not updated, tick mark and order mark will not be shown
    property var sortKey: undefined
    property var sortOrder: undefined

    // Private

    property SortMenu _menu: (menu) ? menu
                                    : sortMenu

    // Aliases

    property alias colorContext: button.colorContext
    property alias checked: button.checked
    property alias focusPolicy: button.focusPolicy
    property alias iconSize: button.size

    // Signals

    // sortSelected is triggered with new sorting key when a different sorting key is selected
    // sortOrderSelected is triggered with Qt.AscendingOrder when different sorting key is selected
    // sortOrderSelected is triggered with Qt.AscendingOrder or Qt.DescendingOrder when the same sorting key is selected
    signal sortSelected(var key)
    signal sortOrderSelected(int type)

    // Settings

    // when height/width is explicitly set (force size), implicit values will not be used.
    // when height/width is not explicitly set, IconToolButton will set its ...
    // height and width to these implicit counterparts because ...
    // height and width will be set to implicit values when they are not ...
    // explicitly set.
    implicitWidth: button.implicitWidth
    implicitHeight: button.implicitHeight

    // Events

    onVisibleChanged: if (!visible) _menu.close()
    onEnabledChanged: if (!enabled) _menu.close()

    // Connections

    Connections {
        target: (_menu) ? _menu : null

        onSelected: {
            var selectedSortKey = root.model[index][root.criteriaRole]

            if (root.sortKey !== selectedSortKey) {
                root.sortSelected(selectedSortKey)
                root.sortOrderSelected(Qt.AscendingOrder)
            } else {
                root.sortOrderSelected(root.sortOrder === Qt.AscendingOrder ? Qt.DescendingOrder
                                                                            : Qt.AscendingOrder)
            }
        }
    }

    // Functions

    function show() {
        var model = root.model.map(function(modelData) {
            var checked = modelData[root.criteriaRole] === sortKey
            var order = checked ? root.sortOrder : undefined
            return {
                "text": modelData[root.textRole],
                "checked": checked,
                "order": order
            }
        })

        var point

        if (root.popupAbove)
            point = root.mapToGlobal(0, - VLCStyle.margin_xxsmall)
        else
            point = root.mapToGlobal(0, root.height + VLCStyle.margin_xxsmall)

        root._menu.popup(point, root.popupAbove, model)
    }

    // Children

    SortMenu { id: sortMenu }

    Widgets.IconToolButton {
        id: button

        // set height and width to root height and width so that ...
        // we can forcefully set SortControl's width and height.
        height: root.height
        width: root.width

        size: VLCStyle.icon_normal

        focus: true

        text: I18n.qtr("Sort")

        iconText: VLCIcons.topbar_sort

        Navigation.parentItem: root

        Keys.priority: Keys.AfterItem
        Keys.onPressed: Navigation.defaultKeyAction(event)

        onClicked: root.show()
    }
}
