/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"


AbstractButton {
    id: artworkInfoItem

    property bool paintOnly: false

    property VLCColors colors: VLCStyle.colors

    readonly property real minimumWidth: cover.width + (leftPadding + rightPadding)

    property bool _keyPressed: false

    padding: VLCStyle.focus_border

    Keys.onPressed: {
        if (KeyHelper.matchOk(event)) {
            event.accepted = true

            _keyPressed = true
        } else {
            Navigation.defaultKeyAction(event)
        }
    }

    Keys.onReleased: {
        if (_keyPressed === false)
            return

        _keyPressed = false

        if (KeyHelper.matchOk(event)) {
            event.accepted = true

            g_mainDisplay.showPlayer()
        }
    }

    onClicked: {
        g_mainDisplay.showPlayer()
    }

    background: Widgets.AnimatedBackground {
        active: visualFocus
        activeBorderColor: colors.bgFocus
    }

    contentItem: RowLayout {
        spacing: infoColumn.visible ? VLCStyle.margin_xsmall : 0

        Item {
            id: coverItem

            implicitHeight: cover.height
            implicitWidth: cover.width

            Rectangle {
                id: coverRect
                anchors.fill: parent

                color: colors.bg
            }

            Widgets.DoubleShadow {
                anchors.fill: parent

                primaryBlurRadius: VLCStyle.dp(3, VLCStyle.scale)
                primaryColor: Qt.rgba(0, 0, 0, 0.18)
                primaryVerticalOffset: VLCStyle.dp(1, VLCStyle.scale)

                secondaryBlurRadius: VLCStyle.dp(14, VLCStyle.scale)
                secondaryColor: Qt.rgba(0, 0, 0, 0.22)
                secondaryVerticalOffset: VLCStyle.dp(6, VLCStyle.scale)
            }

            Widgets.ScaledImage {
                id: cover

                source: {
                    if (!paintOnly
                        && mainPlaylistController.currentItem.artwork
                        && mainPlaylistController.currentItem.artwork.toString())
                        mainPlaylistController.currentItem.artwork
                    else
                        VLCStyle.noArtAlbumCover
                }

                fillMode: Image.PreserveAspectFit

                width: VLCStyle.dp(60)
                height: VLCStyle.dp(60)

                asynchronous: true

                ToolTip.visible: infoColumn.width < infoColumn.implicitWidth
                                 && (artworkInfoItem.hovered || artworkInfoItem.visualFocus)
                ToolTip.delay: VLCStyle.delayToolTipAppear
                ToolTip.text: I18n.qtr("%1\n%2\n%3").arg(titleLabel.text)
                                                    .arg(artistLabel.text)
                                                    .arg(progressIndicator.text)

                property alias colors: artworkInfoItem.colors
            }
        }

        ColumnLayout {
            id: infoColumn

            Layout.preferredHeight: coverItem.implicitHeight
            Layout.fillWidth: true

            clip: true

            Widgets.MenuLabel {
                id: titleLabel

                Layout.fillWidth: true
                Layout.fillHeight: true

                text: {
                    if (paintOnly)
                        I18n.qtr("Title")
                    else
                        mainPlaylistController.currentItem.title
                }
                color: colors.text
            }

            Widgets.MenuCaption {
                id: artistLabel

                Layout.fillWidth: true
                Layout.fillHeight: true

                text: {
                    if (paintOnly)
                        I18n.qtr("Artist")
                    else
                        mainPlaylistController.currentItem.artist
                }
                color: colors.menuCaption
            }

            Widgets.MenuCaption {
                id: progressIndicator

                Layout.fillWidth: true
                Layout.fillHeight: true

                text: {
                    if (paintOnly)
                        " -- / -- "
                    else
                        Player.time.toString() + " / " + Player.length.toString()
                }
                color: colors.menuCaption
            }
        }
    }
}
