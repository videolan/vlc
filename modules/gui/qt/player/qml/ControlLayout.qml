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
    id: controlLayout

    property alias model: repeater.model

    readonly property real minimumWidth: {
        var minimumWidth = 0

        for (var i = 0; i < repeater.count; ++i) {
            var item = repeater.itemAt(i).item

            if (item.minimumWidth !== undefined)
                minimumWidth += item.minimumWidth
            else
                minimumWidth += item.width
        }

        minimumWidth += ((repeater.count - 1) * rowLayout.spacing)

        return minimumWidth
    }
    property real extraWidth: 0
    property int expandableCount: 0 // widget count that can expand when extra width is available

    Navigation.navigable: {
        for (var i = 0; i < repeater.count; ++i) {
            if (repeater.itemAt(i).item.focus) {
                return true
            }
        }
        return false
    }

    implicitWidth: rowLayout.implicitWidth
    implicitHeight: rowLayout.implicitHeight

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
        id: rowLayout

        anchors.fill: parent

        spacing: playerButtonsLayout.spacing

        Repeater {
            id: repeater

            onItemRemoved: {
                if (item.item.extraWidth !== undefined)
                    controlLayout.expandableCount--

                item.recoverFocus(index)
            }

            delegate: Loader {
                id: loader

                source: PlayerControlbarControls.control(model.id).source

                focus: (index === 0)

                function buildFocusChain() {
                    // rebuild the focus chain:
                    if (typeof repeater === "undefined")
                        return

                    var rightItem = repeater.itemAt(index + 1)
                    var leftItem = repeater.itemAt(index - 1)

                    item.Navigation.rightItem = !!rightItem ? rightItem.item : null
                    item.Navigation.leftItem = !!leftItem ? leftItem.item : null
                }

                Component.onCompleted: {
                    repeater.countChanged.connect(loader.buildFocusChain)
                    mainInterface.controlbarProfileModel.selectedProfileChanged.connect(loader.buildFocusChain)
                    mainInterface.controlbarProfileModel.currentModel.dirtyChanged.connect(loader.buildFocusChain)
                }

                onActiveFocusChanged: {
                    if (activeFocus && (!!item && !item.focus)) {
                        recoverFocus()
                    }
                }

                Connections {
                    target: item

                    enabled: loader.status === Loader.Ready

                    onEnabledChanged: {
                        if (activeFocus && !item.enabled) // Loader has focus but item is not enabled
                            recoverFocus()
                    }
                }

                onLoaded: {
                    // control should not request focus if they are not enabled:
                    item.focus = Qt.binding(function() { return item.enabled })

                    // navigation parent of control is always controlLayout
                    // so it can be set here unlike leftItem and rightItem:
                    item.Navigation.parentItem = controlLayout

                    if (item instanceof Widgets.IconToolButton)
                        item.size = Qt.binding(function() { return defaultSize; })

                    // force colors:
                    if (!!colors && !!item.colors) {
                        item.colors = Qt.binding(function() { return colors; })
                    }

                    if (item.extraWidth !== undefined && controlLayout.extraWidth !== undefined) {
                        controlLayout.expandableCount++
                        item.extraWidth = Qt.binding( function() {
                            return (controlLayout.extraWidth / controlLayout.expandableCount) // distribute extra width
                        } )
                    }
                }

                function _focusIfFocusable(_loader) {
                    if (!!_loader && !!_loader.item && _loader.item.focus) {
                        if (item.focusReason !== undefined)
                            _loader.item.forceActiveFocus(item.focusReason)
                        else {
                            console.warn("focusReason is not available in %1!".arg(item))
                            _loader.item.forceActiveFocus()
                        }
                        return true
                    } else {
                        return false
                    }
                }

                function recoverFocus(_index) {
                    if (_index === undefined)
                        _index = index

                    for (var i = 1; i <= Math.max(_index, repeater.count - (_index + 1)); ++i) {
                         if (i <= _index) {
                             var leftItem = repeater.itemAt(_index - i)

                             if (_focusIfFocusable(leftItem))
                                 return
                         }

                         if (_index + i <= repeater.count - 1) {
                             var rightItem = repeater.itemAt(_index + i)

                             if (_focusIfFocusable(rightItem))
                                 return
                         }
                    }

                    // focus to other alignment if focusable control
                    // in the same alignment is not found:
                    if (!!controlLayout.Navigation.rightItem) {
                        controlLayout.Navigation.defaultNavigationRight()
                    } else if (!!controlLayout.Navigation.leftItem) {
                        controlLayout.Navigation.defaultNavigationLeft()
                    } else {
                        controlLayout.altFocusAction()
                    }
                }
            }
        }
    }
}
