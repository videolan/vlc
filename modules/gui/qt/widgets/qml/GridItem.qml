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

    property url image
    property string title: ""
    property string subtitle: ""
    property bool selected: false

    property double progress: 0
    property var labels: []
    property real pictureWidth: VLCStyle.colWidth(1)
    property real pictureHeight: ( pictureWidth * 10 ) / 16

    //space use for zoom
    readonly property real outterMargin: VLCStyle.margin_xxsmall
    //margin wihtin the tile
    readonly property real innerMargin: VLCStyle.margin_xxsmall

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

    property bool _zoomed: mouseArea.containsMouse || root.activeFocus
    property real _picWidth: pictureWidth + (_zoomed ? 2*outterMargin : 0)
    property real _picHeight: pictureHeight + (_zoomed ? 2*outterMargin : 0)


    MouseArea {
        id: mouseArea
        hoverEnabled: true

        anchors.fill: parent
        implicitWidth: zoomArea.implicitWidth + (_zoomed ? 0 : 2*outterMargin)
        implicitHeight: zoomArea.implicitHeight + (_zoomed ? 0 : 2*outterMargin)

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

        FocusBackground {
            id: zoomArea

            anchors.fill: parent
            anchors.margins: outterMargin

            implicitWidth: layout.implicitWidth + 2*innerMargin
            implicitHeight: layout.implicitHeight + 2*innerMargin

            active: root.activeFocus
            selected: root.selected || mouseArea.containsMouse

            states: [
                State {
                    name: "visiblebig"
                    PropertyChanges {
                        target: zoomArea
                        anchors.margins: 0
                    }
                    when: _zoomed
                },
                State {
                    name: "hiddensmall"
                    PropertyChanges {
                        target: zoomArea
                        anchors.margins: VLCStyle.margin_xxsmall
                    }
                    when: !_zoomed
                }
            ]

            Behavior on anchors.margins {
                SmoothedAnimation {
                    duration: 100
                }
            }

            ColumnLayout {
                id: layout
                anchors.fill: parent
                anchors.margins: innerMargin

                spacing: 0

                Widgets.RoundImage {
                    id: picture
                    width: _picWidth
                    height: _picHeight
                    Layout.preferredWidth: _picWidth
                    Layout.preferredHeight: _picHeight
                    Layout.alignment: Qt.AlignHCenter

                    source: image

                    RowLayout {
                        anchors {
                            top: parent.top
                            left: parent.left
                            right: parent.right
                            topMargin: VLCStyle.margin_xxsmall
                            leftMargin: VLCStyle.margin_xxsmall
                            rightMargin: VLCStyle.margin_xxsmall
                        }

                        spacing: VLCStyle.margin_xxsmall

                        Repeater {
                            model: labels
                            VideoQualityLabel {
                                Layout.preferredWidth: implicitWidth
                                Layout.preferredHeight: implicitHeight
                                text: modelData
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                    }

                    VideoProgressBar {
                        value: root.progress
                        visible: root.progress > 0
                        anchors {
                            bottom: parent.bottom
                            left: parent.left
                            right: parent.right
                        }
                    }
                }

                Widgets.ScrollingText {
                    id: textTitleRect

                    Layout.preferredHeight: childrenRect.height
                    Layout.fillWidth: true
                    Layout.maximumWidth: _picWidth

                    text: root.title
                    color: VLCStyle.colors.text
                    font.pixelSize: VLCStyle.fontSize_normal

                    scroll: _zoomed || selected
                }

                Text {
                    id: subtitleTxt

                    Layout.preferredHeight: implicitHeight
                    Layout.maximumWidth: _picWidth
                    Layout.fillWidth: true

                    visible: text !== ""

                    text: root.subtitle
                    font.weight: Font.Light
                    elide: Text.ElideRight
                    font.pixelSize: VLCStyle.fontSize_small
                    color: VLCStyle.colors.textInactive

                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }
}
