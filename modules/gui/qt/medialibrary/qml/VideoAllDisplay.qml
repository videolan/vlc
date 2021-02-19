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
import QtQuick.Layouts  1.3
import QtQml.Models     2.2

import org.videolan.medialib 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///main/"    as MainInterface
import "qrc:///util/"    as Util
import "qrc:///style/"

VideoAll {
    id: root

    //---------------------------------------------------------------------------------------------
    // Events
    //---------------------------------------------------------------------------------------------

    onCurrentIndexChanged: {
        history.update([ "mc", "video", { "initialIndex": currentIndex }])
    }

    //---------------------------------------------------------------------------------------------
    // Settings
    //---------------------------------------------------------------------------------------------

    MLRecentsVideoModel {
        id: modelRecent

        ml: medialib
    }

    header: Column {
        property Item focusItem: loader.item.focusItem

        width: root.width

        topPadding: VLCStyle.margin_normal

        spacing: VLCStyle.margin_normal

        Loader {
            id: loader

            width: parent.width

            height: item.implicitHeight

            active: (modelRecent.count)

            visible: active

            focus: true

            sourceComponent: VideoDisplayRecentVideos {
                id: component

                width: parent.width

                model: modelRecent

                focus: true

                navigationParent: root

                navigationDown: function() {
                    component.focus = false;

                    currentItem.setCurrentItemFocus();
                }
            }
        }

        Widgets.SubtitleLabel {
            width: root.width

            leftPadding  : VLCStyle.margin_xlarge
            bottomPadding: VLCStyle.margin_xsmall

            text: i18n.qtr("Videos")
        }
    }
}
