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
import QtQuick.Templates as T
import QtQuick.Layouts


import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets

FocusScope {
    id: root

    implicitWidth: iconButton.implicitWidth
    implicitHeight: iconButton.implicitHeight

    property alias popup: popup
    property real maxSearchFieldWidth: Number.MAX_VALUE
    property alias buttonWidth: iconButton.implicitWidth
    property string searchPattern
    property alias compressSearchPatternChanges: searchPatternBinding.delayed
    compressSearchPatternChanges: true

    Binding on searchPattern {
        id: searchPatternBinding
        value: textField.text
    }

    property bool popBelow: true
    property alias toggleButton: iconButton
    property alias textField: textField

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
                    target: popup
                    explicit: true
                    height: popup.implicitHeight
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
                }

                PropertyChanges {
                    target: popup
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

            onRunningChanged: {
                if (running)
                    textField.clip = true
                else
                    textField.clip = false
            }

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
        Navigation.downItem: root.popBelow ? textField : null
        Navigation.upItem: root.popBelow ? null : textField

        onClicked: {
            if (expandedState.state == "")
                expandAndFocus()
            else
                expandedState.state = ""
        }

        T.Popup {
            id: popup

            x: -width + parent.width
            y: root.popBelow ? parent.height : -height

            implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                                    implicitContentWidth + leftPadding + rightPadding)
            implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                                     implicitContentHeight + topPadding + bottomPadding)

            closePolicy: Popup.NoAutoClose

            height: 0
            visible: (height > 0)

            onOpened: {
                textField.forceActiveFocus(iconButton.focusReason)
            }

            onClosed: {
                textField.focus = false
                focus = false
                iconButton.focus = true
            }

            contentItem: TextFieldExt {
                id: textField

                property bool _keyPressed: false

                colorContext.palette: theme.palette

                implicitWidth: VLCStyle.widthSearchInput

                padding: VLCStyle.dp(6)
                leftPadding: padding + VLCStyle.dp(4)
                rightPadding: (textField.width - clearButton.x)

                radius: clearButton.radius

                selectByMouse: true

                placeholderText: qsTr("filter")

                Navigation.parentItem: root
                Navigation.upItem: root.popBelow ? iconButton : null 
                Navigation.downItem: root.popBelow ? null : iconButton
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
                        if (activeFocus && !visible && parent.visible) {
                            parent.focus = true
                            parent.focusReason = focusReason
                        }
                    }
                    onClicked: textField.clear()

                    Navigation.parentItem: textField
                    Navigation.leftItem: textField
                }
            }
        }
    }
}
