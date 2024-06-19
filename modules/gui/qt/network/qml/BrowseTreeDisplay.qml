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

import org.videolan.vlc 0.1

import VLC.Util
import "qrc:///VLC/Util/Helpers.js" as Helpers
import VLC.Widgets as Widgets
import VLC.MainInterface as MainInterface
import VLC.Style
import VLC.Network

MainInterface.MainViewLoader {
    id: root

    // Properties

    property var contextMenu

    readonly property var currentIndex: _currentView.currentIndex

    readonly property int contentLeftMargin: currentItem?.contentLeftMargin ?? 0
    readonly property int contentRightMargin: currentItem?.contentRightMargin ?? 0

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

    loadingComponent: busyIndicatorComponent

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
            // this is used to provide context to NetworkCustomCover
            // indexData is networkModel (model data) for this index
            // cover is our custom cover that will be loaded insted of default DragItem cover
            return {"indexData": data, "cover": custom_cover}
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

        Component {
            id: custom_cover

            NetworkCustomCover {
                networkModel: model.indexData

                width: networkDragItem.coverSize
                height: networkDragItem.coverSize

                // we can not change the size of cover and shodows from here,
                // so for best visual use scale image to fit
                fillMode: Image.PreserveAspectCrop

                bgColor: networkDragItem.colorContext.bg.secondary
                color1: networkDragItem.colorContext.fg.primary
                accent: networkDragItem.colorContext.accent
            }
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

            headerDelegate: BrowseTreeHeader {
                providerModel: root.model

                leftPadding: root.contentLeftMargin
                rightPadding: root.contentRightMargin

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

        MainInterface.MainTableView {
            id: tableView

            property Component thumbnailHeader: Widgets.TableHeaderDelegate {
                Widgets.IconLabel {
                    height: VLCStyle.listAlbumCover_height
                    width: VLCStyle.listAlbumCover_width

                    anchors.centerIn: parent

                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: VLCStyle.icon_tableHeader
                    text: VLCIcons.album_cover
                    color: parent.colorContext.fg.secondary
                }
            }

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

                    headerDelegate: thumbnailHeader,
                    colDelegate: thumbnailColumn
                })
            }]

            property var _modelMedium: [{
                size: 1,

                model: {
                    criteria: "thumbnail",

                    text: qsTr("Cover"),

                    isSortable: false,

                    headerDelegate: thumbnailHeader,
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

            header: BrowseTreeHeader {
                providerModel: root.model

                leftPadding: root.contentLeftMargin
                rightPadding: root.contentRightMargin

                width: tableView.width

                Navigation.parentItem: root
                Navigation.downAction: function () {
                    focus = false
                    tableView.forceActiveFocus(Qt.TabFocusReason)
                }
            }

            onActionForSelection: (selection) => _actionAtIndex(selection[0].row)
            onItemDoubleClicked: (index, model) => _actionAtIndex(index)
            onContextMenuButtonClicked: (_,_,globalMousePos) => {
                contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
            }
            onRightClick: (_,_,globalMousePos) => {
                contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
            }

            Widgets.TableColumns {
                id: tableColumns
            }
        }
    }

    Component {
        id: emptyLabelComponent

        StandardView {
            view: Widgets.EmptyLabelButton {
                id: emptyLabel

                visible: !root.isLoading

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

                Layout.fillHeight: true
                Layout.fillWidth: true

                Navigation.parentItem: root
            }
        }
    }

    Component {
        id: busyIndicatorComponent

        StandardView {
            view: Item {
                Navigation.navigable: false

                visible: root.isLoading

                Layout.fillHeight: true
                Layout.fillWidth: true

                Widgets.BusyIndicatorExt {
                    id: busyIndicator

                    runningDelayed: root.isLoading
                    anchors.centerIn: parent
                    z: 1
                }
            }
        }
    }

    // Helper view i.e a ColumnLayout with BrowseHeader
    component StandardView : FocusScope {
        required property Item view

        // NOTE: This is required to pass the focusReason when the current view changes in
        //       MainViewLoader.
        property int focusReason: (header.activeFocus) ? header.focusReason
                                                       : view?.focusReason ?? Qt.NoFocusReason

        // used by MainDisplay to transfer focus
        function setCurrentItemFocus(reason) {
            if (!Navigation.navigable)
                return

            if (header.Navigation.navigable)
                Helpers.enforceFocus(header, reason)
            else
                Helpers.enforceFocus(view, reason)
        }

        onViewChanged: {
            if (layout.children.length === 2)
                layout.children.pop()

            layout.children.push(view)
            view.Navigation.upAction = function () {
                // FIXME: for some reason default navigation flow doesn't work
                // i.e setting Navigtaion.upItem doesn't fallthrough to parent's
                // action if it's navigable is false

                if (header.Navigation.navigable)
                    header.forceActiveFocus(Qt.BacktabFocusReason)
                else
                    return false // fallthrough default action
            }
        }

        ColumnLayout {
            id: layout

            anchors.fill: parent

            BrowseTreeHeader {
                id: header

                focus: true

                providerModel: root.model

                Layout.fillWidth: true

                Navigation.parentItem: root
                Navigation.downItem: (view.Navigation.navigable) ? view : null
            }
        }
    }
}
