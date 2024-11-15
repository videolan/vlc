
/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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
import QtQuick.Layouts
import QtQuick.Templates as T


import VLC.Util
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Network
import VLC.MainInterface

T.Pane {
    id: root

    // Network* model
    required property BaseModel providerModel

    readonly property ColorContext colorContext: ColorContext {
        id: theme

        colorSet: ColorContext.View
    }

    topPadding: VLCStyle.layoutTitle_top_padding
    bottomPadding: VLCStyle.layoutTitle_bottom_padding

    height: implicitHeight
    implicitHeight: layout.implicitHeight + topPadding + bottomPadding
    implicitWidth: layout.implicitWidth + leftPadding + rightPadding

    focus: medialibraryBtn.visible
    Navigation.navigable: medialibraryBtn.visible

    RowLayout {
        id: layout

        anchors.fill: parent

        Widgets.SubtitleLabel {
            text: providerModel.name
            color: colorContext.fg.primary

            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        Widgets.ButtonExt {
            id: medialibraryBtn

            readonly property NetworkMediaModel networkModel: providerModel as NetworkMediaModel

            focus: true

            iconTxt: networkModel?.indexed ? VLCIcons.remove : VLCIcons.add

            text: networkModel?.indexed
                  ? qsTr("Remove from medialibrary")
                  : qsTr("Add to medialibrary")

            visible: providerModel?.canBeIndexed ?? false

            onClicked: networkModel.indexed = !networkModel.indexed

            Layout.preferredWidth: implicitWidth

            Navigation.parentItem: root
        }
    }
}
