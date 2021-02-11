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
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"
import "qrc:///util/KeyHelper.js" as KeyHelper

Item{
    id: controlButtons

    property bool isMiniplayer: false
    property var  parentWindow: undefined

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
        { id:  PlayerControlBarModel.WIDGET_SPACER_EXTEND, label: VLCIcons.space, text: i18n.qtr("Expanding Spacer")},
        { id:  PlayerControlBarModel.PLAYER_SWITCH_BUTTON, label: VLCIcons.fullscreen, text: i18n.qtr("Switch Player")},
        { id:  PlayerControlBarModel.ARTWORK_INFO, label: VLCIcons.info, text: i18n.qtr("Artwork Info")}
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
        case PlayerControlBarModel.PLAYER_SWITCH_BUTTON: return playerSwitchBtnDelegate
        case PlayerControlBarModel.ARTWORK_INFO: return artworkInfoDelegate
        }
        console.log("button delegate id " + inpID +  " doesn't exists")
        return spacerDelegate
    }

    Component{
        id: backBtnDelegate
        Widgets.IconControlButton {
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
        Widgets.IconControlButton {
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
        Widgets.IconControlButton {
            id: prevBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.previous
            enabled: mainPlaylistController.hasPrev
            onClicked: mainPlaylistController.prev()
            property bool acceptFocus: true
            text: i18n.qtr("Previous")
        }
    }

    Component{
        id:playBtnDelegate

        ToolButton {
            id: playBtn

            width: VLCStyle.icon_medium
            height: width

            property bool isOpaque: !isMiniplayer

            property VLCColors colors: VLCStyle.colors
            property color color: isOpaque ? colors.buttonText : "#303030"
            property color colorDisabled: isOpaque ? colors.textInactive : "#7f8c8d"

            property bool acceptFocus: true

            property bool paintOnly: false

            property bool isCursorInside: false

            Keys.onPressed: {
                if (KeyHelper.matchOk(event) ) {
                    event.accepted = true
                }
            }
            Keys.onReleased: {
                if (!event.accepted && KeyHelper.matchOk(event))
                    mainPlaylistController.togglePlayPause()
            }

            states: [
                State {
                    name: "hovered"
                    when: interactionIndicator

                    PropertyChanges {
                        target: contentLabel
                        color: "#FF610A"
                    }

                    PropertyChanges {
                        target: hoverShadow
                        radius: VLCStyle.dp(24, VLCStyle.scale)
                    }
                },
                State {
                    name: "default"
                    when: !interactionIndicator

                    PropertyChanges {
                        target: contentLabel
                        color: enabled ? playBtn.color : playBtn.colorDisabled
                    }

                    PropertyChanges {
                        target: hoverShadow
                        radius: 0
                    }
                }
            ]
            readonly property bool interactionIndicator: (playBtn.activeFocus || playBtn.isCursorInside || playBtn.highlighted)

            contentItem: Label {
                id: contentLabel

                text: (player.playingState !== PlayerController.PLAYING_STATE_PAUSED
                       && player.playingState !== PlayerController.PLAYING_STATE_STOPPED)
                      ? VLCIcons.pause
                      : VLCIcons.play

                Behavior on color {
                    ColorAnimation {
                        duration: 75
                        easing.type: Easing.InOutSine
                    }
                }

                Behavior on text {
                    SequentialAnimation {
                        NumberAnimation {
                            target: contentLabel
                            property: "font.pixelSize"
                            to: 0
                            easing.type: Easing.OutSine
                            duration: 75
                        }

                        // this blank PropertyAction triggers the
                        // text (icon) change amidst the size animation
                        PropertyAction { }

                        NumberAnimation {
                            target: contentLabel
                            property: "font.pixelSize"
                            to: VLCIcons.pixelSize(VLCStyle.icon_normal)
                            easing.type: Easing.InSine
                            duration: 75
                        }
                    }
                }

                font.pixelSize: VLCIcons.pixelSize(VLCStyle.icon_normal)
                font.family: VLCIcons.fontFamily

                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
            }

            background: Item {
                Gradient {
                    id: playBtnGradient
                    GradientStop { position: 0.0; color: "#f89a06" }
                    GradientStop { position: 1.0; color: "#e25b01" }
                }

                MouseArea {
                    id: playBtnMouseArea

                    anchors.fill: parent
                    anchors.margins: VLCStyle.dp(1, VLCStyle.scale)

                    hoverEnabled: true

                    readonly property int radius: playBtnMouseArea.width / 2

                    function distance2D(x0, y0, x1, y1) {
                        return Math.sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0))
                    }

                    onPositionChanged: {
                        if (distance2D(playBtnMouseArea.mouseX, playBtnMouseArea.mouseY, playBtnMouseArea.width / 2, playBtnMouseArea.height / 2) < radius) {
                            // mouse is inside of the round button
                            playBtn.isCursorInside = true
                        }
                        else {
                            // mouse is outside
                            playBtn.isCursorInside = false
                        }
                    }

                    onHoveredChanged: {
                        if (!playBtnMouseArea.containsMouse)
                            playBtn.isCursorInside = false
                    }

                    onClicked: {
                        if (!playBtn.isCursorInside)
                            return

                        mainPlaylistController.togglePlayPause()
                    }

                    onPressAndHold: {
                        if (!playBtn.isCursorInside)
                            return

                        mainPlaylistController.stop()
                    }
                }

                DropShadow {
                    id: hoverShadow

                    anchors.fill: parent

                    visible: radius > 0
                    samples: (radius * 2) + 1
                    // opacity: 0.29 // it looks better without this
                    color: "#FF610A"
                    source: opacityMask
                    antialiasing: true

                    Behavior on radius {
                        NumberAnimation {
                            duration: 75
                            easing.type: Easing.InOutSine
                        }
                    }
                }

                Rectangle {
                    radius: (width * 0.5)
                    anchors.fill: parent
                    anchors.margins: VLCStyle.dp(1, VLCStyle.scale)

                    color: VLCStyle.colors.white
                    opacity: playBtn.isOpaque ? 0.4 : 1.0
                }

                Rectangle {
                    id: outerRect
                    anchors.fill: parent

                    radius: (width * 0.5)
                    gradient: playBtnGradient

                    visible: false
                }

                Rectangle {
                    id: innerRect
                    anchors.fill: parent

                    radius: (width * 0.5)
                    border.width: VLCStyle.dp(2, VLCStyle.scale)

                    color: "transparent"
                    visible: false
                }

                OpacityMask {
                    id: opacityMask
                    anchors.fill: parent

                    source: outerRect
                    maskSource: innerRect

                    antialiasing: true
                }
            }
        }
    }

    Component{
        id: nextBtnDelegate
        Widgets.IconControlButton {
            id: nextBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.next
            enabled: mainPlaylistController.hasNext
            onClicked: mainPlaylistController.next()
            property bool acceptFocus: true
            text: i18n.qtr("Next")
        }
    }

    Component{
        id: chapterPreviousBtnDelegate
        Widgets.IconControlButton {
            id: chapterPreviousBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.dvd_prev
            onClicked: player.chapterPrev()
            enabled: player.hasChapters
            property bool acceptFocus: visible
            text: i18n.qtr("Previous chapter")
        }
    }


    Component{
        id: chapterNextBtnDelegate
        Widgets.IconControlButton {
            id: chapterNextBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.dvd_next
            onClicked: player.chapterNext()
            enabled: player.hasChapters
            property bool acceptFocus: visible
            text: i18n.qtr("Next chapter")
        }
    }


    Component{
        id: repeatBtnDelegate
        Widgets.IconControlButton {
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
        Widgets.IconControlButton {
            id: langBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.audiosub

            onClicked: {
                root._lockAutoHide += 1
                langMenu.open()
            }

            text: i18n.qtr("Languages and tracks")

            LanguageMenu {
                id: langMenu

                parent: rootPlayer
                focus: true
                x: 0
                y: (!!rootPlayer) ? (rootPlayer.positionSliderY - height) : 0
                z: 1

                onOpened: rootPlayer._menu = langMenu
                onMenuClosed: {
                    root._lockAutoHide -= 1
                    langBtn.forceActiveFocus()
                    rootPlayer._menu = undefined
                }
            }
        }
    }

    Component{
        id:playlistBtnDelegate
        Widgets.IconControlButton {
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
        Widgets.IconControlButton {
            id: menuBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.ellipsis
            onClicked: contextMenu.popup(this.mapToGlobal(0, 0))
            property bool acceptFocus: true
            text: i18n.qtr("Menu")

            QmlGlobalMenu {
                id: contextMenu
                ctx: mainctx
            }
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
        Widgets.IconControlButton{
            id: fullScreenBtn
            size: VLCStyle.icon_medium
            enabled: player.hasVideoOutput
            iconText: player.fullscreen ? VLCIcons.defullscreen :VLCIcons.fullscreen
            onClicked: player.fullscreen = !player.fullscreen
            property bool acceptFocus: true
            text: i18n.qtr("fullscreen")
        }
    }

    Component{
        id: recordBtnDelegate
        Widgets.IconControlButton{
            id: recordBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.record
            enabled: player.isPlaying
            checked: player.isRecording
            onClicked: player.toggleRecord()
            property bool acceptFocus: true
            text: i18n.qtr("record")
        }
    }

    Component{
        id: toggleABloopstateDelegate
        Widgets.IconControlButton {
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
        Widgets.IconControlButton{
            id: snapshotBtn
            size: VLCStyle.icon_medium
            enabled: player.isPlaying
            iconText: VLCIcons.snapshot
            onClicked: player.snapshot()
            property bool acceptFocus: true
            text: i18n.qtr("Snapshot")
        }
    }


    Component{
        id: stopBtndelgate
        Widgets.IconControlButton{
            id: stopBtn
            size: VLCStyle.icon_medium
            enabled: player.isPlaying
            iconText: VLCIcons.stop
            onClicked: mainPlaylistController.stop()
            property bool acceptFocus: true
            text: i18n.qtr("Stop")
        }
    }

    Component{
        id: mediainfoBtnDelegate
        Widgets.IconControlButton{
            id: infoBtn
            size: VLCStyle.icon_medium
            enabled: player.isPlaying
            iconText: VLCIcons.info
            onClicked: dialogProvider.mediaInfoDialog()
            property bool acceptFocus: true
            text: i18n.qtr("Informations")
        }
    }

    Component{
        id: framebyframeDelegate

        Widgets.IconControlButton{
            id: frameBtn
            size: VLCStyle.icon_medium
            enabled: player.isPlaying
            iconText: VLCIcons.frame_by_frame
            onClicked: player.frameNext()
            property bool acceptFocus: true
            text: i18n.qtr("Next frame")
        }
    }

    Component{
        id: fasterBtnDelegate

        Widgets.IconControlButton{
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

        Widgets.IconControlButton{
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
        Widgets.IconControlButton{
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
        Widgets.IconControlButton{
            id: extdSettingsBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.extended
            onClicked: dialogProvider.extendedDialog()
            property bool acceptFocus: true
            Accessible.name: i18n.qtr("Extended settings")
        }
    }

    Component{
        id: stepFwdBtnDelegate
        Widgets.IconControlButton{
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
        Widgets.IconControlButton{
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
        Widgets.IconControlButton{
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
        VolumeWidget { parentWindow: controlButtons.parentWindow }
    }

    Component {
        id: playerSwitchBtnDelegate

        Widgets.IconControlButton{
            size: VLCStyle.icon_medium
            iconText: VLCIcons.fullscreen

            onClicked: {
                if (isMiniplayer) {
                    g_mainDisplay.showPlayer()
                }
                else {
                    history.previous()
                }
            }

            property bool acceptFocus: true
            text: i18n.qtr("Switch Player")
        }
    }

    Component {
        id: artworkInfoDelegate

        Widgets.FocusBackground {
            id: artworkInfoItem

            property bool paintOnly: false

            property VLCColors colors: VLCStyle.colors

            readonly property real minimumWidth: cover.width
            property real extraWidth: 0

            implicitWidth: paintOnly ? playingItemInfoRow.width : (minimumWidth + extraWidth)

            implicitHeight: playingItemInfoRow.implicitHeight

            Keys.onPressed: {
                if (KeyHelper.matchOk(event) ) {
                    event.accepted = true
                }
            }
            Keys.onReleased: {
                if (!event.accepted && KeyHelper.matchOk(event))
		    g_mainDisplay.showPlayer()
            }

            MouseArea {
                id: artworkInfoMouseArea
                anchors.fill: parent
                visible: !paintOnly
                onClicked: g_mainDisplay.showPlayer()
                hoverEnabled: true
            }

            Row {
                id: playingItemInfoRow
                width: (coverItem.width + infoColumn.width)

                Item {
                    id: coverItem
                    anchors.verticalCenter: parent.verticalCenter
                    implicitHeight: childrenRect.height
                    implicitWidth:  childrenRect.width

                    Rectangle {
                        id: coverRect
                        anchors.fill: cover
                        color: colors.bg
                    }

                    DropShadow {
                        anchors.fill: coverRect
                        source: coverRect
                        radius: 8
                        samples: 17
                        color: VLCStyle.colors.glowColorBanner
                        spread: 0.2
                    }

                    Image {
                        id: cover

                        source: {
                            if (paintOnly)
                                VLCStyle.noArtAlbum
                            else
                                (mainPlaylistController.currentItem.artwork && mainPlaylistController.currentItem.artwork.toString())
                                                                ? mainPlaylistController.currentItem.artwork
                                                                : VLCStyle.noArtAlbum
                        }
                        fillMode: Image.PreserveAspectFit

                        width: VLCStyle.dp(60)
                        height: VLCStyle.dp(60)
                    }
                }

                Column {
                    id: infoColumn
                    anchors.verticalCenter: parent.verticalCenter
                    leftPadding: VLCStyle.margin_xsmall

                    width: (paintOnly ? Math.max(titleLabel.width, artistLabel.width, progressIndicator.width) : implicitWidth) + VLCStyle.margin_xsmall

                    visible: paintOnly || artworkInfoItem.extraWidth > 0

                    ToolTip {
                        text: i18n.qtr("%1\n%2").arg(titleLabel.text).arg(artistLabel.text)
                        visible: (titleLabel.implicitWidth > titleLabel.width || artistLabel.implicitWidth > titleLabel.width)
                                 && (artworkInfoMouseArea.containsMouse || artworkInfoItem.active)
                        delay: 500 
                         
                        contentItem: Text {
                                  text: i18n.qtr("%1\n%2").arg(titleLabel.text).arg(artistLabel.text)
                                  color: colors.tooltipTextColor
                        }

                        background: Rectangle {
                            color: colors.tooltipColor
                        }
                    }

                    Widgets.MenuLabel {
                        id: titleLabel

                        width: {
                            if (!paintOnly)
                                artworkInfoItem.implicitWidth - titleLabel.mapToItem(artworkInfoItem, titleLabel.x, titleLabel.y).x
                        }

                        text: {
                            if (paintOnly)
                                i18n.qtr("Title")
                            else
                                mainPlaylistController.currentItem.title
                        }
                        visible: text !== ""
                        color: colors.text
                    }

                    Widgets.MenuCaption {
                        id: artistLabel
                        width: {
                            if (!paintOnly)
                                artworkInfoItem.implicitWidth - artistLabel.mapToItem(artworkInfoItem, artistLabel.x, artistLabel.y).x
                        }
                        text: {
                            if (paintOnly)
                                i18n.qtr("Artist")
                            else
                                mainPlaylistController.currentItem.artist
                        }
                        visible: text !== ""
                        color: colors.menuCaption
                    }

                    Widgets.MenuCaption {
                        id: progressIndicator
                        width: {
                            if (!paintOnly)
                                artworkInfoItem.implicitWidth - progressIndicator.mapToItem(artworkInfoItem, progressIndicator.x, progressIndicator.y).x
                        }
                        text: {
                            if (paintOnly)
                                " -- / -- "
                            else
                                player.time.toString() + " / " + player.length.toString()
                        }
                        visible: text !== ""
                        color: colors.menuCaption
                    }
                }
            }
        }
    }
}
