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

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/KeyHelper.js" as KeyHelper
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: playlistToolbar

    property int leftPadding: 0
    property int rightPadding: 0
    height: VLCStyle.heightBar_normal

    property VLCColors _colors: VLCStyle.colors

    Rectangle {
        anchors.fill: parent
        color: _colors.banner

        RowLayout {
            anchors {
                fill: parent
                leftMargin: playlistToolbar.leftPadding
                rightMargin: playlistToolbar.rightPadding
            }

            spacing: VLCStyle.margin_normal

            Widgets.IconToolButton {
                id: loop
                Layout.alignment: Qt.AlignHCenter
                //Layout.minimumWidth: VLCStyle.icon_normal * 2
                size: VLCStyle.icon_normal
                iconText: (mainPlaylistController.repeatMode === PlaylistControllerModel.PLAYBACK_REPEAT_CURRENT)
                          ? VLCIcons.repeat_one
                          : VLCIcons.repeat_all
                checked: mainPlaylistController.repeatMode !== PlaylistControllerModel.PLAYBACK_REPEAT_NONE
                onClicked: mainPlaylistController.toggleRepeatMode()
                focusPolicy: Qt.NoFocus

                color: _colors.buttonText
                colorDisabled: _colors.textInactive
            }

            Widgets.IconToolButton {
                id: shuffle
                Layout.alignment: Qt.AlignHCenter
                //Layout.minimumWidth: VLCStyle.icon_normal * 2
                enabled: !mainPlaylistController.empty
                size: VLCStyle.icon_normal
                iconText: VLCIcons.shuffle_on
                onClicked: mainPlaylistController.shuffle()
                focusPolicy: Qt.NoFocus

                color: _colors.buttonText
                colorDisabled: _colors.textInactive
            }

            Widgets.SortControl {
                id: sort
                Layout.alignment: Qt.AlignHCenter
                //Layout.minimumWidth: VLCStyle.icon_normal * 2
                popupAlignment: Qt.AlignRight | Qt.AlignTop

                model: [
                    { text: i18n.qtr("Tile"),             criteria: PlaylistControllerModel.SORT_KEY_TITLE },
                    { text: i18n.qtr("Duration"),         criteria: PlaylistControllerModel.SORT_KEY_DURATION },
                    { text: i18n.qtr("Artist"),           criteria: PlaylistControllerModel.SORT_KEY_ARTIST },
                    { text: i18n.qtr("Album"),            criteria: PlaylistControllerModel.SORT_KEY_ALBUM },
                    { text: i18n.qtr("Genre"),            criteria: PlaylistControllerModel.SORT_KEY_GENRE },
                    { text: i18n.qtr("Date"),             criteria: PlaylistControllerModel.SORT_KEY_DATE },
                    { text: i18n.qtr("Track number"),     criteria: PlaylistControllerModel.SORT_KEY_TRACK_NUMBER },
                    { text: i18n.qtr("URL"),              criteria: PlaylistControllerModel.SORT_KEY_URL },
                    { text: i18n.qtr("Rating"),           criteria: PlaylistControllerModel.SORT_KEY_RATIN },
                ]
                textRole: "text"

                listWidth: VLCStyle.widthSortBox
                onSortSelected: {
                    mainPlaylistController.sort(modelData.criteria, PlaylistControllerModel.SORT_ORDER_ASC)
                }

                Keys.priority: Keys.AfterItem
                Keys.onPressed: defaultKeyAction(event, 0)
                navigationParent: playlistToolbar

                _colors: playlistToolbar._colors
            }

            Widgets.IconToolButton {
                id: clear
                Layout.alignment: Qt.AlignHCenter
                //Layout.minimumWidth: VLCStyle.icon_normal * 2
                size: VLCStyle.icon_normal
                enabled: !mainPlaylistController.empty
                iconText: VLCIcons.playlist_clear
                onClicked: mainPlaylistController.clear()
                focusPolicy: Qt.NoFocus

                color: _colors.buttonText
                colorDisabled: _colors.textInactive
            }
        }
    }
}

