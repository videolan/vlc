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
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Templates as T
import QtQml.Models
import QtQml


import VLC.Widgets as Widgets
import VLC.MainInterface
import VLC.Util
import VLC.Style
import VLC.Network

FocusScope {
    id: root

    // Properties

    property bool _initialized: false
    property bool _resetFocusPendingAfterInitialization: false

    property int leftPadding: 0
    property int rightPadding: 0

    property int maximumRows: {
        if (model.searchPattern !== "")
            return -1
        else if (MainCtx.gridView)
            return 2
        else
            return 5
    }

    property var sortModel: [
        { text: qsTr("Alphabetic"), criteria: "name"},
        { text: qsTr("Url"),        criteria: "mrl" }
    ]

    readonly property bool hasGridListMode: true
    readonly property bool isSearchable: true

    //behave like a Page
    property var pagePrefix: []

    // Aliases

    property alias model: foldersSection.model

    // Signals

    signal seeAllDevices(var title, var sd_source, int reason)
    signal seeAllFolders(var title, int reason)

    signal browse(var tree, int reason)

    focus: true


    Component.onCompleted: {
        _initialized = true
        if (_resetFocusPendingAfterInitialization) {
            resetFocus()

            _resetFocusPendingAfterInitialization = false
        }
    }

    onActiveFocusChanged: {
        if (!_initialized) {
            _resetFocusPendingAfterInitialization = true
            return
        }

        resetFocus()
    }

    function setCurrentItemFocus(reason) {
        if (foldersSection.visible)
            foldersSection.setCurrentItemFocus(reason);
        else if (deviceSection.visible)
            deviceSection.setCurrentItemFocus(reason);
        else if (lanSection.visible)
            lanSection.setCurrentItemFocus(reason);
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    //FIXME use the right xxxLabel class
    T.Label {
        anchors.centerIn: parent

        visible: (foldersSection.model.count === 0 && deviceSection.model.count === 0
                  &&
                  lanSection.model.count === 0)

        font.pixelSize: VLCStyle.fontHeight_xxlarge
        color: root.activeFocus ? theme.accent : theme.fg.primary
        text: qsTr("No network shares found")
    }

    Flickable {
        id: flickable

        anchors.fill: parent

        anchors.leftMargin: root.leftPadding
        anchors.rightMargin: root.rightPadding

        ScrollBar.vertical: Widgets.ScrollBarExt { }

        flickableDirection: Flickable.AutoFlickIfNeeded
        boundsBehavior: Flickable.StopAtBounds

        focus: true

        pixelAligned: (MainCtx.qtVersion() >= MainCtx.qtVersionCheck(6, 2, 5)) // QTBUG-103996
                      && (Screen.pixelDensity >= VLCStyle.highPixelDensityThreshold) // no need for sub-pixel alignment with high pixel density

        contentWidth: column.width
        contentHeight: column.height

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

        DefaultFlickableScrollHandler { }

        Navigation.parentItem: root

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

        Widgets.NavigableCol {
            id: column

            width: flickable.width
            height: implicitHeight

            spacing: 0 // relied on the generous padding of ViewHeader instead
            bottomPadding: VLCStyle.margin_normal // topPadding taken care by ViewHeader

            Navigation.parentItem: root

            HomeDeviceView {
                id: foldersSection

                title: qsTr("Folders")

                model: StandardPathModel {
                    //we only have a handfull of standard path (5 or 6)
                    //so we don't limit them

                    sortCriteria: MainCtx.sort.criteria
                    sortOrder: MainCtx.sort.order
                    searchPattern: MainCtx.search.pattern
                }
            }

            HomeDeviceView {
                id: computerSection

                title: qsTr("Computer")

                model: NetworkDeviceModel {
                    ctx: MainCtx

                    sd_source: NetworkDeviceModel.CAT_MYCOMPUTER
                    source_name: "*"

                    limit: computerSection.maximumCount

                    sortOrder: MainCtx.sort.order
                    sortCriteria: MainCtx.sort.criteria
                    searchPattern: MainCtx.search.pattern
                }
            }

            HomeDeviceView {
                id: deviceSection

                title: qsTr("Devices")

                model: NetworkDeviceModel {
                    ctx: MainCtx

                    limit: deviceSection.maximumCount

                    sortOrder: MainCtx.sort.order
                    sortCriteria: MainCtx.sort.criteria
                    searchPattern: MainCtx.search.pattern

                    sd_source: NetworkDeviceModel.CAT_DEVICES
                    source_name: "*"
                }
            }

            HomeDeviceView {
                id: lanSection

                title: qsTr("Network")

                model: NetworkDeviceModel {
                    ctx: MainCtx

                    sd_source: NetworkDeviceModel.CAT_LAN
                    source_name: "*"

                    limit: lanSection.maximumCount

                    sortOrder: MainCtx.sort.order
                    sortCriteria: MainCtx.sort.criteria
                    searchPattern: MainCtx.search.pattern
                }
            }
        }
    }

    function resetFocus() {
        if (!activeFocus)
            return

        for (let i = 0; i < column.count; ++i) {
            const widget = column.itemAt(i)
            if (widget.activeFocus && widget.visible)
                return
        }

        for (let i = 0; i < column.count; ++i){
            const widget = column.itemAt(i)
            if (widget.visible) {
                Helpers.transferFocus(widget, Qt.TabFocusReason)
                break
            }
        }
    }

    component HomeDeviceView: BrowseDeviceView {
        width: flickable.width
        height: contentHeight

        maximumRows: root.maximumRows

        visible: (model.count !== 0)

        interactive: false

        enableBeginningFade: false
        enableEndFade: false

        // FIXME: ExpandGridView makes this page completely unusable when `reuseItems` is set (#29084):
        reuseItems: !MainCtx.gridView

        onBrowse: (tree, reason) => root.browse(tree, reason)
        onSeeAll: (reason) => root.seeAllDevices(title, model.sd_source, reason)

        onActiveFocusChanged: {
            if (activeFocus) {
                const item = _currentView?.currentItem ?? _currentView?._getItem(currentIndex) // FIXME: `ExpandGridView` does not have `currentItem`.
                contentYBehavior.enabled = true
                Helpers.positionFlickableToContainItem(flickable, item ?? this)
                contentYBehavior.enabled = false
            }
        }

        onCurrentIndexChanged: {
            if (activeFocus) {
                const item = _currentView?.currentItem ?? _currentView?._getItem(currentIndex) // FIXME: `ExpandGridView` does not have `currentItem`.
                if (item) {
                    contentYBehavior.enabled = true
                    Helpers.positionFlickableToContainItem(flickable, item)
                    contentYBehavior.enabled = false
                }
            }
        }
    }
}
