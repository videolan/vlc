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
import QtQuick.Controls
import QtQuick.Templates as T
import QtQml.Models
import QtQml

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///main/" as MainInterface
import "qrc:///util/" as Util
import "qrc:///style/"

FocusScope {
    id: root

    // Properties

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

    Component.onCompleted: resetFocus()
    onActiveFocusChanged: resetFocus()

    function setCurrentItemFocus(reason) {
        if (foldersSection.visible)
            foldersSection.setCurrentItemFocus(reason);
        else if (deviceSection.visible)
            deviceSection.setCurrentItemFocus(reason);
        else if (lanSection.visible)
            lanSection.setCurrentItemFocus(reason);
    }

    function _centerFlickableOnItem(item) {
        if (item.activeFocus === false)
            return

        let minY
        let maxY

        const index = item.currentIndex

        // NOTE: We want to include the header when we're on the first row.
        if ((MainCtx.gridView && index < item.nbItemPerRow) || index < 1) {
            minY = item.y

            maxY = minY + item.getItemY(index) + item.rowHeight
        } else {
            minY = item.y + item.getItemY(index)

            maxY = minY + item.rowHeight
        }

        // TODO: We could implement a scrolling animation like in ExpandGridView.
        if (maxY > flickable.contentItem.contentY + flickable.height) {
            flickable.contentItem.contentY = maxY - flickable.height
        } else if (minY < flickable.contentItem.contentY) {
            flickable.contentItem.contentY = minY
        }
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

    ScrollView {
        id: flickable

        anchors.fill: parent

        anchors.leftMargin: root.leftPadding
        anchors.rightMargin: root.rightPadding

        focus: true

        Column {
            width: foldersSection.width
            height: implicitHeight

            spacing: (MainCtx.gridView ? VLCStyle.gridView_spacing : VLCStyle.tableView_spacing) -
                     VLCStyle.layoutTitle_top_padding

            BrowseDeviceView {
                id: foldersSection

                width: root.width
                height: contentHeight

                visible: (model.count !== 0)

                model: StandardPathModel {
                    //we only have a handfull of standard path (5 or 6)
                    //so we don't limit them

                    sortCriteria: MainCtx.sort.criteria
                    sortOrder: MainCtx.sort.order
                    searchPattern: MainCtx.search.pattern
                }

                title: qsTr("My Folders")

                Navigation.parentItem: root

                Navigation.downAction: function() {
                    if (deviceSection.visible)
                        deviceSection.setCurrentItemFocus(Qt.TabFocusReason)
                    else if (lanSection.visible)
                        lanSection.setCurrentItemFocus(Qt.TabFocusReason)
                    else
                        root.Navigation.defaultNavigationDown()
                }

                onBrowse: (tree, reason) => root.browse(tree, reason)

                onSeeAll: (reason) => root.seeAllFolders(title, reason)

                onActiveFocusChanged: _centerFlickableOnItem(foldersSection)
                onCurrentIndexChanged: _centerFlickableOnItem(foldersSection)
            }

            BrowseDeviceView {
                id: deviceSection

                width: root.width
                height: contentHeight

                maximumRows: root.maximumRows

                visible: (model.count !== 0)

                model: NetworkDeviceModel {
                    ctx: MainCtx

                    limit: deviceSection.maximumCount

                    sortOrder: MainCtx.sort.order
                    sortCriteria: MainCtx.sort.criteria
                    searchPattern: MainCtx.search.pattern

                    sd_source: NetworkDeviceModel.CAT_DEVICES
                    source_name: "*"
                }


                title: qsTr("My Machine")

                Navigation.parentItem: root

                Navigation.upAction: function() {
                    if (foldersSection.visible)
                        foldersSection.setCurrentItemFocus(Qt.TabFocusReason)
                    else
                        root.Navigation.defaultNavigationUp()
                }

                Navigation.downAction: function() {
                    if (lanSection.visible)
                        lanSection.setCurrentItemFocus(Qt.TabFocusReason)
                    else
                        root.Navigation.defaultNavigationDown()
                }

                onBrowse: (tree, reason) => root.browse(tree, reason)

                onSeeAll: (reason) => root.seeAllDevices(title, model.sd_source, reason)

                onActiveFocusChanged: _centerFlickableOnItem(deviceSection)
                onCurrentIndexChanged: _centerFlickableOnItem(deviceSection)
            }

            BrowseDeviceView {
                id: lanSection

                width: root.width
                height: contentHeight

                maximumRows: root.maximumRows

                visible: (model.count !== 0)

                model: NetworkDeviceModel {
                    ctx: MainCtx

                    sd_source: NetworkDeviceModel.CAT_LAN
                    source_name: "*"

                    limit: lanSection.maximumCount

                    sortOrder: MainCtx.sort.order
                    sortCriteria: MainCtx.sort.criteria
                    searchPattern: MainCtx.search.pattern
                }

                title: qsTr("My LAN")

                Navigation.parentItem: root

                Navigation.upAction: function() {
                    if (deviceSection.visible)
                        deviceSection.setCurrentItemFocus(Qt.TabFocusReason)
                    else if (foldersSection.visible)
                        foldersSection.setCurrentItemFocus(Qt.TabFocusReason)
                    else
                        root.Navigation.defaultNavigationUp()
                }

                onBrowse: (tree, reason) => root.browse(tree, reason)

                onSeeAll: (reason) => root.seeAllDevices(title, model.sd_source, reason)

                onActiveFocusChanged: _centerFlickableOnItem(lanSection)
                onCurrentIndexChanged: _centerFlickableOnItem(lanSection)
            }
        }
    }

    function resetFocus() {
        const widgetlist = [foldersSection, deviceSection, lanSection]
        for (let i in widgetlist) {
            if (widgetlist[i].activeFocus && widgetlist[i].visible)
                return
        }

        let found  = false;
        for (let i in widgetlist) {
            if (widgetlist[i].visible && !found) {
                widgetlist[i].focus = true
                found = true
            } else {
                widgetlist[i].focus = false
            }
        }
    }
}
