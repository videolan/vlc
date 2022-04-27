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
import QtQuick.Templates 2.4 as T
import QtQuick.Layouts 1.11

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

// FIXME: We should refactor this item with a list of presets.
ColumnLayout {
    id: root

    property VLCColors colors: VLCStyle.nightColors

    Widgets.ListLabel {
        text: I18n.qtr("Playback Speed")
        color: root.colors.text
        font.pixelSize: VLCStyle.fontSize_large

        Layout.fillWidth: true
        Layout.bottomMargin: VLCStyle.margin_xsmall

        Layout.alignment: Qt.AlignTop
    }

    Slider {
        id: speedSlider

        // '_inhibitPlayerUpdate' is used to guard against double update
        // initialize with true so that we don't update the Player till
        // we initialize `value` property
        property bool _inhibitPlayerUpdate: true

        from: 0.25
        to: 4
        clip: true
        implicitHeight: VLCStyle.heightBar_small

        Navigation.parentItem: root
        Navigation.downItem: resetButton
        Keys.priority: Keys.AfterItem
        Keys.onPressed: Navigation.defaultKeyAction(event)

        background: Rectangle {
            x: speedSlider.leftPadding
            y: speedSlider.topPadding + speedSlider.availableHeight / 2 - height / 2
            implicitWidth: 200
            implicitHeight: 4
            width: speedSlider.availableWidth
            height: implicitHeight
            radius: 2
            color: root.colors.bgAlt

            Rectangle {
                width: speedSlider.visualPosition * parent.width
                height: parent.height
                radius: 2
                color: (speedSlider.visualFocus || speedSlider.pressed)
                       ? root.colors.accent
                       : root.colors.text
            }
        }

        handle: Rectangle {
            x: speedSlider.leftPadding + speedSlider.visualPosition * (speedSlider.availableWidth - width)
            y: speedSlider.topPadding + speedSlider.availableHeight / 2 - height / 2
            width: speedSlider.implicitHeight
            height: speedSlider.implicitHeight
            radius: speedSlider.implicitHeight
            color: (speedSlider.visualFocus || speedSlider.pressed) ? root.colors.accent : root.colors.text
        }

        onValueChanged:  {
            if (_inhibitPlayerUpdate)
                return
            Player.rate = value
        }

        function _updateFromPlayer() {
            _inhibitPlayerUpdate = true
            value = Player.rate
            _inhibitPlayerUpdate = false
        }

        Connections {
            target: Player
            onRateChanged: speedSlider._updateFromPlayer()
        }

        Layout.fillWidth: true

        Component.onCompleted: speedSlider._updateFromPlayer()
    }

    RowLayout {
        id: buttonLayout

        spacing: 0

        Navigation.parentItem: root
        Navigation.upItem: speedSlider

        Widgets.IconControlButton {
            id: slowerButton

            iconText: VLCIcons.slower
            colors: root.colors

            Navigation.parentItem: buttonLayout
            Navigation.rightItem: resetButton

            onClicked: speedSlider.decrease()
        }

        Item {
            Layout.fillWidth: true
        }

        Widgets.IconControlButton {
            id: resetButton

            colors: root.colors

            Navigation.parentItem: buttonLayout
            Navigation.leftItem: slowerButton
            Navigation.rightItem: fasterButton

            onClicked: speedSlider.value = 1.0

            focus: true

            T.Label {
                anchors.centerIn: parent
                font.pixelSize: VLCStyle.fontSize_normal
                text: I18n.qtr("1x")
                color: resetButton.background.foregroundColor // IconToolButton.background is a AnimatedBackground
            }
        }

        Item {
            Layout.fillWidth: true
        }

        Widgets.IconControlButton {
            id: fasterButton

            iconText: VLCIcons.faster
            colors: root.colors

            Navigation.parentItem: buttonLayout
            Navigation.leftItem: resetButton

            onClicked: speedSlider.increase()
        }
    }
}
