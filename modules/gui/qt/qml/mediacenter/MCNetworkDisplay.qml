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

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///utils/" as Utils
import "qrc:///style/"

Utils.NavigableFocusScope {
    id: root

    property alias tree: providerModel.tree
    Utils.MenuExt {
        id: contextMenu
        property var model: ({})
        property bool isIndexible: !contextMenu.model ? false : Boolean(contextMenu.model.can_index)
        property bool isFileType: !contextMenu.model ? false : contextMenu.model.type === MLNetworkModel.TYPE_FILE
        closePolicy: Popup.CloseOnReleaseOutside | Popup.CloseOnEscape
        focus:true

        Instantiator {
            id: instanciator
            function perform(id){
                switch(id){
                case 0: console.log("not implemented"); break;
                case 1: contextMenu.model.indexed = !contextMenu.model.indexed; break;
                default: console.log("unknown id:",id)
                }
                contextMenu.close()
            }
            model: [
                {
                    active: contextMenu.isFileType,
                    text: qsTr("Play all"),
                    performId: 0
                },
                {
                    active: contextMenu.isIndexible,
                    text: !contextMenu.model ? "" : contextMenu.model.indexed ? qsTr("Unindex") : qsTr("Index") ,
                    performId: 1
                }
            ]
            onObjectAdded: model[index].active && contextMenu.insertItem( index, object )
            onObjectRemoved: model[index].active && contextMenu.removeItem( object )
            delegate: Utils.MenuItemExt {
                focus: true
                text: modelData.text
                onTriggered: instanciator.perform(modelData.performId)
            }
        }

        onClosed: contextMenu.parent.forceActiveFocus()
    }

    MLNetworkModel {
        id: providerModel
        ctx: mainctx
        tree: undefined
    }

    MLNetworkModel {
        id: machineModel
        ctx: mainctx
        tree: undefined
        sd_source: "SD_CAT_DEVICES"
    }
    MLNetworkModel {
        id: lanModel
        ctx: mainctx
        tree: undefined
        sd_source: "SD_CAT_LAN"
    }

    MCNetworksSectionSelectableDM{
        id: delegateModel
        model: providerModel
        viewIndexPropertyName: "currentIndexProvider"
    }

    MCNetworksSectionSelectableDM{
        id: machineDM
        model: machineModel
        viewIndexPropertyName: "currentIndexMachine"
    }

    MCNetworksSectionSelectableDM{
        id: lanDM
        model: lanModel
        viewIndexPropertyName: "currentIndexLan"
    }

    /*
     *define the intial position/selection
     * This is done on activeFocus rather than Component.onCompleted because delegateModel.
     * selectedGroup update itself after this event
     */
    onActiveFocusChanged: {
        if (delegateModel.items.count > 0 && delegateModel.selectedGroup.count === 0) {
            var initialIndex = 0
            if (view.currentIndexProvider !== -1)
                initialIndex = view.currentIndexProvider
            delegateModel.items.get(initialIndex).inSelected = true
            view.currentIndexProvider = initialIndex
        }
    }


    Component{
        id: topComponent

        Utils.NavigableFocusScope {
            id: topFocusScope
            height: view.height
            width: view.width

            Label {
                anchors.centerIn: parent
                visible: (machineDM.items.count === 0 && lanDM.items.count === 0 )
                font.pixelSize: VLCStyle.fontHeight_xxlarge
                color: root.activeFocus ? VLCStyle.colors.accent : VLCStyle.colors.text
                text: qsTr("No network shares found")
            }

            ScrollView {
                id: flickable
                anchors.fill: parent
                ScrollBar.vertical: ScrollBar{}
                focus: true

                Column {
                    width: parent.width
                    height: implicitHeight

                    Utils.LabelSeparator {
                        text: qsTr("Devices")
                        width: flickable.width
                        visible: machineDM.items.count !== 0
                    }

                    Utils.KeyNavigableListView {
                        id: deviceSection

                        focus: false
                        visible: machineDM.items.count !== 0

                        width: flickable.width
                        height: 150 //FIXME
                        orientation: ListView.Horizontal

                        model: machineDM.parts.grid
                        modelCount: machineDM.items.count

                        onSelectAll: machineDM.selectAll()
                        onSelectionUpdated:  machineDM.updateSelection( keyModifiers, oldIndex, newIndex )
                        onActionAtIndex: machineDM.actionAtIndex(index)

                        navigationParent: topFocusScope
                        navigationDown: function (index) {
                            if (lanSection.visible)
                                lanSection.focus = true;
                            else
                                topFocusScope.navigationDown(index);
                        }
                    }

                    Utils.LabelSeparator {
                        text: qsTr("LAN")
                        width: flickable.width
                        visible: lanDM.items.count !== 0
                    }

                    Utils.KeyNavigableListView {
                        id: lanSection

                        visible: lanDM.items.count !== 0
                        focus: false

                        width: flickable.width
                        height: 150 //FIXME
                        orientation: ListView.Horizontal

                        model: lanDM.parts.grid
                        modelCount: lanDM.items.count

                        onSelectAll: lanDM.selectAll()
                        onSelectionUpdated:  lanDM.updateSelection( keyModifiers, oldIndex, newIndex )
                        onActionAtIndex: lanDM.actionAtIndex(index)


                        navigationParent: topFocusScope
                        navigationUp: function (index) {
                            if (deviceSection.visible)
                                deviceSection.focus = true;
                            else
                                topFocusScope.navigationUp(index);
                        }
                    }
                }

            }

            navigationParent: root

            onActiveFocusChanged: {
                if (!deviceSection.focus && !lanSection.focus) {
                    if (deviceSection.visible)
                        deviceSection.focus = true
                    else if (lanSection.visible)
                        lanSection.focus = true
                }
            }

        }
    }

    Component{
        id: gridComponent

        Utils.KeyNavigableGridView {
            id: gridView_id
            height: view.height
            width: view.width

            model: delegateModel.parts.grid
            modelCount: delegateModel.items.count
            currentIndex: view.currentIndexProvider

            focus: true

            cellWidth: VLCStyle.network_normal + VLCStyle.margin_large
            cellHeight: VLCStyle.network_normal + VLCStyle.margin_xlarge

            onSelectAll: delegateModel.selectAll()
            onSelectionUpdated:  delegateModel.updateSelection( keyModifiers, oldIndex, newIndex )
            onActionAtIndex: delegateModel.actionAtIndex(index)

            navigationParent: root
        }
    }

    Component{
        id: listComponent
        Utils.KeyNavigableListView {
            height: view.height
            width: view.width
            model: delegateModel.parts.list
            modelCount: delegateModel.items.count
            currentIndex: view.currentIndexProvider

            focus: true
            spacing: VLCStyle.margin_xxxsmall

            onSelectAll: delegateModel.selectAll()
            onSelectionUpdated: delegateModel.updateSelection( keyModifiers, oldIndex, newIndex )
            onActionAtIndex: delegateModel.actionAtIndex(index)

            navigationParent: root
        }
    }

    Utils.StackViewExt {
        id: view
        anchors.fill:parent
        clip: true
        focus: true
        initialItem: isOnProviderList ? topComponent : medialib.gridView ? gridComponent : listComponent
        property bool isOnProviderList: providerModel.is_on_provider_list
        property int currentIndexProvider: -1

        property int currentIndexMachine: -1
        property int currentIndexLan: -1
        Connections {
            target: medialib
            onGridViewChanged: {
                if (view.isOnProviderList)
                    return
                if (medialib.gridView)
                    view.replace(gridComponent)
                else
                    view.replace(listComponent)
            }
        }
    }

}
