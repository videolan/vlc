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
import QtQml.Models 2.11
import QtQuick.Controls 2.4

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///util/" as Util

FocusScope {
    id: root

    // Properties

    /// cell Width
    property int cellWidth: 100
    // cell Height
    property int cellHeight: 100

    //margin to apply
    property int bottomMargin: 0
    property int topMargin: 0
    property int leftMargin: VLCStyle.margin_normal
    property int rightMargin: VLCStyle.margin_normal

    // NOTE: The grid margin for the item(s) horizontal positioning.
    readonly property int contentMargin: (_contentWidth - _nbItemPerRow * _effectiveCellWidth
                                          +
                                          horizontalSpacing) / 2

    property int rowX: 0
    property int horizontalSpacing: VLCStyle.column_margin_width
    property int verticalSpacing: VLCStyle.column_margin_width

    property int displayMarginEnd: 0

    readonly property int _effectiveCellWidth: cellWidth + horizontalSpacing
    readonly property int _effectiveCellHeight: cellHeight + verticalSpacing

    readonly property int _contentWidth: width - rightMargin - leftMargin

    readonly property int _nbItemPerRow: Math.max(Math.floor((_contentWidth + horizontalSpacing)
                                                             /
                                                             _effectiveCellWidth), 1)

    property Util.SelectableDelegateModel selectionDelegateModel
    property QtAbstractItemModel model

    property int currentIndex: 0

    property bool isAnimating: animateRetractItem.running || animateExpandItem.running

    property int _count: 0

    property bool _isInitialised: false

    property bool _releaseActionButtonPressed: false

    /// the id of the item to be expanded
    property int expandIndex: -1
    property int _newExpandIndex: -1
    property int _expandItemVerticalSpace: 0

    property int _currentFocusReason: Qt.OtherFocusReason

    //delegate to display the extended item
    property Component delegate: Item{}

    property var _idChildrenList: []
    property var _unusedItemList: []
    property var _currentRange: [0,0]

    // Aliases

    property alias contentHeight: flickable.contentHeight
    property alias contentWidth: flickable.contentWidth
    property alias contentX: flickable.contentX
    property alias gridScrollBar: flickableScrollBar

    property alias expandDelegate: expandItemLoader.sourceComponent
    property alias expandItem: expandItemLoader.item

    property alias headerDelegate: headerItemLoader.sourceComponent
    property alias headerHeight: headerItemLoader.implicitHeight
    property alias headerItem: headerItemLoader.item

    property alias footerDelegate: footerItemLoader.sourceComponent
    property alias footerItem: footerItemLoader.item

    // Signals

    //signals emitted when selected items is updated from keyboard
    signal selectAll()
    signal actionAtIndex(int index)

    signal showContextMenu(point globalPos)

    // Settings

    Accessible.role: Accessible.Table

    // Events

    Component.onCompleted: flickable.layout(true)

    onHeightChanged: flickable.layout(false)

    // NOTE: Update on contentMargin change rather than width since the margin defines the x
    //       position and depends on the width.
    onContentMarginChanged: flickable.layout(true)

    onDisplayMarginEndChanged: flickable.layout(false)

    onModelChanged: _onModelCountChanged()

    onCurrentIndexChanged: {
        if (expandIndex !== -1)
            retract()
        positionViewAtIndex(currentIndex, ItemView.Contain)
    }

    on_ExpandItemVerticalSpaceChanged: {
        if (expandItem) {
            expandItem.visible = _expandItemVerticalSpace - verticalSpacing > 0
            expandItem.height = Math.max(_expandItemVerticalSpace - verticalSpacing, 0)
        }
        flickable.layout(true)
    }

    // Keys

    Keys.onPressed: {
        var newIndex = -1
        if (KeyHelper.matchRight(event)) {
            if ((currentIndex + 1) % _nbItemPerRow !== 0) {//are we not at the end of line
                newIndex = Math.min(_count - 1, currentIndex + 1)
            }
        } else if (KeyHelper.matchLeft(event)) {
            if (currentIndex % _nbItemPerRow !== 0) {//are we not at the beginning of line
                newIndex = Math.max(0, currentIndex - 1)
            }
        } else if (KeyHelper.matchDown(event)) {
            // we are not on the last line
            if (Math.floor(currentIndex / _nbItemPerRow) !== Math.floor(_count / _nbItemPerRow)) {
                newIndex = Math.min(_count - 1, currentIndex + _nbItemPerRow)
            }
        } else if (KeyHelper.matchPageDown(event)) {
            newIndex = Math.min(_count - 1, currentIndex + _nbItemPerRow * 5)
        } else if (KeyHelper.matchUp(event)) {
            if (Math.floor(currentIndex / _nbItemPerRow) !== 0) { //we are not on the first line
                newIndex = Math.max(0, currentIndex - _nbItemPerRow)
            }
        } else if (KeyHelper.matchPageUp(event)) {
            newIndex = Math.max(0, currentIndex - _nbItemPerRow * 5)
        } else if (KeyHelper.matchOk(event) || event.matches(StandardKey.SelectAll) ) {
            //these events are matched on release
            event.accepted = true
        }

        if (event.matches(StandardKey.SelectAll) || KeyHelper.matchOk(event)) {
            _releaseActionButtonPressed = true
        } else {
            _releaseActionButtonPressed = false
        }

        if (newIndex !== -1 && newIndex !== currentIndex) {
            event.accepted = true;

            var oldIndex = currentIndex;
            currentIndex = newIndex;
            selectionDelegateModel.updateSelection(event.modifiers, oldIndex, newIndex)

            // NOTE: We make sure we have the proper visual focus on components.
            if (oldIndex < currentIndex)
                setCurrentItemFocus(Qt.TabFocusReason);
            else
                setCurrentItemFocus(Qt.BacktabFocusReason);
        }

        if (!event.accepted) {
            Navigation.defaultKeyAction(event)
        }
    }

    Keys.onReleased: {
        if (!_releaseActionButtonPressed)
            return

        if (event.matches(StandardKey.SelectAll)) {
            event.accepted = true
            selectionDelegateModel.select(model.index(0, 0), ItemSelectionModel.Select | ItemSelectionModel.Columns)
        } else if ( KeyHelper.matchOk(event) ) {
            event.accepted = true
            actionAtIndex(currentIndex)
        }
        _releaseActionButtonPressed = false
    }

    // Connections

    Connections {
        target: model
        onDataChanged: {
            var iMin = topLeft.row
            var iMax = bottomRight.row + 1 // [] => [)
            var f_l = _currentRange
            if (iMin < f_l[1] && f_l[0] < iMax) {
                _refreshData(iMin, iMax)
            }
        }
        onRowsInserted: _onModelCountChanged()
        onRowsRemoved: _onModelCountChanged()
        onModelReset: _onModelCountChanged()
    }

    Connections {
        target: selectionDelegateModel

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
                    var item = root._getItem(j)
                    console.assert(item)
                    item.selected = select
                }
            }
        }
    }

    Connections {
        target: MainCtx
        onIntfScaleFactorChanged: flickable.layout(true)
    }

    // Animations

    PropertyAnimation {
        id: animateContentY;
        target: flickable;
        properties: "contentY"
    }

    // Functions

    function setCurrentItemFocus(reason) {

        // NOTE: Saving the focus reason for later.
        _currentFocusReason = reason;

        if (!model || model.count === 0 || currentIndex === -1) {
            // NOTE: By default we want the focus on the flickable.
            flickable.forceActiveFocus(reason);

            return;
        }

        if (_containsItem(currentIndex))
            Helpers.enforceFocus(_getItem(currentIndex), reason);
        else
            flickable.forceActiveFocus(reason);

        // NOTE: We make sure the current item is fully visible.
        positionViewAtIndex(currentIndex, ItemView.Contain);

        if (expandIndex !== -1) {
            // NOTE: We clear expandIndex so we can apply the proper focus in _setupChild.
            expandIndex = -1;

            retract();
        }
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

    function getItemRowCol(id) {
        var rowId = Math.floor(id / _nbItemPerRow)
        var colId = id % _nbItemPerRow
        return [colId, rowId]
    }

    function getItemPos(id) {
        var rowCol = getItemRowCol(id);

        var x = rowCol[0] * _effectiveCellWidth + contentMargin + leftMargin;

        var y = rowCol[1] * _effectiveCellHeight + headerHeight + topMargin;

        // NOTE: Position needs to be integer based if we want to avoid visual artifacts like
        //       wrong alignments or blurry texture rendering.
        return [Math.round(x), Math.round(y)];
    }

    //use the same signature as Gridview.positionViewAtIndex(index, PositionMode mode)
    //mode is ignored at the moment
    function positionViewAtIndex(index, mode) {
        if (flickable.width === 0 || flickable.height === 0
            ||
            index < 0 || index >= _count)
            return

        var itemTopY = getItemPos(index)[1]
        var itemBottomY = itemTopY + _effectiveCellHeight

        var viewTopY = flickable.contentY
        var viewBottomY = viewTopY + flickable.height

        var newContentY

        if (itemTopY < viewTopY)
             //item above view
            newContentY = itemTopY - topMargin
        else if (itemBottomY > viewBottomY)
             //item below view
            newContentY = itemBottomY + bottomMargin - flickable.height
        else
            newContentY = flickable.contentY

        if (newContentY !== flickable.contentY)
            animateFlickableContentY(newContentY)
    }

    function leftClickOnItem(modifier, index) {
        selectionDelegateModel.updateSelection(modifier, currentIndex, index)
        if (selectionDelegateModel.isSelected(model.index(index, 0)))
            currentIndex = index
        else if (currentIndex === index) {
            if (_containsItem(currentIndex))
                _getItem(currentIndex).focus = false
            currentIndex = -1
        }

        // NOTE: We make sure to clear the keyboard focus.
        flickable.forceActiveFocus();
    }

    function rightClickOnItem(index) {
        if (!selectionDelegateModel.isSelected(model.index(index, 0))) {
            leftClickOnItem(Qt.NoModifier, index)
        }
    }

    function animateFlickableContentY( newContentY ) {
        animateContentY.stop()
        animateContentY.duration = VLCStyle.duration_long
        animateContentY.to = newContentY
        animateContentY.start()
    }

    // Private

    function _initialize() {
        if (_isInitialised)
            return;

        if (flickable.width === 0 || flickable.height === 0)
            return;
        if (currentIndex !== 0)
            positionViewAtIndex(currentIndex, ItemView.Contain)
        _isInitialised = true;
    }

    function _calculateCurrentRange() {
        var myContentY = flickable.contentY
        var contentYWithoutExpand = myContentY
        var heightWithoutExpand = flickable.height + displayMarginEnd

        if (expandIndex !== -1) {
            var expandItemY = getItemPos(flickable.getExpandItemGridId())[1]

            if (myContentY >= expandItemY && myContentY < expandItemY + _expandItemVerticalSpace)
                contentYWithoutExpand = expandItemY
            if (myContentY >= expandItemY + _expandItemVerticalSpace)
                contentYWithoutExpand = myContentY - _expandItemVerticalSpace

            var expandYStart = Math.max(myContentY, expandItemY)
            var expandYEnd = Math.min(myContentY + height, expandItemY + _expandItemVerticalSpace)
            var expandDisplayedHeight = Math.max(expandYEnd - expandYStart, 0)
            heightWithoutExpand -= expandDisplayedHeight
        }

        var onlyGridContentY = contentYWithoutExpand - headerHeight - topMargin
        var rowId = Math.floor(onlyGridContentY / _effectiveCellHeight)
        var firstId = Math.max(rowId * _nbItemPerRow, 0)

        rowId = Math.ceil((onlyGridContentY + heightWithoutExpand) / _effectiveCellHeight)
        var lastId = Math.min(rowId * _nbItemPerRow, _count)

        return [firstId, lastId]
    }

    function _getItem(id) {
        var i = id - _currentRange[0]
        return _idChildrenList[i]
    }

    function _setItem(id, item) {
        var i = id - _currentRange[0]
        _idChildrenList[i] = item
    }

    function _containsItem(id) {
        var i = id - _currentRange[0]
        var childrenList = _idChildrenList
        return i >= 0 && i < childrenList.length && typeof childrenList[i] !== "undefined"
    }

    function _indexToZ(id) {
        var rowCol = getItemRowCol(id)
        return rowCol[0] % 2 + 2 * (rowCol[1] % 2)
    }

    function _repositionItem(id, x, y) {
        var item = _getItem(id)
        console.assert(item !== undefined, "wrong child: " + id)

        //theses properties are always defined in Item
        item.x = x
        item.y = y
        item.z = _indexToZ(id)
        item.selected = selectionDelegateModel.isSelected(model.index(id, 0))

        return item
    }

    function _recycleItem(id, x, y) {
        var item = _unusedItemList.pop()
        console.assert(item !== undefined, "incorrect _recycleItem call, id" + id + " ununsedItemList size" + _unusedItemList.length)

        item.index = id
        item.model = model.getDataAt(id)
        item.selected = selectionDelegateModel.isSelected(model.index(id, 0))
        item.x = x
        item.y = y
        item.z = _indexToZ(id)
        item.visible = true

        _setItem(id, item)

        return item
    }

    function _createItem(id, x, y) {
        var item = delegate.createObject( flickable.contentItem, {
                        selected: selectionDelegateModel.isSelected(model.index(id, 0)),
                        index: id,
                        model: model.getDataAt(id),
                        x: x,
                        y: y,
                        z: _indexToZ(id),
                        visible: true
                    })

        console.assert(item !== undefined, "unable to instantiate " + id)
        _setItem(id, item)

        return item
    }

    function _setupChild(id, ydelta) {
        var pos = getItemPos(id)

        var item;

        if (_containsItem(id))
            item = _repositionItem(id, pos[0], pos[1] + ydelta)
        else if (_unusedItemList.length > 0)
            item = _recycleItem(id, pos[0], pos[1] + ydelta)
        else
            item = _createItem(id, pos[0], pos[1] + ydelta)

        // NOTE: This makes sure we have the proper focus reason on the GridItem.
        if (activeFocus && currentIndex === item.index && expandIndex === -1)
            item.forceActiveFocus(_currentFocusReason)
        else
            item.focus = false
    }

    function _refreshData( iMin, iMax ) {
        var f_l = _currentRange
        if (!iMin || iMin < f_l[0])
            iMin = f_l[0]
        if (!iMax || iMax > f_l[1])
            iMax= f_l[1]

        for (var id  = iMin; id < iMax; id++) {
            var item = _getItem(id)
            item.model = model.getDataAt(id)
        }

        if (expandIndex >= iMin && expandIndex < iMax) {
            expandItem.model = model.getDataAt(expandIndex)
        }
    }

    function _onModelCountChanged() {
        _count = model ? model.rowCount() : 0
        if (!_isInitialised)
            return

        // Hide the expandItem with no animation
        expandIndex = -1
        _expandItemVerticalSpace = 0

        // Regenerate the gridview layout
        flickable.layout(true)
        _refreshData()
    }

    // Children

    //Gridview visible above the expanded item
    Flickable {
        id: flickable

        flickableDirection: Flickable.VerticalFlick

        ScrollBar.vertical: ScrollBar {
            id: flickableScrollBar
        }

        MouseEventFilter {
            target: flickable

            onMouseButtonPress: {
                if (buttons & (Qt.LeftButton | Qt.RightButton)) {
                    Helpers.enforceFocus(flickable, Qt.MouseFocusReason)

                    if (!(modifiers & (Qt.ShiftModifier | Qt.ControlModifier))) {
                        if (selectionDelegateModel)
                            selectionDelegateModel.clear()
                    }
                }
            }

            onMouseButtonRelease: {
                if (button & Qt.RightButton) {
                    root.showContextMenu(globalPos)
                }
            }
        }
        
        Util.FlickableScrollHandler { }

        Loader {
            id: headerItemLoader

            x: 0
            y: root.topMargin

            //load the header early (when the first row is visible)
            visible: flickable.contentY < (root.headerHeight + root._effectiveCellHeight + root.topMargin)

            focus: (status === Loader.Ready) ? item.focus : false
        }

        Loader {
            id: footerItemLoader

            focus: (status === Loader.Ready) ? item.focus : false

            y: root.topMargin + root.headerHeight + (root._effectiveCellHeight * (Math.ceil(model.count / root._nbItemPerRow))) +
               root._expandItemVerticalSpace
        }

        Connections {
            target: headerItem

            function _scrollToHeaderOnFocus() {
                if (!headerItem.activeFocus)
                    return;

                // when we gain the focus ensure the widget is fully visible
                animateFlickableContentY(0)
            }

            onHeightChanged: {
                flickable.layout(true)
            }

            onActiveFocusChanged: {
                // when header loads because of setting headerItem.focus == true, it will suddenly attain the active focus
                // but then a queued flickable.layout() may take away it's focus and assign it to current item,
                // using Qt.callLater we save unnecessary scrolling
                Qt.callLater(_scrollToHeaderOnFocus)
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

            active: root.expandIndex !== -1
            focus: active

            onLoaded: {
                item.height = 0

                // only make loader visible after setting initial y pos from layout() as to not get flickering
                expandItemLoader.visible = false
            }
        }

        anchors.fill: parent

        onContentYChanged: { Qt.callLater(flickable.layout, false) }

        function getExpandItemGridId() {
            var ret
            if (root.expandIndex !== -1) {
                var rowCol = root.getItemRowCol(root.expandIndex)
                var rowId = rowCol[1] + 1
                ret = rowId * root._nbItemPerRow
            } else {
                ret = root._count
            }
            return ret
        }

        function _setupIndexes(force, range, yDelta) {
            for (var i = range[0]; i < range[1]; i++) {
                if (!force && root._containsItem(i))
                    continue
                _setupChild(i, yDelta)
            }
        }

        function _overlappedInterval(i1, i2) {
            if (i1[0] > i2[0]) return _overlappedInterval(i2, i1)
            if (i1[1] > i2[0]) return [i2[0], Math.min(i1[1], i2[1])]
            return [0, 0]
        }

        function _updateChildrenMap(first, last) {
            if (first >= last) {
                root._idChildrenList.forEach(function(item) { item.visible = false; })
                root._unusedItemList = root._idChildrenList
                root._idChildrenList = []
                root._currentRange = [0, 0]
                return
            }

            var overlapped = _overlappedInterval([first, last], root._currentRange)

            var i
            var newList = new Array(last - first)

            for (i = overlapped[0]; i < overlapped[1]; ++i) {
                newList[i - first] = root._getItem(i)
                root._setItem(i, undefined)
            }

            for (i = root._currentRange[0]; i < root._currentRange[1]; ++i) {
                var item = root._getItem(i)
                if (typeof item !== "undefined") {
                    item.visible = false
                    root._unusedItemList.push(item)
                    //  root._setItem(i, undefined) // not needed the list will be reset following this loop
                }
            }

            root._idChildrenList = newList
            root._currentRange = [first, last]
        }

        function layout(forceRelayout) {
            if (flickable.width === 0 || flickable.height === 0)
                return
            else if (!root._isInitialised)
                root._initialize()

            root.rowX = getItemPos(0)[0]

            var expandItemGridId = getExpandItemGridId()

            var f_l = _calculateCurrentRange()
            var nbItems = f_l[1] - f_l[0]
            var firstId = f_l[0]
            var lastId = f_l[1]

            var topGridEndId = Math.max(Math.min(expandItemGridId, lastId), firstId)

            if (!forceRelayout && root._currentRange[0] === firstId && root._currentRange[1] === lastId)
                return;

            _updateChildrenMap(firstId, lastId)

            // Place the delegates before the expandItem
            _setupIndexes(forceRelayout, [firstId, topGridEndId], 0)

            if (root.expandIndex !== -1) {
                var expandItemPos = root.getItemPos(expandItemGridId)
                expandItem.y = expandItemPos[1]
                if (!expandItemLoader.visible)
                    expandItemLoader.visible = true
            }

            // Place the delegates after the expandItem
            _setupIndexes(forceRelayout, [topGridEndId, lastId], root._expandItemVerticalSpace)

            // update contentWidth and contentHeight
            var gridContentWidth = root._effectiveCellWidth * root._nbItemPerRow - root.horizontalSpacing
            contentWidth = root.leftMargin + gridContentWidth + root.rightMargin

            var gridContentHeight = root.getItemPos(root._count - 1)[1] + root._effectiveCellHeight + root._expandItemVerticalSpace
            contentHeight = gridContentHeight
                    + (footerItemLoader.item ? footerItemLoader.item.height : 0)
                    + root.bottomMargin // topMargin and headerHeight is included in root.getItemPos
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
            root.expandIndex = root._newExpandIndex
            if (root.expandIndex === -1)
                return
            expandItem.model = model.getDataAt(root.expandIndex)
            /* We must also start the expand animation here since the expandItem implicitHeight is not
               changed if it had the same height at previous opening. */
            expandAnimation()
        }

        function expandAnimation() {
            if (root.expandIndex === -1)
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
            var currentItemYPos = root.getItemPos(root.expandIndex)[1]
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
            duration: VLCStyle.duration_long
            to: 0
            onStopped: {
                root.expandIndex = -1
                if (root._newExpandIndex !== -1)
                    flickable.expand()
            }
        }

        NumberAnimation {
            id: animateExpandItem;
            target: root;
            properties: "_expandItemVerticalSpace"
            easing.type: Easing.InQuad
            duration: VLCStyle.duration_long
            from: 0
        }
    }
}
