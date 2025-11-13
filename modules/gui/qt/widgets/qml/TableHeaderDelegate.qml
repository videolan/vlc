/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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
import VLC.Style

// FIXME: Qt 6.4
// using QObject/QGadget types in Qt.binding in createObject fails
// see https://bugreports.qt.io/browse/QTBUG-125095
// even typing as var seems to be broken with 6.4, so instead we
// create a local CellModel object that contains the bindings and pass
// it (without bindings) to createObject
Item {
    required property CellModel cellModel

    //we can't use `alias` to reference onything else than a child's property
    readonly property var colModel: cellModel.colModel
    readonly property ColorContext colorContext: cellModel.colorContext

    component CellModel: QtObject {
        required property var colModel
        required property ColorContext colorContext
    }

}
