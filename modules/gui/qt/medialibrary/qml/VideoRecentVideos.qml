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
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQml.Models 2.12

import org.videolan.medialib 0.1
import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/" as Util
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///style/"

FocusScope {
    id: root

    // properties

    readonly property bool isSearchable: true

    property alias subtitleText: subtitleLabel.text

    property int bottomPadding: recentVideosColumn.bottomPadding
    property int leftPadding: VLCStyle.margin_xsmall
    property int rightPadding: VLCStyle.margin_xsmall

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

        limit: MainCtx.gridView ? view.currentItem.nbItemPerRow ?
                                              view.currentItem.nbItemPerRow : 0
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

        topPadding: VLCStyle.margin_large

        spacing: VLCStyle.margin_normal

        bottomPadding: root.bottomPadding

        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right

            anchors.leftMargin: view.currentItem.contentLeftMargin
            anchors.rightMargin: view.currentItem.contentRightMargin

            Widgets.SubtitleLabel {
                id: label

                Layout.fillWidth: true

                text: I18n.qtr("Continue Watching")

                // NOTE: Setting this to gridView.visible seems to causes unnecessary implicitHeight
                //       calculations in the Column parent.
                visible: recentModel.count > 0
                color: theme.fg.primary
            }

            Widgets.TextToolButton {
                id: button

                visible: recentModel.maximumCount > recentModel.count

                Layout.preferredWidth: implicitWidth

                focus: true

                text: I18n.qtr("See All")

                font.pixelSize: VLCStyle.fontSize_large

                Navigation.parentItem: root

                Navigation.downAction: function() {
                    view.setCurrentItemFocus(Qt.TabFocusReason)
                }

                onClicked: History.push(["mc", "video", "all", "recentVideos"]);
            }
        }

        VideoAll {
            id: view

            // Settings

            visible: recentModel.count > 0

            width: root.width
            height: MainCtx.gridView ? VLCStyle.gridItem_video_height + VLCStyle.gridItemSelectedBorder + VLCStyle.margin_xlarge
                                     : VLCStyle.margin_xxlarge + Math.min(recentModel.count, 5) * VLCStyle.tableCoverRow_height

            leftPadding: root.leftPadding
            rightPadding: root.rightPadding

            focus: true

            model: recentModel

            sectionProperty: ""

            sortModel: []

            contextMenu: Util.MLContextMenu {
                model: recentModel

                showPlayAsAudioAction: true
            }

            Navigation.parentItem: root

            Navigation.upItem: button
        }

        Widgets.SubtitleLabel {
            id: subtitleLabel

            visible: text !== ""
            color: theme.fg.primary

            leftPadding: view.currentItem.contentLeftMargin
            rightPadding: view.currentItem.contentRightMargin
        }
    }
}
