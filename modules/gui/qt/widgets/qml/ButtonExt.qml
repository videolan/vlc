/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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
import QtQuick.Templates as T
import QtQuick.Layouts


import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style

T.Button {
    id: control

    // Properties

    property bool showText: (text.length > 0)

    property bool selected: false // WARNING: This property is deprecated. Use `checked` instead.
    checked: selected

    property bool busy: false

    property string iconTxt: ""

    property int iconSize: VLCStyle.icon_normal

    property color color: theme.fg.primary
    property color colorFocus: theme.visualFocus

    //set to true when user animates the background manually
    property bool extBackgroundAnimation: false

    // Aliases
    property real iconRotation

    // Settings

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: VLCStyle.margin_xsmall
    spacing: VLCStyle.margin_xsmall

    font.pixelSize: VLCStyle.fontSize_normal

    // Keys

    Keys.priority: Keys.AfterItem

    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)


    // Accessible

    Accessible.onPressAction: control.clicked()

    // Tooltip

    T.ToolTip.visible: (T.ToolTip.text && (!showText || label.implicitWidth > label.width) && (hovered || visualFocus))

    T.ToolTip.delay: VLCStyle.delayToolTipAppear

    T.ToolTip.text: text

    // Childs


    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ButtonStandard

        focused: control.visualFocus
        hovered: control.hovered
        enabled: control.enabled
        pressed: control.down
    }

    background: Widgets.AnimatedBackground {
        enabled: theme.initialized && !control.extBackgroundAnimation

        color: theme.bg.primary
        border.color: control.visualFocus ? control.colorFocus
                                          : (theme.border.a > 0.0 ? theme.border : color)

        Rectangle {
            anchors {
                bottom: parent.bottom
                bottomMargin: VLCStyle.margin_xxxsmall
                horizontalCenter: parent.horizontalCenter
            }

            implicitWidth: (parent.width - VLCStyle.margin_xsmall)
            implicitHeight: VLCStyle.heightBar_xxxsmall

            width: control.contentItem?.implicitWidth ?? implicitWidth

            visible: (width > 0 && control.checked)
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

            rotation: control.iconRotation

            text: control.iconTxt

            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            color: Qt.alpha(control.color, control.busy ? 0.0 : 1.0)

            font.pixelSize: control.iconSize

            Layout.fillWidth: !label.visible
            Layout.fillHeight: true

            // FIXME: use `BusyIndicatorExt` when it is ready (!7180)
            BusyIndicator {
                anchors.fill: parent

                padding: 0

                running: control.busy

                palette.text: control.color
                palette.dark: control.color
            }
        }

        T.Label {
            id: label

            visible: control.showText

            text: control.text

            verticalAlignment: Text.AlignVCenter

            color: control.color

            elide: Text.ElideRight

            font.pixelSize: VLCStyle.fontSize_normal
            font.weight: Font.DemiBold

            textFormat: Text.PlainText

            //button text is already exposed
            Accessible.ignored: true

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.maximumWidth: implicitWidth + 1
            Layout.leftMargin: iconLabel.visible ? VLCStyle.margin_xsmall : 0
        }

        Item {
            Layout.fillWidth: true
        }
    }
}
