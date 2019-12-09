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
import QtQuick.Templates 2.4 as T
import QtQuick.Layouts 1.3
import QtQml.Models 2.11

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

T.Menu {
    id: control

    property var parentMenu: undefined
    property bool _emitMenuClose: true
    signal menuClosed()

    font.pixelSize: VLCStyle.fontSize_normal

    topPadding: VLCStyle.applicationVerticalMargin
    bottomPadding: VLCStyle.applicationVerticalMargin

    modal: true
    cascade: false

    //don't CloseOnEscape, otherwise the event is processed before our key handlers
    closePolicy: Popup.CloseOnPressOutside

    x:0
    y:0
    width: parent.width / 4
    height: parent.height
    z: 1

    delegate: PlayerMenuItem {
        parentMenu: control
    }

    onOpened: {
        control._emitMenuClose = true

        for (var i = 0; i < control.count; i++) {
            if (control.itemAt(i).enabled) {
                control.currentIndex = i
                break
            }
        }
    }

    onClosed: {
        if (control._emitMenuClose) {
            menuClosed()
        }
    }

    contentItem: ListView {
        header: Label {
            leftPadding: VLCStyle.applicationHorizontalMargin
            text: control.title
            color: VLCStyle.colors.playerFg
            font.bold: true
            font.pixelSize: VLCStyle.fontSize_xlarge
            padding: VLCStyle.margin_xxsmall
        }

        keyNavigationEnabled: false

        implicitWidth: contentWidth
        implicitHeight: contentHeight
        model: control.contentModel
        clip: true
        currentIndex: control.currentIndex

        ScrollIndicator.vertical: ScrollIndicator {}
    }

    background: Rectangle {
        color: VLCStyle.colors.playerBg
        opacity: 0.8
    }

    T.Overlay.modal: Rectangle {
        color: "transparent"
    }

}

