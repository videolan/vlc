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
import QtQuick.Layouts
import QtQml.Models

import VLC.MainInterface
import VLC.MediaLibrary

import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style

FocusScope {
    id: root

    // properties

    readonly property bool isSearchable: true

    property alias subtitleText: subtitleLabel.text

    property int leftPadding: VLCStyle.margin_xsmall
    property int rightPadding: VLCStyle.margin_xsmall

    property int nbItemPerRow

    property int allVideosContentLeftMargin
    property int allVideosContentRightMargin
    
    property alias model: recentModel

    // Settings

    implicitHeight: recentVideosColumn.height

    Navigation.navigable: recentModel.count > 0

    function setCurrentItemFocus(reason) {
        console.assert(root.Navigation.navigable)
        if (reason === Qt.BacktabFocusReason)
            view.setCurrentItemFocus(reason)
        else
            button.forceActiveFocus(reason)
    }

    // Childs

    MLRecentsVideoModel {
        id: recentModel

        ml: MediaLib

        limit: MainCtx.gridView ? root.nbItemPerRow
                                : 5

        searchPattern: MainCtx.search.pattern
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    Column {
        id: recentVideosColumn

        width: root.width

        topPadding: VLCStyle.layoutTitle_top_padding

        Widgets.ViewHeader {
            id: viewHeader

            view: root

            visible: recentModel.count > 0
            seeAllButton.visible: recentModel.maximumCount > recentModel.count

            Layout.fillWidth: true

            leftPadding: view.currentItem?.contentLeftMargin ?? 0
            rightPadding: view.currentItem?.contentRightMargin ?? 0
            topPadding: 0

            text: qsTr("Continue Watching")

            onSeeAllButtonClicked: reason => History.push(["mc", "video", "all", "recentVideos"])

            Navigation.parentItem: root

            Navigation.downAction: function() {
                view.setCurrentItemFocus(Qt.TabFocusReason)
            }
        }

        VideoAll {
            id: view

            // Settings

            visible: recentModel.count > 0

            width: root.width
            height: currentItem?.contentHeight ?? 0

            leftPadding: root.leftPadding
            rightPadding: root.rightPadding

            focus: true

            model: recentModel

            sectionProperty: ""

            fadingEdgeList: false

            sortModel: []

            contextMenu: MLContextMenu {
                model: recentModel

                showPlayAsAudioAction: true
            }

            Navigation.parentItem: root

            Navigation.upItem: viewHeader.seeAllButton
        }

        Widgets.ViewHeader {
            id: subtitleLabel

            visible: text !== ""

            view: root

            leftPadding: allVideosContentLeftMargin
            rightPadding: allVideosContentRightMargin
            topPadding: recentModel.count > 0 ? (MainCtx.gridView ? VLCStyle.gridView_spacing :
                                                                    VLCStyle.tableView_spacing)
                                              : 0

            text: view.title
        }
    }
}
