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
import QtQuick.Layouts 1.11
import QtQuick.Controls 2.4
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Item{
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

    Component {
        id: fallbackDelegate

        Widgets.AnimatedBackground {
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
    }

    Component{
        id: backBtnDelegate
        Widgets.IconControlButton {
            id: backBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.exit
            text: i18n.qtr("Back")
            onClicked: history.previous()
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
            text: i18n.qtr("Previous")
        }
    }

    Component{
        id:playBtnDelegate

        ToolButton {
            id: playBtn

            width: VLCStyle.icon_medium
            height: width

            scale: (playBtnMouseArea.pressed) ? 0.95 : 1.0

            property VLCColors colors: VLCStyle.colors

            property color color: colors.buttonPlayIcon

            property color colorDisabled: colors.textInactive

            property bool paintOnly: false

            property bool isCursorInside: false

            Keys.onPressed: {
                if (KeyHelper.matchOk(event) ) {
                    event.accepted = true
                }
                Navigation.defaultKeyAction(event)
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
                        duration: VLCStyle.ms75
                        easing.type: Easing.InOutSine
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
                    GradientStop { position: 0.0; color: VLCStyle.colors.buttonPlayA }
                    GradientStop { position: 1.0; color: VLCStyle.colors.buttonPlayB }
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
                            duration: VLCStyle.ms75
                            easing.type: Easing.InOutSine
                        }
                    }
                }

                Rectangle {
                    radius: (width * 0.5)
                    anchors.fill: parent
                    anchors.margins: VLCStyle.dp(1, VLCStyle.scale)

                    color: VLCStyle.colors.white
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
            text: i18n.qtr("Repeat")
        }
    }

    Component{
        id: langBtnDelegate
        Widgets.IconControlButton {
            id: langBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.audiosub

            enabled: langMenuLoader.status === Loader.Ready
            onClicked: langMenuLoader.item.open()

            text: i18n.qtr("Languages and tracks")

            Loader {
                id: langMenuLoader

                active: (typeof rootPlayer !== 'undefined') && (rootPlayer !== null)

                sourceComponent: LanguageMenu {
                    id: langMenu

                    parent: rootPlayer
                    focus: true
                    x: 0
                    y: (rootPlayer.positionSliderY - height)
                    z: 1

                    onOpened: {
                        controlButtons.requestLockUnlockAutoHide(true, controlButtons)
                        if (!!rootPlayer)
                            rootPlayer.menu = langMenu
                    }

                    onClosed: {
                        controlButtons.requestLockUnlockAutoHide(false, controlButtons)
                        langBtn.forceActiveFocus()
                        if (!!rootPlayer)
                            rootPlayer.menu = undefined
                    }
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

            text: i18n.qtr("Playlist")
        }

    }

    Component{
        id: menuBtnDelegate
        Widgets.IconControlButton {
            id: menuBtn
            size: VLCStyle.icon_medium
            iconText: VLCIcons.ellipsis
            text: i18n.qtr("Menu")

            onClicked: contextMenu.popup(this.mapToGlobal(0, 0))

            QmlGlobalMenu {
                id: contextMenu

                ctx: mainctx

                onAboutToShow: controlButtons.requestLockUnlockAutoHide(true, contextMenu)
                onAboutToHide: controlButtons.requestLockUnlockAutoHide(false, contextMenu)
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
            text: i18n.qtr("record")
        }
    }

    Component{
        id: toggleABloopstateDelegate
        Widgets.IconControlButton {
            id: abBtn

            size: VLCStyle.icon_medium
            checked: player.ABloopState !== PlayerController.ABLOOP_STATE_NONE
            onClicked: player.toggleABloopState()
            text: i18n.qtr("A to B")

            iconText: {
                switch(player.ABloopState) {
                  case PlayerController.ABLOOP_STATE_A: return VLCIcons.atob_bg_b
                  case PlayerController.ABLOOP_STATE_B: return VLCIcons.atob_bg_none
                  case PlayerController.ABLOOP_STATE_NONE: return VLCIcons.atob_bg_ab
                }
            }

            Widgets.IconLabel {
                anchors.centerIn: abBtn.contentItem

                color: abBtn.colors.accent

                text: {
                    switch(player.ABloopState) {
                      case PlayerController.ABLOOP_STATE_A: return VLCIcons.atob_fg_a
                      case PlayerController.ABLOOP_STATE_B: return VLCIcons.atob_fg_ab
                      case PlayerController.ABLOOP_STATE_NONE: return ""
                    }
                }
            }
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
            text: i18n.qtr("Snapshot")
        }
    }


    Component{
        id: stopBtnDelegate
        Widgets.IconControlButton{
            id: stopBtn
            size: VLCStyle.icon_medium
            enabled: player.isPlaying
            iconText: VLCIcons.stop
            onClicked: mainPlaylistController.stop()
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
                if (history.current.view === "player")
                    history.previous()
                else
                    g_mainDisplay.showPlayer()
            }

            text: i18n.qtr("Switch Player")
        }
    }

    Component {
        id: artworkInfoDelegate

        Widgets.AnimatedBackground {
            id: artworkInfoItem

            property bool paintOnly: false

            property VLCColors colors: VLCStyle.colors

            readonly property real minimumWidth: cover.width + VLCStyle.focus_border * 2
            property real extraWidth: 0

            implicitWidth: playingItemInfoRow.width + VLCStyle.focus_border * 2
            implicitHeight: playingItemInfoRow.height + VLCStyle.focus_border * 2

            activeBorderColor: colors.bgFocus

            Keys.onPressed: {
                if (KeyHelper.matchOk(event) ) {
                    event.accepted = true
                }
                Navigation.defaultKeyAction(event)
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

                anchors.centerIn: parent

                width: (coverItem.width + infoColumn.width + spacing)

                spacing: infoColumn.visible ? VLCStyle.margin_xsmall : 0

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

                        ToolTip {
                            x: parent.x

                            visible: artworkInfoItem.visible
                                     && (titleLabel.implicitWidth > titleLabel.width || artistLabel.implicitWidth > titleLabel.width)
                                     && (artworkInfoMouseArea.containsMouse || artworkInfoItem.active)
                            delay: 500

                            contentItem: Text {
                                text: i18n.qtr("%1\n%2\n%3").arg(titleLabel.text).arg(artistLabel.text).arg(progressIndicator.text)
                                color: colors.tooltipTextColor
                            }

                            background: Rectangle {
                                color: colors.tooltipColor
                            }
                        }
                    }
                }

                Column {
                    id: infoColumn
                    anchors.verticalCenter: parent.verticalCenter

                    readonly property real preferredWidth: Math.max(titleLabel.implicitWidth, artistLabel.implicitWidth, progressIndicator.implicitWidth)
                    width: ((artworkInfoItem.extraWidth > preferredWidth) || (paintOnly)) ? preferredWidth
                                                                                          : artworkInfoItem.extraWidth

                    visible: width > 0

                    Widgets.MenuLabel {
                        id: titleLabel

                        width: parent.width

                        text: {
                            if (paintOnly)
                                i18n.qtr("Title")
                            else
                                mainPlaylistController.currentItem.title
                        }
                        color: colors.text
                    }

                    Widgets.MenuCaption {
                        id: artistLabel

                        width: parent.width

                        text: {
                            if (paintOnly)
                                i18n.qtr("Artist")
                            else
                                mainPlaylistController.currentItem.artist
                        }
                        color: colors.menuCaption
                    }

                    Widgets.MenuCaption {
                        id: progressIndicator

                        width: parent.width

                        text: {
                            if (paintOnly)
                                " -- / -- "
                            else
                                player.time.toString() + " / " + player.length.toString()
                        }
                        color: colors.menuCaption
                    }
                }
            }
        }
    }

    Component {
        id: playbackSpeedButtonDelegate

        Widgets.IconControlButton {
            id: playbackSpeedButton

            size: VLCStyle.icon_medium
            text: i18n.qtr("Playback Speed")
            color: playbackSpeedPopup.visible ? colors.accent : colors.playerControlBarFg

            onClicked: playbackSpeedPopup.open()

            PlaybackSpeed {
                id: playbackSpeedPopup

                z: 1
                colors: playbackSpeedButton.colors
                focus: true
                parent: playbackSpeedButton.paintOnly
                        ? playbackSpeedButton // button is not part of main display (ToolbarEditorDialog)
                        : (history.current.view === "player") ? rootPlayer : g_mainDisplay

                onOpened: {
                    // update popup coordinates
                    //
                    // mapFromItem is affected by various properties of source and target objects
                    // which can't be represented in a binding expression so a initial setting in
                    // object defination (x: clamp(...)) doesn't work, so we set x and y on initial open
                    x = Qt.binding(function () {
                        // coords are mapped through playbackSpeedButton.parent so that binding is generated based on playbackSpeedButton.x
                        var mappedParentCoordinates = parent.mapFromItem(playbackSpeedButton.parent, playbackSpeedButton.x, 0)
                        return Helpers.clamp(mappedParentCoordinates.x  - ((width - playbackSpeedButton.width) / 2),
                                             VLCStyle.margin_xxsmall + VLCStyle.applicationHorizontalMargin,
                                             parent.width - VLCStyle.applicationHorizontalMargin - VLCStyle.margin_xxsmall - width)
                    })

                    y = Qt.binding(function () {
                        // coords are mapped through playbackSpeedButton.parent so that binding is generated based on playbackSpeedButton.y
                        var mappedParentCoordinates = parent.mapFromItem(playbackSpeedButton.parent, 0, playbackSpeedButton.y)
                        return mappedParentCoordinates.y - playbackSpeedPopup.height - VLCStyle.margin_xxsmall
                    })

                    // player related --
                    controlButtons.requestLockUnlockAutoHide(true, controlButtons)
                    if (!!rootPlayer)
                        rootPlayer.menu = playbackSpeedPopup
                }

                onClosed: {
                    controlButtons.requestLockUnlockAutoHide(false, controlButtons)
                    playbackSpeedButton.forceActiveFocus()
                    if (!!rootPlayer)
                        rootPlayer.menu = undefined
                }
            }

            Label {
                anchors.centerIn: parent
                font.pixelSize: VLCStyle.fontSize_normal
                text: !playbackSpeedButton.paintOnly ? i18n.qtr("%1x").arg(+player.rate.toFixed(2)) : i18n.qtr("1x")
                color: playbackSpeedButton.background.foregroundColor // IconToolButton.background is a AnimatedBackground
            }
        }
    }
}
