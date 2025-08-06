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
import QtQuick.Layouts


import VLC.Util
import VLC.Widgets as Widgets
import VLC.MainInterface
import VLC.Style
import VLC.Network

MainViewLoader {
    id: root

    // Properties

    property var contextMenu

    readonly property var currentIndex: _currentView.currentIndex

    readonly property int contentLeftMargin: currentItem?.contentLeftMargin ?? 0
    readonly property int contentRightMargin: currentItem?.contentRightMargin ?? 0

    property int displayMarginBeginning: 0
    property int displayMarginEnd: 0

    // Currently only respected by the list view:
    property bool enableBeginningFade: true
    property bool enableEndFade: true

    readonly property int extraMargin: VLCStyle.dynamicAppMargins(width)

    readonly property int headerLeftPadding: (contentLeftMargin > 0)
                                             ? contentLeftMargin : extraMargin + VLCStyle.layout_left_margin
    readonly property int headerRightPadding: (contentRightMargin > 0)
                                              ? contentRightMargin : extraMargin + VLCStyle.layout_right_margin

    // fixme remove this
    property Item _currentView: currentItem

    signal browse(var tree, int reason)

    // Settings

    isSearchable: true

    sortModel: [
        { text: qsTr("Alphabetic"), criteria: "name"},
        { text: qsTr("Url"), criteria: "mrl" },
        { text: qsTr("File size"), criteria: "fileSizeRaw64" },
        { text: qsTr("File modified"), criteria: "fileModified" }
    ]

    grid: gridComponent
    list: tableComponent

    emptyLabel: emptyLabelComponent

    Navigation.cancelAction: function() {
        History.previous(Qt.BacktabFocusReason)
    }

    function playSelected() {
        model.addAndPlay(selectionModel.selectedIndexes)
    }

    function playAt(index) {
        model.addAndPlay(index)
    }

    function _actionAtIndex(index) {
        if ( selectionModel.selectedIndexes.length > 1 ) {
            playSelected()
        } else {
            const data = model.getDataAt(index)
            if (data.type === NetworkMediaModel.TYPE_DIRECTORY
                    || data.type === NetworkMediaModel.TYPE_NODE)  {
                browse(data.tree, Qt.TabFocusReason)
            } else {
                playAt(index)
            }
        }
    }

    Widgets.DragItem {
        id: networkDragItem

        indexes: selectionModel.selectedIndexes

        defaultText:  qsTr("Unknown Share")

        coverProvider: function(index, data) {
            const fallbackImage = SVGColorImage.colorize(data.artworkFallback)
                .background(networkDragItem.colorContext.bg.secondary)
                .color1(networkDragItem.colorContext.fg.primary)
                .accent(networkDragItem.colorContext.accent)
                .uri()

            return {
                artwork: data.artwork,
                fallback: fallbackImage,
                textureProvider: root.currentItem.itemAtIndex(index)?.artworkTextureProvider
            }
        }

        onRequestData: (indexes, resolve, reject) => {
            resolve(
                indexes.map(x => model.getDataAt(x.row))
            )
        }

        onRequestInputItems: (indexes, data, resolve, reject) => {
            resolve(
                model.getItemsForIndexes(indexes)
            )
        }
    }

    Component{
        id: gridComponent

        Widgets.ExpandGridItemView {
            id: gridView

            basePictureWidth: VLCStyle.gridCover_network_width
            basePictureHeight: VLCStyle.gridCover_network_height
            subtitleHeight: 0

            maxNbItemPerRow: 12

            selectionModel: root.selectionModel
            model: root.model

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            headerDelegate: BrowseTreeHeader {
                providerModel: root.model

                leftPadding: root.headerLeftPadding
                rightPadding: root.headerRightPadding

                width: gridView.width

                Navigation.parentItem: root
                Navigation.downAction: function () {
                    focus = false
                    gridView.forceActiveFocus(Qt.TabFocusReason)
                }
            }

            delegate: NetworkGridItem {
                id: delegateGrid

                width: gridView.cellWidth;
                height: gridView.cellHeight;

                pictureWidth: gridView.maxPictureWidth
                pictureHeight: gridView.maxPictureHeight

                subtitle: ""
                dragItem: networkDragItem

                onPlayClicked: playAt(index)
                onItemClicked : (modifier) => {
                    gridView.leftClickOnItem(modifier, index)
                }

                onItemDoubleClicked: {
                    if (model.type === NetworkMediaModel.TYPE_NODE || model.type === NetworkMediaModel.TYPE_DIRECTORY)
                        browse(model.tree, Qt.MouseFocusReason)
                    else
                        playAt(index)
                }

                onContextMenuButtonClicked: (_, globalMousePos) => {
                    gridView.rightClickOnItem(index)
                    contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
                }
            }

            onActionAtIndex: (index) => { _actionAtIndex(index) }

            Navigation.parentItem: root
            Navigation.upItem: gridView.headerItem
        }
    }

    Component{
        id: tableComponent

        Widgets.TableViewExt {
            id: tableView

            property Component thumbnailColumn: NetworkThumbnailItem {
                onPlayClicked: index => playAt(index)
            }

            property var _modelSmall: [{
                weight: 1,

                model: ({
                    criteria: "name",

                    title: "name",

                    subCriterias: [ "mrl" ],

                    text: qsTr("Name"),

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: thumbnailColumn
                })
            }]

            property var _modelMedium: [{
                size: 1,

                model: {
                    criteria: "thumbnail",

                    text: qsTr("Cover"),

                    isSortable: false,

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: thumbnailColumn
                }
            }, {
                weight: 1,

                model: {
                    criteria: "name",

                    text: qsTr("Name")
                }
            }, {
                weight: 1,

                model: {
                    criteria: "mrl",

                    text: qsTr("Url"),

                    showContextButton: true
                }
            }, {
                size: 1,

                model: {
                    criteria: "duration",

                    text: qsTr("Duration"),

                    showContextButton: true,
                    headerDelegate: tableColumns.timeHeaderDelegate,
                    colDelegate: tableColumns.timeColDelegate
                }
            }]

            dragItem: networkDragItem

            model: root.model

            sortModel: (availableRowWidth < VLCStyle.colWidth(4)) ? _modelSmall
                                                                  : _modelMedium

            selectionModel: root.selectionModel
            focus: true

            Navigation.parentItem: root
            Navigation.upItem: tableView.headerItem

            rowHeight: VLCStyle.tableCoverRow_height

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            fadingEdge.enableBeginningFade: root.enableBeginningFade
            fadingEdge.enableEndFade: root.enableEndFade

            header: BrowseTreeHeader {
                providerModel: root.model

                leftPadding: root.headerLeftPadding
                rightPadding: root.headerRightPadding

                width: tableView.width

                Navigation.parentItem: root
                Navigation.downAction: function () {
                    focus = false
                    tableView.forceActiveFocus(Qt.TabFocusReason)
                }
            }

            rowContextMenu: contextMenu

            onActionForSelection: (selection) => _actionAtIndex(selection[0].row)
            onItemDoubleClicked: (index, model) => _actionAtIndex(index)

            onRightClick: (_,_,globalMousePos) => {
                contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
            }

            Widgets.TableColumns {
                id: tableColumns

                titleCover_width: VLCStyle.listAlbumCover_width
                titleCover_height: VLCStyle.listAlbumCover_height

                showTitleText: false
            }
        }
    }

    Component {
        id: emptyLabelComponent

        Widgets.EmptyLabelButton {
            id: emptyLabel

            background: Rectangle {
                // NOTE: This is necessary because MainViewLoader may position this indicator over the shown header when height is small.
                border.color: emptyLabel.colorContext.border
                radius: VLCStyle.dp(6, VLCStyle.scale)
                color: emptyLabel.colorContext.bg.primary
                opacity: 0.8
            }

            // FIXME: find better cover
            cover: VLCStyle.noArtVideoCover
            coverWidth : VLCStyle.dp(182, VLCStyle.scale)
            coverHeight: VLCStyle.dp(114, VLCStyle.scale)

            text: qsTr("Nothing to see here, go back.")

            button.iconTxt: VLCIcons.back
            button.text: qsTr("Back")
            button.enabled: !History.previousEmpty
            button.width: button.implicitWidth

            function onNavigate(reason) {
                History.previous(reason)
            }

            Navigation.parentItem: root
        }
    }
}
