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
import org.videolan.medialib 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

Widgets.NavigableFocusScope {
    id: root
    property alias sortModel: tracklistdisplay_id.sortModel
    property alias model: tracklistdisplay_id.model

    Widgets.MenuExt {
        id: contextMenu
        property var model: ({})
        closePolicy: Popup.CloseOnReleaseOutside | Popup.CloseOnEscape

        Widgets.MenuItemExt {
            id: playMenuItem
            text: "Play from start"
            onTriggered: {
                medialib.addAndPlay( contextMenu.model.id )
                history.push(["player"])
            }
        }

        Widgets.MenuItemExt {
            text: "Enqueue"
            onTriggered: medialib.addToPlaylist( contextMenu.model.id )
        }

        onClosed: contextMenu.parent.forceActiveFocus()

    }

    MusicTrackListDisplay {
        id: tracklistdisplay_id
        anchors.fill: parent
        visible: model.count > 0
        focus: visible
        navigationParent: root
        navigationCancel: function() {
            if (tracklistdisplay_id.currentIndex <= 0)
                defaultNavigationCancel()
            else
                tracklistdisplay_id.currentIndex = 0;
        }

        onContextMenuButtonClicked: {
            contextMenu.model = menuModel
            contextMenu.popup(menuParent)
        }
    }

    EmptyLabel {
        anchors.fill: parent
        visible: tracklistdisplay_id.model.count === 0
        focus: visible
        text: i18n.qtr("No tracks found\nPlease try adding sources, by going to the Network tab")
        navigationParent: root
    }
}
