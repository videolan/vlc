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
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import Qt5Compat.GraphicalEffects
import QtQuick.Window

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///playlist/" as PL
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///dialogs/" as DG
import "qrc:///util/" as Util

FocusScope {
    id: rootPlayer

    // Properties

    //behave like a Page
    property var pagePrefix: []

    property bool hasEmbededVideo: MainCtx.hasEmbededVideo

    readonly property int positionSliderY: controlBar.y + controlBar.sliderY

    readonly property string coverSource: {
        if (MainPlaylistController.currentItem.artwork &&
            MainPlaylistController.currentItem.artwork.toString())
            MainPlaylistController.currentItem.artwork
        else if (Player.hasVideoOutput)
            VLCStyle.noArtVideoCover
        else
            VLCStyle.noArtAlbumCover

    }

    // Private

    property int _lockAutoHide: 0

    readonly property bool _autoHide: _lockAutoHide == 0
                                      && rootPlayer.hasEmbededVideo
                                      && Player.hasVideoOutput
                                      && playlistpopup.state !== "visible"

    property bool _controlsUnderVideo: (MainCtx.pinVideoControls
                                        &&
                                        (MainCtx.intfMainWindow.visibility !== Window.FullScreen))

    property bool _keyPressed: false

    // Settings

    layer.enabled: (StackView.status === StackView.Deactivating || StackView.status === StackView.Activating)

    Accessible.role: Accessible.Client
    Accessible.name: qsTr("Player")

    // Events

    Component.onCompleted: MainCtx.preferHotkeys = true
    Component.onDestruction: MainCtx.preferHotkeys = false

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => {
        if (event.accepted)
            return

        _keyPressed = true

        rootPlayer.Navigation.defaultKeyAction(event)

        //unhandled keys are forwarded as hotkeys
        if (!event.accepted || controlBar.state !== "visible")
            MainCtx.sendHotkey(event.key, event.modifiers);
    }

    Keys.onReleased: (event) => {
        if (event.accepted || _keyPressed === false)
            return

        _keyPressed = false

        if (event.key === Qt.Key_Menu) {
            toolbarAutoHide.toggleForceVisible()
        } else {
            rootPlayer.Navigation.defaultKeyReleaseAction(event)
        }
    }


    on_AutoHideChanged: {
        if (_autoHide)
            toolbarAutoHide.restart()
    }

    on_ControlsUnderVideoChanged: {
        lockUnlockAutoHide(_controlsUnderVideo)
        if (_controlsUnderVideo)
            toolbarAutoHide.setVisibleControlBar(true)
    }

    Connections {
        target: Player

        function onVolumeChanged() {
            animationVolume.restart()
        }
    }

    // Functions

    function lockUnlockAutoHide(lock) {
        _lockAutoHide += lock ? 1 : -1;
        console.assert(_lockAutoHide >= 0)
    }

    // Private

    function _onNavigationCancel() {
        if (rootPlayer.hasEmbededVideo && controlBar.state === "visible") {
            toolbarAutoHide.setVisibleControlBar(false)
        } else {
            if (MainCtx.hasEmbededVideo && !MainCtx.canShowVideoPIP) {
               MainPlaylistController.stop()
            }
            History.previous()
        }
    }

    //we draw both the view and the window here
    ColorContext {
        id: windowTheme

        // NOTE: We force the night theme when playing a video.
        palette: (MainCtx.hasEmbededVideo && MainCtx.pinVideoControls === false)
                 ? VLCStyle.darkPalette
                 : VLCStyle.palette

        colorSet: ColorContext.Window
    }

    PlayerPlaylistVisibilityFSM {
        id: playlistVisibility

        onShowPlaylist: {
            MainCtx.playlistVisible = true
        }

        onHidePlaylist: {
            MainCtx.playlistVisible = false
        }
    }

    Connections {
        target: MainCtx

        //playlist
        function onPlaylistDockedChanged() {
            playlistVisibility.updatePlaylistDocked()
        }
        function onPlaylistVisibleChanged() {
            playlistVisibility.updatePlaylistVisible()
        }
        function onHasEmbededVideoChanged() {
            playlistVisibility.updateVideoEmbed()
        }
    }

    VideoSurface {
        id: videoSurface

        ctx: MainCtx
        visible: rootPlayer.hasEmbededVideo
        enabled: rootPlayer.hasEmbededVideo
        anchors.fill: parent
        anchors.topMargin: rootPlayer._controlsUnderVideo ? topBar.height : 0
        anchors.bottomMargin: rootPlayer._controlsUnderVideo ? controlBar.height : 0

        onMouseMoved: {
            //short interval for mouse events
            if (Player.isInteractive)
            {
                toggleControlBarButtonAutoHide.restart()
                videoSurface.cursorShape = Qt.ArrowCursor
            }
            else
                toolbarAutoHide.setVisible(1000)
        }
    }

    // background image
    Rectangle {
        visible: !rootPlayer.hasEmbededVideo
        focus: false
        color: bgtheme.bg.primary
        anchors.fill: parent

        readonly property ColorContext colorContext: ColorContext {
            id: bgtheme
            colorSet: ColorContext.View
        }

        PlayerBlurredBackground {
            id: backgroundImage

            //destination aspect ratio
            readonly property real dar: parent.width / parent.height

            anchors.centerIn: parent
            width: (cover.sar < dar) ? parent.width :  parent.height * cover.sar
            height: (cover.sar < dar) ? parent.width / cover.sar :  parent.height

            source: cover

            screenColor: VLCStyle.setColorAlpha(bgtheme.bg.primary, .55)
            overlayColor: VLCStyle.setColorAlpha(Qt.tint(bgtheme.fg.primary, bgtheme.bg.primary), 0.4)
        }
    }

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right

        implicitHeight: VLCStyle.dp(206, VLCStyle.scale)

        opacity: topBar.opacity
        visible: !topBarAcrylicBg.visible && MainCtx.hasEmbededVideo

        gradient: Gradient {
            GradientStop { position: 0; color: Qt.rgba(0, 0, 0, .8) }
            GradientStop { position: 1; color: "transparent" }
        }
    }

    Rectangle {
        anchors.bottom: controlBar.bottom
        anchors.left: controlBar.left
        anchors.right: controlBar.right

        implicitHeight: VLCStyle.dp(206, VLCStyle.scale)

        opacity: controlBar.opacity

        gradient: Gradient {
            GradientStop { position: 0; color: "transparent" }
            GradientStop { position: .64; color: Qt.rgba(0, 0, 0, .8) }
            GradientStop { position: 1; color: "black" }
        }

        visible: !(controlBar.background && controlBar.background.visible)
    }

    TopBar {
        id: topBar

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right

        z: 1

        topMargin: VLCStyle.applicationVerticalMargin
        sideMargin: VLCStyle.applicationHorizontalMargin

        textWidth: (MainCtx.playlistVisible) ? rootPlayer.width - playlistpopup.width
                                             : rootPlayer.width

        // NOTE: With pinned controls, the top controls are hidden when switching to
        //       fullScreen. Except when resume is visible
        visible: (MainCtx.pinVideoControls === false
                  ||
                  MainCtx.intfMainWindow.visibility !== Window.FullScreen
                  ||
                  resumeVisible)

        focus: true
        title: MainPlaylistController.currentItem.title

        pinControls: MainCtx.pinVideoControls

        showCSD: MainCtx.clientSideDecoration && (MainCtx.intfMainWindow.visibility !== Window.FullScreen)
        showToolbar: MainCtx.hasToolbarMenu && (MainCtx.intfMainWindow.visibility !== Window.FullScreen)

        Navigation.parentItem: rootPlayer
        Navigation.downItem: {
            if (playlistVisibility.isPlaylistVisible)
                return playlistpopup
            if (audioControls.visible)
                return audioControls
            if (Player.isInteractive)
                return toggleControlBarButton
            return controlBar
        }

        onTogglePlaylistVisibility: playlistVisibility.togglePlaylistVisibility()

        onRequestLockUnlockAutoHide: (lock) => {
            rootPlayer.lockUnlockAutoHide(lock)
        }

        onBackRequested: {
            if (MainCtx.hasEmbededVideo && !MainCtx.canShowVideoPIP) {
               MainPlaylistController.stop()
            }
            History.previous()
        }

        Util.FadeControllerStateGroup {
            target: topBar
        }

        // TODO: Make TopBar a Control and use background
        Widgets.AcrylicBackground {
            id: topBarAcrylicBg

            z: -1

            anchors.fill: parent

            opacity: (MainCtx.intfMainWindow.visibility === Window.FullScreen) ? MainCtx.pinOpacity
                                                                               : 1.0

            tintColor: windowTheme.bg.primary

            visible: MainCtx.pinVideoControls
        }
    }

    MouseArea {
        id: centerContent

        readonly property ColorContext colorContext: ColorContext {
            id: centerTheme
            colorSet: ColorContext.View
        }

        anchors {
            left: parent.left
            right: parent.right
            top: topBar.bottom
            bottom: controlBar.top
            topMargin: VLCStyle.margin_xsmall
            bottomMargin: VLCStyle.margin_xsmall
        }

        visible: !rootPlayer.hasEmbededVideo

        onWheel: (wheel) => {
            if (rootPlayer.hasEmbededVideo) {
                wheel.accepted = false

                return
            }

            wheel.accepted = true

            var delta = wheel.angleDelta.y

            if (delta === 0)
                return

            Helpers.applyVolume(Player, delta)
        }

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 0


            Item {
                id: coverItem
                Layout.preferredHeight: rootPlayer.height / sizeConstant
                Layout.preferredWidth: cover.paintedWidth
                Layout.maximumHeight: centerContent.height
                Layout.alignment: Qt.AlignHCenter

                readonly property real sizeConstant: 2.7182

                Image {
                    id: cover

                    //source aspect ratio
                    readonly property real sar: paintedWidth / paintedHeight
                    readonly property int maximumWidth: MainCtx.screen
                                                          ? Helpers.alignUp((MainCtx.screen.availableGeometry.width / coverItem.sizeConstant), 32)
                                                          : 1024
                    readonly property int maximumHeight: MainCtx.screen
                                                          ? Helpers.alignUp((MainCtx.screen.availableGeometry.height / coverItem.sizeConstant), 32)
                                                          : 1024

                    readonly property int maximumSize: Math.min(maximumWidth, maximumHeight)

                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    source: VLCAccessImage.uri(rootPlayer.coverSource)
                    fillMode: Image.PreserveAspectFit
                    mipmap: true
                    cache: false
                    asynchronous: true

                    sourceSize: Qt.size(maximumSize, maximumSize)

                    Accessible.role: Accessible.Graphic
                    Accessible.name: qsTr("Cover")

                    onStatusChanged: {
                        if (status === Image.Ready)
                            backgroundImage.scheduleUpdate()
                    }
                }

                //don't use a DoubleShadow here as cover size will change
                //dynamically with the window size
                Widgets.CoverShadow {
                    anchors.fill: parent
                    source: cover
                    primaryVerticalOffset: VLCStyle.dp(24)
                    primaryBlurRadius: VLCStyle.dp(54)
                    secondaryVerticalOffset: VLCStyle.dp(5)
                    secondaryBlurRadius: VLCStyle.dp(14)
                }
            }

            Widgets.SubtitleLabel {
                id: albumLabel

                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: VLCStyle.margin_xxlarge

                Binding on visible {
                    delayed: true
                    when: albumLabel.componentCompleted
                    value: centerContent.height > (albumLabel.y + albumLabel.height)
                }

                text: MainPlaylistController.currentItem.album
                font.pixelSize: VLCStyle.fontSize_xxlarge
                horizontalAlignment: Text.AlignHCenter
                color: centerTheme.fg.primary
                Accessible.description: qsTr("album")

                property bool componentCompleted: false

                Component.onCompleted: {
                    componentCompleted = true
                }
            }

            Widgets.MenuLabel {
                id: artistLabel

                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: VLCStyle.margin_small

                Binding on visible {
                    delayed: true
                    when: artistLabel.componentCompleted
                    value: centerContent.height > (artistLabel.y + artistLabel.height)
                }

                text: MainPlaylistController.currentItem.artist
                font.weight: Font.Light
                horizontalAlignment: Text.AlignHCenter
                color: centerTheme.fg.primary
                Accessible.description: qsTr("artist")

                property bool componentCompleted: false

                Component.onCompleted: {
                    componentCompleted = true
                }
            }

            Widgets.NavigableRow {
                id: audioControls

                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: VLCStyle.margin_large

                Binding on visible {
                    delayed: true
                    when: audioControls.componentCompleted
                    value: Player.videoTracks.count === 0 && centerContent.height > (audioControls.y + audioControls.height)
                }

                focus: visible
                spacing: VLCStyle.margin_xxsmall
                Navigation.parentItem: rootPlayer
                Navigation.upItem: topBar
                Navigation.downItem: Player.isInteractive ? toggleControlBarButton : controlBar

                property bool componentCompleted: false

                Component.onCompleted: {
                    componentCompleted = true
                }

                model: ObjectModel {
                    Widgets.IconToolButton {
                        text: VLCIcons.skip_back
                        font.pixelSize: VLCStyle.icon_audioPlayerButton
                        onClicked: Player.jumpBwd()
                        description: qsTr("Step back")
                    }

                    Widgets.IconToolButton {
                        text: VLCIcons.visualization
                        font.pixelSize: VLCStyle.icon_audioPlayerButton
                        onClicked: Player.toggleVisualization()
                        description: qsTr("Visualization")
                    }

                    Widgets.IconToolButton{
                        text: VLCIcons.skip_for
                        font.pixelSize: VLCStyle.icon_audioPlayerButton
                        onClicked: Player.jumpFwd()
                        description: qsTr("Step forward")
                    }
                }
            }
        }

        Widgets.SubtitleLabel {
            id: labelVolume

            anchors.right: parent.right
            anchors.top: parent.top

            anchors.rightMargin: VLCStyle.margin_normal
            anchors.topMargin: VLCStyle.margin_xxsmall

            visible: false

            text: qsTr("Volume %1%").arg(Math.round(Player.volume * 100))

            color: centerTheme.fg.primary

            font.weight: Font.Normal

            SequentialAnimation {
                id: animationVolume

                PropertyAction { target: labelVolume; property: "visible"; value: true }

                PauseAnimation { duration: VLCStyle.duration_humanMoment }

                PropertyAction { target: labelVolume; property: "visible"; value: false }
            }
        }
    }

    Widgets.DrawerExt {
        id: playlistpopup

        anchors {
            // NOTE: When the controls are pinned we display the playqueue under the topBar.
            top: (rootPlayer._controlsUnderVideo) ? topBar.bottom
                                                  : parent.top

            right: parent.right
            bottom: parent.bottom

            bottomMargin: parent.height - rootPlayer.positionSliderY
        }

        focus: false
        edge: Widgets.DrawerExt.Edges.Right

        Binding on state {
            when: playlistVisibility.started
            value: playlistVisibility.isPlaylistVisible ? "visible" : "hidden"
        }

        component: Rectangle {
            width: Helpers.clamp(rootPlayer.width / resizeHandle.widthFactor
                                 , playlistView.minimumWidth
                                 , (rootPlayer.width + playlistView.rightPadding) / 2)

            height: playlistpopup.height

            color: VLCStyle.setColorAlpha(windowTheme.bg.primary, 0.8)


            PL.PlaylistListView {
                id: playlistView

                useAcrylic: false
                focus: true

                wheelEnabled: true

                anchors.fill: parent
                rightPadding: VLCStyle.applicationHorizontalMargin
                topPadding:  {
                    if (rootPlayer._controlsUnderVideo)
                        return VLCStyle.margin_normal
                    else
                        // NOTE: We increase the padding accordingly to avoid overlapping the TopBar.
                        return topBar.reservedHeight
                }

                Navigation.parentItem: rootPlayer
                Navigation.upItem: topBar
                Navigation.downItem: Player.isInteractive ? toggleControlBarButton : controlBar
                Navigation.leftAction: closePlaylist
                Navigation.cancelAction: closePlaylist

                function closePlaylist() {
                    playlistVisibility.togglePlaylistVisibility()
                    if (audioControls.visible)
                        audioControls.forceActiveFocus()
                    else
                        controlBar.forceActiveFocus()
                }


                Widgets.HorizontalResizeHandle {
                    id: resizeHandle

                    property bool _inhibitMainCtxUpdate: false

                    parent: playlistView

                    anchors {
                        top: parent.top
                        bottom: parent.bottom
                        left: parent.left
                    }

                    atRight: false
                    targetWidth: playlistpopup.width
                    sourceWidth: rootPlayer.width

                    onWidthFactorChanged: {
                        if (!_inhibitMainCtxUpdate)
                            MainCtx.playerPlaylistWidthFactor = widthFactor
                    }

                    Component.onCompleted:  _updateFromMainCtx()

                    function _updateFromMainCtx() {
                        if (widthFactor == MainCtx.playerPlaylistWidthFactor)
                            return

                        _inhibitMainCtxUpdate = true
                        widthFactor = MainCtx.playerPlaylistWidthFactor
                        _inhibitMainCtxUpdate = false
                    }

                    Connections {
                        target: MainCtx

                        function onPlaylistWidthFactorChanged() {
                            resizeHandle._updateFromMainCtx()
                        }
                    }
                }
            }
        }
        onStateChanged: {
            if (state === "hidden")
                toolbarAutoHide.restart()
        }
    }

    DG.Dialogs {
        z: 10
        bgContent: rootPlayer

        anchors {
            bottom: controlBar.visible ? controlBar.top : rootPlayer.bottom
            left: parent.left
            right: parent.right

            bottomMargin: (rootPlayer._controlsUnderVideo || !controlBar.visible)
                          ? 0 : - VLCStyle.margin_large
        }
    }

    Timer {
        // toggleControlBarButton's visibility depends on this timer
        id: toggleControlBarButtonAutoHide
        running: true
        repeat: false
        interval: 3000

        onTriggered: {
            // Cursor hides when toggleControlBarButton is not visible
            videoSurface.forceActiveFocus()
            videoSurface.cursorShape = Qt.BlankCursor
        }
    }

    NavigationBox {
        id: navBox
        visible: Player.isInteractive && navBox.show
                    && (toggleControlBarButtonAutoHide.running
                    || navBox.hovered || !rootPlayer.hasEmbededVideo)

        x: rootPlayer.x + VLCStyle.margin_normal + VLCStyle.applicationHorizontalMargin
        y: controlBar.y - navBox.height - VLCStyle.margin_normal

        dragXMin: 0
        dragXMax: rootPlayer.width - navBox.width
        dragYMin: 0
        dragYMax: rootPlayer.height - navBox.height

        Drag.onDragStarted: {
            navBox.x = drag.x
            navBox.y = drag.y
        }
    }

    // NavigationBox's visibility depends on this timer
    Connections {
        target: MainCtx
        function onNavBoxToggled() { toggleControlBarButtonAutoHide.restart() }
    }

    Connections {
        target: rootPlayer
        function onWidthChanged() {
            if (navBox.x > navBox.dragXMax)
                navBox.x = navBox.dragXMax
        }
        function onHeightChanged() {
            if (navBox.y > navBox.dragYMax)
                navBox.y = navBox.dragYMax
        }
    }

   Widgets.ButtonExt {
        id: toggleControlBarButton
        visible: Player.isInteractive
                 && rootPlayer.hasEmbededVideo
                 && !(MainCtx.pinVideoControls && !Player.fullscreen)
                 && (toggleControlBarButtonAutoHide.running === true
                     || controlBar.state !== "hidden" || toggleControlBarButton.hovered)
        focus: true
        anchors {
            bottom: controlBar.state === "hidden" ? parent.bottom : controlBar.top
            horizontalCenter: parent.horizontalCenter
        }
        iconSize: VLCStyle.icon_large
        iconTxt: controlBar.state === "hidden" ? VLCIcons.expand_inverted : VLCIcons.expand

        Navigation.parentItem: rootPlayer
        Navigation.upItem: playlistVisibility.isPlaylistVisible ? playlistpopup : (audioControls.visible ? audioControls : topBar)
        Navigation.downItem: controlBar

        onClicked: {
            toolbarAutoHide.toggleForceVisible();
        }
    }

    ControlBar {
        id: controlBar

        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }

        hoverEnabled: true

        focus: true

        rightPadding: VLCStyle.applicationHorizontalMargin
        leftPadding: VLCStyle.applicationHorizontalMargin
        bottomPadding: VLCStyle.applicationVerticalMargin + VLCStyle.margin_xsmall

        textPosition: (MainCtx.pinVideoControls)
                      ? ControlBar.TimeTextPosition.LeftRightSlider
                      : ControlBar.TimeTextPosition.AboveSlider

        // hide right text so that it won't overlap with playlist
        showRemainingTime: (textPosition !== ControlBar.TimeTextPosition.AboveSlider)
                           || !playlistVisibility.isPlaylistVisible

        onStateChanged: {
            if (state === "visible")
                showChapterMarks()
        }

        Navigation.parentItem: rootPlayer
        Navigation.upItem: {
            if (playlistVisibility.isPlaylistVisible)
                return playlistpopup
            if (Player.isInteractive)
                return toggleControlBarButton
            if (audioControls.visible)
                return audioControls
            return topBar
        }

        onRequestLockUnlockAutoHide: (lock) => rootPlayer.lockUnlockAutoHide(lock)

        identifier: (Player.hasVideoOutput) ? PlayerControlbarModel.Videoplayer
                                            : PlayerControlbarModel.Audioplayer

        onHoveredChanged: rootPlayer.lockUnlockAutoHide(hovered)

        background: Rectangle {
            id: controlBarBackground

            visible: !MainCtx.hasEmbededVideo || MainCtx.pinVideoControls

            opacity: MainCtx.pinVideoControls ? MainCtx.pinOpacity : 0.7

            color: windowTheme.bg.primary
        }

        Util.FadeControllerStateGroup {
            target: controlBar
        }
    }

    Timer {
        id: toolbarAutoHide
        running: true
        repeat: false
        interval: 3000
        onTriggered: {
            setVisibleControlBar(false)
        }

        function setVisibleControlBar(visible) {
            if (visible)
            {
                controlBar.state = "visible"
                topBar.state = "visible"
                if (!controlBar.focus && !topBar.focus)
                    controlBar.forceActiveFocus()

                videoSurface.cursorShape = Qt.ArrowCursor
            }
            else
            {
                if (!rootPlayer._autoHide)
                    return;
                controlBar.state = "hidden"
                topBar.state = "hidden"
                videoSurface.forceActiveFocus()
                videoSurface.cursorShape = Qt.BlankCursor
            }
        }

        function setVisible(duration) {
            setVisibleControlBar(true)
            toolbarAutoHide.interval = duration
            toolbarAutoHide.restart()
        }

        function toggleForceVisible() {
            setVisibleControlBar(controlBar.state !== "visible")
            toolbarAutoHide.stop()
        }

    }

    //filter key events to keep toolbar
    //visible when user navigates within the control bar
    KeyEventFilter {
        id: filter
        target: MainCtx.intfMainWindow
        enabled: controlBar.state === "visible"
                 && (controlBar.focus || topBar.focus)
        Keys.onPressed: (event) => toolbarAutoHide.setVisible(5000)
    }

    Connections {
        target: MainCtx
        function onAskShow() {
            toolbarAutoHide.toggleForceVisible()
        }
    }
}
