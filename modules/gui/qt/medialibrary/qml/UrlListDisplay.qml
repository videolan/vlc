
/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import QtQml.Models 2.2

import org.videolan.medialib 0.1

import "qrc:///util" as Util
import "qrc:///widgets/" as Widgets
import "qrc:///util/KeyHelper.js" as KeyHelper
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: root

    MLUrlModel {
        id: urlModel

        ml: medialib
    }

    Util.SelectableDelegateModel {
        id: selectionModel
        model: urlModel
    }

    URLContextMenu {
        id: contextMenu
        model: urlModel
    }

    Column {
        anchors.fill: parent

        Item {
            id: searchFieldContainer

            width: root.width
            height: searchField.height + VLCStyle.margin_normal * 2

            TextField {
                id: searchField

                focus: true
                anchors.centerIn: parent
                height: VLCStyle.dp(32, VLCStyle.scale)
                width: VLCStyle.colWidth(Math.max(VLCStyle.gridColumnsForWidth(root.width * .6), 2))
                placeholderText: i18n.qtr("Paste or write the URL here")
                color: VLCStyle.colors.text
                font.pixelSize: VLCStyle.fontSize_large
                background: Rectangle {
                    color: VLCStyle.colors.bg
                    border.width: VLCStyle.dp(2, VLCStyle.scale)
                    border.color: searchField.activeFocus || searchField.hovered
                                  ? VLCStyle.colors.accent
                                  : VLCStyle.colors.setColorAlpha(VLCStyle.colors.text, .4)
                }

                onAccepted: {
                    urlModel.addAndPlay(text)
                }

                Keys.onPressed: {
                    if (event.accepted)
                        return
                    if (KeyHelper.matchUp(event)) {
                        root.navigationUp()
                        event.accepted = true
                    } else if (KeyHelper.matchDown(event)) {
                        listView_id.forceActiveFocus()
                        event.accepted = true
                    } else if (KeyHelper.matchLeft(event)) {
                        root.navigationLeft()
                        event.accepted = true
                    } else if (KeyHelper.matchRight(event)) {
                        root.navigationRight()
                        event.accepted = true
                    }
                }
            }
        }


        Widgets.KeyNavigableTableView {
            id: listView_id

            readonly property int _nbCols: VLCStyle.gridColumnsForWidth(
                                               listView_id.availableRowWidth)
            property Component urlHeaderDelegate: Widgets.IconLabel {
                text: VLCIcons.history
                color: VLCStyle.colors.caption
            }

            visible: urlModel.count > 0
            width: parent.width
            height: parent.height - searchFieldContainer.height
            model: urlModel
            selectionDelegateModel: selectionModel

            sortModel: [{
                    "isPrimary": true,
                    "criteria": "url",
                    "width": VLCStyle.colWidth(Math.max(listView_id._nbCols - 1,
                                                        1)),
                    "text": i18n.qtr("Url"),
                    "showSection": "url",
                    headerDelegate: urlHeaderDelegate
                }, {
                    "criteria": "last_played_date",
                    "width": VLCStyle.colWidth(1),
                    "showSection": "",
                    "headerDelegate": tableColumns.timeHeaderDelegate,
                    "showContextButton": true
                }]

            rowHeight: VLCStyle.listAlbumCover_height + VLCStyle.margin_xxsmall * 2
            headerColor: VLCStyle.colors.bg
            navigationUpItem: searchField
            navigationParent: root

            navigationLeft: function (index) {
                if (isFocusOnContextButton)
                    isFocusOnContextButton = false
                else
                    defaultNavigationLeft(index)
            }
            navigationRight: function (index) {
                if (!isFocusOnContextButton)
                    isFocusOnContextButton = true
                else
                    defaultNavigationRight(index)
            }

            onActionForSelection: medialib.addAndPlay(model.getIdsForIndexes(
                                                          selection))

            onContextMenuButtonClicked: contextMenu.popup(selectionModel.selectedIndexes, menuParent.mapToGlobal(0,0))
            onRightClick: contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)


            Widgets.TableColumns {
                id: tableColumns
            }
        }
    }
}
