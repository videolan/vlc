/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

TracksPage {
    id: root

    // Functions

    function textFromValueA(value, locale) {
        return qsTr("%1 ms").arg(Number(value).toLocaleString(locale, 'f', 0))
    }

    function valueFromTextA(text, locale) {
        return Number.fromLocaleString(locale, text.substring(0, text.length - 3))
    }

    function textFromValueB(value, locale) {
        return qsTr("%1 fps").arg(Number(value / 10).toLocaleString(locale, 'f', 3))
    }

    function valueFromTextB(text, locale) {
        return Number.fromLocaleString(locale, text.substring(0, text.length - 4)) * 10
    }

    // Children

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right

        spacing: VLCStyle.margin_xsmall

        Widgets.SubtitleLabel {
            Layout.fillWidth: true

            text: qsTr("Subtitle synchronization")

            color: root.colorContext.fg.primary
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: VLCStyle.margin_large

            spacing: VLCStyle.margin_xxxsmall

            Accessible.role: Accessible.Grouping
            Accessible.name: qsTr("Primary subtitle delay")

            DelayEstimator {
                id: delayEstimatorPrimary

                onDelayChanged: {
                    Player.addSubtitleDelay(delayEstimatorPrimary.delay)
                }
            }

            RowLayout {
                Layout.fillWidth: true

                spacing: VLCStyle.margin_xsmall

                Widgets.MenuCaption {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignHCenter

                    text: qsTr("Primary subtitle delay")

                    color: root.colorContext.fg.primary
                }

                Widgets.SpinBoxExt {
                    id: spinBoxA

                    property bool update: false

                    Layout.preferredWidth: VLCStyle.dp(128, VLCStyle.scale)

                    stepSize: 50
                    from: -10000
                    to: 10000

                    textFromValue: root.textFromValueA
                    valueFromText: root.valueFromTextA

                    Navigation.parentItem: root
                    Navigation.rightItem: resetA

                    Component.onCompleted: {
                        spinBoxA.value = Player.subtitleDelayMS

                        spinBoxA.update = true
                    }

                    onValueChanged: {
                        if (update === false)
                            return

                        Player.subtitleDelayMS = spinBoxA.value
                    }

                    Connections {
                        target: Player

                        function onSubtitleDelayChanged() {
                            spinBoxA.update = false

                            spinBoxA.value = Player.subtitleDelayMS

                            spinBoxA.update = true
                        }

                    }
                }

                Widgets.TextToolButton {
                    id: resetA

                    text: qsTr("Reset")

                    focus: true

                    Navigation.parentItem: root
                    Navigation.leftItem: spinBoxA
                    Navigation.downItem: resetB

                    onClicked: {
                        Player.subtitleDelayMS = 0
                        delayEstimatorPrimary.reset()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Layout.alignment: Qt.AlignRight

                spacing: VLCStyle.margin_small

                Widgets.TrackDelayButton {
                    id: voiceHeardA

                    iconTxt: VLCIcons.check
                    text: qsTr("Voice Heard")
                    selected: delayEstimatorPrimary.isHeardTimeMarked

                    onClicked: {
                        delayEstimatorPrimary.markHeardTime()
                        if (!delayEstimatorPrimary.isSpottedTimeMarked && delayEstimatorPrimary.isHeardTimeMarked)
                            textSeenA.animate()
                    }
                }

                Widgets.TrackDelayButton {
                    id: textSeenA

                    iconTxt: VLCIcons.check
                    text: qsTr("Text Seen")
                    selected: delayEstimatorPrimary.isSpottedTimeMarked

                    onClicked: {
                        delayEstimatorPrimary.markSpottedTime()
                        if (!delayEstimatorPrimary.isHeardTimeMarked && delayEstimatorPrimary.isSpottedTimeMarked)
                            voiceHeardA.animate()
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true

            spacing: VLCStyle.margin_xxxsmall

            Accessible.role: Accessible.Grouping
            Accessible.name: qsTr("Secondary subtitle delay")

            DelayEstimator {
                id: delayEstimatorSecondary

                onDelayChanged: {
                    Player.addSecondarySubtitleDelay(delayEstimatorSecondary.delay)
                }
            }

            RowLayout {
                Layout.fillWidth: true

                spacing: VLCStyle.margin_xsmall

                Widgets.MenuCaption {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignHCenter

                    text: qsTr("Secondary subtitle delay")

                    color: root.colorContext.fg.primary
                }

                Widgets.SpinBoxExt {
                    id: spinBoxB

                    property bool update: false

                    Layout.preferredWidth: VLCStyle.dp(128, VLCStyle.scale)

                    stepSize: 50
                    from: -10000
                    to: 10000

                    textFromValue: root.textFromValueA
                    valueFromText: root.valueFromTextA

                    Navigation.parentItem: root
                    Navigation.rightItem: resetB

                    Component.onCompleted: {
                        value = Player.secondarySubtitleDelayMS

                        update = true
                    }

                    onValueChanged: {
                        if (update === false)
                            return

                        Player.secondarySubtitleDelayMS = spinBoxB.value
                    }

                    Connections {
                        target: Player

                        function onSecondarySubtitleDelayChanged() {
                            spinBoxB.update = false

                            spinBoxB.value = Player.secondarySubtitleDelayMS

                            spinBoxB.update = true
                        }
                    }
                }

                Widgets.TextToolButton {
                    id: resetB

                    text: qsTr("Reset")

                    Navigation.parentItem: root
                    Navigation.leftItem: spinBoxB
                    Navigation.upItem: resetA
                    Navigation.downItem: resetC

                    onClicked: {
                        Player.secondarySubtitleDelayMS = 0
                        delayEstimatorSecondary.reset()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Layout.alignment: Qt.AlignRight

                spacing: VLCStyle.margin_xsmall

                Widgets.TrackDelayButton {
                    id: voiceHeardB

                    text: qsTr("Voice Heard")
                    iconTxt: VLCIcons.check
                    selected: delayEstimatorSecondary.isHeardTimeMarked

                    onClicked: {
                        delayEstimatorSecondary.markHeardTime()
                        if (!delayEstimatorSecondary.isSpottedTimeMarked && delayEstimatorSecondary.isHeardTimeMarked)
                            textSeenB.animate()
                    }
                }

                Widgets.TrackDelayButton {
                    id: textSeenB

                    text: qsTr("Text Seen")
                    iconTxt: VLCIcons.check
                    selected: delayEstimatorSecondary.isSpottedTimeMarked

                    onClicked: {
                        delayEstimatorSecondary.markSpottedTime()
                        if (!delayEstimatorSecondary.isHeardTimeMarked && delayEstimatorSecondary.isSpottedTimeMarked)
                            voiceHeardB.animate()
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            spacing: VLCStyle.margin_xsmall

            Accessible.role: Accessible.Grouping
            Accessible.name: qsTr("Subtitle Speed")

            Widgets.MenuCaption {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter

                text: qsTr("Subtitle Speed")

                color: root.colorContext.fg.primary
            }

            Widgets.SpinBoxExt {
                id: spinBoxC

                property bool update: false

                Layout.preferredWidth: VLCStyle.dp(128, VLCStyle.scale)

                stepSize: 1
                from: 0
                to: 100

                textFromValue: root.textFromValueB
                valueFromText: root.valueFromTextB

                Navigation.parentItem: root
                Navigation.rightItem: resetC

                Component.onCompleted: {
                    value = Player.subtitleFPS * 10

                    update = true
                }

                onValueChanged: {
                    if (update === false)
                        return

                    Player.subtitleFPS = value / 10
                }

                Connections {
                    target: Player

                    function onSecondarySubtitleDelayChanged() {
                        spinBoxC.update = false

                        value = Player.subtitleFPS / 10

                        spinBoxC.update = true
                    }
                }
            }

            Widgets.TextToolButton {
                id: resetC

                text: qsTr("Reset")

                onClicked: spinBoxC.value = 10

                Navigation.parentItem: root
                Navigation.leftItem: spinBoxC
                Navigation.upItem: resetB
            }
        }
    }
}
