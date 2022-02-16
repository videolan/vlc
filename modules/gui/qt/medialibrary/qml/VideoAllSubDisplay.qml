/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

VideoAll {
    id: root

    // Properties

    // NOTE: We are exposing a custom SortMenu with grouping options.
    property SortMenuVideo sortMenu: SortMenuVideo {
        ctx: MainCtx

        onGrouping: MainCtx.grouping = grouping
    }

    // Private

    property var _meta: (MainCtx.grouping === MainCtx.GROUPING_NONE) ? metaVideo
                                                                     : metaGroup

    // Signals

    signal showList(var model, int reason)

    // Settings

    anchors.fill: parent

    model: _meta.model

    contextMenu: _meta.contextMenu

    // Functions

    function getLabelGroup(model, string) {
        if (!model) return ""

        var count = model.count

        if (count === 1) {
            return getLabel(model)
        } else {
            if (count < 100)
                return [ string.arg(count) ]
            else
                return [ string.arg("99+") ]
        }
    }

    // VideoAll reimplementation

    function setCurrentItemFocus(reason) {
        if (modelRecent.count)
            headerItem.setCurrentItemFocus(reason)
        else
            _currentView.setCurrentItemFocus(reason)
    }

    // VideoAll events reimplementation

    function onAction(indexes) { _meta.onAction(indexes) }

    function onDoubleClick(object) { _meta.onDoubleClick(object) }

    function onLabelGrid(object) { return _meta.onLabelGrid(object) }
    function onLabelList(object) { return _meta.onLabelList(object) }

    // Children

    QtObject {
        id: metaVideo

        property var model: MLVideoModel { ml: MediaLib }

        property var contextMenu: VideoContextMenu { model: metaVideo.model }

        function onAction(indexes) {
            g_mainDisplay.showPlayer()

            MediaLib.addAndPlay(model.getIdsForIndexes(indexes))
        }

        function onDoubleClick(object) { g_mainDisplay.play(MediaLib, object.id) }

        function onLabelGrid(object) { return root.getLabel(object) }
        function onLabelList(object) { return root.getLabel(object) }
    }

    QtObject {
        id: metaGroup

        property var model: MLVideoGroupsModel { ml: MediaLib }

        property var contextMenu: VideoGroupsContextMenu { model: metaGroup.model }

        function onAction(indexes) {
            var index = indexes[0]

            var object = model.getDataAt(index);

            if (object.isVideo) {
                g_mainDisplay.showPlayer()

                MediaLib.addAndPlay(model.getIdsForIndexes(indexes))

                return
            }

            root.showList(object, Qt.TabFocusReason)
        }

        function onDoubleClick(object) {
            if (object.isVideo) {
                g_mainDisplay.play(MediaLib, object.id)

                return
            }

            root.showList(object, Qt.MouseFocusReason)
        }

        function onLabelGrid(object) {
            return root.getLabelGroup(object, I18n.qtr("%1 Videos"))
        }

        function onLabelList(object) {
            return root.getLabelGroup(object, I18n.qtr("%1"))
        }
    }

    // Children

    MLRecentsVideoModel {
        id: modelRecent

        ml: MediaLib
    }

    header: Column {
        property Item focusItem: (loader.status === Loader.Ready) ? loader.item.focusItem
                                                                  : null

        property alias loader: loader

        width: root.width

        topPadding: VLCStyle.margin_normal
        bottomPadding: VLCStyle.margin_normal

        // NOTE: We want the header to be visible when we have at least one media visible.
        //       Otherwise it overlaps the default caption.
        visible: (root.model.count && modelRecent.count)

        function setCurrentItemFocus(reason) {
            var item = loader.item;

            if (item)
                item.setCurrentItemFocus(reason);
        }

        Loader {
            id: loader

            anchors.left : parent.left
            anchors.right: parent.right

            anchors.margins: root.contentMargin

            height: (status === Loader.Ready) ? item.implicitHeight : 0

            active: (modelRecent.count)

            visible: active

            sourceComponent: VideoDisplayRecentVideos {
                id: component

                width: parent.width

                // NOTE: We want grid items to be visible on the sides.
                displayMargins: root.contentMargin

                model: modelRecent

                focus: true

                Navigation.parentItem: root

                Navigation.downAction: function() {
                    _currentView.setCurrentItemFocus(Qt.TabFocusReason);
                }
            }
        }

        Widgets.SubtitleLabel {
            anchors.left: loader.left
            anchors.right: loader.right

            // NOTE: We want this to be properly aligned with the grid items.
            anchors.leftMargin: VLCStyle.margin_normal

            text: I18n.qtr("Videos")
        }
    }
}
