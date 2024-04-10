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

import QtQuick
import QtQuick.Templates as T
import QtQuick.Layouts

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

    // Settings

    width: control.showText ? VLCStyle.bannerTabButton_width_large
                            : VLCStyle.icon_banner

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: VLCStyle.margin_xxsmall

    text: model.displayText

    // Keys

    Keys.priority: Keys.AfterItem

    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

    // Accessible

    Accessible.onPressAction: control.clicked()

    // Tooltip

    T.ToolTip.visible: (showText === false && T.ToolTip.text && (hovered || visualFocus))

    T.ToolTip.delay: VLCStyle.delayToolTipAppear

    T.ToolTip.text: text

    // Childs

    ColorContext {
        id: theme
        colorSet: ColorContext.TabButton

        focused: control.visualFocus
        hovered: control.hovered
        pressed: control.down
        enabled: control.enabled
    }

    background: Widgets.AnimatedBackground {
        enabled: theme.initialized

        animationDuration: VLCStyle.duration_short

        color: theme.bg.primary
        border.color: visualFocus ? theme.visualFocus : "transparent"

        Widgets.CurrentIndicator {
            anchors {
                bottom: parent.bottom
                bottomMargin: VLCStyle.margin_xxxsmall
                horizontalCenter: parent.horizontalCenter
            }

            width: control.contentItem?.implicitWidth ?? 0

            visible: (width > 0 && control.showCurrentIndicator && control.selected)
        }
    }

    contentItem: RowLayout {
        spacing: 0

        Item {
            Layout.fillWidth: true
        }

        Widgets.IconLabel {
            id: iconLabel

            visible: text.length > 0

            text: control.iconTxt

            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            color: (control.selected || control.activeFocus || control.hovered)
                    ? theme.accent
                    : theme.fg.primary

            font.pixelSize: VLCStyle.icon_banner

            Layout.fillWidth: !label.visible
            Layout.fillHeight: true
        }

        T.Label {
            id: label

            visible: showText

            text: control.text

            verticalAlignment: Text.AlignVCenter

            color: control.selected ? theme.fg.secondary : theme.fg.primary

            elide: Text.ElideRight

            font.pixelSize: VLCStyle.fontSize_normal

            font.weight: (control.activeFocus ||
                          control.hovered     ||
                          control.selected) ? Font.DemiBold
                                            : Font.Normal

            //button text is already exposed
            Accessible.ignored: true

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.maximumWidth: implicitWidth + 1
            Layout.leftMargin: iconLabel.visible ? VLCStyle.margin_xsmall : 0

            Behavior on color {
                enabled: theme.initialized

                ColorAnimation {
                    duration: VLCStyle.duration_short
                }
            }
        }

        Item {
            Layout.fillWidth: true
        }
    }
}
