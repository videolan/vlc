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
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///util/" as Util
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///util/" as Util

FocusScope {
    id: listview_id

    // Properties

    property alias modelCount: view.count

    property alias listView: view

    property int fadeSize: view.delegateItem ? (orientation === Qt.Vertical ? view.delegateItem.height
                                                                            : view.delegateItem.width) / 2
                                             : (VLCStyle.margin_large * 2)

    property var fadeColor: undefined // fading will only work when fade color is defined

    // NOTE: We want buttons to be centered vertically but configurable.
    property int buttonMargin: height / 2 - buttonLeft.height / 2

    readonly property int scrollBarWidth: scroll_id.visible ? scroll_id.width : 0

    property bool keyNavigationWraps : false

    // Private

    property int _currentFocusReason: Qt.OtherFocusReason

    // Aliases

    //forward view properties
    property alias spacing: view.spacing
    property alias interactive: view.interactive
    property alias model: view.model
    property alias delegate: view.delegate

    property alias leftMargin: view.leftMargin
    property alias rightMargin: view.rightMargin
    property alias topMargin: view.topMargin
    property alias bottomMargin: view.bottomMargin

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
    property alias currentSection: view.currentSection
    property alias orientation: view.orientation

    property alias add: view.add
    property alias displaced: view.displaced

    property alias displayMarginBeginning: view.displayMarginBeginning
    property alias displayMarginEnd: view.displayMarginEnd

    property alias flickableDirection: view.flickableDirection
    property alias listScrollBar: scroll_id

    property alias buttonLeft: buttonLeft
    property alias buttonRight: buttonRight

    property alias dragAutoScrollDragItem: dragAutoScrollHandler.dragItem
    property alias dragAutoScrollMargin: dragAutoScrollHandler.margin
    property alias dragAutoScrolling: dragAutoScrollHandler.scrolling

    // Signals

    signal selectionUpdated(int keyModifiers, int oldIndex, int newIndex)

    signal selectAll()

    signal actionAtIndex(int index)

    signal deselectAll()

    signal showContextMenu(point globalPos)

    // Settings

    Accessible.role: Accessible.List

    // Events

    onCurrentItemChanged: {
        if (_currentFocusReason === Qt.OtherFocusReason)
            return;

        // NOTE: We make sure the view has active focus before enforcing it on the item.
        if (view.activeFocus && currentItem)
            Helpers.enforceFocus(currentItem, _currentFocusReason);

        _currentFocusReason = Qt.OtherFocusReason;
    }

    // Functions

    function setCurrentItemFocus(reason) {
        if (!model || model.count === 0) {
            // NOTE: By default we want the focus on the flickable.
            view.forceActiveFocus(reason);

            // NOTE: Saving the focus reason for later.
            _currentFocusReason = reason;

            return;
        }

        if (currentIndex === -1)
            currentIndex = 0;

        positionViewAtIndex(currentIndex, ItemView.Contain);

        Helpers.enforceFocus(currentItem, reason);
    }

    function nextPage() {
        view.contentX += (Math.min(view.width, (view.contentWidth - view.width - view.contentX)))
    }
    function prevPage() {
        view.contentX -= Math.min(view.width,view.contentX - view.originX)
    }

    function positionViewAtIndex(index, mode) {
        view.positionViewAtIndex(index, mode)
    }

    function itemAtIndex(index) {
        return view.itemAtIndex(index)
    }

    // Events

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

    // Connections

    // FIXME: This is probably not useful anymore.
    Connections {
        target: view.headerItem
        onFocusChanged: {
            if (!headerItem.focus) {
                currentItem.focus = true
            }
        }
    }

    // Children

    ListView {
        id: view

        anchors.fill: parent

        focus: true

        //key navigation is reimplemented for item selection
        keyNavigationEnabled: false

        ScrollBar.vertical: ScrollBar { id: scroll_id }
        ScrollBar.horizontal: ScrollBar { visible: view.contentWidth > view.width }

        highlightMoveDuration: 300 //ms
        highlightMoveVelocity: 1000 //px/s

        section.property: ""
        section.criteria: ViewSection.FullString
        section.delegate: sectionHeading

        // TODO: Use itemAtIndex(0) Qt >= 5.13
        // FIXME: Delegate with variable size
        readonly property Item delegateItem: view.contentItem.children.length > 0 ? view.contentItem.children[view.contentItem.children.length - 1]
                                                                                  : null

        readonly property bool transitionsRunning: ((view.add ? view.add.running : false) ||
                                                    (view.addDisplaced ? view.addDisplaced.running : false) ||
                                                    (view.populate ? view.populate.running : false) ||
                                                    (view.remove ? view.remove.running : false) ||
                                                    (view.removeDisplaced ? view.removeDisplaced.running : false))

        readonly property Item firstVisibleItem: {
            if (transitionsRunning || !delegateItem)
                null

            var margin = -listview_id.displayMarginBeginning
            if (orientation === Qt.Vertical) {
                if (headerItem && headerItem.visible && headerPositioning === ListView.OverlayHeader)
                    margin += headerItem.height

                itemAt(contentX + (delegateItem.x + delegateItem.width / 2), contentY + margin)
            } else {
                if (headerItem && headerItem.visible && headerPositioning === ListView.OverlayHeader)
                    margin += headerItem.width

                itemAt(contentX + margin, contentY + (delegateItem.y + delegateItem.height / 2))
            }
        }

        readonly property Item lastVisibleItem: {
            if (transitionsRunning || !delegateItem)
                null

            var margin = -listview_id.displayMarginEnd
            if (orientation === Qt.Vertical) {
                if (footerItem && footerItem.visible && footerPositioning === ListView.OverlayFooter)
                    margin += footerItem.height

                itemAt(contentX + (delegateItem.x + delegateItem.width / 2), contentY + height - margin - 1)
            } else {
                if (footerItem && footerItem.visible && footerPositioning === ListView.OverlayFooter)
                    margin += footerItem.width

                itemAt(contentX + width - margin - 1, contentY + (delegateItem.y + delegateItem.height / 2))
            }
        }

        MouseEventFilter {
            target: view

            onMouseButtonPress: {
                if (buttons & (Qt.LeftButton | Qt.RightButton)) {
                    Helpers.enforceFocus(view, Qt.MouseFocusReason)

                    if (!(modifiers & (Qt.ShiftModifier | Qt.ControlModifier))) {
                        listview_id.deselectAll()
                    }
                }
            }

            onMouseButtonRelease: {
                if (button & Qt.RightButton) {
                    listview_id.showContextMenu(globalPos)
                }
            }
        }

        Util.ViewDragAutoScrollHandler {
            id: dragAutoScrollHandler

            view: view
        }

        // NOTE: We always want a valid 'currentIndex' by default.
        onCountChanged: if (count && currentIndex === -1) currentIndex = 0

        Util.FlickableScrollHandler { }

        Keys.onPressed: {
            var newIndex = -1

            if (orientation === ListView.Vertical)
            {
                if ( KeyHelper.matchDown(event) ) {
                    if (currentIndex !== modelCount - 1 )
                        newIndex = currentIndex + 1
                    else if ( listview_id.keyNavigationWraps )
                        newIndex = 0
                } else if ( KeyHelper.matchPageDown(event) ) {
                    newIndex = Math.min(modelCount - 1, currentIndex + 10)
                } else if ( KeyHelper.matchUp(event) ) {
                    if ( currentIndex !== 0 )
                        newIndex = currentIndex - 1
                    else if ( listview_id.keyNavigationWraps )
                        newIndex = modelCount - 1
                } else if ( KeyHelper.matchPageUp(event) ) {
                    newIndex = Math.max(0, currentIndex - 10)
                }
            }else{
                if ( KeyHelper.matchRight(event) ) {
                    if (currentIndex !== modelCount - 1 )
                        newIndex = currentIndex + 1
                    else if ( listview_id.keyNavigationWraps )
                        newIndex = 0
                }
                else if ( KeyHelper.matchPageDown(event) ) {
                    newIndex = Math.min(modelCount - 1, currentIndex + 10)
                } else if ( KeyHelper.matchLeft(event) ) {
                    if ( currentIndex !== 0 )
                        newIndex = currentIndex - 1
                    else if ( listview_id.keyNavigationWraps )
                        newIndex = modelCount - 1
                } else if ( KeyHelper.matchPageUp(event) ) {
                    newIndex = Math.max(0, currentIndex - 10)
                }
            }

            if (KeyHelper.matchOk(event) || event.matches(StandardKey.SelectAll) ) {
                //these events are matched on release
                event.accepted = true
            }

            var oldIndex = currentIndex
            if (newIndex >= 0 && newIndex < modelCount && newIndex !== oldIndex) {
                event.accepted = true;

                currentIndex = newIndex;

                selectionUpdated(event.modifiers, oldIndex, newIndex);

                // NOTE: We make sure we have the proper visual focus on components.
                if (oldIndex < currentIndex)
                    Helpers.enforceFocus(currentItem, Qt.TabFocusReason);
                else
                    Helpers.enforceFocus(currentItem, Qt.BacktabFocusReason);
            }

            if (!event.accepted) {
                listview_id.Navigation.defaultKeyAction(event)
            }
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

        readonly property bool _fadeRectEnoughSize: (view.orientation === Qt.Vertical ? view.height : view.width) > (fadeSize * 2 + VLCStyle.dp(25))

        // TODO: Make fade rectangle inline component when Qt >= 5.15
        LinearGradient {
            id: fadeRectStart

            anchors {
                top: parent.top
                left: parent.left
                right: view.orientation === Qt.Vertical ? parent.right : undefined
                bottom: view.orientation === Qt.Horizontal ? view.bottom : undefined
                topMargin: view.orientation === Qt.Vertical ? ((view.headerItem &&
                                                                view.headerItem.visible &&
                                                                (view.headerPositioning === ListView.OverlayHeader)) ? view.headerItem.height
                                                                                                                     : 0) - listview_id.displayMarginBeginning
                                                            : 0
                leftMargin: view.orientation === Qt.Horizontal ? ((view.headerItem &&
                                                                   view.headerItem.visible &&
                                                                   (view.headerPositioning === ListView.OverlayHeader)) ? view.headerItem.width
                                                                                                                        : 0) - listview_id.displayMarginBeginning
                                                               : 0
            }

            implicitHeight: fadeSize
            implicitWidth: fadeSize

            visible: (opacity !== 0.0)
            opacity: 0.0

            readonly property bool requestShow: !view.firstVisibleItem ||
                                                (!view.firstVisibleItem.activeFocus &&
                                                 // TODO: Qt >5.12 use HoverHandler within the fade:
                                                 !Helpers.get(view.firstVisibleItem, "hovered", false)) &&
                                                (dragAutoScrollHandler.scrollingDirection !== Util.ViewDragAutoScrollHandler.Backward)

            state: (!!listview_id.fadeColor &&
                    view._fadeRectEnoughSize &&
                    requestShow &&
                    (orientation === ListView.Vertical ? !view.atYBeginning
                                                       : !view.atXBeginning)) ? "shown"
                                                                              : ""

            states: State {
                name: "shown"
                PropertyChanges {
                    target: fadeRectStart
                    opacity: 1.0
                }
            }

            transitions: Transition {
                from: ""; to: "shown"
                reversible: true

                NumberAnimation {
                    property: "opacity"
                    duration: VLCStyle.duration_fast
                    easing.type: Easing.InOutSine
                }
            }

            start: Qt.point(0, 0)

            end: {
                if (view.orientation === ListView.Vertical) {
                    return Qt.point(0, fadeRectStart.height)
                } else {
                    return Qt.point(fadeRectStart.width, 0)
                }
            }

            gradient: Gradient {
                GradientStop { position: 0.0; color: !!fadeColor ? fadeColor : "transparent" }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        LinearGradient {
            id: fadeRectEnd

            anchors {
                top: view.orientation === Qt.Horizontal ? parent.top : undefined
                left: view.orientation === Qt.Vertical ? parent.left : undefined
                right: parent.right
                bottom: parent.bottom

                bottomMargin: view.orientation === Qt.Vertical ? ((view.footerItem &&
                                                                   view.footerItem.visible &&
                                                                   (view.footerPositioning === ListView.OverlayFooter)) ? view.footerItem.height
                                                                                                                        : 0) - listview_id.displayMarginEnd
                                                               : 0
                rightMargin: view.orientation === Qt.Horizontal ? ((view.footerItem &&
                                                                    view.footerItem.visible &&
                                                                    (view.headerPositioning === ListView.OverlayFooter)) ? view.footerItem.width
                                                                                                                         : 0) - listview_id.displayMarginEnd
                                                                : 0
            }

            implicitHeight: fadeSize
            implicitWidth: fadeSize

            visible: opacity !== 0.0
            opacity: 0.0

            readonly property bool requestShow: !view.lastVisibleItem ||
                                                (!view.lastVisibleItem.activeFocus &&
                                                 // TODO: Qt >5.12 use HoverHandler within the fade:
                                                 !Helpers.get(view.lastVisibleItem, "hovered", false)) &&
                                                (dragAutoScrollHandler.scrollingDirection !== Util.ViewDragAutoScrollHandler.Forward)

            state: (!!listview_id.fadeColor &&
                    view._fadeRectEnoughSize &&
                    requestShow &&
                    (orientation === ListView.Vertical ? !view.atYEnd
                                                       : !view.atXEnd)) ? "shown"
                                                                        : ""

            states: State {
                name: "shown"
                PropertyChanges {
                    target: fadeRectEnd
                    opacity: 1.0
                }
            }

            transitions: Transition {
                from: ""; to: "shown"
                reversible: true

                NumberAnimation {
                    property: "opacity"
                    duration: VLCStyle.duration_fast
                    easing.type: Easing.InOutSine
                }
            }

            start: Qt.point(0, 0)

            end: {
                if (view.orientation === ListView.Vertical) {
                    return Qt.point(0, fadeRectEnd.height)
                } else {
                    return Qt.point(fadeRectEnd.width, 0)
                }
            }

            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: !!fadeColor ? fadeColor : "transparent" }
            }
        }
    }

    // FIXME: We probably need to upgrade these RoundButton(s) eventually. And we probably need
    //        to have some kind of animation when switching pages.

    RoundButton {
        id: buttonLeft

        anchors.left: parent.left
        anchors.top: parent.top

        anchors.topMargin: buttonMargin

        text: '<'

        visible: (view.orientation === ListView.Horizontal && !(view.atXBeginning))

        onClicked: listview_id.prevPage()
    }

    RoundButton {
        id: buttonRight

        anchors.right: parent.right
        anchors.top: buttonLeft.top

        text: '>'

        visible: (view.orientation === ListView.Horizontal && !(view.atXEnd))

        onClicked: listview_id.nextPage()
    }
}
