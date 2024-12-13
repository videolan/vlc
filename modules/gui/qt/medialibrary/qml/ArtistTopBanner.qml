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
        sourceSize: artist.cover ? Qt.size(Helpers.alignUp(Screen.desktopAvailableWidth, 32), 0)
                                 : undefined
        mipmap: !!artist.cover
        // Fill mode is stretch when blur effect is used, otherwise an implicit layer is created.
        // Having the fill mode stretch does not have a side effect here, because source size
        // is still calculated as to preserve the aspect ratio as height is left empty (0) and the
        // image is not shown stretched because it is invisible when blur effect is used:
        // "If only one dimension of the size is set to greater than 0, the other dimension is
        //  set in proportion to preserve the source image's aspect ratio. (The fillMode is
        //  independent of this.)". Unfortunately with old Qt versions we can not do this
        // because it does not seem to create a layer when fill mode (tiling is wanted)
        // is changed at a later point.
        fillMode: artist.cover ? ((visible || (MainCtx.qtVersion() < MainCtx.qtVersionCheck(6, 5, 0))) ? Image.PreserveAspectCrop : Image.Stretch)
                                : Image.Tile

        visible: !blurEffect.visible
        cache: (source === VLCStyle.noArtArtist)

        // In fact, since layering is not used blur effect
        // would not care about the opacity here. Still,
        // to show intention we can have a simple binding:
        opacity: blurEffect.visible ? 1.0 : 0.5
    }

    Item {
        anchors.fill: background

        // The texture is big, and the blur item should only draw the portion of it.
        // If the blur effect creates an implicit layer, it properly adjusts the
        // area that it needs to cover. However, as we don't want an additional
        // layer that keeps getting updated every time the size changes, we feed
        // the whole static texture. For that reason, we need clipping because
        // the blur effect is applied to the whole texture and shown as whole:
        clip: !blurEffect.sourceNeedsLayering

        visible: (GraphicsInfo.shaderType === GraphicsInfo.RhiShader)

        // This blur effect does not create an implicit layer that is updated
        // each time the size changes. The source texture is static, so the blur
        // is applied only once and we adjust the viewport through the parent item
        // with clipping.
        Widgets.BlurEffect {
            id: blurEffect

            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.right: parent.right

            // If source image is tiled, layering is necessary:
            readonly property bool sourceNeedsLayering: (background.fillMode !== Image.Stretch) ||
                                                        (MainCtx.qtVersion() < MainCtx.qtVersionCheck(6, 5, 0))

            readonly property real aspectRatio: (background.implicitHeight / background.implicitWidth)

            height: sourceNeedsLayering ? background.height : (aspectRatio * width)

            source: background

            radius: VLCStyle.dp(4, VLCStyle.scale)
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

            Widgets.RoundImage {
                id: roundImage
                source: artist.cover || VLCStyle.noArtArtist
                sourceSize: Qt.size(width * eDPR, height * eDPR)
                anchors.fill: parent
                radius: VLCStyle.cover_normal
                readonly property real eDPR: MainCtx.effectiveDevicePixelRatio(Window.window)
            }

            Rectangle {
                anchors.fill: parent
                radius: roundImage.effectiveRadius
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
