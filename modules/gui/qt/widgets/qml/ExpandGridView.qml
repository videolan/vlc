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
    property int marginBottom: 0
    property int marginTop: 0

    property int horizontalSpacing: VLCStyle.column_margin_width
    property int verticalSpacing: VLCStyle.column_margin_width

    readonly property int _effectiveCellWidth: cellWidth + horizontalSpacing
    readonly property int _effectiveCellHeight: cellHeight + verticalSpacing

    property variant delegateModel
    property variant model

    property int currentIndex: 0
    property alias contentHeight: flickable.contentHeight
    property alias contentWidth: flickable.contentWidth
    property alias contentX: flickable.contentX
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
    property int headerHeight: headerItemLoader.implicitHeight
    property alias headerItem: headerItemLoader.item

    //signals emitted when selected items is updated from keyboard
    signal selectionUpdated( int keyModifiers, int oldIndex,int newIndex )
    signal selectAll()
    signal actionAtIndex(int index)

    property variant _idChildrenMap: ({})
    property variant _unusedItemList: []

    Accessible.role: Accessible.Table

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
        return Math.max(Math.floor((width + root.horizontalSpacing) / root._effectiveCellWidth), 1)
    }

    function getItemRowCol(id) {
        var nbItemsPerRow = getNbItemsPerRow()
        var rowId = Math.floor(id / nbItemsPerRow)
        var colId = id % nbItemsPerRow
        return [colId, rowId]
    }

    function getItemPos(id) {
        var colCount = root.getNbItemsPerRow()
        var remainingSpace = flickable.width - (colCount * root._effectiveCellWidth) + root.horizontalSpacing
        var rowCol = getItemRowCol(id)
        return [(rowCol[0] * root._effectiveCellWidth) + (remainingSpace / 2), rowCol[1] * root._effectiveCellHeight + headerHeight + marginTop]
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
            newContentY = itemTopY - marginTop
        } else if (itemBottomY > viewBottomY) {
            //item below view
            newContentY = itemBottomY + marginBottom - flickable.height
        }

        if (newContentY !== flickable.contentY)
            animateFlickableContentY(newContentY)
    }

    function _updateSelected() {
        for (var id in _idChildrenMap) {
            var item = _idChildrenMap[id]
            item.selected = delegateModel.isSelected(model.index(id, 0))
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

    function _getFirstAndLastInstanciatedItemIds() {
        var myContentY = flickable.contentY - root.headerHeight - marginTop

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

    function _setupChild(id, ydelta) {
        var item
        var pos = root.getItemPos(id)

        if (id in _idChildrenMap) {
            // re-position

            item = _idChildrenMap[id]
            if (item === undefined)
                throw "wrong child: " + id
            //theses properties are always defined in Item
            item.focus = (id === root.currentIndex) && (root.expandIndex === -1)
            item.x = pos[0]
            item.y = pos[1] + ydelta

        }  else if (_unusedItemList.length > 0) {
            //recyle

            item = _unusedItemList.pop()
            if (item === undefined)
                throw "wrong toRecycle child " + id + ", len " + toUse.length

            item.index = id
            item.model = model.getDataAt(id)

            item.focus = (id === root.currentIndex) && (root.expandIndex === -1)
            item.x = pos[0]
            item.y = pos[1] + ydelta
            item.visible = true

            _idChildrenMap[id] = item

        } else {
            //instanciate a new item

            item = root.delegate.createObject( flickable.contentItem, {
                            index: id,
                            model: model.getDataAt(id),
                            focus: (id === root.currentIndex) && (root.expandIndex === -1),
                            x: pos[0],
                            y: pos[1] + ydelta,
                            visible: true
                        });
            if (item === undefined)
                throw "wrong unable to instantiate child " + id
            _idChildrenMap[id] = item
        }
        return item
    }

    function _refreshData( iMin, iMax ) {
        var f_l = _getFirstAndLastInstanciatedItemIds()
        if (!iMin || iMin < f_l[0])
            iMin = f_l[0]
        if (!iMax || iMax > f_l[1])
            iMax= f_l[1]

        for (var id  = iMin; id <= iMax; id++) {
            var item = _idChildrenMap[id]
            if (!item) {
                continue
            }
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
            var iMax = bottomRight.row
            var f_l = _getFirstAndLastInstanciatedItemIds()
            if (iMin <= f_l[1] && f_l[0] <= iMax) {
                flickable.layout(true)
                _refreshData(iMin, iMax)
            }
        }
        onRowsInserted: _onModelCountChanged()
        onRowsRemoved: _onModelCountChanged()
        onModelReset: _onModelCountChanged()
    }

    onModelChanged: _onModelCountChanged()

    //Gridview visible above the expanded item
    Flickable {
        id: flickable

        clip: true

        flickableDirection: Flickable.VerticalFlick
        ScrollBar.vertical: ScrollBar { }

        Loader {
            id: headerItemLoader
            //load the header early (when the first row is visible)
            visible: flickable.contentY < (root.headerHeight + root._effectiveCellHeight + root.marginTop)
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
                item.y = root.marginTop
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

        function layout(forceRelayout) {
            if (flickable.width === 0 || flickable.height === 0)
                return
            else if (!root._isInitialised)
                root._initialize()

            var i
            var expandItemGridId = getExpandItemGridId()

            var f_l = _getFirstAndLastInstanciatedItemIds()
            var nbItems = f_l[1] - f_l[0]
            var firstId = f_l[0]
            var lastId = f_l[1]

            var topGridEndId = Math.max(Math.min(expandItemGridId, lastId), firstId)

            // Clean the no longer used ids
            var toKeep = {}

            for (var id in _idChildrenMap) {
                var val = _idChildrenMap[id]

                if (id >= firstId && id < lastId) {
                    toKeep[id] = val
                } else {
                    _unusedItemList.push(val)
                    val.visible = false
                }
            }
            _idChildrenMap = toKeep


            var item
            var pos
            // Place the delegates before the expandItem
            for (i = firstId; i < topGridEndId; ++i) {
                if (!forceRelayout && i in _idChildrenMap)
                    continue
                _setupChild(i,  0)
            }

            if (root.expandIndex !== -1) {
                var expandItemPos = root.getItemPos(expandItemGridId)
                expandItem.x = expandItemPos[0]
                expandItem.y = expandItemPos[1]

                expandItem.width = root.getNbItemsPerRow() * root._effectiveCellWidth - root.horizontalSpacing
            }

            // Place the delegates after the expandItem
            for (i = topGridEndId; i < lastId; ++i) {
                if (!forceRelayout && i in _idChildrenMap)
                    continue
                 _setupChild(i, _expandItemVerticalSpace)
            }

            // Calculate and set the contentHeight
            var newContentHeight = root.getItemPos(_count - 1)[1] + root._effectiveCellHeight + _expandItemVerticalSpace
            contentHeight = newContentHeight + root.marginBottom // marginTop is included from root.getItemPos
            contentWidth = root._effectiveCellWidth * root.getNbItemsPerRow() - root.horizontalSpacing

            _updateSelected()
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
        _updateSelected()
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

        _updateSelected()
    }

    Keys.onReleased: {
        if (event.matches(StandardKey.SelectAll)) {
            event.accepted = true
            root.selectAll()
        } else if ( KeyHelper.matchOk(event) ) {
            event.accepted = true
            root.actionAtIndex(currentIndex)
        }

        _updateSelected()
    }
}
