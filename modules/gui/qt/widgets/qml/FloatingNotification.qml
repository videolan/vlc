/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
 *
 * Authors: Prince Gupta <guptaprince8832@gmail.com>
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

import VLC.MainInterface
import VLC.Style

Column {
    id: root

    signal removeNotification(int index)

    onRemoveNotification: function (index) {
        model.remove(index, 1)
    }

    spacing: VLCStyle.margin_normal

    Behavior on y {
        NumberAnimation {
            duration: VLCStyle.duration_short
            easing.type: Easing.InOutQuad
        }
    }

    Repeater {
        id: repeater

        model: ListModel {
            id: model
        }

        delegate: Rectangle {
            id: delegate

            required property int index
            required property string text

            readonly property ColorContext colorContext: ColorContext {
                id: theme

                colorSet: ColorContext.Window
            }

            function resetLifetime() {
                lifetime.restart()
                state = "VISIBLE"
            }

            anchors.horizontalCenter: parent.horizontalCenter

            color: theme.bg.primary

            focus: false

            width: notifText.implicitWidth
            height: notifText.implicitHeight

            radius: VLCStyle.dp(3, VLCStyle.scale)

            scale: 0

            state: "HIDDEN"

            Component.onCompleted: state = "VISIBLE"

            states: [
                State {
                    name: "VISIBLE"
                    PropertyChanges {
                        target: delegate
                        visible: true
                        scale: 1
                    }
                },
                State {
                    name: "HIDDEN"
                    PropertyChanges {
                        target: delegate
                        visible: false
                        scale: 0
                    }
                }
            ]

            transitions: [
                Transition {
                    from: "VISIBLE"
                    to: "HIDDEN"

                    SequentialAnimation {
                        NumberAnimation {
                            target: delegate
                            property: "scale"
                            duration: VLCStyle.duration_short
                            easing.type: Easing.InOutQuad
                        }

                        PropertyAction {
                            target: delegate
                            property: "visible"
                        }

                        ScriptAction {
                            script: if (!lifetime.running) root.removeNotification(delegate.index)
                        }
                    }
                },

                Transition {
                    from: "HIDDEN"
                    to: "VISIBLE"

                    SequentialAnimation {
                        PropertyAction {
                            target: delegate
                            property: "visible"
                        }

                        NumberAnimation {
                            target: delegate
                            property: "scale"
                            duration: VLCStyle.duration_short
                            easing.type: Easing.InOutQuad
                        }
                    }
                }
            ]

            Timer {
                id: lifetime

                interval: VLCStyle.duration_humanMoment

                running: true

                onTriggered: delegate.state = "HIDDEN"
            }

            DefaultShadow {
                anchors.centerIn: parent

                sourceItem: parent
            }

            SubtitleLabel {
                id: notifText

                padding: VLCStyle.margin_small

                color: theme.fg.primary

                text: delegate.text
            }
        }
    }

    Connections {
        target: UINotifier

        function onShowNotification(id, text) {
            let found = false
            for (let i = 0; i < model.count; ++i) {
                if (model.get(i).id === id) {
                    found = true

                    const del = repeater.itemAt(i)
                    if (del.text !== text) {
                        del.text = text
                        del.resetLifetime()
                    }

                    break
                }
            }

            if (!found) {
                model.append({"id": id, "text": text})
            }
        }
    }
}
