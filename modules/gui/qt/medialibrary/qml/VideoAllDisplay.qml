/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

import QtQuick          2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts  1.11
import QtQml.Models     2.2

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///main/"    as MainInterface
import "qrc:///util/"    as Util
import "qrc:///style/"

VideoAll {
    id: root

    // Events

    onCurrentIndexChanged: {
        History.update([ "mc", "video", { "initialIndex": currentIndex }])
    }

    // Functions

    function setCurrentItemFocus(reason) {
        if (modelRecent.count)
            headerItem.setCurrentItemFocus(reason);
        else
            _currentView.setCurrentItemFocus(reason);
    }

    // Children

    MLRecentsVideoModel {
        id: modelRecent

        ml: MediaLib
    }

    header: Column {
        property Item focusItem: (loader.status === Loader.Ready) ? loader.item.focusItem : null

        property alias loader: loader

        width: root.width

        topPadding: VLCStyle.margin_normal
        bottomPadding: VLCStyle.margin_normal

        // NOTE: We want the header to be visible when we have at least one media visible.
        //       Otherwise it overlaps the default caption.
        visible: (model.count)

        // NOTE: Making sure this item will be focussed by VideoAll::_onNavigationUp().
        focus: true

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
