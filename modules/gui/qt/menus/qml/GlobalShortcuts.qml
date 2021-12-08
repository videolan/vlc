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

import org.videolan.vlc 0.1

Item {
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+O"; onActivated: DialogsProvider.simpleOpenDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+Shift+O"; onActivated: DialogsProvider.openFileDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+F"; onActivated: DialogsProvider.PLOpenDir(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+D"; onActivated: DialogsProvider.openDiscDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+N"; onActivated: DialogsProvider.openNetDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+C"; onActivated: DialogsProvider.openCaptureDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+V"; onActivated: DialogsProvider.openUrlDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+Y"; onActivated: DialogsProvider.savePlayingToPlaylist(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+R"; onActivated: DialogsProvider.openAndTranscodingDialogs(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+S"; onActivated: DialogsProvider.openAndStreamingDialogs(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+Q"; onActivated: DialogsProvider.quit(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+E"; onActivated: DialogsProvider.extendedDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+I"; onActivated: DialogsProvider.mediaInfoDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+J"; onActivated: DialogsProvider.mediaCodecDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+M"; onActivated: DialogsProvider.messagesDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+P"; onActivated: DialogsProvider.prefsDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+T"; onActivated: DialogsProvider.gotoTimeDialog(); }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"F1";     onActivated: DialogsProvider.helpDialog(); }

    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+Shift+W"; onActivated: DialogsProvider.vlmDialog(); enabled: MainCtx.hasVLM; }

    Shortcut{ context: Qt.ApplicationShortcut; sequence:"Ctrl+L"; onActivated: MainCtx.playlistVisible = !MainCtx.playlistVisible; }
    Shortcut{ context: Qt.ApplicationShortcut; sequence:"F11"; onActivated: MainCtx.toggleInterfaceFullScreen(); }

    Loader {
        active: MainCtx.mediaLibraryAvailable
        source: "qrc:///menus/GlobalShortcutsMedialib.qml"
    }
}
