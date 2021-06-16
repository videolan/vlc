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
import QtQml.Models 2.2
import QtGraphicalEffects 1.0
import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

MouseArea {
    id: root

    property alias image: picture.source
    property alias title: titleLabel.text
    property alias subtitle: subtitleTxt.text
    property alias playCoverBorderWidth: picture.playCoverBorderWidth
    property alias playCoverOnlyBorders: picture.playCoverOnlyBorders
    property alias playIconSize: picture.playIconSize
    property alias pictureRadius: picture.radius
    property alias pictureOverlay: picture.imageOverlay
    property alias unselectedUnderlay: unselectedUnderlayLoader.sourceComponent
    property alias selectedUnderlay: selectedUnderlayLoader.sourceComponent

    property real pictureWidth: VLCStyle.colWidth(1)
    property real pictureHeight: pictureWidth
    property int titleMargin: VLCStyle.margin_xsmall
    property Item dragItem: null

    readonly property bool highlighted: root.containsMouse || root.activeFocus
    readonly property int selectedBorderWidth: VLCStyle.gridItemSelectedBorder

    property int _modifiersOnLastPress: Qt.NoModifier

    // if true, texts are horizontally centered, provided it can fit in pictureWidth
    property bool textAlignHCenter: false

    // if the item is selected
    property bool selected: false

    signal playClicked
    signal addToPlaylistClicked
    signal itemClicked(Item menuParent, int key, int modifier)
    signal itemDoubleClicked(Item menuParent, int keys, int modifier)
    signal contextMenuButtonClicked(Item menuParent, var globalMousePos)

    acceptedButtons: Qt.RightButton | Qt.LeftButton
    hoverEnabled: true
    implicitWidth: layout.implicitWidth
    implicitHeight: layout.implicitHeight
    Keys.onMenuPressed: root.contextMenuButtonClicked(picture, root.mapToGlobal(0,0))

    Accessible.role: Accessible.Cell
    Accessible.name: title

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
                playCoverOpacity: 1
                playCoverVisible: true
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
                    properties: "playCoverVisible,visible"
                }

                NumberAnimation {
                    properties: "opacity,playCoverOpacity"
                    duration: 240
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
                    property: "visible"
                }

                NumberAnimation {
                    properties: "opacity,playCoverOpacity"
                    duration: 200
                    easing.type: Easing.OutSine
                }

                PropertyAction {
                    targets: [picture, selectedUnderlayLoader]
                    properties: "playCoverVisible,visible"
                }
            }
        }
    ]

    drag.target: root.dragItem
    drag.axis: Drag.XAndYAxis
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
            drag.target.x = pos.x + 12
            drag.target.y = pos.y + 12
        }
    }

    Widgets.AnimatedBackground {
        id: background

        x: - root.selectedBorderWidth
        y: - root.selectedBorderWidth
        width: root.width + ( root.selectedBorderWidth * 2 )
        height:  root.height + ( root.selectedBorderWidth * 2 )

        active: root.activeFocus

        backgroundColor: root.selected
                         ? VLCStyle.colors.bgHover
                         : VLCStyle.colors.setColorAlpha(VLCStyle.colors.bgHover, 0)

        visible: backgroundAnimationRunning || background.active || root.selected
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

    Column {
        id: layout

        anchors.centerIn: parent

        Widgets.MediaCover {
            id: picture

            width: pictureWidth
            height: pictureHeight
            playCoverVisible: false
            playCoverOpacity: 0
            radius: VLCStyle.gridCover_radius

            onPlayIconClicked: root.playClicked()
        }

        Widgets.ScrollingText {
            id: titleTextRect

            label: titleLabel
            scroll: highlighted
            height: titleLabel.height
            width: titleLabel.width
            visible: root.title !== ""

            Widgets.ListLabel {
                id: titleLabel

                elide: titleTextRect.scroll ?  Text.ElideNone : Text.ElideRight
                width: pictureWidth
                horizontalAlignment: root.textAlignHCenter && titleLabel.contentWidth <= titleLabel.width ? Text.AlignHCenter : Text.AlignLeft
                topPadding: root.titleMargin
                color: background.foregroundColor
            }
        }

        Widgets.MenuCaption {
            id: subtitleTxt

            visible: text !== ""
            text: root.subtitle
            width: pictureWidth
            topPadding: VLCStyle.margin_xsmall              
            elide: Text.ElideRight
            horizontalAlignment: root.textAlignHCenter && subtitleTxt.contentWidth <= subtitleTxt.width ? Text.AlignHCenter : Text.AlignLeft
            color: background.foregroundColor

            // this is based on that MenuCaption.color.a == .6, color of this component is animated (via binding with background.foregroundColor),
            // to save operation during animation, directly set the opacity
            opacity: .6
        }
    }
}
