/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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
import QtQuick.Window
import QtQuick.Templates as T
import QtQuick.Layouts
import QtQml.Models

import VLC.MediaLibrary

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util

T.ItemDelegate {
    id: root

    // Properties

    property ItemView view: ListView.view

    required property var model
    required property int index

    property bool isCurrent: false

    property bool selected: false

    required  property Widgets.MLDragItem dragTarget

    // Aliases
    // Private

    readonly property bool _isHover: contentItem.containsMouse || root.activeFocus

    // Settings

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    height: VLCStyle.play_cover_small + (VLCStyle.margin_xsmall * 2)

    verticalPadding: VLCStyle.margin_xsmall
    horizontalPadding: VLCStyle.margin_normal

    hoverEnabled: true

    Accessible.onPressAction: root.itemClicked()

    // Childs

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Item

        focused: root.activeFocus
        hovered: root.hovered
        enabled: root.enabled
    }

    // TODO: Qt bug 6.2: QTBUG-103604
    DoubleClickIgnoringItem {
        anchors.fill: parent

        TapHandler {
            gesturePolicy: TapHandler.ReleaseWithinBounds // TODO: Qt 6.2 bug: Use TapHandler.DragThreshold

            grabPermissions: TapHandler.CanTakeOverFromHandlersOfDifferentType | TapHandler.ApprovesTakeOverByAnything

            // We need this for extra information such as modifiers
            Component.onCompleted: {
                canceled.connect(initialAction) // DragHandler stole the event
            }

            onSingleTapped: (eventPoint, button) => {
                initialAction()

                if (!(root.selected && button === Qt.RightButton)) {
                    view.selectionModel.updateSelection(point.modifiers, view.currentIndex, index)
                    view.currentIndex = root.index
                }
            }

            onDoubleTapped: (eventPoint, button) => {
                if (button !== Qt.RightButton)
                    MediaLib.addAndPlay(model.id);
            }

            function initialAction() {
                root.forceActiveFocus(Qt.MouseFocusReason)
            }
        }

        DragHandler {
            acceptedDevices: PointerDevice.AllDevices & ~(PointerDevice.TouchScreen)

            target: null

            grabPermissions: PointerHandler.CanTakeOverFromHandlersOfDifferentType | PointerHandler.ApprovesTakeOverByAnything

            onActiveChanged: {
                const target = root.dragTarget
                if (target) {
                    if (active) {
                        if (!selected) {
                            view.selectionModel.select(index, ItemSelectionModel.ClearAndSelect)
                            view.currentIndex = root.index
                        }

                        target.Drag.active = true
                    } else {
                        target.Drag.drop()
                    }
                }
            }
        }
    }

    background: Widgets.AnimatedBackground {
        enabled: theme.initialized
        color: (root.isCurrent || root.selected) ? theme.bg.highlight : theme.bg.primary
        border.color: visualFocus ? theme.visualFocus : "transparent"

        Widgets.CurrentIndicator {
            anchors {
                left: parent.left
                leftMargin: VLCStyle.margin_xxxsmall
                verticalCenter: parent.verticalCenter
            }

            implicitHeight: parent.height * 3 / 4

            visible: isCurrent
        }
    }

    contentItem: RowLayout {
        spacing: VLCStyle.margin_xsmall

        Widgets.RoundImage {
            id: roundImage

            Layout.preferredHeight: VLCStyle.play_cover_small
            Layout.fillHeight: true
            Layout.preferredWidth: height

            radius: width

            source: (model.cover) ? model.cover
                                  : VLCStyle.noArtArtistSmall
            sourceSize: Qt.size(width * eDPR, height * eDPR)

            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

            readonly property real eDPR: MainCtx.effectiveDevicePixelRatio(Window.window)

            Rectangle {
                anchors.centerIn: parent
                anchors.alignWhenCentered: false

                implicitWidth: roundImage.paintedWidth + border.width
                implicitHeight: roundImage.paintedHeight + border.width

                z: -1

                radius: roundImage.effectiveRadius

                color: "transparent"

                border.width: VLCStyle.dp(1, VLCStyle.scale)

                border.color: (isCurrent || _isHover) ? theme.accent
                                                      : theme.border
            }
        }

        Widgets.TextAutoScroller {
            label: artistName

            forceScroll: root.isCurrent || root.visualFocus
            clip: scrolling

            implicitHeight: artistName.implicitHeight
            implicitWidth: artistName.implicitWidth

            Layout.fillWidth: true
            Layout.fillHeight: true

            Widgets.ListLabel {
                id: artistName

                anchors {
                    verticalCenter: parent.verticalCenter
                }

                text: (model.name) ? model.name
                                   : qsTr("Unknown artist")

                color: theme.fg.primary
            }
        }
    }
}
