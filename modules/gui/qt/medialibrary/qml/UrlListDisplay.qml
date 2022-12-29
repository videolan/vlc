
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
import "qrc:///style/"


Widgets.KeyNavigableTableView {
    id: listView_id

    readonly property int _nbCols: VLCStyle.gridColumnsForWidth(
                                       listView_id.availableRowWidth)
    property Component urlHeaderDelegate: Widgets.IconLabel {
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: VLCStyle.icon_tableHeader
        text: VLCIcons.history
        color: VLCStyle.colors.caption
    }

    visible: urlModel.count > 0
    model: urlModel
    selectionDelegateModel: selectionModel

    sortModel: [{
        size: Math.max(listView_id._nbCols - 1, 1),

        model: {
            criteria: url,

            text: I18n.qtr("Url"),

            showSection: url,

            headerDelegate: urlHeaderDelegate
        }
    }, {
        size: 1,

        model: {
            criteria: last_played_date,

            showSection: "",
            showContextButton: true,

            headerDelegate: tableColumns.timeHeaderDelegate
        }
    }]

    rowHeight: VLCStyle.listAlbumCover_height + VLCStyle.margin_xxsmall * 2
    headerColor: VLCStyle.colors.bg

    onActionForSelection: MediaLib.addAndPlay(model.getIdsForIndexes(
                                                  selection))
    onItemDoubleClicked: MediaLib.addAndPlay(model.id)
    onContextMenuButtonClicked: contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
    onRightClick: contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)

    MLUrlModel {
        id: urlModel

        ml: MediaLib
    }

    Util.SelectableDelegateModel {
        id: selectionModel
        model: urlModel
    }

    Util.MLContextMenu {
        id: contextMenu

        model: urlModel
    }


    Widgets.TableColumns {
        id: tableColumns
    }
}
