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

T.Page {
    id: root

    property var pagePrefix: [] // behave like a Page

    property var sortModel: [
        { text: qsTr("Alphabetic"), criteria: "title" },
        { text: qsTr("Duration"), criteria: "duration" }
    ]

    readonly property bool hasGridListMode: true

    readonly property bool isSearchable: true

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding,
                            implicitHeaderWidth,
                            implicitFooterWidth)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding
                             + (implicitHeaderHeight > 0 ? implicitHeaderHeight + spacing : 0)
                             + (implicitFooterHeight > 0 ? implicitFooterHeight + spacing : 0))

    title: qsTr("Home")


    signal seeAllButtonClicked(string name, int reason)


    function setCurrentItemFocus(reason) {
        // `focus` is checked, because that indicates the item that has the focus within the focus scope
        if (continueWatchingRow.focus)
            continueWatchingRow.setCurrentItemFocus(reason)
        else if (favoritesRow.focus)
            favoritesRow.setCurrentItemFocus(reason)
        else if (newMediaRow.focus)
            newMediaRow.setCurrentItemFocus(reason)
        else
            forceActiveFocus(reason) // this should not be necessary normally, but when there is `setCurrentItemFocus()`, it seems the root item does not get focus
    }

    contentItem: Flickable {
        id: flickable

        implicitWidth: Math.max(header.implicitWidth, coneNButtons.implicitWidth, mediaRows.implicitWidth)
        implicitHeight: header.implicitHeight + coneNButtons.implicitHeight + (_hasMedias ? mediaRows.implicitHeight : 0)

        contentWidth: width
        contentHeight: _hasMedias ? header.implicitHeight + coneNButtons.implicitHeight + mediaRows.implicitHeight
                                  : height - topMargin - bottomMargin

        flickableDirection: Flickable.AutoFlickIfNeeded

        boundsBehavior: Flickable.StopAtBounds

        // FIXME: make the media rows positioners consider the flickable margins when adjusting flickable `contentY`.
        topMargin: VLCStyle.layoutTitle_top_padding
        bottomMargin: topMargin

        pixelAligned: (MainCtx.qtVersion() >= MainCtx.qtVersionCheck(6, 2, 5)) // QTBUG-103996
                      && (Screen.pixelDensity >= VLCStyle.highPixelDensityThreshold) // no need for sub-pixel alignment with high pixel density

        property bool _hasMedias: true

        Timer {
            interval: 50
            running: true

            onTriggered: {
                flickable._hasMedias = Qt.binding(() => { return continueWatchingRow.visible || favoritesRow.visible || newMediaRow.visible } )
            }
        }

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

        DefaultFlickableScrollHandler {}

        Component.onCompleted: {
            // Flickable filters child mouse events for flicking (even when
            // the delegate is grabbed). However, this is not a useful
            // feature for non-touch cases, so disable it here and enable
            // it if touch is detected through the hover handler:
            MainCtx.setFiltersChildMouseEvents(this, false)
        }

        HoverHandler {
            acceptedDevices: PointerDevice.TouchScreen

            onHoveredChanged: {
                if (hovered)
                    MainCtx.setFiltersChildMouseEvents(flickable, true)
                else
                    MainCtx.setFiltersChildMouseEvents(flickable, false)
            }
        }

        // FIXME: Do not use `ViewHeader` for page titles.
        // FIXME: Use `header` property for `Page` header.
        //        `header` dosen't respect paddings of `contentItem`,
        //        and allows the `contentItem` to be scrolled through it.
        Widgets.ViewHeader {
            id: header

            visible: flickable._hasMedias

            view: newMediaRow

            text: root.title

            topPadding: 0

            Binding on implicitHeight {
                when: !flickable._hasMedias
                value: 0.0
            }
        }

        NoMedialibHome.ConeNButtons {
            id: coneNButtons

            focus: true

            orientation: flickable._hasMedias ? Qt.Horizontal : Qt.Vertical

            anchors.centerIn: flickable._hasMedias ? undefined : parent
            anchors.top: flickable._hasMedias ? header.bottom : undefined
            anchors.left: flickable._hasMedias ? parent.left : undefined
            anchors.leftMargin: flickable._hasMedias ? newMediaRow.contentLeftMargin : 0

            Navigation.parentItem: root
            Navigation.downAction: function() {
                if (continueWatchingRow.visible)
                    continueWatchingRow.setCurrentItemFocus(Qt.TabFocusReason)
                else
                    continueWatchingRow.Navigation.defaultNavigationDown()
            }

            onActiveFocusChanged: {
                contentYBehavior.enabled = true
                Helpers.positionFlickableToContainItem(flickable, this)
                contentYBehavior.enabled = false
            }
        }

        Column {
            id: mediaRows

            anchors.top: coneNButtons.bottom
            anchors.left: parent.left
            anchors.right: parent.right

            spacing: 0 // NOTE: We depend on the top padding of view header currently.

            Navigation.parentItem: root
            Navigation.upItem: coneNButtons

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

                Navigation.parentItem: mediaRows
                Navigation.downAction: function() {
                    if (favoritesRow.visible)
                        favoritesRow.setCurrentItemFocus(Qt.TabFocusReason)
                    else
                        favoritesRow.Navigation.defaultNavigationDown()
                }

                model: MLRecentVideoModel {
                    ml: MediaLib

                    sortCriteria: MainCtx.sort.criteria
                    sortOrder: MainCtx.sort.order
                    searchPattern: MainCtx.search.pattern

                    // FIXME: Make limit 0 load no items, instead of loading all items.
                    limit: MainCtx.gridView ? Math.max(continueWatchingRow.currentItem?.nbItemPerRow ?? null, 1) : 5
                }

                header: Widgets.ViewHeader {
                    view: continueWatchingRow

                    text: qsTr("Continue Watching")

                    seeAllButton.visible: continueWatchingRow.model.maximumCount > continueWatchingRow.model.count

                    Navigation.parentItem: continueWatchingRow
                    Navigation.downAction: function () {
                        if (continueWatchingRow.currentItem?.setCurrentItemFocus)
                            continueWatchingRow.currentItem.setCurrentItemFocus(Qt.TabFocusReason)
                    }

                    Component.onCompleted: {
                        seeAllButtonClicked.connect(continueWatchingRow.seeAllButtonClicked)
                    }
                }

                contextMenu: MLContextMenu {
                    model: continueWatchingRow.model

                    showPlayAsAudioAction: true
                }

                onSeeAllButtonClicked: function (reason) {
                    root.seeAllButtonClicked("continueWatching", reason)
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

                Navigation.parentItem: mediaRows
                Navigation.upAction: function() {
                    if (continueWatchingRow.visible)
                        continueWatchingRow.setCurrentItemFocus(Qt.BacktabFocusReason)
                    else
                        continueWatchingRow.Navigation.defaultNavigationUp()
                }
                Navigation.downAction: function() {
                    if (newMediaRow.visible)
                        newMediaRow.setCurrentItemFocus(Qt.TabFocusReason)
                    else
                        newMediaRow.Navigation.defaultNavigationDown()
                }

                model: MLMediaModel {
                    favoriteOnly: true

                    ml: MediaLib

                    sortCriteria: MainCtx.sort.criteria || "insertion"
                    sortOrder: MainCtx.sort.order
                    searchPattern: MainCtx.search.pattern

                    // FIXME: Make limit 0 load no items, instead of loading all items.
                    limit: MainCtx.gridView ? Math.max(favoritesRow.currentItem?.nbItemPerRow ?? null, 1) : 5
                }

                headerText: qsTr("Favorites")

                onSeeAllButtonClicked: function (reason) {
                    root.seeAllButtonClicked("favorites", reason)
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

            MediaView {
                id: newMediaRow

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

                Navigation.parentItem: mediaRows
                Navigation.upAction: function() {
                    if (favoritesRow.visible)
                        favoritesRow.setCurrentItemFocus(Qt.BacktabFocusReason)
                    else
                        favoritesRow.Navigation.defaultNavigationUp()
                }

                model: MLMediaModel {
                    ml: MediaLib

                    sortCriteria: MainCtx.sort.criteria || "insertion"
                    sortOrder: MainCtx.sort.order
                    searchPattern: MainCtx.search.pattern

                    // FIXME: Make limit 0 load no items, instead of loading all items.
                    limit: MainCtx.gridView ? Math.max(newMediaRow.currentItem?.nbItemPerRow ?? null, 1) : 5
                }

                headerText: qsTr("New Media")

                onSeeAllButtonClicked: function (reason) {
                    root.seeAllButtonClicked("newMedia", reason)
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
