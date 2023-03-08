
/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import QtQuick.Templates 2.4 as T
import QtQuick.Layouts 1.11
import QtQml.Models 2.11

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///util/" as Util

// FIXME: Keyboard navigation needs to be fixed for this Popup.
T.Popup {
    id: root

    // Settings

    height: VLCStyle.dp(296, VLCStyle.scale)
    width: rootPlayer.width

    // Popup.CloseOnPressOutside doesn't work with non-model Popup on Qt < 5.15
    closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape
    modal: true

    // Animations

    Behavior on width {
        SmoothedAnimation {
            duration: VLCStyle.duration_veryShort
            easing.type: Easing.InOutSine
        }
    }

    // Children

    readonly property ColorContext colorContext: ColorContext {
        id: popupTheme
        colorSet: ColorContext.Window
    }

    T.Overlay.modal: null

    background: Rectangle {
        opacity: 0.8
        color: popupTheme.bg.primary
    }

    contentItem: StackView {
        focus: true
        clip: true

        initialItem: frontPage

        //erf, popup are weird, content is not parented to the root
        //so, duplicate the context here for the childrens
        readonly property ColorContext colorContext: ColorContext {
            id: theme
            colorSet: popupTheme.colorSet
            palette: popupTheme.palette
        }

        onCurrentItemChanged: currentItem.forceActiveFocus()

        pushEnter: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: VLCStyle.duration_long
            }
        }
        pushExit: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: VLCStyle.duration_long
            }
        }
        popEnter: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: VLCStyle.duration_long
            }
        }
        popExit: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: VLCStyle.duration_long
            }
        }
    }

    Component {
        id: frontPage

        RowLayout {
            id: frontRoot

            property var currentItem: StackView.view.currentItem

            spacing: 0

            focus: true

            onActiveFocusChanged: if (activeFocus) column.forceActiveFocus()

            Connections {
                target: frontRoot.StackView.view

                onCurrentItemChanged: {
                    if (currentItem instanceof TracksPage)
                        root.width = Qt.binding(function () {
                            return Math.min(currentItem.preferredWidth, rootPlayer.width)
                        })
                    else
                        root.width = Qt.binding(function () { return rootPlayer.width })
                }
            }

            Connections {
                target: (currentItem && currentItem instanceof TracksPage) ? currentItem : null

                onBackRequested: frontRoot.StackView.view.pop()
            }

            Widgets.NavigableCol {
                id: column

                focus: true

                Layout.preferredWidth: VLCStyle.dp(72, VLCStyle.scale)
                Layout.alignment: Qt.AlignTop | Qt.AlignLeft
                Layout.topMargin: VLCStyle.margin_large

                Navigation.rightItem: row

                model: [{
                    "tooltip": I18n.qtr("Playback Speed"),
                    "source": "qrc:///player/TracksPageSpeed.qml"
                }]

                delegate: Widgets.IconTrackButton {
                    size: (index === 0) ? VLCStyle.fontSize_large
                                        : VLCStyle.dp(40, VLCStyle.scale)

                    x: (column.width - width) / 2

                    text: I18n.qtr("Playback Speed")
                    iconText: (index === 0) ? I18n.qtr("%1x").arg(+Player.rate.toFixed(2))
                                            : modelData.icon

                    T.ToolTip.visible: (hovered || activeFocus)
                    T.ToolTip.text: modelData.tooltip
                    T.ToolTip.delay: VLCStyle.delayToolTipAppear

                    Navigation.parentItem: column

                    onClicked: frontRoot.StackView.view.push(modelData.source)
                }
            }

            Widgets.NavigableRow {
                id: row

                Layout.fillHeight: true
                Layout.fillWidth: true

                Navigation.leftItem: column

                model: [{
                        "title": I18n.qtr("Subtitle"),
                        "tracksModel": Player.subtitleTracks
                    }, {
                        "title": I18n.qtr("Audio"),
                        "tracksModel": Player.audioTracks
                    }, {
                        "title": I18n.qtr("Video Tracks"),
                        "tracksModel": Player.videoTracks
                    }]

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

                            iconText: (index === 2) ? VLCIcons.add
                                                    : VLCIcons.expand

                            Navigation.parentItem: tracksListContainer
                            Navigation.downItem: tracksList

                            onClicked: {
                                switch (index) {
                                case 0:
                                    menuSubtitle.popup(mapToGlobal(0, height))
                                    break
                                case 1:
                                    menuAudio.popup(mapToGlobal(0, height))
                                    break
                                case 2:
                                    DialogsProvider.loadVideoFile()
                                    break
                                }
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
                contentItem.currentItem.StackView.view.push("qrc:///player/TracksPageSubtitle.qml")
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
                contentItem.currentItem.StackView.view.push("qrc:///player/TracksPageAudio.qml")
            }
        }
    }
}
