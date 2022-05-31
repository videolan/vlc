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

ListView {
    id: root

    // Properties

    property int fadeSize: root.delegateItem
                           ? (orientation === Qt.Vertical ? root.delegateItem.height
                                                          : root.delegateItem.width) / 2
                           : (VLCStyle.margin_large * 2)

    property var fadeColor: undefined // fading will only work when fade color is defined

    // NOTE: We want buttons to be centered vertically but configurable.
    property int buttonMargin: height / 2 - buttonLeft.height / 2

    readonly property int scrollBarWidth: scroll_id.visible ? scroll_id.width : 0

    property bool keyNavigationWraps : false

    // TODO: Use itemAtIndex(0) Qt >= 5.13
    // FIXME: Delegate with variable size
    readonly property Item delegateItem: root.contentItem.children.length > 0
                                         ? root.contentItem.children[root.contentItem.children.length - 1]
                                         : null

    readonly property bool transitionsRunning: ((root.add ? root.add.running : false) ||
                                                (root.addDisplaced ? root.addDisplaced.running : false) ||
                                                (root.populate ? root.populate.running : false) ||
                                                (root.remove ? root.remove.running : false) ||
                                                (root.removeDisplaced ? root.removeDisplaced.running : false))

    readonly property Item firstVisibleItem: {
        if (transitionsRunning || !delegateItem)
            null

        var margin = -root.displayMarginBeginning
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

        var margin = -root.displayMarginEnd
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

    // Aliases

    //forward view properties
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

    // Private

    property int _currentFocusReason: Qt.OtherFocusReason

    readonly property bool _fadeRectEnoughSize: (root.orientation === Qt.Vertical
                                                 ? root.height
                                                 : root.width) > (fadeSize * 2 + VLCStyle.dp(25))

    // Settings

    Accessible.role: Accessible.List

    // Events

    onCurrentItemChanged: {
        if (_currentFocusReason === Qt.OtherFocusReason)
            return;

        // NOTE: We make sure the view has active focus before enforcing it on the item.
        if (root.activeFocus && currentItem)
            Helpers.enforceFocus(currentItem, _currentFocusReason);

        _currentFocusReason = Qt.OtherFocusReason;
    }

    // Functions

    function setCurrentItemFocus(reason) {
        if (!model || model.count === 0) {
            // NOTE: By default we want the focus on the flickable.
            root.forceActiveFocus(reason);

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
        root.contentX += (Math.min(root.width, (root.contentWidth - root.width - root.contentX)))
    }

    function prevPage() {
        root.contentX -= Math.min(root.width,root.contentX - root.originX)
    }

    focus: true

    //key navigation is reimplemented for item selection
    keyNavigationEnabled: false

    ScrollBar.vertical: ScrollBar { id: scroll_id; visible: root.contentHeight > root.height }
    ScrollBar.horizontal: ScrollBar { visible: root.contentWidth > root.width }

    highlightMoveDuration: 300 //ms
    highlightMoveVelocity: 1000 //px/s

    section.property: ""
    section.criteria: ViewSection.FullString
    section.delegate: sectionHeading

    // NOTE: We always want a valid 'currentIndex' by default.
    onCountChanged: if (count && currentIndex === -1) currentIndex = 0

    Keys.onPressed: {
        var newIndex = -1

        if (orientation === ListView.Vertical)
        {
            if ( KeyHelper.matchDown(event) ) {
                if (currentIndex !== count - 1 )
                    newIndex = currentIndex + 1
                else if ( root.keyNavigationWraps )
                    newIndex = 0
            } else if ( KeyHelper.matchPageDown(event) ) {
                newIndex = Math.min(count - 1, currentIndex + 10)
            } else if ( KeyHelper.matchUp(event) ) {
                if ( currentIndex !== 0 )
                    newIndex = currentIndex - 1
                else if ( root.keyNavigationWraps )
                    newIndex = count - 1
            } else if ( KeyHelper.matchPageUp(event) ) {
                newIndex = Math.max(0, currentIndex - 10)
            }
        }else{
            if ( KeyHelper.matchRight(event) ) {
                if (currentIndex !== count - 1 )
                    newIndex = currentIndex + 1
                else if ( root.keyNavigationWraps )
                    newIndex = 0
            }
            else if ( KeyHelper.matchPageDown(event) ) {
                newIndex = Math.min(count - 1, currentIndex + 10)
            } else if ( KeyHelper.matchLeft(event) ) {
                if ( currentIndex !== 0 )
                    newIndex = currentIndex - 1
                else if ( root.keyNavigationWraps )
                    newIndex = count - 1
            } else if ( KeyHelper.matchPageUp(event) ) {
                newIndex = Math.max(0, currentIndex - 10)
            }
        }

        if (KeyHelper.matchOk(event) || event.matches(StandardKey.SelectAll) ) {
            //these events are matched on release
            event.accepted = true
        }

        var oldIndex = currentIndex
        if (newIndex >= 0 && newIndex < count && newIndex !== oldIndex) {
            event.accepted = true;

            currentIndex = newIndex;

            selectionUpdated(event.modifiers, oldIndex, newIndex);

            // NOTE: If we skip this call the item might end up under the header.
            positionViewAtIndex(currentIndex, ItemView.Contain);

            // NOTE: We make sure we have the proper visual focus on components.
            if (oldIndex < currentIndex)
                Helpers.enforceFocus(currentItem, Qt.TabFocusReason);
            else
                Helpers.enforceFocus(currentItem, Qt.BacktabFocusReason);
        }

        if (!event.accepted) {
            root.Navigation.defaultKeyAction(event)
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


    MouseEventFilter {
        target: root

        onMouseButtonPress: {
            if (buttons & (Qt.LeftButton | Qt.RightButton)) {
                Helpers.enforceFocus(root, Qt.MouseFocusReason)

                if (!(modifiers & (Qt.ShiftModifier | Qt.ControlModifier))) {
                    root.deselectAll()
                }
            }
        }

        onMouseButtonRelease: {
            if (button & Qt.RightButton) {
                root.showContextMenu(globalPos)
            }
        }
    }

    Util.ViewDragAutoScrollHandler {
        id: dragAutoScrollHandler

        view: root
    }

    Util.FlickableScrollHandler { }

    // FIXME: This is probably not useful anymore.
    Connections {
        target: root.headerItem
        onFocusChanged: {
            if (!headerItem.focus) {
                currentItem.focus = true
            }
        }
    }

    // TODO: Make fade rectangle inline component when Qt >= 5.15
    LinearGradient {
        id: fadeRectStart

        anchors {
            top: parent.top
            left: parent.left
            right: root.orientation === Qt.Vertical ? parent.right : undefined
            bottom: root.orientation === Qt.Horizontal ? root.bottom : undefined
            topMargin: root.orientation === Qt.Vertical ? ((root.headerItem &&
                                                            root.headerItem.visible &&
                                                            (root.headerPositioning === ListView.OverlayHeader)) ? root.headerItem.height
                                                                                                                 : 0) - root.displayMarginBeginning
                                                        : 0
            leftMargin: root.orientation === Qt.Horizontal ? ((root.headerItem &&
                                                               root.headerItem.visible &&
                                                               (root.headerPositioning === ListView.OverlayHeader)) ? root.headerItem.width
                                                                                                                    : 0) - root.displayMarginBeginning
                                                           : 0
        }

        implicitHeight: fadeSize
        implicitWidth: fadeSize

        visible: (opacity !== 0.0)
        opacity: 0.0

        readonly property bool requestShow: !root.firstVisibleItem ||
                                            (!root.firstVisibleItem.activeFocus &&
                                             // TODO: Qt >5.12 use HoverHandler within the fade:
                                             !Helpers.get(root.firstVisibleItem, "hovered", false)) &&
                                            (dragAutoScrollHandler.scrollingDirection !== Util.ViewDragAutoScrollHandler.Backward)

        state: (!!root.fadeColor &&
                root._fadeRectEnoughSize &&
                requestShow &&
                (orientation === ListView.Vertical ? !root.atYBeginning
                                                   : !root.atXBeginning)) ? "shown"
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
                duration: VLCStyle.duration_short
                easing.type: Easing.InOutSine
            }
        }

        start: Qt.point(0, 0)

        end: {
            if (root.orientation === ListView.Vertical) {
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
            top: root.orientation === Qt.Horizontal ? parent.top : undefined
            left: root.orientation === Qt.Vertical ? parent.left : undefined
            right: parent.right
            bottom: parent.bottom

            bottomMargin: root.orientation === Qt.Vertical ? ((root.footerItem &&
                                                               root.footerItem.visible &&
                                                               (root.footerPositioning === ListView.OverlayFooter)) ? root.footerItem.height
                                                                                                                    : 0) - root.displayMarginEnd
                                                           : 0
            rightMargin: root.orientation === Qt.Horizontal ? ((root.footerItem &&
                                                                root.footerItem.visible &&
                                                                (root.headerPositioning === ListView.OverlayFooter)) ? root.footerItem.width
                                                                                                                     : 0) - root.displayMarginEnd
                                                            : 0
        }

        implicitHeight: fadeSize
        implicitWidth: fadeSize

        visible: opacity !== 0.0
        opacity: 0.0

        readonly property bool requestShow: !root.lastVisibleItem ||
                                            (!root.lastVisibleItem.activeFocus &&
                                             // TODO: Qt >5.12 use HoverHandler within the fade:
                                             !Helpers.get(root.lastVisibleItem, "hovered", false)) &&
                                            (dragAutoScrollHandler.scrollingDirection !== Util.ViewDragAutoScrollHandler.Forward)

        state: (!!root.fadeColor &&
                root._fadeRectEnoughSize &&
                requestShow &&
                (orientation === ListView.Vertical ? !root.atYEnd
                                                   : !root.atXEnd)) ? "shown"
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
                duration: VLCStyle.duration_short
                easing.type: Easing.InOutSine
            }
        }

        start: Qt.point(0, 0)

        end: {
            if (root.orientation === ListView.Vertical) {
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

    // FIXME: We probably need to upgrade these RoundButton(s) eventually. And we probably need
    //        to have some kind of animation when switching pages.

    RoundButton {
        id: buttonLeft

        anchors.left: parent.left
        anchors.top: parent.top

        anchors.topMargin: buttonMargin

        text: '<'

        visible: (root.orientation === ListView.Horizontal && !(root.atXBeginning))

        onClicked: root.prevPage()
    }

    RoundButton {
        id: buttonRight

        anchors.right: parent.right
        anchors.top: buttonLeft.top

        text: '>'

        visible: (root.orientation === ListView.Horizontal && !(root.atXEnd))

        onClicked: root.nextPage()
    }
}
