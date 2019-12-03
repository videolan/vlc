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

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root

    property alias tree: providerModel.tree

    property var extraLocalActions: ObjectModel {
        Widgets.TabButtonExt {
            text:  providerModel.indexed ?  i18n.qtr("Remove from medialibrary") : i18n.qtr("Add to medialibrary")
            visible: !providerModel.is_on_provider_list && providerModel.canBeIndexed
            onClicked: providerModel.indexed = !providerModel.indexed
        }
    }

    NetworkMediaModel {
        id: providerModel
        ctx: mainctx
        tree: undefined
    }

    NetworksSectionSelectableDM{
        id: delegateModel
        model: providerModel
    }

    /*
     *define the intial position/selection
     * This is done on activeFocus rather than Component.onCompleted because delegateModel.
     * selectedGroup update itself after this event
     */
    onActiveFocusChanged: {
        if (delegateModel.items.count > 0 && delegateModel.selectedGroup.count === 0) {
            var initialIndex = 0
            if (delegateModel.currentIndex !== -1)
                initialIndex = delegateModel.currentIndex
            delegateModel.items.get(initialIndex).inSelected = true
            delegateModel.currentIndex = initialIndex
        }
    }

    Widgets.MenuExt {
        id: contextMenu
        property var delegateModel: undefined
        property var model: ({})
        closePolicy: Popup.CloseOnReleaseOutside | Popup.CloseOnEscape
        focus:true

        Instantiator {
            id: instanciator
            property var modelActions: {
                "play": function() {
                    if (delegateModel) {
                        delegateModel.playSelection()
                    }
                    contextMenu.close()
                },
                "enqueue": function() {
                    if (delegateModel)
                        delegateModel.enqueueSelection()
                    contextMenu.close()
                },
                "index": function(index) {
                    contextMenu.model.indexed = contextMenu.model.indexed
                    contextMenu.close()
                }
            }

            model: [{
                    active: true,
                    text: i18n.qtr("Play"),
                    action: "play"
                }, {
                    active: true,
                    text: i18n.qtr("Enqueue"),
                    action: "enqueue"
                }, {
                    active:  contextMenu.model && !!contextMenu.model.can_index,
                    text: contextMenu.model && contextMenu.model.indexed ? i18n.qtr("Remove from Media Library") : i18n.qtr("Add to Media Library") ,
                    action: "index"
                }
            ]

            onObjectAdded: model[index].active && contextMenu.insertItem( index, object )
            onObjectRemoved: model[index].active && contextMenu.removeItem( object )
            delegate: Widgets.MenuItemExt {
                focus: true
                text: modelData.text
                onTriggered: {
                    if (modelData.action && instanciator.modelActions[modelData.action]) {
                        instanciator.modelActions[modelData.action]()
                    }
                }
            }
        }

        onClosed: contextMenu.parent.forceActiveFocus()
    }


    Component{
        id: gridComponent

        Widgets.ExpandGridView {
            model: delegateModel
            modelCount: delegateModel.items.count

            headerDelegate: Widgets.LabelSeparator {
                text: providerModel.name
                width: view.width
            }

            cellWidth: VLCStyle.network_normal + VLCStyle.margin_large
            cellHeight: VLCStyle.network_normal + VLCStyle.margin_xlarge

            delegate: Widgets.GridItem {
                id: delegateGrid
                property var model: ({})

                pictureWidth: VLCStyle.network_normal
                pictureHeight: VLCStyle.network_normal

                image: {
                    switch (model.type){
                    case NetworkMediaModel.TYPE_DISC:
                        return  "qrc:///type/disc.svg"
                    case NetworkMediaModel.TYPE_CARD:
                        return  "qrc:///type/capture-card.svg"
                    case NetworkMediaModel.TYPE_STREAM:
                        return  "qrc:///type/stream.svg"
                    case NetworkMediaModel.TYPE_PLAYLIST:
                        return  "qrc:///type/playlist.svg"
                    case NetworkMediaModel.TYPE_FILE:
                        return  "qrc:///type/file_black.svg"
                    default:
                        return "qrc:///type/directory_black.svg"
                    }
                }
                subtitle: model.mrl || ""
                title: model.name || i18n.qtr("Unknown share")
                showContextButton: true

                onItemClicked : {
                    delegateModel.updateSelection( modifier ,  delegateModel.currentIndex, index)
                    delegateModel.currentIndex = index
                    delegateGrid.forceActiveFocus()
                }

                onItemDoubleClicked: {
                    if (model.type === NetworkMediaModel.TYPE_NODE || model.type === NetworkMediaModel.TYPE_DIRECTORY)
                        history.push( ["mc", "network", { tree: model.tree } ], History.Go)
                    else
                        delegateModel.model.addAndPlay( index )
                }

                onContextMenuButtonClicked: {
                    contextMenu.model = model
                    contextMenu.delegateModel = delegateModel
                    contextMenu.popup(menuParent)
                }
            }

            onSelectAll: delegateModel.selectAll()
            onSelectionUpdated: delegateModel.updateSelection( keyModifiers, oldIndex, newIndex )
            onActionAtIndex: delegateModel.actionAtIndex(index)

            navigationParent: root
            navigationCancel: function() {
                history.previous(History.Go)
            }
        }
    }

    Component{
        id: listComponent
        Widgets.KeyNavigableListView {
            height: view.height
            width: view.width
            model: delegateModel.parts.list
            modelCount: delegateModel.items.count
            currentIndex: delegateModel.currentIndex

            focus: true
            spacing: VLCStyle.margin_xxxsmall

            onSelectAll: delegateModel.selectAll()
            onSelectionUpdated: delegateModel.updateSelection( keyModifiers, oldIndex, newIndex )
            onActionAtIndex: delegateModel.actionAtIndex(index)

            navigationParent: root
            navigationCancel: function() {
                history.previous(History.Go)
            }

            header:  Widgets.LabelSeparator {
                text: providerModel.name
                width: view.width
            }
        }
    }

    Widgets.StackViewExt {
        id: view
        anchors.fill:parent
        clip: true
        focus: true
        initialItem: medialib.gridView ? gridComponent : listComponent

        Connections {
            target: medialib
            onGridViewChanged: {
                if (medialib.gridView)
                    view.replace(gridComponent)
                else
                    view.replace(listComponent)
            }
        }

        Widgets.BusyIndicatorExt {
            runningDelayed: providerModel.parsingPending
            anchors.centerIn: parent
            z: 1
        }
    }
}
