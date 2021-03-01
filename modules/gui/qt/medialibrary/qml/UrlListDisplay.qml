
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
import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQml.Models 2.2

import org.videolan.vlc 0.1
import org.videolan.medialib 0.1

import "qrc:///util" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///util/KeyHelper.js" as KeyHelper
import "qrc:///style/"


Widgets.KeyNavigableTableView {
    id: listView_id

    readonly property int _nbCols: VLCStyle.gridColumnsForWidth(
                                       listView_id.availableRowWidth)
    property Component urlHeaderDelegate: Widgets.IconLabel {
        text: VLCIcons.history
        color: VLCStyle.colors.caption
    }

    visible: urlModel.count > 0
    model: urlModel
    selectionDelegateModel: selectionModel

    sortModel: [{
            "isPrimary": true,
            "criteria": "url",
            "width": VLCStyle.colWidth(Math.max(listView_id._nbCols - 1,
                                                1)),
            "text": i18n.qtr("Url"),
            "showSection": "url",
            headerDelegate: urlHeaderDelegate
        }, {
            "criteria": "last_played_date",
            "width": VLCStyle.colWidth(1),
            "showSection": "",
            "headerDelegate": tableColumns.timeHeaderDelegate,
            "showContextButton": true
        }]

    rowHeight: VLCStyle.listAlbumCover_height + VLCStyle.margin_xxsmall * 2
    headerColor: VLCStyle.colors.bg

    onActionForSelection: medialib.addAndPlay(model.getIdsForIndexes(
                                                  selection))
    onItemDoubleClicked: medialib.addAndPlay(model.id)
    onContextMenuButtonClicked: contextMenu.popup(selectionModel.selectedIndexes, menuParent.mapToGlobal(0,0))
    onRightClick: contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)

    MLUrlModel {
        id: urlModel

        ml: medialib
    }

    Util.SelectableDelegateModel {
        id: selectionModel
        model: urlModel
    }

    URLContextMenu {
        id: contextMenu
        model: urlModel
    }


    Widgets.TableColumns {
        id: tableColumns
    }
}
