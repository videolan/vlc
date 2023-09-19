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
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/" as Util
import "qrc:///main/" as MainInterface
import "qrc:///style/"

MainInterface.MainGridView {
    id: root

    //properties

    readonly property bool hasGridListMode: false
    readonly property bool isSearchable: true

    property var pagePrefix: []

    //signals

    signal browseServiceManage(int reason)
    signal browseSourceRoot(string sourceName, int reason)

    //settings

    model: sourcesModel
    cellWidth: VLCStyle.gridItem_network_width
    cellHeight: VLCStyle.gridCover_network_height + VLCStyle.margin_xsmall + VLCStyle.fontHeight_normal

    headerDelegate: Widgets.ViewHeader {
        view: root

        text: qsTr("Services")
    }

    delegate: Widgets.GridItem {

        property var model: ({})
        property int index: -1
        readonly property bool is_dummy: model.type === NetworkSourcesModel.TYPE_DUMMY

        title: is_dummy ? qsTr("Add a service") : model.long_name
        subtitle: ""
        pictureWidth: VLCStyle.colWidth(1)
        pictureHeight: VLCStyle.gridCover_network_height
        height: VLCStyle.gridCover_network_height + VLCStyle.margin_xsmall + VLCStyle.fontHeight_normal
        playCoverBorderWidth: VLCStyle.gridCover_network_border
        playCoverShowPlay: false
        image: {
            if (is_dummy) {
                return SVGColorImage.colorize("qrc:///placeholder/add_service.svg")
                    .color1(this.colorContext.fg.secondary)
                    .accent(this.colorContext.accent)
                    .uri()
            } else if (model.artwork && model.artwork.toString() !== "") {
                //if the source is a qrc artwork, we should colorize it
                if (model.artwork.toString().match(/qrc:\/\/.*svg/))
                {
                    return SVGColorImage.colorize(model.artwork)
                        .color1(this.colorContext.fg.secondary)
                        .accent(this.colorContext.accent)
                        .uri()
                }

                return model.artwork
            }

            // use fallbackImage
            return ""
        }

        fallbackImage: {
            return SVGColorImage.colorize("qrc:///sd/directory.svg")
                .color1(this.colorContext.fg.secondary)
                .uri()
        }

        onItemDoubleClicked: (_, _, modifier) => {
            if (is_dummy)
                root.browseServiceManage(Qt.MouseFocusReason)
            else
                root.browseSourceRoot(model.name, Qt.TabFocusReason)
        }

        onItemClicked : (_, _, modifier) => {
            root.selectionModel.updateSelection(modifier, root.currentIndex, index)
            root.currentIndex = index
            root.forceActiveFocus()
        }
    }

    onActionAtIndex: (index) => {
        const itemData = sourcesModel.getDataAt(index);

        if (itemData.type === NetworkSourcesModel.TYPE_DUMMY)
            browseServiceManage(Qt.TabFocusReason)
        else
            browseSourceRoot(itemData.name, Qt.TabFocusReason)
    }

    Navigation.cancelAction: function() {
        History.previous(Qt.BacktabFocusReason)
    }

    NetworkSourcesModel {
        id: sourcesModel

        ctx: MainCtx

        searchPattern: MainCtx.search.pattern
        sortOrder: MainCtx.sort.order
        sortCriteria: MainCtx.sort.criteria
    }
}
