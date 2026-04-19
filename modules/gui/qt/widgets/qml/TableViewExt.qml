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
import QtQuick
import QtQuick.Controls
import QtQml.Models
import QtQuick.Layouts

import VLC.MainInterface
import VLC.Util
import VLC.Widgets as Widgets
import VLC.Style

ListViewExt {
    id: root

    property var sortModel: []

    property Component tableHeaderDelegate:  TableHeaderDelegate {
        Widgets.CaptionLabel {
            horizontalAlignment: colModel.hCenterText ? Text.AlignHCenter : Text.AlignLeft
            verticalAlignment: Text.AlignVCenter

            anchors.fill: parent

            Accessible.ignored: true

            text: parent.colModel.text ?? ""
            color: parent.colorContext.fg.secondary
        }
    }

    /*
        Expects an object with the following properties:

        // Determines if the menu is visible (optional)
        bool visible

        // The request ID associated with the currently displayed context menu (optional)
        int currentRequest

        // Displays the popup menu at the specified position (required)
        // incase the function doesn't return request id TableViewExt will
        // try to mimic behavior but it may have some caveats
        //
        // Parameters:
        //    current    - calling row index
        //    selectedIndexes - current item selection (all)
        //    globalPos  - the global position where the popup should appear
        //
        // Returns:
        //    int - the generated request ID
        function tableView_popup(current, selectedIndexes, globalPos) -> int (request ID)
    */
    required property var rowContextMenu

    // NOTE: We want edge to edge backgrounds in our delegate and header, so we implement our own
    //       margins implementation like in ExpandGridView. The default values should be the same
    //       than ExpandGridView to respect the grid parti pris.

    property int leftPadding: 0
    property int rightPadding: 0

    readonly property real _extraPadding: VLCStyle.dynamicAppMargins(width)
    property real contentLeftPadding: _extraPadding + leftPadding + VLCStyle.layout_left_margin
    property real contentRightPadding: _extraPadding + rightPadding + VLCStyle.layout_right_margin

    // These are "extra" margins because `ListView`'s own `margins` is also considered for the header.
    property real defaultHeaderExtraLeftMargin: contentLeftPadding
    property real defaultHeaderExtraRightMargin: contentRightPadding

    // FIXME: Get rid of `contentLeftMargin` and `contentRightMargin`:
    // NOTE: The list margins for the item(s) horizontal positioning.
    readonly property int contentLeftMargin: contentLeftPadding
    readonly property int contentRightMargin: contentRightPadding

    property real baseColumnWidth: VLCStyle.column_width

    readonly property int _fixedColumnSize: {
       let size = 0
       for (let i in sortModel) {
           size += sortModel[i].size ?? 0
       }
       return size * baseColumnWidth
    }

    readonly property int _totalColumnWeights: {
         let count = 0

         for (let i in sortModel) {
             count += sortModel[i].weight ?? 0
         }
         return count
      }

    readonly property int _availableSpaceForWeightedColumns: (_availableRowWidth - ( _totalSpacerSize + _fixedColumnSize))
    readonly property int _weightedColumnsSize: _availableSpaceForWeightedColumns / _totalColumnWeights

    readonly property int _totalSpacerSize: VLCStyle.column_spacing * Math.max(0, (sortModel.length - 1))

    property Component preferredHeader
    readonly property Item preferredHeaderItem: headerItem?.loadedHeader ?? null

    // NOTE: Clipping is not used, so if header is not inline, it should have background to not expose the content.
    // TODO: Investigate using clipping here, which makes more sense in general for views. Not using clipping
    //       makes this view not suitable for using it in auxiliary places (non-main, which is not naturally
    //       clipped by the window). In order to make use of clipping, the header should be placed outside of
    //       the view (similar to play queue). This is already a requirement to prevent events reaching to
    //       the delegate (such as, hovering over the header). As `clip: true` adjusts both the scene graph
    //       viewport, and qt quick event delivery agent. Having a control as header and blocking the events
    //       events reaching beneath is not a good behavior, as currently done here.
    property color headerColor: (interactive && (headerPositioning !== ListView.InlineHeader)) ? colorContext.bg.primary : "transparent"
    property int headerTopPadding: 0

    property real rowHeight: VLCStyle.tableRow_height

    property real _availableRowWidth: 0

    // FIXME: Layouting should not be done asynchronously, investigate if getting rid of this is feasible.
    Binding on _availableRowWidth {
        delayed: true
        value: root._currentAvailableRowWidth
    }

    property Widgets.DragItem dragItem: null

    readonly property real _currentAvailableRowWidth: width - leftMargin - contentLeftPadding - rightMargin - contentRightPadding
                                                      // contextButton is implemented as fixed column
                                                      - VLCStyle.contextButton_width - (VLCStyle.contextButton_margin * 2)

    property bool sortingFromHeader: true
    property bool useCurrentSectionLabel: true

    signal actionForSelection( var selection )
    signal rightClick(Item menuParent, var menuModel, point globalMousePos)
    signal itemDoubleClicked(var index, var model)

    Component.onCompleted: {
        // This is a remnant from the time `TableViewExt` derived from `FocusScope`:
        MainCtx.setItemFlag(this, Item.ItemIsFocusScope)
    }

    function getItemY(index) {
        let size = index * rowHeight + topMargin

        if (headerItem)
            size += headerItem.height

        return size
    }

    headerPositioning: ListView.OverlayHeader

    flickableDirection: Flickable.AutoFlickDirection

    Navigation.parentItem: root

    onActionAtIndex: (index) => { root.actionForSelection( selectionModel.selectedIndexes ) }

    onShowContextMenu: (globalPos) => {
        if (selectionModel.hasSelection)
            root.rightClick(null, null, globalPos);
    }

    header: Rectangle {
        property alias loadedHeader: headerLoader.item

        width: root.width
        height: col.height
        z: 3
        color: root.headerColor

        // with inline header positioning and for `root.header` which changes it's height after loading,
        // in such cases after `root.header` completes, the ListView will try to maintain the relative contentY,
        // and hide the completed `root.header`, try to show the `root.header` in such cases by manually
        // positiing view at beginning
        onHeightChanged: if (root.contentY < 0) root.positionViewAtBeginning()

        Navigation.parentItem: root
        Navigation.upItem: loadedHeader
        Navigation.downItem: loadedHeader
        Navigation.leftItem: loadedHeader
        Navigation.rightItem: loadedHeader
        Navigation.navigable: false

        readonly property var setCurrentItemFocus: loadedHeader?.setCurrentItemFocus

        Widgets.ListLabel {
            // NOTE: We want the section label to be slightly shifted to the left.
            x: row.x - VLCStyle.margin_small
            y: row.y + root.headerTopPadding

            height: VLCStyle.tableHeaderText_height
            verticalAlignment: Text.AlignVCenter

            text: root.currentSection
            color: root.colorContext.accent
            visible: root.useCurrentSectionLabel
                     && root.headerPositioning === ListView.OverlayHeader
                     && text !== ""
                     && root.contentY > (row.height - col.height - row.topPadding)
                     && row.visible
        }

        Column {
            id: col

            anchors.left: parent.left
            anchors.right: parent.right

            Loader {
                id: headerLoader

                sourceComponent: root.preferredHeader
            }

            Row {
                id: row

                anchors.left: parent.left
                anchors.right: parent.right

                anchors.leftMargin: root.defaultHeaderExtraLeftMargin
                anchors.rightMargin: root.defaultHeaderExtraRightMargin

                topPadding: root.headerTopPadding
                bottomPadding: VLCStyle.margin_xsmall

                spacing: VLCStyle.column_spacing

                // If there is a specific header, obey to its visibility otherwise hide the header if model is empty:
                visible: headerLoader.item ? headerLoader.item.visible : (root.count > 0)

                Repeater {
                    model: sortModel
                    Item {
                        id: headerCell

                        required property var modelData
                        property TableHeaderDelegate _item: null

                        TableHeaderDelegate.CellModel {
                            id: cellModel
                            colorContext:  root.colorContext
                            colModel: modelData.model
                        }

                        height: VLCStyle.tableHeaderText_height
                        width: {
                            if (!!modelData.size)
                                return modelData.size * root.baseColumnWidth
                            else if (!!modelData.weight)
                                return modelData.weight * root._weightedColumnsSize
                            else
                                return 0
                        }
                        Accessible.role: Accessible.ColumnHeader
                        Accessible.name: modelData.model.text

                        //Using a Loader is unable to pass the initial/required properties
                        Component.onCompleted: {
                            const comp = modelData.model.headerDelegate || root.tableHeaderDelegate
                            headerCell._item = comp.createObject(headerCell, {
                                width:  Qt.binding(() => headerCell.width),
                                height:  Qt.binding(() => headerCell.height),
                                cellModel: cellModel,
                            })
                        }

                        Text {
                            text: (root.model.sortOrder === Qt.AscendingOrder) ? "▼" : "▲"
                            visible: root.model.sortCriteria === modelData.model.criteria
                            font.pixelSize: VLCStyle.fontSize_normal
                            color: root.colorContext.accent

                            anchors {
                                top: parent.top
                                bottom: parent.bottom
                                right: parent.right
                                leftMargin: VLCStyle.margin_xsmall
                                rightMargin: VLCStyle.margin_xsmall
                            }
                        }

                        TapHandler {
                            onTapped: (eventPoint, button) => {
                                if (!root.sortingFromHeader)
                                    return
                                if (!(modelData.model.isSortable ?? true))
                                    return
                                else if (root.model.sortCriteria !== modelData.model.criteria)
                                    root.model.sortCriteria = modelData.model.criteria
                                else
                                    root.model.sortOrder = (root.model.sortOrder === Qt.AscendingOrder) ? Qt.DescendingOrder : Qt.AscendingOrder
                            }
                        }
                    }
                }

                Item {
                    // placeholder for context button

                    width: VLCStyle.icon_normal

                    height: 1
                }
            }
        }
    }

    section.delegate: Widgets.ListLabel {
        // NOTE: We want the section label to be slightly shifted to the left.
        leftPadding: root.contentLeftPadding - VLCStyle.margin_small

        topPadding: VLCStyle.margin_xsmall

        text: section
        color: root.colorContext.accent
    }

    delegate: Widgets.TableViewDelegateExt {
        id: tableDelegate

        required property var model

        width: root.width
        height: Math.round(root.rowHeight)

        fixedColumnWidth: root.baseColumnWidth
        weightedColumnWidth: root._weightedColumnsSize

        leftPadding: root.contentLeftPadding
        rightPadding: root.contentRightPadding

        dragItem: root.dragItem

        contextMenu: root.rowContextMenu

        rowModel: model
        sortModel: root.sortModel

        selected: selectionModel.selectedIndexesFlat.includes(index)

        onRightClick: (menuParent, menuModel, globalMousePos) => {
            root.rightClick(menuParent, menuModel, globalMousePos)
        }
        onItemDoubleClicked: (index, model) => {
            root.itemDoubleClicked(index, model)
        }

        isDropAcceptable: root.isDropAcceptableFunc
        acceptDrop: root.acceptDropFunc

        onSelectAndFocus: (modifiers, focusReason) => {
            selectionModel.updateSelection(modifiers, root.currentIndex, index)

            root.currentIndex = index
            root.positionViewAtIndex(index, ListView.Contain)

            tableDelegate.forceActiveFocus(focusReason)
        }

        onContainsDragChanged: root.updateItemContainsDrag(this, containsDrag)
    }
}
