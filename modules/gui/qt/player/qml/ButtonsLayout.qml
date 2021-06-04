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
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11
import QtQml.Models 2.11

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

FocusScope {
    id: buttonsLayout

    property alias model: buttonsRepeater.model

    readonly property real minimumWidth: {
        var minimumWidth = 0

        for (var i = 0; i < buttonsRepeater.count; ++i) {
            var item = buttonsRepeater.itemAt(i).item

            if (item.minimumWidth !== undefined)
                minimumWidth += item.minimumWidth
            else
                minimumWidth += item.width
        }

        minimumWidth += ((buttonsRepeater.count - 1) * buttonrow.spacing)

        return minimumWidth
    }
    property real extraWidth: 0
    property int expandableCount: 0 // widget count that can expand when extra width is available

    implicitWidth: buttonrow.implicitWidth
    implicitHeight: buttonrow.implicitHeight

    visible: model.count > 0

    RowLayout {
        id: buttonrow
        property bool _focusGiven: false

        anchors.fill: parent

        spacing: playerButtonsLayout.spacing

        Repeater {
            id: buttonsRepeater

            onItemRemoved: {
                if (item.focus) {
                    buttonrow._focusGiven = false
                }

                if (item.item.extraWidth !== undefined)
                    buttonsLayout.expandableCount--
            }

            delegate: Loader {
                id: buttonloader

                sourceComponent: controlmodelbuttons.returnbuttondelegate(model.id)

                onLoaded: {
                    if (!buttonrow._focusGiven) {
                        buttonloader.focus = true
                        buttonrow._focusGiven = true
                    }
                    buttonloader.item.focus = true

                    if (buttonloader.item instanceof Widgets.IconToolButton)
                        buttonloader.item.size = Qt.binding(function() { return defaultSize; })

                    // force colors:
                    if (!!colors && !!buttonloader.item.colors) {
                        buttonloader.item.colors = Qt.binding(function() { return colors; })
                    }

                    buttonloader.item.Navigation.parentItem = buttonsLayout
                    if (index > 0) {
                        buttonloader.item.Navigation.leftItem = buttonrow.children[index-1].item
                        buttonrow.children[index-1].item.Navigation.rightItem = buttonloader.item
                   }

                    if (buttonloader.item.extraWidth !== undefined && buttonsLayout.extraWidth !== undefined) {
                        buttonsLayout.expandableCount++
                        buttonloader.item.extraWidth = Qt.binding( function() {
                            return (buttonsLayout.extraWidth / buttonsLayout.expandableCount) // distribute extra width
                        } )
                    }
                }
            }
        }
    }
}
