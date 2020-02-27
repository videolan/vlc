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

import "qrc:///util/" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root

    property alias tree: providerModel.tree
    readonly property var currentIndex: view.currentItem.currentIndex
    //the index to "go to" when the view is loaded
    property var initialIndex: 0

    NetworkMediaModel {
        id: providerModel
        ctx: mainctx
        tree: undefined
        onCountChanged: resetFocus()
    }

    Util.SelectableDelegateModel{
        id: selectionModel
        model: providerModel
    }

    function resetFocus() {
        var initialIndex = root.initialIndex
        if (initialIndex >= providerModel.count)
            initialIndex = 0
        selectionModel.select(providerModel.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect)
        view.currentItem.currentIndex = initialIndex
        view.currentItem.positionViewAtIndex(initialIndex, ItemView.Contain)
    }


    function _actionAtIndex(index) {
        if ( selectionModel.selectedIndexes.length > 1 ) {
            providerModel.addAndPlay( selectionModel.selectedIndexes )
        } else {
            var data = providerModel.getDataAt(index)
            if (data.type === NetworkMediaModel.TYPE_DIRECTORY
                    || data.type === NetworkMediaModel.TYPE_NODE)  {
                history.push(["mc", "network", { tree: data.tree }]);
            } else {
                providerModel.addAndPlay( selectionModel.selectedIndexes )
            }
        }
    }

    Widgets.MenuExt {
        id: contextMenu
        property var selectionModel: undefined
        property var model: ({})
        closePolicy: Popup.CloseOnReleaseOutside | Popup.CloseOnEscape
        focus:true

        Instantiator {
            id: instanciator
            property var modelActions: {
                "play": function() {
                    if (selectionModel) {
                        providerModel.addAndPlay(selectionModel.selectedIndexes )
                    }
                    contextMenu.close()
                },
                "enqueue": function() {
                    if (selectionModel) {
                        providerModel.addToPlaylist(selectionModel.selectedIndexes )
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

            delegateModel: selectionModel
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
                    selectionModel.updateSelection( modifier ,  view.currentItem.currentIndex, index)
                    view.currentItem.currentIndex = index
                    delegateGrid.forceActiveFocus()
                }

                onItemDoubleClicked: {
                    if (model.type === NetworkMediaModel.TYPE_NODE || model.type === NetworkMediaModel.TYPE_DIRECTORY)
                        history.push( ["mc", "network", { tree: model.tree } ])
                    else
                        selectionModel.model.addAndPlay( index )
                }

                onContextMenuButtonClicked: {
                    contextMenu.model = providerModel
                    contextMenu.selectionModel = selectionModel
                    contextMenu.popup()
                }
            }

            onSelectAll: selectionModel.selectAll()
            onSelectionUpdated: selectionModel.updateSelection( keyModifiers, oldIndex, newIndex )
            onActionAtIndex: _actionAtIndex(index)

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
            model: providerModel

            delegate: NetworkListItem {
                id: delegateList
                focus: true

                selected: selectionModel.isSelected( providerModel.index(index, 0) )
                Connections {
                    target: selectionModel
                    onSelectionChanged: delegateList.selected = selectionModel.isSelected(providerModel.index(index, 0))
                }

                onItemClicked : {
                    selectionModel.updateSelection( modifier, view.currentItem.currentIndex, index )
                    view.currentItem.currentIndex = index
                    delegateList.forceActiveFocus()
                }

                onItemDoubleClicked: {
                    if (model.type === NetworkMediaModel.TYPE_NODE || model.type === NetworkMediaModel.TYPE_DIRECTORY)
                        history.push( ["mc", "network", { tree: model.tree } ])
                    else
                        providerModel.addAndPlay( index )
                }

                onContextMenuButtonClicked: {
                    contextMenu.model = providerModel
                    contextMenu.selectionModel = selectionModel
                    contextMenu.popup(menuParent)
                }

                onActionLeft: root.navigationLeft(0)
                onActionRight: root.navigationRight(0)
            }

            focus: true
            spacing: VLCStyle.margin_xxxsmall

            onSelectAll: selectionModel.selectAll()
            onSelectionUpdated: selectionModel.updateSelection( keyModifiers, oldIndex, newIndex )
            onActionAtIndex: _actionAtIndex(index)

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
