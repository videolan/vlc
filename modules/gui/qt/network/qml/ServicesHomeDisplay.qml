/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import QtQuick.Layouts


import VLC.Widgets as Widgets
import VLC.Util
import VLC.MainInterface
import VLC.Style
import VLC.Network

Widgets.PageLoader {
    id: root

    pageModel: [{
        name: "all",
        default: true,
        component: serviceSourceComponent
    }, {
        name: "services_manage",
        url: "qrc:///qt/qml/VLC/Network/ServicesManage.qml"
    }, {
        name: "source_root",
        component: sourceRootComponent
    }, {
        name: "source_browse",
        component: sourceBrowseComponent,
        guard: function (prop) { return !!prop.tree }
    }]

    property int displayMarginBeginning: 0
    property int displayMarginEnd: 0

    property bool enableBeginningFade: true
    property bool enableEndFade: true

    function _showServiceHome(reason) {
        History.push([...root.pagePrefix, "services"], reason)
    }

    function _showServiceManage(reason) {
        History.push([...root.pagePrefix, "services_manage"], reason)
    }

    function _showServiceRoot(source_name, reason) {
        History.push([...root.pagePrefix, "source_root"], { source_name: source_name }, reason)
    }

    function _showServiceNode(tree, source_name, reason) {
        History.push(
            [...root.pagePrefix, "source_browse"],
            {
                tree: tree,
                source_name: source_name
            },
            reason)
    }

    onCurrentItemChanged: {
        if (currentItem) {
            if (currentItem.displayMarginBeginning !== undefined)
                currentItem.displayMarginBeginning = Qt.binding(() => { return root.displayMarginBeginning })

            if (currentItem.displayMarginEnd !== undefined)
                currentItem.displayMarginEnd = Qt.binding(() => { return root.displayMarginEnd })

            if (currentItem.enableBeginningFade !== undefined)
                currentItem.enableBeginningFade = Qt.binding(() => { return root.enableBeginningFade })

            if (currentItem.enableEndFade !== undefined)
                currentItem.enableEndFade = Qt.binding(() => { return root.enableEndFade })
        }
    }

    Component {
        id: serviceSourceComponent

        ServicesSources {
            onBrowseServiceManage:  (reason) => root._showServiceManage(reason)
            onBrowseSourceRoot: (name, reason) => root. _showServiceRoot(name, reason)
        }
    }


    Component {
        id: sourceRootComponent

        BrowseTreeDisplay {
            property alias source_name: deviceModel.source_name

            property Component localMenuDelegate: NetworkAddressbar {
                path: [{display: deviceModel.name, tree: {}}]

                onHomeButtonClicked: _showServiceHome(reason)
            }

            model: deviceModel
            contextMenu: contextMenu

            onBrowse: (tree, reason) => {
                root._showServiceNode(tree, deviceModel.source_name, reason)
            }

            onCurrentIndexChanged: History.viewProp.initialIndex = currentIndex

            NetworkDeviceModel {
                id: deviceModel

                ctx: MainCtx
                sd_source: NetworkDeviceModel.CAT_INTERNET
            }

            NetworkDeviceContextMenu {
                id: contextMenu

                model: deviceModel
                ctx: MainCtx
            }
        }
    }

    Component {
        id: sourceBrowseComponent

        BrowseTreeDisplay {
            property alias tree: mediaModel.tree
            property string source_name

            property Component localMenuDelegate: NetworkAddressbar {
                path: {
                    const _path = mediaModel.path
                    _path.unshift({display: root_name, tree: {"source_name": source_name, "isRoot": true}})
                    return _path
                }

                onHomeButtonClicked: root._showServiceHome(reason)

                onBrowse: (tree, reason) => {
                    if (tree.isRoot)
                        root._showServiceRoot(source_name, reason)
                    else
                        root._showServiceNode(tree, source_name, reason)
                }
            }

            onBrowse: (tree, reason) => root._showServiceNode(tree, source_name, reason)

            onCurrentIndexChanged: History.viewProp.initialIndex = currentIndex

            model: NetworkMediaModel {
                id: mediaModel
                ctx: MainCtx
            }

            contextMenu: NetworkMediaContextMenu {
                model: mediaModel
                ctx: MainCtx
            }
        }
    }
}
