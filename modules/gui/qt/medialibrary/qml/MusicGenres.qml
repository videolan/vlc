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
import QtQuick.Templates 2.4 as T
import QtQml.Models 2.2

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///util/" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///main/" as MainInterface
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///style/"

MainInterface.MainViewLoader {
    id: root

    // Properties

    property var sortModel: [
        { text: I18n.qtr("Alphabetic"), criteria: "title" }
    ]

    readonly property var currentIndex: Helpers.get(currentItem, "currentIndex", - 1)

    // FIXME: remove this
    property var _currentView: currentItem

    signal showAlbumView(var id, string name, int reason)

    model: genreModel

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
            MediaLib.addAndPlay(model.getIdsForIndexes(selectionModel.selectedIndexes))
        } else if (selectionModel.selectedIndexes.length === 1) {
            var sel = selectionModel.selectedIndexes[0]
            var model = genreModel.getDataAt(sel)
            showAlbumView(model.id, model.name, Qt.TabFocusReason)
        }
    }

    Widgets.MLDragItem {
        id: genreDragItem

        mlModel: genreModel

        indexes: selectionModel.selectedIndexes

        titleRole: "name"
    }

    /*
     *define the initial position/selection
     * This is done on activeFocus rather than Component.onCompleted because selectionModel.
     * selectedGroup update itself after this event
     */
    onActiveFocusChanged: {
        if (activeFocus && genreModel.count > 0 && !selectionModel.hasSelection) {
            var initialIndex = 0
            if (currentIndex !== -1)
                initialIndex = currentIndex

            selectionModel.select(genreModel.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect)
            currentItem.currentIndex = initialIndex
        }
    }

    Util.MLContextMenu {
        id: contextMenu

        model: genreModel
    }

    /* Grid View */
    Component {
        id: gridComponent
        MainInterface.MainGridView {
            id: gridView_id

            selectionDelegateModel: selectionModel
            model: genreModel
            topMargin: VLCStyle.margin_large

           delegate: Widgets.GridItem {
                id: item

                property var model: ({})
                property int index: -1

                width: VLCStyle.colWidth(2)
                height: width / 2
                pictureWidth: width
                pictureHeight: height
                image: model.cover || VLCStyle.noArtAlbumCover
                playCoverBorderWidth: VLCStyle.dp(3, VLCStyle.scale)
                dragItem: genreDragItem

                onItemDoubleClicked: root.showAlbumView(model.id, model.name, Qt.MouseFocusReason)
                onItemClicked: gridView_id.leftClickOnItem(modifier, item.index)

                onPlayClicked: {
                    if (model.id)
                        MediaLib.addAndPlay(model.id)
                }

                onContextMenuButtonClicked: {
                    gridView_id.rightClickOnItem(index)
                    contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
                }

                pictureOverlay: Item {
                    Rectangle
                    {
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
                             width: item.width
                             elide: Text.ElideRight
                             font.pixelSize: VLCStyle.fontSize_large
                             font.weight: Font.DemiBold
                             text: model.name || I18n.qtr("Unknown genre")
                             color: "white"
                             horizontalAlignment: Text.AlignHCenter
                        }

                        Widgets.CaptionLabel {
                            width: item.width
                            text: model.nb_tracks > 1 ? I18n.qtr("%1 Tracks").arg(model.nb_tracks) : I18n.qtr("%1 Track").arg(model.nb_tracks)
                            opacity: .7
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }

            focus: true

            cellWidth: VLCStyle.colWidth(2)
            cellHeight: cellWidth / 2

            onActionAtIndex: _actionAtIndex(index)

            Navigation.parentItem: root
            Navigation.cancelAction: function() {
                if (_currentView.currentIndex <= 0)
                    root.Navigation.defaultNavigationCancel()
                else
                    _currentView.currentIndex = 0;
            }
        }
    }

    Component {
        id: tableComponent
        /* Table View */
        MainInterface.MainTableView {
            id: tableView_id

            property int _nbCols: VLCStyle.gridColumnsForWidth(availableRowWidth)

            property var _modelSmall: [{
                size: Math.max(2, tableView_id._nbCols),

                model: {
                    criteria: "name",

                    subCriterias: [ "nb_tracks" ],

                    text: I18n.qtr("Name"),

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: tableColumns.titleDelegate
                }
            }]

            property var _modelMedium: [{
                size: 1,

                model: {
                    criteria: "cover",

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: tableColumns.titleDelegate
                }
            }, {
                size: Math.max(1, _nbCols - 2),

                model: {
                    criteria: "name",

                    text: I18n.qtr("Name")
                }
            }, {
                size: 1,

                model: {
                    criteria: "nb_tracks",

                    text: I18n.qtr("Tracks")
                }
            }]

            model: genreModel

            sortModel: (availableRowWidth < VLCStyle.colWidth(4)) ? _modelSmall
                                                                  : _modelMedium

            selectionDelegateModel: selectionModel
            focus: true
            onActionForSelection: _actionAtIndex(selection)
            Navigation.parentItem: root
            Navigation.cancelAction: function() {
                if (_currentView.currentIndex <= 0)
                    root.Navigation.defaultNavigationCancel()
                else
                    _currentView.currentIndex = 0;
            }
            dragItem: genreDragItem
            rowHeight: VLCStyle.tableCoverRow_height
            headerTopPadding: VLCStyle.margin_normal

            onItemDoubleClicked: {
                root.showAlbumView(model.id, model.name, Qt.MouseFocusReason)
            }

            onContextMenuButtonClicked: contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
            onRightClick: contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)

            Widgets.TableColumns {
                id: tableColumns

                showTitleText: (tableView_id.sortModel === tableView_id._modelSmall)
                showCriterias: showTitleText

                titleCover_height: VLCStyle.listAlbumCover_height
                titleCover_width: VLCStyle.listAlbumCover_width
                titleCover_radius: VLCStyle.listAlbumCover_radius
            }
        }
    }


    Component {
        id: emptyLabelComponent

        EmptyLabelButton {
            text: I18n.qtr("No genres found\nPlease try adding sources, by going to the Browse tab")
            Navigation.parentItem: root
            cover: VLCStyle.noArtAlbumCover
        }
    }
}
