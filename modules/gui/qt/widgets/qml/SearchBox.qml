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
import QtQuick.Layouts 1.11

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

FocusScope {
    id: root

    implicitWidth: content.implicitWidth
    implicitHeight: content.implicitHeight

    property alias buttonWidth: iconButton.implicitWidth
    property alias searchPattern: textField.text

    property bool _widthOverridden: false
    onWidthChanged: {
        // Proper way of this would be checking if width is bound to implicitWidth
        if (width === implicitWidth)
            _widthOverridden = false
        else
            _widthOverridden = true
    }

    states: State {
        name: "expanded"

        PropertyChanges {
            target: textField

            focus: true
            // Take care if SearchBox was set a specific width:
            width: (root._widthOverridden ? (content.width - iconButton.width) : textField.implicitWidth)
        }

        PropertyChanges {
            target: iconButton

            highlighted: true
        }
    }

    transitions: Transition {
        from: ""; to: "expanded"
        reversible: true

        SequentialAnimation {
            NumberAnimation { property: "width"; easing.type: Easing.InOutSine; duration: VLCStyle.duration_long; }
            PropertyAction { property: "highlighted" }
            PropertyAction { property: "focus" }
        }
    }

    Row {
        id: content
        anchors.fill: parent

        layoutDirection: Qt.RightToLeft // anchor iconButton to right

        Widgets.IconToolButton {
            id: iconButton

            anchors.top: parent.top
            anchors.bottom: parent.bottom

            size: VLCStyle.icon_banner

            iconText: VLCIcons.search
            text: I18n.qtr("Filter")

            focus: root.state === ""

            Navigation.parentItem: root
            Navigation.leftItem: textField

            onClicked: {
                textField.clear()
                root.state = (root.state === "") ? "expanded" : ""
            }
        }

        TextField {
            id: textField

            property bool _keyPressed: false

            anchors.top: parent.top
            anchors.bottom: parent.bottom

            implicitWidth: VLCStyle.widthSearchInput
            width: 0

            visible: (width > 0)

            padding: VLCStyle.dp(6)
            leftPadding: padding + VLCStyle.dp(4)
            rightPadding: (textField.width - clearButton.x)

            font.pixelSize: VLCStyle.fontSize_normal

            palette.text: VLCStyle.colors.buttonText
            palette.highlightedText: VLCStyle.colors.bgHoverText
            palette.base: VLCStyle.colors.button
            palette.highlight: VLCStyle.colors.accent
            palette.mid: VLCStyle.colors.buttonBorder

            selectByMouse: true

            placeholderText: I18n.qtr("filter")

            Navigation.parentItem: root
            Navigation.rightItem: clearButton.visible ? clearButton : iconButton
            Navigation.cancelAction: function() { root.state = "" }

            //ideally we should use Keys.onShortcutOverride but it doesn't
            //work with TextField before 5.13 see QTBUG-68711
            onActiveFocusChanged: {
                if (activeFocus)
                    MainCtx.useGlobalShortcuts = false
                else
                    MainCtx.useGlobalShortcuts = true
            }

            Keys.priority: Keys.AfterItem

            Keys.onPressed: {
                _keyPressed = true

                //we don't want Navigation.cancelAction to match Backspace
                if (event.matches(StandardKey.Backspace))
                    event.accepted = true

                Navigation.defaultKeyAction(event)
            }

            Keys.onReleased: {
                if (_keyPressed === false)
                    return

                _keyPressed = false

                //we don't want Navigation.cancelAction to match Backspace
                if (event.matches(StandardKey.Backspace))
                    event.accepted = true

                Navigation.defaultKeyReleaseAction(event)
            }

            Component.onCompleted: {
                background.border.width = Qt.binding(function() { return root.activeFocus ? VLCStyle.dp(2)
                                                                                          : VLCStyle.dp(1) })

            }

            Widgets.IconToolButton {
                id: clearButton

                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: VLCStyle.margin_xxsmall

                size: VLCStyle.icon_banner
                iconText: VLCIcons.close

                visible: (textField.text.length > 0)

                onVisibleChanged: {
                    if (!visible && parent.visible) {
                        parent.focus = true
                    }
                }
                onClicked: textField.clear()

                Navigation.parentItem: textField
                Navigation.leftItem: textField
                Navigation.rightItem: iconButton
            }
        }
    }
}
