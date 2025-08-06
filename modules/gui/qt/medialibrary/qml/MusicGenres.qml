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

import VLC.MediaLibrary

import VLC.Util
import VLC.Widgets as Widgets
import VLC.MainInterface
import VLC.Style

MainViewLoader {
    id: root

    // Properties
    readonly property var currentIndex: currentItem?.currentIndex ?? - 1

    property Component header: null

    readonly property int contentLeftMargin: currentItem?.contentLeftMargin ?? 0
    readonly property int contentRightMargin: currentItem?.contentRightMargin ?? 0

    property int displayMarginBeginning: 0
    property int displayMarginEnd: 0

    // Currently only respected by the list view:
    property bool enableBeginningFade: true
    property bool enableEndFade: true

    property alias searchPattern: genreModel.searchPattern
    property alias sortOrder: genreModel.sortOrder
    property alias sortCriteria: genreModel.sortCriteria

    // FIXME: remove this
    property var _currentView: currentItem

    signal showAlbumView(var id, string name, int reason)

    isSearchable: true
    model: genreModel

    sortModel: [
        { text: qsTr("Alphabetic"), criteria: "name" }
    ]

    list: tableComponent
    grid: gridComponent
    emptyLabel: emptyLabelComponent

    MLGenreModel {
        id: genreModel
        ml: MediaLib

        coverDefault: VLCStyle.noArtAlbumCover

        onCountChanged: {
            if (genreModel.count > 0 && !selectionModel.hasSelection) {
                root.resetFocus()
            }
        }
    }

    function _actionAtIndex(index) {
        if (selectionModel.selectedIndexes.length > 1) {
            model.addAndPlay( selectionModel.selectedIndexes )
        } else if (selectionModel.selectedIndexes.length === 1) {
            const sel = selectionModel.selectedIndexes[0]
            const model = genreModel.getDataAt(sel)
            showAlbumView(model.id, model.name, Qt.TabFocusReason)
        }
    }

    Widgets.MLDragItem {
        id: genreDragItem

        view: root.currentItem

        indexes: indexesFlat ? selectionModel.selectedIndexesFlat
                             : selectionModel.selectedIndexes
        indexesFlat: !!selectionModel.selectedIndexesFlat
    }

    /*
     *define the initial position/selection
     * This is done on activeFocus rather than Component.onCompleted because selectionModel.
     * selectedGroup update itself after this event
     */
    onActiveFocusChanged: {
        if (activeFocus && genreModel.count > 0 && !selectionModel.hasSelection) {
            let initialIndex = 0
            if (currentIndex !== -1)
                initialIndex = currentIndex

            selectionModel.select(genreModel.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect)
            currentItem.currentIndex = initialIndex
        }
    }

    MLContextMenu {
        id: contextMenu

        model: genreModel
    }

    /* Grid View */
    Component {
        id: gridComponent
        Widgets.ExpandGridItemView {
            id: gridView_id

            basePictureWidth: VLCStyle.gridCover_video_width
            basePictureHeight: VLCStyle.gridCover_video_width / 2
            titleHeight: 0
            subtitleHeight: 0

            selectionModel: root.selectionModel
            model: genreModel

            headerDelegate: root.header

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            delegate: Widgets.GridItem {
                id: genreGridDelegate

                property var model: ({})
                property int index: -1

                width: gridView_id.cellWidth
                height: gridView_id.cellHeight

                pictureWidth: gridView_id.maxPictureWidth
                pictureHeight: gridView_id.maxPictureHeight

                image: model.cover || ""
                cacheImage: true // for this view, we generate custom covers, cache it

                fallbackImage: VLCStyle.noArtAlbumCover

                dragItem: genreDragItem

                onItemDoubleClicked: root.showAlbumView(model.id, model.name, Qt.MouseFocusReason)
                onItemClicked: (modifier) => { gridView_id.leftClickOnItem(modifier, index) }

                onPlayClicked: {
                    if (model.id)
                        MediaLib.addAndPlay(model.id)
                }

                onContextMenuButtonClicked: (_, globalMousePos) => {
                    gridView_id.rightClickOnItem(index)
                    contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
                }

                pictureOverlay: Item {
                    id: overlay

                    Rectangle {
                        anchors.fill: parent

                        radius: VLCStyle.gridCover_radius

                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.3) }
                            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.7) }
                        }
                    }

                    Column {
                        anchors.centerIn: parent

                        //FIXME use the right xxxLabel class
                        T.Label {
                            width: overlay.width
                            elide: Text.ElideRight
                            font.pixelSize: VLCStyle.fontSize_large
                            font.weight: Font.DemiBold
                            text: model.name || qsTr("Unknown genre")
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Widgets.CaptionLabel {
                            width: overlay.width
                            text: model.nb_tracks > 1 ? qsTr("%1 Tracks").arg(model.nb_tracks) : qsTr("%1 Track").arg(model.nb_tracks)
                            opacity: .7
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }

            focus: true

            onActionAtIndex: (index) => { _actionAtIndex(index) }

            Navigation.parentItem: root
        }
    }

    Component {
        id: tableComponent
        /* Table View */
        Widgets.TableViewExt {
            id: tableView_id

            property var _modelSmall: [{
                weight: 1,

                model: {
                    criteria: "name",

                    subCriterias: [ "nb_tracks" ],

                    text: qsTr("Name"),

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: tableColumns.titleDelegate
                }
            }]

            property var _modelMedium: [{
                weight: 1,

                model: {
                    criteria: "name",

                    text: qsTr("Name"),

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: tableColumns.titleDelegate
                }
            }, {
                size: 1,

                model: {
                    criteria: "nb_tracks",

                    text: qsTr("Tracks"),

                    isSortable: false
                }
            }]

            model: genreModel

            sortModel: (availableRowWidth < VLCStyle.colWidth(4)) ? _modelSmall
                                                                  : _modelMedium

            selectionModel: root.selectionModel
            focus: true
            onActionForSelection: (selection) => _actionAtIndex(selection)
            Navigation.parentItem: root
            dragItem: genreDragItem
            rowHeight: VLCStyle.tableCoverRow_height

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            fadingEdge.enableBeginningFade: root.enableBeginningFade
            fadingEdge.enableEndFade: root.enableEndFade

            header: root.header

            rowContextMenu: contextMenu

            onItemDoubleClicked: (index, model) => {
                root.showAlbumView(model.id, model.name, Qt.MouseFocusReason)
            }

            onRightClick: (_,_,globalMousePos) => {
                contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
            }

            Widgets.MLTableColumns {
                id: tableColumns

                showCriterias: (tableView_id.sortModel === tableView_id._modelSmall)

                titleCover_height: VLCStyle.listAlbumCover_height
                titleCover_width: VLCStyle.listAlbumCover_width
                titleCover_radius: VLCStyle.listAlbumCover_radius
            }
        }
    }


    Component {
        id: emptyLabelComponent

        Widgets.EmptyLabelButton {
            text: qsTr("No genres found\nPlease try adding sources")
            Navigation.parentItem: root
            cover: VLCStyle.noArtAlbumCover
        }
    }
}
