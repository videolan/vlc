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

.pragma library

.import QtQuick 2.11 as QtQuick

function matchLeft(event) {
    return event.key === Qt.Key_Left
        || event.matches(QtQuick.StandardKey.MoveToPreviousChar)
}

function matchRight(event) {
    return event.key === Qt.Key_Right
        || event.matches(QtQuick.StandardKey.MoveToNextChar)
}

function matchUp(event) {
    return event.key === Qt.Key_Up
        || event.matches(QtQuick.StandardKey.MoveToPreviousLine)
}

function matchDown(event) {
    return event.key === Qt.Key_Down
        || event.matches(QtQuick.StandardKey.MoveToNextLine)
}

function matchPageDown(event) {
    return event.key === Qt.Key_PageDown
        || event.matches(QtQuick.StandardKey.MoveToNextPage)
}

function matchPageUp(event) {
    return event.key === Qt.Key_PageUp
        || event.matches(QtQuick.StandardKey.MoveToPreviousPage)
}

function matchOk( event ) {
    return event.key === Qt.Key_Space
        || event.matches(QtQuick.StandardKey.InsertParagraphSeparator)
}

function matchSearch( event ) {
    return event.key === Qt.Key_Search
        || event.key === Qt.Key_Slash
        || ( (event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_F )
}

function matchCancel(event) {
    return event.key === Qt.Key_Backspace
        || event.key === Qt.Key_Back
        || event.key === Qt.Key_Cancel
        || event.matches(QtQuick.StandardKey.Back)
        || event.matches(QtQuick.StandardKey.Cancel)
}
