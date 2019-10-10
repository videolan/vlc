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

NavigableFocusScope {
    id: root

    /// cell Width
    property int cellWidth: 100
    // cell Height
    property int cellHeight: 100

    //margin to apply
    property int marginBottom: root.cellHeight / 2
    property int marginTop: root.cellHeight / 3

    //model to be rendered, model has to be passed twice, as they cannot be shared between views
    property alias model: flickable.model
    property int modelCount: 0

    property int currentIndex: 0
    property alias contentHeight: flickable.contentHeight
    property alias contentWidth: flickable.contentWidth
    property alias contentX: flickable.contentX
    property bool isAnimating: animateRetractItem.running || animateExpandItem.running

    /// the id of the item to be expanded
    property int _expandIndex: -1
    property int _newExpandIndex: -1

    //delegate to display the extended item
    property Component delegate: Item{}
    property Component expandDelegate: Item{}

    property Component headerDelegate: Item{}
    property int headerHeight: headerItemLoader.implicitHeight

    //signals emitted when selected items is updated from keyboard
    signal selectionUpdated( int keyModifiers, int oldIndex,int newIndex )
    signal selectAll()
    signal actionAtIndex(int index)

    property double _expandRetractSpeed: 1.

    function renderLayout() {
        flickable.layout()
    }

    function switchExpandItem(index) {
        if (index === _expandIndex)
            _newExpandIndex = -1
        else
            _newExpandIndex = index

        if (_expandIndex !== -1)
            flickable.retract()
        else
            flickable.expand()
    }

    function retract() {
        _newExpandIndex = -1
        flickable.retract()
    }

    function getNbItemsPerRow() {
        return Math.max(Math.floor(width / root.cellWidth), 1)
    }

    function getItemRowCol(id) {
        var nbItemsPerRow = getNbItemsPerRow()
        var rowId = Math.floor(id / nbItemsPerRow)
        var colId = id % nbItemsPerRow
        return [colId, rowId]
    }

    function getItemPos(id) {
        var colCount = root.getNbItemsPerRow()
        var remainingSpace = flickable.width - colCount * root.cellWidth
        var rowCol = getItemRowCol(id)
        return [(rowCol[0] * root.cellWidth) + (remainingSpace / 2), rowCol[1] * root.cellHeight + headerHeight]
    }


    //Gridview visible above the expanded item
    Flickable {
        id: flickable
        clip: true

        flickableDirection: Flickable.VerticalFlick
        ScrollBar.vertical: ScrollBar { }

        Loader {
            id: headerItemLoader
            visible: flickable.contentY < root.headerHeight
            sourceComponent: headerDelegate
            onLoaded: {
                item.x = 0
                item.y = 0
            }
        }


        property variant model
        property alias expandItem: expandItemLoader.item
        //root.expandDelegate.createObject(contentItem, {"height": 0})
        Loader {
            id: expandItemLoader
            sourceComponent: expandDelegate
            active: root._expandIndex !== -1
            focus: active
            onLoaded: item.height = 0
        }


        anchors.fill: parent
        onWidthChanged: { layout() }
        onHeightChanged: { layout() }
        onContentYChanged: { layout() }

        function getExpandItemGridId() {
            var ret
            if (root._expandIndex !== -1) {
                var rowCol = root.getItemRowCol(root._expandIndex)
                var rowId = rowCol[1] + 1
                ret = rowId * root.getNbItemsPerRow()
            } else {
                ret = model.count
            }
            return ret
        }

        property variant idChildrenMap: ({})
        property variant _unusedItemList: []

        function getFirstAndLastInstanciatedItemIds() {
            var myContentY = contentY - root.headerHeight

            var contentYWithoutExpand = myContentY
            var heightWithoutExpand = height
            if (root._expandIndex !== -1) {
                if (myContentY >= expandItem.y && myContentY < expandItem.y + expandItem.height)
                    contentYWithoutExpand = expandItem.y
                if (myContentY >= expandItem.y + expandItem.height)
                    contentYWithoutExpand = myContentY - expandItem.height

                var expandYStart = Math.max(myContentY, expandItem.y)
                var expandYEnd = Math.min(myContentY + height, expandItem.y + expandItem.height)
                var expandDisplayedHeight = Math.max(expandYEnd - expandYStart, 0)
                heightWithoutExpand -= expandDisplayedHeight
            }

            var rowId = Math.floor(contentYWithoutExpand / root.cellHeight)
            var firstId = Math.max(rowId * root.getNbItemsPerRow(), 0)

            rowId = Math.ceil((contentYWithoutExpand + heightWithoutExpand) / root.cellHeight)
            var lastId = Math.min(rowId * root.getNbItemsPerRow(), model.count)

            return [firstId, lastId]
        }

        function getChild(id, toUse) {
            var ret
            if (id in idChildrenMap) {
                ret = idChildrenMap[id]
                if (ret === undefined)
                    throw "wrong child: " + id
            }
            else {
                ret = toUse.pop()
                if (ret === undefined)
                    throw "wrong toRecycle child " + id + ", len " + toUse.length
                idChildrenMap[id] = ret
            }
            return ret
        }

        function layout() {
            var i
            var expandItemGridId = getExpandItemGridId()

            var f_l = getFirstAndLastInstanciatedItemIds()
            var nbItems = f_l[1] - f_l[0]
            var firstId = f_l[0]
            var lastId = f_l[1]

            var topGridEndId = Math.max(Math.min(expandItemGridId, lastId), firstId)

            // Clean the no longer used ids
            var toKeep = {}

            for (var id in idChildrenMap) {
                var val = idChildrenMap[id]

                if (id >= firstId && id < lastId) {
                    toKeep[id] = val
                } else {
                    _unusedItemList.push(val)
                    val.visible = false
                }
            }
            idChildrenMap = toKeep

            // Create delegates if we do not have enough
            if (nbItems > _unusedItemList.length + Object.keys(toKeep).length) {
                var toCreate = nbItems - (_unusedItemList.length + Object.keys(toKeep).length)
                for (i = 0; i < toCreate; ++i) {
                    val = root.delegate.createObject(contentItem);
                    _unusedItemList.push(val)
                }
            }

            // Place the delegates before the expandItem
            for (i = firstId; i < topGridEndId; ++i) {
                var pos = root.getItemPos(i)
                var item = getChild(i, _unusedItemList)
                item.model = model.items.get(i).model
                item.index = i
                item.selected = model.items.get(i).inSelected
                item.focus = false
                item.x = pos[0]
                item.y = pos[1]
                item.visible = true
            }

            if (root._expandIndex !== -1)
                expandItem.y = root.getItemPos(expandItemGridId)[1]

            // Place the delegates after the expandItem
            for (i = topGridEndId; i < lastId; ++i) {
                pos = root.getItemPos(i)
                item = getChild(i, _unusedItemList)
                item.model = model.items.get(i).model
                item.focus = false
                item.index = i
                item.selected = model.items.get(i).inSelected
                item.x = pos[0]
                item.y = pos[1] + expandItem.height
                item.visible = true
            }

            // Calculate and set the contentHeight
            var newContentHeight = root.getItemPos(model.count - 1)[1] + root.cellHeight
            if (root._expandIndex !== -1)
                newContentHeight += expandItem.height
            contentHeight = newContentHeight
            contentWidth = root.cellWidth * root.getNbItemsPerRow()
            setCurrentItemFocus()
        }

        Connections {
            target: model.items
            onChanged: {
                // Hide the expandItem with no animation
                _expandIndex = -1

                //flickable.expandItem.height = 0
                // Regenerate the gridview layout
                flickable.layout()
            }
        }

        Connections {
            target: flickable.expandItem
            onHeightChanged: {
                flickable.layout()
            }
            onImplicitHeightChanged: {
                /* This is the only event we have after the expandItem height content was resized.
                   We can trigger here the expand animation with the right final height. */
                if (root._expandIndex !== -1)
                    flickable.expandAnimation()
            }
            onCurrentItemYChanged: {
                var newContentY = flickable.contentY;
                var currentItemYPos = root.getItemPos(currentIndex)[1] + cellHeight + flickable.expandItem.currentItemY
                if (currentItemYPos + flickable.expandItem.currentItemHeight > flickable.contentY + flickable.height) {
                    //move viewport to see current item bottom
                    newContentY = Math.min(
                                currentItemYPos + flickable.expandItem.currentItemHeight - flickable.height,
                                flickable.contentHeight - flickable.height)
                } else if (currentItemYPos < flickable.contentY) {
                    //move viewport to see current item top
                    newContentY = Math.max(currentItemYPos, 0)
                }

                if (newContentY !== flickable.contentY)
                    animateFlickableContentY(newContentY)
            }
        }

        function expand() {
            _expandIndex = _newExpandIndex
            if (_expandIndex === -1)
                return
            expandItem.model = model.items.get(_expandIndex).model
            /* We must also start the expand animation here since the expandItem implicitHeight is not
               changed if it had the same height at previous opening. */
            expandAnimation()
        }

        function expandAnimation() {
            if (_expandIndex === -1)
                return

            var expandItemHeight = flickable.expandItem.implicitHeight;

            // Expand animation

            flickable.expandItem.focus = true
            // The animation may have already been triggered, we must stop it.
            animateExpandItem.stop()
            animateExpandItem.to = expandItemHeight
            animateExpandItem.start()

            // Sliding animation
            var currentItemYPos = root.getItemPos(_expandIndex)[1]
            animateFlickableContentY(currentItemYPos)
        }

        function retract() {
            animateRetractItem.start()
        }

        NumberAnimation {
            id: animateRetractItem;
            target: flickable.expandItem;
            properties: "height"
            easing.type: Easing.OutQuad
            duration: 250
            to: 0
            onStopped: {
                _expandIndex = -1
                flickable.setCurrentItemFocus()
                root.animateToCurrentIndex()
                if (_newExpandIndex !== -1)
                    flickable.expand()
            }
        }

        NumberAnimation {
            id: animateExpandItem;
            target: flickable.expandItem;
            properties: "height"
            easing.type: Easing.InQuad
            duration: 250
            from: 0
        }

        function setCurrentItemFocus() {
            var child
            if (currentIndex in idChildrenMap)
                child = idChildrenMap[currentIndex]
            if (child !== undefined && _expandIndex === -1)
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

    function animateToCurrentIndex() {
        var newContentY = flickable.contentY;
        var currentItemYPos = root.getItemPos(currentIndex)[1]
        if (currentItemYPos + cellHeight > flickable.contentY + flickable.height) {
            //move viewport to see current item bottom
            newContentY = Math.min(
                        currentItemYPos + cellHeight - flickable.height,
                        flickable.contentHeight - flickable.height)
        } else if (currentItemYPos < flickable.contentY) {
            //move viewport to see current item top
            newContentY = Math.max(currentItemYPos, 0)
        }

        if (newContentY !== flickable.contentY)
            animateFlickableContentY(newContentY)
    }

    onCurrentIndexChanged: {
        flickable.setCurrentItemFocus()
        animateToCurrentIndex()
    }

    Keys.onPressed: {
        var colCount = root.getNbItemsPerRow()

        var newIndex = -1
        if (event.key === Qt.Key_Right || event.matches(StandardKey.MoveToNextChar)) {
            if ((currentIndex + 1) % colCount !== 0) {//are we not at the end of line
                newIndex = Math.min(root.modelCount - 1, currentIndex + 1)
            }
        } else if (event.key === Qt.Key_Left || event.matches(StandardKey.MoveToPreviousChar)) {
            if (currentIndex % colCount !== 0) {//are we not at the begining of line
                newIndex = Math.max(0, currentIndex - 1)
            }
        } else if (event.key === Qt.Key_Down || event.matches(StandardKey.MoveToNextLine) ||event.matches(StandardKey.SelectNextLine) ) {
            if (Math.floor(currentIndex / colCount) !== Math.floor(root.modelCount / colCount)) { //we are not on the last line
                newIndex = Math.min(root.modelCount - 1, currentIndex + colCount)
            }
        } else if (event.key === Qt.Key_PageDown || event.matches(StandardKey.MoveToNextPage) ||event.matches(StandardKey.SelectNextPage)) {
            newIndex = Math.min(root.modelCount - 1, currentIndex + colCount * 5)
        } else if (event.key === Qt.Key_Up || event.matches(StandardKey.MoveToPreviousLine) ||event.matches(StandardKey.SelectPreviousLine)) {
            if (Math.floor(currentIndex / colCount) !== 0) { //we are not on the first line
                newIndex = Math.max(0, currentIndex - colCount)
            }
        } else if (event.key === Qt.Key_PageUp || event.matches(StandardKey.MoveToPreviousPage) ||event.matches(StandardKey.SelectPreviousPage)) {
            newIndex = Math.max(0, currentIndex - colCount * 5)
        }

        if (newIndex != -1 && newIndex != currentIndex) {
            event.accepted = true
            var oldIndex = currentIndex
            currentIndex = newIndex
            root.selectionUpdated(event.modifiers, oldIndex, newIndex)
            flickable.layout()
        }

        if (!event.accepted)
            defaultKeyAction(event, currentIndex)
    }

    Keys.onReleased: {
        if (event.matches(StandardKey.SelectAll)) {
            event.accepted = true
            root.selectAll()
        } else if (event.key === Qt.Key_Space || event.matches(StandardKey.InsertParagraphSeparator)) { //enter/return/space
            event.accepted = true
            root.actionAtIndex(currentIndex)
        }
    }
}
