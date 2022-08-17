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

pragma Singleton

import QtQml 2.11

import org.videolan.vlc 0.1

import "qrc:///player/controlbarcontrols/" as Controls
import "qrc:///widgets/" as Widgets
import "qrc:///style/"

QtObject {
    readonly property string controlPath : "qrc:///player/controlbarcontrols/"

    readonly property var controlList: [
        { id: ControlListModel.PLAY_BUTTON, file: "PlayButton.qml", label: VLCIcons.play, text: I18n.qtr("Play") },
        { id: ControlListModel.STOP_BUTTON, file: "StopButton.qml", label: VLCIcons.stop, text: I18n.qtr("Stop") },
        { id: ControlListModel.OPEN_BUTTON, file: "OpenButton.qml", label: VLCIcons.eject, text: I18n.qtr("Open") },
        { id: ControlListModel.PREVIOUS_BUTTON, file: "PreviousButton.qml", label: VLCIcons.previous, text: I18n.qtr("Previous") },
        { id: ControlListModel.NEXT_BUTTON, file: "NextButton.qml", label: VLCIcons.next, text: I18n.qtr("Next") },
        { id: ControlListModel.SLOWER_BUTTON, file: "SlowerButton.qml", label: VLCIcons.slower, text: I18n.qtr("Slower") },
        { id: ControlListModel.FASTER_BUTTON, file: "FasterButton.qml", label: VLCIcons.faster, text: I18n.qtr("Faster") },
        { id: ControlListModel.FULLSCREEN_BUTTON, file: "FullscreenButton.qml", label: VLCIcons.fullscreen, text: I18n.qtr("Fullscreen") },
        { id: ControlListModel.EXTENDED_BUTTON, file: "ExtendedSettingsButton.qml", label: VLCIcons.extended, text: I18n.qtr("Extended panel") },
        { id: ControlListModel.PLAYLIST_BUTTON, file: "PlaylistButton.qml", label: VLCIcons.playlist, text: I18n.qtr("Playlist") },
        { id: ControlListModel.SNAPSHOT_BUTTON, file: "SnapshotButton.qml", label: VLCIcons.snapshot, text: I18n.qtr("Snapshot") },
        { id: ControlListModel.RECORD_BUTTON, file: "RecordButton.qml", label: VLCIcons.record, text: I18n.qtr("Record") },
        { id: ControlListModel.ATOB_BUTTON, file: "AtoBButton.qml", label: VLCIcons.atob, text: I18n.qtr("A-B Loop") },
        { id: ControlListModel.FRAME_BUTTON, file: "FrameButton.qml", label: VLCIcons.frame_by_frame, text: I18n.qtr("Frame By Frame") },
        { id: ControlListModel.REVERSE_BUTTON, file: "ReverseButton.qml", label: VLCIcons.play_reverse, text: I18n.qtr("Trickplay Reverse") },
        { id: ControlListModel.SKIP_BACK_BUTTON, file: "SkipBackButton.qml", label: VLCIcons.skip_back, text: I18n.qtr("Step backward") },
        { id: ControlListModel.SKIP_FW_BUTTON, file: "SkipForwardButton.qml", label: VLCIcons.skip_for, text: I18n.qtr("Step forward") },
        { id: ControlListModel.QUIT_BUTTON, file: "QuitButton.qml", label: VLCIcons.clear, text: I18n.qtr("Quit") },
        { id: ControlListModel.RANDOM_BUTTON, file: "RandomButton.qml", label: VLCIcons.shuffle_on, text: I18n.qtr("Random") },
        { id: ControlListModel.LOOP_BUTTON, file: "LoopButton.qml", label: VLCIcons.repeat_all, text: I18n.qtr("Loop") },
        { id: ControlListModel.INFO_BUTTON, file: "InfoButton.qml", label: VLCIcons.info, text: I18n.qtr("Information") },
        { id: ControlListModel.LANG_BUTTON, file: "LangButton.qml", label: VLCIcons.audiosub, text: I18n.qtr("Open subtitles") },
        { id: ControlListModel.BOOKMARK_BUTTON, file: "BookmarkButton.qml", label: VLCIcons.bookmark, text: I18n.qtr("Bookmark Button") },
        { id: ControlListModel.CHAPTER_PREVIOUS_BUTTON, file: "ChapterPreviousButton.qml", label: VLCIcons.dvd_prev, text: I18n.qtr("Previous chapter") },
        { id: ControlListModel.CHAPTER_NEXT_BUTTON, file: "ChapterNextButton.qml", label: VLCIcons.dvd_next, text: I18n.qtr("Next chapter") },
        { id: ControlListModel.VOLUME, file: "VolumeWidget.qml", label: VLCIcons.volume_high, text: I18n.qtr("Volume Widget") },
        { id: ControlListModel.NAVIGATION_BUTTONS, file: "NavigationWidget.qml", label: VLCIcons.dvd_menu, text: I18n.qtr("Navigation") },
        { id: ControlListModel.DVD_MENUS_BUTTON, file: "DvdMenuButton.qml", label: VLCIcons.dvd_menu, text: I18n.qtr("DVD menus") },
        { id: ControlListModel.TELETEXT_BUTTONS, file: "TeletextWidget.qml", label: VLCIcons.tvtelx, text: I18n.qtr("Teletext") },
        { id: ControlListModel.PROGRAM_BUTTON, file: "ProgramButton.qml", label: VLCIcons.bookmark, text: I18n.qtr("Program Button") },
        { id: ControlListModel.RENDERER_BUTTON, file: "RendererButton.qml", label: VLCIcons.renderer, text: I18n.qtr("Renderer Button") },
        { id: ControlListModel.ASPECT_RATIO_COMBOBOX, file: "AspectRatioWidget.qml", label: VLCIcons.aspect_ratio, text: I18n.qtr("Aspect Ratio") },
        { id: ControlListModel.WIDGET_SPACER, file: "SpacerWidget.qml", label: VLCIcons.space, text: I18n.qtr("Spacer") },
        { id: ControlListModel.WIDGET_SPACER_EXTEND, file: "ExpandingSpacerWidget.qml", label: VLCIcons.space, text: I18n.qtr("Expanding Spacer") },
        { id: ControlListModel.PLAYER_SWITCH_BUTTON, file: "PlayerSwitchButton.qml", label: VLCIcons.fullscreen, text: I18n.qtr("Switch Player") },
        { id: ControlListModel.ARTWORK_INFO, file: "ArtworkInfoWidget.qml", label: VLCIcons.info, text: I18n.qtr("Artwork Info") },
        { id: ControlListModel.PLAYBACK_SPEED_BUTTON, file: "PlaybackSpeedButton.qml", label: "1x", text: I18n.qtr("Playback Speed") },
        { id: ControlListModel.HIGH_RESOLUTION_TIME_WIDGET, file: "HighResolutionTimeWidget.qml", label: VLCIcons.info, text: I18n.qtr("High Resolution Time") }
    ]

    function control(id) {
        var control = controlList.find( function(control) { return ( control.id === id ) } )

        if (control === undefined) {
            console.log("control delegate id " + id +  " doesn't exist")
            return { source: controlPath + "Fallback.qml" }
        }
        
        control.source = controlPath + control.file

        return control
    }
}
