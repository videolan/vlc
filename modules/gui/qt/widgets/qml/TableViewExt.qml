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

// FIXME: Maybe we could inherit from KeyNavigableListView directly.
FocusScope {
    id: root

    // Properties

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
    property int leftMargin: VLCStyle.layout_left_margin + leftPadding
    property int rightMargin: VLCStyle.layout_right_margin + rightPadding

    property int leftPadding: 0
    property int rightPadding: 0

    readonly property int extraMargin: VLCStyle.dynamicAppMargins(width)

    // NOTE: The list margins for the item(s) horizontal positioning.
    readonly property int contentLeftMargin: extraMargin + leftMargin
    readonly property int contentRightMargin: extraMargin + rightMargin

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

    readonly property int _availableSpaceForWeightedColumns: (availableRowWidth - ( _totalSpacerSize + _fixedColumnSize))
    readonly property int _weightedColumnsSize: _availableSpaceForWeightedColumns / _totalColumnWeights

    readonly property int _totalSpacerSize: VLCStyle.column_spacing * sortModel.length

    property Component header: null
    property Item headerItem: view.headerItem?.loadedHeader ?? null
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

    property real availableRowWidth: 0

    property Widgets.DragItem dragItem: null

    // Private

    property bool _ready: false

    property real _availabeRowWidthLastUpdateTime: Date.now()

    readonly property real _currentAvailableRowWidth: width - leftMargin - rightMargin
                                                      // contextButton is implemented as fixed column
                                                      - VLCStyle.contextButton_width - (VLCStyle.contextButton_margin * 2)

    // Aliases

    property alias topMargin: view.topMargin
    property alias bottomMargin: view.bottomMargin

    property alias spacing: view.spacing

    property alias model: view.model
    property alias selectionModel: view.selectionModel

    property alias delegate: view.delegate

    property alias contentY     : view.contentY
    property alias contentHeight: view.contentHeight

    property alias originX: view.originX
    property alias originY: view.originY

    property alias interactive: view.interactive

    property alias section: view.section

    property alias currentIndex: view.currentIndex
    property alias currentItem: view.currentItem

    property alias headerPositioning: view.headerPositioning

    property alias tableHeaderItem: view.headerItem

    property alias footerItem: view.footerItem
    property alias footer: view.footer

    property alias fadingEdge: view.fadingEdge

    property alias add:       view.add
    property alias displaced: view.displaced

    property alias listView: view

    property alias displayMarginBeginning: view.displayMarginBeginning
    property alias displayMarginEnd: view.displayMarginEnd

    property alias count: view.count

    property alias colorContext: view.colorContext

    property alias reuseItems: view.reuseItems

    readonly property var itemAtIndex: view.itemAtIndex

    // Signals

    //forwarded from subview
    signal actionForSelection( var selection )
    signal rightClick(Item menuParent, var menuModel, point globalMousePos)
    signal itemDoubleClicked(var index, var model)

    // Events

    Component.onCompleted: {
        _ready = true

        availableRowWidthUpdater.enqueueUpdate()
    }

    on_CurrentAvailableRowWidthChanged: if (_ready) availableRowWidthUpdater.enqueueUpdate()

    // Functions

    function setCurrentItem(index) {
        view.setCurrentItem(index)
    }

    function setCurrentItemFocus(reason) {
        view.setCurrentItemFocus(reason);
    }

    function positionViewAtIndex(index, mode) {
        view.positionViewAtIndex(index, mode)
    }

    function positionViewAtBeginning() {
        view.positionViewAtBeginning()
    }

    function getItemY(index) {
        let size = index * rowHeight + topMargin

        if (tableHeaderItem)
            size += tableHeaderItem.height

        return size
    }

    // Private

    // Childs

    Timer {
        id: availableRowWidthUpdater

        interval: 100
        triggeredOnStart: false
        repeat: false
        onTriggered: {
            _update()
        }

        function _update() {
            root.availableRowWidth = root._currentAvailableRowWidth
            root._availabeRowWidthLastUpdateTime = Date.now()
        }

        function enqueueUpdate() {
            // updating availableRowWidth is expensive because of property bindings in sortModel
            // and availableRowWidth is dependent on root.width which can update in a burst
            // so try to maintain a minimum time gap between subsequent availableRowWidth updates
            const sinceLastUpdate = Date.now() - root._availabeRowWidthLastUpdateTime
            if ((root.availableRowWidth === 0) || (sinceLastUpdate > 128 && !availableRowWidthUpdater.running)) {
                _update()
            } else if (!availableRowWidthUpdater.running) {
                availableRowWidthUpdater.interval = Math.max(128 - sinceLastUpdate, 32)
                availableRowWidthUpdater.start()
            }
        }
    }

    ListViewExt {
        id: view

        anchors.fill: parent
        focus: true

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

            width: view.width
            height: col.height
            z: 3
            color: root.headerColor

            // with inline header positioning and for `root.header` which changes it's height after loading,
            // in such cases after `root.header` completes, the ListView will try to maintain the relative contentY,
            // and hide the completed `root.header`, try to show the `root.header` in such cases by manually
            // positiing view at beginning
            onHeightChanged: if (root.contentY < 0) root.positionViewAtBeginning()

            Widgets.ListLabel {
                // NOTE: We want the section label to be slightly shifted to the left.
                x: row.x - VLCStyle.margin_small
                y: row.y + root.headerTopPadding

                height: VLCStyle.tableHeaderText_height
                verticalAlignment: Text.AlignVCenter

                text: view.currentSection
                color: view.colorContext.accent
                visible: view.headerPositioning === ListView.OverlayHeader
                         && text !== ""
                         && view.contentY > (row.height - col.height - row.topPadding)
                         && row.visible
            }

            Column {
                id: col

                anchors.left: parent.left
                anchors.right: parent.right

                Loader {
                    id: headerLoader

                    sourceComponent: root.header
                }

                Row {
                    id: row

                    anchors.left: parent.left
                    anchors.right: parent.right

                    anchors.leftMargin: root.contentLeftMargin
                    anchors.rightMargin: root.contentRightMargin

                    topPadding: root.headerTopPadding
                    bottomPadding: VLCStyle.margin_xsmall

                    spacing: VLCStyle.column_spacing

                    // If there is a specific header, obey to its visibility otherwise hide the header if model is empty:
                    visible: headerLoader.item ? headerLoader.item.visible : (view.count > 0)

                    Repeater {
                        model: sortModel
                        Item {
                            id: headerCell

                            required property var modelData
                            property TableHeaderDelegate _item: null

                            TableHeaderDelegate.CellModel {
                                id: cellModel
                                colorContext:  view.colorContext
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
            leftPadding: root.contentLeftMargin - VLCStyle.margin_small

            topPadding: VLCStyle.margin_xsmall

            text: section
            color: root.colorContext.accent
        }

        delegate: Widgets.TableViewDelegateExt {
            id: tableDelegate

            required property var model

            width: view.width
            height: Math.round(root.rowHeight)

            fixedColumnWidth: root.baseColumnWidth
            weightedColumnWidth: root._weightedColumnsSize

            leftPadding: root.contentLeftMargin
            rightPadding: root.contentRightMargin

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

            isDropAcceptable: view.isDropAcceptableFunc
            acceptDrop: view.acceptDropFunc

            onSelectAndFocus: (modifiers, focusReason) => {
                selectionModel.updateSelection(modifiers, view.currentIndex, index)

                view.currentIndex = index
                view.positionViewAtIndex(index, ListView.Contain)

                tableDelegate.forceActiveFocus(focusReason)
            }

            onContainsDragChanged: view.updateItemContainsDrag(this, containsDrag)

            Connections {
                target: selectionModel

                function onSelectionChanged() {
                    tableDelegate.selected = Qt.binding(function() {
                      return root.selectionModel.selectedIndexesFlat.includes(index)
                    })
                }
            }
        }
    }
}
