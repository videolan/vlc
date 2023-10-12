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
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQml.Models 2.12
import QtQuick.Layouts 1.12

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/" as Util
import "qrc:///main/" as MainInterface
import "qrc:///style/"

Widgets.PageLoader {
    id: root

    pageModel: [{
        name: "all",
        url: "qrc:///network/ServicesSources.qml"
    }, {
        name: "services_manage",
        url: "qrc:///network/ServicesManage.qml"
    }, {
        name: "source_root",
        component: sourceRootComponent
    }, {
        name: "source_browse",
        component: sourceBrowseComponent,
        guard: function (prop) { return !!prop.tree }
    }]

    loadDefaultView: function() {
        History.update(["mc", "discover", "services", "all"])
        loadPage("all")
    }

    function setCurrentItemFocus(reason) {
        stackView.currentItem.setCurrentItemFocus(reason);
    }

    Component {
        id: sourceRootComponent

        BrowseTreeDisplay {
            property alias source_name: deviceModel.source_name

            property Component localMenuDelegate: NetworkAddressbar {
                path: [{display: deviceModel.name, tree: {}}]

                onHomeButtonClicked: History.push(["mc", "discover", "services"], reason)
            }

            model: deviceModel
            contextMenu: contextMenu

            onBrowse: History.push(["mc", "discover", "services", "source_browse"],
                                    { tree: tree, "root_name": deviceModel.name,
                                      "source_name": source_name }, reason)

            NetworkDeviceModel {
                id: deviceModel

                ctx: MainCtx
                sd_source: NetworkDeviceModel.CAT_INTERNET
            }

            NetworkDeviceContextMenu {
                id: contextMenu

                model: deviceModel
            }
        }
    }

    Component {
        id: sourceBrowseComponent

        BrowseTreeDisplay {
            property string root_name
            property string source_name

            property Component localMenuDelegate: NetworkAddressbar {
                path: {
                    const _path = mediaModel.path
                    _path.unshift({display: root_name, tree: {"source_name": source_name, "isRoot": true}})
                    return _path
                }

                onHomeButtonClicked: History.push(["mc", "discover", "services"], reason)

                onBrowse: {
                    if (!!tree.isRoot)
                        History.push(["mc", "discover", "services", "source_root"],
                                      { source_name: tree.source_name }, reason)
                    else
                        History.push(["mc", "discover", "services", "source_browse"],
                                      { tree: tree, "root": root_name }, reason)
                }
            }

            onBrowse: History.push(["mc", "discover", "services", "source_browse"],
                                    { tree: tree, "root": root_name }, reason)

            model: NetworkMediaModel {
                id: mediaModel
                ctx: MainCtx
            }

            contextMenu: NetworkMediaContextMenu {
                model: mediaModel
            }
        }
    }
}
