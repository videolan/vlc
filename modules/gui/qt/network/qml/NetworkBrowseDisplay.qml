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
import QtQuick.Layouts 1.3
import QtQml 2.11

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root

    property alias tree: providerModel.tree

    NetworkMediaModel {
        id: providerModel
        ctx: mainctx
        tree: undefined
    }

    NetworksSectionSelectableDM{
        id: delegateModelId
        model: providerModel
        onCountChanged: resetFocus()
    }

    function resetFocus() {
        if (providerModel.count > 0 && !delegateModelId.hasSelection) {
            var initialIndex = 0
            if (delegateModelId.currentIndex !== -1)
                initialIndex = delegateModelId.currentIndex
            delegateModelId.select(initialIndex, ItemSelectionModel.ClearAndSelect)
            delegateModelId.currentIndex = initialIndex
        }
    }

    Widgets.MenuExt {
        id: contextMenu
        property var delegateModelId: undefined
        property var model: ({})
        closePolicy: Popup.CloseOnReleaseOutside | Popup.CloseOnEscape
        focus:true

        Instantiator {
            id: instanciator
            property var modelActions: {
                "play": function() {
                    if (delegateModelId) {
                        providerModel.addAndPlay(delegateModelId.selectedIndexes())
                    }
                    contextMenu.close()
                },
                "enqueue": function() {
                    if (delegateModelId) {
                        providerModel.addToPlaylist(delegateModelId.selectedIndexes())
                    }
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
            id: gridView

            delegateModel: delegateModelId
            model: providerModel

            headerDelegate: Widgets.LabelSeparator {
                width: view.width
                text: providerModel.name

                navigable: inlineItem.visible

                inlineComponent: Widgets.TabButtonExt {
                    focus: true
                    iconTxt: providerModel.indexed ? VLCIcons.remove : VLCIcons.add
                    text:  providerModel.indexed ?  i18n.qtr("Remove from medialibrary") : i18n.qtr("Add to medialibrary")
                    visible: !providerModel.is_on_provider_list && providerModel.canBeIndexed
                    onClicked: providerModel.indexed = !providerModel.indexed
                }


                Keys.onPressed: defaultKeyAction(event, 0)
                navigationParent: root
                navigationDown: function() {
                    focus = false
                    gridView.forceActiveFocus()
                }
            }

            cellWidth: VLCStyle.gridItem_network_width
            cellHeight: VLCStyle.gridItem_network_height

            delegate: NetworkGridItem {
                id: delegateGrid

                property var model: ({})

                subtitle: ""

                onItemClicked : {
                    delegateModelId.updateSelection( modifier ,  delegateModelId.currentIndex, index)
                    delegateModelId.currentIndex = index
                    delegateGrid.forceActiveFocus()
                }

                onItemDoubleClicked: {
                    if (model.type === NetworkMediaModel.TYPE_NODE || model.type === NetworkMediaModel.TYPE_DIRECTORY)
                        history.push( ["mc", "network", { tree: model.tree } ])
                    else
                        delegateModelId.model.addAndPlay( index )
                }

                onContextMenuButtonClicked: {
                    contextMenu.model = model
                    contextMenu.delegateModelId = delegateModelId
                    contextMenu.popup()
                }
            }

            onSelectAll: delegateModelId.selectAll()
            onSelectionUpdated: delegateModelId.updateSelection( keyModifiers, oldIndex, newIndex )
            onActionAtIndex: delegateModelId.actionAtIndex(index)

            navigationParent: root
            navigationUpItem: gridView.headerItem
            navigationCancel: function() {
                history.previous()
            }
        }
    }

    Component{
        id: listComponent
        Widgets.KeyNavigableListView {
            id: listView
            height: view.height
            width: view.width
            model: delegateModelId.parts.list
            currentIndex: delegateModelId.currentIndex

            focus: true
            spacing: VLCStyle.margin_xxxsmall

            onSelectAll: delegateModelId.selectAll()
            onSelectionUpdated: delegateModelId.updateSelection( keyModifiers, oldIndex, newIndex )
            onActionAtIndex: delegateModelId.actionAtIndex(index)

            navigationParent: root
            navigationUpItem: listView.headerItem
            navigationCancel: function() {
                history.previous()
            }

            header:  Widgets.LabelSeparator {
                text: providerModel.name
                width: view.width

                navigable: inlineItem.visible

                inlineComponent: Widgets.TabButtonExt {
                    focus: true
                    iconTxt: providerModel.indexed ? VLCIcons.remove : VLCIcons.add
                    text:  providerModel.indexed ?  i18n.qtr("Remove from medialibrary") : i18n.qtr("Add to medialibrary")
                    visible: !providerModel.is_on_provider_list && providerModel.canBeIndexed
                    onClicked: providerModel.indexed = !providerModel.indexed
                }

                Keys.onPressed: defaultKeyAction(event, 0)
                navigationParent: root
                navigationUpItem: root.navigationUpItem
                navigationDown: function() {
                    focus = false
                    listView.forceActiveFocus()
                }
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
