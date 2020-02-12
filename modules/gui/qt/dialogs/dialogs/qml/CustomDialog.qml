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

import QtQuick 2.11
import QtQuick.Layouts 1.3
import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

ModalDialog {
    id: root

    property alias text: content.text

    property alias cancelTxt: cancelBtn.text
    property alias okTxt: okBtn.text

    property var _acceptCb: undefined
    property var _rejectCb: undefined

    function ask(text, acceptCb, rejectCb, buttons) {
        //TODO: use a Promise here when dropping support of Qt 5.11
        var okTxt = i18n.qtr("OK")
        var cancelTxt = i18n.qtr("cancel")
        if (buttons) {
            if (buttons.cancel) {
                cancelTxt = buttons.cancel
            }
            if (buttons.ok) {
                okTxt = buttons.ok
            }
        }
        root.cancelTxt = cancelTxt
        root.okTxt = okTxt
        root.text = text
        root._acceptCb = acceptCb
        root._rejectCb = rejectCb
        root.open()
    }

    onAccepted: {
        if (_acceptCb)
            _acceptCb()
    }

    onRejected: {
        if (_rejectCb)
            _rejectCb()
    }

    contentItem: Text {
        id: content
        focus: false
        font.pixelSize: VLCStyle.fontSize_normal
        color: VLCStyle.colors.text
        wrapMode: Text.WordWrap
    }

    footer: FocusScope {
        focus: true
        id: questionButtons
        implicitHeight: VLCStyle.icon_normal

        Rectangle {
            color: VLCStyle.colors.banner
            anchors.fill: parent
            anchors.leftMargin: VLCStyle.margin_xxsmall
            anchors.rightMargin: VLCStyle.margin_xxsmall

            RowLayout {
                anchors.fill: parent

                Widgets.TextToolButton {
                    id: cancelBtn
                    Layout.fillWidth: true
                    focus: true
                    visible: cancelBtn.text !== ""
                    KeyNavigation.right: okBtn
                    onClicked: root.reject()
                }

                Widgets.TextToolButton {
                    id: okBtn
                    Layout.fillWidth: true
                    visible: okBtn.text !== ""
                    onClicked: root.accept()
                }
            }
        }
    }
}
