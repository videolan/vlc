/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import QtQml.Models 2.2

import org.videolan.vlc 0.1

import "qrc:///util" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///util/KeyHelper.js" as KeyHelper
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root

    Column {
        anchors.fill: parent

        Widgets.NavigableFocusScope {
            id: searchFieldContainer

            width: root.width
            height: searchField.height + VLCStyle.margin_normal * 2
            focus: true

            navigationParent:  root
            navigationDownItem: !medialib ? undefined : urlListDisplay.item

            TextField {
                id: searchField

                focus: true
                anchors.centerIn: parent
                height: VLCStyle.dp(32, VLCStyle.scale)
                width: VLCStyle.colWidth(Math.max(VLCStyle.gridColumnsForWidth(root.width * .6), 2))
                placeholderText: i18n.qtr("Paste or write the URL here")
                color: VLCStyle.colors.text
                font.pixelSize: VLCStyle.fontSize_large
                background: Rectangle {
                    color: VLCStyle.colors.bg
                    border.width: VLCStyle.dp(2, VLCStyle.scale)
                    border.color: searchField.activeFocus || searchField.hovered
                                  ? VLCStyle.colors.accent
                                  : VLCStyle.colors.setColorAlpha(VLCStyle.colors.text, .4)
                }

                onAccepted: {
                    mainPlaylistController.append([text], true)
                }

                Keys.priority: Keys.AfterItem
                Keys.onPressed: {
                    if (event.accepted)
                        return
                    searchFieldContainer.defaultKeyAction(event, 0)
                }
            }
        }

        Loader {
            id: urlListDisplay

            width: parent.width
            height: parent.height - searchFieldContainer.height

            active:  !!medialib
            source: "qrc:///medialibrary/UrlListDisplay.qml"
            onLoaded: {
                item.navigationUpItem = searchField
                item.navigationParent =  root
            }
        }
    }
}
