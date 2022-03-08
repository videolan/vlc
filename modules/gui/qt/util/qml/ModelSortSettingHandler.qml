
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
import QtQml 2.11

import org.videolan.vlc 0.1

QtObject {
    id: root

    property var _model: null
    property string _key: ""

    readonly property string _sortCriteriaKey: "sortCriteria/" + _key
    readonly property string _sortOrderKey: "sortOrder/" + _key

    property var _sortCriteriaConnection: Connections {
        target: !!root._model ? root._model : null

        enabled: !!root._model && root._model.hasOwnProperty("sortCriteria")

        onSortCriteriaChanged: {
            MainCtx.setSettingValue(root._sortCriteriaKey, _model.sortCriteria)
        }
    }

    property var _sortOrderConnection: Connections {
        target: !!root._model ? root._model : null

        enabled: !!root._model && root._model.hasOwnProperty("sortOrder")

        onSortOrderChanged: {
            MainCtx.setSettingValue(root._sortOrderKey, root._model.sortOrder)
        }
    }

    function set(model, key) {
        _model = model
        _key = key

        if (!_model)
            return

        if (_model.hasOwnProperty("sortCriteria"))
            _model.sortCriteria = MainCtx.settingValue(_sortCriteriaKey, _model.sortCriteria)

        // MainCtx.settingValue seems to change int -> string
        if (_model.hasOwnProperty("sortOrder"))
            _model.sortOrder = parseInt(MainCtx.settingValue(_sortOrderKey, _model.sortOrder))
    }
}
