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
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11
import QtQml.Models 2.11
import QtGraphicalEffects 1.0
import QtQuick.Window 2.11

import org.videolan.vlc 0.1
import org.videolan.compat 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///playlist/" as PL
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///dialogs/" as DG

FocusScope {
    id: rootPlayer

    //menu/overlay to dismiss
    property var menu: undefined
    property int _lockAutoHide: 0
    readonly property bool _autoHide: _lockAutoHide == 0
                                      && rootPlayer.hasEmbededVideo
                                      && Player.hasVideoOutput
                                      && playlistpopup.state !== "visible"

    property bool pinVideoControls: MainCtx.pinVideoControls && (MainCtx.intfMainWindow.visibility !== Window.FullScreen)
    property bool hasEmbededVideo: MainCtx.hasEmbededVideo
    readonly property int positionSliderY: controlBarView.y + controlBarView.sliderY
    readonly property string coverSource: {
        if (mainPlaylistController.currentItem.artwork && mainPlaylistController.currentItem.artwork.toString())
            mainPlaylistController.currentItem.artwork
        else if (Player.hasVideoOutput)
            VLCStyle.noArtVideoCover
        else
            VLCStyle.noArtAlbumCover

    }
    property bool _keyPressed: false

    layer.enabled: (StackView.status === StackView.Deactivating || StackView.status === StackView.Activating)

    // Events

    Component.onCompleted: MainCtx.preferHotkeys = true
    Component.onDestruction: MainCtx.preferHotkeys = false

    Keys.priority: Keys.AfterItem
    Keys.onPressed: {
        if (event.accepted)
            return

        _keyPressed = true

        rootPlayer.Navigation.defaultKeyAction(event)

        //unhandled keys are forwarded as hotkeys
        if (!event.accepted || controlBarView.state !== "visible")
            MainCtx.sendHotkey(event.key, event.modifiers);
    }

    Keys.onReleased: {
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

    onPinVideoControlsChanged: {
        lockUnlockAutoHide(pinVideoControls)
        if (pinVideoControls)
            toolbarAutoHide.setVisibleControlBar(true)
    }

    // Functions

    function applyMenu(menu) {
        if (rootPlayer.menu === menu)
            return

        // NOTE: When applying a new menu we hide the previous one.
        if (menu)
            dismiss()

        rootPlayer.menu = menu
    }

    function dismiss() {
        if ((typeof menu === undefined) || !menu)
            return
        if (menu.hasOwnProperty("dismiss"))
            menu.dismiss()
        else if (menu.hasOwnProperty("close"))
            menu.close()
    }

    function lockUnlockAutoHide(lock) {
        _lockAutoHide += lock ? 1 : -1;
        console.assert(_lockAutoHide >= 0)
    }

    // Private

    function _onNavigationCancel() {
        if (rootPlayer.hasEmbededVideo && controlBarView.state === "visible") {
            toolbarAutoHide.setVisibleControlBar(false)
        } else {
            if (MainCtx.hasEmbededVideo && !MainCtx.canShowVideoPIP) {
               mainPlaylistController.stop()
            }
            History.previous()
        }
    }

    //we draw both the view and the window here
    ColorContext {
        id: windowTheme
        // NOTE: We force the night theme when playing a video.
        palette: (MainCtx.hasEmbededVideo && !rootPlayer.pinVideoControls) ? VLCStyle.darkPalette
                                                                           : VLCStyle.palette
        colorSet: ColorContext.Window
    }

    PlayerPlaylistVisibilityFSM {
        id: playlistVisibility

        onShowPlaylist: {
            rootPlayer.lockUnlockAutoHide(true)
            MainCtx.playlistVisible = true
        }

        onHidePlaylist: {
            rootPlayer.lockUnlockAutoHide(false)
            MainCtx.playlistVisible = false
        }
    }

    Connections {
        target: MainCtx

        //playlist
        onPlaylistDockedChanged: playlistVisibility.updatePlaylistDocked()
        onPlaylistVisibleChanged: playlistVisibility.updatePlaylistVisible()
        onHasEmbededVideoChanged: playlistVisibility.updateVideoEmbed()
    }

    VideoSurface {
        id: videoSurface

        ctx: MainCtx
        visible: rootPlayer.hasEmbededVideo
        enabled: rootPlayer.hasEmbededVideo
        anchors.fill: parent
        anchors.topMargin: rootPlayer.pinVideoControls ? topcontrolView.height : 0
        anchors.bottomMargin: rootPlayer.pinVideoControls ? controlBarView.height : 0

        onMouseMoved: {
            //short interval for mouse events
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

    /// Backgrounds of topControlbar and controlBar are drawn separately since they can outgrow their content
    Component {
        id: backgroundForPinnedControls

        Rectangle {
            width: rootPlayer.width
            color: windowTheme.bg.primary
        }
    }

    Component {
        id: acrylicBackground

        Widgets.AcrylicBackground {
            width: rootPlayer.width
            tintColor: windowTheme.bg.primary
        }
    }

    /* top control bar background */
    Widgets.DrawerExt {
        edge: Widgets.DrawerExt.Edges.Top
        state: topcontrolView.state
        width: parent.width
        visible: rootPlayer.hasEmbededVideo || rootPlayer.pinVideoControls
        height: contentItem.height

        component: {
            if (rootPlayer.pinVideoControls)
                return acrylicBackground
            else
                return topcontrolViewBackground
        }

        onContentItemChanged: {
            if (rootPlayer.pinVideoControls)
                contentItem.height = Qt.binding(function () { return topcontrolView.height + topcontrolView.anchors.topMargin; })
        }

        Component {
            id: topcontrolViewBackground

            Rectangle {
                width: rootPlayer.width
                height: VLCStyle.dp(206, VLCStyle.scale)
                gradient: Gradient {
                    GradientStop { position: 0; color: Qt.rgba(0, 0, 0, .8) }
                    GradientStop { position: 1; color: "transparent" }
                }
            }
        }
    }

    /* bottom control bar background */
    Widgets.DrawerExt {
        anchors.bottom: controlBarView.bottom
        anchors.left: controlBarView.left
        anchors.right: controlBarView.right
        height: contentItem.height
        edge: Widgets.DrawerExt.Edges.Bottom
        state: controlBarView.state
        component: rootPlayer.pinVideoControls
                   ? backgroundForPinnedControls
                   : (rootPlayer.hasEmbededVideo ? forVideoMedia : forMusicMedia)
        onContentItemChanged: {
            if (rootPlayer.pinVideoControls)
                contentItem.height = Qt.binding(function () { return rootPlayer.height - rootPlayer.positionSliderY; })
        }

        Component {
            id: forVideoMedia

            Rectangle {
                width: rootPlayer.width
                height: VLCStyle.dp(206, VLCStyle.scale)
                gradient: Gradient {
                    GradientStop { position: 0; color: "transparent" }
                    GradientStop { position: .64; color: Qt.rgba(0, 0, 0, .8) }
                    GradientStop { position: 1; color: "black" }
                }
            }
        }

        Component {
            id: forMusicMedia

            Rectangle {
                width: controlBarView.width
                height: controlBarView.height - (rootPlayer.positionSliderY - controlBarView.y)
                color: windowTheme.bg.primary
                opacity: 0.7
            }
        }
    }

    Widgets.DrawerExt{
        id: topcontrolView

        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }

        z: 1
        edge: Widgets.DrawerExt.Edges.Top
        state: "visible"

        component: TopBar {
            id: topbar

            width: topcontrolView.width
            height: topbar.implicitHeight

            topMargin: VLCStyle.applicationVerticalMargin
            sideMargin: VLCStyle.applicationHorizontalMargin

            textWidth: (MainCtx.playlistVisible) ? rootPlayer.width - playlistpopup.width
                                                 : rootPlayer.width

            focus: true
            title: mainPlaylistController.currentItem.title

            pinControls: rootPlayer.pinVideoControls
            showCSD: MainCtx.clientSideDecoration && (MainCtx.intfMainWindow.visibility !== Window.FullScreen)
            showToolbar: MainCtx.hasToolbarMenu && (MainCtx.intfMainWindow.visibility !== Window.FullScreen)

            Navigation.parentItem: rootPlayer
            Navigation.downItem: playlistpopup.showPlaylist ? playlistpopup : (audioControls.visible ? audioControls : controlBarView)

            onTogglePlaylistVisibility: playlistVisibility.togglePlaylistVisibility()

            onRequestLockUnlockAutoHide: {
                rootPlayer.lockUnlockAutoHide(lock)
            }

            onBackRequested: {
                if (MainCtx.hasEmbededVideo && !MainCtx.canShowVideoPIP) {
                   mainPlaylistController.stop()
                }
                History.previous()
            }
        }
    }

    Item {
        id: centerContent

        readonly property ColorContext colorContext: ColorContext {
            id: centerTheme
            colorSet: ColorContext.View
        }

        anchors {
            left: parent.left
            right: parent.right
            top: topcontrolView.bottom
            bottom: controlBarView.top
            topMargin: VLCStyle.margin_xsmall
            bottomMargin: VLCStyle.margin_xsmall
        }

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 0
            visible: !rootPlayer.hasEmbededVideo

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
                    source: rootPlayer.coverSource
                    fillMode: Image.PreserveAspectFit
                    mipmap: true
                    cache: false
                    asynchronous: true

                    sourceSize: Qt.size(maximumSize, maximumSize)

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

                BindingCompat on visible {
                    delayed: true
                    value: centerContent.height > (albumLabel.y + albumLabel.height)
                }

                text: mainPlaylistController.currentItem.album
                font.pixelSize: VLCStyle.fontSize_xxlarge
                horizontalAlignment: Text.AlignHCenter
                color: centerTheme.fg.primary
                Accessible.description: I18n.qtr("album")
            }

            Widgets.MenuLabel {
                id: artistLabel

                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: VLCStyle.margin_small

                BindingCompat on visible {
                    delayed: true
                    value: centerContent.height > (artistLabel.y + artistLabel.height)
                }

                text: mainPlaylistController.currentItem.artist
                font.weight: Font.Light
                horizontalAlignment: Text.AlignHCenter
                color: centerTheme.fg.primary
                Accessible.description: I18n.qtr("artist")
            }

            Widgets.NavigableRow {
                id: audioControls

                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: VLCStyle.margin_large

                BindingCompat on visible {
                    delayed: true
                    value: Player.videoTracks.count === 0 && centerContent.height > (audioControls.y + audioControls.height)
                }

                focus: visible
                spacing: VLCStyle.margin_xxsmall
                Navigation.parentItem: rootPlayer
                Navigation.upItem: topcontrolView
                Navigation.downItem: controlBarView

                model: ObjectModel {
                    Widgets.IconToolButton {
                        iconText: VLCIcons.skip_back
                        size: VLCStyle.icon_audioPlayerButton
                        onClicked: Player.jumpBwd()
                        text: I18n.qtr("Step back")
                    }

                    Widgets.IconToolButton {
                        iconText: VLCIcons.visualization
                        size: VLCStyle.icon_audioPlayerButton
                        onClicked: Player.toggleVisualization()
                        text: I18n.qtr("Visualization")
                    }

                    Widgets.IconToolButton{
                        iconText: VLCIcons.skip_for
                        size: VLCStyle.icon_audioPlayerButton
                        onClicked: Player.jumpFwd()
                        text: I18n.qtr("Step forward")
                    }
                }
            }
        }
    }

    Widgets.DrawerExt {
        id: playlistpopup

        property bool showPlaylist: false

        anchors {
            // NOTE: When the controls are pinned we display the playqueue under the topBar.
            top: (rootPlayer.pinVideoControls) ? topcontrolView.bottom
                                               : parent.top
            right: parent.right
            bottom: parent.bottom

            bottomMargin: parent.height - rootPlayer.positionSliderY
        }

        focus: false
        edge: Widgets.DrawerExt.Edges.Right
        state: playlistVisibility.isPlaylistVisible ? "visible" : "hidden"
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

                anchors.fill: parent
                rightPadding: VLCStyle.applicationHorizontalMargin
                topPadding:  {
                    if (rootPlayer.pinVideoControls)
                        return VLCStyle.margin_normal
                    else
                        // NOTE: We increase the padding accordingly to avoid overlapping the TopBar.
                        return topcontrolView.contentItem.reservedHeight
                }

                Navigation.parentItem: rootPlayer
                Navigation.upItem: topcontrolView
                Navigation.downItem: controlBarView
                Navigation.leftAction: closePlaylist
                Navigation.cancelAction: closePlaylist

                function closePlaylist() {
                    playlistVisibility.togglePlaylistVisibility()
                    if (audioControls.visible)
                        audioControls.forceActiveFocus()
                    else
                        controlBarView.forceActiveFocus()
                }

                // TODO: remember width factor?
                Widgets.HorizontalResizeHandle {
                    id: resizeHandle

                    anchors {
                        top: parent.top
                        bottom: parent.bottom
                        left: parent.left
                    }

                    atRight: false
                    targetWidth: playlistpopup.width
                    sourceWidth: rootPlayer.width
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
            bottom: controlBarView.contentItem.visible ? controlBarView.top : rootPlayer.bottom
            left: parent.left
            right: parent.right
            bottomMargin: rootPlayer.pinVideoControls || !controlBarView.contentItem.visible ? 0 : - VLCStyle.margin_large
        }
    }

    Widgets.DrawerExt {
        id: controlBarView

        readonly property int sliderY: rootPlayer.pinVideoControls ? contentItem.sliderY - VLCStyle.margin_xxxsmall : contentItem.sliderY

        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }
        focus: true
        state: "visible"
        edge: Widgets.DrawerExt.Edges.Bottom

        onStateChanged: {
            if (state === "visible")
                contentItem.showChapterMarks()
        }

        component: MouseArea {
            id: controllerMouseArea

            readonly property alias sliderY: controllerId.sliderY

            height: controllerId.implicitHeight + controllerId.anchors.bottomMargin
            width: controlBarView.width
            hoverEnabled: true

            function showChapterMarks() {
                controllerId.showChapterMarks()
            }

            onContainsMouseChanged: rootPlayer.lockUnlockAutoHide(containsMouse)

            ControlBar {
                id: controllerId

                focus: true
                anchors.fill: parent
                anchors.leftMargin: VLCStyle.applicationHorizontalMargin
                anchors.rightMargin: VLCStyle.applicationHorizontalMargin
                anchors.bottomMargin: VLCStyle.applicationVerticalMargin

                textPosition: rootPlayer.pinVideoControls ? ControlBar.TimeTextPosition.LeftRightSlider : ControlBar.TimeTextPosition.AboveSlider

                Navigation.parentItem: rootPlayer
                Navigation.upItem: playlistpopup.showPlaylist ? playlistpopup : (audioControls.visible ? audioControls : topcontrolView)

                onRequestLockUnlockAutoHide: rootPlayer.lockUnlockAutoHide(lock)

                identifier: (Player.hasVideoOutput) ? PlayerControlbarModel.Videoplayer
                                                    : PlayerControlbarModel.Audioplayer
            }
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
                controlBarView.state = "visible"
                topcontrolView.state = "visible"
                if (!controlBarView.focus && !topcontrolView.focus)
                    controlBarView.forceActiveFocus()

                videoSurface.cursorShape = Qt.ArrowCursor
            }
            else
            {
                if (!rootPlayer._autoHide)
                    return;
                controlBarView.state = "hidden"
                topcontrolView.state = "hidden"
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
            setVisibleControlBar(controlBarView.state !== "visible")
            toolbarAutoHide.stop()
        }

    }

    //filter key events to keep toolbar
    //visible when user navigates within the control bar
    KeyEventFilter {
        id: filter
        target: MainCtx.intfMainWindow
        enabled: controlBarView.state === "visible"
                 && (controlBarView.focus || topcontrolView.focus)
        Keys.onPressed: toolbarAutoHide.setVisible(5000)
    }

    Connections {
        target: MainCtx
        onAskShow: {
            toolbarAutoHide.toggleForceVisible()
        }
    }
}
