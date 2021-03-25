/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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

import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

VideoAll {
    id: root

    //---------------------------------------------------------------------------------------------
    // Properties
    //---------------------------------------------------------------------------------------------

    property int    initialIndex: 0
    property var    initialId
    property string initialName

    //---------------------------------------------------------------------------------------------
    // Aliases
    //---------------------------------------------------------------------------------------------

    // NOTE: This is used to determine which media(s) shall be displayed.
    property alias parentId: modelGroup.parentId

    // NOTE: The name of the group.
    property string name

    //---------------------------------------------------------------------------------------------
    // Childs
    //---------------------------------------------------------------------------------------------

    model: MLVideoModel {
        id: modelGroup

        ml: medialib

        parentId: initialId
    }

    header: Column {
        topPadding: VLCStyle.margin_normal

        Widgets.SubtitleLabel {
            width: root.width

            leftPadding  : VLCStyle.margin_xlarge
            bottomPadding: VLCStyle.margin_xsmall

            text: root.name
        }
    }
}
