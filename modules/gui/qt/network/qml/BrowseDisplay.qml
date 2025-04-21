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
import QtQml.Models
import QtQml


import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style
import VLC.Network

Widgets.PageLoader {
    id: root

    property int displayMarginBeginning: 0
    property int displayMarginEnd: 0

    property bool enableBeginningFade: true
    property bool enableEndFade: true

    // Settings

    pageModel: [{
        name: "home",
        default: true,
        component: browseHome
    }, {
        name: "folders",
        component: browseFolders,
    }, {
        name: "device",
        component: browseDevice,
    }, {
        name: "browse",
        component: browseComponent,
        guard: function (prop) { return !!prop.tree }
    }]

    localMenuDelegate: (pageName !== "home") ? componentBar : null

    Accessible.role: Accessible.Client
    Accessible.name: qsTr("Browse view")

    //functions

    function _showBrowseNode(tree, reason) {
        History.push([...root.pagePrefix, "browse"], { tree: tree }, reason)
    }

    function _showHome(reason) {
        History.push([...root.pagePrefix, "home"], reason)
    }


    function _showBrowseFolder(title, reason) {
        History.push([...root.pagePrefix, "folders"], { title: title }, reason)
    }

    function _showBrowseDevices(title, sd_source, reason) {
        History.push([...root.pagePrefix, "device"], { title: title, sd_source: sd_source }, reason)
    }

    // Children
    Component {
        id: browseHome

        BrowseHomeDisplay {
            onSeeAllDevices: (title, sd_source, reason) => {
                root._showBrowseDevices(title, sd_source, reason)
            }

            onSeeAllFolders:(title, reason) => {
                root._showBrowseFolder(title, reason)
            }

            onBrowse: (tree, reason) => {
                root._showBrowseNode(tree, reason)
            }
        }
    }


    Component {
        id: browseFolders

        BrowseDeviceView {
            property var sortModel: [
                { text: qsTr("Alphabetic"), criteria: "name" },
                { text: qsTr("Url"),        criteria: "mrl"  }
            ]

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            enableBeginningFade: root.enableBeginningFade
            enableEndFade: root.enableEndFade

            model: StandardPathModel {
                sortCriteria: MainCtx.sort.criteria
                sortOrder: MainCtx.sort.order
                searchPattern: MainCtx.search.pattern
            }

            onBrowse: (tree, reason) => { root._showBrowseNode(tree, reason) }

            onCurrentIndexChanged: History.viewProp.initialIndex = currentIndex
        }
    }

    Component {
        id: browseDevice

        BrowseDeviceView {
            id: viewDevice

            //@type {NetworkDeviceModel.SDCatType}
            required property int sd_source

            property var sortModel: [
                { text: qsTr("Alphabetic"), criteria: "name" },
                { text: qsTr("Url"),        criteria: "mrl"  }
            ]

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            enableBeginningFade: root.enableBeginningFade
            enableEndFade: root.enableEndFade

            model: NetworkDeviceModel {
                ctx: MainCtx

                sd_source: viewDevice.sd_source
                source_name: "*"

                sortCriteria: MainCtx.sort.criteria
                sortOrder: MainCtx.sort.order
                searchPattern: MainCtx.search.pattern
            }

            onBrowse: (tree, reason) => {
                root._showBrowseNode(tree, reason)
            }

            onCurrentIndexChanged: History.viewProp.initialIndex = currentIndex
        }
    }

    Component {
        id: browseComponent

        BrowseTreeDisplay {

            property alias tree: mediaModel.tree

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            enableBeginningFade: root.enableBeginningFade
            enableEndFade: root.enableEndFade

            model: NetworkMediaModel {
                id: mediaModel

                ctx: MainCtx

                sortCriteria: MainCtx.sort.criteria
                sortOrder: MainCtx.sort.order
                searchPattern: MainCtx.search.pattern
            }

            contextMenu: NetworkMediaContextMenu {
                model: mediaModel
                ctx: MainCtx
            }

            Navigation.cancelAction: function() {
                History.previous(Qt.BacktabFocusReason)
            }

            onBrowse: (tree, reason) => {
                root._showBrowseNode(tree, reason)
            }

            onCurrentIndexChanged: History.viewProp.initialIndex = currentIndex
        }
    }

    Component {
        id: componentBar

        NetworkAddressbar {
            path: root.pageName === "browse" ? root.currentItem.model.path : []

            onHomeButtonClicked: reason => root._showHome(reason)

            onBrowse:  (tree, reason) => { root._showBrowseNode(tree, reason) }
        }
    }
}
