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
import QtQuick.Window
import QtQuick.Controls


import VLC.MainInterface
import VLC.Player
import VLC.Widgets as Widgets
import VLC.Style


Control {
    id: highResolutionTimeWidget

    property bool paintOnly: false

    padding: VLCStyle.focus_border

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

    Accessible.role: Accessible.Indicator
    Accessible.name: contentItem.text

    function _adjustSMPTETimer(add) {
        if (typeof toolbarEditor !== "undefined") // FIXME: Can't use paintOnly because it is set later
            return

        if (add === true)
            Player.requestAddSMPTETimer()
        else if (add === false)
            Player.requestRemoveSMPTETimer()
    }

    Component.onCompleted: {
        _adjustSMPTETimer(true)
    }

    Component.onDestruction: {
        _adjustSMPTETimer(false)
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ToolButton

        focused: highResolutionTimeWidget.visualFocus
    }

    background: Widgets.AnimatedBackground {
        enabled: theme.initialized
        border.color: visualFocus ? theme.visualFocus : "transparent"
    }

    contentItem: Item {
        implicitHeight: smpteTimecodeMetrics.height
        implicitWidth: smpteTimecodeMetrics.width

        property alias text: label.text

        Widgets.MenuLabel {
            id: label
            anchors.fill: parent

            text: paintOnly ? "00:00:00:00" : timeText
            color: theme.fg.primary

            horizontalAlignment: Text.AlignHCenter

            Accessible.ignored: true

            property string timeText

            Connections {
                target: label.Window.window
                enabled: label.visible

                function onAfterAnimating() {
                    // Sampling point
                    // Emitted from the GUI thread

                    // Constantly update the label, so that the window
                    // prepares new frames as we don't know when the
                    // timecode changes. This is similar to animations:
                    label.update()

                    if (label.timeText === Player.highResolutionTime)
                        return

                    label.timeText = Player.highResolutionTime

                    // Text would like polishing after text change.
                    // We need this because `afterAnimating()` is
                    // signalled after the items are polished:
                    if (label.ensurePolished)
                        label.ensurePolished()
                }
            }
        }

        TextMetrics {
            id: smpteTimecodeMetrics
            font: label.font
            text: "-00:00:00:00-"
        }
    }
}
