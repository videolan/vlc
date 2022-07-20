/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import QtQuick.Templates 2.4 as T
import QtQuick.Layouts 1.11

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

T.TabButton {
    id: control

    // Properties

    property bool selected: false

    property string iconTxt: ""

    property bool showText: true
    property bool showCurrentIndicator: true

    property color color: VLCStyle.colors.topBanner

    // Settings

    width: control.showText ? VLCStyle.bannerTabButton_width_large
                            : VLCStyle.icon_banner

    height: implicitHeight

    implicitWidth: contentItem.implicitWidth
    implicitHeight: contentItem.implicitHeight

    padding: 0

    text: model.displayText

    // Keys

    Keys.priority: Keys.AfterItem

    Keys.onPressed: Navigation.defaultKeyAction(event)

    // Private functions

    function _getBackground() {
        if (activeFocus || hovered)
            return VLCStyle.colors.buttonHover;
        else
            return color;
    }

    function _getForeground() {
        if (activeFocus || hovered || selected)
            return VLCStyle.colors.buttonTextHover;
        else
            return VLCStyle.colors.buttonBanner;
    }

    // Childs

    background: Widgets.AnimatedBackground {
        height: control.height
        width: control.width

        active: visualFocus

        animationDuration: VLCStyle.duration_short

        backgroundColor: _getBackground()
        foregroundColor: _getForeground()
    }

    contentItem: Item {
        implicitWidth: tabRow.implicitWidth
        implicitHeight: tabRow.implicitHeight

        RowLayout {
            id: tabRow

            anchors.centerIn: parent

            spacing: VLCStyle.margin_xsmall

            Widgets.IconLabel {
                text: control.iconTxt

                color: (control.activeFocus ||
                        control.hovered     ||
                        control.selected) ? VLCStyle.colors.accent
                                          : VLCStyle.colors.text

                font.pixelSize: VLCStyle.icon_banner
            }

            T.Label {
                visible: showText

                text: control.text

                color: control.background.foregroundColor

                font.pixelSize: VLCStyle.fontSize_normal

                font.weight: (control.activeFocus ||
                              control.hovered     ||
                              control.selected) ? Font.DemiBold
                                                : Font.Normal
            }
        }

        Widgets.CurrentIndicator {
            width: tabRow.width

            orientation: Qt.Horizontal

            margin: VLCStyle.dp(3, VLCStyle.scale)

            visible: (control.showCurrentIndicator && control.selected)
        }
    }
}
