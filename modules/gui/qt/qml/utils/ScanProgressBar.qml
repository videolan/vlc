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
    property int progressPercent: 0
    property bool discoveryDone: true

    Connections {
        target: medialib
        onProgressUpdated: {
            progressPercent = percent;
            if (discoveryDone)
                progressText_id.text = percent + "%";
        }
        onDiscoveryProgress: {
            progressText_id.text = entryPoint;
        }
        onDiscoveryStarted: discoveryDone = false
        onDiscoveryCompleted: discoveryDone = true
    }

    visible: (progressPercent < 100) && (progressPercent != 0)
    id: progressBar_id
    from: 0
    to: 100
    height: progressText_id.height
    anchors.topMargin: 10
    anchors.bottomMargin: 10
    value: progressPercent
    Text {
        id: progressText_id
        color: VLCStyle.colors.text
        anchors.horizontalCenter: parent.horizontalCenter
    }
}
