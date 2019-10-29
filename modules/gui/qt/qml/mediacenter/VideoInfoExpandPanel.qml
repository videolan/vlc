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
import QtQml.Models 2.11
import QtQuick.Layouts 1.3

import org.videolan.medialib 0.1

import "qrc:///utils/" as Utils
import "qrc:///style/"

Utils.NavigableFocusScope {
    id: expandRect

    property int currentId: -1
    property var model : ({})
    property alias currentItemY: expandRect.y
    property alias currentItemHeight: expandRect.height
    implicitHeight: arrowRect.implicitHeight + contentRect.implicitHeight
    property int notchPosition: 0
    signal retract()

    //arrow
    Item {
        id:arrowRect
        y: -(width/2)
        x: notchPosition  - (width/2)
        clip: true
        width: Math.sqrt(2) *VLCStyle.icon_normal
        height: width/2
        implicitHeight: width/2

        Rectangle{
            x: 0
            y: parent.height
            width: VLCStyle.icon_normal
            height: VLCStyle.icon_normal
            color: VLCStyle.colors.bgAlt
            transformOrigin: Item.TopLeft
            rotation: -45
        }
    }


    Rectangle{
        id: contentRect
        height: implicitHeight
        implicitHeight: contentLayout.implicitHeight + VLCStyle.margin_xsmall * 2
        width: parent.width
        clip: true
        color: VLCStyle.colors.bgAlt

        RowLayout {
            id: contentLayout
            anchors {
                fill: parent
                topMargin: VLCStyle.margin_xsmall
                bottomMargin: VLCStyle.margin_xsmall
                leftMargin: VLCStyle.margin_normal
            }

            spacing: VLCStyle.margin_small

            Image {
                id: img
                Layout.alignment: Qt.AlignCenter
                Layout.preferredWidth: width
                Layout.preferredHeight: height

                width: VLCStyle.cover_xlarge
                height: VLCStyle.cover_xlarge
                fillMode:Image.PreserveAspectFit

                source: model.thumbnail || VLCStyle.noArtCover
            }

            Column{
                id: infoCol
                height: childrenRect.height
                width: Math.min(title.implicitWidth, 200*VLCStyle.scale)

                Layout.alignment: Qt.AlignCenter
                Layout.preferredWidth: width
                Layout.preferredHeight: height

                spacing: VLCStyle.margin_small
                Text{
                    id: newtxt
                    font.pixelSize: VLCStyle.fontSize_normal
                    font.weight: Font.ExtraBold
                    text: "NEW"
                    color: VLCStyle.colors.accent
                    visible: model.playcount < 1
                }
                Text{
                    id: title
                    wrapMode: Text.Wrap
                    font.pixelSize: VLCStyle.fontSize_xlarge
                    font.weight: Font.ExtraBold
                    text: model.title || ""
                    color: VLCStyle.colors.text
                    width: parent.width
                }
                Text {
                    id: time
                    text: model.duration || ""
                    color: VLCStyle.colors.textInactive
                    font.pixelSize: VLCStyle.fontSize_small
                }
            }


            Utils.NavigableFocusScope {
                id: infoPanel

                Layout.fillHeight: true
                Layout.fillWidth: true

                ScrollView {
                    id: infoPannelScrollView
                    contentHeight: infoInnerCol.height

                    anchors.fill: parent

                    focus: true
                    clip: true

                    ListView {
                        id: infoInnerCol
                        spacing: VLCStyle.margin_xsmall
                        model: [
                            {text: qsTr("File Name"),    data: expandRect.model.title, bold: true},
                            {text: qsTr("Path"),         data: expandRect.model.mrl},
                            {text: qsTr("Length"),       data: expandRect.model.duration},
                            {text: qsTr("File size"),    data: ""},
                            {text: qsTr("Times played"), data: expandRect.model.playcount},
                            {text: qsTr("Video track"),  data: expandRect.model.videoDesc},
                            {text: qsTr("Audio track"),  data: expandRect.model.audioDesc},
                        ]
                        delegate: Label {
                            font.bold: Boolean(modelData.bold)
                            font.pixelSize: VLCStyle.fontSize_normal
                            text: modelData.text + ": " + modelData.data
                            color: VLCStyle.colors.text
                            width: parent.width
                            wrapMode: Label.Wrap
                        }
                    }
                    Keys.priority: Keys.BeforeItem
                    Keys.onPressed: {
                        if (event.key !== Qt.Key_Up && event.key !== Qt.Key_Down) {
                            infoPanel.defaultKeyAction(event, 0)
                        }
                    }
                }

                Rectangle {
                    z: 2
                    anchors.fill: parent
                    border.width: 1
                    border.color: VLCStyle.colors.accent
                    color: "transparent"
                    visible: infoPanel.activeFocus
                }

                Rectangle {
                    anchors { top: parent.top; left: parent.left;  right: parent.right }
                    z: 1
                    visible: !infoPannelScrollView.contentItem.atYBeginning
                    height: parent.height * 0.2
                    gradient: Gradient{
                        GradientStop { position: 0.0; color: VLCStyle.colors.bgAlt }
                        GradientStop { position: 1.0; color: "transparent" }
                    }
                }

                Rectangle {
                    anchors { bottom: parent.bottom; left: parent.left;  right: parent.right }
                    z: 1
                    visible: !infoPannelScrollView.contentItem.atYEnd
                    height: parent.height * 0.2
                    gradient: Gradient{
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 1.0; color: VLCStyle.colors.bgAlt }
                    }
                }

                navigationParent: expandRect
                navigationRightItem: actionButtons
            }

            Rectangle {
                width: 1
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.25; color: VLCStyle.colors.buttonBorder }
                    GradientStop { position: 0.75; color: VLCStyle.colors.buttonBorder }
                    GradientStop { position: 1.0; color: "transparent" }
                }

            }

            Utils.NavigableCol {
                id: actionButtons

                focus: true

                Layout.alignment: Qt.AlignCenter
                Layout.preferredWidth: childrenRect.width

                model: ObjectModel {
                    Utils.TabButtonExt {
                        id: playActionBtn
                        iconTxt: VLCIcons.play
                        text: qsTr("Play")
                        onClicked: medialib.addAndPlay( expandRect.model.id )
                    }

                    Utils.TabButtonExt {
                        id: enqueueActionBtn
                        iconTxt: VLCIcons.add
                        text: qsTr("Enqueue")
                        onClicked: medialib.addToPlaylist( expandRect.model.id )
                    }
                }

                navigationParent: expandRect
                navigationLeftItem: infoPanel
                navigationRightItem: closeButton
            }


            Utils.NavigableFocusScope {
                id: closeButton

                Layout.alignment: Qt.AlignTop | Qt.AlignVCenter
                Layout.preferredWidth: closeButtonId.size
                Layout.preferredHeight: closeButtonId.size

                Utils.IconToolButton {
                    id: closeButtonId

                    size: VLCStyle.icon_normal
                    text: VLCIcons.close
                    color: VLCStyle.colors.lightText

                    focus: true
                    onClicked: expandRect.retract()
                }

                Keys.priority: Keys.AfterItem
                Keys.onPressed: defaultKeyAction(event, 0)
                navigationParent: expandRect
                navigationLeftItem: actionButtons
            }
        }
    }
}
