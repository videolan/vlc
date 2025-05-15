/******************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Author: Ash <ashutoshv191@gmail.com>
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
 ******************************************************************************/
import QtQuick

import VLC.MediaLibrary

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Util

Widgets.PageLoader {
    id: root

    pageModel: [{
        name: "base",
        component: homeComponent,
        default: true
    }, {
        name: "continueWatching",
        component: continueWatchingComponent
    }, {
        name: "favorites",
        component: favoritesComponent
    }, {
        name: "newMedia",
        component: newMediaComponent
    }]

    Accessible.role: Accessible.Client
    Accessible.name: qsTr("Home View")


    Component {
        id: homeComponent

        HomePage {
            focus: true

            onSeeAllButtonClicked: (name, reason) => {
                History.push([...root.pagePrefix, name], reason)
            }
        }
    }

    Component {
        id: continueWatchingComponent

        VideoAll {
            id: continueWatching

            focus: true

            model: MLRecentVideoModel {
                ml: MediaLib

                sortCriteria: MainCtx.sort.criteria
                sortOrder: MainCtx.sort.order
                searchPattern: MainCtx.search.pattern
            }

            header: Widgets.ViewHeader {
                view: continueWatching

                text: qsTr("Continue Watching")
            }

            sectionProperty: model.sortCriteria === "title" ? "title_first_symbol" : ""

            contextMenu: MLContextMenu {
                model: continueWatching.model

                showPlayAsAudioAction: true
            }

            listEnableEndFade: g_mainDisplay.hasMiniPlayer === false

            displayMarginEnd: g_mainDisplay.displayMargin
        }
    }

    Component {
        id: favoritesComponent

        MediaView {
            focus: true

            model: MLMediaModel {
                favoriteOnly: true

                ml: MediaLib

                sortCriteria: MainCtx.sort.criteria || "insertion"
                sortOrder: MainCtx.sort.order
                searchPattern: MainCtx.search.pattern
            }

            headerText: qsTr("Favorites")

            listEnableEndFade: g_mainDisplay.hasMiniPlayer === false

            displayMarginEnd: g_mainDisplay.displayMargin
        }
    }

    Component {
        id: newMediaComponent

        MediaView {
            focus: true

            model: MLMediaModel {
                ml: MediaLib

                sortCriteria: MainCtx.sort.criteria || "insertion"
                sortOrder: MainCtx.sort.order
                searchPattern: MainCtx.search.pattern
            }

            headerText: qsTr("New Media")

            listEnableEndFade: g_mainDisplay.hasMiniPlayer === false

            displayMarginEnd: g_mainDisplay.displayMargin
        }
    }
}
