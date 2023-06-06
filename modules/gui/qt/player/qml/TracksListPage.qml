
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
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Templates 2.12 as T
import QtQuick.Layouts 1.12
import QtQml.Models 2.12

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///util/" as Util


RowLayout {
    id: root

    readonly property ColorContext colorContext: ColorContext {
        id: theme

        colorSet: ColorContext.Window // copied from TracksPage, maybe use Pane?
    }

    /* required */ property var trackMenuController: null

    spacing: 0

    focus: true

    onActiveFocusChanged: if (activeFocus) column.forceActiveFocus()

    Widgets.NavigableCol {
        id: column

        focus: true

        Layout.preferredWidth: VLCStyle.dp(72, VLCStyle.scale)
        Layout.alignment: Qt.AlignTop | Qt.AlignLeft
        Layout.topMargin: VLCStyle.margin_large

        Navigation.rightItem: row

        //we store the model in a different property as functions can't be passed in modelData
        property var modelDefination: [{
            "tooltip": I18n.qtr("Playback Speed"),
            "action": function () {
                trackMenuController.requestPlaybackSpeedPage()
            }
        }]

        model: modelDefination

        delegate: Widgets.IconTrackButton {
            size: (index === 0) ? VLCStyle.fontSize_large
                                : VLCStyle.dp(40, VLCStyle.scale)

            x: (column.width - width) / 2

            text: I18n.qtr("Playback Speed")
            iconText: (index === 0) ? I18n.qtr("%1x").arg(+Player.rate.toFixed(2))
                                    : modelData.icon

            T.ToolTip.visible: (hovered || visualFocus)
            T.ToolTip.text: modelData.tooltip
            T.ToolTip.delay: VLCStyle.delayToolTipAppear

            Navigation.parentItem: column

            onClicked: column.modelDefination[index].action()
        }
    }

    Widgets.NavigableRow {
        id: row

        Layout.fillHeight: true
        Layout.fillWidth: true

        Navigation.leftItem: column

        //we store the model in a different property as functions can't be passed in modelData
        property var modelDefinition: [{
                "title": I18n.qtr("Subtitle"),
                "tracksModel": Player.subtitleTracks,
                "menuIcon": VLCIcons.expand,
                "menuText": I18n.qtr("Menu"),
                "menuAction": function(menuPos) {
                    menuSubtitle.popup(menuPos)
                },

            }, {
                "title": I18n.qtr("Audio"),
                "tracksModel": Player.audioTracks,
                "menuIcon": VLCIcons.expand,
                "menuText": I18n.qtr("Menu"),
                "menuAction": function(menuPos) {
                    menuAudio.popup(menuPos)
                }
            }, {
                "title": I18n.qtr("Video Tracks"),
                "tracksModel": Player.videoTracks,
                "menuIcon": VLCIcons.add,
                "menuText": I18n.qtr("Add"),
                "menuAction": function(menuPos) {
                    DialogsProvider.loadVideoFile()
                },
            }]

        //note that parenthesis around functions are *mandatory*
        model: modelDefinition

        delegate: Column {
            id: tracksListContainer

            property var tracksModel: modelData.tracksModel

            width: row.width / 3
            height: row.height

            focus: true

            Accessible.role: Accessible.Pane
            Accessible.name: modelData.title

            onActiveFocusChanged: if (activeFocus) tracksList.forceActiveFocus(focusReason)

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

                    text: modelData.title
                    color: theme.fg.primary
                }

                Widgets.IconTrackButton {
                    id: button

                    size: VLCStyle.icon_track

                    focus: true

                    text: modelData.menuText
                    iconText: modelData.menuIcon

                    Navigation.parentItem: tracksListContainer
                    Navigation.downItem: tracksList

                    onClicked: {
                        //functions aren't passed to modelData
                        row.modelDefinition[index].menuAction(mapToGlobal(0, height))
                    }
                }
            }

            ListView {
                id: tracksList

                model: tracksListContainer.tracksModel
                width: tracksListContainer.width
                height: tracksListContainer.height - titleHeader.height
                leftMargin: separator.width
                focus: true
                clip: true

                Accessible.role: Accessible.List
                Accessible.name: I18n.qtr("Track list")

                Navigation.parentItem: tracksListContainer
                Navigation.upItem: button
                Keys.priority: Keys.AfterItem
                Keys.onPressed: Navigation.defaultKeyAction(event)

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
                }
            }
        }
    }

    QmlSubtitleMenu {
        id: menuSubtitle

        player: Player

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
