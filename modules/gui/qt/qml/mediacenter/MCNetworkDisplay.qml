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

    property alias tree: mlModel.tree
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
    Utils.SelectableDelegateModel {
        id: delegateModel

        model:  MLNetworkModel {
            id: mlModel
            ctx: mainctx
            tree: undefined
        }

        delegate: Package {
            id: element
            Loader {
                id: delegateLoader
                focus: true
                Package.name: "list"
                source: model.type == MLNetworkModel.TYPE_FILE ?
                            "qrc:///mediacenter/NetworkFileDisplay.qml" :
                            "qrc:///mediacenter/NetworkDriveDisplay.qml";
            }
            Connections {
                target: delegateLoader.item
                onActionLeft: root.actionLeft(0)
                onActionRight: root.actionRight(0)
            }

        }

        function actionAtIndex(index) {
            if ( delegateModel.selectedGroup.count > 1 ) {
                var list = []
                for (var i = 0; i < delegateModel.selectedGroup.count; i++) {
                    var type = delegateModel.selectedGroup.get(i).model.type;
                    var mrl = delegateModel.selectedGroup.get(i).model.mrl;
                    if (type == MLNetworkModel.TYPE_FILE)
                        list.push(mrl)
                }
                medialib.addAndPlay( list )
            } else {
                if (delegateModel.items.get(index).model.type != MLNetworkModel.TYPE_FILE)  {
                    root.tree = delegateModel.items.get(index).model.tree
                    history.push(["mc", "network", { tree: delegateModel.items.get(index).model.tree }], History.Stay);
                } else {
                    medialib.addAndPlay( delegateModel.items.get(index).model.mrl );
                }
            }
        }
    }

    /*
     *define the intial position/selection
     * This is done on activeFocus rather than Component.onCompleted because delegateModel.
     * selectedGroup update itself after this event
     */
    onActiveFocusChanged: {
        if (activeFocus && delegateModel.items.count > 0 && delegateModel.selectedGroup.count === 0) {
            var initialIndex = 0
            if (view.currentIndex !== -1)
                initialIndex = view.currentIndex
            delegateModel.items.get(initialIndex).inSelected = true
            view.currentIndex = initialIndex
        }
    }

    Utils.KeyNavigableListView {
        id: view
        anchors.fill: parent
        model: delegateModel.parts.list
        modelCount: delegateModel.items.count

        focus: true
        spacing: VLCStyle.margin_xxxsmall

        onSelectAll: delegateModel.selectAll()
        onSelectionUpdated: delegateModel.updateSelection( keyModifiers, oldIndex, newIndex )
        onActionAtIndex: delegateModel.actionAtIndex(index)

        onActionLeft: root.actionLeft(index)
        onActionRight: root.actionRight(index)
        onActionDown: root.actionDown(index)
        onActionUp: root.actionUp(index)
        onActionCancel: root.actionCancel(index)
    }

    Label {
        anchors.centerIn: parent
        visible: delegateModel.items.count === 0
        font.pixelSize: VLCStyle.fontHeight_xxlarge
        color: root.activeFocus ? VLCStyle.colors.accent : VLCStyle.colors.text
        text: qsTr("No network shares found")
    }
}
