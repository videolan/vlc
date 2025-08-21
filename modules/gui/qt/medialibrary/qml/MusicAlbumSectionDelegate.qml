/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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
import QtQuick.Layouts
import QtQuick.Templates as T

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util
import VLC.MediaLibrary

T.Pane {
    id: root

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    verticalPadding: VLCStyle.margin_small
    horizontalPadding: VLCStyle.margin_normal

    spacing: VLCStyle.margin_xsmall

    required property string section

    required property MLAlbumModel model

    property bool retainWhileLoading: true // akin to `Image::retainWhileLoading`

    property bool previousSectionButtonEnabled: true
    property bool nextSectionButtonEnabled: true

    // For preventing initial twitching, and to prevent loading fallback just to discard it immediately right after:
    property bool _initialFetchCompleted: false

    property var _albumData
    readonly property url _albumCover: _initialFetchCompleted ? ((_albumData?.cover && (_albumData.cover !== "")) ? _albumData.cover
                                                                                                                  : VLCStyle.noArtAlbumCover)
                                                              : ""

    signal changeToNextSectionRequested()
    signal changeToPreviousSectionRequested()

    readonly property ColorContext theme: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    Component.onCompleted: {
        Qt.callLater(root.fetchAlbumData)
    }

    onModelChanged: {
        Qt.callLater(root.fetchAlbumData)
    }

    onSectionChanged: {
        Qt.callLater(root.fetchAlbumData)
    }

    Connections {
        target: root.model

        function onLoadingChanged() {
            Qt.callLater(root.fetchAlbumData)
        }

        function onLayoutChanged() {
            Qt.callLater(root.fetchAlbumData)
        }

        function onDataChanged() {
            Qt.callLater(root.fetchAlbumData)
        }

        function onModelReset() {
            Qt.callLater(root.fetchAlbumData)
        }
    }

    function fetchAlbumData() {
        if (!root.model)
            return

        if (root.model.loading)
            return

        if (section.length === 0)
            return

        if (!root.retainWhileLoading)
            root._albumData = null

        const mlItem = MediaLib.deserializeMlItemIdFromString(section)
        console.assert(mlItem.isValid())

        root.model.getDataById(mlItem).then((albumData, taskId) => {
            root._albumData = albumData
            root._initialFetchCompleted = true
        })
    }

    background: Rectangle {
        visible: root._initialFetchCompleted

        // NOTE: Transparent rectangle has an optimization that it does not use a scene graph node.
        color: blurEffect.visible ? "transparent" : (theme.palette?.isDark ? "black" : "white")

        // NOTE: `FrostedGlassEffect` is purposefully not used here. It should be only used when depth
        //       is relevant and exposed to the user, such as for popups, or the mini player (mini
        //       player is placed on top of the main view and naturally gives that impression to the
        //       user). Here, one way to provide that would be having parallax scrolling, but for now
        //       it is not used.
        Widgets.DualKawaseBlur {
            id: blurEffect

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            height: source ? ((source.implicitHeight / source.implicitWidth) * width) : implicitHeight

            // Instead of clipping in the parent, denote the viewport here so we both
            // do not need to clip the excess, and also save significant video memory:
            viewportRect: Qt.rect((width - parent.width) / 2, (height - parent.height) / 2,
                                  parent.width, parent.height)

            mode: Widgets.DualKawaseBlur.TwoPass
            radius: 1

            // Sections are re-used, but they may not release GPU resources immediately.
            // This ensures resources are freed to limit peak VRAM consumption.
            source: visible ? root.contentItem.artworkTextureProvider : null

            visible: (GraphicsInfo.shaderType === GraphicsInfo.RhiShader) &&
                     !!root.contentItem?.artworkTextureProvider

            postprocess: true
            tint: root.theme.palette?.isDark ? "black" : "white"
            tintStrength: 0.7
            backgroundColor: theme.bg.secondary
        }
    }

    component TitleLabel: Widgets.SubtitleLabel {
        required property MusicAlbumSectionDelegate delegate

        font.pixelSize: VLCStyle.fontSize_xxlarge

        text: delegate._initialFetchCompleted ? (delegate._albumData?.title || qsTr("Unknown title"))
                                              : " " // to get the implicit height during layouting at initialization
        color: theme.fg.primary
    }

    component CaptionLabel: Widgets.CaptionLabel {
        required property MusicAlbumSectionDelegate delegate

        text: {
            if (!delegate._initialFetchCompleted)
                return " " // to get the implicit height during layouting at initialization

            const _albumData = delegate._albumData
            if (!_albumData)
                return ""

            const parts = []

            parts.push(_albumData.main_artist || qsTr("Unknown artist"))

            const year = _albumData.release_year
            if (year)
                parts.push(year)

            const count = _albumData.nb_tracks ?? 0
            parts.push(qsTr("%1 track(s)").arg(count))

            const duration = _albumData.duration?.formatHMS()
            if (duration)
                parts.push(duration)

            return parts.join(" • ")
        }

        visible: (text.length > 0)

        color: theme.fg.secondary
    }

    component PlayButton: Widgets.ActionButtonPrimary {
        required property MusicAlbumSectionDelegate delegate

        iconTxt: VLCIcons.play
        text: qsTr("Play")
        enabled: !!delegate?._albumData?.id
        visible: delegate._initialFetchCompleted

        onClicked: {
            MediaLib.addAndPlay(delegate._albumData.id)
        }
    }

    component EnqueueButton: Widgets.ActionButtonOverlay {
        required property MusicAlbumSectionDelegate delegate

        iconTxt: VLCIcons.enqueue
        text: qsTr("Enqueue")
        enabled: !!delegate?._albumData?.id
        visible: delegate._initialFetchCompleted

        onClicked: {
            MediaLib.addToPlaylist(delegate._albumData.id)
        }
    }

    component PreviousSectionButton: Widgets.ActionButtonOverlay {
        required property MusicAlbumSectionDelegate delegate

        iconTxt: VLCIcons.chevron_up
        text: qsTr("Prev")
        enabled: delegate.previousSectionButtonEnabled

        Component.onCompleted: {
            clicked.connect(delegate, delegate.changeToPreviousSectionRequested)
        }
    }

    component NextSectionButton: Widgets.ActionButtonOverlay {
        required property MusicAlbumSectionDelegate delegate

        iconTxt: VLCIcons.chevron_down
        text: qsTr("Next")
        enabled: delegate.nextSectionButtonEnabled

        Component.onCompleted: {
            clicked.connect(delegate, delegate.changeToNextSectionRequested)
        }
    }

    contentItem: RowLayout {
        id: _contentItem

        spacing: root.spacing

        readonly property Item artworkTextureProvider: (artwork.status === Image.Ready) ? artwork.textureProviderItem
                                                                                        : null

        readonly property bool compactDownButtons: (_contentItem.width < VLCStyle.colWidth(3))
        readonly property bool compactRightButtons: (_contentItem.width < VLCStyle.colWidth(5))

        Widgets.ImageExt {
            id: artwork

            Layout.fillHeight: true
            Layout.preferredHeight: VLCStyle.cover_small
            Layout.preferredWidth: VLCStyle.cover_small

            radius: VLCStyle.expandCover_music_radius

            source: root._albumCover

            backgroundColor: theme.bg.primary

            // There are many sections, we need to be conservative regarding the source
            // size to limit average video and system memory consumption. The visual of
            // the image here is already expected to be (much) lower than the source
            // size, but it is for using the same source for the blur effect as texture
            // provider instead of having another image. There, the visual is bigger
            // than the source size, but since we are using blur effect, the result is
            // acceptable. Imperfections of low quality blurring due to linear upscaling
            // in the source is tolerated by two-pass dual kawase blur, which is good
            // but ends up having stronger blurring than what we desire. Still, consuming
            // less resources is considered more important, at least for now:
            sourceSize: Qt.size(Helpers.alignUp(Screen.desktopAvailableWidth / 8, 32), 0)

            cache: false

            asynchronous: true

            Widgets.DefaultShadow {
                visible: (artwork.status === Image.Ready)
            }
        }

        Column {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter

            spacing: VLCStyle.margin_xsmall

            Column {
                anchors.left: parent.left
                anchors.right: parent.right

                spacing: 0

                TitleLabel {
                    delegate: root

                    anchors.left: parent.left
                    anchors.right: parent.right
                }

                CaptionLabel {
                    delegate: root

                    anchors.left: parent.left
                    anchors.right: parent.right
                }
            }

            Row {
                anchors.left: parent.left
                anchors.right: parent.right

                spacing: VLCStyle.margin_small

                PlayButton {
                    id: playButton

                    delegate: root

                    focus: true
                    activeFocusOnTab: false

                    showText: !_contentItem.compactDownButtons

                    Navigation.parentItem: root
                    Navigation.rightItem: enqueueButton
                }

                EnqueueButton {
                    id: enqueueButton

                    delegate: root

                    activeFocusOnTab: false

                    showText: !_contentItem.compactDownButtons

                    Navigation.parentItem: root
                    Navigation.rightItem: previousSectionButton.enabled ? previousSectionButton
                                                                        : nextSectionButton
                    Navigation.leftItem: playButton
                }
            }
        }

        Column {
            Layout.alignment: Qt.AlignVCenter

            spacing: VLCStyle.margin_small

            PreviousSectionButton {
                id: previousSectionButton

                delegate: root

                activeFocusOnTab: false

                showText: !_contentItem.compactRightButtons

                Navigation.parentItem: root
                Navigation.downItem: nextSectionButton
                Navigation.leftItem: enqueueButton
            }

            NextSectionButton {
                id: nextSectionButton

                delegate: root

                activeFocusOnTab: false

                showText: !_contentItem.compactRightButtons

                Navigation.parentItem: root
                Navigation.upItem: previousSectionButton
                Navigation.leftItem: enqueueButton
            }
        }
    }
}
