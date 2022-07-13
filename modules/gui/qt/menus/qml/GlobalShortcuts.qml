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

    ShortcutExt{ context: Qt.WindowShortcut; sequence:"Ctrl+O"; onActivated: DialogsProvider.simpleOpenDialog(); }
    ShortcutExt{ sequence:"Ctrl+Shift+O"; onActivated: DialogsProvider.openFileDialog(); }
    ShortcutExt{ context: Qt.WindowShortcut; sequence:"Ctrl+F"; onActivated: DialogsProvider.PLOpenDir(); }
    ShortcutExt{ sequence:"Ctrl+D"; onActivated: DialogsProvider.openDiscDialog(); }
    ShortcutExt{ sequence:"Ctrl+N"; onActivated: DialogsProvider.openNetDialog(); }
    ShortcutExt{ sequence:"Ctrl+C"; onActivated: DialogsProvider.openCaptureDialog(); }
    ShortcutExt{ sequence:"Ctrl+V"; onActivated: DialogsProvider.openUrlDialog(); }
    ShortcutExt{ context: Qt.WindowShortcut; sequence:"Ctrl+Y"; onActivated: DialogsProvider.savePlayingToPlaylist(); }
    ShortcutExt{ sequence:"Ctrl+R"; onActivated: DialogsProvider.openAndTranscodingDialogs(); }
    ShortcutExt{ sequence:"Ctrl+S"; onActivated: DialogsProvider.openAndStreamingDialogs(); }
    ShortcutExt{ sequence:"Ctrl+Q"; onActivated: DialogsProvider.quit(); }
    ShortcutExt{ sequence:"Ctrl+E"; onActivated: DialogsProvider.extendedDialog(); }
    ShortcutExt{ sequence:"Ctrl+I"; onActivated: DialogsProvider.mediaInfoDialog(); }
    ShortcutExt{ sequence:"Ctrl+J"; onActivated: DialogsProvider.mediaCodecDialog(); }
    ShortcutExt{ sequence:"Ctrl+M"; onActivated: DialogsProvider.messagesDialog(); }
    ShortcutExt{ sequence:"Ctrl+P"; onActivated: DialogsProvider.prefsDialog(); }
    ShortcutExt{ sequence:"Ctrl+T"; onActivated: DialogsProvider.gotoTimeDialog(); }

    ShortcutExt{ sequence:"Ctrl+Shift+W"; onActivated: DialogsProvider.vlmDialog(); }

    ShortcutExt{ sequence:"Ctrl+L"; onActivated: MainCtx.playlistVisible = !MainCtx.playlistVisible; }

    ShortcutExt{ sequence:"F1"; onActivated: DialogsProvider.helpDialog() }
    ShortcutExt{ sequence:"F11"; onActivated: MainCtx.toggleInterfaceFullScreen() }

    Loader {
        active: MainCtx.mediaLibraryAvailable
        source: "qrc:///menus/GlobalShortcutsMedialib.qml"
    }
}
