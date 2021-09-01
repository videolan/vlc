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
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"


Control {
    id: artworkInfoItem

    property bool paintOnly: false

    property VLCColors colors: VLCStyle.colors

    readonly property real minimumWidth: cover.width + (leftPadding + rightPadding)
    property real extraWidth: 0

    padding: VLCStyle.focus_border

    Keys.onPressed: {
        if (KeyHelper.matchOk(event))
            event.accepted = true

        Navigation.defaultKeyAction(event)
    }

    Keys.onReleased: {
        if (KeyHelper.matchOk(event)) {
            g_mainDisplay.showPlayer()
            event.accepted = true
        }
    }

    MouseArea {
        id: artworkInfoMouseArea
        anchors.fill: parent
        visible: !paintOnly
        onClicked: g_mainDisplay.showPlayer()
        hoverEnabled: true
    }

    background: Widgets.AnimatedBackground {
        active: visualFocus
        activeBorderColor: colors.bgFocus
    }

    contentItem: Row {
        spacing: infoColumn.visible ? VLCStyle.margin_xsmall : 0

        Item {
            id: coverItem

            anchors.verticalCenter: parent.verticalCenter

            implicitHeight: childrenRect.height
            implicitWidth:  childrenRect.width

            Rectangle {
                id: coverRect

                anchors.fill: cover

                color: colors.bg
            }

            DropShadow {
                anchors.fill: coverRect

                source: coverRect
                radius: 8
                samples: 17
                color: VLCStyle.colors.glowColorBanner
                spread: 0.2
            }

            Image {
                id: cover

                source: {
                    if (paintOnly)
                        VLCStyle.noArtAlbum
                    else
                        (mainPlaylistController.currentItem.artwork && mainPlaylistController.currentItem.artwork.toString())
                                                        ? mainPlaylistController.currentItem.artwork
                                                        : VLCStyle.noArtAlbum
                }
                fillMode: Image.PreserveAspectFit

                width: VLCStyle.dp(60)
                height: VLCStyle.dp(60)

                ToolTip {
                    x: parent.x

                    visible: artworkInfoItem.visible
                             && infoColumn.width < infoColumn.preferredWidth
                             && (artworkInfoMouseArea.containsMouse || artworkInfoItem.visualFocus)
                    delay: 500

                    contentItem: Text {
                        text: i18n.qtr("%1\n%2\n%3").arg(titleLabel.text).arg(artistLabel.text).arg(progressIndicator.text)
                        color: colors.tooltipTextColor
                    }

                    background: Rectangle {
                        color: colors.tooltipColor
                    }
                }
            }
        }

        Column {
            id: infoColumn
            anchors.verticalCenter: parent.verticalCenter

            readonly property real preferredWidth: Math.max(titleLabel.implicitWidth, artistLabel.implicitWidth, progressIndicator.implicitWidth)
            width: ((extraWidth > preferredWidth) || (paintOnly)) ? preferredWidth
                                                                  : extraWidth

            visible: width > VLCStyle.dp(15, VLCStyle.scale)

            Widgets.MenuLabel {
                id: titleLabel

                width: parent.width

                text: {
                    if (paintOnly)
                        i18n.qtr("Title")
                    else
                        mainPlaylistController.currentItem.title
                }
                color: colors.text
            }

            Widgets.MenuCaption {
                id: artistLabel

                width: parent.width

                text: {
                    if (paintOnly)
                        i18n.qtr("Artist")
                    else
                        mainPlaylistController.currentItem.artist
                }
                color: colors.menuCaption
            }

            Widgets.MenuCaption {
                id: progressIndicator

                width: parent.width

                text: {
                    if (paintOnly)
                        " -- / -- "
                    else
                        player.time.toString() + " / " + player.length.toString()
                }
                color: colors.menuCaption
            }
        }
    }
}
