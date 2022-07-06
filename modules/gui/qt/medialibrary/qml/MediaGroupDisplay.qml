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
import "qrc:///util/" as Util
import "qrc:///style/"

VideoAll {
    id: root

    // Properties

    // NOTE: We need 'var' for properties altered by StackView.replace().
    property var    initialId
    property string initialTitle

    // Aliases

    // NOTE: This is used to determine which media(s) shall be displayed.
    property alias parentId: modelVideo.parentId

    // NOTE: The title of the group.
    property string title: initialTitle

    // Children

    model: MLVideoModel {
        id: modelVideo

        ml: MediaLib

        parentId: initialId
    }

    contextMenu: Util.MLContextMenu { model: modelVideo; showPlayAsAudioAction: true }

    header: Column {
        width: root.width

        topPadding: VLCStyle.margin_normal
        bottomPadding: VLCStyle.margin_normal

        Widgets.SubtitleLabel {
            anchors.left: parent.left
            anchors.right: parent.right

            // NOTE: We want this to be properly aligned with the grid items.
            anchors.leftMargin: contentMargin + VLCStyle.margin_normal

            text: root.title
        }
    }
}
