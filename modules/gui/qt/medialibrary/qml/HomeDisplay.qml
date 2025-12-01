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

    property int displayMarginBeginning: 0
    property int displayMarginEnd: 0

    property bool enableBeginningFade: true
    property bool enableEndFade: true

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

        Widgets.PageExt {
            id: continueWatchingPage

            title: qsTr("Continue Watching")

            VideoAll {
                id: continueWatching

                anchors.fill: parent

                focus: true

                model: MLRecentVideoModel {
                    ml: MediaLib

                    sortCriteria: MainCtx.sort.criteria
                    sortOrder: MainCtx.sort.order
                    searchPattern: MainCtx.search.pattern
                }

                sectionProperty: model.sortCriteria === "title" ? "title_first_symbol" : ""

                contextMenu: MLContextMenu {
                    model: continueWatching.model

                    showPlayAsAudioAction: true
                }

                displayMarginBeginning: root.displayMarginBeginning
                displayMarginEnd: root.displayMarginEnd

                enableBeginningFade: root.enableBeginningFade
                enableEndFade: root.enableEndFade
            }
        }
    }

    Component {
        id: favoritesComponent

        Widgets.PageExt {
            id: favoritesPage

            title: qsTr("Favorites")

            MediaView {
                focus: true

                anchors.fill: parent

                model: MLMediaModel {
                    favoriteOnly: true

                    ml: MediaLib

                    sortCriteria: MainCtx.sort.criteria || "insertion"
                    sortOrder: MainCtx.sort.order
                    searchPattern: MainCtx.search.pattern
                }

                displayMarginBeginning: root.displayMarginBeginning
                displayMarginEnd: root.displayMarginEnd

                enableBeginningFade: root.enableBeginningFade
                enableEndFade: root.enableEndFade
            }
        }
    }

    Component {
        id: newMediaComponent

        Widgets.PageExt {
            id: newMediaPage

            title: qsTr("New Medias")

            MediaView {
                focus: true

                anchors.fill: parent

                model: MLMediaModel {
                    ml: MediaLib

                    sortCriteria: MainCtx.sort.criteria || "insertion"
                    sortOrder: MainCtx.sort.order
                    searchPattern: MainCtx.search.pattern
                }

                displayMarginBeginning: root.displayMarginBeginning
                displayMarginEnd: root.displayMarginEnd

                enableBeginningFade: root.enableBeginningFade
                enableEndFade: root.enableEndFade
            }
        }
    }
}
