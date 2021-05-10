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

import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"


Rectangle {
    id: delegate

    readonly property int selectionLength: root.model.selectedCount

    readonly property bool isLastItem: (index === listView.modelCount - 1)
    readonly property bool selected : model.selected

    property alias hovered: mouseArea.containsMouse

    border.width: VLCStyle.dp(1)
    border.color: {
        if (activeFocus && (listView.mode === PlaylistListView.Mode.Select))
            colors.caption
        else
            "transparent"
    }

    color: {
        if ((selected && activeFocus && listView.mode !== PlaylistListView.Mode.Select) || (hovered && selected))
            colors.plItemFocused
        else if (hovered)
            colors.plItemHovered
        else if (selected)
            colors.plItemSelected
        else
            "transparent"
    }

    height: artworkItem.height * 1.5

    onHoveredChanged: {
        if(hovered)
            showTooltip(false)
    }

    onSelectedChanged: {
        if(selected)
            showTooltip(true)
    }

    function showTooltip(selectAction) {
        plInfoTooltip.close()
        plInfoTooltip.text = Qt.binding(function() { return (textInfo.text + '\n' + textArtist.text); })
        plInfoTooltip.parent = textInfoColumn
        if (selectionLength > 1 && selectAction)
            plInfoTooltip.timeout = 2000
        else
            plInfoTooltip.timeout = 0
        plInfoTooltip.visible = Qt.binding(function() { return ( (selectAction ? selected : hovered) && !overlayMenu.visible && mainInterface.playlistVisible &&
                                                                (textInfo.implicitWidth > textInfo.width || textArtist.implicitWidth > textArtist.width)); })
    }

    Connections {
        target: listView

        onSetItemDropIndicatorVisible: {
            if (index === model.index) {
                topDropIndicator.visible = Qt.binding(function() { return visible || higherDropArea.containsDragItem; })
            }
        }
    }

    Rectangle {
        id: topDropIndicator

        anchors.left: parent.left
        anchors.right: parent.right

        height: VLCStyle.dp(1)
        anchors.top: parent.top

        visible: higherDropArea.containsDragItem
        color: colors.accent
    }

    MouseArea {
        id: mouseArea

        width: parent.width
        implicitHeight: childrenRect.height

        hoverEnabled: true

        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onContainsMouseChanged: {
            if (containsMouse) {
                var bottomItemIndex = listView.listView.indexAt(delegate.width / 2, (listView.listView.contentY + listView.height) + 1)
                var topItemIndex = listView.listView.indexAt(delegate.width / 2, listView.listView.contentY - 1)

                if(bottomItemIndex !== -1 && model.index >= bottomItemIndex - 1)
                {
                    listView.fadeRectBottomHovered = Qt.binding(function() {return delegate.hovered})
                }
                if(model.index <= topItemIndex + 1)
                {
                    listView.fadeRectTopHovered = Qt.binding(function() {return delegate.hovered})
                }
            }
        }

        onClicked: {
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
            } else if (!(root.model.isSelected(index) && mouse.button === Qt.RightButton)) {
                listView.updateSelection(mouse.modifiers, listView.currentIndex, index)
                listView.currentIndex = index
            }

            if (mouse.button === Qt.RightButton)
                contextMenu.popup(index, this.mapToGlobal(mouse.x, mouse.y))
        }

        onDoubleClicked: {
            if (mouse.button !== Qt.RightButton && listView.mode === PlaylistListView.Mode.Normal)
                mainPlaylistController.goTo(index, true)
        }

        drag.target: dragItem

        drag.onActiveChanged: {
            if (drag.active) {
                if (!selected) {
                    /* the dragged item is not in the selection, replace the selection */
                    root.model.setSelection([index])
                }

                dragItem.index = index
                dragItem.Drag.active = drag.active
            }
            else {
                dragItem.Drag.drop()
            }
        }

        onPositionChanged: {
            if (drag.active) {
                var pos = drag.target.parent.mapFromItem(mouseArea, mouseX, mouseY)
                dragItem.updatePos(pos)
            }
        }

        RowLayout {
            id: content

            height: delegate.height
            spacing: 0

            anchors {
                left: parent.left
                right: parent.right

                leftMargin: VLCStyle.margin_normal
                rightMargin: listView.scrollBarWidth
            }

            Item {
                id: artworkItem
                Layout.preferredHeight: VLCStyle.icon_normal
                Layout.preferredWidth: VLCStyle.icon_normal
                Layout.alignment: Qt.AlignVCenter

                DropShadow {
                    id: effect
                    anchors.fill: artwork
                    source: artwork
                    radius: 8
                    samples: 17
                    color: colors.glowColorBanner
                    visible: artwork.visible
                    spread: 0.1
                }

                Image {
                    id: artwork
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectFit
                    source: (model.artwork && model.artwork.toString()) ? model.artwork : VLCStyle.noArtCover
                    visible: !statusIcon.visible
                }

                Widgets.IconLabel {
                    id: statusIcon
                    anchors.fill: parent
                    visible: (model.isCurrent && text !== "")
                    width: height
                    height: VLCStyle.icon_normal
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: colors.accent
                    text: player.playingState === PlayerController.PLAYING_STATE_PLAYING ? VLCIcons.volume_high :
                                                    player.playingState === PlayerController.PLAYING_STATE_PAUSED ? VLCIcons.pause : ""
                }
            }

            ColumnLayout {
                id: textInfoColumn
                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.margin_large
                Layout.preferredHeight: artworkItem.height * 1.25
                spacing: 0

                Widgets.ListLabel {
                    id: textInfo

                    Layout.fillHeight: true
                    Layout.fillWidth: true

                    font.weight: model.isCurrent ? Font.Bold : Font.DemiBold
                    text: model.title
                    color: colors.text
                }

                Widgets.ListSubtitleLabel {
                    id: textArtist

                    Layout.fillHeight: true
                    Layout.fillWidth: true

                    font.weight: model.isCurrent ? Font.DemiBold : Font.Normal
                    text: (model.artist ? model.artist : i18n.qtr("Unknown Artist"))
                    color: colors.text
                }
            }

            Widgets.ListLabel {
                id: textDuration
                Layout.rightMargin: VLCStyle.margin_xsmall
                Layout.preferredWidth: durationMetric.width
                text: model.duration
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: colors.text
                opacity: 0.5
            }
        }

        ColumnLayout {
            anchors.fill: content
            spacing: 0

            DropArea {
                id: higherDropArea
                Layout.fillWidth: true
                Layout.fillHeight: true

                property bool containsDragItem: false

                onEntered: {
                    if (!isDropAcceptable(drag, index))
                        return

                    containsDragItem = true
                }
                onExited: {
                    containsDragItem = false
                }
                onDropped: {
                    if (!isDropAcceptable(drop, index))
                        return

                    root.acceptDrop(index, drop)

                    containsDragItem = false
                }
            }

            DropArea {
                id: lowerDropArea
                Layout.fillWidth: true
                Layout.fillHeight: true

                function handleDropIndicators(visible) {
                    if (isLastItem)
                        listView.footerItem.setDropIndicatorVisible(visible)
                    else
                        listView.setItemDropIndicatorVisible(index + 1, visible)
                }

                onEntered: {
                    if (!isDropAcceptable(drag, index + 1))
                        return

                    handleDropIndicators(true)
                }
                onExited: {
                    handleDropIndicators(false)
                }
                onDropped: {
                    if(!isDropAcceptable(drop, index + 1))
                        return

                    root.acceptDrop(index + 1, drop)

                    handleDropIndicators(false)
                }
            }
        }
    }
}
