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
import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQml.Models 2.2
import QtQuick.Layouts 1.11

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/" as Util
import "qrc:///main/" as MainInterface
import "qrc:///style/"

Widgets.PageLoader {
    id: root

    property bool isViewMultiView: false
    property var sortModel
    property var model
    property Component localMenuDelegate: null

    pageModel: [{
        name: "all",
        component: allSourcesComponent
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

    onCurrentItemChanged: {
        sortModel = currentItem.sortModel
        model = currentItem.model
        localMenuDelegate = !!currentItem.addressBar ? currentItem.addressBar : null
        isViewMultiView = currentItem.isViewMultiView === undefined || currentItem.isViewMultiView
    }

    function setCurrentItemFocus(reason) {
        stackView.currentItem.setCurrentItemFocus(reason);
    }

    Component {
        id: sourceRootComponent

        NetworkBrowseDisplay {
            property alias source_name: deviceModel.source_name

            property Component addressBar: NetworkAddressbar {
                path: [{display: deviceModel.name, tree: {}}]

                onHomeButtonClicked: {
                    History.push(["mc", "discover", "services"]);

                    root.setCurrentItemFocus(reason);
                }
            }

            providerModel: deviceModel
            contextMenu: contextMenu

            onBrowse: {
                History.push(["mc", "discover", "services", "source_browse",
                              { tree: tree, "root_name": deviceModel.name,
                                "source_name": source_name }]);

                root.setCurrentItemFocus(reason);
            }

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

        NetworkBrowseDisplay {
            property string root_name
            property string source_name

            property Component addressBar: NetworkAddressbar {
                path: {
                    var _path = providerModel.path
                    _path.unshift({display: root_name, tree: {"source_name": source_name, "isRoot": true}})
                    return _path
                }

                onHomeButtonClicked: {
                    History.push(["mc", "discover", "services"]);

                    root.setCurrentItemFocus(reason);
                }

                onBrowse: {
                    if (!!tree.isRoot)
                        History.push(["mc", "discover", "services", "source_root",
                                      { source_name: tree.source_name }]);
                    else
                        History.push(["mc", "discover", "services", "source_browse",
                                      { tree: tree, "root": root_name }]);

                    root.setCurrentItemFocus(reason);
                }
            }

            onBrowse: {
                History.push(["mc", "discover", "services", "source_browse",
                              { tree: tree, "root": root_name }]);

                root.setCurrentItemFocus(reason);
            }

            providerModel: NetworkMediaModel {
                ctx: MainCtx
            }

            contextMenu: NetworkMediaContextMenu {
                model: providerModel
            }
        }
    }

    Component {
        id: allSourcesComponent

        MainInterface.MainGridView {
            id: gridView

            readonly property bool isViewMultiView: false

            selectionDelegateModel: selectionModel
            model: sourcesFilterModel
            topMargin: VLCStyle.margin_large
            cellWidth: VLCStyle.gridItem_network_width
            cellHeight: VLCStyle.gridCover_network_height + VLCStyle.margin_xsmall + VLCStyle.fontHeight_normal

            delegate: Widgets.GridItem {

                property var model: ({})
                property int index: -1
                readonly property bool is_dummy: model.type === NetworkSourcesModel.TYPE_DUMMY

                title: is_dummy ? I18n.qtr("Add a service") : model.long_name
                subtitle: ""
                pictureWidth: VLCStyle.colWidth(1)
                pictureHeight: VLCStyle.gridCover_network_height
                height: VLCStyle.gridCover_network_height + VLCStyle.margin_xsmall + VLCStyle.fontHeight_normal
                playCoverBorderWidth: VLCStyle.gridCover_network_border
                playCoverShowPlay: false
                image: {
                    if (is_dummy) {
                        return SVGColorImage.colorize("qrc:///placeholder/add_service.svg")
                            .color1(VLCStyle.colors.text)
                            .accent(VLCStyle.colors.accent)
                            .uri()
                    } else if (model.artwork && model.artwork.toString() !== "") {
                        //if the source is a qrc artwork, we should colorize it
                        if (model.artwork.toString().match(/qrc:\/\/.*svg/))
                        {
                            return SVGColorImage.colorize(model.artwork)
                                .color1(VLCStyle.colors.text)
                                .accent(VLCStyle.colors.accent)
                                .uri()
                        }
                        return model.artwork
                    } else {
                        return SVGColorImage.colorize("qrc:///sd/directory.svg")
                                   .color1(VLCStyle.colors.text)
                                   .uri()
                    }
                }

                onItemDoubleClicked: {
                    if (is_dummy)
                        History.push(["mc", "discover", "services", "services_manage"]);
                    else
                        History.push(["mc", "discover", "services", "source_root",
                                      { source_name: model.name }]);

                    root.setCurrentItemFocus(Qt.MouseFocusReason);
                }

                onItemClicked : {
                    selectionModel.updateSelection(modifier , gridView.currentIndex, index)
                    gridView.currentIndex = index
                    gridView.forceActiveFocus()
                }
            }

            onActionAtIndex: {
                var itemData = sourcesFilterModel.getDataAt(index);

                if (itemData.type === NetworkSourcesModel.TYPE_DUMMY)
                    History.push(["mc", "discover", "services", "services_manage"]);
                else
                    History.push(["mc", "discover", "services", "source_root",
                                  { source_name: itemData.name }]);

                root.setCurrentItemFocus(Qt.TabFocusReason);
            }

            Navigation.parentItem: root

            Navigation.cancelAction: function() {
                History.previous();

                root.setCurrentItemFocus(Qt.TabFocusReason);
            }

            NetworkSourcesModel {
                id: sourcesModel

                ctx: MainCtx
            }

            Util.SelectableDelegateModel {
                id: selectionModel

                model: sourcesFilterModel
            }

            SortFilterProxyModel {
                id: sourcesFilterModel

                sourceModel: sourcesModel
                searchRole: "name"
            }
        }
    }
}
