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

import QtQml 2.11

import org.videolan.vlc 0.1

import "qrc:///player/controlbarcontrols/" as Controls
import "qrc:///widgets/" as Widgets
import "qrc:///style/"

QtObject {
    id: controlButtons

    property var  parentWindow: undefined

    signal requestLockUnlockAutoHide(bool lock, var source)

    readonly property var buttonList: [
        { id: ControlListModel.PLAY_BUTTON, component: playBtnDelegate, label: VLCIcons.play, text: i18n.qtr("Play") },
        { id: ControlListModel.STOP_BUTTON, component: stopBtnDelegate, label: VLCIcons.stop, text: i18n.qtr("Stop") },
        { id: ControlListModel.OPEN_BUTTON, component: openmediaBtnDelegate, label: VLCIcons.eject, text: i18n.qtr("Open") },
        { id: ControlListModel.PREVIOUS_BUTTON, component: prevBtnDelegate, label: VLCIcons.previous, text: i18n.qtr("Previous") },
        { id: ControlListModel.NEXT_BUTTON, component: nextBtnDelegate, label: VLCIcons.next, text: i18n.qtr("Next") },
        { id: ControlListModel.SLOWER_BUTTON, component: slowerBtnDelegate, label: VLCIcons.slower, text: i18n.qtr("Slower") },
        { id: ControlListModel.FASTER_BUTTON, component: fasterBtnDelegate, label: VLCIcons.faster, text: i18n.qtr("Faster") },
        { id: ControlListModel.FULLSCREEN_BUTTON, component: fullScreenBtnDelegate, label: VLCIcons.fullscreen, text: i18n.qtr("Fullscreen") },
        { id: ControlListModel.EXTENDED_BUTTON, component: extdSettingsBtnDelegate, label: VLCIcons.extended, text: i18n.qtr("Extended panel") },
        { id: ControlListModel.PLAYLIST_BUTTON, component: playlistBtnDelegate, label: VLCIcons.playlist, text: i18n.qtr("Playlist") },
        { id: ControlListModel.SNAPSHOT_BUTTON, component: snapshotBtnDelegate, label: VLCIcons.snapshot, text: i18n.qtr("Snapshot") },
        { id: ControlListModel.RECORD_BUTTON, component: recordBtnDelegate, label: VLCIcons.record, text: i18n.qtr("Record") },
        { id: ControlListModel.ATOB_BUTTON, component: toggleABloopstateDelegate, label: VLCIcons.atob, text: i18n.qtr("A-B Loop") },
        { id: ControlListModel.FRAME_BUTTON, component: framebyframeDelegate, label: VLCIcons.frame_by_frame, text: i18n.qtr("Frame By Frame") },
        { id: ControlListModel.SKIP_BACK_BUTTON, component: stepBackBtnDelegate, label: VLCIcons.skip_back, text: i18n.qtr("Step backward") },
        { id: ControlListModel.SKIP_FW_BUTTON, component: stepFwdBtnDelegate, label: VLCIcons.skip_for, text: i18n.qtr("Step forward") },
        { id: ControlListModel.QUIT_BUTTON, component: quitBtnDelegate, label: VLCIcons.clear, text: i18n.qtr("Quit") },
        { id: ControlListModel.RANDOM_BUTTON, component: randomBtnDelegate, label: VLCIcons.shuffle_on, text: i18n.qtr("Random") },
        { id: ControlListModel.LOOP_BUTTON, component: repeatBtnDelegate, label: VLCIcons.repeat_all, text: i18n.qtr("Loop") },
        { id: ControlListModel.INFO_BUTTON, component: mediainfoBtnDelegate, label: VLCIcons.info, text: i18n.qtr("Information") },
        { id: ControlListModel.LANG_BUTTON, component: langBtnDelegate, label: VLCIcons.audiosub, text: i18n.qtr("Open subtitles") },
        { id: ControlListModel.MENU_BUTTON, component: menuBtnDelegate, label: VLCIcons.menu, text: i18n.qtr("Menu Button") },
        { id: ControlListModel.BACK_BUTTON, component: backBtnDelegate, label: VLCIcons.exit, text: i18n.qtr("Back Button") },
        { id: ControlListModel.CHAPTER_PREVIOUS_BUTTON, component: chapterPreviousBtnDelegate, label: VLCIcons.dvd_prev, text: i18n.qtr("Previous chapter") },
        { id: ControlListModel.CHAPTER_NEXT_BUTTON, component: chapterNextBtnDelegate, label: VLCIcons.dvd_next, text: i18n.qtr("Next chapter") },
        { id: ControlListModel.VOLUME, component: volumeBtnDelegate, label: VLCIcons.volume_high, text: i18n.qtr("Volume Widget") },
        { id: ControlListModel.TELETEXT_BUTTONS, component: teletextdelegate, label: VLCIcons.tvtelx, text: i18n.qtr("Teletext") },
        { id: ControlListModel.ASPECT_RATIO_COMBOBOX, component: aspectRatioDelegate, label: VLCIcons.aspect_ratio, text: i18n.qtr("Aspect Ratio") },
        { id: ControlListModel.WIDGET_SPACER, component: spacerDelegate, label: VLCIcons.space, text: i18n.qtr("Spacer") },
        { id: ControlListModel.WIDGET_SPACER_EXTEND, component: extendiblespacerDelegate, label: VLCIcons.space, text: i18n.qtr("Expanding Spacer") },
        { id: ControlListModel.PLAYER_SWITCH_BUTTON, component: playerSwitchBtnDelegate, label: VLCIcons.fullscreen, text: i18n.qtr("Switch Player") },
        { id: ControlListModel.ARTWORK_INFO, component: artworkInfoDelegate, label: VLCIcons.info, text: i18n.qtr("Artwork Info") },
        { id: ControlListModel.PLAYBACK_SPEED_BUTTON, component: playbackSpeedButtonDelegate, label: "1x", text: i18n.qtr("Playback Speed") }
    ]

    function button(id) {
        var button = buttonList.find( function(button) { return ( button.id === id ) } )

        if (button === undefined) {
            console.log("button delegate id " + id +  " doesn't exist")
            return { component: fallbackDelegate }
        }
        
        return button
    }

    readonly property Component fallbackDelegate : Widgets.AnimatedBackground {
        implicitWidth: fbLabel.width + VLCStyle.focus_border * 2
        implicitHeight: fbLabel.height + VLCStyle.focus_border * 2

        activeBorderColor: colors.bgFocus

        property bool paintOnly: false
        property VLCColors colors: VLCStyle.colors

        Widgets.MenuLabel {
            id: fbLabel

            anchors.centerIn: parent

            text: i18n.qtr("WIDGET\nNOT\nFOUND")
            horizontalAlignment: Text.AlignHCenter

            color: colors.text
        }
    }

    readonly property Component backBtnDelegate : Controls.BackButton { }

    readonly property Component randomBtnDelegate : Controls.RandomButton { }

    readonly property Component prevBtnDelegate : Controls.PreviousButton { }

    readonly property Component playBtnDelegate : Controls.PlayButton { }

    readonly property Component nextBtnDelegate : Controls.NextButton { }

    readonly property Component chapterPreviousBtnDelegate : Controls.ChapterPreviousButton { }

    readonly property Component chapterNextBtnDelegate : Controls.ChapterNextButton { }

    readonly property Component repeatBtnDelegate : Controls.LoopButton { }

    readonly property Component langBtnDelegate : Controls.LangButton { }

    readonly property Component playlistBtnDelegate : Controls.PlaylistButton { }

    readonly property Component menuBtnDelegate : Controls.MenuButton { }

    readonly property Component spacerDelegate : Controls.SpacerWidget { }

    readonly property Component extendiblespacerDelegate : Controls.ExpandingSpacerWidget { }

    readonly property Component fullScreenBtnDelegate : Controls.FullscreenButton { }

    readonly property Component recordBtnDelegate : Controls.RecordButton { }

    readonly property Component toggleABloopstateDelegate : Controls.AtoBButton { }

    readonly property Component snapshotBtnDelegate : Controls.SnapshotButton { }

    readonly property Component stopBtnDelegate : Controls.StopButton { }

    readonly property Component mediainfoBtnDelegate : Controls.InfoButton { }

    readonly property Component framebyframeDelegate : Controls.FrameButton { }

    readonly property Component fasterBtnDelegate : Controls.FasterButton { }

    readonly property Component slowerBtnDelegate : Controls.SlowerButton { }

    readonly property Component openmediaBtnDelegate : Controls.OpenButton { }

    readonly property Component extdSettingsBtnDelegate : Controls.ExtendedSettingsButton { }

    readonly property Component stepFwdBtnDelegate : Controls.SkipForwardButton { }

    readonly property Component stepBackBtnDelegate : Controls.SkipBackButton { }

    readonly property Component quitBtnDelegate : Controls.QuitButton { }

    readonly property Component aspectRatioDelegate : Controls.AspectRatioWidget { }

    readonly property Component teletextdelegate : Controls.TeletextWidget { }

    readonly property Component volumeBtnDelegate : Controls.VolumeWidget { parentWindow: controlButtons.parentWindow }

    readonly property Component playerSwitchBtnDelegate : Controls.PlayerSwitchButton { }

    readonly property Component artworkInfoDelegate : Controls.ArtworkInfoWidget { }

    readonly property Component playbackSpeedButtonDelegate : Controls.PlaybackSpeedButton { }
}
