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
import org.videolan.medialib 0.1

Item {
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+B"; onActivated: dialogProvider.bookmarksDialog(); }

    MLRecentModel {
        id: recentModel
        numberOfItemsToShow: 10
        ml: medialib
    }

    //build all the shortcuts everytime, it seems that they can't be added/removed dynamically
    Repeater {
        model: 10

        Item {
            Shortcut {
                sequence: "Ctrl+" + ((index + 1) % 10)
                onActivated:  {
                    if (index < recentModel.count)
                    {

                        var trackId = recentModel.data(recentModel.index(index, 0), MLRecentModel.RECENT_MEDIA_ID)
                        if (!!trackId)
                            medialib.addAndPlay([trackId])
                    }
                }
                context: Qt.ApplicationShortcut
            }
        }
    }
}
