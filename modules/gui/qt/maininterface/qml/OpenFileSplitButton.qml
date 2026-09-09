/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

import VLC.Widgets as Widgets
import VLC.Dialogs

Widgets.SplitButton {
    id: root

    text: qsTr("Open File")

    model: [
        qsTr("Open Multiple Files"),
        qsTr("Open Directory"),
        qsTr("Open Disc"),
        qsTr("Open Network Stream"),
        qsTr("Open Capture Device"),
        qsTr("Open Location from clipboard"),
    ]

    onClicked: DialogsProvider.simpleOpenDialog()

    onMenuSelected: (index) => {
        switch (index) {
        case 0:
            DialogsProvider.openFileDialog()
            break
        case 1:
            DialogsProvider.PLOpenDir()
            break
        case 2:
            DialogsProvider.openDiscDialog()
            break
        case 3:
            DialogsProvider.openNetDialog()
            break
        case 4:
            DialogsProvider.openCaptureDialog()
            break
        case 5:
            DialogsProvider.openUrlDialog()
            break
        default:
            break
        }
    }
}
