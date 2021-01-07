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
import QtQuick.Layouts 1.3
import QtQml.Models 2.2
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/KeyHelper.js" as KeyHelper
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root

    property var plmodel: PlaylistListModel {
        playlistId: mainctx.playlist
    }

    property int leftPadding: 0
    property int rightPadding: 0
    property alias backgroundColor: parentRect.color

    property VLCColors colors: VLCStyle.colors

    signal setItemDropIndicatorVisible(int index, bool isVisible, bool top)

    function isDropAcceptable(drop, index) {
        return drop.hasUrls
                || ((!!drop.source && (drop.source instanceof PlaylistDroppable))
                     && drop.source.canInsertIntoPlaylist(index))
    }

    function acceptDrop(index, drop) {
        if (!!drop.source && (drop.source instanceof PlaylistDroppable)) {
            drop.source.insertIntoPlaylist(index)
        } else if (drop.hasUrls) {
            //force conversion to an actual list
            var urlList = []
            for ( var url in drop.urls)
                urlList.push(drop.urls[url])
            mainPlaylistController.insert(index, urlList, false)
        }
        drop.accept(Qt.IgnoreAction)
    }

    PlaylistOverlayMenu {
        id: overlayMenu
        anchors.fill: parent
        z: 1

        backgroundItem: parentRect
    }

    Rectangle {
        id: parentRect
        anchors.fill: parent
        color: colors.banner

        onActiveFocusChanged: {
            if (activeFocus)
                view.forceActiveFocus()
        }

        //label for DnD
        Widgets.DNDLabel {
            id: dragItem

            colors: root.colors
            color: parent.color

            property int _scrollingDirection: 0

            function insertIntoPlaylist(index) {
                root.plmodel.moveItemsPre(root.plmodel.getSelection(), index)
            }

            function canInsertIntoPlaylist(index) {
                var delta = model.index - index
                return delta !== 0 && delta !== -1 && index !== model.index
            }

            on_PosChanged: {
                var dragItemY = root.mapToGlobal(dragItem._pos.x, dragItem._pos.y).y
                var viewY     = root.mapToGlobal(view.x, view.y).y

                var topDiff    = (viewY + VLCStyle.dp(20, VLCStyle.scale)) - dragItemY
                var bottomDiff = dragItemY - (viewY + view.height - VLCStyle.dp(20, VLCStyle.scale))

                if(!view.listView.atYBeginning && topDiff > 0) {
                    _scrollingDirection = -1

                    view.fadeRectTopHovered = true
                }
                else if( !view.listView.atYEnd && bottomDiff > 0) {
                    _scrollingDirection = 1

                    view.fadeRectBottomHovered = true
                }
                else {
                    _scrollingDirection = 0

                    view.fadeRectTopHovered = false
                    view.fadeRectBottomHovered = false
                }
            }

            SmoothedAnimation {
                id: upAnimation
                target: view.listView
                property: "contentY"
                to: 0
                running: dragItem._scrollingDirection === -1 && dragItem.visible

                velocity: VLCStyle.dp(150, VLCStyle.scale)
            }

            SmoothedAnimation {
                id: downAnimation
                target: view.listView
                property: "contentY"
                to: view.listView.contentHeight - view.height
                running: dragItem._scrollingDirection === 1 && dragItem.visible

                velocity: VLCStyle.dp(150, VLCStyle.scale)
            }
        }

        PlaylistContextMenu {
            id: contextMenu
            model: root.plmodel
            controler: mainPlaylistController
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.bottomMargin: VLCStyle.margin_normal

            ColumnLayout {
                id: headerTextLayout
                Layout.fillWidth: true
                Layout.leftMargin: root.leftPadding + VLCStyle.margin_normal
                Layout.topMargin: VLCStyle.margin_normal

                Widgets.SubtitleLabel {
                    text: i18n.qtr("Playqueue")
                    color: colors.text
                }

                Widgets.CaptionLabel {
                    function getHoursMinutesText(duration) {
                        var hours = duration.toHours()
                        var minutes = duration.toMinutes()
                        var text
                        if (hours >= 1) {
                            minutes = minutes % 60
                            text = i18n.qtr("%1h %2min").arg(hours).arg(minutes)
                        }
                        else if (minutes > 0) {
                            text = i18n.qtr("%1 min").arg(minutes)
                        }
                        else {
                            text = i18n.qtr("%1 sec").arg(duration.toSeconds())
                        }

                        return text
                    }

                    anchors.topMargin: VLCStyle.margin_small
                    visible: plmodel.count !== 0
                    text: i18n.qtr("%1 elements, %2").arg(root.plmodel.count).arg(getHoursMinutesText(plmodel.duration))
                    color: colors.caption
                }
            }

            RowLayout {
                id: content
                visible: plmodel.count !== 0

                Layout.topMargin: VLCStyle.margin_normal
                Layout.leftMargin: root.leftPadding + VLCStyle.margin_normal
                Layout.rightMargin: root.rightPadding + view.scrollBarWidth

                Widgets.IconLabel {
                    Layout.preferredWidth: VLCStyle.icon_normal
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: VLCIcons.album_cover
                    color: colors.caption
                }

                Widgets.CaptionLabel {
                    Layout.fillWidth: true
                    Layout.leftMargin: VLCStyle.margin_large
                    verticalAlignment: Text.AlignVCenter
                    text: i18n.qtr("Title")
                    color: colors.caption
                }

                Widgets.IconLabel {
                    Layout.rightMargin: VLCStyle.margin_xsmall
                    Layout.preferredWidth: durationMetric.width

                    text: VLCIcons.time
                    color: colors.caption
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter

                    TextMetrics {
                        id: durationMetric
                        font.pixelSize: VLCStyle.fontSize_normal
                        text: "-00:00-"
                    }
                }
            }

            Widgets.KeyNavigableListView {
                id: view

                Layout.fillWidth: true
                Layout.fillHeight: true

                focus: true

                model: root.plmodel
                modelCount: root.plmodel.count

                fadeColor: root.backgroundColor

                property int shiftIndex: -1
                property string mode: "normal"

                Connections {
                    target: root.plmodel
                    onRowsInserted: {
                        if (view.currentIndex == -1)
                            view.currentIndex = 0
                    }
                    onModelReset: {
                        if (view.currentIndex == -1 &&  root.plmodel.count > 0)
                            view.currentIndex = 0
                    }
                    onSelectedCountChanged: {
                        var selectedIndexes = root.plmodel.getSelection()
                        var modelCount = root.plmodel.count

                        if (modelCount === 0 || selectedIndexes.length === 0)
                            return

                        var bottomItemIndex = view.listView.indexAt(view.listView.contentX, (view.listView.contentY + view.height) - 2)
                        var topItemIndex    = view.listView.indexAt(view.listView.contentX, view.listView.contentY + 2)

                        if (topItemIndex !== -1 && (root.plmodel.isSelected(topItemIndex) || (modelCount >= 2 && root.plmodel.isSelected(topItemIndex + 1))))
                            view.fadeRectTopHovered = true
                        else
                            view.fadeRectTopHovered = false

                        if (bottomItemIndex !== -1 && (root.plmodel.isSelected(bottomItemIndex) || (root.plmodel.isSelected(bottomItemIndex - 1))))
                            view.fadeRectBottomHovered = true
                        else
                            view.fadeRectBottomHovered = false
                    }
                }

                footer: Item {
                    width: parent.width
                    height: Math.max(VLCStyle.icon_normal, view.height - y)

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton | Qt.LeftButton

                        onClicked: {
                            view.forceActiveFocus()
                            if( mouse.button === Qt.RightButton )
                                contextMenu.popup(-1, this.mapToGlobal(mouse.x, mouse.y))
                            else if ( mouse.button === Qt.LeftButton )
                                root.plmodel.deselectAll()
                        }
                    }

                    DropArea {
                        anchors.fill: parent
                        onEntered: {
                            if(!root.isDropAcceptable(drag, root.plmodel.count))
                                return

                            root.setItemDropIndicatorVisible(view.modelCount - 1, true, false);
                        }
                        onExited: {


                            root.setItemDropIndicatorVisible(view.modelCount - 1, false, false);
                        }
                        onDropped: {
                            if(!root.isDropAcceptable(drop, root.plmodel.count))
                                return
                            root.acceptDrop(root.plmodel.count, drop)
                            root.setItemDropIndicatorVisible(view.modelCount - 1, false, false);
                        }
                    }
                }

                ToolTip {
                    id: plInfoTooltip
                    delay: 750
                }

                delegate: Item {
                    implicitWidth: plitem.width
                    implicitHeight: childrenRect.height

                    Loader {
                        anchors.top: plitem.top

                        active: (index === 0) // load only for the first element to prevent overlapping
                        width: parent.width
                        height: 1
                        z: (model.selected || plitem.hovered || plitem.activeFocus) ? 2 : 1
                        sourceComponent: Rectangle {
                            color: colors.playlistSeparator
                            opacity: colors.isThemeDark ? 0.05 : 1.0
                        }
                    }

                    PlaylistDelegate {
                        /*
                         * implicit variables:
                         *  - model: gives access to the values associated to PlaylistListModel roles
                         *  - index: the index of this item in the list
                         */
                        id: plitem
                        plmodel: root.plmodel
                        width: root.width
                        z: 1
                        leftPadding: root.leftPadding + VLCStyle.margin_normal
                        rightPadding: root.rightPadding + view.scrollBarWidth

                        onItemClicked : {
                            /* to receive keys events */
                            view.forceActiveFocus()
                            if (view.mode == "move") {
                                var selectedIndexes = root.plmodel.getSelection()
                                if (selectedIndexes.length === 0)
                                    return
                                var preTarget = index
                                /* move to _above_ the clicked item if move up, but
                                 * _below_ the clicked item if move down */
                                if (preTarget > selectedIndexes[0])
                                    preTarget++
                                view.currentIndex = selectedIndexes[0]
                                root.plmodel.moveItemsPre(selectedIndexes, preTarget)
                                return
                            } else if (view.mode == "select") {
                            } else if (!(root.plmodel.isSelected(index) && button === Qt.RightButton)) {
                                view.updateSelection(modifier, view.currentIndex, index)
                                view.currentIndex = index
                            }

                            if (button === Qt.RightButton)
                                contextMenu.popup(index, globalMousePos)
                        }
                        onItemDoubleClicked: mainPlaylistController.goTo(index, true)
                        color: colors.getPLItemColor(model.selected, plitem.hovered, plitem.activeFocus)
                        colors: root.colors

                        onDragStarting: {
                            if (!root.plmodel.isSelected(index)) {
                                /* the dragged item is not in the selection, replace the selection */
                                root.plmodel.setSelection([index])
                            }
                        }

                        function isDropAcceptable(drop, index) {
                            return root.isDropAcceptable(drop, index)
                        }

                        onDropedMovedAt: {
                            root.acceptDrop(target, drop)
                        }

                        onHoveredChanged: {
                            var bottomItemIndex = view.listView.indexAt(plitem.width / 2, (view.listView.contentY + view.height) - 2)
                            var topItemIndex = view.listView.indexAt(plitem.width / 2, view.listView.contentY + 2)

                            if(bottomItemIndex !== -1 && model.index >= bottomItemIndex - 1)
                            {
                                view.fadeRectBottomHovered = plitem.hovered
                            }
                            if(topItemIndex !== -1 && model.index <= topItemIndex + 1)
                            {
                                view.fadeRectTopHovered = plitem.hovered
                            }
                        }
                    }

                    Connections {
                        target: root

                        onSetItemDropIndicatorVisible: {
                            if ((index === model.index && !top) || (index === model.index + 1 && top)) {
                                bottomSeparator.visible = !isVisible
                            }
                        }
                    }

                    Rectangle {
                        id: bottomSeparator
                        anchors.top: plitem.bottom

                        width: parent.width
                        height: 1
                        z: 2
                        color: colors.playlistSeparator
                        opacity: colors.isThemeDark ? 0.05 : 1.0
                    }
                }

                add: Transition {
                    NumberAnimation { property: "opacity"; from: 0; to: 1.0; duration: 200 }
                }

                displaced: Transition {
                    NumberAnimation { properties: "x,y"; duration: 200; easing.type: Easing.OutSine }
                    NumberAnimation { property: "opacity"; to: 1.0 }
                }

                onSelectAll: root.plmodel.selectAll()
                onSelectionUpdated: {
                    if (view.mode === "select") {
                        console.log("update selection select")
                    } else if (mode == "move") {
                        var selectedIndexes = root.plmodel.getSelection()
                        if (selectedIndexes.length === 0)
                            return
                        /* always move relative to the first item of the selection */
                        var target = selectedIndexes[0];
                        if (newIndex > oldIndex) {
                            /* move down */
                            target++
                        } else if (newIndex < oldIndex && target > 0) {
                            /* move up */
                            target--
                        }

                        view.currentIndex = selectedIndexes[0]
                        /* the target is the position _after_ the move is applied */
                        root.plmodel.moveItemsPost(selectedIndexes, target)
                    } else { // normal
                        updateSelection(keyModifiers, oldIndex, newIndex);
                    }
                }

                Keys.onDeletePressed: onDelete()
                Keys.onMenuPressed: overlayMenu.open()

                navigationParent: root
                navigationRight: function(index) {
                    overlayMenu.open()
                }
                navigationLeft: function(index) {
                    if (mode === "normal") {
                        root.navigationLeft(index)
                    } else {
                        mode = "normal"
                    }
                }
                navigationCancel: function(index) {
                    if (mode === "normal") {
                        root.navigationCancel(index)
                    } else {
                        mode = "normal"
                    }
                }

                onActionAtIndex: {
                    if (index < 0)
                        return

                    if (mode === "select")
                        root.plmodel.toggleSelected(index)
                    else //normal
                        // play
                        mainPlaylistController.goTo(index, true)
                }

                function onPlay() {
                    let selection = root.plmodel.getSelection()
                    if (selection.length === 0)
                        return
                    mainPlaylistController.goTo(selection[0], true)
                }

                function onDelete() {
                    let selection = root.plmodel.getSelection()
                    if (selection.length === 0)
                        return
                    root.plmodel.removeItems(selection)
                }

                function _addRange(from, to) {
                    root.plmodel.setRangeSelected(from, to - from + 1, true)
                }

                function _delRange(from, to) {
                    root.plmodel.setRangeSelected(from, to - from + 1, false)
                }

                // copied from SelectableDelegateModel, which is intended to be removed
                function updateSelection( keymodifiers, oldIndex, newIndex ) {
                    if ((keymodifiers & Qt.ShiftModifier)) {
                        if ( shiftIndex === oldIndex) {
                            if ( newIndex > shiftIndex )
                                _addRange(shiftIndex, newIndex)
                            else
                                _addRange(newIndex, shiftIndex)
                        } else if (shiftIndex <= newIndex && newIndex < oldIndex) {
                            _delRange(newIndex + 1, oldIndex )
                        } else if ( shiftIndex < oldIndex && oldIndex < newIndex ) {
                            _addRange(oldIndex, newIndex)
                        } else if ( newIndex < shiftIndex && shiftIndex < oldIndex ) {
                            _delRange(shiftIndex, oldIndex)
                            _addRange(newIndex, shiftIndex)
                        } else if ( newIndex < oldIndex && oldIndex < shiftIndex  ) {
                            _addRange(newIndex, oldIndex)
                        } else if ( oldIndex <= shiftIndex && shiftIndex < newIndex ) {
                            _delRange(oldIndex, shiftIndex)
                            _addRange(shiftIndex, newIndex)
                        } else if ( oldIndex < newIndex && newIndex <= shiftIndex  ) {
                            _delRange(oldIndex, newIndex - 1)
                        }
                    } else {
                        shiftIndex = newIndex
                        if (keymodifiers & Qt.ControlModifier) {
                            root.plmodel.toggleSelected(newIndex)
                        } else {
                            root.plmodel.setSelection([newIndex])
                        }
                    }
                }

                Column {
                    anchors.centerIn: parent
                    visible: plmodel.count === 0

                    Widgets.IconLabel {
                        font.pixelSize: VLCStyle.dp(48, VLCStyle.scale)
                        anchors.horizontalCenter: parent.horizontalCenter
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        text: VLCIcons.playlist
                        color: view.activeFocus ? colors.accent : colors.text
                        opacity: 0.3
                    }

                    // ToDo: Use TitleLabel
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.topMargin: VLCStyle.margin_xlarge
                        text: i18n.qtr("No content yet")
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: VLCStyle.fontSize_xxlarge
                        color: view.activeFocus ? colors.accent : colors.text
                        opacity: 0.4
                    }

                    // ToDo: Use BodyLabel
                    Label {
                        anchors.topMargin: VLCStyle.margin_normal
                        text: i18n.qtr("Drag & Drop some content here!")
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: VLCStyle.fontSize_large
                        color: view.activeFocus ? colors.accent : colors.text
                        opacity: 0.4
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                height: VLCStyle.heightBar_normal
                visible: !(infoText.text === "")

                RectangularGlow {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter

                    width: infoText.width + VLCStyle.dp(18, VLCStyle.scale)
                    height: infoText.height + VLCStyle.dp(12, VLCStyle.scale)

                    glowRadius: 2
                    cornerRadius: 10
                    spread: 0.1
                    color: colors.glowColorBanner
                }

                Label {
                    id: infoText
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.centerIn: parent
                    horizontalAlignment: Text.AlignHCenter

                    text: (view.mode === "select")
                            ? i18n.qtr("Select tracks (%1)").arg(plmodel.selectedCount)
                        : (view.mode === "move")
                            ? i18n.qtr("Move tracks (%1)").arg(plmodel.selectedCount)
                        : ""
                    font.pixelSize: VLCStyle.fontSize_large
                    color: colors.text
                    elide: Text.ElideRight
                }
            }

            PlaylistToolbar {
                Layout.fillWidth: true

                leftPadding: root.leftPadding
                rightPadding: root.rightPadding
                navigationParent: root
                navigationUpItem: view

                colors: root.colors
            }
        }
    }

    Keys.priority: Keys.AfterItem
    Keys.forwardTo: view
    Keys.onPressed: {
        if (event.matches(StandardKey.SelectAll))
        {
            root.plmodel.selectAll();
        }
        else
        {
            defaultKeyAction(event, 0)
        }
    }
}
