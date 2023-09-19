
/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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
import QtQml

import org.videolan.vlc 0.1

/**
  * save and restore global context properties when view is changing
  */
QtObject {
    id: root

    readonly property string _sortCriteriaKey: "sortCriteria"
    readonly property string _sortOrderKey: "sortOrder"

    function save(path) {
        const orderKey = [_sortOrderKey, ...path].join("/")
        const critKey = [_sortCriteriaKey, ...path].join("/")
        if (MainCtx.sort.available) {
            MainCtx.setSettingValue(orderKey, MainCtx.sort.order)
            MainCtx.setSettingValue(critKey, MainCtx.sort.criteria)
        }
    }

    function restore(path) {
        const orderKey = [_sortOrderKey, ...path].join("/")
        const critKey = [_sortCriteriaKey, ...path].join("/")

        const criteria = MainCtx.settingValue(critKey, undefined)
        if (criteria !== undefined)
            MainCtx.sort.criteria = criteria

        const order = MainCtx.settingValue(orderKey, undefined)
        if (order !== undefined)
            MainCtx.sort.order = parseInt(order)
    }
}
