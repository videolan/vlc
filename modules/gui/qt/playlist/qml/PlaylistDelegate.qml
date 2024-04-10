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
import "qrc:///style/"

T.ItemDelegate {
    id: delegate

    // Properties

    property Flickable view: ListView.view

    readonly property bool selected : view.selectionModel.selectedIndexesFlat.includes(index)

    readonly property bool topContainsDrag: higherDropArea.containsDrag

    readonly property bool bottomContainsDrag: lowerDropArea.containsDrag

    readonly property bool containsDrag: (topContainsDrag || bottomContainsDrag)

    // drag -> point
    // current drag pos inside the item
    readonly property point drag: {
        if (!containsDrag)
            return Qt.point(0, 0)

        const d = topContainsDrag ? higherDropArea : lowerDropArea
        const p = d.drag
        return mapFromItem(d, p.x, p.y)
    }


    // Optional
    property var contextMenu

    // Optional, an item to show as drag target
    property Item dragItem

    // Optional, used to show the drop indicator
    property var isDropAcceptable

    // Optional, but required to drop a drag
    property var acceptDrop

    // Settings

    verticalPadding: VLCStyle.playlistDelegate_verticalPadding

    leftPadding: VLCStyle.margin_normal

    rightPadding: VLCStyle.margin_normal

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    ListView.delayRemove: mouseArea.drag.active

    T.ToolTip.visible: ( visible && (visualFocus || hovered) &&
                         (textInfoColumn.implicitWidth > textInfoColumn.width) )

    // NOTE: This is useful for keyboard navigation on a column, to avoid blocking visibility on
    //       the surrounding items.
    T.ToolTip.timeout: (visualFocus) ? VLCStyle.duration_humanMoment : 0

    T.ToolTip.text: (textInfo.text + '\n' + textArtist.text)

    T.ToolTip.delay: VLCStyle.delayToolTipAppear

    // Events

    // Functions

    // Childs

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Item

        focused: delegate.activeFocus
        hovered: delegate.hovered
        enabled: delegate.enabled
    }

    background: Widgets.AnimatedBackground {
        color: selected ? theme.bg.highlight : theme.bg.primary

        enabled: theme.initialized

        border.color: delegate.visualFocus ? theme.visualFocus : "transparent"

        Widgets.CurrentIndicator {
            anchors {
                left: parent.left
                leftMargin: VLCStyle.margin_xxsmall
                verticalCenter: parent.verticalCenter
            }

            implicitHeight: parent.height * 3 / 4

            color: {
                if (model.isCurrent)
                    return theme.accent

                // based on design, ColorContext can't handle this case
                if (!delegate.hovered)
                    return VLCStyle.setColorAlpha(theme.indicator, 0)

                return theme.indicator
            }
        }
    }

    contentItem: RowLayout {
        spacing: 0

        Item {
            id: artworkItem

            Layout.preferredHeight: VLCStyle.icon_playlistArt
            Layout.preferredWidth: VLCStyle.icon_playlistArt
            Layout.alignment: Qt.AlignVCenter

            Accessible.role: Accessible.Graphic
            Accessible.name: qsTr("Cover")
            Accessible.description: {
                if (model.isCurrent) {
                    if (Player.playingState === Player.PLAYING_STATE_PLAYING)
                        return qsTr("Playing")
                    else if (Player.playingState === Player.PLAYING_STATE_PAUSED)
                        return qsTr("Paused")
                }
                return qsTr("Media cover")
            }

            Widgets.ScaledImage {
                id: artwork

                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                source: (model?.artwork.toString()) ? VLCAccessImage.uri(model.artwork) : VLCStyle.noArtAlbumCover
                visible: !statusIcon.visible
                asynchronous: true

                Widgets.DefaultShadow {
                    anchors.centerIn: parent

                    sourceItem: parent

                    visible: (artwork.status === Image.Ready)
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
                        return VLCIcons.pause_filled
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

                text: model?.artist ?? qsTr("Unknown Artist")
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

        onClicked: (mouse) => {
            /* to receive keys events */
            if (!(delegate.selected && mouse.button === Qt.RightButton)) {
                view.selectionModel.updateSelection(mouse.modifiers, view.currentIndex, index)
                view.currentIndex = index
            }

            if (contextMenu && mouse.button === Qt.RightButton)
                contextMenu.popup(index, mapToGlobal(mouse.x, mouse.y))
        }

        onDoubleClicked: (mouse) => {
            if (mouse.button !== Qt.RightButton)
                MainPlaylistController.goTo(index, true)
        }

        onPressed: (mouse) => {
            delegate.forceActiveFocus(Qt.MouseFocusReason)
        }

        drag.target: dragItem

        drag.smoothed: false

        drag.onActiveChanged: {
            if (dragItem) {
                if (drag.active) {
                    if (!selected) {
                        /* the dragged item is not in the selection, replace the selection */
                        view.selectionModel.select(index, ItemSelectionModel.ClearAndSelect)
                    }

                    dragItem.indexes = view.selectionModel.selectedIndexesFlat
                    dragItem.indexesFlat = true
                    dragItem.Drag.active = true
                } else {
                    dragItem.Drag.drop()
                }
            }
        }

        TapHandler {
            acceptedDevices: PointerDevice.TouchScreen
            
            onTapped: {
                MainPlaylistController.goTo(index, true)
            }

            onLongPressed: {
                if (contextMenu)
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

            onEntered: (drag) => {
                if (!acceptDrop) {
                    drag.accept = false
                    return
                }

                if (isDropAcceptable && !isDropAcceptable(drag, index)) {
                    drag.accepted = false
                    return
                }
            }

            onDropped: (drop) => {
                console.assert(acceptDrop)
                acceptDrop(index, drop)
            }
        }

        DropArea {
            id: lowerDropArea

            Layout.fillWidth: true
            Layout.fillHeight: true

            onEntered: (drag) =>  {
                if (!acceptDrop) {
                    drag.accept = false
                    return
                }

                if (isDropAcceptable && !isDropAcceptable(drag, index + 1)) {
                    drag.accepted = false
                    return
                }
            }

            onDropped: (drop) => {
                console.assert(acceptDrop)
                acceptDrop(index + 1, drop)
            }
        }
    }
}
