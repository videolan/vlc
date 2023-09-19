/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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

QtObject {
    property bool active: false

    ///initial sub state
    property var initialState: null

    /**
     * @typedef {Object} TransitionDefinition
     * @property {Function} [guard] transition is only evaluated if this function returns true
     * @property {Function} [action] action executed on transition
     * @property {FSMState} [target] target state, if not defined, FSM will stay in
     *           current state
     */

    /**
     * dictionnary containing definition of transitions
     * key is the signal name as defined in
     * @type {Object<string:null|FSMState|TransitionDefinition|TransitionDefinition[]>}
     */
    property var transitions: ({})


    default property list<QtObject> _children

    //subStates
    property QtObject _state: null
    property var _parentStates: []
    property var _subStates: []
}
