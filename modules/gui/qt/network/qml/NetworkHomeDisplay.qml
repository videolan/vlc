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

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: topFocusScope
    focus: true

    function _centerFlickableOnItem(minY, maxY) {
        if (maxY > flickable.contentItem.contentY + flickable.height) {
            flickable.contentItem.contentY = maxY - flickable.height
        } else if (minY < flickable.contentItem.contentY) {
            flickable.contentItem.contentY = minY
        }
    }

    Label {
        anchors.centerIn: parent
        visible: (machineModel.count === 0 && lanModel.count === 0 )
        font.pixelSize: VLCStyle.fontHeight_xxlarge
        color: topFocusScope.activeFocus ? VLCStyle.colors.accent : VLCStyle.colors.text
        text: i18n.qtr("No network shares found")
    }

    NetworksSectionSelectableDM{
        id: machineDM
        model: NetworkDeviceModel {
            id: machineModel
            ctx: mainctx
            sd_source: NetworkDeviceModel.CAT_DEVICES
        }

        onCurrentIndexChanged: {
            deviceSection.currentIndex = currentIndex
        }
    }

    NetworksSectionSelectableDM{
        id: lanDM
        model: NetworkDeviceModel {
            id: lanModel
            ctx: mainctx
            sd_source: NetworkDeviceModel.CAT_LAN
        }

        onCurrentIndexChanged: {
            lanSection.currentIndex = currentIndex
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

            Widgets.LabelSeparator {
                id: deviceLabel
                text: i18n.qtr("Devices")
                width: flickable.width
                visible: machineModel.count !== 0
            }

            Widgets.KeyNavigableListView {
                id: deviceSection

                focus: false
                visible: machineModel.count !== 0
                onVisibleChanged: topFocusScope.resetFocus()

                currentIndex: machineDM.currentIndex
                onFocusChanged: {
                    if (activeFocus && machineDM.currentIndex === -1 && machineModel.count > 0)
                        machineDM.currentIndex = 0
                }

                width: flickable.width
                height: VLCStyle.gridItem_network_height
                orientation: ListView.Horizontal

                model: machineModel
                delegate: NetworkGridItem {
                    focus: true

                    onItemClicked : {
                        machineDM.updateSelection( modifier ,  machineDM.currentIndex, index)
                        machineDM.currentIndex = index
                        forceActiveFocus()
                    }

                    onItemDoubleClicked: {
                        if (model.type === NetworkMediaModel.TYPE_NODE || model.type === NetworkMediaModel.TYPE_DIRECTORY)
                            history.push( ["mc", "network", { tree: model.tree } ])
                        else
                            model.addAndPlay( index )
                    }
                }

                onSelectAll: machineDM.selectAll()
                onSelectionUpdated:  machineDM.updateSelection( keyModifiers, oldIndex, newIndex )
                onActionAtIndex: machineDM.actionAtIndex(index)

                navigationParent: topFocusScope
                navigationDownItem: lanSection.visible ?  lanSection : undefined

                onActiveFocusChanged: {
                    if (activeFocus)
                        _centerFlickableOnItem(deviceLabel.y, deviceSection.y + deviceSection.height)
                }
            }

            Widgets.LabelSeparator {
                id: lanLabel
                text: i18n.qtr("LAN")
                width: flickable.width
                visible: lanModel.count !== 0
            }

            Widgets.KeyNavigableListView {
                id: lanSection

                visible: lanModel.count !== 0
                onVisibleChanged: topFocusScope.resetFocus()
                focus: false

                currentIndex: lanDM.currentIndex
                onFocusChanged: {
                    if (activeFocus && lanDM.currentIndex === -1 && lanModel.count > 0)
                        lanDM.currentIndex = 0
                }

                width: flickable.width
                height: VLCStyle.gridItem_network_height
                orientation: ListView.Horizontal

                model: lanModel
                delegate: NetworkGridItem {
                    focus: true

                    onItemClicked : {
                        lanDM.updateSelection( modifier ,  lanDM.currentIndex, index)
                        lanDM.currentIndex = index
                        forceActiveFocus()
                    }

                    onItemDoubleClicked: {
                        if (model.type === NetworkMediaModel.TYPE_NODE || model.type === NetworkMediaModel.TYPE_DIRECTORY)
                            history.push( ["mc", "network", { tree: model.tree } ])
                        else
                            model.addAndPlay( index )
                    }
                }

                onSelectAll: lanDM.selectAll()
                onSelectionUpdated:  lanDM.updateSelection( keyModifiers, oldIndex, newIndex )
                onActionAtIndex: lanDM.actionAtIndex(index)

                navigationParent: topFocusScope
                navigationUpItem: deviceSection.visible ? deviceSection : undefined

                onActiveFocusChanged: {
                    if (activeFocus)
                        _centerFlickableOnItem(lanLabel.y, lanSection.y + lanSection.height)
                }
            }
        }

    }

    Component.onCompleted: resetFocus()
    onActiveFocusChanged: resetFocus()
    function resetFocus() {
        var widgetlist = [deviceSection, lanSection]
        var i;
        for (i in widgetlist) {
            if (widgetlist[i].activeFocus && widgetlist[i].visible)
                return
        }

        var found  = false;
        for (i in widgetlist) {
            if (widgetlist[i].visible && !found) {
                widgetlist[i].focus = true
                found = true
            } else {
                widgetlist[i].focus = false
            }
        }
    }
}
