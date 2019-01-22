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
import QtQuick.Layouts 1.3

import org.videolan.medialib 0.1

import "qrc:///utils/" as Utils
import "qrc:///style/"

Utils.KeyNavigableTableView {
    id: root

    sortModel: ListModel {
        ListElement{ criteria: "track_number";width:0.15; text: qsTr("TRACK NB"); showSection: "" }
        ListElement{ criteria: "disc_number"; width:0.15; text: qsTr("DISC NB");  showSection: "" }
        ListElement{ criteria: "title";       width:0.15; text: qsTr("TITLE");    showSection: "title" }
        ListElement{ criteria: "main_artist"; width:0.15; text: qsTr("ARTIST");   showSection: "main_artist" }
        ListElement{ criteria: "album_title"; width:0.15; text: qsTr("ALBUM");    showSection: "album_title" }
        ListElement{ criteria: "duration";    width:0.15; text: qsTr("DURATION"); showSection: "" }
    }

    model: MLAlbumTrackModel {
        id: rootmodel
        ml: medialib
    }

    property alias parentId: rootmodel.parentId

    onActionForSelection: {
        var list = []
        for (var i = 0; i < selection.count; i++ ) {
            list.push(selection.get(i).model.id)
        }
        medialib.addAndPlay(list)
    }

    Label {
        anchors.centerIn: parent
        visible: rootmodel.count === 0
        font.pixelSize: VLCStyle.fontHeight_xxlarge
        color: root.activeFocus ? VLCStyle.colors.accent : VLCStyle.colors.text
        text: qsTr("No tracks found")
    }
}
