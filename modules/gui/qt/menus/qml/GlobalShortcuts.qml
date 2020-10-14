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

Item {
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+O"; onActivated: dialogProvider.simpleOpenDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+Shift+O"; onActivated: dialogProvider.openFileDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+F"; onActivated: dialogProvider.PLOpenDir(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+D"; onActivated: dialogProvider.openDiscDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+N"; onActivated: dialogProvider.openNetDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+C"; onActivated: dialogProvider.openCaptureDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+V"; onActivated: dialogProvider.openUrlDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+Y"; onActivated: dialogProvider.savePlayingToPlaylist(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+R"; onActivated: dialogProvider.openAndTranscodingDialogs(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+S"; onActivated: dialogProvider.openAndStreamingDialogs(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+Q"; onActivated: dialogProvider.quit(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+E"; onActivated: dialogProvider.extendedDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+I"; onActivated: dialogProvider.mediaInfoDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+J"; onActivated: dialogProvider.mediaCodecDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+M"; onActivated: dialogProvider.messagesDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+P"; onActivated: dialogProvider.prefsDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+B"; onActivated: dialogProvider.bookmarksDialog(); enabled: !!medialib}
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+T"; onActivated: dialogProvider.gotoTimeDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"F1";     onActivated: dialogProvider.helpDialog(); }

    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+Shift+W"; onActivated: dialogProvider.vlmDialog(); enabled: mainInterface.hasVLM; }

    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+L"; onActivated: mainInterface.playlistVisible = !mainInterface.playlistVisible; }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"F11"; onActivated: mainInterface.toggleInterfaceFullScreen(); }


}
