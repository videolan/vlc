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

import QtQuick
import QtQuick.Templates as T

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

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    Navigation.navigable: (_countEnabled > 0)

    // Events

    onIndexFocusChanged: if (_hasFocus()) _applyFocus()

    onActiveFocusChanged: {
        // NOTE: We try to restore the preferred focused item.
        if (!activeFocus || _applyFocus())
            return;

        // Next item
        if (focusReason === Qt.TabFocusReason) {
            for (let i = 0; i < count; i++) {
                const item = repeater.itemAt(i);

                if (item.visible && item.enabled) {
                    item.forceActiveFocus(Qt.TabFocusReason);

                    return;
                }
            }
        }
        // Previous item
        else if (focusReason === Qt.BacktabFocusReason) {
            for (let i = count -1; i >= 0; i--) {
                const item = repeater.itemAt(i);

                if (item.visible && item.enabled) {
                    item.forceActiveFocus(Qt.BacktabFocusReason);

                    return;
                }
            }
        }
        // NOTE: We make sure that one item has the focus.
        else {
            let itemFocus = undefined;

            for (let i = 0 ; i < count; i++) {
                const item = repeater.itemAt(i);

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

    Keys.onPressed: (event) => root.Navigation.defaultKeyAction(event)

    // Functions

    function _applyFocus() {
        if (indexFocus < 0 || indexFocus >= count) return false;

        const item = repeater.itemAt(indexFocus);

        if (item.visible && item.enabled) {
            item.forceActiveFocus(focusReason);

            return true;
        }

        return false;
    }

    function _hasFocus() {
        for (let i = 0 ; i < count; i++) {
            if (repeater.itemAt(i).activeFocus)
                return true;
        }

        return false;
    }

    // Childs

    contentItem: Row {
        spacing: root.spacing

        Repeater{
            id: repeater

            onItemAdded: (index, item) => {
                if (item.enabled) root._countEnabled += 1;

                item.onEnabledChanged.connect(() => { root._countEnabled += (item.enabled ? 1 : -1) })

                item.Navigation.parentItem = root;

                item.Navigation.leftAction = function() {
                    let i = index;

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
                    let i = index;

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

            onItemRemoved: (index, item) => {
                if (item.enabled) root._countEnabled -= 1
            }
        }
    }
}
