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
import VLC.Style
import VLC.Dialogs
import VLC.Util

Widgets.SplitButton {
    id: root

    text: qsTr("Open File")

    iconTxt: VLCIcons.ic_fluent_folder_open_24_regular
    iconSize: VLCStyle.fontSize_xxlarge

    model: [
        { text: qsTr("Open Multiple Files"), icon_svg: ":/menu/file.svg" },
        { text: qsTr("Open Directory"), icon_svg: ":/menu/folder.svg" },
        { text: qsTr("Open Disc"), icon_svg: ":/menu/disc.svg" },
        { text: qsTr("Open Network Stream"), icon_svg: ":/menu/network.svg" },
        { text: qsTr("Open Capture Device"), icon_svg: ":/menu/capture-card.svg" },
        qsTr("Open Location from clipboard"),
    ]

    AccessibleCompat.id: "openFileButton"

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
