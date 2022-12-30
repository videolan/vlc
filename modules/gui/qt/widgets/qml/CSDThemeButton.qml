/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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
import QtQuick.Layouts 1.11
import QtQuick.Window 2.11

import org.videolan.vlc 0.1

import "qrc:///style/"

T.AbstractButton {
    id: control

    padding: 0
    implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    focusPolicy: Qt.NoFocus

    property int buttonType: CSDThemeImage.CLOSE
    property int bannerHeight: -1
    property bool showBg: false

    background: null

    contentItem: CSDThemeImage {

        anchors.verticalCenter: parent.verticalCenter

        bannerHeight: control.bannerHeight

        theme: VLCStyle.palette

        windowMaximized: MainCtx.intfMainWindow.visibility === Window.Maximized
        windowActive: MainCtx.intfMainWindow.active

        buttonType: control.buttonType
        buttonState: {
            if (control.pressed)
                return CSDThemeImage.PRESSED
            else if (control.hovered)
                return CSDThemeImage.HOVERED
            else
                return CSDThemeImage.NORMAL
        }
    }
}
