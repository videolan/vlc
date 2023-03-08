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
import QtQml.Models 2.2
import QtQml 2.11

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///style/"

Widgets.PageLoader {
    id: root

    // Properties

    property var sortModel
    property var contentModel
    property bool isViewMultiView: true

    property Component localMenuDelegate

    // Settings

    pageModel: [{
        name: "home",
        url: "qrc:///network/BrowseHomeDisplay.qml"
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

    loadDefaultView: function() {
        History.update(["mc", "network", "home"])
        loadPage("home")
    }

    Accessible.role: Accessible.Client
    Accessible.name: I18n.qtr("Browse view")

    // Events
    onCurrentItemChanged: {
        sortModel = currentItem.sortModel;
        contentModel = currentItem.model;

        isViewMultiView = (currentItem.isViewMultiView === undefined
                           ||
                           currentItem.isViewMultiView);

        if (view.name === "home")
            localMenuDelegate = null
        else
            localMenuDelegate = componentBar
    }

    // Connections
    Connections {
        target: (Helpers.isValidInstanceOf(stackViewItem, BrowseHomeDisplay)) ? stackViewItem
                                                                              : null

        onSeeAll: {
            if (sd_source === -1)
                History.push(["mc", "network", "folders", { title: title }])
            else
                History.push(["mc", "network", "device", { title: title, sd_source: sd_source }])

            stackViewItem.setCurrentItemFocus(reason)
        }
    }

    Connections {
        target: stackViewItem

        onBrowse: {
            History.push(["mc", "network", "browse", { tree: tree }])

            stackViewItem.setCurrentItemFocus(reason)
        }
    }

    // Children

    Component {
        id: browseFolders

        BrowseDeviceView {
            property var sortModel: [
                { text: I18n.qtr("Alphabetic"), criteria: "name" },
                { text: I18n.qtr("Url"),        criteria: "mrl"  }
            ]

            displayMarginEnd: g_mainDisplay.displayMargin

            model: modelFilter

            sourceModel: StandardPathModel {}
        }
    }

    Component {
        id: browseDevice

        BrowseDeviceView {
            id: viewDevice

            property var sd_source

            property var sortModel: [
                { text: I18n.qtr("Alphabetic"), criteria: "name" },
                { text: I18n.qtr("Url"),        criteria: "mrl"  }
            ]

            displayMarginEnd: g_mainDisplay.displayMargin

            model: modelFilter

            sourceModel: NetworkDeviceModel {
                ctx: MainCtx

                sd_source: viewDevice.sd_source
                source_name: "*"
            }
        }
    }

    Component {
        id: browseComponent

        BrowseTreeDisplay {
            providerModel: NetworkMediaModel {
                ctx: MainCtx
            }

            contextMenu: NetworkMediaContextMenu {
                model: providerModel
            }

            Navigation.cancelAction: function() {
                History.previous()

                stackViewItem.setCurrentItemFocus(Qt.BacktabFocusReason)
            }
        }
    }

    Component {
        id: componentBar

        NetworkAddressbar {
            path: view.name === "browse" ? root.stackViewItem.providerModel.path : []

            onHomeButtonClicked: {
                History.push(["mc", "network", "home"])

                stackViewItem.setCurrentItemFocus(reason)
            }

            onBrowse: {
                History.push(["mc", "network", "browse", { "tree": tree }])

                stackViewItem.setCurrentItemFocus(reason)
            }
        }
    }
}
