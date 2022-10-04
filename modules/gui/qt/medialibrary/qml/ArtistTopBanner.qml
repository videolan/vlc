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
import QtQuick.Layouts 1.11
import QtQml.Models 2.11
import QtGraphicalEffects 1.0

import org.videolan.medialib 0.1
import org.videolan.controls 0.1
import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"
import "qrc:///util/Helpers.js" as Helpers

FocusScope {
    id: root

    property var artist: ({})

    height: VLCStyle.artistBanner_height

    function setCurrentItemFocus(reason) {
        playActionBtn.forceActiveFocus(reason);
    }

    Image {
        id: background
        asynchronous: true

        width: parent.width
        height: VLCStyle.artistBanner_height
        source: artist.cover || VLCStyle.noArtArtist
        sourceSize: artist.cover ? Qt.size(MainCtx.screen ? Helpers.alignUp(MainCtx.screen.availableGeometry.width, 32) : 1024, 0)
                                 : undefined
        mipmap: !!artist.cover
        fillMode: artist.cover ? Image.PreserveAspectCrop : Image.Tile
        visible: false

        Rectangle {
            anchors.fill: background
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, .5) }
                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, .7) }
            }
        }
    }

    FastBlur {
        source: background
        anchors.fill: background
        radius: VLCStyle.dp(4, VLCStyle.scale)
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

            RoundImage {
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
                text: artist.name || I18n.qtr("No artist")
                color: "white"
            }

            Widgets.MenuCaption {
                text: I18n.qtr("%1 Songs").arg(artist.nb_tracks)
                color: "white"
                opacity: .6

                Layout.topMargin: VLCStyle.margin_xxxsmall
            }

            Widgets.NavigableRow {
                id: actionButtons

                focus: true
                Navigation.parentItem: root
                spacing: VLCStyle.margin_large

                Layout.fillWidth: true
                Layout.topMargin: VLCStyle.margin_large

                model: ObjectModel {
                    Widgets.ActionButtonPrimary {
                        id: playActionBtn
                        iconTxt: VLCIcons.play_outline
                        text: I18n.qtr("Play all")
                        focus: true
                        // NOTE: In overlay, the focus rectangle is always white.
                        colorFocus: VLCStyle.colors.white
                        onClicked: MediaLib.addAndPlay( artist.id )
                    }

                    Widgets.ActionButtonOverlay {
                        id: enqueueActionBtn
                        iconTxt: VLCIcons.enqueue
                        text: I18n.qtr("Enqueue all")
                        onClicked: MediaLib.addToPlaylist( artist.id )
                    }
                }
            }

            Item {
                Layout.fillWidth: true
            }
        }
    }

}
