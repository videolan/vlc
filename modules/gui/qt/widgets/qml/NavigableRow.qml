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
import QtQuick.Templates 2.4 as T

import org.videolan.vlc 0.1

T.Control {
    id: root

    // Properties

    property int indexFocus: -1

    property int _countEnabled: 0

    // Aliases

    property alias count: repeater.count

    property alias model: repeater.model
    property alias delegate: repeater.delegate

    // Settings

    implicitWidth: Math.max(background ? background.implicitWidth : 0,
        (contentItem ? contentItem.implicitWidth : 0) + leftPadding + rightPadding)
    implicitHeight: Math.max(background ? background.implicitHeight : 0,
        (contentItem ? contentItem.implicitHeight : 0) + topPadding + bottomPadding)

    Navigation.navigable: (_countEnabled > 0)

    // Events

    onIndexFocusChanged: if (_hasFocus()) _applyFocus()

    onActiveFocusChanged: {
        // NOTE: We try to restore the preferred focused item.
        if (!activeFocus || _applyFocus())
            return;

        // Next item
        if (focusReason === Qt.TabFocusReason) {
            for (var i = 0; i < count; i++) {
                var item = repeater.itemAt(i);

                if (item.visible && item.enabled) {
                    item.forceActiveFocus(Qt.TabFocusReason);

                    return;
                }
            }
        }
        // Previous item
        else if (focusReason === Qt.BacktabFocusReason) {
            for (var i = count -1; i >= 0; i--) {
                var item = repeater.itemAt(i);

                if (item.visible && item.enabled) {
                    item.forceActiveFocus(Qt.BacktabFocusReason);

                    return;
                }
            }
        }
        // NOTE: We make sure that one item has the focus.
        else {
            var itemFocus = undefined;

            for (var i = 0 ; i < count; i++) {
                var item = repeater.itemAt(i);

                if (item.visible && item.enabled) {
                    // NOTE: We already have a focused item, so we keep it this way.
                    if (item.activeFocus)
                        return;

                    if (itemFocus == undefined)
                        itemFocus = item;
                }
            }

            if (itemFocus)
                itemFocus.forceActiveFocus(focusReason);
        }
    }

    // Keys

    Keys.priority: Keys.AfterItem

    Keys.onPressed: root.Navigation.defaultKeyAction(event)

    // Functions

    function _applyFocus() {
        if (indexFocus < 0 || indexFocus >= count) return false;

        var item = repeater.itemAt(indexFocus);

        if (item.visible && item.enabled) {
            item.forceActiveFocus(focusReason);

            return true;
        }

        return false;
    }

    function _hasFocus() {
        for (var i = 0 ; i < count; i++) {
            if (repeater.itemAt(i).activeFocus)
                return true;
        }

        return false;
    }

    // Childs

    Component {
        id: enabledConnection

        Connections {
            onEnabledChanged: root._countEnabled += (target.enabled ? 1 : -1)
        }
    }

    contentItem: Row {
        spacing: root.spacing

        Repeater{
            id: repeater

            onItemAdded: {
                if (item.enabled) root._countEnabled += 1;

                enabledConnection.createObject(item, { target: item });

                item.Navigation.parentItem = root;

                item.Navigation.leftAction = function() {
                    var i = index;

                    do {
                        i--;
                    } while (i >= 0
                             &&
                             (!repeater.itemAt(i).enabled || !repeater.itemAt(i).visible));

                    if (i == -1)
                        root.Navigation.defaultNavigationLeft();
                    else
                        repeater.itemAt(i).forceActiveFocus(Qt.BacktabFocusReason);
                };

                item.Navigation.rightAction = function() {
                    var i = index;

                    do {
                        i++;
                    } while (i < count
                             &&
                             (!repeater.itemAt(i).enabled || !repeater.itemAt(i).visible));

                    if (i === count)
                        root.Navigation.defaultNavigationRight();
                    else
                        repeater.itemAt(i).forceActiveFocus(Qt.TabFocusReason);
                };
            }

            onItemRemoved: if (item.enabled) root._countEnabled -= 1
        }
    }
}
