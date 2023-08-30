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
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import Qt5Compat.GraphicalEffects

import org.videolan.medialib 0.1
import org.videolan.controls 0.1
import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"
import "qrc:///util/Helpers.js" as Helpers

FocusScope {
    id: root

    property int rightPadding: 0

    property var artist: ({})

    implicitHeight: VLCStyle.artistBanner_height

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Artist banner")

    function setCurrentItemFocus(reason) {
        playActionBtn.forceActiveFocus(reason);
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        ////force the dark theme in the header
        palette: VLCStyle.darkPalette
        colorSet: ColorContext.View
    }

    Image {
        id: background

        // NOTE: We want the banner to ignore safe margins.
        anchors.fill: parent

        asynchronous: true
        source: artist.cover || VLCStyle.noArtArtist
        sourceSize: artist.cover ? Qt.size(MainCtx.screen ? Helpers.alignUp(MainCtx.screen.availableGeometry.width, 32) : 1024, 0)
                                 : undefined
        mipmap: !!artist.cover
        fillMode: artist.cover ? Image.PreserveAspectCrop : Image.Tile
        visible: layer.enabled
        cache: (source === VLCStyle.noArtArtist)

        // Single pass linear filtering, in case the effect is not available:
        layer.enabled: (GraphicsInfo.shaderType === GraphicsInfo.UnknownShadingLanguage)
        layer.smooth: true
        layer.textureSize: Qt.size(width * .75, height * .75)
    }

    FastBlur {
        anchors.fill: background
        source: background
        radius: VLCStyle.dp(4, VLCStyle.scale)
    }

    Rectangle {
        anchors.fill: background
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, .5) }
            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, .7) }
        }
    }

    RowLayout {
        id: col

        anchors.fill: parent
        anchors.topMargin: VLCStyle.margin_xxlarge
        anchors.bottomMargin: VLCStyle.margin_xxlarge
        anchors.leftMargin: VLCStyle.margin_xlarge
        anchors.rightMargin: root.rightPadding

        spacing: VLCStyle.margin_normal

        Item {
            implicitHeight: VLCStyle.cover_normal
            implicitWidth: VLCStyle.cover_normal

            RoundImage {
                source: artist.cover || VLCStyle.noArtArtist
                anchors.fill: parent
                radius: VLCStyle.cover_normal
            }

            Rectangle {
                anchors.fill: parent
                radius: VLCStyle.cover_normal
                color: "transparent"
                border.width: VLCStyle.dp(1, VLCStyle.scale)
                border.color: theme.border
            }
        }

        ColumnLayout {
            Layout.fillWidth: true

            // NOTE: The layout can be resized to zero to hide the text entirely.
            Layout.minimumWidth: 0

            Layout.rightMargin: VLCStyle.margin_small

            spacing: 0

            Widgets.SubtitleLabel {
                Layout.fillWidth: true

                text: artist.name || qsTr("No artist")
                color: theme.fg.primary

                Layout.maximumWidth: parent.width
            }

            Widgets.MenuCaption {
                Layout.fillWidth: true

                Layout.topMargin: VLCStyle.margin_xxxsmall

                text: qsTr("%1 Songs").arg(artist.nb_tracks)
                color: theme.fg.secondary
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
                        iconTxt: VLCIcons.play
                        text: qsTr("Play all")
                        focus: true

                        //we probably want to keep this button like the other action buttons
                        colorContext.palette: VLCStyle.palette

                        onClicked: MediaLib.addAndPlay( artist.id )
                    }

                    Widgets.ActionButtonOverlay {
                        id: enqueueActionBtn
                        iconTxt: VLCIcons.enqueue
                        text: qsTr("Enqueue all")
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
