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

import org.videolan.medialib 0.1

import "qrc:///utils/" as Utils
import "qrc:///dialogs/" as DG
import "qrc:///style/"

Utils.ExpandGridView {
    id: expandableGV

    activeFocusOnTab:true

    property real expandDelegateImplicitHeight: parent.height
    property real expandDelegateWidth: parent.width

    expandDelegate:  Rectangle {
        id: expandRect
        property int currentId: -1
        property var model : ({})
        property alias currentItemY: expandRect.y
        property alias currentItemHeight: expandRect.height
        implicitHeight: expandableGV.expandDelegateImplicitHeight
        width: expandableGV.expandDelegateWidth

        color: "transparent"
        Rectangle{
            id:arrowRect
            y: -(width/2)
            width: VLCStyle.icon_normal
            height: VLCStyle.icon_normal
            color: VLCStyle.colors.text
            rotation: 45
            visible: !expandableGV.isAnimating
        }
        Rectangle{
            height: parent.height
            width: parent.width
            clip: true
            color: VLCStyle.colors.text
            x: expandableGV.contentX

            Rectangle {
                color: "transparent"
                height: parent.height
                anchors {
                    left:parent.left
                    right:parent.right
                }


                Image {
                    id: img
                    anchors.left: parent.left
                    anchors.leftMargin: VLCStyle.margin_large
                    anchors.verticalCenter: parent.verticalCenter
                    width: VLCStyle.cover_large
                    height: VLCStyle.cover_large
                    fillMode:Image.PreserveAspectFit

                    source: model.thumbnail || ""
                }
                Column{
                    id: infoCol
                    height: childrenRect.height
                    anchors.left:img.right
                    anchors.leftMargin: VLCStyle.margin_normal
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: VLCStyle.margin_small
                    width: 300 * VLCStyle.scale
                    Text{
                        id: newtxt
                        font.pixelSize: VLCStyle.fontSize_normal
                        font.weight: Font.ExtraBold
                        text: "NEW"
                        color: VLCStyle.colors.accent
                        visible: model.playcount < 1
                    }
                    Column{
                        width: parent.width
                        spacing: VLCStyle.margin_xsmall
                        Text{
                            id: title
                            wrapMode: Text.Wrap
                            font.pixelSize: VLCStyle.fontSize_large
                            font.weight: Font.ExtraBold
                            text: model.title || ""
                            color: VLCStyle.colors.bg
                            width: parent.width
                        }
                        Text {
                            id: time
                            text: model.duration || ""
                            color: VLCStyle.colors.textInactive
                            font.pixelSize: VLCStyle.fontSize_small
                        }
                    }

                    Button {
                        id: playBtn
                        hoverEnabled: true
                        width: VLCStyle.icon_xlarge
                        height: VLCStyle.icon_medium
                        background: Rectangle{
                            color: playBtn.pressed? VLCStyle.colors.textInactive: VLCStyle.colors.accent
                            width: parent.width
                            height: parent.height
                            radius: playBtn.width/3
                        }
                        contentItem:Item{
                            implicitWidth: childrenRect.width
                            implicitHeight: childrenRect.height
                            anchors.centerIn: playBtn

                            Label {
                                anchors.verticalCenter: parent.verticalCenter
                                id: icon
                                text:  playBtn.fontIcon
                                font.family: VLCIcons.fontFamily
                                font.pixelSize: parent.height
                                color: playBtn.pressed || playBtn.hovered?  VLCStyle.colors.bg : VLCStyle.colors.bgAlt
                            }


                            Label {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: icon.right
                                text: playBtn.text
                                font: playBtn.font
                                color: playBtn.pressed || playBtn.hovered? VLCStyle.colors.bg : VLCStyle.colors.bgAlt

                            }
                        }


                        property string fontIcon: VLCIcons.play

                        text: qsTr("Play Video")
                        onClicked: medialib.addAndPlay( model.id )
                    }
                }
                Flickable{
                    anchors{
                        left: infoCol.right
                        right: controlCol.left
                        top: parent.top
                        bottom: parent.bottom
                        topMargin: VLCStyle.margin_small
                        bottomMargin: VLCStyle.margin_small
                    }
                    width: parent.width
                    contentHeight: infoInnerCol.height
                    Column{
                        id: infoInnerCol
                        height: childrenRect.height
                        anchors{
                            left: parent.left
                            right: parent.right
                        }
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: VLCStyle.margin_xsmall
                        Repeater {
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
                                text: modelData.text + ": " + modelData.data
                                color: VLCStyle.colors.textInactive
                                width: parent.width
                                wrapMode: Label.Wrap
                            }
                        }
                    }
                }

                Rectangle{
                    id: controlCol
                    anchors.right: parent.right
                    width: 300 * VLCStyle.scale
                    height: parent.height
                    color: VLCStyle.colors.text

                    Column{
                        anchors {
                            left: parent.left
                            right: parent.right
                            verticalCenter: parent.verticalCenter
                        }

                        spacing: VLCStyle.margin_normal
                        Repeater {
                            id:reptr
                            anchors.fill: parent
                            model: [
                                {label: qsTr("Rename Video"), ic: VLCIcons.rename},
                                {label: qsTr("Enqueue"), ic: VLCIcons.add},
                                {label: qsTr("Share"), ic: VLCIcons.lan},
                                {label: qsTr("Delete"), ic: VLCIcons.del}
                            ]
                            
                            delegate: Button {
                                id: reptrBtn
                                hoverEnabled: true
                                width: reptr.width
                                background: Rectangle{
                                    color: pressed? "#000": VLCStyle.colors.text
                                    width: parent.width
                                    height: parent.height
                                    radius: 3
                                }
                                contentItem: Item{
                                    implicitWidth: childrenRect.width
                                    implicitHeight: childrenRect.height

                                    Label {
                                        id: icon
                                        text:  reptrBtn.fontIcon
                                        font.family: VLCIcons.fontFamily
                                        font.pixelSize: VLCStyle.icon_normal
                                        verticalAlignment: Text.AlignVCenter
                                        color: pressed || hovered? VLCStyle.colors.accent : VLCStyle.colors.bgAlt
                                    }


                                    Label {
                                        anchors.left: icon.right
                                        anchors.leftMargin: VLCStyle.margin_normal
                                        text: reptrBtn.text
                                        font: reptrBtn.font
                                        verticalAlignment: Text.AlignVCenter
                                        color: pressed || hovered? VLCStyle.colors.accent : VLCStyle.colors.bgAlt
                                    }
                                }


                                text: modelData.label
                                property string fontIcon: modelData.ic
                                onClicked: reptr.handleClick(index)
                            }
                            function handleClick(index){
                                switch(index){
                                case 1:medialib.addToPlaylist( expandRect.model.id )
                                    break

                                default:
                                    console.log("you clicked on an unhandled index:",index)
                                }
                            }
                        }
                    }
                }


            }
            Button {
                id: closeBtn
                hoverEnabled: true
                width: VLCStyle.icon_medium
                height: VLCStyle.icon_medium
                anchors.right: parent.right
                background: Rectangle{
                    color: closeBtn.pressed? "#000": VLCStyle.colors.text
                    width: parent.width
                    height: parent.height
                    radius: 3
                }
                contentItem:Label {
                    text: closeBtn.text
                    font: VLCIcons.fontFamily
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    color: closeBtn.pressed || closeBtn.hovered? VLCStyle.colors.accent : VLCStyle.colors.bgAlt
                }

                text: VLCIcons.close
                font.pixelSize: VLCStyle.icon_normal
                font.family: VLCIcons.fontFamily
                onClicked: expandableGV.retract()
            }

        }

    }


    cellWidth: (VLCStyle.video_normal_width)
    cellHeight: (VLCStyle.video_normal_height) + VLCStyle.margin_xlarge + VLCStyle.margin_normal

    onSelectAll: expandableGV.model.selectAll()
    onSelectionUpdated: expandableGV.model.updateSelection( keyModifiers, oldIndex, newIndex )
    onActionAtIndex: expandableGV.model.actionAtIndex(index)

}
