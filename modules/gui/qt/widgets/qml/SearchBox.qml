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
import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

FocusScope {
    id: root

    implicitWidth: content.implicitWidth

    property alias buttonWidth: icon.width
    property alias searchPattern: searchBox.text

    property bool _expanded: false

    function reqExpand() {
        if (visible)
            _expanded = true
    }

    on_ExpandedChanged: {
        if (_expanded) {
            searchBox.forceActiveFocus(Qt.ShortcutFocusReason)
            animateExpand.start()
        } else {
            searchBox.clear()
            icon.focus = true
            searchBox.focus = false
            animateRetract.start()
        }
    }

    onActiveFocusChanged: {
        if (!activeFocus && searchBox.text == "") {
            _expanded = false
        }
    }

    SmoothedAnimation {
        id: animateExpand;
        target: searchBoxRect;
        properties: "width"
        duration: VLCStyle.duration_faster
        to: VLCStyle.widthSearchInput
        easing.type: Easing.InSine
    }

    SmoothedAnimation {
        id: animateRetract;
        target: searchBoxRect;
        properties: "width"
        duration: VLCStyle.duration_faster
        to: 0
        easing.type: Easing.OutSine
    }


    Row {
        id: content

        Widgets.IconToolButton {
            id: icon

            size: VLCStyle.banner_icon_size
            iconText: VLCIcons.search
            text: i18n.qtr("Filter")
            height: root.height

            focus: true

            onClicked: {
                searchBox.clear()
                _expanded = !_expanded
            }

            Navigation.parentItem: root
            Navigation.rightItem: _expanded ? searchBox : nullptr
            Keys.priority: Keys.AfterItem
            Keys.onPressed: Navigation.defaultKeyAction(event)
        }

        Rectangle {
            id: searchBoxRect
            color: VLCStyle.colors.button

            anchors.verticalCenter: parent.verticalCenter

            width: 0
            implicitHeight: searchBox.height

            border.color: searchBox.activeFocus ? VLCStyle.colors.accent : VLCStyle.colors.buttonBorder

            TextField {
                id: searchBox

                enabled: root._expanded

                anchors.fill: searchBoxRect
                anchors.rightMargin: clearButton.visible ? (VLCStyle.margin_xxsmall + clearButton.width) : 0

                font.pixelSize: VLCStyle.fontSize_normal

                palette.text: VLCStyle.colors.buttonText
                palette.highlight: VLCStyle.colors.bgHover
                palette.highlightedText: VLCStyle.colors.bgHoverText

                selectByMouse: true

                placeholderText: i18n.qtr("filter")

                background: Rectangle { color: "transparent" }

                Navigation.parentItem: root
                Navigation.leftItem: icon
                Navigation.rightItem: clearButton.visible ? clearButton : null
                Navigation.cancelAction: function() { _expanded = false }
                Keys.priority: Keys.AfterItem
                Keys.onPressed: {
                    //we don't want Navigation.cancelAction to match Backspace
                    if (event.matches(StandardKey.Backspace)) {
                        return
                    }
                    Navigation.defaultKeyAction(event)
                }

                Keys.onReleased: {
                    //we don't want Navigation.cancelAction to match Backspace
                    if (event.matches(StandardKey.Backspace)) {
                        return
                    }
                    Navigation.defaultKeyReleaseAction(event)
                }
            }

            Widgets.IconToolButton {
                id: clearButton

                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: VLCStyle.margin_xxsmall

                size: VLCStyle.icon_small
                iconText: VLCIcons.close

                visible: ( _expanded && searchBox.text.length > 0 )
                enabled: visible

                onClicked: {
                    searchBox.clear()
                }

                Navigation.parentItem: root
                Navigation.leftItem: searchBox
                Keys.priority: Keys.AfterItem
                Keys.onPressed: Navigation.defaultKeyAction(event)
            }
        }
    }
}
