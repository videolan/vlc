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
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Templates as T

import VLC.MediaLibrary

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style

Widgets.PageExt {
    id: root

    property var pagePrefix: [] // behave like a Page

    property var sortModel: [
        { text: qsTr("Alphabetic"), criteria: "title" },
        { text: qsTr("Duration"), criteria: "duration" }
    ]

    title: qsTr("Home")

    hasGridListMode: true

    isSearchable: true

    property real listCoverHeight: VLCStyle.listAlbumCover_height
    property real listCoverWidth: VLCStyle.listAlbumCover_width
    property real listCoverRadius: VLCStyle.listAlbumCover_radius

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    signal seeAllButtonClicked(string name, int reason)

    function positionContentAtBeginning() {
        contentYBehavior.enabled = true
        flickable.contentY = -flickable.originY
        contentYBehavior.enabled = false
    }

    function setCurrentItemFocus(reason) {
        // `focus` is checked, because that indicates the item that has the focus within the focus scope
        if (continueWatchingRow.focus)
            continueWatchingRow.setCurrentItemFocus(reason)
        else if (favoritesRow.focus)
            favoritesRow.setCurrentItemFocus(reason)
        else if (newVideoRow.focus)
            newVideoRow.setCurrentItemFocus(reason)
        else
            coneNButtons.forceActiveFocus(reason)
    }

    contentItem: Flickable {
        id: flickable

        implicitWidth: Math.max(coneNButtons.implicitWidth, mediaRows.implicitWidth)
        implicitHeight: coneNButtons.implicitHeight + (_hasMedias ? mediaRows.implicitHeight : 0)

        contentWidth: width
        contentHeight: _hasMedias ? coneNButtons.implicitHeight + mediaRows.implicitHeight
                                  : height - topMargin - bottomMargin

        flickableDirection: Flickable.AutoFlickIfNeeded

        boundsBehavior: Flickable.StopAtBounds

        // FIXME: make the media rows positioners consider the flickable margins when adjusting flickable `contentY`.
        topMargin: VLCStyle.layoutTitle_top_padding
        bottomMargin: topMargin

        pixelAligned: (MainCtx.qtVersion() >= MainCtx.qtVersionCheck(6, 2, 5)) // QTBUG-103996
                      && (Screen.pixelDensity >= VLCStyle.highPixelDensityThreshold) // no need for sub-pixel alignment with high pixel density

        property bool _hasMedias: true

        ScrollBar.vertical: Widgets.ScrollBarExt {}

        // This behavior allows to have similar "smooth" animation
        // that Qt views have with `highlightFollowsCurrentItem`.
        Behavior on contentY {
            id: contentYBehavior

            enabled: false

            // NOTE: Usage of `SmoothedAnimation` is intentional here.
            SmoothedAnimation {
                duration: VLCStyle.duration_veryLong
                easing.type: Easing.InOutSine
            }
        }

        property Component implicitFlickableScrollHandler: DefaultFlickableScrollHandler { }

        // NOTE: This property can be set to null to prevent using a scroll handler:
        property FlickableScrollHandler scrollHandler: {
            if (interactive) {
                // JS ownership:
                return implicitFlickableScrollHandler.createObject(null, { target: flickable })
            } else {
                return null
            }
        }

        Component.onCompleted: {
            if (!usingTouch) {
                // Flickable filters child mouse events for flicking (even when
                // the delegate is grabbed). However, this is not a useful
                // feature for non-touch cases, so disable it here and enable
                // it if touch is detected through the hover handler:
                MainCtx.setFiltersChildMouseEvents(flickable, false)
            }

            MainCtx.setTimeout(() => {
                flickable._hasMedias = Qt.binding(() => { return continueWatchingRow.visible || favoritesRow.visible || newVideoRow.visible } )
            }, 50, [], flickable)
        }

        readonly property bool usingTouch: MainCtx.usingTouch

        onUsingTouchChanged: {
            if (usingTouch)
                MainCtx.setFiltersChildMouseEvents(flickable, true)
            // We do not disable filtering child mouse events
            // because Qt currently has a bug that the flickable
            // jumps when it is enabled again later on.
        }

        NoMedialibHome.ConeNButtons {
            id: coneNButtons

            focus: true

            orientation: flickable._hasMedias ? Qt.Horizontal : Qt.Vertical

            anchors.centerIn: flickable._hasMedias ? undefined : parent
            anchors.top: flickable._hasMedias ? parent.top : undefined
            anchors.left: flickable._hasMedias ? parent.left : undefined
            anchors.leftMargin: flickable._hasMedias ? newVideoRow.contentLeftMargin : 0

            Navigation.parentItem: root
            Navigation.upItem: root.header
            Navigation.downItem: mediaRows

            onActiveFocusChanged: {
                contentYBehavior.enabled = true
                Helpers.positionFlickableToContainItem(flickable, this)
                contentYBehavior.enabled = false
            }
        }

        Widgets.NavigableCol {
            id: mediaRows

            anchors.top: coneNButtons.bottom
            anchors.left: parent.left
            anchors.right: parent.right

            spacing: 0 // NOTE: We depend on the top padding of view header currently.

            Navigation.parentItem: root
            Navigation.upItem: coneNButtons

            Widgets.ViewHeader {
                visible: continueWatchingRow.visible
                text: qsTr("Continue Watching")
                view: continueWatchingRow
                seeAllButton.visible: continueWatchingRow.model.maximumCount > continueWatchingRow.model.count

                onSeeAllButtonClicked: function (reason) {
                    root.seeAllButtonClicked("continueWatching", reason)
                }
            }

            VideoAll {
                id: continueWatchingRow

                anchors.left: parent.left
                anchors.right: parent.right

                height: currentItem?.contentHeight ?? implicitHeight

                visible: model.count !== 0

                headerPositioning: ListView.InlineHeader
                enableBeginningFade: false
                enableEndFade: false
                sectionProperty: ""

                interactive: false

                emptyLabel: null

                // FIXME: `ExpandGridView` causes extreme performance degradation when `reuseItems`
                //        is true and items provided by the model change (#29084).
                reuseItems: !MainCtx.gridView

                listCoverWidth: root.listCoverWidth
                listCoverHeight: root.listCoverHeight
                listCoverRadius: root.listCoverRadius

                Navigation.parentItem: mediaRows

                model: MLRecentVideoModel {
                    ml: MediaLib

                    sortCriteria: root.sort.criteria
                    sortOrder: root.sort.order
                    searchPattern: root.search.pattern

                    // FIXME: Make limit 0 load no items, instead of loading all items.
                    limit: MainCtx.gridView ? Math.max(continueWatchingRow.currentItem?.nbItemPerRow ?? null, 1) : 5
                }

                contextMenu: MLContextMenu {
                    model: continueWatchingRow.model

                    showPlayAsAudioAction: true
                }

                onActiveFocusChanged: {
                    if (activeFocus) {
                        const item = currentItem?.currentItem ?? currentItem?._getItem(currentIndex) // FIXME: `ExpandGridView` does not have `currentItem`.
                        contentYBehavior.enabled = true
                        Helpers.positionFlickableToContainItem(flickable, item ?? this)
                        contentYBehavior.enabled = false
                    }
                }

                onCurrentIndexChanged: {
                    if (activeFocus) {
                        const item = currentItem?.currentItem ?? currentItem?._getItem(currentIndex) // FIXME: `ExpandGridView` does not have `currentItem`.
                        if (item) {
                            contentYBehavior.enabled = true
                            Helpers.positionFlickableToContainItem(flickable, item)
                            contentYBehavior.enabled = false
                        }
                    }
                }
            }

            Widgets.ViewHeader {
                visible: favoritesRow.visible
                text: qsTr("Favorites")
                view: favoritesRow
                seeAllButton.visible: favoritesRow.model.maximumCount > favoritesRow.model.count

                onSeeAllButtonClicked: function (reason) {
                    root.seeAllButtonClicked("favorites", reason)
                }
            }

            MediaView {
                id: favoritesRow

                anchors.left: parent.left
                anchors.right: parent.right

                height: currentItem?.contentHeight ?? implicitHeight

                visible: model.count !== 0

                listHeaderPositioning: ListView.InlineHeader
                enableBeginningFade: false
                enableEndFade: false
                listSectionProperty: ""

                interactive: false

                emptyLabel: null

                // FIXME: `ExpandGridView` causes extreme performance degradation when `reuseItems`
                //        is true and items provided by the model change (#29084).
                reuseItems: !MainCtx.gridView

                listCoverWidth: root.listCoverWidth
                listCoverHeight: root.listCoverHeight
                listCoverRadius: root.listCoverRadius

                Navigation.parentItem: mediaRows

                model: MLMediaModel {
                    favoriteOnly: true

                    ml: MediaLib

                    sortCriteria: root.sort.criteria || "insertion"
                    sortOrder: root.sort.order
                    searchPattern: root.search.pattern

                    // FIXME: Make limit 0 load no items, instead of loading all items.
                    limit: MainCtx.gridView ? Math.max(favoritesRow.currentItem?.nbItemPerRow ?? null, 1) : 5
                }

                onActiveFocusChanged: {
                    if (activeFocus) {
                        const item = currentItem?.currentItem ?? currentItem?._getItem(currentIndex) // FIXME: `ExpandGridView` does not have `currentItem`.
                        contentYBehavior.enabled = true
                        Helpers.positionFlickableToContainItem(flickable, item ?? this)
                        contentYBehavior.enabled = false
                    }
                }

                onCurrentIndexChanged: {
                    if (activeFocus) {
                        const item = currentItem?.currentItem ?? currentItem?._getItem(currentIndex) // FIXME: `ExpandGridView` does not have `currentItem`.
                        if (item) {
                            contentYBehavior.enabled = true
                            Helpers.positionFlickableToContainItem(flickable, item)
                            contentYBehavior.enabled = true
                        }
                    }
                }
            }

            Widgets.ViewHeader {
                text: qsTr("New Videos")
                visible: newVideoRow.visible
                view: newVideoRow
                seeAllButton.visible: newVideoRow.model.maximumCount > newVideoRow.model.count

                onSeeAllButtonClicked: function (reason) {
                    root.seeAllButtonClicked("newVideo", reason)
                }
            }

            VideoAll {
                id: newVideoRow

                anchors.left: parent.left
                anchors.right: parent.right

                height: currentItem?.contentHeight ?? implicitHeight

                visible: model.count !== 0

                headerPositioning: ListView.InlineHeader
                enableBeginningFade: false
                enableEndFade: false
                sectionProperty: ""

                interactive: false

                emptyLabel: null

                // FIXME: `ExpandGridView` causes extreme performance degradation when `reuseItems`
                //        is true and items provided by the model change (#29084).
                reuseItems: !MainCtx.gridView

                listCoverWidth: root.listCoverWidth
                listCoverHeight: root.listCoverHeight
                listCoverRadius: root.listCoverRadius

                Navigation.parentItem: mediaRows

                model: MLVideoModel {
                    ml: MediaLib

                    sortCriteria: root.sort.criteria || "insertion"
                    sortOrder: root.sort.order
                    searchPattern: root.search.pattern

                    // FIXME: Make limit 0 load no items, instead of loading all items.
                    limit: MainCtx.gridView ? Math.max(newVideoRow.currentItem?.nbItemPerRow ?? null, 1) : 5
                }

                contextMenu: MLContextMenu {
                    model: newVideoRow.model

                    showPlayAsAudioAction: true
                }

                onActiveFocusChanged: {
                    if (activeFocus) {
                        const item = currentItem?.currentItem ?? currentItem?._getItem(currentIndex) // FIXME: `ExpandGridView` does not have `currentItem`.
                        contentYBehavior.enabled = true
                        Helpers.positionFlickableToContainItem(flickable, item ?? this)
                        contentYBehavior.enabled = false
                    }
                }

                onCurrentIndexChanged: {
                    if (activeFocus) {
                        const item = currentItem?.currentItem ?? currentItem?._getItem(currentIndex) // FIXME: `ExpandGridView` does not have `currentItem`.
                        if (item) {
                            contentYBehavior.enabled = true
                            Helpers.positionFlickableToContainItem(flickable, item)
                            contentYBehavior.enabled = false
                        }
                    }
                }
            }
        }
    }
}
