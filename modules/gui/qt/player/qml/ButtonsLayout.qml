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
import QtQuick.Layouts 1.3
import QtQml.Models 2.11

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

Widgets.NavigableFocusScope {
    id: buttonsLayout

    property alias model: buttonsRepeater.model

    property real _minimumWidth: 0 // minimumWidth without spacing (minimumWidth of all controls inside)
    property real minimumWidth: _minimumWidth + ((buttonsRepeater.count - 1) * buttonrow.spacing) // minimumWidth with spacing
    property real extraWidth: undefined
    property int expandableCount: 0 // widget count that can expand when extra width is available

    implicitWidth: buttonrow.implicitWidth
    implicitHeight: buttonrow.implicitHeight

    visible: model.count > 0

    Keys.priority: Keys.AfterItem
    Keys.onPressed: {
        if (!event.accepted)
            defaultKeyAction(event, 0)
    }

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

                if (item.item.minimumWidth !== undefined)
                    buttonsLayout._minimumWidth -= item.item.minimumWidth
                else
                    buttonsLayout._minimumWidth -= item.item.width
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
                    if (!!colors) {
                        if (!!buttonloader.item.colors)
                            buttonloader.item.colors = Qt.binding(function() { return colors; })
                        else
                            // legacy color forcing for IconToolButton etc. :
                            if (!!buttonloader.item.color)
                                buttonloader.item.color = Qt.binding(function() { return colors.playerFg; })
                            if (!!buttonloader.item.bgColor)
                                buttonloader.item.bgColor = Qt.binding(function() {
                                    return VLCStyle.colors.setColorAlpha(colors.playerBg, 0.8); })
                            if (!!buttonloader.item.borderColor)
                                buttonloader.item.borderColor = Qt.binding(function() { return colors.playerBorder; })
                    }

                    if (index > 0)
                        buttonloader.item.KeyNavigation.left = buttonrow.children[index-1].item

                    if (buttonloader.item.navigationRight !== undefined)
                        buttonloader.item.navigationRight = buttonsLayout.navigationRight

                    if (buttonloader.item.navigationLeft !== undefined)
                        buttonloader.item.navigationLeft = buttonsLayout.navigationLeft

                    if (buttonloader.item.minimumWidth !== undefined)
                        buttonsLayout._minimumWidth += buttonloader.item.minimumWidth
                    else
                        buttonsLayout._minimumWidth += buttonloader.item.width

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
