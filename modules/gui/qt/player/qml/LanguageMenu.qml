
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
import QtQuick.Layouts 1.3
import QtQml.Models 2.11

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///util/" as Util
import "qrc:///util/KeyHelper.js" as KeyHelper

T.Popup {
    id: control

    height: VLCStyle.dp(296, VLCStyle.scale)
    width: rootPlayer.width

    // Popup.CloseOnPressOutside doesn't work with non-model Popup on Qt < 5.15
    closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape
    modal: true
    T.Overlay.modal: Rectangle {
        color: "transparent"
    }

    contentItem: StackView {
        id: view

        initialItem: frontPage
        clip: true
        focus: true

        onCurrentItemChanged: currentItem.forceActiveFocus()

        pushEnter: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 200
            }
        }
        pushExit: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 200
            }
        }
        popEnter: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 200
            }
        }
        popExit: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 200
            }
        }
    }

    background: Rectangle {
        color: "#212121"
        opacity: .8
    }

    function _updateWidth(isFirstPage) {
        if (isFirstPage)
            control.width = Qt.binding(function () {
                return rootPlayer.width
            })
        else
            control.width = Qt.binding(function () {
                return Math.min(VLCStyle.dp(624, VLCStyle.scale),
                                rootPlayer.width)
            })
    }

    Behavior on width {
        SmoothedAnimation {
            duration: 64
            easing.type: Easing.InOutSine
        }
    }

    Component {
        id: frontPage

        RowLayout {
            id: frontPageRoot

            spacing: 0
            focus: true
            onActiveFocusChanged: if (activeFocus)
                                      btnsCol.forceActiveFocus()

            Widgets.NavigableCol {
                id: btnsCol

                focus: true
                Layout.preferredWidth: VLCStyle.dp(72, VLCStyle.scale)
                Layout.alignment: Qt.AlignTop | Qt.AlignLeft
                Layout.topMargin: VLCStyle.margin_large
                navigationRightItem: tracksListRow

                model: [{
                        "icon": VLCIcons.download,
                        "tooltip": i18n.qtr("Download Subtitles"),
                        "component": undefined
                    }, {
                        "icon": VLCIcons.time,
                        "tooltip": i18n.qtr("Delay"),
                        "component": delayPage
                    }, {
                        "icon": VLCIcons.sync,
                        "tooltip": i18n.qtr("Sync"),
                        "component": syncPage
                    }, {
                        "icon": VLCIcons.multiselect,
                        "tooltip": i18n.qtr("Select Multiple Subtitles"),
                        "component": undefined
                    }]

                delegate: Widgets.IconToolButton {
                    id: btn

                    iconText: modelData.icon
                    color: "white"
                    size: VLCStyle.dp(40, VLCStyle.scale)
                    x: (btnsCol.width - width) / 2
                    highlighted: index === 3
                                 && player.subtitleTracks.multiSelect

                    ToolTip.visible: btn.hovered || btn.activeFocus
                    ToolTip.text: modelData.tooltip
                    ToolTip.delay: 1000
                    ToolTip.toolTip.z: 2

                    onClicked: {
                        if (index === 0) {
                            player.openVLsub()
                        } else if (index === 3) {
                            player.subtitleTracks.multiSelect = !player.subtitleTracks.multiSelect
                            focus = false
                        } else {
                            control._updateWidth(false)
                            frontPageRoot.StackView.view.push(
                                        modelData.component)
                        }
                    }
                }
            }

            Widgets.NavigableRow {
                id: tracksListRow

                navigationLeftItem: btnsCol
                Layout.fillHeight: true
                Layout.fillWidth: true

                model: [{
                        "title": i18n.qtr("Subtitle"),
                        "tracksModel": player.subtitleTracks
                    }, {
                        "title": i18n.qtr("Audio"),
                        "tracksModel": player.audioTracks
                    }, {
                        "title": i18n.qtr("Video Tracks"),
                        "tracksModel": player.videoTracks
                    }]

                delegate: Column {
                    id: tracksListContainer

                    property var tracksModel: modelData.tracksModel

                    width: tracksListRow.width / 3
                    height: tracksListRow.height
                    focus: true

                    onActiveFocusChanged: if (activeFocus)
                                              tracksList.forceActiveFocus()

                    Item {
                        // keep it inside so "Column" doesn't mess with it
                       Rectangle {
                           id: separator

                           x: 0
                           y: 0
                           width: VLCStyle.dp(2, VLCStyle.scale)

                           height: tracksListContainer.height
                           color: "white"
                           opacity: .1
                       }
                    }

                    Row {
                        id: titleHeader

                        width: tracksListContainer.width
                        height: implicitHeight
                        focus: true

                        topPadding: VLCStyle.margin_large
                        leftPadding: VLCStyle.margin_xxlarge + separator.width
                        padding: VLCStyle.margin_xsmall
                        clip: true

                        Widgets.SubtitleLabel {
                            id: titleText

                            text: modelData.title
                            color: "white"
                            width: parent.width - addBtn.width
                                   - parent.leftPadding - parent.rightPadding
                        }

                        Widgets.IconToolButton {
                            id: addBtn

                            iconText: VLCIcons.add
                            size: VLCStyle.icon_normal
                            color: "white"
                            focus: true
                            onClicked: {
                                switch (index) {
                                case 0:
                                    dialogProvider.loadSubtitlesFile()
                                    break
                                case 1:
                                    dialogProvider.loadAudioFile()
                                    break
                                case 2:
                                    dialogProvider.loadVideoFile()
                                    break
                                }
                            }

                            KeyNavigation.down: tracksList
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

                        delegate: Widgets.CheckedDelegate {
                            readonly property bool isModelChecked: model.checked
                            clip: true

                            focus: true
                            text: model.display
                            width: tracksListContainer.width
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

                        KeyNavigation.up: addBtn
                    }
                }
            }
        }
    }

    Component {
        id: delayPage

        RowLayout {
            id: delayPageRoot

            spacing: 0
            focus: true
            onActiveFocusChanged: if (activeFocus)
                                      backBtn.forceActiveFocus()

            Item {
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.preferredWidth: VLCStyle.dp(72, VLCStyle.scale)
                Layout.topMargin: VLCStyle.margin_large
                Layout.fillHeight: true

                Widgets.IconToolButton {
                    id: backBtn

                    anchors.horizontalCenter: parent.horizontalCenter
                    size: VLCStyle.dp(36, VLCStyle.scale)
                    iconText: VLCIcons.back
                    color: "white"

                    onClicked: {
                        control._updateWidth(true)
                        delayPageRoot.StackView.view.pop()
                    }
                    KeyNavigation.right: audioDelaySpin
                }
            }

            Rectangle {
                Layout.preferredWidth: VLCStyle.dp(2, VLCStyle.scale)
                Layout.fillHeight: true
                color: "white"
                opacity: .1
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.leftMargin: VLCStyle.margin_xxlarge
                Layout.rightMargin: VLCStyle.margin_xxlarge
                Layout.topMargin: VLCStyle.margin_large
                spacing: VLCStyle.margin_xxsmall

                KeyNavigation.left: backBtn

                Widgets.SubtitleLabel {
                    Layout.fillWidth: true
                    text: i18n.qtr("Audio track synchronization")
                    color: "white"
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: VLCStyle.margin_xsmall

                    Widgets.MenuCaption {
                        text: i18n.qtr("Audio track delay")
                        color: "white"

                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Widgets.TransparentSpinBox {
                        id: audioDelaySpin

                        property bool inhibitUpdate: true

                        textFromValue: function (value, locale) {
                            return i18n.qtr("%1 ms").arg(
                                        Number(value).toLocaleString(locale,
                                                                     'f', 0))
                        }
                        valueFromText: function (text, locale) {
                            return Number.fromLocaleString(
                                        locale, text.substring(0,
                                                               text.length - 3))
                        }
                        stepSize: 50
                        from: -10000

                        Layout.preferredWidth: VLCStyle.dp(128, VLCStyle.scale)

                        onValueChanged: {
                            if (inhibitUpdate)
                                return
                            player.audioDelayMS = value
                        }

                        Component.onCompleted: {
                            value = player.audioDelayMS
                            inhibitUpdate = false
                        }

                        Connections {
                            target: player
                            onAudioDelayChanged: {
                                inhibitUpdate = true
                                value = player.audioDelayMS
                                inhibitUpdate = false
                            }
                        }

                        KeyNavigation.right: audioDelaySpinReset
                    }

                    Widgets.TabButtonExt {
                        id: audioDelaySpinReset

                        text: i18n.qtr("Reset")
                        color: "white"

                        onClicked: audioDelaySpin.value = 0
                        KeyNavigation.left: audioDelaySpin
                        KeyNavigation.right: primarySubSpin
                        KeyNavigation.down: primarySubSpinReset
                    }
                }

                Widgets.SubtitleLabel {
                    Layout.fillWidth: true
                    Layout.topMargin: VLCStyle.margin_large
                    text: i18n.qtr("Subtitle synchronization")
                    color: "white"
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: VLCStyle.margin_xsmall

                    Widgets.MenuCaption {
                        text: i18n.qtr("Primary subtitle delay")
                        color: "white"
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Widgets.TransparentSpinBox {
                        id: primarySubSpin

                        property bool inhibitUpdate: true

                        textFromValue: audioDelaySpin.textFromValue
                        valueFromText: audioDelaySpin.valueFromText
                        stepSize: 50
                        from: -10000

                        Layout.preferredWidth: VLCStyle.dp(128, VLCStyle.scale)

                        onValueChanged: {
                            if (inhibitUpdate)
                                return
                            player.subtitleDelayMS = value
                        }

                        Component.onCompleted: {
                            value = player.subtitleDelayMS
                            inhibitUpdate = false
                        }

                        Connections {
                            target: player
                            onSubtitleDelayChanged: {
                                inhibitUpdate = true
                                value = player.subtitleDelayMS
                                inhibitUpdate = false
                            }
                        }

                        KeyNavigation.right: primarySubSpinReset
                    }

                    Widgets.TabButtonExt {
                        id: primarySubSpinReset

                        text: i18n.qtr("Reset")
                        color: "white"
                        focus: true
                        onClicked: primarySubSpin.value = 0
                        KeyNavigation.left: primarySubSpin
                        KeyNavigation.right: secondarySubSpin
                        KeyNavigation.up: audioDelaySpinReset
                        KeyNavigation.down: secondarySubSpinReset
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: VLCStyle.margin_xsmall

                    Widgets.MenuCaption {
                        text: i18n.qtr("Secondary subtitle delay")
                        color: "white"
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Widgets.TransparentSpinBox {
                        id: secondarySubSpin

                        property bool inhibitUpdate: true

                        textFromValue: primarySubSpin.textFromValue
                        valueFromText: primarySubSpin.valueFromText
                        stepSize: 50
                        from: -10000

                        Layout.preferredWidth: VLCStyle.dp(128, VLCStyle.scale)

                        onValueChanged: {
                            if (inhibitUpdate)
                                return
                            player.secondarySubtitleDelayMS = value
                        }

                        Component.onCompleted: {
                            value = player.secondarySubtitleDelayMS
                            inhibitUpdate = false
                        }

                        Connections {
                            target: player
                            onSecondarySubtitleDelayChanged: {
                                inhibitUpdate = true
                                value = player.secondarySubtitleDelayMS
                                inhibitUpdate = false
                            }
                        }

                        KeyNavigation.right: secondarySubSpinReset
                    }

                    Widgets.TabButtonExt {
                        id: secondarySubSpinReset

                        text: i18n.qtr("Reset")
                        color: "white"
                        onClicked: secondarySubSpin.value = 0
                        KeyNavigation.left: secondarySubSpin
                        KeyNavigation.up: primarySubSpinReset
                    }
                }
            }
        }
    }

    Component {
        id: syncPage

        RowLayout {
            id: syncPageRoot

            spacing: 0
            focus: true
            onActiveFocusChanged: if (activeFocus)
                                      backBtn.forceActiveFocus()

            Item {
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.preferredWidth: VLCStyle.dp(72, VLCStyle.scale)
                Layout.topMargin: VLCStyle.margin_large
                Layout.fillHeight: true

                Widgets.IconToolButton {
                    id: backBtn

                    anchors.horizontalCenter: parent.horizontalCenter
                    size: VLCStyle.dp(36, VLCStyle.scale)
                    iconText: VLCIcons.back
                    color: "white"

                    onClicked: {
                        control._updateWidth(true)
                        syncPageRoot.StackView.view.pop()
                    }
                    KeyNavigation.right: subSpeedSpin
                }
            }

            Rectangle {
                Layout.preferredWidth: VLCStyle.dp(2, VLCStyle.scale)
                Layout.fillHeight: true
                color: "white"
                opacity: .1
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.leftMargin: VLCStyle.margin_xxlarge
                Layout.rightMargin: VLCStyle.margin_xxlarge
                Layout.topMargin: VLCStyle.margin_large
                spacing: VLCStyle.margin_xsmall

                KeyNavigation.left: backBtn

                Widgets.SubtitleLabel {
                    Layout.fillWidth: true
                    text: i18n.qtr("Subtitles")
                    color: "white"
                }
                RowLayout {
                    width: parent.width
                    spacing: VLCStyle.margin_xsmall

                    Widgets.MenuCaption {
                        text: i18n.qtr("Subtitle Speed")
                        color: "white"
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Widgets.TransparentSpinBox {
                        id: subSpeedSpin

                        property bool inhibitUpdate: true

                        stepSize: 1
                        textFromValue: function (value, locale) {
                            return i18n.qtr("%1 fps").arg(
                                        Number(value / 10).toLocaleString(
                                            locale, 'f', 3))
                        }
                        valueFromText: function (text, locale) {
                            return Number.fromLocaleString(
                                        locale,
                                        text.substring(0, text.length - 4)) * 10
                        }

                        Layout.preferredWidth: VLCStyle.dp(128, VLCStyle.scale)

                        onValueChanged: {
                            if (inhibitUpdate)
                                return
                            player.subtitleFPS = value / 10
                        }

                        Component.onCompleted: {
                            value = player.subtitleFPS * 10
                            inhibitUpdate = false
                        }

                        Connections {
                            target: player
                            onSecondarySubtitleDelayChanged: {
                                inhibitUpdate = true
                                value = player.subtitleFPS / 10
                                inhibitUpdate = false
                            }
                        }

                        KeyNavigation.right: subSpeedSpinReset
                    }

                    Widgets.TabButtonExt {
                        id: subSpeedSpinReset

                        text: i18n.qtr("Reset")
                        color: "white"
                        onClicked: subSpeedSpin.value = 10

                        KeyNavigation.right: subSpeedSpin
                    }
                }
            }
        }
    }
}
