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

import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T

import VLC.Widgets
import VLC.MediaLibrary
import VLC.Style

ProgressBarExt {
    id: control

    value: MediaLib.parsingProgress

    indeterminate: MediaLib.discoveryPending

    SubtitleLabel {
        parent: control.contentItem

        anchors.left: parent.left
        anchors.right: parent.right

        text: (MediaLib.discoveryPending) ? qsTr("Scanning %1")
                                            .arg(MediaLib.discoveryEntryPoint)
                                          : qsTr("Indexing Medias (%1%)")
                                            .arg(MediaLib.parsingProgress)

        elide: Text.ElideMiddle

        font.pixelSize: VLCStyle.fontSize_large
        font.weight: Font.Normal
        color: theme.fg.primary
    }
}
