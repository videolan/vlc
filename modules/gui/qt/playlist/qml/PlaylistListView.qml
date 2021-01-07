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

    property alias model: listView.model

    property int leftPadding: 0
    property int rightPadding: 0

    property VLCColors colors: VLCStyle.colors

    enum Mode {
        Normal,
        Select, // Keyboard item selection mode, activated through PlaylistOverlayMenu
        Move // Keyboard item move mode, activated through PlaylistOverlayMenu
    }

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

        colors: root.colors
        backgroundItem: parentRect
    }

    Rectangle {
        id: parentRect
        anchors.fill: parent
        color: colors.banner

        onActiveFocusChanged: {
            if (activeFocus)
                listView.forceActiveFocus()
        }

        //label for DnD
        Widgets.DNDLabel {
            id: dragItem

            colors: root.colors
            color: parent.color

            property int _scrollingDirection: 0

            function insertIntoPlaylist(index) {
                root.model.moveItemsPre(root.model.getSelection(), index)
            }

            function canInsertIntoPlaylist(index) {
                var delta = model.index - index
                return delta !== 0 && delta !== -1 && index !== model.index
            }

            on_PosChanged: {
                var dragItemY = dragItem._pos.y
                var viewY     = root.mapFromItem(listView, listView.x, listView.y).y

                var topDiff    = (viewY + VLCStyle.dp(20, VLCStyle.scale)) - dragItemY
                var bottomDiff = dragItemY - (viewY + listView.height - toolbar.height - VLCStyle.dp(20, VLCStyle.scale))

                if(!listView.listView.atYBeginning && topDiff > 0) {
                    _scrollingDirection = -1

                    listView.fadeRectTopHovered = true
                }
                else if( !listView.listView.atYEnd && bottomDiff > 0) {
                    _scrollingDirection = 1

                    listView.fadeRectBottomHovered = true
                }
                else {
                    _scrollingDirection = 0

                    listView.fadeRectTopHovered = false
                    listView.fadeRectBottomHovered = false
                }
            }

            SmoothedAnimation {
                id: upAnimation
                target: listView.listView
                property: "contentY"
                to: 0
                running: dragItem._scrollingDirection === -1 && dragItem.visible

                velocity: VLCStyle.dp(150, VLCStyle.scale)
            }

            SmoothedAnimation {
                id: downAnimation
                target: listView.listView
                property: "contentY"
                to: listView.listView.contentHeight - listView.height
                running: dragItem._scrollingDirection === 1 && dragItem.visible

                velocity: VLCStyle.dp(150, VLCStyle.scale)
            }
        }

        PlaylistContextMenu {
            id: contextMenu
            model: root.model
            controler: mainPlaylistController
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.topMargin: VLCStyle.margin_normal
            anchors.bottomMargin: VLCStyle.margin_normal

            anchors.leftMargin: root.leftPadding
            anchors.rightMargin: root.rightPadding

            spacing: 0

            ColumnLayout {
                id: headerTextLayout

                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.margin_normal

                spacing: VLCStyle.margin_xxxsmall

                Widgets.SubtitleLabel {
                    text: i18n.qtr("Playqueue")
                    color: colors.text
                    font.weight: Font.Bold
                    font.pixelSize: VLCStyle.dp(24, VLCStyle.scale)
                }

                Widgets.CaptionLabel {
                    color: (listView.mode === PlaylistListView.Mode.Select || listView.mode === PlaylistListView.Mode.Move)
                           ? colors.accent : colors.caption
                    visible: model.count !== 0
                    text: {
                        switch (listView.mode) {
                        case PlaylistListView.Mode.Select:
                            return i18n.qtr("Selected tracks: %1").arg(model.selectedCount)
                        case PlaylistListView.Mode.Move:
                            return i18n.qtr("Moving tracks: %1").arg(model.selectedCount)
                        case PlaylistListView.Mode.Normal:
                        default:
                            return i18n.qtr("%1 elements, %2").arg(model.count).arg(getHoursMinutesText(model.duration))
                        }
                    }

                    function getHoursMinutesText(duration) {
                        var hours = duration.toHours()
                        var minutes = duration.toMinutes()
                        var text
                        if (hours >= 1) {
                            minutes = minutes % 60
                            text = i18n.qtr("%1h %2 min").arg(hours).arg(minutes)
                        }
                        else if (minutes > 0) {
                            text = i18n.qtr("%1 min").arg(minutes)
                        }
                        else {
                            text = i18n.qtr("%1 sec").arg(duration.toSeconds())
                        }

                        return text
                    }
                }
            }

            RowLayout {
                visible: model.count !== 0

                Layout.topMargin: VLCStyle.margin_normal
                Layout.leftMargin: VLCStyle.margin_normal
                Layout.rightMargin: listView.scrollBarWidth

                spacing: 0

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
                id: listView

                Layout.fillWidth: true
                Layout.fillHeight: true

                focus: true

                model: PlaylistListModel {
                    playlistId: mainctx.playlist
                }

                fadeColor: parentRect.color

                property int shiftIndex: -1
                property int mode: PlaylistListView.Mode.Normal

                signal setItemDropIndicatorVisible(int index, bool visible)

                Connections {
                    target: root.model
                    onRowsInserted: {
                        if (listView.currentIndex == -1)
                            listView.currentIndex = 0
                    }
                    onModelReset: {
                        if (listView.currentIndex == -1 &&  root.model.count > 0)
                            listView.currentIndex = 0
                    }
                    onSelectedCountChanged: {
                        var selectedIndexes = root.model.getSelection()

                        if (listView.modelCount === 0 || selectedIndexes.length === 0)
                            return

                        var bottomItemIndex = listView.listView.indexAt(listView.listView.contentX, (listView.listView.contentY + listView.height) - 2)
                        var topItemIndex    = listView.listView.indexAt(listView.listView.contentX, listView.listView.contentY + 2)

                        if (topItemIndex !== -1 && (root.model.isSelected(topItemIndex) || (modelCount >= 2 && root.model.isSelected(topItemIndex + 1))))
                            listView.fadeRectTopHovered = true
                        else
                            listView.fadeRectTopHovered = false

                        if (bottomItemIndex !== -1 && (root.model.isSelected(bottomItemIndex) || (root.model.isSelected(bottomItemIndex - 1))))
                            listView.fadeRectBottomHovered = true
                        else
                            listView.fadeRectBottomHovered = false
                    }
                }

                footer: Item {
                    width: parent.width
                    height: Math.max(VLCStyle.icon_normal, listView.height - y)

                    function setDropIndicatorVisible(visible) {
                        dropIndicator.visible = visible
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton | Qt.LeftButton

                        onClicked: {
                            listView.forceActiveFocus()
                            if( mouse.button === Qt.RightButton )
                                contextMenu.popup(-1, this.mapToGlobal(mouse.x, mouse.y))
                        }
                    }

                    Rectangle {
                        id: dropIndicator

                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: VLCStyle.dp(1)
                        anchors.top: parent.top

                        visible: false
                        color: colors.accent
                    }

                    DropArea {
                        id: dropArea

                        anchors.fill: parent

                        onEntered: {
                            if(!root.isDropAcceptable(drag, root.model.count))
                                return

                            dropIndicator.visible = true
                        }
                        onExited: {
                            dropIndicator.visible = false
                        }
                        onDropped: {
                            if(!root.isDropAcceptable(drop, root.model.count))
                                return

                            root.acceptDrop(root.model.count, drop)

                            dropIndicator.visible = false
                        }
                    }
                }

                ToolTip {
                    id: plInfoTooltip
                    delay: 750
                }

                delegate: Item {
                    implicitWidth: delegate.width
                    implicitHeight: childrenRect.height

                    Loader {
                        anchors.top: delegate.top

                        active: (index === 0) // load only for the first element to prevent overlapping
                        width: parent.width
                        height: 1
                        z: (model.selected || delegate.hovered || delegate.activeFocus) ? 2 : 1
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
                        id: delegate
                        _model: root.model
                        width: root.width
                        z: 1
                        leftPadding: root.leftPadding + VLCStyle.margin_normal
                        rightPadding: root.rightPadding + listView.scrollBarWidth

                        onItemClicked : {
                            /* to receive keys events */
                            listView.forceActiveFocus()
                            if (listView.mode === PlaylistListView.Mode.Move) {
                                var selectedIndexes = root.model.getSelection()
                                if (selectedIndexes.length === 0)
                                    return
                                var preTarget = index
                                /* move to _above_ the clicked item if move up, but
                                 * _below_ the clicked item if move down */
                                if (preTarget > selectedIndexes[0])
                                    preTarget++
                                listView.currentIndex = selectedIndexes[0]
                                root.model.moveItemsPre(selectedIndexes, preTarget)
                                return
                            } else if (listView.mode === PlaylistListView.Mode.Select) {
                            } else if (!(root.model.isSelected(index) && button === Qt.RightButton)) {
                                listView.updateSelection(modifier, listView.currentIndex, index)
                                listView.currentIndex = index
                            }

                            if (button === Qt.RightButton)
                                contextMenu.popup(index, globalMousePos)
                        }
                        onItemDoubleClicked: {
                            if (listView.mode === PlaylistListView.Mode.Normal)
                                mainPlaylistController.goTo(index, true)
                        }

                        colors: root.colors

                        onDragStarting: {
                            if (!root.model.isSelected(index)) {
                                /* the dragged item is not in the selection, replace the selection */
                                root.model.setSelection([index])
                            }
                        }

                        function isDropAcceptable(drop, index) {
                            return root.isDropAcceptable(drop, index)
                        }

                        onDropedMovedAt: {
                            root.acceptDrop(target, drop)
                        }

                        onHoveredChanged: {
                            var bottomItemIndex = listView.listView.indexAt(delegate.width / 2, (listView.listView.contentY + listView.height) - 2)
                            var topItemIndex = listView.listView.indexAt(delegate.width / 2, listView.listView.contentY + 2)

                            if(bottomItemIndex !== -1 && model.index >= bottomItemIndex - 1)
                            {
                                listView.fadeRectBottomHovered = delegate.hovered
                            }
                            if(topItemIndex !== -1 && model.index <= topItemIndex + 1)
                            {
                                listView.fadeRectTopHovered = delegate.hovered
                            }
                        }
                    }

                    Rectangle {
                        id: bottomSeparator
                        anchors.top: delegate.bottom

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

                onSelectAll: root.model.selectAll()
                onSelectionUpdated: {
                    if (listView.mode === PlaylistListView.Mode.Select) {
                        console.log("update selection select")
                    } else if (listView.mode === PlaylistListView.Mode.Move) {
                        var selectedIndexes = root.model.getSelection()
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

                        listView.currentIndex = selectedIndexes[0]
                        /* the target is the position _after_ the move is applied */
                        root.model.moveItemsPost(selectedIndexes, target)
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
                    if (mode === PlaylistListView.Mode.Normal) {
                        root.navigationLeft(index)
                    } else {
                        mode = PlaylistListView.Mode.Normal
                    }
                }
                navigationCancel: function(index) {
                    if (mode === PlaylistListView.Mode.Normal) {
                        root.navigationCancel(index)
                    } else {
                        mode = PlaylistListView.Mode.Normal
                    }
                }

                navigationUp: function(index) {
                    if (mode === PlaylistListView.Mode.Normal)
                        root.navigationUp(index)
                }

                navigationDown: function(index) {
                    if (mode === PlaylistListView.Mode.Normal)
                        root.navigationDown(index)
                }

                onActionAtIndex: {
                    if (index < 0)
                        return

                    if (mode === PlaylistListView.Mode.Select)
                        root.model.toggleSelected(index)
                    else if (mode === PlaylistListView.Mode.Normal)
                        mainPlaylistController.goTo(index, true)
                }

                function onDelete() {
                    let selection = root.model.getSelection()
                    if (selection.length === 0)
                        return
                    root.model.removeItems(selection)
                }

                function _addRange(from, to) {
                    root.model.setRangeSelected(from, to - from + 1, true)
                }

                function _delRange(from, to) {
                    root.model.setRangeSelected(from, to - from + 1, false)
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
                            root.model.toggleSelected(newIndex)
                        } else {
                            root.model.setSelection([newIndex])
                        }
                    }
                }

                Column {
                    anchors.centerIn: parent
                    visible: model.count === 0

                    Widgets.IconLabel {
                        font.pixelSize: VLCStyle.dp(48, VLCStyle.scale)
                        anchors.horizontalCenter: parent.horizontalCenter
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        text: VLCIcons.playlist
                        color: listView.activeFocus ? colors.accent : colors.text
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
                        color: listView.activeFocus ? colors.accent : colors.text
                        opacity: 0.4
                    }

                    // ToDo: Use BodyLabel
                    Label {
                        anchors.topMargin: VLCStyle.margin_normal
                        text: i18n.qtr("Drag & Drop some content here!")
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: VLCStyle.fontSize_large
                        color: listView.activeFocus ? colors.accent : colors.text
                        opacity: 0.4
                    }
                }
            }

            PlaylistToolbar {
                id: toolbar

                Layout.fillWidth: true

                Layout.leftMargin: root.leftPadding
                Layout.rightMargin: root.rightPadding
            }
        }
    }

    Keys.priority: Keys.AfterItem
    Keys.forwardTo: listView
    Keys.onPressed: defaultKeyAction(event, 0)
}
