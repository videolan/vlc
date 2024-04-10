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

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

Widgets.IconToolButton {
    id: root

    // Properties

    property var model: []

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

    // Signals

    // sortSelected is triggered with new sorting key when a different sorting key is selected
    // sortOrderSelected is triggered with Qt.AscendingOrder when different sorting key is selected
    // sortOrderSelected is triggered with Qt.AscendingOrder or Qt.DescendingOrder when the same sorting key is selected
    signal sortSelected(var key)
    signal sortOrderSelected(int type)


    font.pixelSize: VLCStyle.icon_normal

    focus: true

    description: qsTr("Sort")

    text: VLCIcons.topbar_sort

    checked: _menu && _menu.shown

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)


    // Events

    onClicked: root.show()
    onVisibleChanged: if (!visible) _menu.close()
    onEnabledChanged: if (!enabled) _menu.close()

    // Connections

    Connections {
        target: (_menu) ? _menu : null

        function onSelected() {
            const selectedSortKey = root.model[index].criteria

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
        const model = root.model.map(function(modelData) {
            const checked = modelData.criteria === sortKey
            const order = checked ? root.sortOrder : undefined
            return {
                "text": modelData.text,
                "checked": checked,
                "order": order
            }
        })

        let point

        if (root.popupAbove)
            point = root.mapToGlobal(0, - VLCStyle.margin_xxsmall)
        else
            point = root.mapToGlobal(0, root.height + VLCStyle.margin_xxsmall)

        root._menu.popup(point, root.popupAbove, model)
    }

    // Children

    SortMenu { id: sortMenu }

}

