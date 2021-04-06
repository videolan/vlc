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
import "qrc:///util/KeyHelper.js" as KeyHelper
import "qrc:///style/"

NavigableFocusScope {
    id: root

    /// cell Width
    property int cellWidth: 100
    // cell Height
    property int cellHeight: 100

    //margin to apply
    property int bottomMargin: 0
    property int topMargin: 0
    property int leftMargin: VLCStyle.margin_normal
    property int rightMargin: VLCStyle.margin_normal

    property int rowX: 0
    property int horizontalSpacing: VLCStyle.column_margin_width
    property int verticalSpacing: VLCStyle.column_margin_width

    readonly property int _effectiveCellWidth: cellWidth + horizontalSpacing
    readonly property int _effectiveCellHeight: cellHeight + verticalSpacing

    property var delegateModel
    property var model

    property int currentIndex: 0
    property alias contentHeight: flickable.contentHeight
    property alias contentWidth: flickable.contentWidth
    property alias contentX: flickable.contentX
    property alias gridScrollBar: flickableScrollBar
    property bool isAnimating: animateRetractItem.running || animateExpandItem.running

    property int _count: 0

    property bool _isInitialised: false

    /// the id of the item to be expanded
    property int expandIndex: -1
    property int _newExpandIndex: -1
    property int _expandItemVerticalSpace: 0
    on_ExpandItemVerticalSpaceChanged: {
        if (expandItem) {
            expandItem.visible = root._expandItemVerticalSpace - root.verticalSpacing > 0
            expandItem.height = Math.max(root._expandItemVerticalSpace - root.verticalSpacing, 0)
        }
        flickable.layout(true)
    }

    //delegate to display the extended item
    property Component delegate: Item{}
    property Component expandDelegate: Item{}
    property alias expandItem: expandItemLoader.item

    property Component headerDelegate: Item{}
    property alias headerHeight: headerItemLoader.implicitHeight
    property alias headerItem: headerItemLoader.item

    property alias footerItem: footerItemLoader.item
    property alias footerDelegate: footerItemLoader.sourceComponent

    //signals emitted when selected items is updated from keyboard
    signal selectionUpdated( int keyModifiers, int oldIndex,int newIndex )
    signal selectAll()
    signal actionAtIndex(int index)

    property var _idChildrenMap: ({})
    property var _unusedItemList: []
    property var _currentRange: [0,0]

    Accessible.role: Accessible.Table

    function setCurrentItemFocus() {
        if (!model || model.count === 0 || currentIndex === -1)
            return
        positionViewAtIndex(currentIndex, ItemView.Contain)
        flickable.setCurrentItemFocus()
    }

    function switchExpandItem(index) {
        if (_count === 0)
            return

        if (index === expandIndex)
            _newExpandIndex = -1
        else
            _newExpandIndex = index

        if (expandIndex !== -1)
            flickable.retract()
        else
            flickable.expand()
    }

    function retract() {
        _newExpandIndex = -1
        flickable.retract()
    }

    function getNbItemsPerRow() {
        return Math.max(Math.floor(((width - root.rightMargin - root.leftMargin) + root.horizontalSpacing) / root._effectiveCellWidth), 1)
    }

    function getItemRowCol(id) {
        var nbItemsPerRow = getNbItemsPerRow()
        var rowId = Math.floor(id / nbItemsPerRow)
        var colId = id % nbItemsPerRow
        return [colId, rowId]
    }

    function getItemPos(id) {
        var colCount = root.getNbItemsPerRow()
        var remainingSpace = (flickable.width - root.rightMargin - root.leftMargin) - (colCount * root._effectiveCellWidth) + root.horizontalSpacing
        var rowCol = getItemRowCol(id)
        return [(rowCol[0] * root._effectiveCellWidth) + (remainingSpace / 2) + root.leftMargin, rowCol[1] * root._effectiveCellHeight + headerHeight + topMargin]
    }

    //use the same signature as Gridview.positionViewAtIndex(index, PositionMode mode)
    //mode is ignored at the moment
    function positionViewAtIndex(index, mode) {
        if (flickable.width === 0 || flickable.height === 0)
            return

        if (index <= 0) {
            animateFlickableContentY(0)
            return
        } else if (index >= _count) {
            return
        }

        var newContentY = flickable.contentY

        var itemTopY = root.getItemPos(index)[1]
        var itemBottomY = itemTopY + root._effectiveCellHeight

        var viewTopY = flickable.contentY
        var viewBottomY = viewTopY + flickable.height

        if (index < getNbItemsPerRow()) {
            //force to see the header when on the first row
            newContentY = 0
        } else if ( itemTopY < viewTopY ) {
            //item above view
            newContentY = itemTopY - topMargin
        } else if (itemBottomY > viewBottomY) {
            //item below view
            newContentY = itemBottomY + bottomMargin - flickable.height
        }

        if (newContentY !== flickable.contentY)
            animateFlickableContentY(newContentY)
    }

    function leftClickOnItem(modifier, index) {
        delegateModel.updateSelection( modifier , currentIndex, index)
        currentIndex = index
        root.forceActiveFocus()
    }

    function rightClickOnItem(index) {
        if (!delegateModel.isSelected(model.index(index, 0))) {
            root.leftClickOnItem(Qt.NoModifier, index)
        }
    }

    function _initialize() {
        if (root._isInitialised)
            return;

        if (flickable.width === 0 || flickable.height === 0)
            return;
        if (currentIndex !== 0)
            positionViewAtIndex(currentIndex, ItemView.Contain)
        root._isInitialised = true;
    }

    function _calculateCurrentRange() {
        var myContentY = flickable.contentY - root.headerHeight - topMargin

        var contentYWithoutExpand = myContentY
        var heightWithoutExpand = flickable.height
        if (root.expandIndex !== -1) {
            if (myContentY >= expandItem.y && myContentY < expandItem.y + _expandItemVerticalSpace)
                contentYWithoutExpand = expandItem.y
            if (myContentY >= expandItem.y + _expandItemVerticalSpace)
                contentYWithoutExpand = myContentY - _expandItemVerticalSpace

            var expandYStart = Math.max(myContentY, expandItem.y)
            var expandYEnd = Math.min(myContentY + height, expandItem.y + _expandItemVerticalSpace)
            var expandDisplayedHeight = Math.max(expandYEnd - expandYStart, 0)
            heightWithoutExpand -= expandDisplayedHeight
        }

        var rowId = Math.floor(contentYWithoutExpand / root._effectiveCellHeight)
        var firstId = Math.max(rowId * root.getNbItemsPerRow(), 0)


        rowId = Math.ceil((contentYWithoutExpand + heightWithoutExpand) / root._effectiveCellHeight)
        var lastId = Math.min(rowId * root.getNbItemsPerRow(), _count)

        return [firstId, lastId]
    }

    function _repositionItem(id, x, y) {
        var item = _idChildrenMap[id]
        if (item === undefined)
            throw "wrong child: " + id

        //theses properties are always defined in Item
        item.focus = (id === root.currentIndex) && (root.expandIndex === -1)
        item.x = x
        item.y = y
        item.selected = delegateModel.isSelected(model.index(id, 0))
    }

    function _recycleItem(id, x, y) {
        var item = _unusedItemList.pop()
        if (item === undefined)
            throw "wrong toRecycle child " + id + ", len " + toUse.length

        item.index = id
        item.model = model.getDataAt(id)
        item.selected = delegateModel.isSelected(model.index(id, 0))
        item.focus = (id === root.currentIndex) && (root.expandIndex === -1)
        item.x = x
        item.y = y
        item.visible = true

        _idChildrenMap[id] = item
    }

    function _createItem(id, x, y) {
        var item = root.delegate.createObject( flickable.contentItem, {
                        selected: delegateModel.isSelected(model.index(id, 0)),
                        index: id,
                        model: model.getDataAt(id),
                        focus: (id === root.currentIndex) && (root.expandIndex === -1),
                        x: x,
                        y: y,
                        visible: true
                    });
        if (item === undefined)
            throw "wrong unable to instantiate child " + id
        _idChildrenMap[id] = item
    }

    function _setupChild(id, ydelta) {
        var pos = root.getItemPos(id)

        if (id in _idChildrenMap) {
            _repositionItem(id, pos[0], pos[1] + ydelta)
        }  else if (_unusedItemList.length > 0) {
            _recycleItem(id, pos[0], pos[1] + ydelta)
        } else {
            _createItem(id, pos[0], pos[1] + ydelta)
        }
    }

    function _refreshData( iMin, iMax ) {
        var f_l = root._currentRange
        if (!iMin || iMin < f_l[0])
            iMin = f_l[0]
        if (!iMax || iMax > f_l[1])
            iMax= f_l[1]

        for (var id  = iMin; id < iMax; id++) {
            var item = _idChildrenMap[id]
            item.model = model.getDataAt(id)
        }
    }

    function _onModelCountChanged() {
        _count = model ? model.rowCount() : 0
        if (!root._isInitialised)
            return

        // Hide the expandItem with no animation
        expandIndex = -1
        _expandItemVerticalSpace = 0

        // Regenerate the gridview layout
        flickable.layout(true)
        _refreshData()
    }

    Connections {
        target: model
        onDataChanged: {
            var iMin = topLeft.row
            var iMax = bottomRight.row + 1 // [] => [)
            var f_l = root._currentRange
            if (iMin < f_l[1] && f_l[0] < iMax) {
                _refreshData(iMin, iMax)
            }
        }
        onRowsInserted: _onModelCountChanged()
        onRowsRemoved: _onModelCountChanged()
        onModelReset: _onModelCountChanged()
    }

    Connections {
        target: delegateModel

        onSelectionChanged: {
            var i
            for (i = 0; i < selected.length; ++i) {
                _updateSelectedRange(selected[i].topLeft, selected[i].bottomRight, true)
            }

            for (i = 0; i < deselected.length; ++i) {
                _updateSelectedRange(deselected[i].topLeft, deselected[i].bottomRight, false)
            }
        }

        function _updateSelectedRange(topLeft, bottomRight, select) {
            var iMin = topLeft.row
            var iMax = bottomRight.row + 1 // [] => [)
            if (iMin < root._currentRange[1] && root._currentRange[0] < iMax) {
                iMin = Math.max(iMin, root._currentRange[0])
                iMax = Math.min(iMax, root._currentRange[1])
                for (var j = iMin; j < iMax; j++) {
                    var item = root._idChildrenMap[j]
                    console.assert(item)
                    item.selected = select
                }
            }
        }
    }

    onModelChanged: _onModelCountChanged()

    Connections {
        target: mainInterface
        onIntfScaleFactorChanged: flickable.layout(true)
    }

    //Gridview visible above the expanded item
    Flickable {
        id: flickable

        clip: true

        flickableDirection: Flickable.VerticalFlick
        ScrollBar.vertical: ScrollBar {
            id: flickableScrollBar
        }

        Loader {
            id: headerItemLoader
            //load the header early (when the first row is visible)
            visible: flickable.contentY < (root.headerHeight + root._effectiveCellHeight + root.topMargin)
            sourceComponent: headerDelegate
            focus: item.focus
            onFocusChanged: {
                if (focus) {
                    //when we gain the focus ensure the widget is fully visible
                    animateFlickableContentY(0)
                } else {
                    //when we lose the focus restore the focus on the current grid item
                    flickable.setCurrentItemFocus()
                }
            }
            onLoaded: {
                item.x = 0
                item.y = root.topMargin
            }
        }

        Loader {
            id: footerItemLoader
            focus: item.focus
            y: root.topMargin + root.headerHeight + (root._effectiveCellHeight * (Math.ceil(model.count / getNbItemsPerRow()))) +
               _expandItemVerticalSpace
        }

        Connections {
            target: headerItemLoader
            onHeightChanged: {
                flickable.layout(true)
            }
        }

        Connections {
            target: footerItem
            onHeightChanged: {
                if (flickable.contentY + flickable.height > footerItemLoader.y + footerItemLoader.height)
                    flickable.contentY = footerItemLoader.y + footerItemLoader.height - flickable.height
                flickable.layout(false)
            }
        }

        Loader {
            id: expandItemLoader
            sourceComponent: expandDelegate
            active: root.expandIndex !== -1
            focus: active
            onLoaded: item.height = 0
        }


        anchors.fill: parent
        onWidthChanged: { layout(true) }
        onHeightChanged: { layout(false) }
        onContentYChanged: { layout(false) }

        function getExpandItemGridId() {
            var ret
            if (root.expandIndex !== -1) {
                var rowCol = root.getItemRowCol(root.expandIndex)
                var rowId = rowCol[1] + 1
                ret = rowId * root.getNbItemsPerRow()
            } else {
                ret = _count
            }
            return ret
        }

        function _setupIndexes(force, range, yDelta) {
            for (var i = range[0]; i < range[1]; i++) {
                if (!force && i in _idChildrenMap)
                    continue
                _setupChild(i, yDelta)
            }
        }

        function _updateChildrenMap(first, last) {
            var toKeep = {}

            for (var id in _idChildrenMap) {
                var val = _idChildrenMap[id]

                if (id >= first && id < last) {
                    toKeep[id] = val
                } else {
                    _unusedItemList.push(val)
                    val.visible = false
                }
            }
            _idChildrenMap = toKeep
        }

        function layout(forceRelayout) {
            if (flickable.width === 0 || flickable.height === 0)
                return
            else if (!root._isInitialised)
                root._initialize()

            root.rowX = getItemPos(0)[0]

            var i
            var expandItemGridId = getExpandItemGridId()

            var f_l = _calculateCurrentRange()
            var nbItems = f_l[1] - f_l[0]
            var firstId = f_l[0]
            var lastId = f_l[1]

            var topGridEndId = Math.max(Math.min(expandItemGridId, lastId), firstId)

            if (!forceRelayout && root._currentRange[0] === firstId && root._currentRange[1] === lastId)
                return;

            _updateChildrenMap(firstId, lastId)
            root._currentRange = [firstId, lastId]

            var item
            var pos
            // Place the delegates before the expandItem
            _setupIndexes(forceRelayout, [firstId, topGridEndId], 0)

            if (root.expandIndex !== -1) {
                var expandItemPos = root.getItemPos(expandItemGridId)
                expandItem.y = expandItemPos[1]
            }

            // Place the delegates after the expandItem
            _setupIndexes(forceRelayout, [topGridEndId, lastId], _expandItemVerticalSpace)

            // Calculate and set the contentHeight
            var newContentHeight = root.getItemPos(_count - 1)[1] + root._effectiveCellHeight + _expandItemVerticalSpace
            contentHeight = newContentHeight + root.bottomMargin // topMargin is included from root.getItemPos
            contentHeight += footerItemLoader.item ? footerItemLoader.item.height : 0
            contentWidth = root._effectiveCellWidth * root.getNbItemsPerRow() - root.horizontalSpacing
        }

        Connections {
            target: expandItem
            onImplicitHeightChanged: {
                /* This is the only event we have after the expandItem height content was resized.
                   We can trigger here the expand animation with the right final height. */
                if (root.expandIndex !== -1)
                    flickable.expandAnimation()
            }
        }

        function expand() {
            expandIndex = _newExpandIndex
            if (expandIndex === -1)
                return
            expandItem.model = model.getDataAt(expandIndex)
            /* We must also start the expand animation here since the expandItem implicitHeight is not
               changed if it had the same height at previous opening. */
            expandAnimation()
        }

        function expandAnimation() {
            if (expandIndex === -1)
                return

            var expandItemHeight = expandItem.implicitHeight + root.verticalSpacing

            // Expand animation

            expandItem.focus = true
            // The animation may have already been triggered, we must stop it.
            animateExpandItem.stop()
            animateExpandItem.from = root._expandItemVerticalSpace
            animateExpandItem.to = expandItemHeight
            animateExpandItem.start()

            // Sliding animation
            var currentItemYPos = root.getItemPos(expandIndex)[1]
            currentItemYPos += root._effectiveCellHeight / 2
            animateFlickableContentY(currentItemYPos)
        }

        function retract() {
            animateRetractItem.start()
        }

        NumberAnimation {
            id: animateRetractItem;
            target: root;
            properties: "_expandItemVerticalSpace"
            easing.type: Easing.OutQuad
            duration: 250
            to: 0
            onStopped: {
                expandIndex = -1
                flickable.setCurrentItemFocus()
                root.positionViewAtIndex(root.currentIndex, ItemView.Contain)
                if (_newExpandIndex !== -1)
                    flickable.expand()
            }
        }

        NumberAnimation {
            id: animateExpandItem;
            target: root;
            properties: "_expandItemVerticalSpace"
            easing.type: Easing.InQuad
            duration: 250
            from: 0
        }

        function setCurrentItemFocus() {
            if (expandIndex !== -1)
                return
            var child
            if (currentIndex in _idChildrenMap)
                child = _idChildrenMap[currentIndex]
            if (child !== undefined)
                child.focus = true
        }
    }

    PropertyAnimation {
        id: animateContentY;
        target: flickable;
        properties: "contentY"
    }

    function animateFlickableContentY( newContentY ) {
        animateContentY.stop()
        animateContentY.duration = 250
        animateContentY.to = newContentY
        animateContentY.start()
    }

    onCurrentIndexChanged: {
        if (expandIndex !== -1)
            retract()
        flickable.setCurrentItemFocus()
        positionViewAtIndex(root.currentIndex, ItemView.Contain)
    }

    Keys.onPressed: {
        var colCount = root.getNbItemsPerRow()

        var newIndex = -1
        if (KeyHelper.matchRight(event)) {
            if ((currentIndex + 1) % colCount !== 0) {//are we not at the end of line
                newIndex = Math.min(_count - 1, currentIndex + 1)
            }
        } else if (KeyHelper.matchLeft(event)) {
            if (currentIndex % colCount !== 0) {//are we not at the begining of line
                newIndex = Math.max(0, currentIndex - 1)
            }
        } else if (KeyHelper.matchDown(event)) {
            if (Math.floor(currentIndex / colCount) !== Math.floor(_count / colCount)) { //we are not on the last line
                newIndex = Math.min(_count - 1, currentIndex + colCount)
            }
        } else if (KeyHelper.matchPageDown(event)) {
            newIndex = Math.min(_count - 1, currentIndex + colCount * 5)
        } else if (KeyHelper.matchUp(event)) {
            if (Math.floor(currentIndex / colCount) !== 0) { //we are not on the first line
                newIndex = Math.max(0, currentIndex - colCount)
            }
        } else if (KeyHelper.matchPageUp(event)) {
            newIndex = Math.max(0, currentIndex - colCount * 5)
        } else if (KeyHelper.matchOk(event) || event.matches(StandardKey.SelectAll) ) {
            //these events are matched on release
            event.accepted = true
        }

        if (newIndex !== -1 && newIndex !== currentIndex) {
            event.accepted = true
            var oldIndex = currentIndex
            currentIndex = newIndex
            root.selectionUpdated(event.modifiers, oldIndex, newIndex)
        }

        if (!event.accepted)
            defaultKeyAction(event, currentIndex)
    }

    Keys.onReleased: {
        if (event.matches(StandardKey.SelectAll)) {
            event.accepted = true
            root.selectAll()
        } else if ( KeyHelper.matchOk(event) ) {
            event.accepted = true
            root.actionAtIndex(currentIndex)
        }
    }
}
