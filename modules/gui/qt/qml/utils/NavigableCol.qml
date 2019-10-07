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
    id: navigableCol

    property alias model: colRepeater.model
    property alias delegate: colRepeater.delegate

    width: col.width
    height: col.height
    property alias implicitWidth: col.implicitWidth
    property alias implicitHeight: col.implicitHeight

    Keys.priority: Keys.AfterItem
    Keys.onPressed: defaultKeyAction(event, 0)

    navigable: _countEnabled > 0
    property int _countEnabled: 0

    Component {
        id: enabledConnection
        Connections {
            onEnabledChanged: {
                navigableCol._countEnabled += ( target.enabled ? 1 : -1)
            }
        }
    }

    Column {
        id: col

        Repeater{
            id: colRepeater

            onItemAdded: {
                if (item.enabled) {
                    navigableCol._countEnabled += 1
                }
                enabledConnection.createObject(item, {target: item})

                item.Keys.pressed.connect(function(event) {
                    if (event.accepted)
                        return
                    var i = index
                    if (event.key ===  Qt.Key_Up) {
                        do {
                            i--;
                        } while (i >= 0 && (!colRepeater.itemAt(i).enabled || !colRepeater.itemAt(i).visible))

                        if (i === -1) {
                            navigableCol.navigationUp()
                        } else {
                            colRepeater.itemAt(i).forceActiveFocus()
                        }
                        event.accepted = true

                    } else if (event.key ===  Qt.Key_Down) {
                        do {
                            i++;
                        } while (i < colRepeater.count && (!colRepeater.itemAt(i).enabled || !colRepeater.itemAt(i).visible))

                        if (i === colRepeater.count) {
                            navigableCol.navigationDown()
                        } else {
                            colRepeater.itemAt(i).forceActiveFocus()
                        }
                        event.accepted = true
                    }
                })
            }

            onItemRemoved:  {
                if (item.enabled) {
                    navigableCol._countEnabled -= 1
                }
            }
        }
    }

    onActiveFocusChanged: {
        if (activeFocus) {
            var firstWithoutFocus = undefined
            for (var i = 0 ; i < colRepeater.count; i++) {
                var item= colRepeater.itemAt(i)
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
