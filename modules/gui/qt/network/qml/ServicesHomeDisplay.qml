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
import QtQuick.Shapes 1.0

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
        component: servicesManageComponent
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
        id: servicesManageComponent

        Widgets.KeyNavigableListView {
            id: servicesView

            readonly property bool isViewMultiView: false

            model: discoveryFilterModel
            topMargin: VLCStyle.margin_large
            leftMargin: VLCStyle.margin_large
            rightMargin: VLCStyle.margin_large
            spacing: VLCStyle.margin_xsmall

            // To get blur effect while scrolling in mainview
            displayMarginEnd: g_mainDisplay.displayMargin

            delegate: Rectangle {
                width: servicesView.width - VLCStyle.margin_large * 2
                height: row.implicitHeight + VLCStyle.margin_small * 2
                color: VLCStyle.colors.bgAlt

                onActiveFocusChanged: if (activeFocus) action_btn.forceActiveFocus()

                RowLayout {
                    id: row

                    spacing: VLCStyle.margin_xsmall
                    anchors.fill: parent
                    anchors.margins: VLCStyle.margin_small

                    Image {

                        width: VLCStyle.icon_large
                        height: VLCStyle.icon_large
                        fillMode: Image.PreserveAspectFit
                        source: model.artwork

                        Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                    }

                    ColumnLayout {
                        id: content

                        spacing: 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        RowLayout {
                            spacing: 0

                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            Column {
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                Widgets.SubtitleLabel {
                                    text: model.name
                                    width: parent.width
                                }

                                Widgets.CaptionLabel {
                                    color: VLCStyle.colors.text
                                    text: model.author ? I18n.qtr("by <b>%1</b>").arg(model.author) : I18n.qtr("by <b>Unknown</b>")
                                    topPadding: VLCStyle.margin_xxxsmall
                                    width: parent.width
                                }
                            }

                            Widgets.TabButtonExt {
                                id: action_btn

                                focus: true
                                iconTxt: model.state === ServicesDiscoveryModel.INSTALLED ? VLCIcons.del : VLCIcons.add
                                busy: model.state === ServicesDiscoveryModel.INSTALLING || model.state === ServicesDiscoveryModel.UNINSTALLING
                                text: {
                                    switch(model.state) {
                                    case ServicesDiscoveryModel.INSTALLED:
                                        return I18n.qtr("Remove")
                                    case ServicesDiscoveryModel.NOTINSTALLED:
                                        return I18n.qtr("Install")
                                    case ServicesDiscoveryModel.INSTALLING:
                                        return I18n.qtr("Installing")
                                    case ServicesDiscoveryModel.UNINSTALLING:
                                        return I18n.qtr("Uninstalling")
                                    }
                                }

                                onClicked: {
                                    if (model.state === ServicesDiscoveryModel.NOTINSTALLED)
                                        discoveryModel.installService(discoveryFilterModel.mapIndexToSource(index))
                                    else if (model.state === ServicesDiscoveryModel.INSTALLED)
                                        discoveryModel.installService(discoveryFilterModel.mapIndexToSource(index))
                                }
                            }
                        }

                        Widgets.CaptionLabel {
                            elide: Text.ElideRight
                            text:  model.description || model.summary || I18n.qtr("No information available")
                            topPadding: VLCStyle.margin_xsmall
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            Layout.preferredHeight: implicitHeight
                        }

                        Widgets.CaptionLabel {
                            text: I18n.qtr("Score: %1/5  Downloads: %2").arg(model.score).arg(model.downloads)
                            topPadding: VLCStyle.margin_xsmall
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            Widgets.BusyIndicatorExt {
                runningDelayed: discoveryModel.parsingPending
                anchors.centerIn: parent
                z: 1
            }

            ServicesDiscoveryModel {
                id: discoveryModel

                ctx: MainCtx
            }

            SortFilterProxyModel {
                id: discoveryFilterModel

                sourceModel: discoveryModel
                searchRole: "name"
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
                pictureOverlay: overlay

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

                Component {
                    id: overlay

                    Item {
                        Image {
                            x: (pictureWidth - paintedWidth) / 2
                            y: (pictureHeight - paintedWidth) / 2
                            width: VLCStyle.icon_large
                            height: VLCStyle.icon_large
                            fillMode: Image.PreserveAspectFit
                            source:  model.artwork || "qrc:///sd/directory.svg"
                            visible: !is_dummy
                        }


                        Loader {
                            anchors.fill: parent
                            active: is_dummy
                            visible: is_dummy
                            sourceComponent: Item {
                                Shape {
                                    id: shape

                                    x: 1
                                    y: 1
                                    width: parent.width - 2
                                    height: parent.height - 2

                                    ShapePath {
                                        strokeColor: VLCStyle.colors.setColorAlpha(VLCStyle.colors.text, .62)
                                        strokeWidth: VLCStyle.dp(1, VLCStyle.scale)
                                        dashPattern: [VLCStyle.dp(2, VLCStyle.scale), VLCStyle.dp(4, VLCStyle.scale)]
                                        strokeStyle: ShapePath.DashLine
                                        fillColor: VLCStyle.colors.setColorAlpha(VLCStyle.colors.bg, .62)
                                        startX: 1
                                        startY: 1
                                        PathLine { x: shape.width ; y: 1 }
                                        PathLine { x: shape.width ; y: shape.height }
                                        PathLine { x: 1; y: shape.height }
                                        PathLine { x: 1; y: 1 }
                                    }
                                }

                                Widgets.IconLabel {
                                    text: VLCIcons.add
                                    font.pixelSize: VLCStyle.icon_large
                                    anchors.centerIn: parent
                                    color: VLCStyle.colors.accent
                                }
                            }
                        }
                    }
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
