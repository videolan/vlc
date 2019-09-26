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

    color: "transparent"
    property url image
    property string title: ""
    property string subtitle: ""
    property bool selected: false
    property bool showContextButton: isVideo

    property alias sourceSize: cover.sourceSize
    property string infoLeft: ""
    property string resolution: ""
    property bool isVideo: false
    property bool isNew: false
    property double progress: 0
    property string channel: ""
    property real pictureWidth: isVideo ? VLCStyle.video_normal_width : VLCStyle.cover_small
    property real pictureHeight: isVideo ? VLCStyle.video_normal_height : VLCStyle.cover_small
    property alias contextButtonDown: contextButton.down
    width: gridItem.width
    height: gridItem.height

    signal playClicked
    signal addToPlaylistClicked
    signal itemClicked(Item menuParent, int key, int modifier)
    signal itemDoubleClicked(Item menuParent, int keys, int modifier)
    signal contextMenuButtonClicked(Item menuParent)

    property int index: 0

    Keys.onMenuPressed: root.contextMenuButtonClicked(cover_bg)

    Item {
        id: gridItem
        width: childrenRect.width
        height: childrenRect.height
        
        MouseArea {
            id: mouseArea
            hoverEnabled: true
            onClicked: {
                if (mouse.button === Qt.RightButton)
                    contextMenuButtonClicked(cover_bg);
                else {
                    root.itemClicked(cover_bg,mouse.button, mouse.modifiers);
                }
            }
            onDoubleClicked: {
                root.itemDoubleClicked(cover_bg,mouse.buttons, mouse.modifiers)
            }
            width: childrenRect.width
            height: childrenRect.height
            acceptedButtons: Qt.RightButton | Qt.LeftButton
            Keys.onMenuPressed: root.contextMenuButtonClicked(cover_bg)

            Item {
                id: picture
                width: root.pictureWidth
                height: root.pictureHeight
                anchors.top: mouseArea.top
                property bool highlighted: selected || root.activeFocus

                Rectangle {
                    id: cover_bg
                    width: picture.width
                    height: picture.height


                    color: VLCStyle.colors.getBgColor(
                               selected, mouseArea.containsMouse,
                               root.activeFocus)

                    RoundImage {
                        id: cover
                        anchors.fill: parent
                        anchors.margins: VLCStyle.margin_normal
                        source: image

                        Behavior on anchors.margins {
                            SmoothedAnimation {
                                velocity: 100
                            }
                        }

                        VideoProgressBar {
                            value: root.progress
                            visible: isVideo && root.progress > 0
                            anchors {
                                bottom: parent.bottom
                                left: parent.left
                                right: parent.right
                            }
                        }
                    }
                    ContextButton {
                        id: contextButton
                        visible: root.showContextButton
                        anchors {
                            top: cover.top
                            right: cover.right
                        }
                        color: "white"
                        onClicked: root.contextMenuButtonClicked(cover_bg)
                    }

                    VideoQualityLabel {
                        id: resolutionLabel
                        visible: root.isVideo
                        anchors {
                            top: cover.top
                            left: cover.left
                            topMargin: VLCStyle.margin_xxsmall
                            leftMargin: VLCStyle.margin_xxsmall
                        }
                        text: root.resolution
                    }
                    VideoQualityLabel {
                        anchors {
                            top: cover.top
                            left: resolutionLabel.right
                            topMargin: VLCStyle.margin_xxsmall
                            leftMargin: VLCStyle.margin_xxxsmall
                        }
                        visible: channel.length > 0
                        text: root.channel
                        color: "limegreen"
                    }
                    states: [
                        State {
                            name: "visiblebig"
                            PropertyChanges {
                                target: cover
                                anchors.margins: VLCStyle.margin_xxsmall
                            }
                            when: mouseArea.containsMouse
                        },
                        State {
                            name: "hiddenbig"
                            PropertyChanges {
                                target: cover
                                anchors.margins: VLCStyle.margin_xxsmall
                            }
                            when: !mouseArea.containsMouse
                                  && picture.highlighted
                        },
                        State {
                            name: "hiddensmall"
                            PropertyChanges {
                                target: cover
                                anchors.margins: VLCStyle.margin_xsmall
                            }
                            when: !mouseArea.containsMouse
                                  && !picture.highlighted
                        }
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

                    Text {
                        id: textTitle
                        text: root.title
                        color: VLCStyle.colors.text
                        font.pixelSize: VLCStyle.fontSize_normal
                        property bool _needsToScroll: (textTitleRect.width < textTitle.width)

                        state: ((mouseArea.containsMouse
                                 || contextButton.activeFocus || picture.highlighted)
                                && textTitle._needsToScroll) ? "HOVERED" : "RELEASED"

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
                                    PauseAnimation {
                                        duration: 1000
                                    }
                                    SmoothedAnimation {
                                        property: "x"
                                        maximumEasingTime: 0
                                        velocity: 25
                                    }
                                    PauseAnimation {
                                        duration: 2000
                                    }
                                    ScriptAction {
                                        script: textTitle.state = "RELEASED"
                                    }
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
                    font.weight: Font.Light
                    elide: Text.ElideRight
                    font.pixelSize: VLCStyle.fontSize_small
                    color: VLCStyle.colors.lightText
                    horizontalAlignment: Qt.AlignHCenter
                }
                RowLayout {
                    visible: isVideo
                    anchors {
                        top: subtitleTxt.top
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
