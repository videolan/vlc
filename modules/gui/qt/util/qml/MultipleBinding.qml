/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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
import QtQml 2.11
import QtQml.Models 2.11

import org.videolan.compat 0.1

QtObject {
    id: root

    property ListModel model
    property alias enabled: instantiator.active
    property alias asynchronous: instantiator.asynchronous

    property QtObject target: null
    property bool when
    property bool delayed: false

    readonly property QtObject _instantiator: Instantiator {
        id: instantiator

        model: root.model

        delegate: BindingCompat {
            target: model.target ? model.target
                                 : root.target
            when: model.when !== undefined ? model.when
                                           : root.when
            property: model.property
            value: model.value
            delayed: model.delayed !== undefined ? model.delayed
                                                 : root.delayed
        }
    }
}
