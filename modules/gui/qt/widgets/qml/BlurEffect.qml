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

    implicitWidth: source ? Math.min(source.paintedWidth ?? Number.MAX_VALUE, source.width) : 0
    implicitHeight: source ? Math.min(source.paintedHeight ?? Number.MAX_VALUE, source.height) : 0

    blurEnabled: true
    blur: 1.0

    // Avoid using padding, as Qt thinks that it needs to layering implicitly.
    autoPaddingEnabled: false

    property alias radius: effect.blurMax

    onChildrenChanged: {
        for (let i in children) {
            if (children[i] instanceof ShaderEffect) {
                // Qt creates multiple ShaderEffect, depending
                // on parameters. They support atlas/sub textures
                // but Qt does not set `supportsAtlasTextures`:
                children[i].supportsAtlasTextures = true
            }
        }
    }
}
