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

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

AbstractButton {
    id: root

    // Properties

    property bool paintOnly: false

    Layout.minimumWidth: height

    implicitHeight: 0

    property bool _keyPressed: false

    readonly property ColorContext colorContext: ColorContext {
        id: theme

        colorSet: ColorContext.ToolButton

        focused: root.visualFocus
        hovered: root.hovered
    }

    // Settings

    text: qsTr("Open player")

    padding: VLCStyle.focus_border

    Accessible.onPressAction: root.clicked()

    // Keys

    Keys.onPressed: (event) => {
        if (KeyHelper.matchOk(event)) {
            event.accepted = true

            _keyPressed = true
        } else {
            Navigation.defaultKeyAction(event)
        }
    }

    Keys.onReleased: (event) => {
        if (_keyPressed === false)
            return

        _keyPressed = false

        if (KeyHelper.matchOk(event)) {
            event.accepted = true

            History.push(["player"])
        }
    }

    // Events

    onClicked: History.push(["player"])

    background: Widgets.AnimatedBackground {
        enabled: theme.initialized
        border.color: visualFocus ? theme.visualFocus : "transparent"
    }

    // Children

    contentItem: RowLayout {
        spacing: VLCStyle.margin_xsmall

        Image {
            id: coverImage

            Layout.fillHeight: true
            Layout.preferredWidth: height

            source: {
                if (!paintOnly && Player.artwork && Player.artwork.toString())
                    return VLCAccessImage.uri(Player.artwork)
                else
                    return VLCStyle.noArtAlbumCover
            }

            sourceSize.height: root.height * MainCtx.screen.devicePixelRatio

            fillMode: Image.PreserveAspectFit

            asynchronous: true

            Accessible.role: Accessible.Graphic
            Accessible.name: qsTr("Cover")

            ToolTip.visible: infoColumn.width < infoColumn.implicitWidth
                             && (root.hovered || root.visualFocus)
            ToolTip.delay: VLCStyle.delayToolTipAppear
            ToolTip.text: qsTr("%1\n%2\n%3").arg(titleLabel.text)
                                                .arg(artistLabel.text)
                                                .arg(progressIndicator.text)

            Widgets.DefaultShadow {
                anchors.centerIn: coverImage

                sourceItem: coverImage

            }
        }

        ColumnLayout {
            id: infoColumn

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0.1 // FIXME: Qt layout bug

            spacing: 0

            Widgets.MenuLabel {
                id: titleLabel

                Layout.fillWidth: true
                Layout.fillHeight: true

                visible: text.length > 0

                text: {
                    if (paintOnly)
                        qsTr("Title")
                    else
                        Player.title
                }
                color: theme.fg.primary
            }

            Widgets.MenuCaption {
                id: artistLabel

                Layout.fillWidth: true
                Layout.fillHeight: true

                Binding on visible {
                    delayed: (MainCtx.qtVersion() < MainCtx.qtVersionCheck(5, 15, 8))
                    value: (infoColumn.height > infoColumn.implicitHeight) && (artistLabel.text.length > 0)
                }

                text: {
                    if (paintOnly)
                        qsTr("Artist")
                    else
                        Player.artist
                }

                color: theme.fg.secondary
            }

            Widgets.MenuCaption {
                id: progressIndicator

                Layout.fillWidth: true
                Layout.fillHeight: true

                visible: text.length > 0

                text: {
                    if (paintOnly)
                        " -- / -- "
                    else
                        Player.time.formatHMS() + " / " + Player.length.formatHMS()
                }
                color: theme.fg.secondary
            }
        }
    }
}
