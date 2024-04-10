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
import QtQuick.Window
import QtQuick.Controls

import QtQml.Models

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
    property int leftMargin: VLCStyle.column_margin + leftPadding
    property int rightMargin: VLCStyle.column_margin + rightPadding

    property int leftPadding: 0
    property int rightPadding: 0

    readonly property int extraMargin: (_contentWidth - nbItemPerRow * _effectiveCellWidth
                                        +
                                        horizontalSpacing) / 2

    // NOTE: The grid margins for the item(s) horizontal positioning.
    readonly property int contentLeftMargin: extraMargin + leftMargin
    readonly property int contentRightMargin: extraMargin + rightMargin

    readonly property int rowHeight: cellHeight + verticalSpacing

    // This property enables you to reuse items that are instantiated for
    // different indexes when particular index goes out of view
    // not setting may result in large performance penalty
    // default is true
    property bool reuseItems: true

    property int rowX: 0
    property int horizontalSpacing: VLCStyle.column_spacing
    property int verticalSpacing: VLCStyle.column_spacing

    property int displayMarginEnd: 0

    readonly property int nbItemPerRow: Math.max(Math.floor((_contentWidth + horizontalSpacing)
                                                            /
                                                            _effectiveCellWidth), 1)

    readonly property int _effectiveCellWidth: cellWidth + horizontalSpacing

    readonly property int _contentWidth: width - rightMargin - leftMargin

    property ListSelectionModel selectionModel: ListSelectionModel {
        model: root.model
    }

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

    // The delegate provides a template defining each item instantiated by the view.
    // 'delegate' must have following properties defined -
    // 'var model'
    //      - set by ExpandGridView, this defines model data associated with item
    //        index data can be accesses by the model roles
    // 'int index'
    //      - set by ExpandGridView, this defines the index to which this delegate
    //        is associated to
    // 'bool selected'
    //      - set by ExpandGridView, this defines if the associated index is selected
    //        in selectionModel
    //
    // optional properties -
    // 'bool delayRemove'
    //      - if defined and set, item will not be removed when it goes out of view
    //        ExpandGridView will never modify this value
    //
    property Component delegate: Item {
        property var model: null
        property int index: - 1
        property bool selected: false
    }

    property var _idChildrenList: []
    property var _unusedItemList: []
    property var _currentRange: [0,0]
    property var _delayedChildrenMap: ({})

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

    contentWidth: {
        const size = _effectiveCellWidth * nbItemPerRow - horizontalSpacing

        return leftMargin + size + rightMargin
    }

    contentHeight: {
        const size = getItemPos(_count - 1)[1] + rowHeight + _expandItemVerticalSpace

        // NOTE: topMargin and headerHeight are included in root.getItemPos.
        if (footerItem)
            return size + footerItem.height + bottomMargin
        else
            return size + bottomMargin
    }

    Accessible.role: Accessible.Table

    activeFocusOnTab: true

    // Events

    Component.onCompleted: flickable.layout(true)

    onHeightChanged: flickable.layout(false)

    // NOTE: Update on contentLeftMargin since we depend on this for item placements.
    onContentLeftMarginChanged: flickable.layout(true)

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

    onReuseItemsChanged: {
        if (!reuseItems) {
            _unusedItemList.forEach((item) => { item.destroy() })
            _unusedItemList = []
        }
    }

    // Keys

    Keys.onPressed: (event) => {
        let newIndex = -1
        if (KeyHelper.matchRight(event)) {
            if ((currentIndex + 1) % nbItemPerRow !== 0) {//are we not at the end of line
                newIndex = Math.min(_count - 1, currentIndex + 1)
            }
        } else if (KeyHelper.matchLeft(event)) {
            if (currentIndex % nbItemPerRow !== 0) {//are we not at the beginning of line
                newIndex = Math.max(0, currentIndex - 1)
            }
        } else if (KeyHelper.matchDown(event)) {
            const lastIndex = _count - 1
            // we are not on the last line
            if (Math.floor(currentIndex / nbItemPerRow)
                !==
                Math.floor(lastIndex / nbItemPerRow)) {
                newIndex = Math.min(lastIndex, currentIndex + nbItemPerRow)
            }
        } else if (KeyHelper.matchPageDown(event)) {
            newIndex = Math.min(_count - 1, currentIndex + nbItemPerRow * 5)
        } else if (KeyHelper.matchUp(event)) {
            if (Math.floor(currentIndex / nbItemPerRow) !== 0) { //we are not on the first line
                newIndex = Math.max(0, currentIndex - nbItemPerRow)
            }
        } else if (KeyHelper.matchPageUp(event)) {
            newIndex = Math.max(0, currentIndex - nbItemPerRow * 5)
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

            const oldIndex = currentIndex;
            currentIndex = newIndex;
            selectionModel.updateSelection(event.modifiers, oldIndex, newIndex)

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

    Keys.onReleased: (event) => {
        if (!_releaseActionButtonPressed)
            return

        if (event.matches(StandardKey.SelectAll)) {
            event.accepted = true
            selectionModel.selectAll()
        } else if ( KeyHelper.matchOk(event) ) {
            event.accepted = true
            actionAtIndex(currentIndex)
        }
        _releaseActionButtonPressed = false
    }

    // Connections

    Connections {
        target: model
        function onDataChanged(topLeft, bottomRight, roles) {
            const iMin = topLeft.row
            const iMax = bottomRight.row + 1 // [] => [)
            const f_l = _currentRange

            _refreshDelayedChildData(iMin, iMax)

            if (iMin < f_l[1] && f_l[0] < iMax) {
                _refreshData(iMin, iMax)
            }
        }
        function onRowsInserted() { _onModelCountChanged() }
        function onRowsRemoved() { _onModelCountChanged() }
        function onModelReset() { _onModelCountChanged() }

        // NOTE: This is useful for SortFilterProxyModel(s).
        function onLayoutChanged() { _onModelCountChanged() }
    }

    Connections {
        target: selectionModel

        function onSelectionChanged(selected, deselected) {
            for (let i = 0; i < selected.length; ++i) {
                _updateSelectedRange(selected[i].topLeft, selected[i].bottomRight, true)
            }

            for (let i = 0; i < deselected.length; ++i) {
                _updateSelectedRange(deselected[i].topLeft, deselected[i].bottomRight, false)
            }
        }

        function _updateSelectedRange(topLeft, bottomRight, select) {
            let iMin = topLeft.row
            let iMax = bottomRight.row + 1 // [] => [)

            // delayed children can be out of currentRange
            _updateDelayedChildSelected(iMin, iMax, select)

            if (iMin < root._currentRange[1] && root._currentRange[0] < iMax) {
                // only update item in the view
                iMin = Math.max(iMin, root._currentRange[0])
                iMax = Math.min(iMax, root._currentRange[1])

                for (let j = iMin; j < iMax; j++) {
                    const item = root._getItem(j)
                    console.assert(item)
                    item.selected = select
                }
            }
        }
    }

    Connections {
        target: MainCtx
        function onIntfScaleFactorChanged() { flickable.layout(true) }
    }

    // Animations

    PropertyAnimation {
        id: animateContentY;
        target: flickable;
        properties: "contentY"
    }

    // Functions

    // NOTE: This function is useful to set the currentItem without losing the visual focus.
    function setCurrentItem(index) {
        if (currentIndex === index)
            return

        let reason

        let item = _getItem(index)

        if (item)
            reason = item.focusReason
        else
            reason = _currentFocusReason

        currentIndex = index

        item = _getItem(index)

        if (reason !== Qt.OtherFocusReason) {
            if (item)
                Helpers.enforceFocus(item, reason)
            else
                setCurrentItemFocus(reason)
        }
    }

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
            // We clear expandIndex so we can apply the proper focus in _setupChild.
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

    function getItemY(index) {
        return Math.floor(index / nbItemPerRow) * rowHeight + headerHeight + topMargin
    }

    function getItemRowCol(id) {
        const rowId = Math.floor(id / nbItemPerRow)
        const colId = id % nbItemPerRow
        return [colId, rowId]
    }

    function getItemPos(id) {
        const rowCol = getItemRowCol(id);

        const x = rowCol[0] * _effectiveCellWidth + contentLeftMargin;

        const y = rowCol[1] * rowHeight + headerHeight + topMargin;

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

        const newContentY = Helpers.flickablePositionContaining(flickable,
                                                                getItemPos(index)[1]
                                                                , rowHeight
                                                                , topMargin, bottomMargin)

        if (newContentY !== flickable.contentY)
            animateFlickableContentY(newContentY)
    }

    function leftClickOnItem(modifier, index) {
        selectionModel.updateSelection(modifier, currentIndex, index)
        if (selectionModel.isSelected(index))
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
        if (!selectionModel.isSelected(index)) {
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
        const myContentY = flickable.contentY
        let contentYWithoutExpand = myContentY
        let heightWithoutExpand = flickable.height + displayMarginEnd

        if (expandIndex !== -1) {
            const expandItemY = getItemPos(flickable.getExpandItemGridId())[1]

            if (myContentY >= expandItemY && myContentY < expandItemY + _expandItemVerticalSpace)
                contentYWithoutExpand = expandItemY
            if (myContentY >= expandItemY + _expandItemVerticalSpace)
                contentYWithoutExpand = myContentY - _expandItemVerticalSpace

            const expandYStart = Math.max(myContentY, expandItemY)
            const expandYEnd = Math.min(myContentY + height, expandItemY + _expandItemVerticalSpace)
            const expandDisplayedHeight = Math.max(expandYEnd - expandYStart, 0)
            heightWithoutExpand -= expandDisplayedHeight
        }

        const onlyGridContentY = contentYWithoutExpand - headerHeight - topMargin
        const firstRowId = Math.floor(onlyGridContentY / rowHeight)
        const firstId = Math.max(firstRowId * nbItemPerRow, 0)

        const lastRowId = Math.ceil((onlyGridContentY + heightWithoutExpand) / rowHeight)
        const lastId = Math.min(lastRowId * nbItemPerRow, _count)

        return [firstId, lastId]
    }

    function _getItem(id) {
        const i = id - _currentRange[0]
        return _idChildrenList[i]
    }

    function _setItem(id, item) {
        const i = id - _currentRange[0]
        _idChildrenList[i] = item
    }

    function _containsItem(id) {
        const i = id - _currentRange[0]
        const childrenList = _idChildrenList
        return i >= 0 && i < childrenList.length && typeof childrenList[i] !== "undefined"
    }

    function _indexToZ(id) {
        const rowCol = getItemRowCol(id)
        return rowCol[0] % 2 + 2 * (rowCol[1] % 2)
    }

    function _updatePosition(id, item, x, y) {
        //theses properties are always defined in Item
        item.x = x
        item.y = y
        item.z = _indexToZ(id)

        // update required property (do we need this??)
        item.selected = selectionModel.isSelected(id)
    }

    function _repositionItem(id, x, y) {
        const item = _getItem(id)
        console.assert(item !== undefined, "wrong child: " + id)

        _updatePosition(id, item, x, y)
        return item
    }

    function _repositionDelayedItem(id, x, y) {
        const item = _delayedChildrenMap[id]

        _updatePosition(id, item, x, y)
        return item
    }

    function _recycleItem(id, x, y) {
        const item = _unusedItemList.pop()
        console.assert(item !== undefined, "incorrect _recycleItem call, id" + id + " ununsedItemList size" + _unusedItemList.length)

        item.index = id
        item.model = model.getDataAt(id)
        item.selected = selectionModel.isSelected(id)
        item.x = x
        item.y = y
        item.z = _indexToZ(id)
        item.visible = true

        _setItem(id, item)

        return item
    }

    function _createItem(id, x, y) {
        const item = delegate.createObject( flickable.contentItem, {
                        selected: selectionModel.isSelected(id),
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

    function _takeDelayedChild(id) {
        const item = _delayedChildrenMap[id]
        console.assert(typeof item !== "undefined")
        delete _delayedChildrenMap[id]
        return item
    }

    function _shouldDelayRemove(item) {
        return Helpers.get(item, "delayRemove", false)
    }

    function _delayRemove(id, item) {
        _delayedChildrenMap[id] = item

        item.delayRemoveChanged.connect(() => {
            if (id in _delayedChildrenMap && !item.delayRemove) {
                const removed = _takeDelayedChild(id)
                console.assert(removed === item)
                item.destroy()
            }
        })
    }

    function _refreshDelayedChildData(iMin, iMax) {
        for (let i = iMin; i < iMax; ++i) {
            if (!(i in _delayedChildrenMap))
                continue

            const item =  _delayedChildrenMap[i]
            item.model = model.getDataAt(i)
        }
    }

    function _updateDelayedChildSelected(iMin, iMax, select) {
        for (let i = iMin; i < iMax; ++i) {
            if (!(i in _delayedChildrenMap))
                continue

            const item =  _delayedChildrenMap[i]
            item.selected = select
        }
    }

    function _setupChild(id, ydelta) {
        const pos = getItemPos(id)
        pos[1] += ydelta

        let item;

        if (_containsItem(id))
            item = _repositionItem(id, pos[0], pos[1])
        else if (id in _delayedChildrenMap)
            item = _repositionDelayedItem(id, pos[0], pos[1])
        else if (_unusedItemList.length > 0) // if reuseItems is false, _unusedItemList is always empty
            item = _recycleItem(id, pos[0], pos[1])
        else
            item = _createItem(id, pos[0], pos[1])

        // NOTE: This makes sure we have the proper focus reason on the GridItem.
        if (activeFocus && currentIndex === item.index && expandIndex === -1)
            item.forceActiveFocus(_currentFocusReason)
        else
            item.focus = false
    }

    function _refreshData( iMin, iMax ) {
        const f_l = _currentRange
        if (!iMin || iMin < f_l[0])
            iMin = f_l[0]
        if (!iMax || iMax > f_l[1])
            iMax= f_l[1]

        for (let id  = iMin; id < iMax; id++) {
            const item = _getItem(id)
            item.model = model.getDataAt(id)
        }

        if (expandIndex >= iMin && expandIndex < iMax) {
            expandItem.model = model.getDataAt(expandIndex)
        }
    }

    function _onModelCountChanged() {
        _count = model?.rowCount() ?? 0
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

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }


    Flickable {
        id: flickable

        flickableDirection: Flickable.VerticalFlick

        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {
            id: flickableScrollBar
        }

        MouseArea {
            anchors.fill: parent
            z: -1

            preventStealing: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton

            onPressed: (mouse) => {
                Helpers.enforceFocus(flickable, Qt.MouseFocusReason)

                if (!(mouse.modifiers & (Qt.ShiftModifier | Qt.ControlModifier))) {
                    if (selectionModel)
                        selectionModel.clearSelection()
                }
            }

            onReleased: (mouse) => {
                if (mouse.button & Qt.RightButton) {
                    root.showContextMenu(mapToGlobal(mouse.x, mouse.y))
                }
            }
        }

        Util.FlickableScrollHandler { }

        Loader {
            id: headerItemLoader

            x: 0
            y: root.topMargin

            //load the header early (when the first row is visible)
            visible: flickable.contentY < (root.headerHeight + root.rowHeight + root.topMargin)

            focus: (status === Loader.Ready) ? item.focus : false
        }

        Loader {
            id: footerItemLoader

            focus: (status === Loader.Ready) ? item.focus : false

            y: root.topMargin + root.headerHeight + (root.rowHeight * (Math.ceil(model.count / root.nbItemPerRow))) +
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

            function onHeightChanged() {
                flickable.layout(true)
            }

            function onActiveFocusChanged() {
                // when header loads because of setting headerItem.focus == true, it will suddenly attain the active focus
                // but then a queued flickable.layout() may take away it's focus and assign it to current item,
                // using Qt.callLater we save unnecessary scrolling
                Qt.callLater(_scrollToHeaderOnFocus)
            }
        }

        Connections {
            target: footerItem
            function onHeightChanged() {
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
            if (root.expandIndex !== -1) {
                const rowCol = root.getItemRowCol(root.expandIndex)
                const rowId = rowCol[1] + 1
                return rowId * root.nbItemPerRow
            } else {
                return root._count
            }
        }

        function _setupIndexes(force, range, yDelta) {
            for (let i = range[0]; i < range[1]; i++) {
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
                if (root.reuseItems) {
                    root._idChildrenList.forEach((item) => { item.visible = false; })
                    root._unusedItemList = root._idChildrenList
                } else {
                    root._idChildrenList.forEach((item) => { item.destroy() })
                }

                root._idChildrenList = []
                root._currentRange = [0, 0]
                return
            }

            const overlapped = _overlappedInterval([first, last], root._currentRange)

            const newList = new Array(last - first)

            // move items from currentRange still in view
            for (let i = overlapped[0]; i < overlapped[1]; ++i) {
                newList[i - first] = root._getItem(i)
                root._setItem(i, undefined)
            }

            for (let id in _delayedChildrenMap) {
                if (id >= first && id < last) {
                    newList[id - first] = _takeDelayedChild(id)
                }
            }

            // handle item from current range which are not in view
            for (let i = root._currentRange[0]; i < root._currentRange[1]; ++i) {
                const item = root._getItem(i)
                if (typeof item !== "undefined") {
                    if (_shouldDelayRemove(item)) {
                        _delayRemove(i, item)
                    } else if (root.reuseItems) {
                        item.visible = false
                        root._unusedItemList.push(item)
                    } else {
                        item.destroy()
                    }

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

            const expandItemGridId = getExpandItemGridId()

            const f_l = _calculateCurrentRange()
            const nbItems = f_l[1] - f_l[0]
            const firstId = f_l[0]
            const lastId = f_l[1]

            const topGridEndId = Math.max(Math.min(expandItemGridId, lastId), firstId)

            if (!forceRelayout && root._currentRange[0] === firstId && root._currentRange[1] === lastId)
                return;

            _updateChildrenMap(firstId, lastId)

            // Place the delegates before the expandItem
            _setupIndexes(forceRelayout, [firstId, topGridEndId], 0)

            if (root.expandIndex !== -1) {
                const expandItemPos = root.getItemPos(expandItemGridId)
                expandItem.y = expandItemPos[1]
                if (!expandItemLoader.visible)
                    expandItemLoader.visible = true
            }

            // Place the delegates after the expandItem
            _setupIndexes(forceRelayout, [topGridEndId, lastId], root._expandItemVerticalSpace)

            // handle delayedRemoveChildren
            if (forceRelayout) {
                for (let id in _delayedChildrenMap) {
                    // check invariant: delayedRemove child must be reused when they come in view
                    console.assert((id < root._currentRange[0]) || (id >= root._currentRange[1]))

                    if (id >= root._count) {
                        // index is no longer valid
                        const item = _takeDelayedChild(id)
                        item.destroy()
                    } else {
                        const yDelta = (id >= topGridEndId) ? root._expandItemVerticalSpace : 0
                        _setupChild(id, yDelta)
                    }
                }
            }

            if (!root.reuseItems) // check invariant: correct cleanup if reuseItems is not set
                console.assert(_unusedItemList.length == 0)
        }

        Connections {
            target: expandItem
            function onImplicitHeightChanged() {
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

            const expandItemHeight = expandItem.implicitHeight + root.verticalSpacing

            // Expand animation

            expandItem.focus = true
            // The animation may have already been triggered, we must stop it.
            animateExpandItem.stop()
            animateExpandItem.from = root._expandItemVerticalSpace
            animateExpandItem.to = expandItemHeight
            animateExpandItem.start()

            // Sliding animation
            const currentItemYPos = root.getItemPos(root.expandIndex)[1]
                                    + root.rowHeight / 2
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
