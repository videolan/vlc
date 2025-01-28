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
    property var artistId: undefined

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

    property bool isScreenSmall: VLCStyle.isScreenSmall

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
            artistModel.getIndexFromId(root.artistId)
                .then((row) => {
                    artistList.currentIndex = row
                    artistList._sidebarInitialyPositioned = true
                })
                .catch(() => {
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


            onCurrentIndexChanged: {
                if (!artistList._sidebarInitialyPositioned)
                    return
                root.artistId = model.getDataAt(currentIndex).id
            }

            Widgets.AcrylicBackground {
                id: artistListBackground

                z: -1

                anchors.fill: parent

                anchors.bottomMargin: usingAcrylic ? 0 : -artistList.displayMarginEnd

                tintColor: artistList.colorContext.bg.secondary

                Rectangle {
                    // For the frosted glass effect to work properly, the content
                    // (source) should be opaque. If there is inter-window backdrop
                    // blur, this is not the case.

                    // Note that beneath there is already (`stackViewParent`) a
                    // background rectangle, but since `ViewBlockingRectangle` does
                    // not use blending to block anything beneath in the scene graph
                    // being rendered, the layered source essentially has transparent
                    // area. Qt 5 (barely) allowed having layered items to have RGB
                    // texture, but Qt 6 RHI mandates RGBA format
                    // (`ShaderEffectSource.RGBA`).

                    // For that reason, we need to restrict the extents of inter-window
                    // backdrop blur (`AcrylicBackground`) to the beginning boundaries
                    // of the area that is going to be blurred by `FrostedGlassEffect`.

                    // This rectangle would not be necessary if there was a guarantee
                    // that `stackViewParent`'s color is the same as
                    // `artistListBackground.alternativeColor` as the `AcrylicBackground`
                    // area is restricted, but since there is no such mandate this
                    // rectangle can act as the opaque background provider with the
                    // desired color.

                    // Ideally the `FrostedGlassEffect` itself should have a base
                    // rectangle that it places beneath the source, similar to
                    // how it has a `filter` that it places on top of the source,
                    // but this is not possible because `AcrylicBackground`/
                    // `ViewBlockingRectangle` prevents anything beneath in the
                    // scene graph to be rendered as it acts as a "pass through"
                    // (for video visual PIP player case, or the window itself
                    // for the inter-window backdrop blur case).

                    // Background should be opaque, though here if it is translucent,
                    // there is also another background provider `stackViewParent`
                    // but its color may be different:
                    color: Qt.alpha(artistListBackground.alternativeColor, 1.0)

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.bottom

                    height: artistList.displayMarginEnd

                    visible: (height > 0.0) && artistListBackground.usingAcrylic
                }
            }

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

                leftPadding: VLCStyle.margin_normal
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

                sourceWidth: root.width
                targetWidth: artistList.width

                onWidthFactorChanged: {
                    if (!_inhibitMainCtxUpdate)
                        MainCtx.artistAlbumsWidthFactor = widthFactor
                }

                Component.onCompleted:  _updateFromMainCtx()

                function _updateFromMainCtx() {
                    if (widthFactor == MainCtx.artistAlbumsWidthFactor)
                        return

                    _inhibitMainCtxUpdate = true
                    widthFactor = MainCtx.artistAlbumsWidthFactor
                    _inhibitMainCtxUpdate = false
                }

                Connections {
                    target: MainCtx

                    function onArtistAlbumsWidthFactorChanged() {
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

            rightPadding: root.rightPadding

            focus: true
            initialIndex: root.initialAlbumIndex
            Navigation.parentItem: root
            Navigation.leftItem: VLCStyle.isScreenSmall ? null : artistList
        }
    }

    Widgets.EmptyLabelButton {
        anchors.fill: parent
        anchors.leftMargin: root.leftPadding
        anchors.rightMargin: root.rightPadding

        visible: !artistModel.loading && (artistModel.count <= 0)
        focus: visible
        text: qsTr("No artists found\nPlease try adding sources, by going to the Browse tab")
        Navigation.parentItem: root
    }
}
