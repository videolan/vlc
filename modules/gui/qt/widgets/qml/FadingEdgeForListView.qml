/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

import VLC.Style
import VLC.Util

FadingEdge {
    id: root

    required property ListView listView

    sourceItem: listView.contentItem

    beginningMargin: listView.displayMarginBeginning
    endMargin: listView.displayMarginEnd
    orientation: (listView.orientation === ListView.Vertical) ? Qt.Vertical : Qt.Horizontal

    sourceX: listView.contentX
    sourceY: listView.contentY

    fadeSize: delegateItem ? (orientation === Qt.Vertical ? delegateItem.height
                                                          : delegateItem.width) / 2
                           : (VLCStyle.margin_large * 2)

    readonly property bool transitionsRunning: (listView.add?.running ||
                                                listView.addDisplaced?.running ||
                                                listView.displaced?.running ||
                                                listView.move?.running ||
                                                listView.moveDisplaced?.running ||
                                                listView.populate?.running ||
                                                listView.remove?.running ||
                                                listView.removeDisplaced?.running) ?? false

    // FIXME: Delegate with variable size
    readonly property Item delegateItem: (listView.count > 0) ? listView.itemAtIndex(0) : null


    readonly property bool _fadeRectEnoughSize: (orientation === Qt.Vertical ? listView.height
                                                                             : listView.width) > (fadeSize * 2 + VLCStyle.dp(25))

    readonly property rect _currentItemMappedRect: listView.currentItem ? Qt.rect(listView.currentItem.x - sourceX,
                                                                                  listView.currentItem.y - sourceY,
                                                                                  listView.currentItem.width,
                                                                                  listView.currentItem.height)
                                                                        : Qt.rect(-1, -1, -1, -1)

    readonly property bool _disableBeginningFade: (!!listView.headerItem && (listView.headerPositioning !== ListView.InlineHeader)) ||
                                                  !_fadeRectEnoughSize ||
                                                  (orientation === Qt.Vertical ? listView.atYBeginning
                                                                               : listView.atXBeginning) ||
                                                  !Helpers.itemIntersects(beginningArea, _currentItemMappedRect)

    readonly property bool _disableEndFade: (!!listView.footerItem && (listView.footerPositioning !== ListView.InlineFooter)) ||
                                            !_fadeRectEnoughSize ||
                                            (orientation === Qt.Vertical ? listView.atYEnd
                                                                         : listView.atXEnd) ||
                                            !Helpers.itemIntersects(endArea, _currentItemMappedRect)

    Binding on enableBeginningFade {
        // This explicit binding is to override `enableBeginningFade` when it is not feasible to have fading edge.
        when: root._disableBeginningFade || beginningHoverHandler.hovered
        value: false
    }

    Binding on enableEndFade {
        // This explicit binding is to override `enableEndFade` when it is not feasible to have fading edge.
        when: root._disableEndFade || endHoverHandler.hovered
        value: false
    }

    Item {
        id: beginningArea

        z: 99
        parent: root.listView

        visible: !root._disableBeginningFade

        anchors {
            top: parent.top
            left: parent.left

            topMargin: (orientation === Qt.Vertical) ? -beginningMargin : undefined
            leftMargin: (orientation === Qt.Horizontal) ? -beginningMargin : undefined

            bottom: (orientation === Qt.Horizontal) ? parent.bottom : undefined
            right: (orientation === Qt.Vertical) ? parent.right : undefined
        }

        implicitWidth: fadeSize
        implicitHeight: fadeSize

        HoverHandler {
            id: beginningHoverHandler

            grabPermissions: PointerHandler.ApprovesTakeOverByAnything
        }
    }

    Item {
        id: endArea

        z: 99
        parent: root.listView

        visible: !root._disableEndFade

        anchors {
            bottom: parent.bottom
            right: parent.right

            bottomMargin: (orientation === Qt.Vertical) ? -endMargin : undefined
            rightMargin: (orientation === Qt.Horizontal) ? -endMargin : undefined

            top: (orientation === Qt.Horizontal) ? parent.top : undefined
            left: (orientation === Qt.Vertical) ? parent.left : undefined
        }

        implicitWidth: fadeSize
        implicitHeight: fadeSize

        HoverHandler {
            id: endHoverHandler

            grabPermissions: PointerHandler.ApprovesTakeOverByAnything
        }
    }
}
