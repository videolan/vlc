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


import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets

FocusScope {
    id: root

    implicitWidth: iconButton.implicitWidth
    implicitHeight: iconButton.implicitHeight

    property real maxSearchFieldWidth: Number.MAX_VALUE
    property alias buttonWidth: iconButton.implicitWidth
    property alias searchPattern: textField.text

    // public functions

    function expandAndFocus() {
        expandedState.state = "expanded"
        textField.forceActiveFocus(Qt.ShortcutFocusReason)
    }

    StateGroup {
        id: expandedState

        state: ""

        states: [
            State {
                name: "expanded"

                PropertyChanges {
                    target: textField
                    height:  textField.implicitHeight
                }

                PropertyChanges {
                    target: iconButton
                    checked: true
                }
            },
            State {
                name: ""

                PropertyChanges {
                    target: textField
                    text: ""
                    height: 0.0
                }

                PropertyChanges {
                    target: iconButton
                    focus: true
                    checked: false
                }
            }
        ]

        transitions: Transition {
            from: ""; to: "expanded"
            reversible: true

            NumberAnimation { property: "height"; easing.type: Easing.InOutSine; duration: VLCStyle.duration_long; }
        }
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
    }

    Widgets.IconToolButton {
        id: iconButton

        anchors.fill: parent

        font.pixelSize: VLCStyle.icon_banner

        text: VLCIcons.search
        description: qsTr("Filter")

        focus: true

        Navigation.parentItem: root
        Navigation.downItem: textField

        onClicked: {
            if (expandedState.state == "")
                expandAndFocus()
            else
                expandedState.state = ""
        }
    }

    TextFieldExt {
        id: textField

        property bool _keyPressed: false

        anchors.top: iconButton.bottom
        anchors.right: iconButton.right

        implicitWidth: VLCStyle.widthSearchInput
        height: 0

        visible: (height > 0)

        padding: VLCStyle.dp(6)
        leftPadding: padding + VLCStyle.dp(4)
        rightPadding: (textField.width - clearButton.x)

        radius: clearButton.radius

        selectByMouse: true

        placeholderText: qsTr("filter")

        Navigation.parentItem: root
        Navigation.upItem: iconButton
        Navigation.rightItem: clearButton.visible ? clearButton : null
        Navigation.cancelAction: function() {
            expandedState.state = ""
            iconButton.focusReason = Qt.ShortcutFocusReason
        }

        Accessible.searchEdit: true

        //ideally we should use Keys.onShortcutOverride but it doesn't
        //work with TextField before 5.13 see QTBUG-68711
        onActiveFocusChanged: {
            if (activeFocus)
                MainCtx.useGlobalShortcuts = false
            else
                MainCtx.useGlobalShortcuts = true
        }

        Keys.priority: Keys.AfterItem

        Keys.onPressed: (event) => {
            _keyPressed = true

            //we don't want Navigation.cancelAction to match Backspace
            if (event.matches(StandardKey.Backspace))
                event.accepted = true

            Navigation.defaultKeyAction(event)
        }

        Keys.onReleased: (event) => {
            if (_keyPressed === false)
                return

            _keyPressed = false

            //we don't want Navigation.cancelAction to match Backspace
            if (event.matches(StandardKey.Backspace))
                event.accepted = true

            Navigation.defaultKeyReleaseAction(event)
        }

        Widgets.IconToolButton {
            id: clearButton

            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: VLCStyle.margin_xxsmall

            font.pixelSize: VLCStyle.icon_banner
            text: VLCIcons.close

            description: qsTr("Clear")

            visible: (textField.text.length > 0)

            onVisibleChanged: {
                if (!visible && parent.visible) {
                    parent.focus = true
                }
            }
            onClicked: textField.clear()

            Navigation.parentItem: textField
            Navigation.leftItem: textField
        }
    }

}
