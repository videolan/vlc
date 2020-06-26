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
import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Item {
    id: root

    property alias image: picture.source
    property alias title: titleLabel.text
    property alias subtitle: subtitleTxt.text
    property alias playCoverBorder: picture.playCoverBorder
    property alias playCoverOnlyBorders: picture.playCoverOnlyBorders
    property bool selected: false

    property alias progress: picture.progress
    property alias labels: picture.labels
    property bool showNewIndicator: false
    property real pictureWidth: VLCStyle.colWidth(1)
    property real pictureHeight: pictureWidth
    property int titleMargin: VLCStyle.margin_xsmall

    signal playClicked
    signal addToPlaylistClicked
    signal itemClicked(Item menuParent, int key, int modifier)
    signal itemDoubleClicked(Item menuParent, int keys, int modifier)
    signal contextMenuButtonClicked(Item menuParent)

    Keys.onMenuPressed: root.contextMenuButtonClicked(picture)

    Accessible.role: Accessible.Cell
    Accessible.name: title

    implicitWidth: mouseArea.implicitWidth
    implicitHeight: mouseArea.implicitHeight

    readonly property bool _highlighted: mouseArea.containsMouse || content.activeFocus

    readonly property int _selectedBorderWidth: VLCStyle.column_margin_width - ( VLCStyle.margin_small * 2 )

    property alias _primaryShadowVerticalOffset: primaryShadow.verticalOffset
    property alias _primaryShadowRadius: primaryShadow.radius
    property alias _secondaryShadowVerticalOffset: secondaryShadow.verticalOffset
    property alias _secondaryShadowRadius: secondaryShadow.radius

    property int _newIndicatorMedian: VLCStyle.margin_xsmall

    states: [
        State {
            name: "unselected"
            when: !root._highlighted
            PropertyChanges {
                target: root
                _primaryShadowVerticalOffset: VLCStyle.dp(6, VLCStyle.scale)
                _primaryShadowRadius: VLCStyle.dp(14, VLCStyle.scale)
                _secondaryShadowVerticalOffset: VLCStyle.dp(1, VLCStyle.scale)
                _secondaryShadowRadius: VLCStyle.dp(3, VLCStyle.scale)
                _newIndicatorMedian: VLCStyle.margin_xsmall
            }
        },
        State {
            name: "selected"
            when: root._highlighted
            PropertyChanges {
                target: root
                _primaryShadowVerticalOffset: VLCStyle.dp(32, VLCStyle.scale)
                _primaryShadowRadius: VLCStyle.dp(72, VLCStyle.scale)
                _secondaryShadowVerticalOffset: VLCStyle.dp(6, VLCStyle.scale)
                _secondaryShadowRadius: VLCStyle.dp(8, VLCStyle.scale)
                _newIndicatorMedian: VLCStyle.margin_small
            }
        }
    ]

    transitions: Transition {
        to: "*"
        SmoothedAnimation {
          duration: 64
          properties: "_primaryShadowVerticalOffset,_primaryShadowRadius,_secondaryShadowVerticalOffset,_secondaryShadowRadius,_newIndicatorMedian"
       }
    }

    MouseArea {
        id: mouseArea
        hoverEnabled: true

        anchors.fill: parent
        implicitWidth: content.implicitWidth
        implicitHeight: content.implicitHeight

        acceptedButtons: Qt.RightButton | Qt.LeftButton
        Keys.onMenuPressed: root.contextMenuButtonClicked(picture)

        onClicked: {
            if (mouse.button === Qt.RightButton)
                contextMenuButtonClicked(picture);
            else {
                root.itemClicked(picture, mouse.button, mouse.modifiers);
            }
        }

        onDoubleClicked: {
            if (mouse.button === Qt.LeftButton)
                root.itemDoubleClicked(picture,mouse.buttons, mouse.modifiers)
        }

        FocusScope {
            id: content

            anchors.fill: parent
            implicitWidth: layout.implicitWidth
            implicitHeight: layout.implicitHeight
            focus: root.activeFocus

            /* background visible when selected */
            Rectangle {
                x: - root._selectedBorderWidth
                y: - root._selectedBorderWidth
                width: layout.implicitWidth + ( root._selectedBorderWidth * 2 )
                height:  layout.implicitHeight + ( root._selectedBorderWidth * 2 )
                color: VLCStyle.colors.bgAlt
                visible: root.selected || root._highlighted
            }

            Rectangle {
                id: baseRect

                x: 1 // this rect is set such that it hides behind picture component
                y: 1
                width: pictureWidth - 2
                height: pictureHeight - 2
                radius: picture.radius
                color: VLCStyle.colors.bg
            }

            DropShadow {
                id: primaryShadow

                anchors.fill: baseRect
                source: baseRect
                horizontalOffset: 0
                verticalOffset: VLCStyle.dp(6, VLCStyle.scale)
                radius: VLCStyle.dp(14, scale)
                spread: 0
                samples: ( radius * 2 ) + 1
                color: Qt.rgba(0, 0, 0, .22)
            }

            DropShadow {
                id: secondaryShadow

                anchors.fill: baseRect
                source: baseRect
                horizontalOffset: 0
                verticalOffset: VLCStyle.dp(1, VLCStyle.scale)
                radius: VLCStyle.dp(3, VLCStyle.scale)
                spread: 0
                samples: ( radius * 2 ) + 1
                color: Qt.rgba(0, 0, 0, .18)
            }

            ColumnLayout {
                id: layout

                anchors.fill: parent
                spacing: 0

                Widgets.MediaCover {
                    id: picture

                    width: pictureWidth
                    height: pictureHeight
                    playCoverVisible: root._highlighted
                    onPlayIconClicked: root.playClicked()

                    Layout.preferredWidth: pictureWidth
                    Layout.preferredHeight: pictureHeight

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

                    Layout.preferredHeight: titleLabel.contentHeight
                    Layout.topMargin: VLCStyle.margin_xxsmall
                    Layout.fillWidth: true
                    Layout.maximumWidth: pictureWidth

                    Widgets.MenuLabel {
                        id: titleLabel

                        elide: Text.ElideNone
                        width: pictureWidth
                    }
                }

                Widgets.MenuCaption {
                    id: subtitleTxt

                    visible: text !== ""
                    text: root.subtitle

                    Layout.preferredHeight: implicitHeight
                    Layout.maximumWidth: pictureWidth
                    Layout.fillWidth: true
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }
}
