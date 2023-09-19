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
import QtQuick.Controls
import QtQuick
import QtQml.Models
import QtQuick.Layouts

import org.videolan.medialib 0.1
import org.videolan.controls 0.1
import org.videolan.vlc 0.1

import "qrc:///util/" as Util
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///widgets/" as Widgets
import "qrc:///main/" as MainInterface
import "qrc:///style/"

FocusScope {
    id: root

    // Properties

    property int leftPadding: 0
    property int rightPadding: 0

    property var sortModel: [
        { text: qsTr("Alphabetic"),  criteria: "title" },
        { text: qsTr("Release Year"),  criteria: "release_year" }
    ]

    property int initialIndex: 0
    property int initialAlbumIndex: 0

    //behave like a page
    property var pagePrefix: []

    readonly property bool hasGridListMode: true
    readonly property bool isSearchable: true

    property alias model: artistModel
    property alias selectionModel: selectionModel

    property alias searchPattern: albumSubView.searchPattern
    property alias sortOrder: albumSubView.sortOrder
    property alias sortCriteria: albumSubView.sortCriteria

    property alias currentIndex: artistList.currentIndex
    property alias currentAlbumIndex: albumSubView.currentIndex

    property alias currentArtist: albumSubView.artist
    property bool isScreenSmall: VLCStyle.isScreenSmall

    onInitialAlbumIndexChanged: resetFocus()
    onInitialIndexChanged: resetFocus()
    onCurrentIndexChanged: currentArtist = model.getDataAt(currentIndex)
    onIsScreenSmallChanged: {
        if (VLCStyle.isScreenSmall)
            resetFocus()
    }

    function resetFocus() {
        if (VLCStyle.isScreenSmall) {
            albumSubView.setCurrentItemFocus(Qt.OtherFocusReason)
            return
        }

        if (model.count === 0 || initialIndex === -1) return

        selectionModel.select(model.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect)

        artistList.positionViewAtIndex(initialIndex, ItemView.Contain)

        artistList.setCurrentItem(initialIndex)
    }

    function setCurrentItemFocus(reason) {
        if (VLCStyle.isScreenSmall) {
            albumSubView.setCurrentItemFocus(reason);
        } else {
            artistList.setCurrentItemFocus(reason);
        }
    }

    function _actionAtIndex(index) {
        albumSubView.forceActiveFocus()
    }

    MLArtistModel {
        id: artistModel
        ml: MediaLib

        onCountChanged: {
            if (count === 0 || selectionModel.hasSelection)
                return

            root.resetFocus()
        }

        onDataChanged: {
            if (topLeft.row <= currentIndex && bottomRight.row >= currentIndex)
                currentArtist = artistModel.getDataAt(currentIndex)
        }
    }

    ListSelectionModel {
        id: selectionModel
        model: artistModel
    }

    Widgets.AcrylicBackground {
      id: artistListBackground

      visible: artistModel.count > 0
      width: artistList.width
      height: artistList.height + artistList.displayMarginEnd

      tintColor: artistList.colorContext.bg.secondary

      focus: false
    }

    RowLayout {
        anchors.fill: parent

        visible: artistModel.count > 0

        spacing: 0

        Widgets.KeyNavigableListView {
            id: artistList

            model: artistModel
            selectionModel: root.selectionModel
            currentIndex: -1
            z: 1
            Layout.fillHeight: true
            Layout.preferredWidth: VLCStyle.isScreenSmall
                                   ? 0
                                   : Math.round(Helpers.clamp(root.width / resizeHandle.widthFactor,
                                                              VLCStyle.colWidth(1) + VLCStyle.column_spacing,
                                                              root.width * .5))

            visible: !VLCStyle.isScreenSmall && (artistModel.count > 0)
            focus: !VLCStyle.isScreenSmall && (artistModel.count > 0)

            fadingEdge.backgroundColor: artistListBackground.usingAcrylic ? "transparent"
                                                                          : artistListBackground.alternativeColor

            // To get blur effect while scrolling in mainview
            displayMarginEnd: g_mainDisplay.displayMargin

            Navigation.parentItem: root

            Navigation.rightAction: function() {
                albumSubView.setCurrentItemFocus(Qt.TabFocusReason);
            }

            Navigation.cancelAction: function() {
                if (artistList.currentIndex <= 0) {
                    root.Navigation.defaultNavigationCancel()

                    return
                }

                artistList.positionViewAtIndex(0, ItemView.Contain)

                artistList.setCurrentItem(0)
            }

            header: Widgets.ViewHeader {
                view: artistList

                leftPadding: root.leftPadding + VLCStyle.margin_normal
                topPadding: VLCStyle.margin_xlarge
                bottomPadding: VLCStyle.margin_small

                text: qsTr("Artists")
            }

            Widgets.MLDragItem {
                id: musicArtistDragItem

                mlModel: artistModel

                indexes: indexesFlat ? selectionModel.selectedIndexesFlat
                                     : selectionModel.selectedIndexes
                indexesFlat: !!selectionModel.selectedIndexesFlat
            }

            delegate: MusicArtistDelegate {
                width: artistList.width

                leftPadding: rightPadding + root.leftPadding

                isCurrent: ListView.isCurrentItem

                mlModel: artistModel

                dragTarget: musicArtistDragItem

                selected: selectionModel.selectedIndexesFlat.includes(index)
            }

            Widgets.HorizontalResizeHandle {
                id: resizeHandle

                anchors {
                    top: parent.top
                    bottom: parent.bottom
                    right: parent.right
                }
                sourceWidth: root.width
                targetWidth: artistList.width
            }
        }

        Rectangle {
            Layout.fillHeight: true

            implicitWidth: VLCStyle.border
            color: artistList.colorContext.separator
        }

        MusicArtist {
            id: albumSubView

            Layout.fillHeight: true
            Layout.fillWidth: true

            rightPadding: root.rightPadding

            focus: true
            initialIndex: root.initialAlbumIndex
            Navigation.parentItem: root
            Navigation.leftItem: VLCStyle.isScreenSmall ? null : artistList
        }
    }

    Widgets.EmptyLabelButton {
        anchors.fill: parent
        visible: !artistModel.loading && (artistModel.count <= 0)
        focus: visible
        text: qsTr("No artists found\nPlease try adding sources, by going to the Browse tab")
        Navigation.parentItem: root
    }
}
