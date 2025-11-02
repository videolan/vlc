
/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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
import QtQuick.Templates as T
import QtQuick.Layouts
import QtQml.Models


import VLC.MainInterface
import VLC.Style
import VLC.Player
import VLC.Widgets as Widgets
import VLC.Util
import VLC.Dialogs

RowLayout {
    id: root

    readonly property ColorContext colorContext: ColorContext {
        id: theme

        colorSet: ColorContext.View
    }

    //FIXME make TrackMenuController a proper type (see TrackMenu)
    required property QtObject trackMenuController

    spacing: 0

    focus: true

    onActiveFocusChanged: if (activeFocus) playbackBtn.forceActiveFocus()

    Widgets.ButtonExt {
        id: playbackBtn

        text: qsTr("%1x").arg(+Player.rate.toFixed(2))

        onPressed: {
            trackMenuController.requestPlaybackSpeedPage()
        }

        T.ToolTip.visible: (hovered || visualFocus)
        T.ToolTip.text: qsTr("Playback Speed")
        T.ToolTip.delay: VLCStyle.delayToolTipAppear

        Navigation.parentItem: root
        Navigation.rightItem: row

        Layout.alignment: Qt.AlignTop | Qt.AlignLeft
        Layout.margins: VLCStyle.margin_large
    }

    Widgets.NavigableRow {
        id: row

        Layout.fillHeight: true
        Layout.fillWidth: true

        Navigation.leftItem: playbackBtn

        TrackColumn {
            title: qsTr("Subtitle")
            tracksModel: Player.subtitleTracks
            menuIcon: VLCIcons.expand
            menuText: qsTr("Menu")
            onMenuAction: (menuPos)  => {
                menuSubtitle.popup(menuPos)
            }
        }

        TrackColumn {
            title: qsTr("Audio")
            tracksModel: Player.audioTracks
            menuIcon: VLCIcons.expand
            menuText: qsTr("Menu")
            onMenuAction: (menuPos)  => {
                menuAudio.popup(menuPos)
            }
        }

        TrackColumn {
            title: qsTr("Video Tracks")
            tracksModel: Player.videoTracks
            menuIcon: VLCIcons.add
            menuText: qsTr("Add")
            onMenuAction: (menuPos) => {
                DialogsProvider.loadVideoFile()
            }
        }
    }

    component TrackColumn: Container {
        // wrap the contentItem i.e Column into Container
        // so that we can get focusReason, also Container
        // is a FocusScope
        id: tracksListContainer


        required property string title
        required property var tracksModel
        required property string menuIcon
        required property string menuText

        signal menuAction(point position)

        focus: true

        width: row.width / 3
        height: row.height

        onActiveFocusChanged: if (activeFocus) tracksList.forceActiveFocus(focusReason)

        // this is required to initialize attached Navigation property
        Navigation.parentItem: row

        contentItem: Column {
            anchors.fill: parent

            focus: true

            Accessible.role: Accessible.Pane
            Accessible.name: tracksListContainer.title

            Item {
                // keep it inside so "Column" doesn't mess with it
               Rectangle {
                   id: separator

                   x: 0
                   y: 0
                   width: VLCStyle.margin_xxxsmall

                   height: tracksListContainer.height
                   color: theme.border
               }
            }

            Row {
                id: titleHeader

                width: tracksListContainer.width
                height: implicitHeight

                padding: VLCStyle.margin_xsmall

                topPadding: VLCStyle.margin_large
                leftPadding: VLCStyle.margin_xxlarge + separator.width

                focus: true

                clip: true

                Widgets.SubtitleLabel {
                    id: titleText

                    width: parent.width - button.width - parent.leftPadding
                           - parent.rightPadding

                    text: tracksListContainer.title
                    color: theme.fg.primary
                }

                Widgets.IconTrackButton {
                    id: button

                    font.pixelSize: VLCStyle.icon_track

                    focus: true

                    description: tracksListContainer.menuText
                    text: tracksListContainer.menuIcon

                    Navigation.parentItem: tracksListContainer
                    Navigation.downItem: tracksList

                    onClicked: {
                        tracksListContainer.menuAction(mapToGlobal(0, height))
                    }
                }
            }

            Widgets.ListViewExt {
                id: tracksList

                model: tracksListContainer.tracksModel
                width: tracksListContainer.width
                height: tracksListContainer.height - titleHeader.height
                leftMargin: separator.width
                focus: true
                clip: true

                fadingEdge.backgroundColor: theme.bg.primary

                Accessible.role: Accessible.List
                Accessible.name: qsTr("Track list")

                Navigation.parentItem: tracksListContainer
                Navigation.upItem: button
                Keys.priority: Keys.AfterItem
                Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

                delegate: Widgets.CheckedDelegate {
                    readonly property bool isModelChecked: model.checked
                    clip: true

                    focus: true
                    text: model.display
                    width: tracksListContainer.width - VLCStyle.margin_xxxsmall
                    height: VLCStyle.dp(40, VLCStyle.scale)
                    opacity: hovered || activeFocus || checked ? 1 : .6
                    font.weight: hovered
                                 || activeFocus ? Font.DemiBold : Font.Normal

                    onIsModelCheckedChanged: {
                        if (model.checked !== checked)
                            checked = model.checked
                    }

                    onCheckedChanged: {
                        if (model.checked !== checked)
                            model.checked = checked
                    }

                    onClicked: {
                        tracksList.currentIndex = index
                        tracksList.setCurrentItemFocus(Qt.MouseFocusReason)
                    }

                    Navigation.parentItem: tracksList
                }
            }
        }
    }

    QmlSubtitleMenu {
        id: menuSubtitle

        player: Player
        ctx: MainCtx

        onTriggered: {
            if (action === QmlSubtitleMenu.Open) {
                DialogsProvider.loadSubtitlesFile()
            }
            else if (action === QmlSubtitleMenu.Synchronize) {
                trackMenuController.requestSubtitlePage()
            }
            else if (action === QmlSubtitleMenu.Download) {
                Player.openVLsub()
            }
        }
    }

    QmlAudioMenu {
        id: menuAudio

        ctx: MainCtx

        onTriggered: {
            if (action === QmlSubtitleMenu.Open) {
                DialogsProvider.loadAudioFile()
            }
            else if (action === QmlSubtitleMenu.Synchronize) {
                trackMenuController.requestAudioPage()
            }
        }
    }
}
