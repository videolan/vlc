
/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Author: Prince Gupta <guptaprince8832@gmail.com>
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

import org.videolan.medialib 0.1

DragItem {
    id: root

    /* required */ property MLModel mlModel: null

    // string => role for medialib id, data[id] will be pass to Medialib::mlInputItem for SharedInputItem
    property string mlIDRole: "id"

    onRequestData: (indexes, resolve, reject) =>  {
        if (indexesFlat)
            mlModel.getDataFlat(indexes, resolve)
        else
            mlModel.getData(indexes, resolve)
    }

    onRequestInputItems: (indexes, data, resolve, reject) => {
        console.assert(mlIDRole)
        const inputIdList = data.map(o => o[root.mlIDRole])
        MediaLib.mlInputItem(inputIdList, resolve)
    }
}
