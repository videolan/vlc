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

import "qrc:///utils/" as Utils
import "qrc:///menus/" as Menus
import "qrc:///style/"

Item{

    function returnbuttondelegate(inpID){
        switch (inpID){
        case PlayerControlBarModel.RANDOM_BUTTON: return randomBtnDelegate
        case PlayerControlBarModel.PREVIOUS_BUTTON: return prevBtnDelegate
        case PlayerControlBarModel.PLAY_BUTTON: return playBtnDelegate
        case PlayerControlBarModel.NEXT_BUTTON: return nextBtnDelegate
        case PlayerControlBarModel.LOOP_BUTTON: return repeatBtnDelegate
        case PlayerControlBarModel.LANG_BUTTON: return langBtnDelegate
        case PlayerControlBarModel.PLAYLIST_BUTTON:return playlistBtnDelegate
        case PlayerControlBarModel.MENU_BUTTONS:return  menuBtnDelegate
        case PlayerControlBarModel.GOBACK_BUTTON:return  backBtnDelegate
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
        Utils.IconToolButton {
            id: backBtn
            objectName: "IconToolButton"
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            size: VLCStyle.icon_medium
            text: VLCIcons.exit
            onClicked: history.previous(History.Go)
            property bool acceptFocus: true
        }
    }

    Component{
        id: randomBtnDelegate
        Utils.IconToolButton {
            id: randomBtn
            objectName: "IconToolButton"
            size: VLCStyle.icon_medium
            checked: mainPlaylistController.random
            text: VLCIcons.shuffle_on
            onClicked: mainPlaylistController.toggleRandom()
            property bool acceptFocus: true
        }
    }

    Component{
        id: prevBtnDelegate
        Utils.IconToolButton {
            id: prevBtn
            objectName: "IconToolButton"
            size: VLCStyle.icon_medium
            text: VLCIcons.previous
            onClicked: mainPlaylistController.prev()
            property bool acceptFocus: true
        }
    }

    Component{
        id:playBtnDelegate
        Utils.IconToolButton {
            id: playBtn
            objectName: "IconToolButton"
            size: VLCStyle.icon_medium
            text: (player.playingState !== PlayerController.PLAYING_STATE_PAUSED
                   && player.playingState !== PlayerController.PLAYING_STATE_STOPPED)
                  ? VLCIcons.pause
                  : VLCIcons.play
            onClicked: mainPlaylistController.togglePlayPause()
            property bool acceptFocus: true
        }
    }

    Component{
        id: nextBtnDelegate
        Utils.IconToolButton {
            id: nextBtn
            objectName: "IconToolButton"
            size: VLCStyle.icon_medium
            text: VLCIcons.next
            onClicked: mainPlaylistController.next()
            property bool acceptFocus: true
        }
    }

    Component{
        id: repeatBtnDelegate
        Utils.IconToolButton {
            id: repeatBtn
            objectName: "IconToolButton"
            size: VLCStyle.icon_medium
            checked: mainPlaylistController.repeatMode !== PlaylistControllerModel.PLAYBACK_REPEAT_NONE
            text: (mainPlaylistController.repeatMode === PlaylistControllerModel.PLAYBACK_REPEAT_CURRENT)
                  ? VLCIcons.repeat_one
                  : VLCIcons.repeat_all
            onClicked: mainPlaylistController.toggleRepeatMode()
            property bool acceptFocus: true
        }
    }

    Component{
        id: langBtnDelegate
        Utils.IconToolButton {
            id: langBtn
            objectName: "IconToolButton"
            size: VLCStyle.icon_medium
            text: VLCIcons.audiosub
            onClicked: root.showTrackBar()
            property bool acceptFocus: true
        }
    }

    Component{
        id:playlistBtnDelegate
        Utils.IconToolButton {
            id: playlistBtn
            objectName: "IconToolButton"
            size: VLCStyle.icon_medium
            text: VLCIcons.playlist
            onClicked: rootWindow.playlistVisible = !rootWindow.playlistVisible
            property bool acceptFocus: true
        }

    }

    Component{
        id: menuBtnDelegate
        Utils.IconToolButton {
            id: menuBtn
            objectName: "IconToolButton"
            size: VLCStyle.icon_medium
            text: VLCIcons.menu
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
        }
    }

    Component{
        id:spacerDelegate
        Item {
            objectName: "space"
            id: spacer
            implicitWidth: 32 * VLCStyle.scale
            property bool acceptFocus: false
        }
    }

    Component{
        id: extendiblespacerDelegate
        Item{
            objectName: "extendedspace"
            id: extendedspacer
            implicitWidth: 128 * VLCStyle.scale
            property bool acceptFocus: false
            Component.onCompleted: {
                parent.Layout.fillWidth=true
            }
        }
    }

    Component{
        id: fullScreenBtnDelegate
        Utils.IconToolButton{
            id: fullScreenBtn
            size: VLCStyle.icon_medium
            text: rootWindow.interfaceFullScreen ?VLCIcons.defullscreen :VLCIcons.fullscreen
            onClicked: rootWindow.interfaceFullScreen = !rootWindow.interfaceFullScreen
            property bool acceptFocus: true
        }
    }

    Component{
        id: recordBtnDelegate
        Utils.IconToolButton{
            id: recordBtn
            size: VLCStyle.icon_medium
            text: VLCIcons.record
            enabled: player.isPlaying
            checked: player.isRecording
            onClicked: player.toggleRecord()
            property bool acceptFocus: true
        }
    }

    Component{
        id: toggleABloopstateDelegate
        Utils.IconToolButton {
            id: abBtn
            size: VLCStyle.icon_medium
            checked: player.ABloopState !== PlayerController.ABLOOP_STATE_NONE
            text: switch(player.ABloopState) {
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
        }
    }

    Component{
        id: snapshotBtnDelegate
        Utils.IconToolButton{
            id: snapshotBtn
            size: VLCStyle.icon_medium
            enabled: player.isPlaying
            text: VLCIcons.snapshot
            onClicked: player.snapshot()
            property bool acceptFocus: true
        }
    }


    Component{
        id: stopBtndelgate
        Utils.IconToolButton{
            id: stopBtn
            size: VLCStyle.icon_medium
            enabled: player.isPlaying
            text: VLCIcons.stop
            onClicked: mainPlaylistController.stop()
            property bool acceptFocus: true
        }
    }

    Component{
        id: mediainfoBtnDelegate
        Utils.IconToolButton{
            id: infoBtn
            size: VLCStyle.icon_medium
            enabled: player.isPlaying
            text: VLCIcons.info
            onClicked: dialogProvider.mediaInfoDialog()
            property bool acceptFocus: true
        }
    }

    Component{
        id: framebyframeDelegate

        Utils.IconToolButton{
            id: frameBtn
            size: VLCStyle.icon_medium
            enabled: player.isPlaying
            text: VLCIcons.frame_by_frame
            onClicked: player.frameNext()
            property bool acceptFocus: true
        }
    }

    Component{
        id: fasterBtnDelegate

        Utils.IconToolButton{
            id: fasterBtn
            size: VLCStyle.icon_medium
            text: VLCIcons.faster
            onClicked: player.faster()
            property bool acceptFocus: true
        }
    }

    Component{
        id: slowerBtnDelegate

        Utils.IconToolButton{
            id: slowerBtn
            size: VLCStyle.icon_medium
            text: VLCIcons.slower
            onClicked: player.slower()
            property bool acceptFocus: true
        }
    }

    Component{
        id: openmediaBtnDelegate
        Utils.IconToolButton{
            id: openMediaBtn
            size: VLCStyle.icon_medium
            text: VLCIcons.eject
            onClicked: dialogProvider.openDialog()
            property bool acceptFocus: true
        }
    }

    Component{
        id: extdSettingsBtnDelegate
        Utils.IconToolButton{
            id: extdSettingsBtn
            size: VLCStyle.icon_medium
            text: VLCIcons.extended
            onClicked: dialogProvider.extendedDialog()
            property bool acceptFocus: true
        }
    }

    Component{
        id: stepFwdBtnDelegate
        Utils.IconToolButton{
            id: stepfwdBtn
            size: VLCStyle.icon_medium
            text: VLCIcons.skip_for
            onClicked: player.jumpFwd()
            property bool acceptFocus: true
        }
    }

    Component{
        id: stepBackBtnDelegate
        Utils.IconToolButton{
            id: stepBackBtn
            size: VLCStyle.icon_medium
            text: VLCIcons.skip_back
            onClicked: player.jumpBwd()
            property bool acceptFocus: true
        }
    }

    Component{
        id: quitBtnDelegate
        Utils.IconToolButton{
            id: quitBtn
            size: VLCStyle.icon_medium
            text: VLCIcons.clear
            onClicked: rootWindow.close()
            property bool acceptFocus: true
        }
    }

    Component{
        id: aspectRatioDelegate
        Utils.ComboBoxExt {
            id: combo
            Layout.alignment: Qt.AlignVCenter
            height: 28 * scale
            width: 100 * scale
            textRole: "display"
            model: player.aspectRatio
            onCurrentIndexChanged: model.toggleIndex(currentIndex)
            property bool acceptFocus: true
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
