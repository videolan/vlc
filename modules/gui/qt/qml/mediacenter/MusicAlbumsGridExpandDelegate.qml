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

import org.videolan.medialib 0.1

import "qrc:///utils/" as Utils
import "qrc:///style/"

Utils.NavigableFocusScope {
    id: root
    property var model: []
    //color: VLCStyle.colors.bg
    implicitHeight: layout.height

    Row {
        id: layout
        spacing: VLCStyle.margin_xsmall
        width: parent.width
        height: Math.max(expand_infos_id.height, artAndControl.height)

        FocusScope {
            id: artAndControl
            //width: VLCStyle.cover_large + VLCStyle.margin_small * 2
            //height: VLCStyle.cover_xsmall + VLCStyle.cover_large
            width:  artAndControlLayout.implicitWidth
            height:  artAndControlLayout.implicitHeight

            Column {
                id: artAndControlLayout
                anchors.margins: VLCStyle.margin_small
                spacing: VLCStyle.margin_small

                Item {
                    //dummy item for margins
                    width: parent.width
                    height: 1
                }

                /* A bigger cover for the album */
                Image {
                    id: expand_cover_id
                    height: VLCStyle.cover_large
                    width: VLCStyle.cover_large
                    source: model.cover || VLCStyle.noArtCover
                }

                RowLayout {
                    anchors {
                        left: parent.left
                        right: parent.right
                    }

                    Utils.IconToolButton {
                        id: addButton
                        focus: true

                        Layout.preferredWidth: VLCStyle.icon_normal
                        Layout.preferredHeight: VLCStyle.icon_normal
                        size: VLCStyle.icon_normal
                        Layout.alignment: Qt.AlignHCenter

                        text: VLCIcons.add

                        onClicked: medialib.addToPlaylist(model.id)

                        KeyNavigation.right: playButton
                    }
                    Utils.IconToolButton {
                        id: playButton

                        Layout.preferredWidth: VLCStyle.icon_normal
                        Layout.preferredHeight: VLCStyle.icon_normal
                        Layout.alignment: Qt.AlignHCenter
                        size: VLCStyle.icon_normal

                        text: VLCIcons.play

                        onClicked: medialib.addAndPlay(model.id)

                        Keys.onRightPressed: {
                            expand_track_id.focus = true
                            event.accepted = true
                        }
                    }
                }

                Item {
                    //dummy item for margins
                    width: parent.width
                    height: 1
                }
            }
        }


        Column {
            id: expand_infos_id

            spacing: VLCStyle.margin_xsmall
            width: root.width - x

            /* The title of the albums */
            // Needs a rectangle too prevent the tracks from overlapping the title when scrolled
            Rectangle {
                id: expand_infos_titleRect_id
                height: expand_infos_title_id.implicitHeight
                anchors {
                    left: parent.left
                    right: parent.right
                    topMargin: VLCStyle.margin_small
                    leftMargin: VLCStyle.margin_small
                    rightMargin: VLCStyle.margin_small
                }
                color: "transparent"
                Text {
                    id: expand_infos_title_id
                    text: "<b>"+(model.title || qsTr("Unknown title") )+"</b>"
                    font.pixelSize: VLCStyle.fontSize_xxlarge
                    color: VLCStyle.colors.text
                }
            }

            Rectangle {
                id: expand_infos_subtitleRect_id
                height: expand_infos_subtitle_id.implicitHeight
                anchors {
                    left: parent.left
                    right: parent.right
                    topMargin: VLCStyle.margin_xxsmall
                    leftMargin: VLCStyle.margin_small
                    rightMargin: VLCStyle.margin_small
                }

                color: "transparent"
                Text {
                    id: expand_infos_subtitle_id
                    text: qsTr("By %1 - %2 - %3")
                    .arg(model.main_artist || qsTr("Unknown title"))
                    .arg(model.release_year || "")
                    .arg(model.duration || "")
                    font.pixelSize: VLCStyle.fontSize_large
                    color: VLCStyle.colors.text
                }
            }

            /* The list of the tracks available */
            MusicTrackListDisplay {
                id: expand_track_id

                height: expand_track_id.contentHeight
                anchors {
                    left: parent.left
                    right: parent.right
                    topMargin: VLCStyle.margin_xxsmall
                    leftMargin: VLCStyle.margin_small
                    rightMargin: VLCStyle.margin_small
                    bottomMargin: VLCStyle.margin_small
                }

                interactive: false

                parentId : root.model.id
                sortModel: ListModel {
                    ListElement{ criteria: "track_number";  width:0.10; visible: true; text: qsTr("#"); showSection: "" }
                    ListElement{ criteria: "title";         width:0.70; visible: true; text: qsTr("TITLE"); showSection: "" }
                    ListElement{ criteria: "duration";      width:0.20; visible: true; text: qsTr("DURATION"); showSection: "" }
                }
                focus: true

                onActionLeft:  playButton.forceActiveFocus()
                onActionRight: root.actionRight(index)
                onActionUp: root.actionUp(index)
                onActionDown: root.actionDown(index)
                onActionCancel: root.actionCancel(index)
            }

            Item {
                //dummy item for margins
                width: parent.width
                height: 1
            }
        }
    }


    Keys.priority:  KeyNavigation.AfterItem
    Keys.onPressed:  defaultKeyAction(event, 0)
}
