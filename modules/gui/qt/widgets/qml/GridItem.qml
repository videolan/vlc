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

FocusScope {
    id: root

    property alias image: picture.source
    property alias title: titleLabel.text
    property alias subtitle: subtitleTxt.text
    property alias textHorizontalAlignment: subtitleTxt.horizontalAlignment
    property alias playCoverBorder: picture.playCoverBorder
    property alias playCoverOnlyBorders: picture.playCoverOnlyBorders
    property alias playIconSize: picture.playIconSize
    property alias pictureRadius: picture.radius
    property alias pictureOverlay: picture.imageOverlay
    property bool selected: false

    property alias progress: picture.progress
    property alias labels: picture.labels
    property bool showNewIndicator: false
    property real pictureWidth: VLCStyle.colWidth(1)
    property real pictureHeight: pictureWidth
    property int titleMargin: VLCStyle.margin_xsmall
    property Item dragItem

    signal playClicked
    signal addToPlaylistClicked
    signal itemClicked(Item menuParent, int key, int modifier)
    signal itemDoubleClicked(Item menuParent, int keys, int modifier)
    signal contextMenuButtonClicked(Item menuParent, var globalMousePos)

    Keys.onMenuPressed: root.contextMenuButtonClicked(picture, root.mapToGlobal(0,0))

    Accessible.role: Accessible.Cell
    Accessible.name: title

    implicitWidth: mouseArea.implicitWidth
    implicitHeight: mouseArea.implicitHeight

    readonly property bool _highlighted: mouseArea.containsMouse || content.activeFocus

    readonly property int selectedBorderWidth: VLCStyle.column_margin_width - ( VLCStyle.margin_small * 2 )

    property int _newIndicatorMedian: VLCStyle.margin_xsmall
    property int _modifiersOnLastPress: Qt.NoModifier

    state: _highlighted ? "selected" : "unselected"
    states: [
        State {
            name: "unselected"

            PropertyChanges {
                target: selectedShadow
                opacity: 0
                visible: false
            }

            PropertyChanges {
                target: unselectedShadow
                opacity: 1
                visible: true
            }

            PropertyChanges {
                target: picture
                playCoverOpacity: 0
                playCoverVisible: false
            }

            PropertyChanges {
                target: root
                _newIndicatorMedian: VLCStyle.margin_xsmall
            }
        },
        State {
            name: "selected"

            PropertyChanges {
                target: selectedShadow
                opacity: 1
                visible: true
            }

            PropertyChanges {
                target: unselectedShadow
                opacity: 0
                visible: false
            }

            PropertyChanges {
                target: picture
                playCoverOpacity: 1
                playCoverVisible: true
            }

            PropertyChanges {
                target: root
                _newIndicatorMedian: VLCStyle.margin_small
            }
        }
    ]

    transitions: [
        Transition {
            from: "unselected"
            to: "selected"
            // reversible: true // doesn't work

            SequentialAnimation {
                PropertyAction {
                    targets: [picture, selectedShadow]
                    properties: "playCoverVisible,visible"
                }

                ParallelAnimation {
                    NumberAnimation {
                        properties: "opacity,playCoverOpacity"
                        duration: 240
                        easing.type: Easing.InSine
                    }

                    SmoothedAnimation {
                        target: root
                        property: "_newIndicatorMedian"
                        duration: 240
                        easing.type: Easing.InSine
                    }
                }

                PropertyAction {
                    target: unselectedShadow
                    property: "visible"
                }
            }
        },

        Transition {
            from: "selected"
            to: "unselected"

            SequentialAnimation {
                PropertyAction {
                    target: unselectedShadow
                    property: "visible"
                }

                ParallelAnimation {
                    NumberAnimation {
                        properties: "opacity,playCoverOpacity"
                        duration: 200
                        easing.type: Easing.OutSine
                    }

                    SmoothedAnimation {
                        target: root
                        duration: 200
                        property: "_newIndicatorMedian"
                        easing.type: Easing.OutSine
                    }
                }

                PropertyAction {
                    targets: [picture, selectedShadow]
                    properties: "playCoverVisible,visible"
                }
            }
        }
    ]

    MouseArea {
        id: mouseArea
        hoverEnabled: true

        anchors.fill: parent
        implicitWidth: content.implicitWidth
        implicitHeight: content.implicitHeight
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

        acceptedButtons: Qt.RightButton | Qt.LeftButton
        Keys.onMenuPressed: root.contextMenuButtonClicked(picture, root.mapToGlobal(0,0))

        onClicked: {
            if (mouse.button === Qt.RightButton)
                contextMenuButtonClicked(picture, mouseArea.mapToGlobal(mouse.x,mouse.y));
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
                var pos = drag.target.parent.mapFromItem(mouseArea, mouseX, mouseY)
                drag.target.x = pos.x + 12
                drag.target.y = pos.y + 12
            }
        }

        FocusScope {
            id: content

            anchors.fill: parent
            implicitWidth: layout.implicitWidth
            implicitHeight: layout.implicitHeight
            focus: true

            /* background visible when selected */
            Rectangle {
                id: selectionRect

                x: - root.selectedBorderWidth
                y: - root.selectedBorderWidth
                width: root.width + ( root.selectedBorderWidth * 2 )
                height:  root.height + ( root.selectedBorderWidth * 2 )
                color: VLCStyle.colors.bgHover
                visible: root.selected || root._highlighted
            }

            Rectangle {
                id: baseRect

                x: layout.x + 1 // this rect is set such that it hides behind picture component
                y: layout.y + 1
                width: pictureWidth - 2
                height: pictureHeight - 2
                radius: picture.radius
                color: VLCStyle.colors.bg
            }

            // animating shadows properties are expensive and not smooth
            // thus we use two different shadows for states "selected" and "unselected"
            // and animate their opacity on state changes to get better animation
            CoverShadow {
                id: unselectedShadow

                anchors.fill: baseRect
                source: baseRect
                cached: true
                secondaryVerticalOffset: VLCStyle.dp(1, VLCStyle.scale)
                secondaryRadius: VLCStyle.dp(2, VLCStyle.scale)
                secondarySamples: 1 + VLCStyle.dp(2, VLCStyle.scale) * 2
                primaryVerticalOffset: VLCStyle.dp(4, VLCStyle.scale)
                primaryRadius: VLCStyle.dp(9, VLCStyle.scale)
                primarySamples: 1 + VLCStyle.dp(9, VLCStyle.scale) * 2
            }

            CoverShadow {
                id: selectedShadow

                anchors.fill: baseRect
                source: baseRect
                cached: true
                secondaryVerticalOffset: VLCStyle.dp(6, VLCStyle.scale)
                secondaryRadius: VLCStyle.dp(18, VLCStyle.scale)
                secondarySamples: 1 + VLCStyle.dp(18, VLCStyle.scale) * 2
                primaryVerticalOffset: VLCStyle.dp(32, VLCStyle.scale)
                primaryRadius: VLCStyle.dp(72, VLCStyle.scale)
                primarySamples: 1 + VLCStyle.dp(72, VLCStyle.scale) * 2
            }

            Column {
                id: layout

                anchors.centerIn: parent

                Widgets.MediaCover {
                    id: picture

                    width: pictureWidth
                    height: pictureHeight
                    playCoverVisible: root._highlighted
                    onPlayIconClicked: root.playClicked()
                    clip: true

                    /* new indicator (triangle at top-left of cover)*/
                    Rectangle {
                        id: newIndicator

                        // consider this Rectangle as a triangle, then following property is its median length
                        property alias median: root._newIndicatorMedian

                        x: parent.width - median
                        y: - median
                        width: 2 * median
                        height: 2 * median
                        color: VLCStyle.colors.accent
                        rotation: 45
                        visible: root.showNewIndicator && root.progress === 0
                    }
                }

                Widgets.ScrollingText {
                    id: titleTextRect

                    label: titleLabel
                    scroll: _highlighted
                    height: titleLabel.height
                    width: titleLabel.width
                    visible: root.title !== ""

                    Widgets.MenuLabel {
                        id: titleLabel

                        elide: Text.ElideNone
                        width: pictureWidth
                        horizontalAlignment: root.textHorizontalAlignment
                        topPadding: root.titleMargin
                        color: selectionRect.visible ? VLCStyle.colors.bgHoverText : VLCStyle.colors.text
                    }
                }

                Widgets.MenuCaption {
                    id: subtitleTxt

                    visible: text !== ""
                    text: root.subtitle
                    width: pictureWidth
                    color: selectionRect.visible
                           ? VLCStyle.colors.setColorAlpha(VLCStyle.colors.bgHoverText, .6)
                           : VLCStyle.colors.menuCaption
                }
            }
        }
    }
}
