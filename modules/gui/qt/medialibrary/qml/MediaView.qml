/******************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Author: Ash <ashutoshv191@gmail.com>
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
 ******************************************************************************/
import QtQuick

import VLC.MediaLibrary

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style

// NOTE: This is a customizable view that can be used with
//       the `MLMediaModel` and its drivatives.
// TODO: Investigate and derive `VideoAll` from `MediaView`.
MainViewLoader {
    id: root

    required property string headerText

    property int listHeaderPositioning: ListView.OverlayHeader // ListView.InlineHeader, when as a row

    // Currently only respected by the list view:
    property bool enableBeginningFade: true
    property bool enableEndFade: true

    property string listSectionProperty: model.sortCriteria === "title" ? "title_first_symbol" : "" // "", when as a row

    property int listCoverWidth: VLCStyle.trackListAlbumCover_width
    property int listCoverHeight: VLCStyle.trackListAlbumCover_heigth
    property int listCoverRadius: VLCStyle.trackListAlbumCover_radius

    property int gridCoverWidth: VLCStyle.gridCover_music_width
    property int gridCoverHeight: VLCStyle.gridCover_music_height

    property int fillMode: MainCtx.gridView ? Image.PreserveAspectCrop : Image.PreserveAspectFit

    property int displayMarginBeginning: 0
    property int displayMarginEnd: 0

    property bool interactive: true // false, when as a row

    property bool reuseItems: true

    property bool seeAllButtonVisible: model.maximumCount > model.count

    readonly property int currentIndex: currentItem?.currentIndex ?? -1

    readonly property int rowHeight: currentItem?.rowHeight ?? 0

    readonly property int contentLeftMargin: currentItem?.contentLeftMargin ?? 0
    readonly property int contentRightMargin: currentItem?.contentRightMargin ?? 0

    isSearchable: true

    sortModel: [
        { text: qsTr("Alphabetic"), criteria: "title" },
        { text: qsTr("Duration"), criteria: "duration" }
    ]

    list: listComponent
    grid: gridComponent
    emptyLabel: emptyLabelComponent


    signal seeAllButtonClicked(int reason)


    function getItemY(index) {
        return currentItem?.getItemY(index) ?? 0
    }

    function _onItemDoubleClicked(model) {
        MediaLib.addAndPlay(model.id)
        History.push(["player"])
    }

    function _onActionAtIndex() {
        model.addAndPlay(selectionModel.selectedIndexes)
        History.push(["player"])
    }


    Widgets.MLDragItem {
        id: dragItemId

        view: root.currentItem

        coverRole: "small_cover"

        defaultCover: VLCStyle.noArtAlbumCover // TODO: better fallback image
                                               // use a better vlc branding fallback image like 'noart.png' which is used in vlc 3 for media

        indexesFlat: !!root.selectionModel.selectedIndexesFlat

        indexes: indexesFlat ? root.selectionModel.selectedIndexesFlat : root.selectionModel.selectedIndexes
    }

    MLContextMenu {
        id: contextMenu

        model: root.model
    }

    Component {
        id: headerComponent

        Widgets.ViewHeader {
            view: root

            visible: view.count > 0

            text: root.headerText

            seeAllButton.visible: root.seeAllButtonVisible

            Navigation.parentItem: root
            Navigation.downAction: function () {
                if (root.currentItem?.setCurrentItemFocus)
                    root.currentItem.setCurrentItemFocus(Qt.TabFocusReason)
            }

            Component.onCompleted: {
                seeAllButtonClicked.connect(root.seeAllButtonClicked)
            }
        }
    }

    Component {
        id: gridComponent

        Widgets.ExpandGridItemView {
            id: gridView

            focus: true

            model: root.model

            selectionModel: root.selectionModel

            headerDelegate: headerComponent

            basePictureWidth: root.gridCoverWidth
            basePictureHeight: root.gridCoverHeight

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            interactive: root.interactive

            reuseItems: root.reuseItems

            Navigation.parentItem: root
            Navigation.upItem: headerItem

            delegate: Widgets.GridItem {
                id: gridDelegate

                required property int index
                required property var model

                // TODO: Investigate if it makes sense to use `sourceClipRect`
                //       instead of setting `fillMode` to preserve aspect crop.
                width: gridView.cellWidth
                height: gridView.cellHeight

                pictureWidth: gridView.maxPictureWidth
                pictureHeight: gridView.maxPictureHeight

                fillMode: root.fillMode

                image: model.small_cover ?? ""
                fallbackImage: model.isVideo ? VLCStyle.noArtVideoCover : VLCStyle.noArtAlbumCover

                title: model.title || qsTr("Unknown Title")
                subtitle: model.duration?.formatHMS() ?? "--:--"

                opacity: gridView.expandIndex !== -1 && gridView.expandIndex !== index ? 0.7 : 1

                dragItem: dragItemId

                onPlayClicked: {
                    if (model.id !== undefined) {
                        MediaLib.addAndPlay(model.id)
                        if (model.isVideo)
                            MainCtx.requestShowPlayerView()
                    }
                }

                onItemDoubleClicked: root._onItemDoubleClicked(model)

                onContextMenuButtonClicked: (_, globalMousePos) => {
                    gridView.rightClickOnItem(index)
                    contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
                }

                onItemClicked: (modifier) => {
                    gridView.leftClickOnItem(modifier, index)
                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: VLCStyle.duration_short
                    }
                }
            }

            onActionAtIndex: root._onActionAtIndex()
        }
    }

    Component {
        id: listComponent

        Widgets.TableViewExt {
            id: tableView

            property var _sortModelLarge: [{
                weight: 1,

                model: {
                    criteria: "title",

                    text: qsTr("Title"),

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: tableColumns.titleDelegate,

                    placeHolder: VLCStyle.noArtAlbumCover // TODO: better fallback image
                }
            }, {
                size: 1,

                model: {
                    criteria: "duration",

                    text: qsTr("Duration"),

                    headerDelegate: tableColumns.timeHeaderDelegate,
                    colDelegate: tableColumns.timeColDelegate
                }
            }]

            property var _sortModelSmall: [{
                weight: 1,

                model: {
                    criteria: "title",

                    text: qsTr("Title"),

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: tableColumns.titleDelegate,

                    placeHolder: VLCStyle.noArtAlbumCover, // TODO: better fallback image

                    subCriterias: ["duration"]
                }
            }]

            focus: true

            model: root.model

            selectionModel: root.selectionModel

            sortModel: availableRowWidth >= VLCStyle.colWidth(4) ? _sortModelLarge : _sortModelSmall

            header: headerComponent

            headerPositioning: root.listHeaderPositioning

            rowHeight: VLCStyle.tableCoverRow_height

            section.property: root.listSectionProperty

            fadingEdge.enableBeginningFade: root.enableBeginningFade
            fadingEdge.enableEndFade: root.enableEndFade

            dragItem: dragItemId

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            interactive: root.interactive

            reuseItems: root.reuseItems

            Navigation.parentItem: root
            Navigation.upItem: headerItem

            rowContextMenu: contextMenu

            onItemDoubleClicked: (_, model) => {
                root._onItemDoubleClicked(model)
            }

            onRightClick: (_, _, globalMousePos) => {
                contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
            }

            onActionForSelection: root._onActionAtIndex()


            Widgets.MLTableColumns {
                id: tableColumns

                fillMode: root.fillMode

                titleCover_width: root.listCoverWidth
                titleCover_height: root.listCoverHeight
                titleCover_radius: root.listCoverRadius

                criteriaCover: "small_cover"

                showCriterias: tableView.sortModel === tableView._sortModelSmall
            }
        }
    }

    Component {
        id: emptyLabelComponent

        Widgets.EmptyLabelButton {
            focus: true

            cover: VLCStyle.noArtAlbumCover // TODO: better fallback image

            text: qsTr("No media found\nPlease try adding sources")

            Navigation.parentItem: root
        }
    }
}
