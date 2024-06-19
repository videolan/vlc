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
import QtQuick.Templates as T
import QtQuick.Controls


import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets
import VLC.Util

T.ComboBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + spacing + implicitIndicatorWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: VLCStyle.margin_xxsmall
    font.pixelSize: VLCStyle.fontSize_large

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ComboBox

        focused: control.activeFocus
        enabled: control.enabled
        hovered: control.hovered
        pressed: control.pressed
    }

    property color color: theme.fg.primary
    property color bgColor: theme.bg.primary
    property color borderColor: theme.border

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

    delegate: T.ItemDelegate {
        width: control.width
        height: implicitContentHeight + topPadding + bottomPadding
        padding: VLCStyle.margin_xsmall
        leftPadding: control.leftPadding
        contentItem: Widgets.ListLabel {
            text: control.textRole ? (Helpers.isArray(control.model) ? modelData[control.textRole] : model[control.textRole]) : modelData
            color: control.color
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        highlighted: control.highlightedIndex === index
    }

    indicator: Widgets.IconLabel {
        x: control.width - width - rightPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        font.pixelSize: VLCStyle.icon_normal
        font.bold: true
        text: VLCIcons.expand
        color: control.color
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }

    contentItem: Widgets.ListLabel {
        rightPadding: control.spacing + control.indicator.width

        text: control.displayText
        color: control.color
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Widgets.AnimatedBackground {
        enabled: theme.initialized
        color: control.bgColor
        border.color: theme.border
        border.width: VLCStyle.dp(2, VLCStyle.scale)
        radius: VLCStyle.dp(2, VLCStyle.scale)
    }

    popup: Popup {
        y: control.height - 1

        // NOTE: This Popup should be on top of other Popup(s) most of the time.
        z: 100

        width: control.width
        padding: 1

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex

            highlight: Widgets.AnimatedBackground {
                enabled: theme.initialized
                border.color: visualFocus ? theme.visualFocus : "transparent"
                color: theme.bg.secondary
            }

            ScrollIndicator.vertical: ScrollIndicator { }
        }

        background: Rectangle {
            color: control.bgColor
            border.color: control.borderColor
            radius: 2
        }
    }
}
