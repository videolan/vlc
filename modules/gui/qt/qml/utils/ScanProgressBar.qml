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

import "qrc:///style/"

ProgressBar {
    visible: !medialib.idle

    id: progressBar_id
    from: 0
    to: 100
    height: progressText_id.height
    anchors.topMargin: 10
    anchors.bottomMargin: 10
    value: medialib.parsingProgress
    indeterminate: medialib.discoveryPending
    Text {
        id: progressText_id
        color: VLCStyle.colors.text
        text:  medialib.discoveryPending ? medialib.discoveryEntryPoint : (medialib.parsingProgress + "%")
        z: progressBar_id.z + 1
        anchors.horizontalCenter: parent.horizontalCenter
        visible: true
    }
}
