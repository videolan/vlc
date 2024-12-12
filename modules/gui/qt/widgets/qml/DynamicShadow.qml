/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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
import QtQuick.Effects

MultiEffect {
    id: effect

    implicitWidth: sourceItem ? Math.min(sourceItem.paintedWidth ?? Number.MAX_VALUE, sourceItem.width) : 0
    implicitHeight: sourceItem ? Math.min(sourceItem.paintedHeight ?? Number.MAX_VALUE, sourceItem.height) : 0

    shadowEnabled: true
    shadowBlur: 1.0

    paddingRect: Qt.rect(xOffset, yOffset, 0, 0)

    visible: (GraphicsInfo.shaderType === GraphicsInfo.RhiShader)

    property alias sourceItem: effect.source

    property alias xOffset: effect.shadowHorizontalOffset
    property alias yOffset: effect.shadowVerticalOffset

    property alias blurRadius: effect.blurMax

    property alias color: effect.shadowColor

    Binding {
        // Source item should be made invisible
        // since multi effect already renders
        // the source item.
        when: sourceItem && effect.visible
        target: sourceItem
        property: "visible"
        value: false
    }
}
