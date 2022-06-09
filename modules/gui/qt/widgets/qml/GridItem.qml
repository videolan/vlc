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
import QtQml.Models 2.2
import QtGraphicalEffects 1.0
import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///style/"

T.Control {
    id: root

    // Properties

    property real pictureWidth: VLCStyle.colWidth(1)
    property real pictureHeight: pictureWidth
    property int titleMargin: VLCStyle.margin_xsmall
    property Item dragItem: null

    readonly property bool highlighted: (mouseArea.containsMouse || visualFocus)

    readonly property int selectedBorderWidth: VLCStyle.gridItemSelectedBorder

    property int _modifiersOnLastPress: Qt.NoModifier

    // if true, texts are horizontally centered, provided it can fit in pictureWidth
    property bool textAlignHCenter: false

    // if the item is selected
    property bool selected: false

    // Aliases

    property alias image: picture.source
    property alias title: titleLabel.text
    property alias subtitle: subtitleTxt.text
    property alias playCoverBorderWidth: picture.playCoverBorderWidth
    property alias playCoverShowPlay: picture.playCoverShowPlay
    property alias playIconSize: picture.playIconSize
    property alias pictureRadius: picture.radius
    property alias pictureOverlay: picture.imageOverlay
    property alias unselectedUnderlay: unselectedUnderlayLoader.sourceComponent
    property alias selectedUnderlay: selectedUnderlayLoader.sourceComponent

    // Signals

    signal playClicked
    signal addToPlaylistClicked
    signal itemClicked(Item menuParent, int key, int modifier)
    signal itemDoubleClicked(Item menuParent, int keys, int modifier)
    signal contextMenuButtonClicked(Item menuParent, point globalMousePos)

    // Settings

    implicitWidth: layout.implicitWidth
    implicitHeight: layout.implicitHeight

    Accessible.role: Accessible.Cell
    Accessible.name: title

    Keys.onMenuPressed: root.contextMenuButtonClicked(picture, root.mapToGlobal(0,0))

    // States

    states: [
        State {
            name: "highlighted"
            when: highlighted

            PropertyChanges {
                target: selectedUnderlayLoader
                opacity: 1
                visible: true
            }

            PropertyChanges {
                target: unselectedUnderlayLoader
                opacity: 0
                visible: false
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
                    targets: [picture, selectedUnderlayLoader]
                    properties: "visible, playCoverVisible"
                }

                NumberAnimation {
                    properties: "opacity, playCoverOpacity"
                    duration: VLCStyle.duration_long
                    easing.type: Easing.InSine
                }

                PropertyAction {
                    target: unselectedUnderlayLoader
                    property: "visible"
                }
            }
        },

        Transition {
            from: "highlighted"
            to: ""

            SequentialAnimation {
                PropertyAction {
                    target: unselectedUnderlayLoader
                    property: "visible, playCoverVisible"
                }

                NumberAnimation {
                    properties: "opacity, playCoverOpacity"
                    duration: VLCStyle.duration_long
                    easing.type: Easing.OutSine
                }

                PropertyAction {
                    targets: [picture, selectedUnderlayLoader]
                    properties: "visible"
                }
            }
        }
    ]

    // Childs

    background: AnimatedBackground {
        id: background

        width: root.width + (selectedBorderWidth * 2)
        height: root.height + (selectedBorderWidth * 2)

        x: - selectedBorderWidth
        y: - selectedBorderWidth

        active: visualFocus

        backgroundColor: root.selected
                         ? VLCStyle.colors.gridSelect
                         : VLCStyle.colors.setColorAlpha(VLCStyle.colors.gridSelect, 0)

        visible: animationRunning || background.active || root.selected
    }

    contentItem: MouseArea {
        id: mouseArea

        implicitWidth: layout.implicitWidth
        implicitHeight: layout.implicitHeight

        acceptedButtons: Qt.RightButton | Qt.LeftButton

        hoverEnabled: true

        drag.target: root.dragItem

        drag.axis: Drag.XAndYAxis

        onClicked: {
            if (mouse.button === Qt.RightButton)
                contextMenuButtonClicked(picture, root.mapToGlobal(mouse.x,mouse.y));
            else {
                root.itemClicked(picture, mouse.button, mouse.modifiers);
            }
        }

        onDoubleClicked: {
            if (mouse.button === Qt.LeftButton)
                root.itemDoubleClicked(picture,mouse.buttons, mouse.modifiers)
        }

        onPressed: _modifiersOnLastPress = mouse.modifiers

        onPositionChanged: {
            if (drag.active) {
                var pos = drag.target.parent.mapFromItem(root, mouseX, mouseY)
                drag.target.x = pos.x + VLCStyle.dragDelta
                drag.target.y = pos.y + VLCStyle.dragDelta
            }
        }

        drag.onActiveChanged: {
            // perform the "click" action because the click action is only executed on mouse release (we are in the pressed state)
            // but we will need the updated list on drop
            if (drag.active && !selected) {
                root.itemClicked(picture, Qt.LeftButton, root._modifiersOnLastPress)
            } else if (root.dragItem) {
                root.dragItem.Drag.drop()
            }
            root.dragItem.Drag.active = drag.active
        }

        Loader {
            id: unselectedUnderlayLoader

            asynchronous: true
        }

        Loader {
            id: selectedUnderlayLoader

            asynchronous: true
            active: false
            visible: false
            opacity: 0

            onVisibleChanged: {
                if (visible && !active)
                    active = true
            }
        }

        ColumnLayout {
            id: layout

            anchors.centerIn: parent
            spacing: 0

            Widgets.MediaCover {
                id: picture

                playCoverVisible: false
                playCoverOpacity: 0
                radius: VLCStyle.gridCover_radius

                Layout.preferredWidth: pictureWidth
                Layout.preferredHeight: pictureHeight

                onPlayIconClicked: root.playClicked()
            }

            Widgets.ScrollingText {
                id: titleTextRect

                label: titleLabel
                forceScroll: highlighted
                visible: root.title !== ""
                clip: scrolling

                Layout.preferredWidth: Math.min(titleLabel.implicitWidth, pictureWidth)
                Layout.preferredHeight: titleLabel.height
                Layout.topMargin: root.titleMargin
                Layout.alignment: root.textAlignHCenter ? Qt.AlignCenter : Qt.AlignLeft

                Widgets.ListLabel {
                    id: titleLabel

                    height: implicitHeight
                    color: background.foregroundColor
                    textFormat: Text.PlainText
                }
            }

            Widgets.MenuCaption {
                id: subtitleTxt

                visible: text !== ""
                text: root.subtitle
                elide: Text.ElideRight
                color: background.foregroundColor
                textFormat: Text.PlainText

                Layout.preferredWidth: Math.min(pictureWidth, implicitWidth)
                Layout.alignment: root.textAlignHCenter ? Qt.AlignCenter : Qt.AlignLeft
                Layout.topMargin: VLCStyle.margin_xsmall

                // this is based on that MenuCaption.color.a == .6, color of this component is animated (via binding with background.foregroundColor),
                // to save operation during animation, directly set the opacity
                opacity: .6

                ToolTip.delay: VLCStyle.delayToolTipAppear
                ToolTip.text: subtitleTxt.text
                ToolTip.visible: subtitleTxtMouseArea.containsMouse

                MouseArea {
                    id: subtitleTxtMouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }
        }
    }
}
