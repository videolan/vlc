/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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
import VLC.Player
import VLC.Widgets as Widgets
import VLC.Style

Flickable {
    id: lyricsFlickable

    property bool lyricsSyncToPlayback: true
    property bool animationsEnabled: true
    property bool fadingEdgeEnabled: true

    readonly property ColorContext colorContext: ColorContext {
        colorSet: ColorContext.View
    }

    implicitWidth: contentWidth
    implicitHeight: contentHeight

    contentWidth: lyricsColumn.width
    contentHeight: lyricsColumn.height

    interactive: height < implicitHeight

    ScrollBar.vertical: Widgets.ScrollBarExt {}

    boundsBehavior: Flickable.StopAtBounds
    clip: !fadingEdge.implicitClipping && (height < implicitHeight)

    // TODO: `vlcTime` type can not be used due to Qt 6.2:
    signal playerPositionChangeRequested(var time)

    function snapToCurrentLyric() {
        const idx = Player.currentLyricIndex
        if (idx < 0)
            return

        const item = lyricsRepeater.itemAt(idx)
        if (!item)
            return

        const targetY = item.y + item.height / 2 - lyricsFlickable.height / 2
        lyricsFlickable.contentY = Math.max(0, Math.min(
            targetY, lyricsFlickable.contentHeight - lyricsFlickable.height))
    }

    function scheduleSnapToCurrentLyric() {
        if (lyricsSyncToPlayback)
            Qt.callLater(lyricsFlickable.snapToCurrentLyric)
    }

    // Disable auto-scroll when user manually flicks
    onFlickStarted: lyricsSyncToPlayback = false
    onDragStarted: lyricsSyncToPlayback = false
    onLyricsSyncToPlaybackChanged: scheduleSnapToCurrentLyric()
    onHeightChanged: scheduleSnapToCurrentLyric()
    onContentHeightChanged: scheduleSnapToCurrentLyric()

    Component.onCompleted: scheduleSnapToCurrentLyric()

    Behavior on contentY {
        enabled: animationsEnabled && lyricsSyncToPlayback
        SmoothedAnimation {
            velocity: VLCStyle.dp(300, VLCStyle.scale)
            duration: VLCStyle.duration_veryLong
        }
    }

    Connections {
        target: Player
        enabled: lyricsSyncToPlayback
        function onCurrentLyricIndexChanged(idx) {
            lyricsFlickable.snapToCurrentLyric()
        }
    }

    Column {
        id: lyricsColumn

        width: lyricsFlickable.width

        Repeater {
            id: lyricsRepeater

            model: Player.syltLyrics

            delegate: Text {
                id: lyricDelegate

                required property int index
                // TODO: `timedText` type can not be used due to Qt 6.2:
                required property var modelData

                readonly property bool isCurrent: index === Player.currentLyricIndex

                anchors.left: parent.left
                anchors.right: parent.right
                height: (index === (lyricsRepeater.count - 1))
                    ? Math.max(implicitHeight, lyricsFlickable.height / 2)
                    : implicitHeight
                padding: VLCStyle.margin_small

                text: modelData.text

                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap

                // Use a single pixel size and rely on `scale` to differentiate
                // the current line; this is cheaper than re-rasterizing the
                // font on every animation frame.
                font.pixelSize: VLCStyle.fontSize_xlarge
                font.weight: isCurrent ? Font.Bold : Font.Normal

                color: lyricsFlickable.colorContext.fg.primary
                opacity: isCurrent ? 1.0 : 0.45
                scale: isCurrent ? 1.0 : (VLCStyle.fontSize_large / VLCStyle.fontSize_xlarge)

                Behavior on scale {
                    // Native text rendering does not handle scaled glyphs
                    // gracefully, so skip the animation in that case.
                    enabled: lyricsFlickable.animationsEnabled
                             && lyricDelegate.renderType !== Text.NativeRendering
                    NumberAnimation { duration: VLCStyle.duration_long }
                }
                Behavior on opacity {
                    enabled: lyricsFlickable.animationsEnabled
                    OpacityAnimator { duration: VLCStyle.duration_long }
                }

                TapHandler {
                    onTapped: {
                        lyricsFlickable.playerPositionChangeRequested(lyricDelegate.modelData.time)
                    }
                }
            }
        }
    }

    Widgets.FadingEdge {
        id: fadingEdge

        parent: lyricsFlickable

        anchors.fill: parent

        backgroundColor: "transparent"

        sourceItem: lyricsFlickable.contentItem

        sourceX: lyricsFlickable.contentX
        sourceY: lyricsFlickable.contentY

        orientation: Qt.Vertical

        // Scale the fade with the lyrics' default (non-current) line size,
        // similar to how list views derive it from delegate height.
        fadeSize: VLCStyle.fontSize_large / 2

        // `fadingEdgeEnabled` is the master toggle; the per-edge disabler
        // Bindings below override these when the view is scrolled to the
        // corresponding edge.
        enableBeginningFade: lyricsFlickable.fadingEdgeEnabled
        enableEndFade: lyricsFlickable.fadingEdgeEnabled

        Binding on enableBeginningFade {
            when: lyricsFlickable.atYBeginning
            value: false
        }

        Binding on enableEndFade {
            when: lyricsFlickable.atYEnd
            value: false
        }
    }
}