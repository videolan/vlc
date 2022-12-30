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

import QtQuick 2.11
import QtQuick.Layouts 1.11

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

TracksPage {
    id: root

    // Functions

    function textFromValueA(value, locale) {
        return I18n.qtr("%1 ms").arg(Number(value).toLocaleString(locale, 'f', 0))
    }

    function valueFromTextA(text, locale) {
        return Number.fromLocaleString(locale, text.substring(0, text.length - 3))
    }

    function textFromValueB(value, locale) {
        return I18n.qtr("%1 fps").arg(Number(value / 10).toLocaleString(locale, 'f', 3))
    }

    function valueFromTextB(text, locale) {
        return Number.fromLocaleString(locale, text.substring(0, text.length - 4)) * 10
    }

    // Children

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right

        spacing: VLCStyle.margin_xxsmall

        Widgets.SubtitleLabel {
            Layout.fillWidth: true

            text: I18n.qtr("Subtitle synchronization")

            color: "white"
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: VLCStyle.margin_large

            spacing: VLCStyle.margin_xsmall

            Widgets.MenuCaption {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter

                text: I18n.qtr("Primary subtitle delay")

                color: "white"
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
                    value = Player.subtitleDelayMS

                    update = true
                }

                onValueChanged: {
                    if (update === false)
                        return

                    Player.subtitleDelayMS = value
                }

                Connections {
                    target: Player

                    onSubtitleDelayChanged: {
                        spinBoxA.update = false

                        spinBoxA.value = Player.subtitleDelayMS

                        spinBoxA.update = true
                    }
                }
            }

            Widgets.ActionButtonOverlay {
                id: resetA

                focus: true

                text: I18n.qtr("Reset")

                Navigation.parentItem: root
                Navigation.leftItem: spinBoxA
                Navigation.downItem: resetB

                onClicked: spinBoxA.value = 0
            }
        }

        RowLayout {
            Layout.fillWidth: true

            spacing: VLCStyle.margin_xsmall

            Widgets.MenuCaption {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter

                text: I18n.qtr("Secondary subtitle delay")

                color: "white"
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

                    Player.secondarySubtitleDelayMS = value
                }

                Connections {
                    target: Player

                    onSecondarySubtitleDelayChanged: {
                        spinBoxB.update = false

                        spinBoxB.value = Player.secondarySubtitleDelayMS

                        spinBoxB.update = true
                    }
                }
            }

            Widgets.ActionButtonOverlay {
                id: resetB

                text: I18n.qtr("Reset")

                Navigation.parentItem: root
                Navigation.leftItem: spinBoxB
                Navigation.upItem: resetA
                Navigation.downItem: resetC

                onClicked: spinBoxB.value = 0
            }
        }

        RowLayout {
            Layout.fillWidth: true

            spacing: VLCStyle.margin_xsmall

            Widgets.MenuCaption {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter

                text: I18n.qtr("Subtitle Speed")

                color: "white"
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

                    onSecondarySubtitleDelayChanged: {
                        spinBoxC.update = false

                        value = Player.subtitleFPS / 10

                        spinBoxC.update = true
                    }
                }
            }

            Widgets.ActionButtonOverlay {
                id: resetC

                text: I18n.qtr("Reset")

                onClicked: spinBoxC.value = 10

                Navigation.parentItem: root
                Navigation.leftItem: spinBoxC
                Navigation.upItem: resetB
            }
        }
    }
}
