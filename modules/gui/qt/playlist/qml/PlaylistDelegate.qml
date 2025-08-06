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

// import VLC.MainInterface // TODO: for vlcTick, not used for now due to Qt 6.2
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Playlist
import VLC.Player
import VLC.Util

T.Control {
    id: delegate

    // Properties

    property Flickable view: ListView.view

    readonly property bool selected : view.selectionModel.selectedIndexesFlat.includes(index)

    readonly property bool topContainsDrag: dropAreaLayout.higherDropArea.containsDrag

    readonly property bool bottomContainsDrag: dropAreaLayout.lowerDropArea.containsDrag

    readonly property bool containsDrag: (topContainsDrag || bottomContainsDrag)

    readonly property point dragPosition: mapFromItem(dropAreaLayout,
                                                      dropAreaLayout.dragPosition.x,
                                                      dropAreaLayout.dragPosition.y)

    readonly property Image artworkTextureProvider: contentItem?.artworkTextureProvider ?? null

    // Model roles:
    required property int index
    required property bool isCurrent
    required property url artwork
    required property url url
    required property string artist
    required property string album
    required property string title
    required property var duration // TODO: Qt 6.2 can not use value type vlcTick
    required property bool preparsed

    // Optional
    property var contextMenu

    // Optional, an item to show as drag target
    property Item dragItem

    // Optional, used to show the drop indicator
    property alias isDropAcceptable: dropAreaLayout.isDropAcceptable

    // Optional, but required to drop a drag
    property alias acceptDrop: dropAreaLayout.acceptDrop

    // Settings

    hoverEnabled: true

    verticalPadding: VLCStyle.margin_xxxsmall

    leftPadding: VLCStyle.margin_normal

    rightPadding: VLCStyle.margin_normal

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    height: Math.round(implicitHeight)

    ListView.delayRemove: dragHandler.active

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
                if (delegate.isCurrent)
                    return theme.accent

                // based on design, ColorContext can't handle this case
                if (!delegate.hovered)
                    return theme.indicator.alpha(0)

                return theme.indicator
            }
        }
    }

    contentItem: RowLayout {
        spacing: 0

        property alias artworkTextureProvider: artwork

        Item {
            id: artworkItem

            Layout.preferredHeight: VLCStyle.icon_playlistArt
            Layout.preferredWidth: VLCStyle.icon_playlistArt
            Layout.alignment: Qt.AlignVCenter

            Accessible.role: Accessible.Graphic
            Accessible.name: qsTr("Cover")
            Accessible.description: {
                if (delegate.isCurrent) {
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
                source: defaultSource
                visible: !statusIcon.visible
                asynchronous: true

                readonly property url targetSource: (delegate?.artwork.toString()) ? VLCAccessImage.uri(delegate.artwork) : VLCStyle.noArtAlbumCover
                readonly property url defaultSource: delegate.preparsed ? targetSource : ""

                onStatusChanged: {
                    if (source !== VLCStyle.noArtAlbumCover && status === Image.Error)
                        source = VLCStyle.noArtAlbumCover
                }

                function resetSource() {
                    artwork.source = Qt.binding(() => { return artwork.defaultSource })
                    artworkTimer.running = Qt.binding(() => { return artworkTimer.defaultRunning })
                }

                function removeSource() {
                    artworkTimer.running = false
                    artwork.source = ""
                }

                Component.onCompleted: {
                    delegate.ListView.reused.connect(artwork.resetSource)
                    delegate.ListView.pooled.connect(artwork.removeSource)
                }

                Timer {
                    id: artworkTimer
                    running: defaultRunning
                    readonly property bool defaultRunning: (artwork.status === Image.Null)
                    interval: VLCStyle.duration_long
                    onTriggered: {
                        // Remove the preparse guard, enough time has passed:
                        artwork.source = Qt.binding(() => { return artwork.targetSource; })
                    }
                }

                Rectangle {
                    // NOTE: If the image is opaque and if there is depth buffer, this rectangle
                    //       is not going to be painted by the graphics backend. Though, it will
                    //       still have its own scene graph node, as well as QML item.
                    // TODO: Investigate if using `ImageExt` just for its built-in background
                    //       coloring is worth it.
                    anchors.centerIn: parent
                    anchors.alignWhenCentered: false
                    width: parent.paintedWidth
                    height: parent.paintedHeight
                    z: -1

                    color: theme.bg.primary

                    visible: (artwork.status === Image.Ready)
                }

                Widgets.DefaultShadow {
                    visible: (artwork.status === Image.Ready)
                    z: -2
                }
            }

            Widgets.IconLabel {
                id: statusIcon

                anchors.centerIn: parent
                visible: (delegate.isCurrent && text !== "")
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
            spacing: VLCStyle.margin_xxxsmall

            Widgets.ListLabel {
                id: textInfo

                Layout.fillWidth: true

                font.weight: delegate.isCurrent ? Font.Bold : Font.DemiBold
                text: delegate.title || qsTr("Unknown Title")
                color: theme.fg.primary
            }

            Widgets.ListSubtitleLabel {
                id: textArtist

                Layout.fillWidth: true

                text: delegate.artist || (delegate.title.length === 0 ? delegate.url : "")
                color: theme.fg.primary
            }
        }

        Widgets.ListLabel {
            id: textDuration

            text: delegate.duration.formatHMS()
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: theme.fg.primary
            opacity: 0.5
        }
    }

    // TODO: Qt bug 6.2: QTBUG-103604
    DoubleClickIgnoringItem {
        anchors.fill: parent

        TapHandler {
            acceptedDevices: PointerDevice.AllDevices & ~(PointerDevice.TouchScreen)

            acceptedButtons: Qt.LeftButton | Qt.RightButton

            gesturePolicy: TapHandler.ReleaseWithinBounds // TODO: Qt 6.2 bug: Use TapHandler.DragThreshold

            grabPermissions: TapHandler.CanTakeOverFromHandlersOfDifferentType | TapHandler.ApprovesTakeOverByAnything

            onSingleTapped: (eventPoint, button) => {
                initialAction()

                if (!(delegate.selected && button === Qt.RightButton)) {
                    view.selectionModel.updateSelection(point.modifiers, view.currentIndex, index)
                    view.currentIndex = index
                }

                if (contextMenu && button === Qt.RightButton)
                    contextMenu.popup(index, parent.mapToGlobal(eventPoint.position.x, eventPoint.position.y))
            }

            onDoubleTapped: (eventPoint, button) => {
                if (button !== Qt.RightButton)
                    MainPlaylistController.goTo(index, true)
            }

            Component.onCompleted: {
                canceled.connect(initialAction)
            }

            function initialAction() {
                delegate.forceActiveFocus(Qt.MouseFocusReason)
            }
        }

        DragHandler {
            id: dragHandler

            acceptedDevices: PointerDevice.AllDevices & ~(PointerDevice.TouchScreen)

            target: null

            grabPermissions: PointerHandler.CanTakeOverFromHandlersOfDifferentType | PointerHandler.ApprovesTakeOverByAnything

            onActiveChanged: {
                if (dragItem) {
                    if (active) {
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
        }

        TapHandler {
            acceptedDevices: PointerDevice.TouchScreen

            grabPermissions: TapHandler.CanTakeOverFromHandlersOfDifferentType | TapHandler.ApprovesTakeOverByAnything

            onTapped: (eventPoint, button) => {
                MainPlaylistController.goTo(index, true)
            }

            onLongPressed: (eventPoint, button) => {
                if (contextMenu)
                    contextMenu.popup(index, point.scenePosition)
            }
        }
    }

    Widgets.ListViewExt.VerticalDropAreaLayout {
        id: dropAreaLayout
        anchors.fill: parent

        view: delegate.view
    }
}
