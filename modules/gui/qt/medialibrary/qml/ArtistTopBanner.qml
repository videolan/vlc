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
import QtQuick.Window
import QtQml.Models

import VLC.MainInterface
import VLC.MediaLibrary

import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util

FocusScope {
    id: root

    property int rightPadding: 0

    required property var artist

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
        source: root.artist.id ? (root.artist.cover || VLCStyle.noArtArtist) : "" // do not load the fallback image during initialization
        sourceSize: artist.cover ? Qt.size(Helpers.alignUp(Screen.desktopAvailableWidth / 2, 32), 0)
                                 : undefined
        mipmap: !!artist.cover

        fillMode: artist.cover ? Image.PreserveAspectCrop : Image.Tile

        visible: !blurEffect.visible
        cache: (source === VLCStyle.noArtArtist)

        // In fact, since layering is not used blur effect
        // would not care about the opacity here. Still,
        // to show intention we can have a simple binding:
        opacity: blurEffect.visible ? 1.0 : 0.5
    }

    Item {
        anchors.fill: background

        visible: (GraphicsInfo.shaderType === GraphicsInfo.RhiShader) && (root.artist.id) // do not display the effect during initialization

        // This blur effect does not create an implicit layer that is updated
        // each time the size changes. The source texture is static, so the blur
        // is applied only once and we adjust the viewport through the parent item
        // with clipping.
        Widgets.DualKawaseBlur {
            id: blurEffect

            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.right: parent.right

            // NOTE: No need to disable `live`, as this uses two pass mode so there is no video memory saving benefit.

            readonly property bool sourceNeedsTiling: (background.fillMode === Image.Tile)

            readonly property real aspectRatio: (background.implicitHeight / background.implicitWidth)

            height: sourceNeedsTiling ? background.height : (aspectRatio * width)

            source: textureProviderItem

            // Instead of clipping in the parent, denote the viewport here so we both
            // do not need to clip the excess, and also save significant video memory:
            viewportRect: !blurEffect.sourceNeedsLayering ? Qt.rect((width - parent.width) / 2, (height - parent.height) / 2, parent.width, parent.height)
                                                          : Qt.rect(0, 0, 0, 0)

            Widgets.TextureProviderItem {
                id: textureProviderItem

                // Like in `Player.qml`, this is used because when the source is
                // mipmapped, sometimes it can not be sampled. This is considered
                // a Qt bug, but `QSGTextureView` has a workaround for that. So,
                // we can have an indirection here through `TextureProviderItem`.
                // This is totally acceptable as there is virtually no overhead.

                source: background

                detachAtlasTextures: blurEffect.sourceNeedsTiling

                horizontalWrapMode: blurEffect.sourceNeedsTiling ? Widgets.TextureProviderItem.Repeat : Widgets.TextureProviderItem.ClampToEdge
                verticalWrapMode: blurEffect.sourceNeedsTiling ? Widgets.TextureProviderItem.Repeat : Widgets.TextureProviderItem.ClampToEdge

                textureSubRect: blurEffect.sourceNeedsTiling ? Qt.rect(blurEffect.width / 8,
                                                                       blurEffect.height / 8,
                                                                       blurEffect.width * 1.25,
                                                                       blurEffect.height * 1.25) : undefined
            }

            // Strong blurring is not wanted here:
            mode: Widgets.DualKawaseBlur.Mode.TwoPass
            radius: 1
        }
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

            Widgets.ImageExt {
                id: roundImage
                source: root.artist.id ? (root.artist.cover || VLCStyle.noArtArtist) : "" // do not load the fallback image during initialization
                textureProviderItem: background
                sourceSize: Qt.size(width * eDPR, height * eDPR)
                anchors.fill: parent
                radius: VLCStyle.cover_normal
                borderColor: theme.border
                borderWidth: VLCStyle.dp(1, VLCStyle.scale)
                fillMode: Image.PreserveAspectCrop
                readonly property real eDPR: MainCtx.effectiveDevicePixelRatio(Window.window)
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


                Widgets.ActionButtonPrimary {
                    id: playActionBtn
                    iconTxt: VLCIcons.play
                    text: qsTr("Play all")
                    focus: true

                    //we probably want to keep this button like the other action buttons
                    colorContext.palette: VLCStyle.palette

                    showText: actionButtons.width > VLCStyle.colWidth(2)

                    onClicked: MediaLib.addAndPlay( artist.id )
                }

                Widgets.ActionButtonOverlay {
                    id: enqueueActionBtn
                    iconTxt: VLCIcons.enqueue
                    text: qsTr("Enqueue all")
                    onClicked: MediaLib.addToPlaylist( artist.id )

                    showText: actionButtons.width > VLCStyle.colWidth(2)
                }

            }

            Item {
                Layout.fillWidth: true
            }
        }
    }

}
