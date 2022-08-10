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
import QtQuick.Controls 2.4
import QtQuick 2.11
import QtQml.Models 2.2
import QtQuick.Layouts 1.11

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

    property alias model: artistModel
    property var sortModel: [
        { text: I18n.qtr("Alphabetic"),  criteria: "title" }
    ]

    property alias currentIndex: artistList.currentIndex
    property alias currentAlbumIndex: albumSubView.currentIndex
    property int initialIndex: 0
    property int initialAlbumIndex: 0
    property alias currentArtist: albumSubView.artist

    onInitialAlbumIndexChanged: resetFocus()
    onInitialIndexChanged: resetFocus()
    onCurrentIndexChanged: currentArtist = model.getDataAt(currentIndex)

    function resetFocus() {
        if (artistModel.count === 0) {
            return
        }
        var initialIndex = root.initialIndex
        if (initialIndex >= artistModel.count)
            initialIndex = 0
        if (initialIndex !== artistList.currentIndex) {
            selectionModel.select(artistModel.index(initialIndex, 0), ItemSelectionModel.ClearAndSelect)
            if (artistList) {
                artistList.currentIndex = initialIndex
                artistList.positionViewAtIndex(initialIndex, ItemView.Contain)
            }
        }
    }

    function setCurrentItemFocus(reason) {
        artistList.setCurrentItemFocus(reason);
    }

    function _actionAtIndex(index) {
        albumSubView.forceActiveFocus()
    }

    MLArtistModel {
        id: artistModel
        ml: MediaLib

        onCountChanged: {
            if (artistModel.count > 0 && !selectionModel.hasSelection) {
                var initialIndex = root.initialIndex
                if (initialIndex >= artistModel.count)
                    initialIndex = 0
                artistList.currentIndex = initialIndex
            }
        }

        onDataChanged: {
            if (topLeft.row <= currentIndex && bottomRight.row >= currentIndex)
                currentArtist = artistModel.getDataAt(currentIndex)
        }
    }

    Util.SelectableDelegateModel {
        id: selectionModel
        model: artistModel
    }

    Widgets.AcrylicBackground {
      /* id: artistListBackground */

      visible: artistModel.count > 0
      width: artistList.width
      height: artistList.height
      alternativeColor: VLCStyle.colors.bgAlt
      focus: false
    }

    Row {
        anchors.fill: parent
        visible: artistModel.count > 0

        Widgets.KeyNavigableListView {
            id: artistList

            spacing: 4
            model: artistModel
            currentIndex: -1
            z: 1
            height: parent.height
            width: Helpers.clamp(root.width / resizeHandle.widthFactor,
                                 VLCStyle.colWidth(1) + VLCStyle.column_margin_width,
                                 root.width * .5)

            visible: artistModel.count > 0
            focus: artistModel.count > 0
            displayMarginEnd: miniPlayer.height // to get blur effect while scrolling in mainview
            Navigation.parentItem: root

            Navigation.rightAction: function() {
                albumSubView.setCurrentItemFocus(Qt.TabFocusReason);
            }

            Navigation.cancelAction: function() {
                if (artistList.currentIndex <= 0)
                    root.Navigation.defaultNavigationCancel()
                else
                    artistList.currentIndex = 0;
            }

            header: Widgets.SubtitleLabel {
                text: I18n.qtr("Artists")
                font.pixelSize: VLCStyle.fontSize_large
                leftPadding: VLCStyle.margin_normal
                bottomPadding: VLCStyle.margin_small
                topPadding: VLCStyle.margin_xlarge
            }

            delegate: MusicArtistDelegate {
                width: artistList.width

                isCurrent: ListView.isCurrentItem

                mlModel: artistModel

                onItemClicked: {
                    selectionModel.updateSelection(mouse.modifiers, artistList.currentIndex,
                                                   index);

                    artistList.currentIndex = index;

                    artistList.forceActiveFocus(Qt.MouseFocusReason);
                }

                onItemDoubleClicked: {
                    if (mouse.buttons === Qt.LeftButton)
                        MediaLib.addAndPlay(model.id);
                    else
                        albumSubView.forceActiveFocus();
                }
            }

            Behavior on width {
                SmoothedAnimation {
                    easing.type: Easing.InSine
                    duration: VLCStyle.duration_veryShort
                }
            }

            Rectangle {
                // id: musicArtistLeftBorder

                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right

                width: VLCStyle.border
                color: VLCStyle.colors.border
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

        MusicArtist {
            id: albumSubView

            height: parent.height
            width: root.width - artistList.width
            focus: true
            initialIndex: root.initialAlbumIndex
            Navigation.parentItem: root
            Navigation.leftItem: artistList
        }
    }

    EmptyLabelButton {
        anchors.fill: parent
        visible: artistModel.count === 0
        focus: visible
        text: I18n.qtr("No artists found\nPlease try adding sources, by going to the Browse tab")
        Navigation.parentItem: root
    }
}
