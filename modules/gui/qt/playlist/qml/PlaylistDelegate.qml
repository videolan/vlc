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

import org.videolan.vlc 0.1
import org.videolan.compat 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

T.ItemDelegate {
    id: delegate

    // Properties

    readonly property int selectionLength: root.model.selectedCount

    readonly property bool selected : model.selected

    readonly property bool topContainsDrag: higherDropArea.containsDrag

    readonly property bool bottomContainsDrag: lowerDropArea.containsDrag

    readonly property bool containsDrag: (topContainsDrag || bottomContainsDrag)

    // Settings

    topPadding: VLCStyle.playlistDelegate_verticalPadding

    bottomPadding: VLCStyle.playlistDelegate_verticalPadding

    leftPadding: VLCStyle.margin_normal

    rightPadding: Math.max(listView.scrollBarWidth, VLCStyle.margin_normal)

    implicitWidth: Math.max(background.implicitWidth,
                            contentItem.implicitWidth + leftPadding + rightPadding)

    implicitHeight: Math.max(background.implicitHeight,
                            contentItem.implicitHeight + topPadding + bottomPadding)

    ListView.delayRemove: mouseArea.drag.active

    T.ToolTip.visible: ( (visualFocus || hovered) &&
                         !overlayMenu.shown && MainCtx.playlistVisible &&
                         (textInfoColumn.implicitWidth > textInfoColumn.width) )

    T.ToolTip.timeout: (hovered ? 0 : VLCStyle.duration_humanMoment)

    T.ToolTip.text: (textInfo.text + '\n' + textArtist.text)

    T.ToolTip.delay: VLCStyle.delayToolTipAppear

    // Events

    // Functions

    function moveSelected() {
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
    }

    // Childs

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Item

        focused: delegate.activeFocus
        hovered: delegate.hovered
        enabled: delegate.enabled
    }

    background: Widgets.AnimatedBackground {
        backgroundColor: selected ? theme.bg.highlight : theme.bg.primary

        active: delegate.visualFocus
        animate: theme.initialized

        activeBorderColor: theme.visualFocus

        visible: animationRunning || active || selected || hovered
    }

    contentItem: RowLayout {
        spacing: 0

        Item {
            id: artworkItem

            Layout.preferredHeight: VLCStyle.icon_normal
            Layout.preferredWidth: VLCStyle.icon_normal
            Layout.alignment: Qt.AlignVCenter

            Accessible.role: Accessible.Graphic
            Accessible.name: I18n.qtr("Cover")
            Accessible.description: {
                if (model.isCurrent) {
                    if (Player.playingState === Player.PLAYING_STATE_PLAYING)
                        return I18n.qtr("Playing")
                    else if (Player.playingState === Player.PLAYING_STATE_PAUSED)
                        return I18n.qtr("Paused")
                }
                return I18n.qtr("Media cover")
            }

            Widgets.ScaledImage {
                id: artwork

                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                source: (model.artwork && model.artwork.toString()) ? model.artwork : VLCStyle.noArtAlbumCover
                visible: !statusIcon.visible
                asynchronous: true

                Widgets.DoubleShadow {
                    anchors.centerIn: parent
                    width: parent.paintedWidth
                    height: parent.paintedHeight

                    z: -1

                    primaryBlurRadius: VLCStyle.dp(3)
                    primaryVerticalOffset: VLCStyle.dp(1)

                    secondaryBlurRadius: VLCStyle.dp(14)
                    secondaryVerticalOffset: VLCStyle.dp(6)
                }
            }

            Widgets.IconLabel {
                id: statusIcon

                anchors.centerIn: parent
                visible: (model.isCurrent && text !== "")
                color: theme.accent
                text: {
                    if (Player.playingState === Player.PLAYING_STATE_PLAYING)
                        return VLCIcons.volume_high
                    else if (Player.playingState === Player.PLAYING_STATE_PAUSED)
                        return VLCIcons.pause
                    else
                        return ""
                }
            }
        }

        ColumnLayout {
            id: textInfoColumn

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: VLCStyle.margin_large
            spacing: VLCStyle.margin_xsmall

            Widgets.ListLabel {
                id: textInfo

                Layout.fillHeight: true
                Layout.fillWidth: true

                font.weight: model.isCurrent ? Font.Bold : Font.DemiBold
                text: model.title
                color: theme.fg.primary
                verticalAlignment: Text.AlignTop
            }

            Widgets.ListSubtitleLabel {
                id: textArtist

                Layout.fillHeight: true
                Layout.fillWidth: true

                font.weight: model.isCurrent ? Font.DemiBold : Font.Normal
                text: (model.artist ? model.artist : I18n.qtr("Unknown Artist"))
                color: theme.fg.primary
                verticalAlignment: Text.AlignBottom
            }
        }

        Widgets.ListLabel {
            id: textDuration

            text: model.duration.formatHMS()
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: theme.fg.primary
            opacity: 0.5
        }
    }

    MouseArea {
        id: mouseArea

        anchors.fill: parent

        hoverEnabled: true

        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onClicked: {
            /* to receive keys events */
            listView.forceActiveFocus()
            if (root.mode === PlaylistListView.Mode.Move) {
                moveSelected()
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
                dragItem.y = pos.y
            }
        }

        TouchScreenTapHandlerCompat {
            onTapped: {
                if (root.mode === PlaylistListView.Mode.Normal) {
                    mainPlaylistController.goTo(index, true)
                } else if (root.mode === PlaylistListView.Mode.Move) {
                    moveSelected()
                }
            }

            onLongPressed: {
                contextMenu.popup(index, point.scenePosition)
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

            onEntered: {
                if (!isDropAcceptable(drag, index + 1)) {
                    drag.accepted = false
                    return
                }
            }

            onDropped: {
                root.acceptDrop(index + 1, drop)
            }
        }
    }
}
