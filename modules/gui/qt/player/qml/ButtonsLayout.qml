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

    Navigation.navigable: {
        for (var i = 0; i < buttonsRepeater.count; ++i) {
            if (buttonsRepeater.itemAt(i).item.focus) {
                return true
            }
        }
        return false
    }

    implicitWidth: buttonrow.implicitWidth
    implicitHeight: buttonrow.implicitHeight

    property var altFocusAction: Navigation.defaultNavigationUp

    function _handleFocus() {
        if (typeof activeFocus === "undefined")
            return

        if (activeFocus && (!visible || model.count === 0))
            altFocusAction()
    }

    Component.onCompleted: {
        visibleChanged.connect(_handleFocus)
        activeFocusChanged.connect(_handleFocus)
        model.countChanged.connect(_handleFocus)
    }

    RowLayout {
        id: buttonrow

        anchors.fill: parent

        spacing: playerButtonsLayout.spacing

        Repeater {
            id: buttonsRepeater

            onItemRemoved: {
                if (item.item.extraWidth !== undefined)
                    buttonsLayout.expandableCount--

                item.recoverFocus(index)
            }

            delegate: Loader {
                id: buttonloader

                source: PlayerControlButtons.button(model.id).source

                focus: (index === 0)

                function buildFocusChain() {
                    // rebuild the focus chain:
                    if (typeof buttonsRepeater === "undefined")
                        return

                    var rightItem = buttonsRepeater.itemAt(index + 1)
                    var leftItem = buttonsRepeater.itemAt(index - 1)

                    item.Navigation.rightItem = !!rightItem ? rightItem.item : null
                    item.Navigation.leftItem = !!leftItem ? leftItem.item : null
                }

                Component.onCompleted: {
                    buttonsRepeater.countChanged.connect(buttonloader.buildFocusChain)
                    mainInterface.controlbarProfileModel.selectedProfileChanged.connect(buttonloader.buildFocusChain)
                    mainInterface.controlbarProfileModel.currentModel.dirtyChanged.connect(buttonloader.buildFocusChain)
                }

                onActiveFocusChanged: {
                    if (activeFocus && !item.focus) {
                        recoverFocus()
                    }
                }

                Connections {
                    target: item

                    enabled: buttonloader.status === Loader.Ready

                    onEnabledChanged: {
                        if (activeFocus && !item.enabled) // Loader has focus but item is not enabled
                            recoverFocus()
                    }
                }

                onLoaded: {
                    // control should not request focus if they are not enabled:
                    item.focus = Qt.binding(function() { return item.enabled })

                    // navigation parent of control is always buttonsLayout
                    // so it can be set here unlike leftItem and rightItem:
                    item.Navigation.parentItem = buttonsLayout

                    if (buttonloader.item instanceof Widgets.IconToolButton)
                        buttonloader.item.size = Qt.binding(function() { return defaultSize; })

                    // force colors:
                    if (!!colors && !!buttonloader.item.colors) {
                        buttonloader.item.colors = Qt.binding(function() { return colors; })
                    }

                    if (buttonloader.item.extraWidth !== undefined && buttonsLayout.extraWidth !== undefined) {
                        buttonsLayout.expandableCount++
                        buttonloader.item.extraWidth = Qt.binding( function() {
                            return (buttonsLayout.extraWidth / buttonsLayout.expandableCount) // distribute extra width
                        } )
                    }
                }

                function _focusIfFocusable(loader, reason) {
                    if (!!loader && !!loader.item && loader.item.focus) {
                        loader.item.forceActiveFocus(reason)
                        return true
                    } else {
                        return false
                    }
                }

                function recoverFocus(_index) {
                    if (_index === undefined)
                        _index = index

                    for (var i = 1; i <= Math.max(_index, buttonsRepeater.count - (_index + 1)); ++i) {
                         if (i <= _index) {
                             var leftItem = buttonsRepeater.itemAt(_index - i)

                             if (_focusIfFocusable(leftItem))
                                 return
                         }

                         if (_index + i <= buttonsRepeater.count - 1) {
                             var rightItem = buttonsRepeater.itemAt(_index + i)

                             if (_focusIfFocusable(rightItem))
                                 return
                         }
                    }

                    // focus to other alignment if focusable control
                    // in the same alignment is not found:
                    if (_index > (buttonsRepeater.count + 1) / 2) {
                        buttonsLayout.Navigation.defaultNavigationRight()
                    } else {
                        buttonsLayout.Navigation.defaultNavigationLeft()
                    }
                }
            }
        }
    }
}
