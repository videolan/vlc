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
import QtQml.Models 2.11
import QtGraphicalEffects 1.0

import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root

    property var artist: ({})

    height: VLCStyle.artistBanner_height

    Image {
        id: background
        asynchronous: true

        width: parent.width
        height: VLCStyle.artistBanner_height
        source: artist.cover || VLCStyle.noArtArtist
        fillMode: artist.cover ? Image.PreserveAspectCrop : Image.Tile

        Rectangle {
            anchors.fill: background
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, .5) }
                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, .7) }
            }
        }
    }

    GaussianBlur {
        source: background
        anchors.fill: background
        radius: VLCStyle.dp(4, VLCStyle.scale)
        samples: (radius * 2) + 1
    }

    RowLayout {
        id: col

        anchors.fill: background
        anchors.topMargin: VLCStyle.margin_xxlarge
        anchors.bottomMargin: VLCStyle.margin_xxlarge
        anchors.leftMargin: VLCStyle.margin_xlarge
        spacing: VLCStyle.margin_normal
        clip: true

        Item {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: VLCStyle.cover_normal
            Layout.preferredWidth: VLCStyle.cover_normal

            Widgets.RoundImage {
                source: artist.cover || VLCStyle.noArtArtist
                height: VLCStyle.cover_normal
                width: VLCStyle.cover_normal
                radius: VLCStyle.cover_normal

            }

            Rectangle {
                height: VLCStyle.cover_normal
                width: VLCStyle.cover_normal
                radius: VLCStyle.cover_normal
                color: "transparent"
                border.width: VLCStyle.dp(1, VLCStyle.scale)
                border.color: VLCStyle.colors.roundPlayCoverBorder
            }
        }

        ColumnLayout {
            spacing: 0

            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true

            Widgets.SubtitleLabel {
                text: artist.name || i18n.qtr("No artist")
                color: "white"
            }

            Widgets.MenuCaption {
                text: i18n.qtr("%1 Songs").arg(artist.nb_tracks)
                color: "white"
                opacity: .6

                Layout.topMargin: VLCStyle.margin_xxxsmall
            }

            Widgets.NavigableRow {
                id: actionButtons

                focus: true
                navigationParent: root
                spacing: VLCStyle.margin_large

                Layout.fillWidth: true
                Layout.topMargin: VLCStyle.margin_large

                model: ObjectModel {
                    Widgets.TabButtonExt {
                        id: playActionBtn
                        iconTxt: VLCIcons.play
                        text: i18n.qtr("Play all")
                        color: "white"
                        focus: true
                        onClicked: medialib.addAndPlay( artist.id )
                    }

                    Widgets.TabButtonExt {
                        id: enqueueActionBtn
                        iconTxt: VLCIcons.enqueue
                        text: i18n.qtr("Enqueue all")
                        color: "white"
                        onClicked: medialib.addToPlaylist( artist.id )
                    }
                }
            }

            Item {
                Layout.fillWidth: true
            }
        }
    }

}
