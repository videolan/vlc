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
import QtQuick.Controls 2.4
import QtQml.Models 2.2
import QtQml 2.11

import org.videolan.medialib 0.1

import "qrc:///utils/" as Utils
import "qrc:///style/"

Utils.NavigableFocusScope {
    id: topFocusScope
    focus: true

    Label {
        anchors.centerIn: parent
        visible: (machineDM.items.count === 0 && lanDM.items.count === 0 )
        font.pixelSize: VLCStyle.fontHeight_xxlarge
        color: topFocusScope.activeFocus ? VLCStyle.colors.accent : VLCStyle.colors.text
        text: qsTr("No network shares found")
    }

    MCNetworksSectionSelectableDM{
        id: machineDM
        model: MLNetworkDeviceModel {
            id: machineModel
            ctx: mainctx
            sd_source: MLNetworkDeviceModel.CAT_DEVICES
        }
    }

    MCNetworksSectionSelectableDM{
        id: lanDM
        model: MLNetworkDeviceModel {
            id: lanModel
            ctx: mainctx
            sd_source: MLNetworkDeviceModel.CAT_LAN
        }
    }

    ScrollView {
        id: flickable
        anchors.fill: parent
        ScrollBar.vertical: ScrollBar{}
        focus: true

        Column {
            width: parent.width
            height: implicitHeight

            spacing: VLCStyle.margin_normal

            Utils.LabelSeparator {
                text: qsTr("Devices")
                width: flickable.width
                visible: machineDM.items.count !== 0
            }

            Utils.KeyNavigableListView {
                id: deviceSection

                focus: false
                visible: machineDM.items.count !== 0
                onVisibleChanged: topFocusScope.resetFocus()

                width: flickable.width
                height: VLCStyle.gridItem_default_height
                orientation: ListView.Horizontal

                model: machineDM.parts.grid
                modelCount: machineDM.items.count

                onSelectAll: machineDM.selectAll()
                onSelectionUpdated:  machineDM.updateSelection( keyModifiers, oldIndex, newIndex )
                onActionAtIndex: machineDM.actionAtIndex(index)

                navigationParent: topFocusScope
                navigationDownItem: lanSection.visible ?  lanSection : undefined
            }

            Utils.LabelSeparator {
                text: qsTr("LAN")
                width: flickable.width
                visible: lanDM.items.count !== 0
            }

            Utils.KeyNavigableListView {
                id: lanSection

                visible: lanDM.items.count !== 0
                onVisibleChanged: topFocusScope.resetFocus()
                focus: false

                width: flickable.width
                height: VLCStyle.gridItem_default_height
                orientation: ListView.Horizontal

                model: lanDM.parts.grid
                modelCount: lanDM.items.count

                onSelectAll: lanDM.selectAll()
                onSelectionUpdated:  lanDM.updateSelection( keyModifiers, oldIndex, newIndex )
                onActionAtIndex: lanDM.actionAtIndex(index)

                navigationParent: topFocusScope
                navigationUpItem: deviceSection.visible ? deviceSection : undefined
            }
        }

    }

    Component.onCompleted: resetFocus()
    onActiveFocusChanged: resetFocus()
    function resetFocus() {
        if (!deviceSection.focus && !lanSection.focus) {
            if (deviceSection.visible)
                deviceSection.focus = true
            else if (lanSection.visible)
                lanSection.focus = true
        }
    }
}
