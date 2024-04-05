/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

import "qrc:///style/"

QtObject {
    id: root

    property Flickable view: null

    // if 'dragItem' is null, user must override property 'dragging' and 'dragPosProvider'
    property Item dragItem: null

    property bool dragging: dragItem?.visible

    property var dragPosProvider: function () {
        return root.view.mapFromItem(root.dragItem.parent,
                                     root.dragItem.x,
                                     root.dragItem.y)
    }

    property int orientation: (view && view.orientation !== undefined) ? view.orientation
                                                                       : Qt.Vertical
    property real margin: VLCStyle.dp(20)

    readonly property bool scrolling: animation.running // for convenience
    property alias scrollingDirection: animation.direction

    property alias velocity: animation.velocity

    enum Direction {
        Forward,
        Backward,
        None // not scrolling
    }

    readonly property SmoothedAnimation _animation: SmoothedAnimation {
        id: animation
        target: root.view
        property: root.orientation === Qt.Vertical ? "contentY"
                                                   : "contentX"
        reversingMode: SmoothedAnimation.Immediate
        easing.type: Easing.OutQuad

        velocity: VLCStyle.dp(200)
        maximumEasingTime: 1000

        readonly property Timer _timer: Timer {
            interval: 100
        }

        readonly property ScrollBar _scrollBar: (root.view) ? ((root.orientation === Qt.Vertical) ? root.view.ScrollBar.vertical
                                                                                                  : root.view.ScrollBar.horizontal)
                                                            : null

        readonly property int direction: {
            if (!root.view || !root.view.visible || !root.dragging)
                return ViewDragAutoScrollHandler.Direction.None

            const pos = root.dragPosProvider()

            let size, mark, atBeginning, atEnd
            if (root.orientation === Qt.Vertical) {
                size = root.view.height
                mark = pos.y

                atBeginning = root.view.atYBeginning
                atEnd = root.view.atYEnd
            } else {
                size = root.view.width
                mark = pos.x

                atBeginning = root.view.atXBeginning
                atEnd = root.view.atXEnd
            }

            if (size < root.margin * 2.5)
                return ViewDragAutoScrollHandler.Direction.None

            if (mark < root.margin)
                return !atBeginning ? ViewDragAutoScrollHandler.Direction.Backward
                                    : ViewDragAutoScrollHandler.Direction.None
            else if (mark > (size - root.margin))
                return !atEnd ? ViewDragAutoScrollHandler.Direction.Forward
                              : ViewDragAutoScrollHandler.Direction.None
            else
                return ViewDragAutoScrollHandler.Direction.None
        }

        Component.onCompleted: {
            // prevent direction to bounce
            _timer.triggered.connect(directionChangedHandler)
        }

        onDirectionChanged: {
            _timer.start()
        }

        function directionChangedHandler() {
            if (direction === ViewDragAutoScrollHandler.Direction.None) {
                running = false
            } else if (!running) {
                let _to

                if (direction === ViewDragAutoScrollHandler.Direction.Backward) {
                    _to = 0
                } else if (direction === ViewDragAutoScrollHandler.Direction.Forward) {
                    // FIXME: Is there a better way to calculate extents?
                    _to = ((root.orientation === Qt.Vertical) ? root.view.contentHeight - root.view.height
                                                              : root.view.contentWidth - root.view.width)
                    if (root.view.footerItem !== undefined
                            && root.view.footerItem
                            && root.view.footerPositioning === 0 /* inline positioning */)
                        _to += (root.orientation === Qt.Vertical) ? root.view.footerItem.height
                                                                  : root.view.footerItem.width
                    if (root.view.headerItem !== undefined
                            && root.view.headerItem
                            && root.view.headerPositioning === 0 /* inline positioning */)
                        _to += (root.orientation === Qt.Vertical) ? root.view.headerItem.height
                                                                  : root.view.headerItem.width
                }

                to = _to
                running = true
            }
        }

        readonly property Binding _scrollBarActiveBinding: Binding {
            when: !!animation._scrollBar && animation.running
            target: animation._scrollBar
            property: "active"
            value: true
        }
    }
}
