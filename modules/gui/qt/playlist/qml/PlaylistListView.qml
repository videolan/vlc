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
import QtQuick.Layouts 1.11
import QtQml.Models 2.2
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///style/"

Control {
    id: root

    property alias model: listView.model

    property alias useAcrylic: acrylicBackground.enabled

    readonly property real minimumWidth: noContentInfoColumn.implicitWidth +
                                         leftPadding +
                                         rightPadding +
                                         2 * (VLCStyle.margin_xsmall)

    topPadding: VLCStyle.margin_normal
    bottomPadding: VLCStyle.margin_normal

    onActiveFocusChanged: if (activeFocus) listView.forceActiveFocus(focusReason)

    property VLCColors colors: VLCStyle.colors

    enum Mode {
        Normal,
        Select, // Keyboard item selection mode, activated through PlaylistOverlayMenu
        Move // Keyboard item move mode, activated through PlaylistOverlayMenu
    }

    function isDropAcceptable(drop, index) {
        if (drop.hasUrls)
            return true // external drop (i.e. from filesystem)

        if (Helpers.isValidInstanceOf(drop.source, Widgets.DragItem)) {
            // internal drop (inter-view or intra-playlist)
            var selection = drop.source.selection
            if (!!selection) {
                var length = selection.length
                var firstIndex = selection[0]
                var lastIndex = selection[length - 1]
                var consecutive = true
                if (length > 1) {
                    for (var i = 0; i < length - 1; ++i) {
                        if (selection[i + 1] - selection[i] !== 1) {
                            consecutive = false
                            break
                        }
                    }
                }
                return !consecutive || (index > lastIndex + 1 || index < firstIndex)
            } else {
                return true
            }
        }

        return false
    }

    function acceptDrop(index, drop) {
        var item = drop.source;

        // NOTE: Move implementation.
        if (dragItem == item) {
            model.moveItemsPre(model.getSelection(), index);

        // NOTE: Dropping medialibrary content into the queue.
        } else if (Helpers.isValidInstanceOf(item, Widgets.DragItem)) {
            mainPlaylistController.insert(index, item.getSelectedInputItem());

        // NOTE: Dropping an external item (i.e. filesystem) into the queue.
        } else if (drop.hasUrls) {
            var urlList = [];

            for (var url in drop.urls)
                urlList.push(drop.urls[url]);

            mainPlaylistController.insert(index, urlList, false);

            // NOTE This is required otherwise backend may handle the drop as well yielding double addition.
            drop.accept(Qt.IgnoreAction);
        }

        listView.forceActiveFocus();
    }

    Loader {
        id: overlayMenu
        anchors.fill: parent
        z: 1

        active: mainInterface.playlistDocked

        readonly property bool shown: (status === Loader.Ready) ? item.visible : false

        function open() {
            if (status === Loader.Ready)
                item.open()
        }

        sourceComponent: PlaylistOverlayMenu {
            colors: root.colors

            isRight: true
            rightPadding: VLCStyle.margin_xsmall + VLCStyle.applicationHorizontalMargin
            bottomPadding: VLCStyle.margin_large + root.bottomPadding

            itemParent: listView
            backgroundItem: contentItem
        }
    }

    Widgets.DragItem {
        id: dragItem

        parent: (typeof g_mainDisplay !== 'undefined') ? g_mainDisplay : root

        property var selection: null

        colors: root.colors

        function updateComponents(maxCovers) {
            var count = root.model.selectedCount
            selection = root.model.getSelection()
            var _selection = selection.slice(0, maxCovers)

            var title = _selection.map(function (index){
                return root.model.itemAt(index).title
            }).join(", ")

            var covers = _selection.map(function (index) {
                var artwork = root.model.itemAt(index).artwork
                return {artwork: (artwork && artwork.toString()) ? artwork : VLCStyle.noArtCover}
            })

            return ({covers: covers, title: title, count: root.model.selectedCount})
        }

        function getSelectedInputItem(index) {
            return model.getItemsForIndexes(model.getSelection())
        }

        property int _scrollingDirection: 0

        onYChanged: {
            var dragItemY = dragItem.y
            var viewY     = root.mapFromItem(listView, listView.x, listView.y).y

            var margin = VLCStyle.dp(20, VLCStyle.scale)

            var topDiff    = (viewY + margin) - dragItemY
            var bottomDiff = dragItemY - (viewY + listView.height - toolbar.height - margin)

            if (topDiff > 0)
                _scrollingDirection = -1
            else if (bottomDiff > 0)
                _scrollingDirection = 1
            else
                _scrollingDirection = 0
        }

        SmoothedAnimation {
            id: upAnimation
            target: listView
            property: "contentY"
            to: 0
            running: dragItem._scrollingDirection === -1 && dragItem.visible && !target.listView.atYBeginning

            velocity: VLCStyle.dp(225, VLCStyle.scale)

            onRunningChanged: {
                target.fadeRectTopHovered = running
            }
        }

        SmoothedAnimation {
            id: downAnimation
            target: listView
            property: "contentY"
            to: target.contentHeight - target.height + target.footerItem.height
            running: dragItem._scrollingDirection === 1 && dragItem.visible && !target.listView.atYEnd

            velocity: VLCStyle.dp(225, VLCStyle.scale)

            onRunningChanged: {
                target.fadeRectBottomHovered = running
            }
        }
    }

    PlaylistContextMenu {
        id: contextMenu
        model: root.model
        controler: mainPlaylistController
    }

    background: Widgets.AcrylicBackground {
        id: acrylicBackground

        alternativeColor: colors.bgAlt
    }

    contentItem: ColumnLayout {
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
            Layout.bottomMargin: VLCStyle.margin_xxsmall
            Layout.leftMargin: VLCStyle.margin_normal
            Layout.rightMargin: Math.max(listView.scrollBarWidth, VLCStyle.margin_normal)

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
                Layout.preferredWidth: durationMetric.width

                text: VLCIcons.time
                color: colors.caption
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

                TextMetrics {
                    id: durationMetric

                    font.weight: Font.DemiBold
                    font.pixelSize: VLCStyle.fontSize_normal
                    text: "00:00"
                }
            }
        }

        Widgets.KeyNavigableListView {
            id: listView

            Layout.fillWidth: true
            Layout.fillHeight: true

            focus: true

            clip: true // else out of view items will overlap with surronding items

            model: PlaylistListModel {
                playlistId: mainctx.playlist
            }

            fadeColor: background.usingAcrylic ? undefined
                                               : background.alternativeColor

            property int shiftIndex: -1
            property int mode: PlaylistListView.Mode.Normal

            signal setItemDropIndicatorVisible(int index, bool visible)

            Connections {
                target: listView.model

                onRowsInserted: {
                    if (listView.currentIndex === -1)
                        listView.currentIndex = 0
                }

                onModelReset: {
                    if (listView.currentIndex === -1 && root.model.count > 0)
                        listView.currentIndex = 0
                }

                onSelectedCountChanged: {
                    var selectedIndexes = listView.model.getSelection()

                    if (listView.modelCount === 0 || selectedIndexes.length === 0)
                        return

                    var bottomItemIndex = listView.listView.indexAt(listView.width / 2, (listView.listView.contentY + listView.height) + 1)
                    var topItemIndex = listView.listView.indexAt(listView.width / 2, listView.listView.contentY - 1)

                    if (listView.model.isSelected(topItemIndex) || (listView.model.isSelected(topItemIndex + 1)))
                        listView.fadeRectTopHovered = true
                    else
                        listView.fadeRectTopHovered = false

                    if (listView.model.isSelected(bottomItemIndex) || (bottomItemIndex !== -1 && listView.model.isSelected(bottomItemIndex - 1)))
                        listView.fadeRectBottomHovered = true
                    else
                        listView.fadeRectBottomHovered = false
                }
            }

            footer: Item {
                width: parent.width
                height: Math.max(VLCStyle.icon_normal, listView.height - y)

                property alias firstItemIndicatorVisible: firstItemIndicator.visible

                function setDropIndicatorVisible(visible) {
                    dropIndicator.visible = Qt.binding(function() { return (visible || dropArea.containsDrag); })
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton | Qt.LeftButton

                    onClicked: {
                        listView.forceActiveFocus()

                        if ( mouse.button === Qt.LeftButton || mouse.button === Qt.RightButton ) {
                            root.model.deselectAll()
                        }

                        if ( mouse.button === Qt.RightButton ) {
                            contextMenu.popup(-1, this.mapToGlobal(mouse.x, mouse.y))
                        }
                    }
                }

                Rectangle {
                    id: dropIndicator

                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: VLCStyle.dp(1)
                    anchors.top: parent.top

                    visible: (root.model.count > 0 && dropArea.containsDrag)
                    color: colors.accent
                }

                Rectangle {
                    id: firstItemIndicator

                    anchors.fill: parent
                    anchors.margins: VLCStyle.margin_small

                    border.width: VLCStyle.dp(2)
                    border.color: colors.accent

                    color: "transparent"

                    visible: (root.model.count === 0 && dropArea.containsDrag)

                    opacity: 0.8

                    Label {
                        anchors.centerIn: parent

                        text: VLCIcons.add

                        font.pointSize: VLCStyle.fontHeight_xxxlarge

                        font.family: VLCIcons.fontFamily
                        color: colors.accent
                    }
                }

                DropArea {
                    id: dropArea

                    anchors.fill: parent

                    onEntered: {
                        if(!root.isDropAcceptable(drag, root.model.count)) {
                            drag.accepted = false
                            return
                        }
                    }

                    onDropped: {
                        root.acceptDrop(root.model.count, drop)
                    }
                }
            }

            ToolTip {
                id: plInfoTooltip
                delay: 750
            }

            delegate: PlaylistDelegate {
                id: delegate

                // Instead of property forwarding, PlaylistDelegate is tightly coupled with PlaylistlistView
                // since PlaylistDelegate is expected to be used only within PlaylistlistView

                width: listView.width
            }

            add: Transition {
                NumberAnimation {
                    property: "opacity"; from: 0; to: 1.0

                    duration: VLCStyle.duration_normal
                }
            }

            displaced: Transition {
                NumberAnimation {
                    properties: "x,y"

                    duration: VLCStyle.duration_normal; easing.type: Easing.OutSine
                }

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

            Navigation.parentItem: root
            Navigation.rightAction: function() {
                overlayMenu.open()
            }
            Navigation.leftAction: function() {
                if (mode === PlaylistListView.Mode.Normal) {
                    root.Navigation.defaultNavigationLeft()
                } else {
                    mode = PlaylistListView.Mode.Normal
                }
            }
            Navigation.cancelAction: function() {
                if (mode === PlaylistListView.Mode.Normal) {
                    root.Navigation.defaultNavigationCancel()
                } else {
                    mode = PlaylistListView.Mode.Normal
                }
            }

            Navigation.upAction: function() {
                if (mode === PlaylistListView.Mode.Normal)
                    root.Navigation.defaultNavigationUp()
            }

            Navigation.downAction: function() {
                if (mode === PlaylistListView.Mode.Normal)
                    root.Navigation.defaultNavigationDown()
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
                var selection = root.model.getSelection()
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
                id: noContentInfoColumn

                anchors.centerIn: parent

                visible: (model.count === 0 && !listView.footerItem.firstItemIndicatorVisible)

                opacity: (listView.activeFocus) ? 1.0 : 0.4

                Widgets.IconLabel {
                    id: label

                    anchors.horizontalCenter: parent.horizontalCenter

                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter

                    text: VLCIcons.playlist

                    color: (listView.activeFocus) ? colors.bgFocus
                                                  : colors.text

                    font.pixelSize: VLCStyle.dp(48, VLCStyle.scale)
                }

                Label {
                    anchors.topMargin: VLCStyle.margin_xlarge

                    anchors.horizontalCenter: parent.horizontalCenter

                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter

                    text: i18n.qtr("No content yet")

                    color: label.color

                    font.pixelSize: VLCStyle.fontSize_xxlarge
                }

                Label {
                    anchors.topMargin: VLCStyle.margin_normal

                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter

                    text: i18n.qtr("Drag & Drop some content here!")

                    color: label.color

                    font.pixelSize: VLCStyle.fontSize_large
                }
            }
        }

        PlaylistToolbar {
            id: toolbar

            Layout.fillWidth: true
        }
    }

    Keys.priority: Keys.AfterItem
    Keys.forwardTo: listView
    Keys.onPressed: root.Navigation.defaultKeyAction(event)
}
