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
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///util/" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root

    property var providerModel
    property var tree
    onTreeChanged: providerModel.tree = tree
    readonly property var currentIndex: view.currentItem.currentIndex
    //the index to "go to" when the view is loaded
    property var initialIndex: 0

    function changeTree(new_tree) {
        history.push(["mc", "network", { tree: new_tree }]);
    }

    Util.SelectableDelegateModel{
        id: selectionModel
        model: providerModel
    }

    NetworkMediaContextMenu {
        id: contextMenu
        model: providerModel
    }

    function resetFocus() {
        var initialIndex = root.initialIndex
        if (initialIndex >= providerModel.count)
            initialIndex = 0
        selectionModel.select(providerModel.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect)
        if (view.currentItem) {
            view.currentItem.currentIndex = initialIndex
            view.currentItem.positionViewAtIndex(initialIndex, ItemView.Contain)
        }
    }


    function _actionAtIndex(index) {
        if ( selectionModel.selectedIndexes.length > 1 ) {
            providerModel.addAndPlay( selectionModel.selectedIndexes )
        } else {
            var data = providerModel.getDataAt(index)
            if (data.type === NetworkMediaModel.TYPE_DIRECTORY
                    || data.type === NetworkMediaModel.TYPE_NODE)  {
                changeTree(data.tree)
            } else {
                providerModel.addAndPlay( selectionModel.selectedIndexes )
            }
        }
    }

    Component{
        id: gridComponent

        Widgets.ExpandGridView {
            id: gridView

            delegateModel: selectionModel
            model: providerModel

            headerDelegate: Widgets.NavigableFocusScope {
                width: view.width
                height: layout.implicitHeight + VLCStyle.margin_large + VLCStyle.margin_normal
                navigable: btn.visible

                RowLayout {
                    id: layout

                    anchors.fill: parent
                    anchors.topMargin: VLCStyle.margin_large
                    anchors.bottomMargin: VLCStyle.margin_normal
                    anchors.rightMargin: VLCStyle.margin_small

                    Widgets.SubtitleLabel {
                        text: providerModel.name
                        leftPadding: gridView.rowX

                        Layout.fillWidth: true
                    }

                    Widgets.TabButtonExt {
                        id: btn

                        focus: true
                        iconTxt: providerModel.indexed ? VLCIcons.remove : VLCIcons.add
                        text:  providerModel.indexed ?  i18n.qtr("Remove from medialibrary") : i18n.qtr("Add to medialibrary")
                        visible: !providerModel.is_on_provider_list && !!providerModel.canBeIndexed
                        onClicked: providerModel.indexed = !providerModel.indexed

                        Layout.preferredWidth: implicitWidth
                    }
                }

                Keys.onPressed: defaultKeyAction(event, 0)
                navigationParent: root
                navigationDown: function() {
                    focus = false
                    gridView.forceActiveFocus()
                }
            }

            cellWidth: VLCStyle.gridItem_network_width
            cellHeight: VLCStyle.gridCover_network_height + VLCStyle.margin_xsmall + VLCStyle.fontHeight_normal

            delegate: NetworkGridItem {
                id: delegateGrid

                property var model: ({})
                property int index: -1

                subtitle: ""
                height: VLCStyle.gridCover_network_height + VLCStyle.margin_xsmall + VLCStyle.fontHeight_normal

                onPlayClicked: selectionModel.model.addAndPlay( index )
                onItemClicked : gridView.leftClickOnItem(modifier, index)

                onItemDoubleClicked: {
                    if (model.type === NetworkMediaModel.TYPE_NODE || model.type === NetworkMediaModel.TYPE_DIRECTORY)
                        changeTree(model.tree)
                    else
                        selectionModel.model.addAndPlay( index )
                }

                onContextMenuButtonClicked: {
                    gridView.rightClickOnItem(index)
                    contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
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
        id: tableComponent

        Widgets.KeyNavigableTableView {
            id: tableView

            readonly property int _nbCols: VLCStyle.gridColumnsForWidth(tableView.availableRowWidth)
            readonly property int _nameColSpan: Math.max((_nbCols - 1) / 2, 1)
            property Component thumbnailHeader: Item {
                Widgets.IconLabel {
                    height: VLCStyle.listAlbumCover_height
                    width: VLCStyle.listAlbumCover_width
                    horizontalAlignment: Text.AlignHCenter
                    text: VLCIcons.album_cover
                    color: VLCStyle.colors.caption
                }
            }

            property Component thumbnailColumn: NetworkThumbnailItem {
                onPlayClicked: providerModel.addAndPlay(index)
            }

            height: view.height
            width: view.width
            model: providerModel
            selectionDelegateModel: selectionModel
            focus: true
            headerColor: VLCStyle.colors.bg
            navigationParent: root
            navigationUpItem: tableView.headerItem
            navigationCancel: function() {
                history.previous()
            }

            rowHeight: VLCStyle.listAlbumCover_height + VLCStyle.margin_xxsmall * 2

            header: Widgets.NavigableFocusScope {
                width: view.width
                height: layout.implicitHeight + VLCStyle.margin_large + VLCStyle.margin_small
                navigable: btn.visible

                RowLayout {
                    id: layout

                    anchors.fill: parent
                    anchors.topMargin: VLCStyle.margin_large
                    anchors.bottomMargin: VLCStyle.margin_small
                    anchors.rightMargin: VLCStyle.margin_small

                    Widgets.SubtitleLabel {
                        text: providerModel.name
                        leftPadding: VLCStyle.margin_large

                        Layout.fillWidth: true
                    }

                    Widgets.TabButtonExt {
                        id: btn

                        focus: true
                        iconTxt: providerModel.indexed ? VLCIcons.remove : VLCIcons.add
                        text:  providerModel.indexed ?  i18n.qtr("Remove from medialibrary") : i18n.qtr("Add to medialibrary")
                        visible: !providerModel.is_on_provider_list && !!providerModel.canBeIndexed
                        onClicked: providerModel.indexed = !providerModel.indexed

                        Layout.preferredWidth: implicitWidth
                    }
                }

                Keys.onPressed: defaultKeyAction(event, 0)
                navigationParent: root
                navigationUpItem: root.navigationUpItem
                navigationDown: function() {
                    focus = false
                    tableView.forceActiveFocus()
                }
            }

            sortModel: [
                { criteria: "thumbnail", width: VLCStyle.colWidth(1), headerDelegate: tableView.thumbnailHeader, colDelegate: tableView.thumbnailColumn },
                { isPrimary: true, criteria: "name", width: VLCStyle.colWidth(tableView._nameColSpan), text: i18n.qtr("Name") },
                { criteria: "mrl", width: VLCStyle.colWidth(Math.max(tableView._nbCols - tableView._nameColSpan - 1), 1), text: i18n.qtr("Url"), showContextButton: true },
            ]

            onActionForSelection: _actionAtIndex(selection[0].row)
            onContextMenuButtonClicked: contextMenu.popup(selectionModel.selectedIndexes, menuParent.mapToGlobal(0,0))
            onRightClick: contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
        }
    }

    Widgets.StackViewExt {
        id: view
        anchors.fill:parent
        clip: true
        focus: true
        initialItem: mainInterface.gridView ? gridComponent : tableComponent

        Connections {
            target: mainInterface
            onGridViewChanged: {
                if (mainInterface.gridView)
                    view.replace(gridComponent)
                else
                    view.replace(tableComponent)
            }
        }

        Widgets.BusyIndicatorExt {
            runningDelayed: providerModel.parsingPending
            anchors.centerIn: parent
            z: 1
        }
    }
}
