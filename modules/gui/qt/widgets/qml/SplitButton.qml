/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

import VLC.MainInterface
import VLC.Menus
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util

ButtonExt {
    id: root

    property var model: []

    property alias menu: _menu

    signal menuSelected(int index, string text)

    signal menuIndicatorClicked()

    rightPadding: leftPadding + implicitIndicatorWidth

    spacing: VLCStyle.margin_xxsmall

    onKeyPressedFunc: function(event) {
        if (KeyHelper.matchRight(event) && contentItem?.menuIndicatorButton) {
            contentItem.menuIndicatorButton.forceActiveFocus(Qt.TabFocusReason)
            event.accepted = true
            return
        }

        Navigation.defaultKeyAction(event)
    }

    colorContext {
        colorSet: ColorContext.ButtonAccent
        focused: root.visualFocus
        hovered: root.hovered && !_menu.visible && !root.contentItem?.menuIndicatorButton?.hovered
    }

    Keys.onMenuPressed: root.clickMenuIndicator()

    contentItem: RowLayout {
        property alias menuIndicatorButton: menuIndicator

        property alias label: label

        spacing: 0

        Item {
            Layout.fillWidth: true
        }

        ButtonExt.DefaultContentIconLabel {
            id: iconLabel

            control: root

            topPadding: -VLCStyle.margin_xxsmall
            bottomPadding: topPadding

            Layout.fillWidth: !label.visible
            Layout.fillHeight: true
            Layout.rightMargin: label.visible ? root.spacing : 0
        }

        ButtonExt.DefaultContentLabel {
            id: label

            control: root

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.maximumWidth: implicitWidth + 1
        }

        Item {
            Layout.fillWidth: true
        }
    }

    indicator: Row {
        // It is intentional that we use content item in visible, but background item for anchoring:
        visible: !!root.contentItem?.visible

        anchors.right: root.background?.right ?? undefined
        anchors.top: root.background?.top ?? undefined
        anchors.bottom: root.background?.bottom ?? undefined

        Rectangle {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: root.background?.border.width ?? 0
            anchors.bottomMargin: anchors.topMargin

            width: MainCtx.dp(1, VLCStyle.scale)

            color: menuIndicator.visualFocus ? "transparent" : Qt.alpha(root.colorContext.fg.primary, 0.4)
        }

        T.AbstractButton {
            id: menuIndicator

            anchors.top: parent.top
            anchors.bottom: parent.bottom

            implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                                    implicitContentWidth + leftPadding + rightPadding)
            implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                                     implicitContentHeight + topPadding + bottomPadding)

            padding: VLCStyle.margin_xsmall

            topInset: (background.border.width > 0 && background.border.color.a > 0) ? 0
                                                                                     : (root.background?.border.width ?? 0)
            bottomInset: topInset
            leftInset: 0
            rightInset: topInset

            Accessible.name: qsTr("More options")

            T.ToolTip.text: menuIndicator.Accessible.name
            T.ToolTip.delay: VLCStyle.delayToolTipAppear
            T.ToolTip.visible: menuIndicator.hovered || menuIndicator.visualFocus

            Navigation.parentItem: root

            Keys.priority: Keys.AfterItem
            Keys.onPressed: (event) => {
                if (KeyHelper.matchLeft(event)) {
                    root.forceActiveFocus(Qt.BacktabFocusReason)
                    event.accepted = true
                    return
                }

                Navigation.defaultKeyAction(event)
            }

            onClicked: {
                root.popupMenu()
                root.menuIndicatorClicked()
            }

            contentItem: Widgets.IconLabel {
                text: VLCIcons.chevron_down
                font.pixelSize: VLCStyle.icon_small
                color: root.color
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Widgets.AnimatedBackground {
                implicitWidth: VLCStyle.icon_small
                implicitHeight: VLCStyle.icon_small

                enabled: menuTheme.initialized
                color: (menuTheme.hovered || menuTheme.pressed) ? menuTheme.bg.primary
                                                                : Qt.alpha(menuTheme.bg.primary, 0.0)
                border.width: root.background?.border.width ?? 0
                border.color: menuIndicator.visualFocus ? menuTheme.visualFocus
                                                        : Qt.alpha(menuTheme.visualFocus, 0.0)
                radius: root.radius - menuIndicator.topInset
            }

            ColorContext {
                id: menuTheme

                colorSet: ColorContext.ButtonAccent

                enabled: root.enabled
                focused: menuIndicator.visualFocus
                hovered: _menu.visible || menuIndicator.hovered
                pressed: menuIndicator.down
            }
        }
    }

    StringListMenu {
        id: _menu

        ctx: MainCtx

        onSelected: (index, text) => root.menuSelected(index, text)
    }

    function clickMenuIndicator() {
        root.contentItem?.menuIndicatorButton?.click()
    }

    function popupMenu() {
        _menu.popup(root.mapToGlobal(0, root.height), root.model)
    }
}
