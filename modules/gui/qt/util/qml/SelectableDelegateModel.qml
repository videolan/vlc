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
import QtQml.Models 2.2

DelegateModel {
    id: delegateModel

    property int shiftIndex: -1
    property alias selectedGroup: selectedGroup
    readonly property bool hasSelection: selectedGroup.count > 0

    groups: [
        DelegateModelGroup { id: selectedGroup; name: "selected"; includeByDefault: false }
    ]

    function _addRange(from, to) {
        for (var i = from; i <= to; i++) {
            delegateModel.items.get(i).inSelected = true
        }
    }
    function _delRange(from, to) {
        for (var i = from; i <= to; i++) {
            delegateModel.items.get(i).inSelected = false
        }
    }

    function selectNone() {
        if (hasSelection)
            selectedGroup.remove(0,selectedGroup.count)
    }

    function selectAll() {
        delegateModel.items.addGroups(0, delegateModel.items.count, ["selected"])
    }

    function updateSelection( keymodifiers, oldIndex, newIndex ) {
        if ((keymodifiers & Qt.ShiftModifier)) {
            if ( shiftIndex === oldIndex) {
                if ( newIndex > shiftIndex )
                    _addRange(shiftIndex, newIndex)
                else
                    _addRange(newIndex, shiftIndex)
            } else if (shiftIndex <= newIndex && newIndex < oldIndex) {
                _delRange(newIndex + 1, oldIndex )
            } else if ( shiftIndex < oldIndex && oldIndex < newIndex ) {
                _addRange(oldIndex, newIndex)
            } else if ( newIndex < shiftIndex && shiftIndex < oldIndex ) {
                _delRange(shiftIndex, oldIndex)
                _addRange(newIndex, shiftIndex)
            } else if ( newIndex < oldIndex && oldIndex < shiftIndex  ) {
                _addRange(newIndex, oldIndex)
            } else if ( oldIndex <= shiftIndex && shiftIndex < newIndex ) {
                _delRange(oldIndex, shiftIndex)
                _addRange(shiftIndex, newIndex)
            } else if ( oldIndex < newIndex && newIndex <= shiftIndex  ) {
                _delRange(oldIndex, newIndex - 1)
            }
        } else {
            var e = delegateModel.items.get(newIndex)
            if ((keymodifiers & Qt.ControlModifier) == Qt.ControlModifier) {
                e.inSelected = !e.inSelected
            } else {
                selectNone()
                e.inSelected = true
            }
            shiftIndex = newIndex
        }
    }
}
