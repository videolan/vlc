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
import QtQuick.Controls
import QtQuick.Window
import Qt5Compat.GraphicalEffects

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///util/" as Util
import "qrc:///util/Helpers.js" as Helpers

ListView {
    id: root

    // Properties

    // NOTE: We want buttons to be centered vertically but configurable.
    property int buttonMargin: height / 2 - buttonLeft.height / 2

    property ListSelectionModel selectionModel: ListSelectionModel {
        model: root.model
    }

    // Private

    property bool _keyPressed: false

    // Aliases

    // TODO: Qt 7 try to assign the item inline if it is possible
    //       to set it to null, so that the item is not created
    //       if the effect is not wanted.
    property alias fadingEdge: fadingEdge

    //forward view properties

    property alias buttonLeft: buttonLeft
    property alias buttonRight: buttonRight

    // Signals
    signal actionAtIndex(int index)

    signal showContextMenu(point globalPos)

    // Private

    property int _currentFocusReason: Qt.OtherFocusReason

    // Settings

    focus: true

    activeFocusOnTab: true

    //key navigation is reimplemented for item selection
    keyNavigationEnabled: false
    keyNavigationWraps: false

    ScrollBar.vertical: ScrollBar { }
    ScrollBar.horizontal: ScrollBar { }

    flickableDirection: Flickable.AutoFlickIfNeeded

    highlightMoveDuration: 300 //ms
    highlightMoveVelocity: 1000 //px/s

    boundsBehavior: Flickable.StopAtBounds

    section.property: ""
    section.criteria: ViewSection.FullString
    section.delegate: sectionHeading

    Accessible.role: Accessible.List

    // Events

    Component.onCompleted: {
        if (typeof root.reuseItems === "boolean") {
            // Qt 5.15 feature, on by default here:
            root.reuseItems = true
        }
    }

    // NOTE: We always want a valid 'currentIndex' by default.
    onCountChanged: if (count && currentIndex === -1) currentIndex = 0

    onCurrentItemChanged: {
        if (_currentFocusReason === Qt.OtherFocusReason)
            return;

        // NOTE: We make sure the view has active focus before enforcing it on the item.
        if (root.activeFocus && currentItem)
            Helpers.enforceFocus(currentItem, _currentFocusReason);

        _currentFocusReason = Qt.OtherFocusReason;
    }

    // Functions

    // NOTE: This function is useful to set the currentItem without losing the visual focus.
    function setCurrentItem(index) {
        if (currentIndex === index)
            return

        let reason

        if (currentItem)
            reason = currentItem.focusReason
        else
            reason = _currentFocusReason

        currentIndex = index

        if (reason !== Qt.OtherFocusReason) {
            if (currentItem)
                Helpers.enforceFocus(currentItem, reason)
            else
                setCurrentItemFocus(reason)
        }
    }

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

    // Add an indirection here because additional control
    // might be necessary as in Playqueue.
    // Derived views may override this function.
    function updateSelection(modifiers, oldIndex, newIndex) {
        if (selectionModel)
            selectionModel.updateSelection(modifiers, oldIndex, newIndex)
    }

    Keys.onPressed: (event) => {
        let newIndex = -1

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

        // these events are matched on release
        if (event.matches(StandardKey.SelectAll) || KeyHelper.matchOk(event)) {
            event.accepted = true

            _keyPressed = true
        }

        const oldIndex = currentIndex
        if (newIndex >= 0 && newIndex < count && newIndex !== oldIndex) {
            event.accepted = true;

            currentIndex = newIndex;

            root.updateSelection(event.modifiers, oldIndex, newIndex);

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

    Keys.onReleased: (event) => {
        if (_keyPressed === false)
            return

        _keyPressed = false

        if (event.matches(StandardKey.SelectAll)) {
            event.accepted = true
            if (selectionModel)
                selectionModel.selectAll()
        } else if (KeyHelper.matchOk(event)) { //enter/return/space
            event.accepted = true
            actionAtIndex(currentIndex)
        }
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    FadingEdgeForListView {
        id: fadingEdge

        anchors.fill: parent

        listView: root

        backgroundColor: theme.bg.primary
    }

    Component {
        id: sectionHeading

        Column {
            width: parent.width

            Text {
                text: section
                font.pixelSize: VLCStyle.fontSize_xlarge
                color: theme.accent
            }

            Rectangle {
                width: parent.width
                height: 1
                color: theme.border
            }
        }
    }

    Util.FlickableScrollHandler { }

    // FIXME: This is probably not useful anymore.
    Connections {
        target: root.headerItem
        function onFocusChanged() {
            if (!headerItem.focus) {
                currentItem.focus = true
            }
        }
    }

    MouseArea {
        anchors.fill: parent

        z: -1

        preventStealing: true

        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onPressed: (mouse) => {
            focus = true // Grab the focus from delegate
            root.forceActiveFocus(Qt.MouseFocusReason) // Re-focus the list

            if (!(mouse.modifiers & (Qt.ShiftModifier | Qt.ControlModifier))) {
                if (selectionModel)
                    selectionModel.clearSelection()
            }

            mouse.accepted = true
        }

        onReleased: (mouse) => {
            if (mouse.button & Qt.RightButton) {
                root.showContextMenu(mapToGlobal(mouse.x, mouse.y))
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

        visible: (root.orientation === ListView.Horizontal && !(root.atXBeginning))

        onClicked: root.prevPage()

        activeFocusOnTab: false
    }

    RoundButton {
        id: buttonRight

        anchors.right: parent.right
        anchors.top: buttonLeft.top

        text: '>'

        visible: (root.orientation === ListView.Horizontal && !(root.atXEnd))

        onClicked: root.nextPage()

        activeFocusOnTab: false
    }
}
