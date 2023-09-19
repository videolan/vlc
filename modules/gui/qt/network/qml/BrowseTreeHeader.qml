
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

import org.videolan.vlc 0.1

import "qrc:///util/" as Util
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///widgets/" as Widgets
import "qrc:///style/"

T.Control {
    id: root

    // Network* model
    /* required */ property var providerModel

    readonly property ColorContext colorContext: ColorContext {
        id: theme

        colorSet: ColorContext.View
    }

    height: implicitHeight
    implicitHeight: layout.implicitHeight + topPadding + bottomPadding

    topPadding: VLCStyle.layoutTitle_top_padding
    bottomPadding: VLCStyle.layoutTitle_bottom_padding

    focus: medialibraryBtn.visible

    Navigation.navigable: medialibraryBtn.visible

    RowLayout {
        id: layout

        anchors {
            fill: parent

            leftMargin: root.leftPadding
            rightMargin: root.rightPadding

            topMargin: root.topPadding
            bottomMargin: root.bottomPadding
        }

        Widgets.SubtitleLabel {
            text: providerModel.name
            color: colorContext.fg.primary

            Layout.fillWidth: true
        }

        Widgets.ButtonExt {
            id: medialibraryBtn

            focus: true

            iconTxt: providerModel.indexed ? VLCIcons.remove : VLCIcons.add

            text: providerModel.indexed
                  ? qsTr("Remove from medialibrary")
                  : qsTr("Add to medialibrary")

            visible: !providerModel.is_on_provider_list
                     && !!providerModel.canBeIndexed

            onClicked: providerModel.indexed = !providerModel.indexed

            Layout.preferredWidth: implicitWidth

            Navigation.parentItem: root
        }
    }
}
