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
    id: gridview_id

    property int modelCount: 0

    signal selectionUpdated( int keyModifiers, int oldIndex,int newIndex )
    signal selectAll()
    signal actionAtIndex( int index )

    //compute a delta that can be applied to grid elements to obtain an horizontal distribution
    function shiftX( index ) {
        var rightSpace = width - (view._colCount * view.cellWidth)
        return ((index % view._colCount) + 1) * (rightSpace / (view._colCount + 1))
    }

    //forward view properties
    property alias interactive: view.interactive
    property alias model: view.model

    property alias cellWidth: view.cellWidth
    property alias cellHeight: view.cellHeight

    property alias originX: view.originX
    property alias originY: view.originY

    property alias contentX: view.contentX
    property alias contentY:  view.contentY
    property alias contentHeight: view.contentHeight

    property alias footer: view.footer
    property alias footerItem: view.footerItem
    property alias header: view.header
    property alias headerItem: view.headerItem

    property alias currentIndex: view.currentIndex

    GridView {
        id: view

        anchors.fill: parent

        clip: true
        ScrollBar.vertical: ScrollBar { }

        focus: true

        //key navigation is reimplemented for item selection
        keyNavigationEnabled: false

        property int _colCount: Math.floor(width / cellWidth)

        Keys.onPressed: {
            var newIndex = -1
            if (event.key === Qt.Key_Right || event.matches(StandardKey.MoveToNextChar)) {
                if ((currentIndex + 1) % _colCount !== 0) {//are we not at the end of line
                    newIndex = Math.min(gridview_id.modelCount - 1, currentIndex + 1)
                }
            } else if (event.key === Qt.Key_Left || event.matches(StandardKey.MoveToPreviousChar)) {
                if (currentIndex % _colCount !== 0) {//are we not at the begining of line
                    newIndex = Math.max(0, currentIndex - 1)
                }
            } else if (event.key === Qt.Key_Down || event.matches(StandardKey.MoveToNextLine) ||event.matches(StandardKey.SelectNextLine) ) {
                if (Math.floor(currentIndex / _colCount) !== Math.floor(gridview_id.modelCount / _colCount)) { //we are not on the last line
                    newIndex = Math.min(gridview_id.modelCount - 1, currentIndex + _colCount)
                }
            } else if (event.key === Qt.Key_PageDown || event.matches(StandardKey.MoveToNextPage) ||event.matches(StandardKey.SelectNextPage)) {
                newIndex = Math.min(gridview_id.modelCount - 1, currentIndex + _colCount * 5)
            } else if (event.key === Qt.Key_Up || event.matches(StandardKey.MoveToPreviousLine) ||event.matches(StandardKey.SelectPreviousLine)) {
                if (Math.floor(currentIndex / _colCount) !== 0) { //we are not on the first line
                    newIndex = Math.max(0, currentIndex - _colCount)
                }
            } else if (event.key === Qt.Key_PageUp || event.matches(StandardKey.MoveToPreviousPage) ||event.matches(StandardKey.SelectPreviousPage)) {
                newIndex = Math.max(0, currentIndex - _colCount * 5)
            }

            if (newIndex != -1 && newIndex != currentIndex) {
                var oldIndex = currentIndex
                currentIndex = newIndex
                event.accepted = true
                selectionUpdated(event.modifiers, oldIndex, newIndex)
            }

            if (!event.accepted)
                defaultKeyAction(event, currentIndex)
        }

        Keys.onReleased: {
            if (event.matches(StandardKey.SelectAll)) {
                event.accepted = true
                selectAll()
            } else if (event.key === Qt.Key_Space || event.matches(StandardKey.InsertParagraphSeparator)) { //enter/return/space
                event.accepted = true
                actionAtIndex(currentIndex)
            }

        }
    }

}
