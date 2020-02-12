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


NavigableFocusScope {
    id: gridview_id

    property int modelCount: view.count

    signal selectionUpdated( int keyModifiers, int oldIndex,int newIndex )
    signal selectAll()
    signal actionAtIndex( int index )

    //forward view properties
    property alias interactive: view.interactive
    property alias model: view.model
    property alias delegate: view.delegate

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

    Accessible.role: Accessible.Table

    function positionViewAtIndex(index, mode) {
        view.positionViewAtIndex(index, mode)
    }

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
            if (KeyHelper.matchRight(event)) {
                if ((currentIndex + 1) % _colCount !== 0) {//are we not at the end of line
                    newIndex = Math.min(gridview_id.modelCount - 1, currentIndex + 1)
                }
            } else if (KeyHelper.matchLeft(event)) {
                if (currentIndex % _colCount !== 0) {//are we not at the begining of line
                    newIndex = Math.max(0, currentIndex - 1)
                }
            } else if (KeyHelper.matchDown(event)) {
                if (Math.floor(currentIndex / _colCount) !== Math.floor(gridview_id.modelCount / _colCount)) { //we are not on the last line
                    newIndex = Math.min(gridview_id.modelCount - 1, currentIndex + _colCount)
                }
            } else if (KeyHelper.matchPageDown(event)) {
                newIndex = Math.min(gridview_id.modelCount - 1, currentIndex + _colCount * 5)
            } else if (KeyHelper.matchUp(event)) {
                if (Math.floor(currentIndex / _colCount) !== 0) { //we are not on the first line
                    newIndex = Math.max(0, currentIndex - _colCount)
                }
            } else if (KeyHelper.matchPageUp(event)) {
                newIndex = Math.max(0, currentIndex - _colCount * 5)
            } else if (KeyHelper.matchOk(event) || event.matches(StandardKey.SelectAll) ) {
                //these events are matched on release
                event.accepted = true
            }

            if (newIndex >= 0 && newIndex < modelCount && newIndex != currentIndex) {
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
            } else if (KeyHelper.matchOk(event)) {
                event.accepted = true
                actionAtIndex(currentIndex)
            }

        }
    }

}
