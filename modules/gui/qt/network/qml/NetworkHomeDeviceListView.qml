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
import QtQuick.Controls 2.4
import QtQml.Models 2.2

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/" as Util
import "qrc:///style/"

FocusScope {
    id: root

    height: deviceListView.implicitHeight

    property alias ctx: deviceModel.ctx
    property alias sd_source: deviceModel.sd_source
    property alias model: deviceModel
    property int leftPadding: VLCStyle.margin_xlarge

    property int _currentIndex: -1

    signal browse(var tree, int reason)

    on_CurrentIndexChanged: {
        deviceListView.currentIndex = _currentIndex
    }

    function setCurrentItemFocus(reason) {
        deviceListView.setCurrentItemFocus(reason);
    }

    function _actionAtIndex(index, model, selectionModel) {
        var data = model.getDataAt(index);

        if (data.type === NetworkMediaModel.TYPE_DIRECTORY
            ||
            data.type === NetworkMediaModel.TYPE_NODE)
            browse(data.tree, Qt.TabFocusReason);
        else
            model.addAndPlay( selectionModel.selectedIndexes);
    }

    onFocusChanged: {
        if (activeFocus && root._currentIndex === -1 && deviceModel.count > 0)
            root._currentIndex = 0
    }

    NetworkDeviceModel {
        id: deviceModel

        source_name: "*"
    }

    Util.SelectableDelegateModel {
        id: deviceSelection
        model: deviceModel
    }

    Widgets.KeyNavigableListView {
        id: deviceListView

        focus: true

        currentIndex: root._currentIndex

        implicitHeight: VLCStyle.gridItem_network_height + VLCStyle.gridItemSelectedBorder + VLCStyle.margin_large
        orientation: ListView.Horizontal
        anchors.fill: parent
        spacing: VLCStyle.column_margin_width

        header: Item {
            width: root.leftPadding
        }

        model: deviceModel
        delegate: NetworkGridItem {
            focus: true
            x: selectedBorderWidth
            y: selectedBorderWidth

            onItemClicked : {
                deviceSelection.updateSelection( modifier ,  deviceSelection.currentIndex, index)
                root._currentIndex = index
                forceActiveFocus()
            }

            onPlayClicked: deviceModel.addAndPlay( index )

            onItemDoubleClicked: {
                if (model.type === NetworkMediaModel.TYPE_NODE
                    ||
                    model.type === NetworkMediaModel.TYPE_DIRECTORY)
                    browse(model.tree, Qt.MouseFocusReason);
                else
                    deviceModel.addAndPlay(index);
            }
        }

        onActionAtIndex: {
            _actionAtIndex(index, deviceModel, deviceSelection)
        }
        Navigation.parentItem: root
    }
}
