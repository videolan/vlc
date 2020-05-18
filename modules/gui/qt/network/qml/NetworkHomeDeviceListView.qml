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

import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/" as Util
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root

    height: deviceListView.implicitHeight

    property alias ctx: deviceModel.ctx
    property alias sd_source: deviceModel.sd_source
    property alias model: deviceModel

    property int _currentIndex: -1
    on_CurrentIndexChanged: {
        deviceListView.currentIndex = _currentIndex
    }

    function _actionAtIndex(index, model, selectionModel) {
        var data = model.getDataAt(index)
        if (data.type === NetworkMediaModel.TYPE_DIRECTORY
                || data.type === NetworkMediaModel.TYPE_NODE)  {
            history.push(["mc", "network", { tree: data.tree }]);
        } else {
            model.addAndPlay( selectionModel.selectedIndexes )
        }
    }

    onFocusChanged: {
        if (activeFocus && root._currentIndex === -1 && deviceModel.count > 0)
            root._currentIndex = 0
    }

    NetworkDeviceModel {
        id: deviceModel
    }

    Util.SelectableDelegateModel {
        id: deviceSelection
        model: deviceModel
    }

    Widgets.KeyNavigableListView {
        id: deviceListView

        focus: true

        currentIndex: root._currentIndex

        implicitHeight: VLCStyle.gridItem_network_height
        orientation: ListView.Horizontal
        anchors.fill: parent

        model: deviceModel
        delegate: NetworkGridItem {
            focus: true

            onItemClicked : {
                deviceSelection.updateSelection( modifier ,  deviceSelection.currentIndex, index)
                root._currentIndex = index
                forceActiveFocus()
            }

            onItemDoubleClicked: {
                if (model.type === NetworkMediaModel.TYPE_NODE || model.type === NetworkMediaModel.TYPE_DIRECTORY)
                    history.push( ["mc", "network", { tree: model.tree } ])
                else
                    model.addAndPlay( index )
            }
        }

        onSelectAll: deviceSelection.selectAll()
        onSelectionUpdated:  deviceSelection.updateSelection( keyModifiers, oldIndex, newIndex )
        onActionAtIndex: {
            _actionAtIndex(index, deviceModel, deviceSelection)
        }
        navigationParent: root
    }
}
