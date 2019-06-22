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
    property bool isVideo: false
    property bool isNew: false
    property double progress: 0.5

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
                    property bool highlighted: selected || root.activeFocus

                    Rectangle {
                        id: cover_bg
                        width: isVideo? VLCStyle.video_normal_width : VLCStyle.cover_small
                        height: VLCStyle.cover_small
                        Behavior on width  { SmoothedAnimation { velocity: 100 } }
                        Behavior on height { SmoothedAnimation { velocity: 100 } }
                        anchors.centerIn: parent
                        color: VLCStyle.colors.banner

                        Image {
                            id: cover
                            anchors.fill: parent
                            source: image
                            fillMode: Image.PreserveAspectCrop
                            sourceSize: Qt.size(width, height)

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
                        states: [
                            State {
                                name: "visible"
                                PropertyChanges { target: overlay; visible: true }
                                when: mouseArea.containsMouse
                            },
                            State {
                                name: "hidden"
                                PropertyChanges { target: overlay; visible: false }
                                when: !mouseArea.containsMouse
                            }
                        ]
                        transitions: [
                            Transition {
                                from: "hidden";  to: "visible"
                                NumberAnimation  {
                                    target: overlay
                                    properties: "opacity"
                                    from: 0; to: 0.8; duration: 300
                                }
                             }
                            ]
                        }

                        Rectangle {
                            visible: picture.highlighted
                            anchors.fill: parent
                            color: "transparent"
                            border.width: VLCStyle.selectedBorder
                            border.color: VLCStyle.colors.accent
                        }
                    }
                }
                Text {
                    id: textTitle
                    width: cover_bg.width
                    anchors.horizontalCenter: parent.horizontalCenter

                    text: root.title

                    elide: Text.ElideRight
                    font.pixelSize: VLCStyle.fontSize_normal
                    color: VLCStyle.colors.text
                    horizontalAlignment: Qt.AlignHCenter
                }
                Text {
                    width: cover_bg.width
                    anchors.horizontalCenter: parent.horizontalCenter

                    text : root.subtitle

                    elide: Text.ElideRight
                    font.pixelSize: VLCStyle.fontSize_small
                    color: VLCStyle.colors.lightText
                    horizontalAlignment: Qt.AlignHCenter
                }
                RowLayout {
                    visible: isVideo
                    anchors {
                        bottom:parent.bottom
                        left: parent.left
                        right: parent.right
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
                    }
                }
            }
        }
    }
}

