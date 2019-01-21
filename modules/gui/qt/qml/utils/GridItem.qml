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

Item {
    id: root
    width: VLCStyle.cover_normal
    height: VLCStyle.cover_normal
            + VLCStyle.fontHeight_normal
            + VLCStyle.fontHeight_small
            + VLCStyle.margin_xsmall

    property url image: VLCStyle.noArtCover
    property string title: ""
    property string subtitle: ""
    property bool selected: false
    property int shiftX: 0
    property bool noActionButtons: false

    signal playClicked
    signal addToPlaylistClicked
    signal itemClicked(int keys, int modifier)
    signal itemDoubleClicked(int keys, int modifier)

    Item {
        x: shiftX
        width: parent.width
        height: parent.height

        MouseArea {

            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked:  root.itemClicked(mouse.buttons, mouse.modifiers)
            onDoubleClicked: root.itemDoubleClicked(mouse.buttons, mouse.modifiers);

            Item {
                anchors.fill: parent
                Item {
                    id: picture

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top

                    width: VLCStyle.cover_normal
                    height: VLCStyle.cover_normal
                    property bool highlighted: selected || mouseArea.containsMouse

                    RectangularGlow {
                        visible: picture.highlighted
                        anchors.fill: cover
                        cornerRadius: 25
                        spread: 0.5
                        glowRadius: VLCStyle.margin_xsmall
                        color: VLCStyle.colors.getBgColor( selected, mouseArea.containsMouse, root.activeFocus )
                    }

                    /*
                    Item {
                        id: coverPlaceHolder
                        x: cover.x + (cover.width - cover.paintedWidth) / 2
                        y: cover.y +(cover.height - cover.paintedHeight) / 2
                        width: cover.paintedWidth
                        height: cover.paintedHeight
                    }
                    */

                    Image {
                        id: cover
                        width: VLCStyle.cover_small
                        height: VLCStyle.cover_small
                        Behavior on width  { SmoothedAnimation { velocity: 100 } }
                        Behavior on height { SmoothedAnimation { velocity: 100 } }
                        anchors.centerIn: parent
                        source: image
                        fillMode: Image.PreserveAspectCrop

                        Rectangle {
                            id: overlay
                            anchors.fill: parent
                            visible: mouseArea.containsMouse
                            color: "black" //darken the image below

                            RowLayout {
                                anchors.fill: parent
                                visible: !noActionButtons
                                Item {
                                    Layout.fillHeight: true
                                    Layout.fillWidth: true
                                    /* A addToPlaylist button visible when hovered */
                                    Text {
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
                                }

                                /* A play button visible when hovered */
                                Item {
                                    Layout.fillHeight: true
                                    Layout.fillWidth: true

                                    Text {
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
                                }
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

                    states: [
                        State {
                            name: "big"
                            when: picture.highlighted
                            PropertyChanges {
                                target: cover
                                width:  VLCStyle.cover_normal - 2 * VLCStyle.margin_xsmall
                                height: VLCStyle.cover_normal - 2 * VLCStyle.margin_xsmall
                            }
                        },
                        State {
                            name: "small"
                            when: !picture.highlighted
                            PropertyChanges {
                                target: cover
                                width:  VLCStyle.cover_normal - 2 * VLCStyle.margin_small
                                height: VLCStyle.cover_normal - 2 * VLCStyle.margin_small
                            }
                        }
                    ]
                }
                Text {
                    id: textTitle
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: picture.bottom
                        rightMargin: VLCStyle.margin_small
                        leftMargin: VLCStyle.margin_small
                    }

                    text: root.title

                    elide: Text.ElideRight
                    font.pixelSize: VLCStyle.fontSize_normal
                    font.bold: true
                    color: VLCStyle.colors.text
                }
                Text {
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: textTitle.bottom
                        rightMargin: VLCStyle.margin_small
                        leftMargin: VLCStyle.margin_small
                    }

                    text : root.subtitle

                    elide: Text.ElideRight
                    font.pixelSize: VLCStyle.fontSize_small
                    color: VLCStyle.colors.text
                }
            }
        }
    }
}
