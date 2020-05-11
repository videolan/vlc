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

import "qrc:///widgets/" as Widgets
import "qrc:///util/KeyHelper.js" as KeyHelper
import "qrc:///style/"

Widgets.NavigableFocusScope {
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
        //height: implicitHeight
        implicitHeight: contentLayout.implicitHeight + VLCStyle.margin_xsmall * 2
        width: parent.width

        anchors {
            bottom: parent.bottom
            right: parent.right
            left: parent.left
            top: arrowRect.bottom
        }
        //height: parent.height - arrowRect.height

        clip: true
        color: VLCStyle.colors.bgAlt

        RowLayout {
            id: contentLayout
            spacing: VLCStyle.margin_xsmall

            anchors.margins: VLCStyle.margin_small

            implicitHeight: artAndControl.implicitHeight

            anchors.fill: parent

            FocusScope {
                id: artAndControl

                focus: true

                implicitHeight: artAndControlLayout.implicitHeight
                implicitWidth: artAndControlLayout.implicitWidth

                Layout.preferredWidth: implicitWidth
                Layout.preferredHeight: implicitHeight
                Layout.alignment: Qt.AlignTop

                Column {
                    id: artAndControlLayout
                    anchors.margins: VLCStyle.margin_small
                    spacing: VLCStyle.margin_small

                    /* A bigger cover for the album */
                    Image {
                        id: expand_cover_id

                        height: VLCStyle.video_large_height
                        width: VLCStyle.video_large_width
                        source: model.thumbnail || VLCStyle.noArtCover
                        sourceSize: Qt.size(width, height)
                        fillMode: Image.PreserveAspectFit
                    }

                    Widgets.NavigableCol {
                        id: actionButtons

                        focus: true

                        width: expand_cover_id.width

                        model: ObjectModel {
                            Widgets.TabButtonExt {
                                id: playActionBtn

                                width: actionButtons.width

                                iconTxt: VLCIcons.play
                                text: i18n.qtr("Play")
                                onClicked: medialib.addAndPlay( model.id )
                            }

                            Widgets.TabButtonExt {
                                id: enqueueActionBtn

                                width: actionButtons.width

                                iconTxt: VLCIcons.add
                                text: i18n.qtr("Enqueue")
                                onClicked: medialib.addToPlaylist( model.id )
                            }
                        }

                        navigationParent: expandRect
                        navigationRightItem: infoPannelScrollView
                    }
                }
            }


            ColumnLayout {
                id: expand_infos_id

                Layout.fillWidth: true
                Layout.fillHeight: true

                spacing: VLCStyle.margin_xsmall

                Text {
                    id: expand_infos_title_id

                    Layout.preferredHeight: implicitHeight
                    Layout.fillWidth: true

                    text: model.title || i18n.qtr("Unknown title")
                    font.pixelSize: VLCStyle.fontSize_xxlarge
                    font.bold: true
                    color: VLCStyle.colors.text
                }

                Widgets.NavigableFocusScope {
                    id: infoPanel

                    Layout.fillHeight: true
                    Layout.fillWidth: true

                    navigationParent: expandRect
                    navigationLeftItem: actionButtons

                    ScrollView {
                        id: infoPannelScrollView

                        contentHeight: infoInnerCol.height

                        anchors.fill: parent
                        anchors.margins: VLCStyle.margin_xxsmall

                        focus: true
                        clip: true

                        ListView {
                            id: infoInnerCol
                            spacing: VLCStyle.margin_xsmall
                            model: [
                                {text: i18n.qtr("File Name"),    data: expandRect.model.title, bold: true},
                                {text: i18n.qtr("Path"),         data: expandRect.model.display_mrl},
                                {text: i18n.qtr("Length"),       data: expandRect.model.duration},
                                {text: i18n.qtr("File size"),    data: ""},
                                {text: i18n.qtr("Times played"), data: expandRect.model.playcount},
                                {text: i18n.qtr("Video track"),  data: expandRect.model.videoDesc},
                                {text: i18n.qtr("Audio track"),  data: expandRect.model.audioDesc},
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
                            if ( !KeyHelper.matchUp(event) && !KeyHelper.matchDown(event) ) {
                                infoPanel.defaultKeyAction(event, 0)
                            }
                        }
                    }

                    Rectangle {
                        z: 2
                        anchors.fill: parent
                        border.width: VLCStyle.dp(2)
                        border.color: VLCStyle.colors.accent
                        color: "transparent"
                        visible: infoPanel.activeFocus
                    }
                }
            }
        }
    }
}
