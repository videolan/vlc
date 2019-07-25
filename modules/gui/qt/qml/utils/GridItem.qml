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


import "qrc:///utils/" as Utils
import "qrc:///style/"

Rectangle {
    id: root

    property url image
    property string title: ""
    property string subtitle: ""
    property bool selected: false
    property int shiftX: 0
    property bool noActionButtons: false

    property alias sourceSize: cover.sourceSize
    property string infoLeft: ""
    property string resolution: ""
    property bool isVideo: false
    property bool isNew: false
    property double progress: 0.5
    property string channel: ""

    signal playClicked
    signal addToPlaylistClicked
    signal itemClicked(int key, int modifier)
    signal itemDoubleClicked(int keys, int modifier)
    signal contextMenuButtonClicked(Item menuParent)

    
    Rectangle {
        id: gridItem
        x: shiftX
        width: childrenRect.width
        height: childrenRect.height
        color: "transparent"

        MouseArea {
            id: mouseArea
            hoverEnabled: true
            onClicked: root.itemClicked(mouse.button, mouse.modifiers)
            onDoubleClicked: root.itemDoubleClicked(mouse.buttons, mouse.modifiers);
            width: childrenRect.width
            height: childrenRect.height
            acceptedButtons: Qt.RightButton | Qt.LeftButton
            Keys.onMenuPressed: root.contextMenuButtonClicked(cover_bg)

                Item {
                    id: picture
                width: isVideo ? VLCStyle.video_normal_width : VLCStyle.cover_small
                height: isVideo ? VLCStyle.video_normal_height : VLCStyle.cover_small
                anchors.top:mouseArea.top
                    property bool highlighted: selected || root.activeFocus

                Rectangle {
                    id: cover_bg
                    width: picture.width
                    height: picture.height
                    color: (cover.status !== Image.Ready ) ? VLCStyle.colors.banner : "transparent"

                    RectangularGlow {
                        visible: picture.highlighted || mouseArea.containsMouse
                        anchors.fill: cover
                        spread: 0.1
                        glowRadius: VLCStyle.margin_xxsmall
                        color: VLCStyle.colors.getBgColor( selected, mouseArea.containsMouse, root.activeFocus )
                    }

                        Image {
                            id: cover
                            anchors.fill: parent
                        anchors.margins: VLCStyle.margin_normal
                            source: image
                            fillMode: Image.PreserveAspectCrop
                            sourceSize: Qt.size(width, height)
                            layer.enabled: true
                            layer.effect: OpacityMask{
                                maskSource: Rectangle{
                                radius: 3
                                    width: cover.width
                                    height: cover.height
                                    visible: false
                                }
                            }
                        Behavior on anchors.margins  { SmoothedAnimation { velocity: 100 } }
                            Rectangle {
                                id: overlay
                                anchors.fill: parent
                                color: "black" //darken the image below

                                RowLayout {
                                    anchors.fill: parent
                                    visible: !noActionButtons
                                    Item {
                                    id: plusItem
                                        Layout.fillHeight: true
                                        Layout.fillWidth: true
                                        /* A addToPlaylist button visible when hovered */
                                        Text {
                                        id: plusIcon
                                            property int iconSize: VLCStyle.icon_large
                                            Behavior on iconSize  { SmoothedAnimation { velocity: 100 } }
                                            Binding on iconSize {
                                                value: VLCStyle.icon_large * 1.2
                                                when: mouseAreaAdd.containsMouse
                                            }

                                            //Layout.alignment: Qt.AlignCenter
                                            anchors.centerIn: parent
                                            text: VLCIcons.add
                                            font.family: VLCIcons.fontFamily
                                            horizontalAlignment: Text.AlignHCenter
                                            color: mouseAreaAdd.containsMouse ? "white" : "lightgray"
                                            font.pixelSize: iconSize

                                            MouseArea {
                                                id: mouseAreaAdd
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                propagateComposedEvents: true
                                                onClicked: root.addToPlaylistClicked()
                                            }
                                        }
                                    Text {
                                        anchors{
                                            top: plusIcon.bottom
                                        }
                                        anchors.horizontalCenter:plusItem.horizontalCenter
                                        font.pixelSize: root.isVideo ? VLCStyle.fontSize_normal : VLCStyle.fontSize_small
                                        text: qsTr("Enqueue")
                                        color: "white"
                                    }

                                    /* A play button visible when hovered */
                                    Item {
                                    id: playItem
                                        Layout.fillHeight: true
                                        Layout.fillWidth: true

                                        Text {
                                        id: playIcon
                                            property int iconSize: VLCStyle.icon_large
                                            Behavior on iconSize  {
                                                SmoothedAnimation { velocity: 100 }
                                            }
                                            Binding on iconSize {
                                                value: VLCStyle.icon_large * 1.2
                                                when: mouseAreaPlay.containsMouse
                                            }

                                            anchors.centerIn: parent
                                            text: VLCIcons.play
                                            font.family: VLCIcons.fontFamily
                                            horizontalAlignment: Text.AlignHCenter
                                            color: mouseAreaPlay.containsMouse ? "white" : "lightgray"
                                            font.pixelSize: iconSize

                                            MouseArea {
                                                id: mouseAreaPlay
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                onClicked: root.playClicked()
                                            }
                                        }
                                    Text {
                                        anchors{
                                            top: playIcon.bottom
                                        }
                                        anchors.horizontalCenter:playItem.horizontalCenter
                                        font.pixelSize: root.isVideo ? VLCStyle.fontSize_normal : VLCStyle.fontSize_small
                                        text: qsTr("Play")
                                        color: "white"
                                    }
                                }
                            }
                        ProgressBar {
                            id: progressBar
                            value: root.progress
                            visible: isVideo
                            anchors{
                                bottom: parent.bottom
                                left: parent.left
                                right: parent.right
                            }
                            background: Rectangle {
                                implicitHeight: 2 * VLCStyle.scale
                                color: VLCStyle.colors.text
                            }
                            contentItem: Item {
                                Rectangle {
                                    width: progressBar.visualPosition * parent.width
                                    height: parent.height
                                    color: VLCStyle.colors.accent
                                }
                            }
                        }

                        }
                        Button {
                            id: contextButton
                            visible: isVideo
                            anchors {
                                top:cover.top
                                right:cover.right
                            }
                            width: VLCStyle.icon_normal
                            height: VLCStyle.icon_normal
                            text: "\u22ef" //ellipsis icon
                            font.pointSize: VLCStyle.fontMetrics_normal

                            hoverEnabled: true
                            onClicked: root.contextMenuButtonClicked(cover_bg)
                            background: Rectangle {
                                id: contextButtonRect
                                anchors.fill: contextButton
                                color: "transparent"
                            }
                            contentItem: Text {
                                id: btnTxt
                                text: contextButton.text
                                font: contextButton.font
                            color: "white"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                //                                layer.enabled: true
                                //                                layer.effect: DropShadow {
                                //                                    color: VLCStyle.colors.text
                                //                                }

                            }
                        }

                    Label {
                        id: resolutionLabel
                        visible: root.isVideo
                        anchors {
                            top:cover.top
                            left:cover.left
                            topMargin: VLCStyle.margin_xxsmall
                            leftMargin: VLCStyle.margin_xxsmall
                        }
                        text: root.resolution
                        color: "white"
                        bottomPadding: VLCStyle.margin_xxxsmall
                        topPadding: VLCStyle.margin_xxxsmall
                        leftPadding: VLCStyle.margin_xxxsmall
                        rightPadding: VLCStyle.margin_xxxsmall
                        font.pixelSize: VLCStyle.fontSize_normal
                        background: Rectangle {
                            id: resolutionLabelRect
                            anchors.fill: resolutionLabel
                            color: "black"
                            opacity: 0.5
                            radius: 3
                        }

                    }
                    Label {
                        id: audioChannelLabel
                        anchors {
                            top:cover.top
                            left:resolutionLabel.right
                            topMargin: VLCStyle.margin_xxsmall
                            leftMargin: VLCStyle.margin_xxxsmall
                        }
                        visible: channel.length > 0
                        text: root.channel
                        color: "limegreen"
                        bottomPadding: VLCStyle.margin_xxxsmall
                        topPadding: VLCStyle.margin_xxxsmall
                        leftPadding: VLCStyle.margin_xxxsmall
                        rightPadding: VLCStyle.margin_xxxsmall
                        font.pixelSize: VLCStyle.fontSize_normal
                        background: Rectangle {
                            id: audioChannelLabelRect
                            anchors.fill: audioChannelLabel
                            color: "black"
                            opacity: 0.5
                            radius: 3
                            }
                        }
                        states: [
                            State {
                            name: "visiblebig"
                            PropertyChanges { target: overlay; visible: true; }
                            PropertyChanges { target: cover; anchors.margins: VLCStyle.margin_xxsmall; }
                                when: mouseArea.containsMouse
                            },
                            State {
                            name: "hiddenbig"
                            PropertyChanges { target: overlay; visible: false;}
                            PropertyChanges { target: cover; anchors.margins: VLCStyle.margin_xxsmall; }
                            when: !mouseArea.containsMouse && picture.highlighted
                        },
                        State {
                            name: "hiddensmall"
                            PropertyChanges { target: overlay; visible: false;}
                            PropertyChanges { target: cover; anchors.margins: VLCStyle.margin_xsmall; }
                            when: !mouseArea.containsMouse && !picture.highlighted
                            }
                        ]
                        transitions: [
                        Transition {from: "hiddenbig";  to: "visiblebig";NumberAnimation  {target: overlay;properties: "opacity";from: 0; to: 0.8; duration: 300}},
                        Transition {from: "hiddensmall";  to: "visiblebig";NumberAnimation  {target: overlay;properties: "opacity";from: 0; to: 0.8; duration: 300}}
                        ]

                    }
                }

                Rectangle {
                    id: textHolderRect
                width: picture.width
                height: childrenRect.height
                anchors.top: picture.bottom
                color: "transparent"
                Rectangle {
                    id: textTitleRect
                    height: childrenRect.height
                    color: "transparent"
                    clip: true
                    property bool showTooltip: false
                    anchors {
                        left: parent.left
                        right: parent.right
                        rightMargin: VLCStyle.margin_small
                        leftMargin: VLCStyle.margin_small
                    }

                    ToolTip {
                        visible: textTitleRect.showTooltip
                        x: (parent.width/2) - (width/2)
                        y: (parent.height/2) - (height/2)
                        text: root.title
                    }

                    Text{
                        id:textTitle
                        text:root.title
                        color: VLCStyle.colors.text
                        font.pixelSize: VLCStyle.fontSize_normal
                        property bool _needsToScroll: (textTitleRect.width < textTitle.width)

                        state: ((mouseArea.containsMouse || contextButton.activeFocus || picture.highlighted) && textTitle._needsToScroll ) ? "HOVERED": "RELEASED"

                            states: [
                                State {
                                    name: "HOVERED"
                                    PropertyChanges {
                                        target: textTitle
                                        x: textTitleRect.width - textTitle.width - VLCStyle.margin_small
                                    }
                                },
                                State {
                                    name: "RELEASED"
                                    PropertyChanges {
                                        target: textTitle
                                        x: 0
                                    }

                                }
                            ]
                            transitions: [
                                Transition {
                                    from: "RELEASED"
                                    to: "HOVERED"

                                    SequentialAnimation {
                                    PauseAnimation { duration: 1000 }
                                        SmoothedAnimation{
                                            property: "x"
                                            maximumEasingTime: 0
                                            velocity: 25
                                        }
                                    PauseAnimation { duration: 2000 }
                                        ScriptAction { script: textTitle.state = "RELEASED"; }
                                    }
                                }
                            ]

                        }
                    }

                    MouseArea {
                        id: titleMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: textTitleRect.showTooltip = true
                        onExited: textTitleRect.showTooltip = false
                    }

                Text {
                    id: subtitleTxt
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: textTitleRect.bottom
                        rightMargin: VLCStyle.margin_small
                        leftMargin: VLCStyle.margin_small
                    }
                    text: root.subtitle
                    font.weight:Font.Light
                    elide: Text.ElideRight
                    font.pixelSize: VLCStyle.fontSize_small
                    color: VLCStyle.colors.lightText
                    horizontalAlignment: Qt.AlignHCenter
                }
                RowLayout {
                    visible: isVideo
                    anchors {
                        top:subtitleTxt.top
                        left: parent.left
                        right: parent.right
                        rightMargin: VLCStyle.margin_small
                        leftMargin: VLCStyle.margin_small
                        topMargin: VLCStyle.margin_xxxsmall
                    }
                    Text {
                        Layout.alignment: Qt.AlignLeft
                        font.pixelSize: VLCStyle.fontSize_small
                        color: VLCStyle.colors.videosGridInfoLeft
                        text: infoLeft
                    }
                    Text {
                        visible: root.isNew
                        Layout.alignment: Qt.AlignRight
                        font.pixelSize: VLCStyle.fontSize_small
                        color: VLCStyle.colors.accent
                        text: "NEW"
                        font.bold: true
                    }
                }


            }
        }
    }
}
}
}
