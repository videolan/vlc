
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
import QtQuick.Controls
import QtQuick.Templates as T


import VLC.Util
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Network
import VLC.MainInterface

T.Pane {
    id: root

    signal browse(var tree, int reason)
    signal homeButtonClicked(int reason)

    // Network* model
    required property BaseModel providerModel
    property var path: providerModel?.path ?? []

    readonly property ColorContext colorContext: ColorContext {
        id: theme

        colorSet: ColorContext.View
    }

    topPadding: VLCStyle.layoutTitle_top_padding
    bottomPadding: VLCStyle.layoutTitle_bottom_padding

    // FIXME: `GridItem`'s background extends beyond its
    //        bounding rect, violating the hypothetical
    //        clip test (see 7e6b23db).
    bottomInset: MainCtx.gridView ? VLCStyle.gridItemSelectedBorder : undefined

    height: implicitHeight
    implicitHeight: layout.implicitHeight + topPadding + bottomPadding
    implicitWidth: layout.implicitWidth + leftPadding + rightPadding

    focus: medialibraryBtn.visible
    Navigation.navigable: medialibraryBtn.visible

    background: Rectangle {
        color: theme.bg.primary
    }

    RowLayout {
        id: layout

        anchors.fill: parent

        NetworkAddressbar {
            Layout.fillWidth: true
            Layout.fillHeight: true

            background: null

            path: root.path

            onHomeButtonClicked: reason => root.homeButtonClicked(reason)

            onBrowse:  (tree, reason) => root.browse(tree, reason)
        }

        Widgets.ButtonExt {
            id: medialibraryBtn

            focus: true

            iconTxt: root.providerModel.indexed ? VLCIcons.collections_remove : VLCIcons.collections_add

            text: root.providerModel.indexed
                  ? qsTr("Remove from medialibrary")
                  : qsTr("Add to medialibrary")

            display: VLCStyle.isScreenSmall ?  AbstractButton.IconOnly  : AbstractButton.TextBesideIcon

            visible: root.providerModel.canBeIndexed ?? false

            onClicked: root.providerModel.indexed = !root.providerModel.indexed

            Layout.preferredWidth: implicitWidth

            Navigation.parentItem: root
            Navigation.rightItem: gridSortFilter
        }

        Widgets.GridSortFilterControls {
            id: gridSortFilter

            Navigation.leftItem: medialibraryBtn
        }
    }
}
