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


import VLC.MainInterface
import VLC.Style
import VLC.Util
import VLC.Widgets

ListView {
    id: root

    // Properties

    // NOTE: We want buttons to be centered vertically but configurable.
    property int buttonMargin: height / 2 - buttonLeft.height / 2

    property ListSelectionModel selectionModel: ListSelectionModel {
        model: root.model
    }

    // Optional property for drop indicator placement and auto scroll feature:
    property var itemContainsDrag: undefined
    
    // Optional functions for the optional drag accessory footer:
    property var isDropAcceptableFunc
    property var acceptDropFunc

    // Private

    property bool _keyPressed: false

    // Aliases

    // TODO: Qt 7 try to assign the item inline if it is possible
    //       to set it to null, so that the item is not created
    //       if the effect is not wanted.
    property alias fadingEdge: fadingEdge

    property alias autoScrollDirection: viewDragAutoScrollHandlerLoader.scrollingDirection

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

    reuseItems: true

    section.property: ""
    section.criteria: ViewSection.FullString
    section.delegate: sectionHeading

    // Content width is set to the width by default
    // If the delegate does not obey it, calculate
    // the content width appropriately.
    contentWidth: width
    
    footer: !!root.acceptDropFunc ? footerDragAccessoryComponent : null

    Component {
        id: footerDragAccessoryComponent

        Item {
            id: footerItem

            implicitWidth: root.contentWidth

            Binding on implicitHeight {
                delayed: true
                value: Math.max(VLCStyle.icon_normal, root.height - y - (root.headerItem?.height ?? 0))
            }

            property alias firstItemIndicatorVisible: firstItemIndicator.visible

            readonly property bool containsDrag: dropArea.containsDrag
            readonly property bool topContainsDrag: containsDrag
            readonly property bool bottomContainsDrag: false

            onContainsDragChanged: {
                if (root.model.count > 0) {
                    root.updateItemContainsDrag(this, containsDrag)
                } else if (!containsDrag && root.itemContainsDrag === this) {
                    // In case model count is changed somehow while
                    // containsDrag is set
                    root.updateItemContainsDrag(this, false)
                }
            }

            property alias drag: dropArea.drag

            Rectangle {
                id: firstItemIndicator

                anchors.fill: parent
                anchors.margins: VLCStyle.margin_small

                border.width: VLCStyle.dp(2)
                border.color: theme.accent

                color: "transparent"

                visible: (root.model.count === 0 && (dropArea.containsDrag || dropArea.dropOperationOngoing))

                opacity: 0.8

                IconLabel {
                    anchors.centerIn: parent

                    text: VLCIcons.add

                    font.pixelSize: VLCStyle.fontHeight_xxxlarge

                    color: theme.accent
                }
            }

            DropArea {
                id: dropArea

                anchors.fill: parent

                property bool dropOperationOngoing: false

                onEntered: function(drag) {
                    if (!root.isDropAcceptableFunc || !root.isDropAcceptableFunc(drag, root.model.rowCount())
                            || !root.acceptDropFunc) {
                        drag.accepted = false
                        return
                    }

                    drag.accepted = true
                }

                onDropped: function(drop) {
                    console.assert(!!root.acceptDropFunc)
                    dropOperationOngoing = true
                    root.acceptDropFunc(root.model.count, drop)
                        .then(() => { dropOperationOngoing = false })
                }
            }
        }
    }

    Accessible.role: Accessible.List

    add: Transition {
        OpacityAnimator {
            from: 0.0 // QTBUG-66475
            to: 1.0
            duration: VLCStyle.duration_long
            easing.type: Easing.OutSine
        }
    }

    displaced: Transition {
        NumberAnimation {
            // TODO: Use YAnimator >= Qt 6.0 (QTBUG-66475)
            property: "y"
            duration: VLCStyle.duration_long
            easing.type: Easing.OutSine
        }
    }

    // Events

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

    function updateItemContainsDrag(item, set) {
        if (set) {
            // This callLater is needed because in Qt 5.15,
            // an item might set itemContainsDrag, before
            // the owning item releases it.
            Qt.callLater(function() {
                if (itemContainsDrag)
                    console.debug(item + " set itemContainsDrag before it was released!")
                itemContainsDrag = item
            })
        } else {
            if (itemContainsDrag !== item)
                console.debug(item + " released itemContainsDrag that is not owned!")
            itemContainsDrag = null
        }
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

        Binding on enableBeginningFade {
            when: (root.autoScrollDirection === ViewDragAutoScrollHandler.Direction.Backward)
            value: false
        }

        Binding on enableEndFade {
            when: (root.autoScrollDirection === ViewDragAutoScrollHandler.Direction.Forward)
            value: false
        }
    }

    Loader {
        id: viewDragAutoScrollHandlerLoader

        active: root.itemContainsDrag !== undefined

        readonly property int scrollingDirection: item ? item.scrollingDirection : -1

        sourceComponent: ViewDragAutoScrollHandler {
            view: root
            dragging: root.itemContainsDrag !== null
            dragPosProvider: function () {
                const source = root.itemContainsDrag
                const point = source.drag
                return root.mapFromItem(source, point.x, point.y)
            }
        }
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

    DefaultFlickableScrollHandler { }

    // FIXME: This is probably not useful anymore.
    Connections {
        target: root.headerItem
        function onFocusChanged() {
            if (!headerItem.focus) {
                currentItem.focus = true
            }
        }
    }

    TapHandler {
        acceptedDevices: PointerDevice.Mouse

        acceptedButtons: Qt.LeftButton | Qt.RightButton

        grabPermissions: PointerHandler.TakeOverForbidden

        gesturePolicy: TapHandler.ReleaseWithinBounds

        onTapped: (eventPoint, button) => {
            initialAction()

            if (button === Qt.RightButton) {
                root.showContextMenu(parent.mapToGlobal(eventPoint.position.x, eventPoint.position.y))
            }
        }

        Component.onCompleted: {
            canceled.connect(initialAction)
        }

        function initialAction() {
            if (root.currentItem)
                root.currentItem.focus = false // Grab the focus from delegate
            root.forceActiveFocus(Qt.MouseFocusReason) // Re-focus the list

            if (!(point.modifiers & (Qt.ShiftModifier | Qt.ControlModifier))) {
                if (selectionModel)
                    selectionModel.clearSelection()
            }
        }
    }

    Rectangle {
        id: dropIndicator

        parent: {
            const item = root.itemContainsDrag
            if (!item || item.topContainsDrag === undefined || item.bottomContainsDrag === undefined)
              return null
            return item
        }

        z: 99

        anchors {
            left: !!parent ? parent.left : undefined
            right: !!parent ? parent.right : undefined
            top: {
                if (parent === null)
                    return undefined
                else if (parent.topContainsDrag === true)
                    return parent.top
                else if (parent.bottomContainsDrag === true)
                    return parent.bottom
                else
                    return undefined
            }
        }

        implicitHeight: VLCStyle.dp(1)

        visible: !!parent
        color: theme.accent
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
