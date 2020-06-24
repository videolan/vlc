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
import org.videolan.vlc 0.1


NavigableFocusScope {
    id: navigableRow

    property alias model: rowRepeater.model
    property alias delegate: rowRepeater.delegate
    property alias spacing: row.spacing

    width: row.width
    height: row.height
    property alias implicitWidth: row.implicitWidth
    property alias implicitHeight: row.implicitHeight

    Keys.priority: Keys.AfterItem
    Keys.onPressed: defaultKeyAction(event, 0)

    navigable: _countEnabled > 0
    property int _countEnabled: 0

    Component {
        id: enabledConnection
        Connections {
            onEnabledChanged: {
                navigableRow._countEnabled += ( target.enabled ? 1 : -1)
            }
        }
    }

    Row {
        id: row

        Repeater{
            id: rowRepeater

            onItemAdded: {
                if (item.enabled) {
                    navigableRow._countEnabled += 1
                }
                enabledConnection.createObject(item, {target: item})

                item.Keys.pressed.connect(function(event) {
                    if (event.accepted)
                        return
                    var i = index
                    if (event.key ===  Qt.Key_Left) {
                        do {
                            i--;
                        } while (i >= 0 && (!rowRepeater.itemAt(i).enabled || !rowRepeater.itemAt(i).visible))

                        if (i === -1) {
                            navigableRow.navigationLeft()
                        } else {
                            rowRepeater.itemAt(i).forceActiveFocus()
                        }
                        event.accepted = true

                    } else if (event.key ===  Qt.Key_Right) {
                        do {
                            i++;
                        } while (i < rowRepeater.count && (!rowRepeater.itemAt(i).enabled || !rowRepeater.itemAt(i).visible))

                        if (i === rowRepeater.count) {
                            navigableRow.navigationRight()
                        } else {
                            rowRepeater.itemAt(i).forceActiveFocus()
                        }
                        event.accepted = true
                    }
                })
            }

            onItemRemoved:  {
                if (item.enabled) {
                    navigableRow._countEnabled -= 1
                }
            }
        }
    }

    onActiveFocusChanged: {
        if (activeFocus) {
            var firstWithoutFocus = undefined
            for (var i = 0 ; i < rowRepeater.count; i++) {
                var item= rowRepeater.itemAt(i)
                if (item.enabled && item.visible) {
                    //already an item with the focus, keep it this way
                    if (item.focus )
                        return
                    else if (!firstWithoutFocus)
                        firstWithoutFocus = item
                }
            }
            if (firstWithoutFocus)
                firstWithoutFocus.focus = true
            return
        }
    }
}
