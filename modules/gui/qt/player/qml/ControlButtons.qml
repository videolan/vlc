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
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.4

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///menus/" as Menus
import "qrc:///style/"

Item{
    property var buttonL: [
        { id:  PlayerControlBarModel.PLAY_BUTTON, label: VLCIcons.play, text: i18n.qtr("Play")},
        { id:  PlayerControlBarModel.STOP_BUTTON, label: VLCIcons.stop, text: i18n.qtr("Stop")},
        { id:  PlayerControlBarModel.OPEN_BUTTON, label: VLCIcons.eject, text: i18n.qtr("Open")},
        { id:  PlayerControlBarModel.PREVIOUS_BUTTON, label: VLCIcons.previous, text: i18n.qtr("Previous")},
        { id:  PlayerControlBarModel.NEXT_BUTTON, label: VLCIcons.next, text: i18n.qtr("Next")},
        { id:  PlayerControlBarModel.SLOWER_BUTTON, label: VLCIcons.slower, text: i18n.qtr("Slower")},
        { id:  PlayerControlBarModel.FASTER_BUTTON, label: VLCIcons.faster, text: i18n.qtr("Faster")},
        { id:  PlayerControlBarModel.FULLSCREEN_BUTTON, label: VLCIcons.fullscreen, text: i18n.qtr("Fullscreen")},
        { id:  PlayerControlBarModel.EXTENDED_BUTTON, label: VLCIcons.extended, text: i18n.qtr("Extended panel")},
        { id:  PlayerControlBarModel.PLAYLIST_BUTTON, label: VLCIcons.playlist, text: i18n.qtr("Playlist")},
        { id:  PlayerControlBarModel.SNAPSHOT_BUTTON, label: VLCIcons.snapshot, text: i18n.qtr("Snapshot")},
        { id:  PlayerControlBarModel.RECORD_BUTTON, label: VLCIcons.record, text: i18n.qtr("Record")},
        { id:  PlayerControlBarModel.ATOB_BUTTON, label: VLCIcons.atob, text: i18n.qtr("A-B Loop")},
        { id:  PlayerControlBarModel.FRAME_BUTTON, label: VLCIcons.frame_by_frame, text: i18n.qtr("Frame By Frame")},
        { id:  PlayerControlBarModel.SKIP_BACK_BUTTON, label: VLCIcons.skip_back, text: i18n.qtr("Step backward")},
        { id:  PlayerControlBarModel.SKIP_FW_BUTTON, label: VLCIcons.skip_for, text: i18n.qtr("Step forward")},
        { id:  PlayerControlBarModel.QUIT_BUTTON, label: VLCIcons.clear, text: i18n.qtr("Quit")},
        { id:  PlayerControlBarModel.RANDOM_BUTTON, label: VLCIcons.shuffle_on, text: i18n.qtr("Random")},
        { id:  PlayerControlBarModel.LOOP_BUTTON, label: VLCIcons.repeat_all, text: i18n.qtr("Loop")},
        { id:  PlayerControlBarModel.INFO_BUTTON, label: VLCIcons.info, text: i18n.qtr("Information")},
        { id:  PlayerControlBarModel.LANG_BUTTON, label: VLCIcons.audiosub, text: i18n.qtr("Open subtitles")},
        { id:  PlayerControlBarModel.MENU_BUTTON, label: VLCIcons.menu, text: i18n.qtr("Menu Button")},
        { id:  PlayerControlBarModel.BACK_BUTTON, label: VLCIcons.exit, text: i18n.qtr("Back Button")},
        { id:  PlayerControlBarModel.CHAPTER_PREVIOUS_BUTTON, label: VLCIcons.dvd_prev, text: i18n.qtr("Previous chapter")},
        { id:  PlayerControlBarModel.CHAPTER_NEXT_BUTTON, label: VLCIcons.dvd_next, text: i18n.qtr("Next chapter")},
        { id:  PlayerControlBarModel.VOLUME, label: VLCIcons.volume_high, text: i18n.qtr("Volume Widget")},
        { id:  PlayerControlBarModel.TELETEXT_BUTTONS, label: VLCIcons.tvtelx, text: i18n.qtr("Teletext")},
        { id:  PlayerControlBarModel.ASPECT_RATIO_COMBOBOX, label: VLCIcons.aspect_ratio, text: i18n.qtr("Aspect Ratio")},
        { id:  PlayerControlBarModel.WIDGET_SPACER, label: VLCIcons.space, text: i18n.qtr("Spacer")},
        { id:  PlayerControlBarModel.WIDGET_SPACER_EXTEND, label: VLCIcons.space, text: i18n.qtr("Expanding Spacer")}
    ]

    function returnbuttondelegate(inpID){
        switch (inpID){
        case PlayerControlBarModel.RANDOM_BUTTON: return randomBtnDelegate
        case PlayerControlBarModel.PREVIOUS_BUTTON: return prevBtnDelegate
        case PlayerControlBarModel.PLAY_BUTTON: return playBtnDelegate
        case PlayerControlBarModel.NEXT_BUTTON: return nextBtnDelegate
        case PlayerControlBarModel.LOOP_BUTTON: return repeatBtnDelegate
        case PlayerControlBarModel.LANG_BUTTON: return langBtnDelegate
        case PlayerControlBarModel.PLAYLIST_BUTTON:return playlistBtnDelegate
        case PlayerControlBarModel.MENU_BUTTON:return  menuBtnDelegate
        case PlayerControlBarModel.CHAPTER_PREVIOUS_BUTTON:return  chapterPreviousBtnDelegate
        case PlayerControlBarModel.CHAPTER_NEXT_BUTTON:return  chapterNextBtnDelegate
        case PlayerControlBarModel.BACK_BUTTON:return  backBtnDelegate
        case PlayerControlBarModel.WIDGET_SPACER:return  spacerDelegate
        case PlayerControlBarModel.WIDGET_SPACER_EXTEND:return  extendiblespacerDelegate
        case PlayerControlBarModel.RECORD_BUTTON: return recordBtnDelegate
        case PlayerControlBarModel.FULLSCREEN_BUTTON: return fullScreenBtnDelegate
        case PlayerControlBarModel.ATOB_BUTTON: return toggleABloopstateDelegate
        case PlayerControlBarModel.SNAPSHOT_BUTTON: return snapshotBtnDelegate
        case PlayerControlBarModel.STOP_BUTTON: return stopBtndelgate
        case PlayerControlBarModel.INFO_BUTTON: return mediainfoBtnDelegate
        case PlayerControlBarModel.FRAME_BUTTON: return framebyframeDelegate
        case PlayerControlBarModel.FASTER_BUTTON: return fasterBtnDelegate
        case PlayerControlBarModel.SLOWER_BUTTON: return slowerBtnDelegate
        case PlayerControlBarModel.OPEN_BUTTON: return openmediaBtnDelegate
        case PlayerControlBarModel.EXTENDED_BUTTON: return extdSettingsBtnDelegate
        case PlayerControlBarModel.SKIP_FW_BUTTON: return stepFwdBtnDelegate
        case PlayerControlBarModel.SKIP_BACK_BUTTON: return stepBackBtnDelegate
        case PlayerControlBarModel.QUIT_BUTTON: return quitBtnDelegate
        case PlayerControlBarModel.VOLUME: return volumeBtnDelegate
        case PlayerControlBarModel.ASPECT_RATIO_COMBOBOX: return aspectRatioDelegate
        case PlayerControlBarModel.TELETEXT_BUTTONS: return teletextdelegate
        }
        console.log("button delegate id " + inpID +  " doesn't exists")
        return spacerDelegate
    }

    Component{
        id: backBtnDelegate
        Widgets.IconToolButton {
            id: backBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.exit
            text: i18n.qtr("Back")
            onClicked: history.previous()
            property bool acceptFocus: true
        }
    }

    Component{
        id: randomBtnDelegate
        Widgets.IconToolButton {
            id: randomBtn
            size: VLCStyle.icon_medium
            checked: mainPlaylistController.random
            iconText: VLCIcons.shuffle_on
            onClicked: mainPlaylistController.toggleRandom()
            property bool acceptFocus: true
            text: i18n.qtr("Random")
        }
    }

    Component{
        id: prevBtnDelegate
        Widgets.IconToolButton {
            id: prevBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.previous
            onClicked: mainPlaylistController.prev()
            property bool acceptFocus: true
            text: i18n.qtr("Previous")
        }
    }

    Component{
        id:playBtnDelegate
        Widgets.IconToolButton {
            id: playBtn
            size: VLCStyle.icon_medium
            iconText: (player.playingState !== PlayerController.PLAYING_STATE_PAUSED
                   && player.playingState !== PlayerController.PLAYING_STATE_STOPPED)
                  ? VLCIcons.pause
                  : VLCIcons.play
            onClicked: mainPlaylistController.togglePlayPause()
            property bool acceptFocus: true
            text: (player.playingState !== PlayerController.PLAYING_STATE_PAUSED
                   && player.playingState !== PlayerController.PLAYING_STATE_STOPPED)
                  ? i18n.qtr("Pause")
                  : i18n.qtr("Play")
        }
    }

    Component{
        id: nextBtnDelegate
        Widgets.IconToolButton {
            id: nextBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.next
            onClicked: mainPlaylistController.next()
            property bool acceptFocus: true
            text: i18n.qtr("Next")
        }
    }

    Component{
        id: chapterPreviousBtnDelegate
        Widgets.IconToolButton {
            id: chapterPreviousBtnDelegate
            size: VLCStyle.icon_medium
            width: visible? VLCStyle.icon_medium : 0
            iconText: VLCIcons.dvd_prev
            onClicked: player.chapterPrev()
            visible: player.hasChapters
            enabled: visible
            property bool acceptFocus: visible
            text: i18n.qtr("Previous chapter")
        }
    }


    Component{
        id: chapterNextBtnDelegate
        Widgets.IconToolButton {
            id: chapterPreviousBtnDelegate
            size: VLCStyle.icon_medium
            width: visible? VLCStyle.icon_medium : 0
            iconText: VLCIcons.dvd_next
            onClicked: player.chapterNext()
            visible: player.hasChapters
            enabled: visible
            property bool acceptFocus: visible
            text: i18n.qtr("Next chapter")
        }
    }


    Component{
        id: repeatBtnDelegate
        Widgets.IconToolButton {
            id: repeatBtn
            size: VLCStyle.icon_medium
            checked: mainPlaylistController.repeatMode !== PlaylistControllerModel.PLAYBACK_REPEAT_NONE
            iconText: (mainPlaylistController.repeatMode === PlaylistControllerModel.PLAYBACK_REPEAT_CURRENT)
                  ? VLCIcons.repeat_one
                  : VLCIcons.repeat_all
            onClicked: mainPlaylistController.toggleRepeatMode()
            property bool acceptFocus: true
            text: i18n.qtr("Repeat")
        }
    }

    Component{
        id: langBtnDelegate
        Widgets.IconToolButton {
            id: langBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.audiosub

            onClicked: {
                root._lockAutoHide += 1
                langMenu.open()
            }

            text: i18n.qtr("Languages and tracks")

            PlayerMenu {
                id: langMenu
                parent: rootPlayer
                onOpened: rootPlayer._menu = langMenu
                onMenuClosed: {
                    root._lockAutoHide -= 1
                    langBtn.forceActiveFocus()
                    rootPlayer._menu = undefined
                }
                focus: true

                title: i18n.qtr("Languages and Tracks")


                Connections {
                    target: player
                    onInputChanged: {
                        subtrackMenu.dismiss()
                        audiotrackMenu.dismiss()
                        videotrackMenu.dismiss()
                        langMenu.dismiss()
                    }
                }

                PlayerMenu {
                    id: subtrackMenu
                    onOpened: rootPlayer._menu = subtrackMenu
                    parentMenu: langMenu
                    title: i18n.qtr("Subtitle Track")
                    enabled: player.isPlaying && player.subtitleTracks.count > 0
                    Repeater {
                        model: player.subtitleTracks
                        PlayerMenuItem {
                            parentMenu:  subtrackMenu
                            text: model.display
                            checkable: true
                            checked: model.checked
                            onTriggered: model.checked = !model.checked
                        }
                    }
                    onMenuClosed: langMenu.menuClosed()
                }

                PlayerMenu {
                    id: audiotrackMenu
                    title: i18n.qtr("Audio Track")

                    parentMenu: langMenu
                    onOpened: rootPlayer._menu = audiotrackMenu

                    enabled: player.isPlaying && player.audioTracks.count > 0
                    Repeater {
                        model: player.audioTracks
                        PlayerMenuItem {
                            parentMenu: audiotrackMenu

                            text: model.display
                            checkable: true
                            checked: model.checked
                            onTriggered: model.checked = !model.checked
                        }
                    }
                    onMenuClosed: langMenu.menuClosed()
                }

                PlayerMenu {
                    id: videotrackMenu
                    title: i18n.qtr("Video Track")
                    parentMenu: langMenu
                    onOpened: rootPlayer._menu = videotrackMenu
                    enabled: player.isPlaying && player.videoTracks.count > 0
                    Repeater {
                        model: player.videoTracks
                        PlayerMenuItem {
                            parentMenu: videotrackMenu
                            text: model.display
                            checkable: true
                            checked: model.checked
                            onTriggered: model.checked = !model.checked
                        }
                    }
                    onMenuClosed: langMenu.menuClosed()
                }
            }

            property bool acceptFocus: true
        }
    }

    Component{
        id:playlistBtnDelegate
        Widgets.IconToolButton {
            id: playlistBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.playlist
            onClicked: {
                mainInterface.playlistVisible = !mainInterface.playlistVisible
                if (mainInterface.playlistVisible && mainInterface.playlistDocked) {
                    playlistWidget.gainFocus(playlistBtn)
                }
            }
            property bool acceptFocus: true
            text: i18n.qtr("Playlist")
        }

    }

    Component{
        id: menuBtnDelegate
        Widgets.IconToolButton {
            id: menuBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.menu
            onClicked: {
                root._lockAutoHide += 1
                mainMenu.openAbove(this)
            }
            property alias mainMenuExt: mainMenu
            Menus.MainDropdownMenu {
                id: mainMenu
                onClosed: {
                    root._lockAutoHide -= 1
                    menuBtn.forceActiveFocus()
                }
            }
            property bool acceptFocus: true
            text: i18n.qtr("Menu")
        }
    }

    Component{
        id:spacerDelegate
        Item {
            id: spacer
            enabled: false
            implicitWidth: VLCStyle.icon_normal
            implicitHeight: VLCStyle.icon_normal
            property alias spacetextExt: spacetext
            property bool paintOnly: false
            Label {
                id: spacetext
                text: VLCIcons.space
                color: VLCStyle.colors.buttonText
                visible: parent.paintOnly

                anchors.centerIn: parent

                font.pixelSize: VLCIcons.pixelSize(VLCStyle.icon_medium)
                font.family: VLCIcons.fontFamily

                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
            }
            property bool acceptFocus: false
        }
    }

    Component{
        id: extendiblespacerDelegate
        Item{
            id: extendedspacer
            enabled: false
            implicitWidth: VLCStyle.widthExtendedSpacer
            implicitHeight: VLCStyle.icon_normal
            property bool paintOnly: false
            property alias spacetextExt: spacetext
            Label {
                id: spacetext
                text: VLCIcons.space
                color: VLCStyle.colors.buttonText
                visible: paintOnly

                anchors.centerIn: parent

                font.pixelSize: VLCIcons.pixelSize(VLCStyle.icon_medium)
                font.family: VLCIcons.fontFamily

                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
            }
            property bool acceptFocus: false
            Component.onCompleted: {
                parent.Layout.fillWidth=true
            }
        }
    }

    Component{
        id: fullScreenBtnDelegate
        Widgets.IconToolButton{
            id: fullScreenBtn
            size: VLCStyle.icon_medium
            enabled: !paintOnly && player.hasVideoOutput
            iconText: player.fullscreen ? VLCIcons.defullscreen :VLCIcons.fullscreen
            onClicked: player.fullscreen = !player.fullscreen
            property bool acceptFocus: true
            text: i18n.qtr("fullscreen")
        }
    }

    Component{
        id: recordBtnDelegate
        Widgets.IconToolButton{
            id: recordBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.record
            enabled: !paintOnly && player.isPlaying
            checked: player.isRecording
            onClicked: player.toggleRecord()
            property bool acceptFocus: true
            text: i18n.qtr("record")
        }
    }

    Component{
        id: toggleABloopstateDelegate
        Widgets.IconToolButton {
            id: abBtn
            size: VLCStyle.icon_medium
            checked: player.ABloopState !== PlayerController.ABLOOP_STATE_NONE
            iconText: switch(player.ABloopState) {
                  case PlayerController.ABLOOP_STATE_A: return VLCIcons.atob_bg_b
                  case PlayerController.ABLOOP_STATE_B: return VLCIcons.atob_bg_none
                  case PlayerController.ABLOOP_STATE_NONE: return VLCIcons.atob_bg_ab
                  }
            textOverlay: switch(player.ABloopState) {
                         case PlayerController.ABLOOP_STATE_A: return VLCIcons.atob_fg_a
                         case PlayerController.ABLOOP_STATE_B: return VLCIcons.atob_fg_ab
                         case PlayerController.ABLOOP_STATE_NONE: return ""
                         }
            onClicked: player.toggleABloopState()
            color: VLCStyle.colors.buttonText
            colorOverlay: VLCStyle.colors.banner
            property bool acceptFocus: true
            text: i18n.qtr("A to B")
        }
    }

    Component{
        id: snapshotBtnDelegate
        Widgets.IconToolButton{
            id: snapshotBtn
            size: VLCStyle.icon_medium
            enabled: !paintOnly && player.isPlaying
            iconText: VLCIcons.snapshot
            onClicked: player.snapshot()
            property bool acceptFocus: true
            text: i18n.qtr("Snapshot")
        }
    }


    Component{
        id: stopBtndelgate
        Widgets.IconToolButton{
            id: stopBtn
            size: VLCStyle.icon_medium
            enabled: !paintOnly && player.isPlaying
            iconText: VLCIcons.stop
            onClicked: mainPlaylistController.stop()
            property bool acceptFocus: true
            text: i18n.qtr("Stop")
        }
    }

    Component{
        id: mediainfoBtnDelegate
        Widgets.IconToolButton{
            id: infoBtn
            size: VLCStyle.icon_medium
            enabled: !paintOnly && player.isPlaying
            iconText: VLCIcons.info
            onClicked: dialogProvider.mediaInfoDialog()
            property bool acceptFocus: true
            text: i18n.qtr("Informations")
        }
    }

    Component{
        id: framebyframeDelegate

        Widgets.IconToolButton{
            id: frameBtn
            size: VLCStyle.icon_medium
            enabled: !paintOnly && player.isPlaying
            iconText: VLCIcons.frame_by_frame
            onClicked: player.frameNext()
            property bool acceptFocus: true
            text: i18n.qtr("Next frame")
        }
    }

    Component{
        id: fasterBtnDelegate

        Widgets.IconToolButton{
            id: fasterBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.faster
            onClicked: player.faster()
            property bool acceptFocus: true
            text: i18n.qtr("Faster")
        }
    }

    Component{
        id: slowerBtnDelegate

        Widgets.IconToolButton{
            id: slowerBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.slower
            onClicked: player.slower()
            property bool acceptFocus: true
            text: i18n.qtr("Slower")
        }
    }

    Component{
        id: openmediaBtnDelegate
        Widgets.IconToolButton{
            id: openMediaBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.eject
            onClicked: dialogProvider.openDialog()
            property bool acceptFocus: true
            text: i18n.qtr("Open media")
        }
    }

    Component{
        id: extdSettingsBtnDelegate
        Widgets.IconToolButton{
            id: extdSettingsBtn
            size: VLCStyle.icon_medium
            text: VLCIcons.extended
            onClicked: dialogProvider.extendedDialog()
            property bool acceptFocus: true
            Accessible.name: i18n.qtr("Extended settings")
        }
    }

    Component{
        id: stepFwdBtnDelegate
        Widgets.IconToolButton{
            id: stepfwdBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.skip_for
            onClicked: player.jumpFwd()
            property bool acceptFocus: true
            text: i18n.qtr("Step forward")
        }
    }

    Component{
        id: stepBackBtnDelegate
        Widgets.IconToolButton{
            id: stepBackBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.skip_back
            onClicked: player.jumpBwd()
            property bool acceptFocus: true
            text: i18n.qtr("Step back")
        }
    }

    Component{
        id: quitBtnDelegate
        Widgets.IconToolButton{
            id: quitBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.clear
            onClicked: mainInterface.close()
            property bool acceptFocus: true
            text: i18n.qtr("Quit")
        }
    }

    Component{
        id: aspectRatioDelegate
        Widgets.ComboBoxExt {
            property bool paintOnly: false
            enabled: !paintOnly
            Layout.alignment: Qt.AlignVCenter
            width: VLCStyle.combobox_width_normal
            height: VLCStyle.combobox_height_normal
            textRole: "display"
            model: player.aspectRatio
            currentIndex: -1
            onCurrentIndexChanged: model.toggleIndex(currentIndex)
            property bool acceptFocus: true
            Accessible.name: i18n.qtr("Aspect ratio")
        }
    }

    Component{
        id: teletextdelegate
        TeletextWidget{}
    }

    Component{
        id: volumeBtnDelegate
        VolumeWidget{}
    }
}
