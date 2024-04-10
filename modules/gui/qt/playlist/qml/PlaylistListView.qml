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
import QtQuick.Templates as T
import QtQuick.Layouts
import QtQml.Models

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util" as Util
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///style/"

T.Pane {
    id: root

    property var model: PlaylistListModel {
        playlist: MainPlaylistController.playlist
    }
    readonly property ListSelectionModel selectionModel: listView?.selectionModel ?? null

    property bool useAcrylic: true

    readonly property real minimumWidth: contentItem.Layout.minimumWidth +
                                         leftPadding +
                                         rightPadding +
                                         2 * (VLCStyle.margin_xsmall)

    readonly property ListView listView: contentItem.listView

    property alias contextMenu: contextMenu

    property alias dragItem: dragItem

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)

    verticalPadding: VLCStyle.margin_normal

    Accessible.name: qsTr("Playqueue")

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View

        focused: root.activeFocus
        hovered: root.hovered
        enabled: root.enabled
    }

    function isDropAcceptable(drop, index) {
        if (drop.source === dragItem)
            return Helpers.itemsMovable(selectionModel.sortedSelectedIndexesFlat, index)
        else if (Helpers.isValidInstanceOf(drop.source, Widgets.DragItem))
            return true
        else if (drop.hasUrls)
            return true
        else
            return false
    }

    function acceptDrop(index, drop) {
        const item = drop.source;

        // NOTE: Move implementation.
        if (dragItem === item) {
            model.moveItemsPre(root.selectionModel.sortedSelectedIndexesFlat, index);
            listView.forceActiveFocus();
        // NOTE: Dropping medialibrary content into the queue.
        } else if (Helpers.isValidInstanceOf(item, Widgets.DragItem)) {
            return item.getSelectedInputItem().then((inputItems) => {
                    if (!Array.isArray(inputItems) || inputItems.length === 0) {
                        console.warn("can't convert items to input items");
                        return
                    }
                    MainPlaylistController.insert(index, inputItems, false)
                }).then(() => { listView.forceActiveFocus(); })
        // NOTE: Dropping an external item (i.e. filesystem) into the queue.
        } else if (drop.hasUrls) {
            const urlList = [];

            for (let url in drop.urls)
                urlList.push(drop.urls[url]);

            MainPlaylistController.insert(index, urlList, false);

            // NOTE This is required otherwise backend may handle the drop as well yielding double addition.
            drop.accept(Qt.IgnoreAction);
            listView.forceActiveFocus();
        }

        return Promise.resolve()
    }

    Widgets.DragItem {
        id: dragItem

        onRequestData: (indexes, resolve, reject) => {
            resolve(indexes.map((index) => {
                const item = root.model.itemAt(index)
                return {
                    "title": item.title,
                    "cover": (!!item.artwork && item.artwork.toString() !== "") ? item.artwork : VLCStyle.noArtAlbumCover
                }
            }))
        }

        onRequestInputItems: (indexes, data, resolve, reject) => {
            resolve(root.model.getItemsForIndexes(root.selectionModel.selectedIndexesFlat))
        }
    }

    PlaylistContextMenu {
        id: contextMenu
        model: root.model
        selectionModel: root.selectionModel
        controler: MainPlaylistController

        onJumpToCurrentPlaying: listView.positionViewAtIndex( MainPlaylistController.currentIndex, ItemView.Center)
    }

    background: Widgets.AcrylicBackground {
        enabled: root.useAcrylic
        tintColor: theme.bg.primary
    }

    contentItem: ColumnLayout {
        spacing: VLCStyle.margin_xxsmall

        Layout.minimumWidth: noContentInfoColumn.implicitWidth

        readonly property ListView listView: listView

        Column {
            Layout.fillHeight: false
            Layout.fillWidth: true
            Layout.leftMargin: VLCStyle.margin_normal

            spacing: VLCStyle.margin_xxxsmall

            Widgets.SubtitleLabel {
                text: qsTr("Playqueue")
                color: theme.fg.primary
                font.weight: Font.Bold
                font.pixelSize: VLCStyle.dp(24, VLCStyle.scale)
            }

            Widgets.CaptionLabel {
                color: theme.fg.secondary
                visible: model.count !== 0
                text: qsTr("%1 elements, %2").arg(model.count).arg(model.duration.formatLong())
            }
        }

        Item {
            // Spacer

            implicitHeight: VLCStyle.margin_xsmall
        }

        RowLayout {
            visible: model.count !== 0

            Layout.fillHeight: false
            Layout.leftMargin: VLCStyle.margin_normal
            Layout.rightMargin: Math.max(listView.ScrollBar.vertical.width, VLCStyle.margin_normal)

            spacing: VLCStyle.margin_large

            Widgets.IconLabel {
                // playlist cover column
                Layout.preferredWidth: VLCStyle.icon_playlistArt

                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: VLCIcons.album_cover
                font.pixelSize: VLCStyle.icon_playlistHeader

                color: theme.fg.secondary

                Accessible.role: Accessible.ColumnHeader
                Accessible.name: qsTr("Cover")
                Accessible.ignored: false
            }

            //Use Text here as we're redefining its Accessible.role
            Text {
                Layout.fillWidth: true

                elide: Text.ElideRight
                font.pixelSize: VLCStyle.fontSize_normal
                textFormat: Text.PlainText

                verticalAlignment: Text.AlignVCenter
                text: qsTr("Title")
                color: theme.fg.secondary

                Accessible.role: Accessible.ColumnHeader
                Accessible.name: text
            }

            Widgets.IconLabel {
                Layout.preferredWidth: durationMetric.width

                text: VLCIcons.time
                color: theme.fg.secondary
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: VLCStyle.icon_playlistHeader

                Accessible.role: Accessible.ColumnHeader
                Accessible.name: qsTr("Duration")
                Accessible.ignored: false

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

            model: root.model

            Binding on fadingEdge.enableBeginningFade {
                when: (autoScroller.scrollingDirection === Util.ViewDragAutoScrollHandler.Direction.Backward)
                value: false
            }

            Binding on fadingEdge.enableEndFade {
                when: (autoScroller.scrollingDirection === Util.ViewDragAutoScrollHandler.Direction.Forward)
                value: false
            }

            fadingEdge.backgroundColor: root.background.usingAcrylic ? "transparent"
                                                                     : listView.colorContext.bg.primary

            contentWidth: width

            property int shiftIndex: -1
            property Item itemContainsDrag: null

            onShowContextMenu: (globalPos) => {
                contextMenu.popup(-1, globalPos)
            }

            Connections {
                target: listView.model

                function onRowsInserted() {
                    if (listView.currentIndex === -1)
                        listView.currentIndex = 0
                }

                function onModelReset() {
                    if (listView.currentIndex === -1 && root.model.count > 0)
                        listView.currentIndex = 0
                }
            }

            Util.ViewDragAutoScrollHandler {
                id: autoScroller

                view: listView
                dragging: !!listView.itemContainsDrag && listView.itemContainsDrag !== listView.footerItem
                dragPosProvider: function () {
                    const source = listView.itemContainsDrag
                    const point = source.drag
                    return listView.mapFromItem(source, point.x, point.y)
                }
            }

            footer: Item {
                implicitWidth: parent.width

                Binding on implicitHeight {
                    delayed: true
                    value: Math.max(VLCStyle.icon_normal, listView.height - y)
                }

                property alias firstItemIndicatorVisible: firstItemIndicator.visible

                readonly property bool containsDrag: dropArea.containsDrag

                readonly property point drag: Qt.point(dropArea.drag.x, dropArea.drag.y)

                onContainsDragChanged: {
                    if (root.model.count > 0) {
                        listView.updateItemContainsDrag(this, containsDrag)
                    } else if (!containsDrag && listView.itemContainsDrag === this) {
                        // In case model count is changed somehow while
                        // containsDrag is set
                        listView.updateItemContainsDrag(this, false)
                    }
                }

                Rectangle {
                    id: firstItemIndicator

                    anchors.fill: parent
                    anchors.margins: VLCStyle.margin_small

                    border.width: VLCStyle.dp(2)
                    border.color: theme.accent

                    color: "transparent"

                    visible: (root.model.count === 0 && (dropArea.containsDrag || dropArea.dropOperationOngoing))

                    opacity: 0.8

                    Widgets.IconLabel {
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

                    onEntered: (drag) => {
                        if(!root.isDropAcceptable(drag, root.model.count)) {
                            drag.accepted = false
                            return
                        }
                    }

                    onDropped: (drop) => {
                        dropOperationOngoing = true
                        root.acceptDrop(root.model.count, drop)
                            .then(() => { dropOperationOngoing = false })
                    }
                }
            }

            Rectangle {
                id: dropIndicator

                parent: listView.itemContainsDrag

                z: 99

                anchors {
                    left: !!parent ? parent.left : undefined
                    right: !!parent ? parent.right : undefined
                    top: !!parent ? (parent.bottomContainsDrag === true ? parent.bottom : parent.top)
                                  : undefined
                }

                implicitHeight: VLCStyle.dp(1)

                visible: !!parent
                color: theme.accent
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

            delegate: PlaylistDelegate {
                id: delegate

                width: listView.contentWidth
                rightPadding: Math.max(listView.ScrollBar.vertical.width, VLCStyle.margin_normal)

                contextMenu: root.contextMenu

                dragItem: root.dragItem

                isDropAcceptable: root.isDropAcceptable
                acceptDrop: root.acceptDrop

                onContainsDragChanged: listView.updateItemContainsDrag(this, containsDrag)
            }

            add: Transition {
                SequentialAnimation {
                    PropertyAction {
                        // TODO: Remove this >= Qt 5.15
                        property: "opacity"
                        value: 0.0
                    }

                    OpacityAnimator {
                        from: 0.0 // QTBUG-66475
                        to: 1.0
                        duration: VLCStyle.duration_long
                        easing.type: Easing.OutSine
                    }
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

            Keys.onDeletePressed: model.removeItems(selectionModel.selectedIndexesFlat)

            Navigation.parentItem: root

            onActionAtIndex: (index) => {
                if (index < 0)
                    return

                MainPlaylistController.goTo(index, true)
            }

            Column {
                id: noContentInfoColumn

                anchors.centerIn: parent

                visible: false
                enabled: visible

                opacity: (listView.activeFocus) ? 1.0 : 0.4

                Binding on visible {
                    delayed: true
                    value: (listView.model.count === 0 && !listView.footerItem.firstItemIndicatorVisible)
                }

                Widgets.IconLabel {
                    id: label

                    anchors.horizontalCenter: parent.horizontalCenter

                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter

                    text: VLCIcons.playlist

                    color: theme.fg.primary

                    font.pixelSize: VLCStyle.dp(48, VLCStyle.scale)
                }

                T.Label {
                    anchors.topMargin: VLCStyle.margin_xlarge

                    anchors.horizontalCenter: parent.horizontalCenter

                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter

                    text: qsTr("No content yet")

                    color: label.color

                    font.pixelSize: VLCStyle.fontSize_xxlarge
                }

                T.Label {
                    anchors.topMargin: VLCStyle.margin_normal

                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter

                    text: qsTr("Drag & Drop some content here!")

                    color: label.color

                    font.pixelSize: VLCStyle.fontSize_large
                }
            }
        }

        PlaylistToolbar {
            id: toolbar

            Layout.preferredHeight: VLCStyle.heightBar_normal
            Layout.fillHeight: false
            Layout.fillWidth: true
            Layout.leftMargin: VLCStyle.margin_normal
            Layout.rightMargin: VLCStyle.margin_normal
        }
    }

    Keys.priority: Keys.AfterItem
    Keys.forwardTo: listView
    Keys.onPressed: (event) => root.Navigation.defaultKeyAction(event)
}
