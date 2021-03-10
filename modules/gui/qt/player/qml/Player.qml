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
import QtQuick.Layouts 1.3
import QtQml.Models 2.11
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///util/KeyHelper.js" as KeyHelper
import "qrc:///playlist/" as PL

Widgets.NavigableFocusScope {
    id: rootPlayer

    //menu/overlay to dismiss
    property var _menu: undefined
    property int _lockAutoHide: 0
    readonly property bool _autoHide: _lockAutoHide == 0
                                      && rootPlayer.hasEmbededVideo
                                      && player.hasVideoOutput
                                      && playlistpopup.state !== "visible"

    property bool pinVideoControls: rootPlayer.hasEmbededVideo && mainInterface.pinVideoControls
    property bool hasEmbededVideo: mainInterface.hasEmbededVideo
    readonly property int positionSliderY: controlBarView.y + controlBarView.sliderY
    readonly property string coverSource: (mainPlaylistController.currentItem.artwork && mainPlaylistController.currentItem.artwork.toString())
                                          ? mainPlaylistController.currentItem.artwork
                                          : VLCStyle.noArtCover
    readonly property VLCColors colors: (mainInterface.hasEmbededVideo || (coverLuminance.luminance < 140))
                                        ? VLCStyle.nightColors : VLCStyle.dayColors

    Keys.priority: Keys.AfterItem
    Keys.onPressed: {
        if (event.accepted)
            return
        defaultKeyAction(event, 0)

        //unhandled keys are forwarded as hotkeys
        if (!event.accepted || controlBarView.state !== "visible")
            mainInterface.sendHotkey(event.key, event.modifiers);
    }

    Keys.onReleased: {
        if (event.accepted)
            return
        if (event.key === Qt.Key_Menu) {
            toolbarAutoHide.toggleForceVisible()
        } else {
            defaultKeyReleaseAction(event, 0)
        }
    }

    navigationCancel: function() {
        if (rootPlayer.hasEmbededVideo && controlBarView.state === "visible") {
            toolbarAutoHide.setVisibleControlBar(false)
        } else {
            if (mainInterface.hasEmbededVideo && !mainInterface.canShowVideoPIP) {
               mainPlaylistController.stop()
            }
            history.previous()
        }
    }

    on_AutoHideChanged: {
        if (_autoHide)
            toolbarAutoHide.restart()
    }

    onPinVideoControlsChanged: {
        lockUnlockAutoHide(pinVideoControls, "pinVideoControl")
        if (pinVideoControls)
            toolbarAutoHide.setVisibleControlBar(true)
    }

    function dismiss() {
        if (_menu)
            _menu.dismiss()
    }

    function lockUnlockAutoHide(lock, source /*unused*/) {
        _lockAutoHide += lock ? 1 : -1;
        console.assert(_lockAutoHide >= 0)
    }

    ImageLuminanceExtractor {
        id: coverLuminance

        enabled: !rootPlayer.hasEmbededVideo
        source: rootPlayer.coverSource
    }

    VideoSurface {
        id: videoSurface

        ctx: mainctx
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
        color: rootPlayer.colors.bg
        anchors.fill: parent

        Item {
            //destination aspect ration
            readonly property real dar: parent.width / parent.height

            anchors.centerIn: parent
            width: (cover.sar < dar) ? parent.width :  parent.height * cover.sar
            height: (cover.sar < dar) ? parent.width / cover.sar :  parent.height

            GaussianBlur {
                id: blur

                anchors.fill: parent
                source: cover
                samples: 102
                radius: 50
                visible: false
            }

            Rectangle {
                id: blurOverlay

                color: rootPlayer.colors.setColorAlpha(rootPlayer.colors.playerBg, .55)
                anchors.fill: parent
                visible: false
            }

            Blend {
                id:screen

                anchors.fill: parent
                foregroundSource: blurOverlay
                source: blur
                mode: "screen"
                visible: false
            }

            Blend {
                anchors.fill: parent
                source: screen
                foregroundSource: blurOverlay
                mode: "multiply"
            }

            Rectangle {
                id: colorOverlay

                anchors.fill: parent
                visible: true
                opacity: .4
                color: rootPlayer.colors.setColorAlpha(Qt.tint(rootPlayer.colors.playerFg, rootPlayer.colors.playerBg), 1)
            }
        }
    }

    /// Backgrounds of topControlbar and controlBar are drawn separately since they can outgrow their content
    Component {
        id: backgroundForPinnedControls

        Rectangle {
            width: rootPlayer.width
            color: rootPlayer.colors.playerBg
        }
    }

    /* top control bar background */
    Widgets.DrawerExt {
        edge: Widgets.DrawerExt.Edges.Top
        state: topcontrolView.state
        width: parent.width
        visible: rootPlayer.hasEmbededVideo || rootPlayer.pinVideoControls
        height: contentItem.height
        component: rootPlayer.pinVideoControls ? backgroundForPinnedControls : topcontrolViewBackground
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

            Item {
                width: controlBarView.width
                height: controlBarView.height - (rootPlayer.positionSliderY - controlBarView.y)

                Rectangle {
                    id: controlBarBackground

                    anchors.fill: parent
                    visible: false
                    color: rootPlayer.colors.isThemeDark
                           ? Qt.darker(rootPlayer.colors.playerBg, 1.2)
                           : Qt.lighter(rootPlayer.colors.playerBg, 1.2)
                }

                GaussianBlur {
                    anchors.fill: parent
                    source: controlBarBackground
                    radius: 22
                    samples: 46
                    opacity: .7
                }
            }
        }
    }

    Widgets.DrawerExt{
        id: topcontrolView

        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            topMargin: VLCStyle.applicationVerticalMargin
            leftMargin: VLCStyle.applicationHorizontalMargin
            rightMargin: VLCStyle.applicationHorizontalMargin
        }

        z: 1
        edge: Widgets.DrawerExt.Edges.Top
        state: "visible"

        component: FocusScope {
            width: topcontrolView.width
            height: topbar.implicitHeight
            focus: true

            TopBar {
                id: topbar

                anchors.fill: parent
                focus: true
                visible: !resumeDialog.visible
                title: mainPlaylistController.currentItem.title
                colors: rootPlayer.colors
                groupAlignment: rootPlayer.pinVideoControls ? TopBar.GroupAlignment.Horizontal : TopBar.GroupAlignment.Vertical
                navigationParent: rootPlayer
                navigationDownItem: playlistpopup.showPlaylist ? playlistpopup : (audioControls.visible ? audioControls : controlBarView)

                onTooglePlaylistVisibility: {
                    if (mainInterface.playlistDocked)
                        playlistpopup.showPlaylist = !playlistpopup.showPlaylist
                    else
                        mainInterface.playlistVisible = !mainInterface.playlistVisible
                }

                onRequestLockUnlockAutoHide: {
                    rootPlayer.lockUnlockAutoHide(lock, source)
                }
            }

            ResumeDialog {
                id: resumeDialog

                anchors.fill: parent
                colors: rootPlayer.colors
                navigationParent: rootPlayer

                onHidden: {
                    if (activeFocus) {
                        topbar.focus = true
                        controlBarView.forceActiveFocus()
                    }
                }

                onVisibleChanged: {
                    rootPlayer.lockUnlockAutoHide(visible, resumeDialog)
                }
            }
        }
    }

    Item {
        id: centerContent

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
                Layout.preferredHeight: rootPlayer.height / 2.7182
                Layout.preferredWidth: height * cover.sar
                Layout.maximumHeight: centerContent.height
                Layout.alignment: Qt.AlignHCenter

                Image {
                    id: cover

                    //source aspect ratio
                    readonly property real sar: cover.sourceSize.width / cover.sourceSize.height

                    anchors.fill: parent
                    source: rootPlayer.coverSource
                    fillMode: Image.PreserveAspectFit
                    mipmap: height < sourceSize.height * .3
                }

                Widgets.CoverShadow {
                    anchors.fill: cover
                    source: cover
                    primaryVerticalOffset: VLCStyle.dp(24)
                    primaryRadius: VLCStyle.dp(54)
                    secondaryVerticalOffset: VLCStyle.dp(5)
                    secondaryRadius: VLCStyle.dp(14)
                }
            }

            Widgets.SubtitleLabel {
                id: albumLabel

                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: VLCStyle.margin_xxlarge

                visible: centerContent.height > (albumLabel.y + albumLabel.height)
                text: mainPlaylistController.currentItem.album
                font.pixelSize: VLCStyle.fontSize_xxlarge
                horizontalAlignment: Text.AlignHCenter
                color: rootPlayer.colors.playerFg
                Accessible.description: i18n.qtr("album")
            }

            Widgets.MenuLabel {
                id: artistLabel

                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: VLCStyle.margin_small

                visible: centerContent.height > (artistLabel.y + artistLabel.height)
                text: mainPlaylistController.currentItem.artist
                font.weight: Font.Light
                horizontalAlignment: Text.AlignHCenter
                color: rootPlayer.colors.playerFg
                Accessible.description: i18n.qtr("artist")
            }

            Widgets.NavigableRow {
                id: audioControls

                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: VLCStyle.margin_large

                visible: player.videoTracks.count === 0 && centerContent.height > (audioControls.y + audioControls.height)
                focus: visible
                spacing: VLCStyle.margin_xxsmall
                navigationParent: rootPlayer
                KeyNavigation.up: topcontrolView
                KeyNavigation.down: controlBarView

                model: ObjectModel {
                    Widgets.IconToolButton {
                        size: VLCIcons.pixelSize(VLCStyle.icon_large)
                        iconText: VLCIcons.skip_back
                        onClicked: player.jumpBwd()
                        text: i18n.qtr("Step back")
                        color: rootPlayer.colors.playerFg
                    }

                    Widgets.IconToolButton {
                        size: VLCIcons.pixelSize(VLCStyle.icon_large)
                        iconText: VLCIcons.visualization
                        onClicked: player.toggleVisualization()
                        text: i18n.qtr("Visualization")
                        color: rootPlayer.colors.playerFg
                    }

                    Widgets.IconToolButton{
                        size: VLCIcons.pixelSize(VLCStyle.icon_large)
                        iconText: VLCIcons.skip_for
                        onClicked: player.jumpFwd()
                        text: i18n.qtr("Step forward")
                        color: rootPlayer.colors.playerFg
                    }
                }
            }
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

        component: MouseArea {
            id: controllerMouseArea

            readonly property alias sliderY: controllerId.sliderY

            height: controllerId.implicitHeight + controllerId.anchors.bottomMargin
            width: controlBarView.width
            hoverEnabled: true

            onContainsMouseChanged: rootPlayer.lockUnlockAutoHide(containsMouse, topcontrolView)

            ControlBar {
                id: controllerId
                focus: true
                anchors.fill: parent
                anchors.leftMargin: VLCStyle.applicationHorizontalMargin
                anchors.rightMargin: VLCStyle.applicationHorizontalMargin
                anchors.bottomMargin: VLCStyle.applicationVerticalMargin
                colors: rootPlayer.colors
                textPosition: rootPlayer.pinVideoControls ? ControlBar.TimeTextPosition.LeftRightSlider : ControlBar.TimeTextPosition.AboveSlider
                navigationParent: rootPlayer
                navigationUpItem: playlistpopup.showPlaylist ? playlistpopup : (audioControls.visible ? audioControls : topcontrolView)

                onRequestLockUnlockAutoHide: rootPlayer.lockUnlockAutoHide(lock, source)
            }
        }
    }

    Widgets.DrawerExt {
        id: playlistpopup

        property bool showPlaylist: false

        anchors {
            top: parent.top
            right: parent.right
            bottom: parent.bottom
            bottomMargin: parent.height - rootPlayer.positionSliderY
        }
        focus: false
        edge: Widgets.DrawerExt.Edges.Right
        state: showPlaylist && mainInterface.playlistDocked ? "visible" : "hidden"
        component: Rectangle {
            color: rootPlayer.colors.setColorAlpha(rootPlayer.colors.banner, 0.8)
            width: rootPlayer.width/4
            height: playlistpopup.height

            PL.PlaylistListView {
                id: playlistView
                focus: true
                anchors.fill: parent

                colors: rootPlayer.colors
                navigationParent: rootPlayer
                navigationUpItem: topcontrolView
                navigationDownItem: controlBarView
                navigationLeft: closePlaylist
                navigationCancel: closePlaylist

                function closePlaylist() {
                    playlistpopup.showPlaylist = false
                    if (audioControls.visible)
                        audioControls.forceActiveFocus()
                    else
                        controlBarView.forceActiveFocus()
                }
            }
        }
        onStateChanged: {
            if (state === "hidden")
                toolbarAutoHide.restart()
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

    //filter global events to keep toolbar
    //visible when user navigates within the control bar
    EventFilter {
        id: filter
        source: topWindow
        filterEnabled: controlBarView.state === "visible"
                       && (controlBarView.focus || topcontrolView.focus)
        Keys.onPressed: toolbarAutoHide.setVisible(5000)
    }

    Connections {
        target: mainInterface
        onAskShow: {
            toolbarAutoHide.toggleForceVisible()
        }
    }
}
