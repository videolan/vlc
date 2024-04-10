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
import QtQuick
import QtQuick.Window
import QtQuick.Layouts

import org.videolan.vlc 0.1

import "qrc:///style/"

Rectangle {
    id: root

    readonly property bool buttonHovered: {
        for (let i = 0; i < loader.count; ++i) {
            const button = loader.itemAt(i)
            if (button.hovered) return true
        }
        return false
    }

    readonly property int _frameMarginLeft: VLCStyle.palette.csdMetrics?.csdFrameMarginLeft ?? 0
    readonly property int _frameMarginRight: VLCStyle.palette.csdMetrics?.csdFrameMarginRight ?? 0
    readonly property int _interNavButtonSpacing: VLCStyle.palette.csdMetrics?.interNavButtonSpacing ?? 0

    implicitWidth: layout.implicitWidth + _frameMarginLeft + _frameMarginRight
    implicitHeight: layout.implicitHeight

    color: "transparent"

    Row {
        id: layout

        anchors.fill: parent
        anchors.leftMargin: root._frameMarginLeft
        anchors.rightMargin: root._frameMarginRight

        spacing: root._interNavButtonSpacing

        Repeater {
            id: loader

            model: MainCtx.csdButtonModel.windowCSDButtons

            CSDThemeButton {

                anchors.verticalCenter: parent.verticalCenter

                bannerHeight: root.height

                buttonType: {
                    switch (modelData.type) {
                    case CSDButton.Minimize:
                        return CSDThemeImage.MINIMIZE

                    case CSDButton.MaximizeRestore:
                        return (MainCtx.intfMainWindow.visibility === Window.Maximized)
                                ? CSDThemeImage.RESTORE
                                : CSDThemeImage.MAXIMIZE

                    case CSDButton.Close:
                        return CSDThemeImage.CLOSE
                    }

                    console.assert(false, "unreachable")
                }

                onClicked: modelData.click()
            }
        }
    }
}
