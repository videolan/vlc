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
import QtQuick.Layouts

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

    property Component defaultScrollBar: Component {
        ScrollBarExt { }
    }

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

    pixelAligned: (MainCtx.qtVersion() >= MainCtx.qtVersionCheck(6, 2, 5)) // QTBUG-103996
                  && (Screen.pixelDensity >= VLCStyle.highPixelDensityThreshold) // no need for sub-pixel alignment with high pixel density

    focus: true

    activeFocusOnTab: true

    //key navigation is reimplemented for item selection
    keyNavigationEnabled: false
    keyNavigationWraps: false

    ScrollBar.vertical: {
        // By default vertical scroll bar is only used when the orientation is vertical.
        if (root.defaultScrollBar && (root.orientation === ListView.Vertical))
            return root.defaultScrollBar.createObject() // rely on JS/QML engine's garbage collection
        return null
    }
    ScrollBar.horizontal: {
        // By default horizontal scroll bar is only used when the orientation is horizontal.
        if (root.defaultScrollBar && (root.orientation === ListView.Horizontal))
            return root.defaultScrollBar.createObject() // rely on JS/QML engine's garbage collection
        return null
    }

    flickableDirection: Flickable.AutoFlickIfNeeded

    highlightMoveDuration: 300 //ms
    highlightMoveVelocity: 1000 //px/s

    boundsBehavior: Flickable.StopAtBounds

    reuseItems: true

    section.property: ""
    section.criteria: ViewSection.FullString
    section.delegate: sectionHeading

    // Content size is set to the size by default
    // If the delegate does not obey it, calculate
    // the content size appropriately.
    contentWidth: (orientation === ListView.Vertical) ? width - (leftMargin + rightMargin) : -1
    contentHeight: (orientation === ListView.Horizontal) ? height - (topMargin + bottomMargin) : -1
    
    footer: !!root.acceptDropFunc ? footerDragAccessoryComponent : null

    onItemContainsDragChanged: {
        if (!dropIndicatorItem && dropIndicator)
            dropIndicatorItem = dropIndicator.createObject(this)
    }

    component VerticalDropAreaLayout : ColumnLayout {
        spacing: 0

        property alias higherDropArea: higherDropArea
        property alias lowerDropArea: lowerDropArea

        property var isDropAcceptable
        property var acceptDrop

        property Item view

        readonly property point dragPosition: {
            let area = null

            if (higherDropArea.containsDrag)
                area = higherDropArea
            else if (lowerDropArea.containsDrag)
                area = lowerDropArea
            else
                return Qt.point(0, 0)

            const drag = area.drag
            return Qt.point(drag.x, drag.y)
        }

        function commonDrop(targetIndex, drop) {
            const promise = acceptDrop(targetIndex, drop)
            if (view) {
                MainCtx.setCursor(view, Qt.BusyCursor)
                promise.then(() => {
                    // NOTE: check view again for the unlikely case it is
                    //       gone by the time the promise is resolved:
                    if (view)
                        MainCtx.unsetCursor(view)
                })
            }
        }

        // NOTE: Nested inline components are not supported in QML as of Qt 6.8

        DropArea {
            id: higherDropArea

            Layout.fillWidth: true
            Layout.fillHeight: true

            onEntered: (drag) => {
                if (!acceptDrop) {
                    drag.accepted = false
                    return
                }

                if (isDropAcceptable && !isDropAcceptable(drag, index)) {
                    drag.accepted = false
                    return
                }
            }

            onDropped: (drop) => {
                console.assert(acceptDrop)
                commonDrop(index, drop)
            }
        }

        DropArea {
            id: lowerDropArea

            Layout.fillWidth: true
            Layout.fillHeight: true

            onEntered: (drag) =>  {
                if (!acceptDrop) {
                    drag.accepted = false
                    return
                }

                if (isDropAcceptable && !isDropAcceptable(drag, index + 1)) {
                    drag.accepted = false
                    return
                }
            }

            onDropped: (drop) => {
                console.assert(acceptDrop)
                commonDrop(index + 1, drop)
            }
        }
    }

    Component {
        id: footerDragAccessoryComponent

        Item {
            id: footerItem

            implicitWidth: root.contentWidth
            implicitHeight: Math.max(VLCStyle.icon_normal, root.height - y - (root.headerItem?.height ?? 0))

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

            property alias dragPosition: dropArea.drag

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

                onDropOperationOngoingChanged: {
                    if (dropOperationOngoing)
                        MainCtx.setCursor(root, Qt.BusyCursor)
                    else
                        MainCtx.unsetCursor(root)
                }

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
        // Transition is relevant when drag and drop is feasible.
        // Component approach can not be used here because `Transition`
        // does not have a "target" property, and wants a valid parent.
        enabled: !!root.acceptDropFunc

        OpacityAnimator {
            from: 0.0 // QTBUG-66475
            to: 1.0
            duration: VLCStyle.duration_long
            easing.type: Easing.OutSine
        }

        onRunningChanged: {
            if (!running) {
                // This intends to clear the artifact (QTBUG-110969).
                // Note that this does not help if items are not
                // positioned correctly, for that x/y animation should
                // not be used for add transitions.
                const epsilon = 0.001
                if (root.orientation === ListView.Vertical) {
                    root.contentY -= epsilon
                    root.contentY += epsilon
                } else if (root.orientation === ListView.Horizontal) {
                    root.contentX -= epsilon
                    root.contentX += epsilon
                }
            }
        }
    }

    // WARNING: Add displaced transition is disabled, because it is
    //          often not executed properly, and causes items to have
    //          incorrect positions. It is currently not possible to
    //          recover from that situation. Fortunately move and
    //          remove displaced seemingly are not affected from that
    //          issue. See QTBUG-131106, QTBUG-89158, ...

    moveDisplaced: Transition {
        // Transition is relevant when drag and drop is feasible.
        // Component approach can not be used here because `Transition`
        // does not have a "target" property, and wants a valid parent.
        enabled: !!root.acceptDropFunc

        NumberAnimation {
            // TODO: Use YAnimator >= Qt 6.0 (QTBUG-66475)
            property: (root.orientation === ListView.Vertical) ? "y" : "x"
            duration: VLCStyle.duration_long
            easing.type: Easing.OutSine
        }
    }

    removeDisplaced: moveDisplaced

    // Events

    Component.onCompleted: {
        // Flickable filters child mouse events for flicking (even when
        // the delegate is grabbed). However, this is not a useful
        // feature for non-touch cases, so disable it here and enable
        // it if touch is detected through the hover handler:
        MainCtx.setFiltersChildMouseEvents(root, false)
    }

    HoverHandler {
        acceptedDevices: PointerDevice.TouchScreen

        onHoveredChanged: {
            if (hovered)
                MainCtx.setFiltersChildMouseEvents(root, true)
            else
                MainCtx.setFiltersChildMouseEvents(root, false)
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

    // Qt does not allow having multiple behavior on a single
    // property. Having this behavior for a single purpose can
    // be problematic for the cases where a different behavior
    // wanted to be used. However, I have not found a nice
    // solution for that, so we have this behavior here for now.
    Behavior on contentX {
        id: horizontalPageAnimationBehavior

        enabled: false

        // NOTE: Usage of `SmoothedAnimation` is intentional here.
        SmoothedAnimation {
            duration: VLCStyle.duration_veryLong
            easing.type: Easing.InOutSine
        }
    }

    function animatePage(func) {
        // One might think, what is the purpose of this if `highlightFollowsCurrentItem` (default
        // true) causes the view to smoothly follow the current item. The thing is that, not in
        // all cases the current index is changed. With the horizontal page buttons, for example,
        // it is not conventional to change the current index.
        console.assert(func === root.nextPage || func === root.prevPage)
        horizontalPageAnimationBehavior.enabled = true
        func()
        horizontalPageAnimationBehavior.enabled = false
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
            if (itemContainsDrag)
                console.debug(item + " set itemContainsDrag before it was released!")
            itemContainsDrag = item
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
            if (root.highlightFollowsCurrentItem) {
                // FIXME: Items can go beneath `OverlayHeader` or `OverlayFooter`.
                //        We should move the header and footer outside of the view,
                //        if we do not want that behavior instead of having this
                //        workaround, as Qt does not seem to offer that configuration
                //        as a built-in mode in its views.

                if (root.headerItem && (root.headerPositioning !== ListView.InlineHeader)) {
                    if (root.currentItem.y < (root.headerItem.y + root.headerItem.height))
                        positionViewAtIndex(currentIndex, ItemView.Contain);
                }

                if (root.footerItem && (root.footerPositioning !== ListView.InlineFooter)) {
                    if (root.currentItem.y > root.footerItem.y)
                        positionViewAtIndex(currentIndex, ItemView.Contain);
                }
            }

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
                const point = source.dragPosition
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

    property Component dropIndicator: Rectangle {
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

    property Item dropIndicatorItem

    // FIXME: We probably need to upgrade these RoundButton(s) eventually. And we probably need
    //        to have some kind of animation when switching pages.

    RoundButtonExt {
        id: buttonLeft

        anchors.left: parent.left
        anchors.top: parent.top

        anchors.topMargin: buttonMargin

        text: '<'

        visible: (root.orientation === ListView.Horizontal && !(root.atXBeginning))

        onClicked: {
            root.animatePage(root.prevPage)
        }

        activeFocusOnTab: false
    }

    RoundButtonExt {
        id: buttonRight

        anchors.right: parent.right
        anchors.top: buttonLeft.top

        text: '>'

        visible: (root.orientation === ListView.Horizontal && !(root.atXEnd))

        onClicked: {
            root.animatePage(root.nextPage)
        }

        activeFocusOnTab: false
    }
}
