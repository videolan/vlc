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
import QtQuick.Templates 2.4 as T
import QtQuick.Layouts 1.11

import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

T.Control {
    id: delegate

    // Properties

    readonly property int selectionLength: root.model.selectedCount

    readonly property bool isLastItem: (index === listView.modelCount - 1)

    readonly property bool selected : model.selected

    // Settings

    topPadding: VLCStyle.margin_xxsmall

    bottomPadding: VLCStyle.margin_xxsmall

    leftPadding: VLCStyle.margin_normal

    rightPadding: Math.max(listView.scrollBarWidth, VLCStyle.margin_normal)

    implicitWidth: Math.max(background.implicitWidth,
                            contentItem.implicitWidth + leftPadding + rightPadding)

    implicitHeight: Math.max(background.implicitHeight,
                            contentItem.implicitHeight + topPadding + bottomPadding)

    ListView.delayRemove: mouseArea.drag.active

    // Events

    onHoveredChanged: {
        if (hovered)
            adjustTooltip()
    }

    onVisualFocusChanged: {
        if (visualFocus)
            adjustTooltip()
    }

    // Connections

    Connections {
        target: listView

        onSetItemDropIndicatorVisible: {
            if (index === model.index) {
                topDropIndicator.visible = Qt.binding(function() { return visible || higherDropArea.containsDrag; })
            }
        }
    }

    // Functions

    function adjustTooltip() {
        plInfoTooltip.close()
        plInfoTooltip.text = Qt.binding(function() { return (textInfo.text + '\n' + textArtist.text); })
        plInfoTooltip.parent = textInfoColumn
        if (hovered)
            plInfoTooltip.timeout = 0
        else
            plInfoTooltip.timeout = 2000
        plInfoTooltip.visible = Qt.binding(function() { return ( (visualFocus || hovered) && !mouseArea.drag.active &&
                                                                !overlayMenu.shown && MainCtx.playlistVisible &&
                                                                (textInfo.implicitWidth > textInfo.width || textArtist.implicitWidth > textArtist.width) ) })
    }

    // Childs

    background: Widgets.AnimatedBackground {
        color: {
            if (selected)
                return colors.gridSelect;
            else if (hovered)
                return colors.listHover;
            else
                return colors.setColorAlpha(colors.gridSelect, 0);
        }

        active: visualFocus

        activeBorderColor: colors.bgFocus

        visible: animationRunning || active || selected || hovered
    }

    contentItem: RowLayout {
        id: content

        spacing: 0

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

                mipmap: true
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
                text: {
                    if (Player.playingState === Player.PLAYING_STATE_PLAYING)
                        return VLCIcons.volume_high
                    if (Player.playingState === Player.PLAYING_STATE_PAUSED)
                        return VLCIcons.pause
                    return ""
                }
            }
        }

        ColumnLayout {
            id: textInfoColumn

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: VLCStyle.margin_large
            spacing: 0

            Widgets.ListLabel {
                id: textInfo

                Layout.fillHeight: true
                Layout.fillWidth: true

                font.weight: model.isCurrent ? Font.Bold : Font.DemiBold
                text: model.title
                color: colors.text
                verticalAlignment: Text.AlignTop
            }

            Widgets.ListSubtitleLabel {
                id: textArtist

                Layout.fillHeight: true
                Layout.fillWidth: true

                font.weight: model.isCurrent ? Font.DemiBold : Font.Normal
                text: (model.artist ? model.artist : I18n.qtr("Unknown Artist"))
                color: colors.text
                verticalAlignment: Text.AlignBottom
            }
        }

        Widgets.ListLabel {
            id: textDuration

            text: model.duration
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: colors.text
            opacity: 0.5
        }
    }

    Rectangle {
        id: topDropIndicator

        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }

        visible: higherDropArea.containsDrag

        height: VLCStyle.dp(1, VLCStyle.scale)

        color: colors.accent
    }

    MouseArea {
        id: mouseArea

        anchors.fill: parent

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
            if (root.mode === PlaylistListView.Mode.Move) {
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
            } else if (root.mode === PlaylistListView.Mode.Select) {
            } else if (!(root.model.isSelected(index) && mouse.button === Qt.RightButton)) {
                listView.updateSelection(mouse.modifiers, listView.currentIndex, index)
                listView.currentIndex = index
            }

            if (mouse.button === Qt.RightButton)
                contextMenu.popup(index, this.mapToGlobal(mouse.x, mouse.y))
        }

        onDoubleClicked: {
            if (mouse.button !== Qt.RightButton && root.mode === PlaylistListView.Mode.Normal)
                mainPlaylistController.goTo(index, true)
        }

        drag.target: dragItem

        drag.onActiveChanged: {
            if (drag.active) {
                if (!selected) {
                    /* the dragged item is not in the selection, replace the selection */
                    root.model.setSelection([index])
                }

                if (contains(mapFromItem(dragItem.parent, dragItem.x, dragItem.y))) {
                    // Force trigger entered signal in drop areas
                    // so that containsDrag work properly
                    dragItem.x = -1
                    dragItem.y = -1
                }

                dragItem.Drag.active = drag.active
            }
            else {
                dragItem.Drag.drop()
            }
        }

        onPositionChanged: {
            if (drag.active) {
                // FIXME: Override dragItem's position
                var pos = mapToItem(dragItem.parent, mouseX, mouseY)
                dragItem.x = pos.x + VLCStyle.dp(15)
                dragItem.y = pos.y // y should be changed after x
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        DropArea {
            id: higherDropArea

            Layout.fillWidth: true
            Layout.fillHeight: true

            onEntered: {
                if (!isDropAcceptable(drag, index)) {
                    drag.accepted = false
                    return
                }
            }

            onDropped: {
                root.acceptDrop(index, drop)
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
                if (!isDropAcceptable(drag, index + 1)) {
                    drag.accepted = false
                    return
                }

                handleDropIndicators(true)
            }

            onExited: {
                handleDropIndicators(false)
            }

            onDropped: {
                root.acceptDrop(index + 1, drop)
                handleDropIndicators(false)
            }
        }
    }
}
