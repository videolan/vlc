
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


// @brief - a class that can be used to create native menus with support
// to asyncronously retreive data from MLBaseModel like model

VanillaObject {
    id: root

    /**
      actions - list of action to show

      each action is an Object and contains following keys
        "text" - (string) display text of the action

        "action" - (function(dataList, options, indexes)) a function that will be called when action is selected

        "visible" - (Boolean or function(options, indexes) -> Boolean) (optional) a boolean value or function which
                    controls whether the action is shown in the menu following 'popup' call

       e.g see MLContextMenu.qml
    **/

    property var actions: []

    signal requestData(var requestID, var indexes)


    property var _options: null

    property var _indexes: []

    property bool _pendingData: false

    property var _dataList: null

    property int _currentRequest: 0

    property int _actionOnDataReceived: -1

    property var _effectiveActions: null


    function popup(_indexes, point, _options) {
        root._options = _options
        root._indexes = _indexes
        _actionOnDataReceived = -1
        _pendingData = true
        _dataList = null

        var requestID = ++_currentRequest
        requestData(requestID, _indexes)

        var textStrings = []
        _effectiveActions = []
        for (var i in actions) {
            if (!actions[i].hasOwnProperty("visible")
                    || (typeof actions[i].visible === "boolean" && actions[i].visible)
                    || (typeof actions[i].visible === "function" && actions[i].visible(_options, _indexes))) {
                _effectiveActions.push(actions[i])
                textStrings.push(actions[i].text)
            }
        }

        menu.popup(point, textStrings)
    }

    function setData(id, data) {
        if (_currentRequest !== id)
            return;

        _dataList = data
        _pendingData = false

        if (_actionOnDataReceived !== -1)
           _executeAction(_actionOnDataReceived)
    }

    function _executeAction(index) {
        var action = root._effectiveActions[index]
        action.action(_dataList, _options, _indexes)
    }


    StringListMenu {
        id: menu

        onSelected: {
            if (root._pendingData)
                root._actionOnDataReceived = index
            else
                root._executeAction(index)
        }
    }
}
