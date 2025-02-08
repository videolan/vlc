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


import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style

T.ItemDelegate {
    id: root

    // Properties

    property real pictureWidth: VLCStyle.colWidth(1)
    property real pictureHeight: pictureWidth
    property int titleTopMargin: VLCStyle.gridItemTitle_topMargin
    property int subtitleTopMargin: VLCStyle.gridItemSubtitle_topMargin
    property Item dragItem: null

    readonly property int selectedBorderWidth: VLCStyle.gridItemSelectedBorder

    property int _modifiersOnLastPress: Qt.NoModifier

    // if true, texts are horizontally centered, provided it can fit in pictureWidth
    property bool textAlignHCenter: false

    // if the item is selected
    property bool selected: false

    // Aliases

    property alias image: picture.source
    property alias cacheImage: picture.cacheImage
    property alias fallbackImage: picture.fallbackImageSource

    property alias fillMode: picture.fillMode

    property alias title: titleLabel.text
    property alias subtitle: subtitleTxt.text
    property alias subtitleVisible: subtitleTxt.visible
    property alias playCoverShowPlay: picture.playCoverShowPlay
    property alias playIconSize: picture.playIconSize
    property alias pictureRadius: picture.radius
    property alias effectiveRadius: picture.effectiveRadius
    property alias pictureOverlay: picture.imageOverlay

    property alias selectedShadow: selectedShadow
    property alias unselectedShadow: unselectedShadow

    // Signals

    signal playClicked
    signal addToPlaylistClicked
    signal itemClicked(int modifier)
    signal itemDoubleClicked(int modifier)
    signal contextMenuButtonClicked(Item menuParent, point globalMousePos)

    // Settings

    implicitWidth: layout.implicitWidth
    implicitHeight: layout.implicitHeight

    width: Math.round(implicitWidth)
    height: Math.round(implicitHeight)

    highlighted: (hovered || visualFocus)

    Accessible.role: Accessible.Cell
    Accessible.name: title
    Accessible.selected: root.selected
    Accessible.onPressAction: root.playClicked()

    Keys.onMenuPressed: root.contextMenuButtonClicked(picture, root.mapToGlobal(0,0))

    // States

    states: [
        State {
            name: "highlighted"
            when: highlighted

            PropertyChanges {
                target: selectedShadow
                opacity: 1.0
            }

            PropertyChanges {
                target: unselectedShadow
                opacity: 0
            }

            PropertyChanges {
                target: picture
                playCoverVisible: true
                playCoverOpacity: 1.0
            }

        }
    ]

    transitions: [
        Transition {
            from: ""
            to: "highlighted"
            // reversible: true // doesn't work

            SequentialAnimation {
                PropertyAction {
                    target: picture
                    properties: "playCoverVisible"
                }

                NumberAnimation {
                    properties: "opacity, playCoverOpacity"
                    duration: VLCStyle.duration_long
                    easing.type: Easing.InSine
                }
            }
        },

        Transition {
            from: "highlighted"
            to: ""

            SequentialAnimation {
                PropertyAction {
                    target: picture
                    property: "playCoverVisible"
                }

                NumberAnimation {
                    properties: "opacity, playCoverOpacity"
                    duration: VLCStyle.duration_long
                    easing.type: Easing.OutSine
                }
            }
        }
    ]

    // Childs

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Item

        focused: root.visualFocus
        hovered: root.hovered
    }

    // TODO: Qt bug 6.2: QTBUG-103604
    DoubleClickIgnoringItem {
        anchors.fill: parent

        DragHandler {
            id: dragHandler

            acceptedDevices: PointerDevice.AllDevices & ~(PointerDevice.TouchScreen)

            target: null

            grabPermissions: PointerHandler.CanTakeOverFromHandlersOfDifferentType | PointerHandler.ApprovesTakeOverByAnything

            onActiveChanged: {
                if (dragItem) {
                    if (active && !selected) {
                        root.itemClicked(root._modifiersOnLastPress)
                    }

                    if (active)
                        dragItem.Drag.active = true
                    else
                        dragItem.Drag.drop()
                }
            }
        }

        TapHandler {
            acceptedDevices: PointerDevice.AllDevices & ~(PointerDevice.TouchScreen)

            acceptedButtons: Qt.RightButton | Qt.LeftButton

            grabPermissions: TapHandler.CanTakeOverFromHandlersOfDifferentType | TapHandler.ApprovesTakeOverByAnything

            gesturePolicy: TapHandler.ReleaseWithinBounds // TODO: Qt 6.2 bug: Use TapHandler.DragThreshold

            onSingleTapped: (eventPoint, button) => {
                initialAction()

                // FIXME: The signals are messed up in this item.
                //        Right click does not fire itemClicked?
                if (button === Qt.RightButton)
                    contextMenuButtonClicked(picture, parent.mapToGlobal(eventPoint.position.x, eventPoint.position.y));
                else
                    root.itemClicked(point.modifiers);
            }

            onDoubleTapped: (eventPoint, button) => {
                if (button === Qt.LeftButton)
                    root.itemDoubleClicked(point.modifiers)
            }

            Component.onCompleted: {
                canceled.connect(initialAction)
            }

            function initialAction() {
                _modifiersOnLastPress = point.modifiers

                root.forceActiveFocus(Qt.MouseFocusReason)
            }
        }

        TapHandler {
            acceptedDevices: PointerDevice.TouchScreen

            grabPermissions: TapHandler.CanTakeOverFromHandlersOfDifferentType | TapHandler.ApprovesTakeOverByAnything

            onTapped: (eventPoint, button) => {
                root.itemClicked(Qt.NoModifier)
                root.itemDoubleClicked(Qt.NoModifier)
            }

            onLongPressed: {
                contextMenuButtonClicked(picture, parent.mapToGlobal(point.position.x, point.position.y));
            }
        }
    }

    background: AnimatedBackground {
        width: root.width + (selectedBorderWidth * 2)
        height: root.height + (selectedBorderWidth * 2)

        x: - selectedBorderWidth
        y: - selectedBorderWidth

        enabled: theme.initialized

        //don't show the backgroud unless selected
        color: root.selected ?  theme.bg.highlight : theme.bg.primary
        border.color: visualFocus ? theme.visualFocus : "transparent"
    }

    contentItem: ColumnLayout {
        id: layout

        // Raise the content item so that the handlers of the control
        // do not handle events that are to be handled by the handlers
        // of the content item. Raising the content item should be
        // fine because content item is supposed to be the foreground
        // item.
        z: 1

        spacing: 0

        Widgets.MediaCover {
            id: picture

            playCoverVisible: false
            playCoverOpacity: 0
            radius: VLCStyle.gridCover_radius
            color: theme.bg.secondary

            Layout.preferredWidth: root.width
            Layout.preferredHeight: (root.pictureHeight / root.pictureWidth) * root.width
            Layout.alignment: Qt.AlignCenter

            pictureWidth: root.pictureWidth
            pictureHeight: root.pictureHeight

            onPlayIconClicked: (point) => {
                // emulate a mouse click before delivering the play signal as to select the item
                // this helps in updating the selection and restore of initial index in the parent views
                root.itemClicked(point.modifiers)
                root.playClicked()
            }

            DefaultShadow {
                id: unselectedShadow

                anchors.centerIn: parent

                visible: opacity > 0

                sourceItem: parent

                width: picture.paintedWidth + viewportHorizontalOffset - Math.ceil(picture.padding) * 2
                height: picture.paintedHeight + viewportVerticalOffset - Math.ceil(picture.padding) * 2

                rectWidth: sourceSize.width
                rectHeight: sourceSize.height

                // TODO: Apply painted size's aspect ratio (constant) in source size
                sourceSize: Qt.size(128, 128)
            }

            DoubleShadow {
                id: selectedShadow

                anchors.centerIn: parent

                visible: opacity > 0
                opacity: 0

                sourceItem: parent

                width: picture.paintedWidth + viewportHorizontalOffset - Math.ceil(picture.padding) * 2
                height: picture.paintedHeight + viewportVerticalOffset - Math.ceil(picture.padding) * 2

                rectWidth: sourceSize.width
                rectHeight: sourceSize.height

                // TODO: Apply painted size's aspect ratio (constant) in source size
                sourceSize: Qt.size(128, 128)

                primaryVerticalOffset: VLCStyle.dp(6, VLCStyle.scale)
                primaryBlurRadius: VLCStyle.dp(18, VLCStyle.scale)

                secondaryVerticalOffset: VLCStyle.dp(32, VLCStyle.scale)
                secondaryBlurRadius: VLCStyle.dp(72, VLCStyle.scale)
            }
        }

        Widgets.TextAutoScroller {
            id: titleTextRect

            label: titleLabel
            forceScroll: highlighted
            visible: root.title !== ""
            clip: scrolling

            Layout.preferredWidth: Math.min(titleLabel.implicitWidth, root.width)
            Layout.preferredHeight: titleLabel.height
            Layout.topMargin: root.titleTopMargin
            Layout.alignment: root.textAlignHCenter ? Qt.AlignCenter : Qt.AlignLeft

            Widgets.ListLabel {
                id: titleLabel

                height: implicitHeight
                color: root.selected
                    ? theme.fg.highlight
                    : theme.fg.primary
            }
        }

        Widgets.MenuCaption {
            id: subtitleTxt

            visible: text !== ""
            text: root.subtitle
            elide: Text.ElideRight
            color: root.selected
                ? theme.fg.highlight
                : theme.fg.secondary

            Layout.preferredWidth: Math.min(root.width, implicitWidth)
            Layout.alignment: root.textAlignHCenter ? Qt.AlignCenter : Qt.AlignLeft
            Layout.topMargin: root.subtitleTopMargin

            ToolTip.delay: VLCStyle.delayToolTipAppear
            ToolTip.text: subtitleTxt.text
            ToolTip.visible: subtitleTxtMouseHandler.hovered

            HoverHandler {
                id: subtitleTxtMouseHandler
            }
        }
    }
}
