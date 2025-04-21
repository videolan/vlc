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
import QtQuick
import QtQuick.Controls
import QtQml.Models


import VLC.MainInterface
import VLC.Util
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Playlist
import VLC.Network

FocusScope {
    id: root

    // Properties

    //behave like a Page
    property var pagePrefix: []

    readonly property bool hasGridListMode: false
    readonly property bool isSearchable: urlListDisplay.active
                                    && urlListDisplay.item.isSearchable !== undefined
                                    && urlListDisplay.item.isSearchable

    property int leftPadding: 0
    property int rightPadding: 0

    property int displayMarginEnd: 0

    property bool enableBeginningFade: true
    property bool enableEndFade: true

    //---------------------------------------------------------------------------------------------
    // Functions
    //---------------------------------------------------------------------------------------------

    function setCurrentItemFocus(reason) {
        searchField.forceActiveFocus(reason);
    }

    //---------------------------------------------------------------------------------------------
    // Childs
    //---------------------------------------------------------------------------------------------

    Column {
        anchors.fill: parent

        FocusScope {
            id: searchFieldContainer

            width: root.width
            height: searchField.height + VLCStyle.margin_normal * 2
            focus: true

            Navigation.parentItem:  root
            Navigation.downItem: urlListDisplay.item ?? null

            Widgets.TextFieldExt {
                id: searchField

                focus: true
                anchors.centerIn: parent
                height: VLCStyle.dp(32, VLCStyle.scale)
                width: root.width * .6
                placeholderText: qsTr("Paste or write the URL here")
                selectByMouse: true

                onAccepted: {
                    if (urlListDisplay.status == Loader.Ready)
                        urlListDisplay.item.model.addAndPlay(text)
                    else
                        MainPlaylistController.append([text], true)
                }

                Keys.priority: Keys.AfterItem
                Keys.onPressed: (event) => searchFieldContainer.Navigation.defaultKeyAction(event)

                //ideally we should use Keys.onShortcutOverride but it doesn't
                //work with TextField before 5.13 see QTBUG-68711
                onActiveFocusChanged: {
                    if (activeFocus)
                        MainCtx.useGlobalShortcuts = false
                    else
                        MainCtx.useGlobalShortcuts = true
                }
            }
        }

        Loader {
            id: urlListDisplay

            width: parent.width
            height: parent.height - searchFieldContainer.height

            active: MainCtx.mediaLibraryAvailable
            source: "qrc:///qt/qml/VLC/MediaLibrary/UrlListDisplay.qml"

            onLoaded: {
                item.leftPadding = Qt.binding(function() {
                    return root.leftPadding
                })

                item.rightPadding = Qt.binding(function() {
                    return root.rightPadding
                })

                item.displayMarginEnd = Qt.binding(() => { return root.displayMarginEnd })

                item.fadingEdge.enableBeginningFade = Qt.binding(() => { return root.enableBeginningFade })
                item.fadingEdge.enableEndFade = Qt.binding(() => { return root.enableEndFade })

                item.Navigation.upItem = searchField
                item.Navigation.parentItem =  root

                item.searchPattern = Qt.binding(() => MainCtx.search.pattern)
            }
        }
    }
}
