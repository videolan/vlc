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

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root
    property variant model: MLAlbumModel{}
    //color: VLCStyle.colors.bg
    implicitHeight: layout.height
    clip: true

    width: parent.width

    property int currentItemY
    property int currentItemHeight

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
                    source: model.cover || VLCStyle.noArtAlbum
                    sourceSize: Qt.size(width, height)
                }

                RowLayout {
                    anchors {
                        left: parent.left
                        right: parent.right
                    }

                    Widgets.IconToolButton {
                        id: addButton
                        focus: true

                        Layout.preferredWidth: VLCStyle.icon_normal
                        Layout.preferredHeight: VLCStyle.icon_normal
                        size: VLCStyle.icon_normal
                        Layout.alignment: Qt.AlignHCenter

                        iconText: VLCIcons.add
                        text: i18n.qtr("Enqueue")

                        onClicked: medialib.addToPlaylist(model.id)

                        KeyNavigation.right: playButton
                    }
                    Widgets.IconToolButton {
                        id: playButton

                        Layout.preferredWidth: VLCStyle.icon_normal
                        Layout.preferredHeight: VLCStyle.icon_normal
                        Layout.alignment: Qt.AlignHCenter
                        size: VLCStyle.icon_normal

                        iconText: VLCIcons.play
                        text: i18n.qtr("Play")

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
                    text: "<b>"+(model.title || i18n.qtr("Unknown title") )+"</b>"
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
                    text: i18n.qtr("By %1 - %2 - %3")
                    .arg(model.main_artist || i18n.qtr("Unknown title"))
                    .arg(model.release_year || "")
                    .arg(model.duration || "")
                    font.pixelSize: VLCStyle.fontSize_large
                    color: VLCStyle.colors.text
                }
            }

            /* The list of the tracks available */
            MusicTrackListDisplay {
                id: expand_track_id

                section.property: ""

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

                headerColor: VLCStyle.colors.bgAlt

                parentId : root.model.id
                onParentIdChanged: {
                    currentIndex = 0
                    focus = true
                }

                onCurrentItemChanged: {
                    if (currentItem != undefined) {
                        root.currentItemY = expand_infos_id.y + expand_track_id.y + headerItem.height + currentItem.y
                        root.currentItemHeight = currentItem.height
                    }
                }

                sortModel: [
                    { criteria: "track_number",  width:0.10, visible: true, text: i18n.qtr("#"), showSection: "" },
                    { isPrimary: true, criteria: "title",         width:0.70, visible: true, text: i18n.qtr("Title"), showSection: "" },
                    { criteria: "duration",      width:0.20, visible: true, text: i18n.qtr("Duration"), showSection: "" },
                ]
                focus: true

                navigationParent: root
                navigationLeft: function() { playButton.forceActiveFocus() }
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
