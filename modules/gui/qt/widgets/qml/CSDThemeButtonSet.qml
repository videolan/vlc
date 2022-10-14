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
import QtQuick.Window 2.11
import QtQuick.Layouts 1.11

import org.videolan.vlc 0.1

import "qrc:///style/"

Rectangle {
    id: root

    readonly property bool hovered: minimizeButton.hovered || maximizeButton.hovered || closeButton.hovered

    readonly property int _frameMarginLeft: VLCStyle.theme.csdMetrics ? VLCStyle.theme.csdMetrics.csdFrameMarginLeft : 0
    readonly property int _frameMarginRight: VLCStyle.theme.csdMetrics ? VLCStyle.theme.csdMetrics.csdFrameMarginRight : 0
    readonly property int _interNavButtonSpacing: VLCStyle.theme.csdMetrics ? VLCStyle.theme.csdMetrics.interNavButtonSpacing : 0

    implicitWidth: layout.implicitWidth + _frameMarginLeft + _frameMarginRight
    implicitHeight: layout.implicitHeight

    color: "transparent"

    Row {
        id: layout

        anchors.fill: parent
        anchors.leftMargin: root._frameMarginLeft
        anchors.rightMargin: root._frameMarginRight

        spacing: root._interNavButtonSpacing

        CSDThemeButton {
            id: minimizeButton

            anchors.verticalCenter: parent.verticalCenter

            bannerHeight: root.height

            buttonType: CSDThemeImage.MINIMIZE

            onClicked: MainCtx.requestInterfaceMinimized()
        }

        CSDThemeButton {
            id: maximizeButton

            anchors.verticalCenter: parent.verticalCenter

            bannerHeight: root.height

            buttonType: (MainCtx.intfMainWindow.visibility === Window.Maximized)  ? CSDThemeImage.RESTORE : CSDThemeImage.MAXIMIZE

            onClicked: {
                if (MainCtx.intfMainWindow.visibility === Window.Maximized) {
                    MainCtx.requestInterfaceNormal()
                } else {
                    MainCtx.requestInterfaceMaximized()
                }
            }
        }

        CSDThemeButton {
            id: closeButton

            anchors.verticalCenter: parent.verticalCenter

            bannerHeight: root.height

            buttonType: CSDThemeImage.CLOSE

            onClicked: MainCtx.intfMainWindow.close()
        }
    }

}
