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
    property alias modelTop: topView.model
    property alias modelBottom: bottomView.model
    property int modelCount: 0

    property alias delegateTop: topView.delegate
    property alias delegateBottom: bottomView.delegate

    property int currentIndex: 0

    /// the id of the item to be expanded
    property int expandIndex: -1
    //delegate to display the extended item
    property Component expandDelegate: Item{}

    //signals emitted when selected items is updated from keyboard
    signal selectionUpdated( int keyModifiers, int oldIndex,int newIndex )
    signal selectAll()
    signal actionAtIndex(int index)

    property alias contentY: flickable.contentY
    property alias interactive: flickable.interactive
    property alias clip: flickable.clip
    property alias contentHeight: flickable.contentHeight
    property alias contentWidth: flickable.contentWidth

    //compute a delta that can be applied to grid elements to obtain an horizontal distribution
    function shiftX( index ) {
        var rightSpace = width - (flickable._colCount * root.cellWidth)
        return ((index % flickable._colCount) + 1) * (rightSpace / (flickable._colCount + 1))
    }

    Flickable {
        id: flickable

        anchors.fill: parent
        clip: true
        //ScrollBar.vertical: ScrollBar { }

        //disable bound behaviors to avoid visual artifacts around the expand delegate
        boundsBehavior: Flickable.StopAtBounds


        // number of elements per row, for internal computation
        property int _colCount: Math.floor(width / root.cellWidth)
        property int topContentY: flickable.contentY
        property int bottomContentY: flickable.contentY + flickable.height

        property int _oldExpandIndex: -1
        property bool _expandActive: root.expandIndex !== -1

        function _rowOfIndex( index ) {
            return Math.ceil( (index + 1) / flickable._colCount) - 1
        }

        //from KeyNavigableGridView
        function _yOfIndex( index ) {
            if ( root.expandIndex != -1
                 && (index > (flickable._rowOfIndex( root.expandIndex ) + 1) * flickable._colCount )  )
                return flickable._rowOfIndex(root.currentIndex) * root.cellHeight + expandItem.height
            else
                return flickable._rowOfIndex(root.currentIndex) * root.cellHeight
        }

        Connections {
            target: root
            onExpandIndexChanged: {
                flickable._updateExpandPosition()
            }
        }
        on_ColCountChanged: _updateExpandPosition()
        function _updateExpandPosition() {
            expandItem.y = root.cellHeight * (Math.floor(root.expandIndex / flickable._colCount) + 1)
            _oldExpandIndex = root.expandIndex
        }


        states: [
            State {
                name: "-expand"
                when: ! flickable._expandActive
                PropertyChanges {
                    target: flickable
                    topContentY: flickable.contentY
                    contentHeight: root.cellHeight * Math.ceil(root.modelCount / flickable._colCount)
                }
            },
            State {
                name: "+expand"
                when: flickable._expandActive
                PropertyChanges {
                    target: flickable
                    topContentY: flickable.contentY
                    contentHeight: root.cellHeight * Math.ceil(root.modelCount / flickable._colCount) + expandItem.height
                }
            }
        ]

        //Gridview visible above the expanded item
        GridView {
            id: topView
            clip: true
            interactive: false

            focus: !flickable._expandActive

            highlightFollowsCurrentItem: false
            currentIndex: root.currentIndex

            cellWidth: root.cellWidth
            cellHeight: root.cellHeight

            anchors.left: parent.left
            anchors.right: parent.right

            states: [
                //expand is unactive or below the view
                State {
                    name: "visible_noexpand"
                    when: !flickable._expandActive || expandItem.y >= flickable.bottomContentY
                    PropertyChanges {
                        target: topView
                        y: flickable.topContentY

                        height:flickable.height
                        //FIXME: should we add + originY? this seemed to fix some issues but has performance impacts
                        //OriginY, seems to change randomly on grid resize
                        contentY: flickable.topContentY
                        visible: true
                        enabled: true
                    }
                },
                //expand is active and within the view
                State {
                    name: "visible_expand"
                    when: flickable._expandActive && (expandItem.y >= flickable.contentY) && (expandItem.y < flickable.bottomContentY)
                    PropertyChanges {
                        target: topView
                        y: flickable.contentY
                        height: expandItem.y - flickable.topContentY
                        //FIXME: should we add + originY? this seemed to fix some issues but has performance impacts
                        //OriginY, seems to change randomly on grid resize
                        contentY: flickable.topContentY
                        visible: true
                        enabled: true
                    }
                },
                //expand is active and above the view
                State {
                    name: "hidden"
                    when: flickable._expandActive && (expandItem.y < flickable.contentY)
                    PropertyChanges {
                        target: topView
                        visible: false
                        enabled: false
                        height: 1
                        y: 0
                        contentY: 0
                    }
                }
            ]
        }

        //Expanded item view
        Loader {
            id: expandItem
            sourceComponent: root.expandDelegate
            active: flickable._expandActive
            focus: flickable._expandActive
            y: 0 //updated by _updateExpandPosition
            property int bottomY: y + height
            anchors.left: parent.left
            anchors.right: parent.right
        }

        //Gridview visible below the expand item
        GridView {
            id: bottomView
            clip: true
            interactive: false
            highlightFollowsCurrentItem: false
            //don't bind the current index, otherwise it reposition the contentY on it's own
            //currentIndex: root.currentIndex

            cellWidth: root.cellWidth
            cellHeight: root.cellHeight

            anchors.left: parent.left
            anchors.right: parent.right

            property bool hidden: !flickable._expandActive
                                  || (expandItem.bottomY >= flickable.bottomContentY)
                                  || flickable._rowOfIndex(root.expandIndex) === flickable._rowOfIndex(root.modelCount - 1)
            states: [
                //expand is visible and above the view
                State {
                    name: "visible_noexpand"
                    when: !bottomView.hidden && (expandItem.bottomY < flickable.contentY)
                    PropertyChanges {
                        target: bottomView
                        enabled: true
                        visible: true
                        height: flickable.height
                        y: flickable.contentY
                        contentY: expandItem.y + flickable.contentY - expandItem.bottomY
                    }
                },
                //expand is visible and within the view
                State {
                    name: "visible_expand"
                    when: !bottomView.hidden && (expandItem.bottomY > flickable.contentY) && (expandItem.bottomY < flickable.bottomContentY)
                    PropertyChanges {
                        target: bottomView
                        enabled: true
                        visible: true
                        height: Math.min(flickable.bottomContentY - expandItem.bottomY, root.cellHeight * ( flickable._rowOfIndex(root.modelCount - 1) - flickable._rowOfIndex(root.expandIndex)))
                        y: expandItem.bottomY
                        contentY:  expandItem.y
                    }
                },
                //expand is inactive or below the view
                State {
                    name: "hidden"
                    when: bottomView.hidden
                    PropertyChanges {
                        target: bottomView
                        enabled: false
                        visible: false
                        height: 1
                        y: 0
                        contentY: 0
                    }
                }
            ]
        }
    }

    onCurrentIndexChanged: {
        if ( flickable._yOfIndex(root.currentIndex) + root.cellHeight > flickable.bottomContentY) {
            //move viewport to see expanded item bottom
            flickable.contentY = Math.min(
                        flickable._yOfIndex(root.currentIndex) + root.cellHeight - flickable.height, // + flickable.marginBottom,
                        flickable.contentHeight - flickable.height)
        } else if (flickable._yOfIndex(root.currentIndex)  < flickable.contentY) {
            //move viewport to see expanded item at top
            flickable.contentY = Math.max(
                        flickable._yOfIndex(root.currentIndex) - root.marginTop,
                        0)
        }
    }

    onExpandIndexChanged: {
        if (expandIndex != -1) {
            //move viewport to see expanded item at top
            flickable.contentY = Math.max( (flickable._rowOfIndex( root.expandIndex ) * root.cellHeight) - root.marginTop, 0)
        }
    }

    Keys.onPressed: {
        var newIndex = -1
        if (event.key === Qt.Key_Right || event.matches(StandardKey.MoveToNextChar)) {
            if ((root.currentIndex + 1) % flickable._colCount !== 0) {//are we not at the end of line
                newIndex = Math.min(root.modelCount - 1, root.currentIndex + 1)
            }
        } else if (event.key === Qt.Key_Left || event.matches(StandardKey.MoveToPreviousChar)) {
            if (root.currentIndex % flickable._colCount !== 0) {//are we not at the begining of line
                newIndex = Math.max(0, root.currentIndex - 1)
            }
        } else if (event.key === Qt.Key_Down || event.matches(StandardKey.MoveToNextLine) ||event.matches(StandardKey.SelectNextLine) ) {
            if (Math.floor(root.currentIndex / flickable._colCount) !== Math.floor(root.modelCount / flickable._colCount)) { //we are not on the last line
                newIndex = Math.min(root.modelCount - 1, root.currentIndex + flickable._colCount)
            }
        } else if (event.key === Qt.Key_PageDown || event.matches(StandardKey.MoveToNextPage) ||event.matches(StandardKey.SelectNextPage)) {
            newIndex = Math.min(root.modelCount - 1, root.currentIndex + flickable._colCount * 5)
        } else if (event.key === Qt.Key_Up || event.matches(StandardKey.MoveToPreviousLine) ||event.matches(StandardKey.SelectPreviousLine)) {
             if (Math.floor(root.currentIndex / flickable._colCount) !== 0) { //we are not on the first line
                newIndex = Math.max(0, root.currentIndex - flickable._colCount)
             }
        } else if (event.key === Qt.Key_PageUp || event.matches(StandardKey.MoveToPreviousPage) ||event.matches(StandardKey.SelectPreviousPage)) {
            newIndex = Math.max(0, root.currentIndex - flickable._colCount * 5)
        }

        if (newIndex != -1 && newIndex != root.currentIndex) {
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
        } else if (event.key === Qt.Key_Space || event.matches(StandardKey.InsertParagraphSeparator)) { //enter/return/space
            event.accepted = true
            root.actionAtIndex(root.currentIndex)
        }
    }
}
