/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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

import VLC.MainInterface
import VLC.MediaLibrary

import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style
import VLC.Menus

Widgets.PageExt {

    id: root

    property alias currentIndex: videoAll.currentIndex

    // NOTE: This is used to determine which media(s) shall be displayed.
    property alias parentId: modelVideo.parentId

    title: qsTr("Videos")

    sortMenu: SortMenuVideo {
        ctx: MainCtx

        onGrouping: (grouping) => { MainCtx.grouping = grouping }
    }

    VideoAll {
        id: videoAll

        // Aliases

        anchors.fill: parent

        focus: true

        displayMarginBeginning: root.displayMarginBeginning
        displayMarginEnd: root.displayMarginEnd
        enableBeginningFade: root.enableBeginningFade
        enableEndFade: root.enableEndFade

        sectionProperty: {
            switch (model.sortCriteria) {
            case "title":
                return "title_first_symbol"
            default:
                return ""
            }
        }

        function isInfoExpandPanelAvailable(/* modelIndexData */) {
            return true
        }

        // Children

        model: MLVideoModel {
            id: modelVideo

            searchPattern: MainCtx.search.pattern
            sortOrder: MainCtx.sort.order
            sortCriteria: MainCtx.sort.criteria

            ml: MediaLib
        }

        contextMenu: MLContextMenu { model: modelVideo; showPlayAsAudioAction: true }

    }
}
