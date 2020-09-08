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

import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/" as Util
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root

    property string view: "all"
    property var viewProperties: ({})

    property var sortModel
    property var model

    readonly property var pageModel: [{
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
        component: sourceBrowseComponent
    }]

    Component.onCompleted: loadView()
    onViewChanged: {
        viewProperties = {}
        loadView()
    }
    onViewPropertiesChanged: loadView()

    function loadDefaultView() {
        root.view = "all"
        root.viewProperties = ({})
    }

    function loadView() {
        var found = stackView.loadView(root.pageModel, view, viewProperties)
        if (!found)
            stackView.replace(root.pageModel[0].component)
        stackView.currentItem.navigationParent = root
    }

    Component {
        id: sourceRootComponent

        NetworkBrowseDisplay {
            property alias source_name: deviceModel.source_name

            providerModel: deviceModel

            function changeTree(new_tree) {
                history.push(["mc", "discover", "services", "source_browse", { tree: new_tree }]);
            }

            NetworkDeviceModel {
                id: deviceModel

                ctx: mainctx
                sd_source: NetworkDeviceModel.CAT_INTERNET
            }
        }
    }

    Component {
        id: sourceBrowseComponent
        NetworkBrowseDisplay {

            providerModel: mediaModel

            function changeTree(new_tree) {
                history.push(["mc", "discover", "services", "source_browse", { tree: new_tree }]);
            }

            NetworkMediaModel {
                id: mediaModel

                ctx: mainctx
            }
        }
    }


    Component {
        id: servicesManageComponent

        Widgets.KeyNavigableListView {
            id: servicesView

            model: discoveryModel
            topMargin: VLCStyle.margin_large
            leftMargin: VLCStyle.margin_large
            rightMargin: VLCStyle.margin_large
            spacing: VLCStyle.margin_xsmall

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

                        width: VLCIcons.pixelSize(VLCStyle.icon_large)
                        height: VLCIcons.pixelSize(VLCStyle.icon_large)
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
                                    text: model.author ? i18n.qtr("by <b>%1</b>").arg(model.author) : i18n.qtr("by <b>Unknown</b>")
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
                                        return i18n.qtr("Remove")
                                    case ServicesDiscoveryModel.NOTINSTALLED:
                                        return i18n.qtr("Install")
                                    case ServicesDiscoveryModel.INSTALLING:
                                        return i18n.qtr("Installing")
                                    case ServicesDiscoveryModel.UNINSTALLING:
                                        return i18n.qtr("Uninstalling")
                                    }
                                }

                                onClicked: {
                                    if (model.state === ServicesDiscoveryModel.NOTINSTALLED)
                                        discoveryModel.installService(index)
                                    else if (model.state === ServicesDiscoveryModel.INSTALLED)
                                        discoveryModel.removeService(index)
                                }
                            }
                        }

                        Widgets.CaptionLabel {
                            elide: Text.ElideRight
                            text:  model.description || model.summary || i18n.qtr("No information available")
                            topPadding: VLCStyle.margin_xsmall
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            Layout.preferredHeight: implicitHeight
                        }

                        Widgets.CaptionLabel {
                            text: i18n.qtr("Score: %1/5  Downloads: %2").arg(model.score).arg(model.downloads)
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

                ctx: mainctx
            }

        }
    }

    Component {
        id: allSourcesComponent

        Widgets.ExpandGridView {
            id: gridView

            delegateModel: selectionModel
            model: sourcesModel
            topMargin: VLCStyle.margin_large
            cellWidth: VLCStyle.gridItem_network_width
            cellHeight: VLCStyle.gridCover_network_height + VLCStyle.margin_xsmall + VLCStyle.fontHeight_normal

            delegate: Widgets.GridItem {

                property var model: ({})
                property int index: -1
                readonly property bool is_dummy: model.type === NetworkSourcesModel.TYPE_DUMMY

                title: is_dummy ? i18n.qtr("Add a service") : model.long_name
                subtitle: ""
                pictureWidth: VLCStyle.colWidth(1)
                pictureHeight: VLCStyle.gridCover_network_height
                height: VLCStyle.gridCover_network_height + VLCStyle.margin_xsmall + VLCStyle.fontHeight_normal
                playCoverBorder.width: VLCStyle.gridCover_network_border
                playCoverOnlyBorders: true
                pictureOverlay: overlay

                onItemDoubleClicked: {
                    if (is_dummy)
                        history.push(["mc", "discover", "services", "services_manage"])
                    else
                        history.push(["mc", "discover", "services", "source_root", { source_name: model.name } ])
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
                            source:  model.artwork || "qrc:///type/directory_black.svg"
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
                                    font.pixelSize: VLCIcons.pixelSize(VLCStyle.icon_large)
                                    anchors.centerIn: parent
                                    color: VLCStyle.colors.accent
                                }
                            }
                        }
                    }
                }

            }

            onSelectAll: selectionModel.selectAll()
            onSelectionUpdated: selectionModel.updateSelection( keyModifiers, oldIndex, newIndex )
            onActionAtIndex: {
                var itemData = sourcesModel.getDataAt(index)
                if (itemData.type === NetworkSourcesModel.TYPE_DUMMY)
                    history.push(["mc", "discover", "services", "services_manage"])
                else
                    history.push(["mc", "discover", "services", "source_root", { source_name: itemData.name } ])
            }

            navigationParent: root
            navigationCancel: function() {
                history.previous()
            }

            NetworkSourcesModel {
                id: sourcesModel

                ctx: mainctx
            }

            Util.SelectableDelegateModel {
                id: selectionModel

                model: sourcesModel
            }
        }
    }



    Widgets.StackViewExt {
        id: stackView

        anchors.fill: parent
        focus: true
    }
}
