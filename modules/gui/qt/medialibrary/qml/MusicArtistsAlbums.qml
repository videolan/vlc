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

import VLC.MainInterface
import VLC.MediaLibrary

import VLC.Util
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Menus

Widgets.PageExt {
    id: root

    // Properties
    property int initialIndex: 0
    property int initialAlbumIndex: 0
    property var artistId: undefined

    property var _requestedArtistId: undefined

    property alias model: artistModel
    property alias selectionModel: selectionModel

    property alias currentIndex: artistList.currentIndex
    property alias currentAlbumIndex: albumSubView.currentIndex

    property bool isScreenSmall: VLCStyle.isScreenSmall

    //padding is handled internally
    leftPadding: 0
    rightPadding: 0

    hasGridListMode: true
    isSearchable: true

    //we provide our own header
    header: null

    sortModel: MainCtx.gridView ? [
        { text: qsTr("Title"), criteria: "title" },
        { text: qsTr("Release Year"), criteria: "release_year" },
    ] : [
        { text: qsTr("Title"), criteria: "title" },
        { text: qsTr("Release Year"), criteria: "release_year" },
        { text: qsTr("Album Title"), criteria: "album_title" },
        { text: qsTr("Duration"), criteria: "duration" }
    ]

    onInitialAlbumIndexChanged: resetFocus()
    onInitialIndexChanged: resetFocus()

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

        //set the list on the requested artist id
        onLoadingChanged: {
            if (loading)
                return

            const defined = v => typeof v !== "undefined"

            if ((root._requestedArtistId === root.artistId)
                    && defined(root._requestedArtistId))
                return

            root._requestedArtistId = root.artistId
            if (!defined(root._requestedArtistId))
                return

            const thisRequestID = root._requestedArtistId
            artistModel.getIndexFromId(root._requestedArtistId)
                .then((row) => {
                    if ((root._requestedArtistId !== thisRequestID)
                        || (root._requestedArtistId !== root.artistId))
                        return

                    artistList.currentIndex = row
                    artistList._sidebarInitialyPositioned = true
                })
                .catch(() => {
                    if ((root._requestedArtistId !== thisRequestID)
                       || (root._requestedArtistId !== root.artistId))
                    return

                    artistList._sidebarInitialyPositioned = true
                })
        }
    }

    ListSelectionModel {
        id: selectionModel
        model: artistModel
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.leftPadding
        anchors.rightMargin: root.rightPadding

        visible: artistModel.count > 0

        spacing: 0

        Widgets.ListViewExt {
            id: artistList

            property bool _sidebarInitialyPositioned: false

            property int maximumWidth: root.width / 2
            property int minimumWidth: VLCStyle.colWidth(1) + VLCStyle.column_spacing

            model: artistModel
            selectionModel: root.selectionModel
            currentIndex: -1
            z: 1
            Layout.fillHeight: true

            Layout.preferredWidth: VLCStyle.isScreenSmall
                                   ? 0
                                   : Helpers.clamp(resizeHandle.requestedWidth, minimumWidth, maximumWidth)
            visible: !VLCStyle.isScreenSmall && (artistModel.count > 0)
            focus: !VLCStyle.isScreenSmall && (artistModel.count > 0)

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            fadingEdge.backgroundColor: (root.background && (root.background?.color.a > (1.0 - Number.EPSILON))) ? root.background.color
                                                                                                                 : "transparent"

            fadingEdge.enableBeginningFade: root.enableBeginningFade
            fadingEdge.enableEndFade: root.enableEndFade

            onCurrentIndexChanged: {
                if (!artistList._sidebarInitialyPositioned)
                    return
                root.artistId = model.getDataAt(currentIndex).id
            }

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

                visible: view.count > 0

                leftPadding: VLCStyle.margin_normal
                bottomPadding: VLCStyle.margin_small

                text: qsTr("Artists")
            }

            MLDragItem {
                id: musicArtistDragItem

                view: artistList

                indexes: indexesFlat ? selectionModel.selectedIndexesFlat
                                     : selectionModel.selectedIndexes
                indexesFlat: !!selectionModel.selectedIndexesFlat
            }

            delegate: MusicArtistDelegate {
                width: artistList.contentWidth

                isCurrent: ListView.isCurrentItem

                dragTarget: musicArtistDragItem

                selected: selectionModel.selectedIndexesFlat.includes(index)
            }

            Widgets.HorizontalResizeHandle {
                id: resizeHandle

                property bool _inhibitMainCtxUpdate: false

                anchors {
                    top: parent.top
                    bottom: parent.bottom
                    right: parent.right
                }

                z: 1

                currentWidth: artistList.width
                minimumWidth: artistList.minimumWidth
                maximumWidth: artistList.maximumWidth

                visible: !VLCStyle.isScreenSmall

                onRequestedWidthChanged: {
                    if (!_inhibitMainCtxUpdate)
                        MainCtx.artistAlbumsWidth = requestedWidth
                }

                Component.onCompleted:  _updateFromMainCtx()

                function _updateFromMainCtx() {
                    if (requestedWidth === MainCtx.artistAlbumsWidth)
                        return

                    _inhibitMainCtxUpdate = true
                    requestedWidth = MainCtx.artistAlbumsWidth
                    _inhibitMainCtxUpdate = false
                }

                Connections {
                    target: MainCtx

                    function onArtistAlbumsWidthChanged() {
                        resizeHandle._updateFromMainCtx()
                    }
                }
            }
        }

        Rectangle {
            Layout.fillHeight: true

            implicitWidth: VLCStyle.border
            color: artistList.colorContext.separator
            visible: artistList.visible
        }

        MusicArtist {
            id: albumSubView

            artistId: root.artistId

            Layout.fillHeight: true
            Layout.fillWidth: true

            search: root.search
            sort: root.sort

            enableBeginningFade: root.enableBeginningFade
            enableEndFade: root.enableEndFade

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            rightPadding: root.rightPadding

            focus: true
            initialIndex: root.initialAlbumIndex
            Navigation.parentItem: root
            Navigation.leftItem: VLCStyle.isScreenSmall ? null : artistList
        }
    }

    Widgets.EmptyLabelButton {
        anchors.centerIn: parent

        visible: !artistModel.loading && (artistModel.count <= 0)
        focus: visible
        text: qsTr("No artists found\nPlease try adding sources")
        Navigation.parentItem: root
    }
}
