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
import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQml.Models 2.2

import org.videolan.medialib 0.1

import "qrc:///utils/" as Utils
import "qrc:///style/"

Utils.KeyNavigableTableView {
    id: listView_id
    model: MLVideoModel {
        ml: medialib
    }
    sortModel: ListModel {
        ListElement{ type: "image"; criteria: "thumbnail";   width:0.2; text: qsTr("Thumbnail"); showSection: "" }
        ListElement{ criteria: "duration";    width:0.1; text: qsTr("Duration"); showSection: "" }
        ListElement{ isPrimary: true; criteria: "title";       width:0.6; text: qsTr("Title");    showSection: "title" }
        ListElement{ type: "contextButton";   width:0.1; }
    }
    section.property: "title_first_symbol"

    rowHeight: VLCStyle.video_small_height + VLCStyle.margin_normal

    headerColor: VLCStyle.colors.bg
    spacing: VLCStyle.margin_small

    onActionForSelection: {
        var list = []
        for (var i = 0; i < selection.count; i++ ) {
            list.push(selection.get(i).model.id)
        }
        medialib.addAndPlay(list)
    }

}
