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

import "qrc:///style/"
import "qrc:///util/KeyHelper.js" as KeyHelper

NavigableFocusScope {
    id: listview_id

    property int modelCount: view.count

    signal selectionUpdated( int keyModifiers, int oldIndex,int newIndex )
    signal selectAll()
    signal actionAtIndex( int index )

    //forward view properties
    property alias spacing: view.spacing
    property alias interactive: view.interactive
    property alias model: view.model
    property alias delegate: view.delegate

    property alias originX: view.originX
    property alias originY: view.originY

    property alias contentX: view.contentX
    property alias contentY:  view.contentY
    property alias contentHeight: view.contentHeight
    property alias contentWidth: view.contentWidth

    property alias footer: view.footer
    property alias footerItem: view.footerItem
    property alias header: view.header
    property alias headerItem: view.headerItem
    property alias headerPositioning: view.headerPositioning

    property alias currentIndex: view.currentIndex
    property alias currentItem: view.currentItem

    property alias highlightMoveVelocity: view.highlightMoveVelocity

    property alias section: view.section
    property alias orientation: view.orientation

    property alias add: view.add
    property alias displaced: view.displaced

    property int highlightMargin: VLCStyle.margin_large

    property int scrollBarWidth: scroll_id.visible ? scroll_id.width : 0

    Accessible.role: Accessible.List

    function nextPage() {
        view.contentX += (Math.min(view.width, (view.contentWidth - view.width - view.contentX ) ))
    }
    function prevPage() {
        view.contentX -= Math.min(view.width,view.contentX )
    }

    function positionViewAtIndex(index, mode) {
        view.positionViewAtIndex(index, mode)
    }

    function itemAtIndex(index) {
        return view.itemAtIndex(index)
    }

    Component {
        id: sectionHeading

        Column {
            width: parent.width

            Text {
                text: section
                font.pixelSize: VLCStyle.fontSize_xlarge
                color: VLCStyle.colors.accent
            }

            Rectangle {
                width: parent.width
                height: 1
                color: VLCStyle.colors.textDisabled
            }
        }
    }


    Connections {
        target: view.headerItem
        onFocusChanged: {
            if (!headerItem.focus) {
                view.currentItem.focus = true
            }
        }
    }

    ListView {
        id: view
        anchors.fill: parent
        //key navigation is reimplemented for item selection
        keyNavigationEnabled: false

        preferredHighlightBegin : (view.orientation === ListView.Vertical)
                                    ? highlightMargin + (headerItem ? headerItem.height : 0)
                                    : highlightMargin
        preferredHighlightEnd : (view.orientation === ListView.Vertical)
                                    ? height - highlightMargin
                                    : width - highlightMargin
        highlightRangeMode: ListView.ApplyRange

        focus: true

        clip: true
        ScrollBar.vertical: ScrollBar { id: scroll_id }
        ScrollBar.horizontal: ScrollBar { }

        highlightMoveDuration: 300 //ms
        highlightMoveVelocity: 1000 //px/s

        section.property: ""
        section.criteria: ViewSection.FullString
        section.delegate: sectionHeading

        Connections {
            target: view.currentItem
            ignoreUnknownSignals: true
            onActionRight: listview_id.navigationRight(currentIndex)
            onActionLeft: listview_id.navigationLeft(currentIndex)
            onActionDown: {
                if ( currentIndex !== modelCount - 1 ) {
                    var newIndex = currentIndex + 1
                    var oldIndex = currentIndex
                    currentIndex = newIndex
                    selectionUpdated(0, oldIndex, newIndex)
                } else {
                    root.navigationDown(currentIndex)
                }
            }
            onActionUp: {
                if ( currentIndex !== 0 ) {
                    var newIndex = currentIndex - 1
                    var oldIndex = currentIndex
                    currentIndex = newIndex
                    selectionUpdated(0, oldIndex, newIndex)
                } else {
                    root.navigationUp(currentIndex)
                }
            }
        }

        Keys.onPressed: {
            var newIndex = -1

            if (orientation === ListView.Vertical)
            {
                if ( KeyHelper.matchDown(event) ) {
                    if (currentIndex !== modelCount - 1 )
                        newIndex = currentIndex + 1
                } else if ( KeyHelper.matchPageDown(event) ) {
                    newIndex = Math.min(modelCount - 1, currentIndex + 10)
                } else if ( KeyHelper.matchUp(event) ) {
                    if ( currentIndex !== 0 )
                        newIndex = currentIndex - 1
                } else if ( KeyHelper.matchPageUp(event) ) {
                    newIndex = Math.max(0, currentIndex - 10)
                }
            }else{
                if ( KeyHelper.matchRight(event) ) {
                    if (currentIndex !== modelCount - 1 )
                        newIndex = currentIndex + 1
                }
                else if ( KeyHelper.matchPageDown(event) ) {
                    newIndex = Math.min(modelCount - 1, currentIndex + 10)
                } else if ( KeyHelper.matchLeft(event) ) {
                    if ( currentIndex !== 0 )
                        newIndex = currentIndex - 1
                } else if ( KeyHelper.matchPageUp(event) ) {
                    newIndex = Math.max(0, currentIndex - 10)
                }
            }

            if (KeyHelper.matchOk(event) || event.matches(StandardKey.SelectAll) ) {
                //these events are matched on release
                event.accepted = true
            }

            if (newIndex >= 0 && newIndex < modelCount) {
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
            } else if ( KeyHelper.matchOk(event) ) { //enter/return/space
                event.accepted = true
                actionAtIndex(currentIndex)
            }
        }
    }

    RoundButton{
        id: leftBtn
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        text:"<"
        onClicked: listview_id.prevPage()
        visible: view.orientation === ListView.Horizontal && view.contentX > 0
    }


    RoundButton{
        id: rightBtn
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        text:">"
        onClicked: listview_id.nextPage()
        visible: view.orientation === ListView.Horizontal && (view.contentWidth - view.width - view.contentX) > 0
    }
}
