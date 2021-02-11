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
import QtQuick.Layouts 1.3

import "qrc:///util/KeyHelper.js" as KeyHelper
import "qrc:///style/"

Item {
    id: root

    property real widthRatio: (3 / 4)
    property bool isRight: true // when set, menu is placed on the right side

    property real leftPadding: VLCStyle.margin_xsmall
    property real rightPadding: VLCStyle.margin_xsmall
    property real topPadding: VLCStyle.margin_large
    property real bottomPadding: VLCStyle.margin_large

    // Sample model:
    // {
    //     title : "Sample Overlay Menu",
    //     entries : []
    // }
    //
    // Entries are intended to be an instance of Action
    // Nested structure (menus inside menus) is supported:
    // if an entry contains 'model' property, it yields another menu.
    // If 'icon.source' is used, Image will be used, but
    // fontIcon property can also be used for loading icon from icon font family.
    // if tickMark property is 'true', a tick mark will be shown on the icon area
    // 'marking' is a string property that can be used to show a label on the right side
    // Example usage can be found in 'Playlist/PlaylistOverlayMenu.qml' file
    property var model: undefined

    property VLCColors colors: VLCStyle.colors

    onModelChanged: {
        listView.currentModel = model
        listView.resetStack()
    }

    property var backgroundItem: undefined

    visible: false

    function open() {
        listView.currentModel = root.model
        visible = true
        listView.forceActiveFocus()
    }

    function close() {
        visible = false
        backgroundItem.forceActiveFocus()
    }

    Rectangle {
        color: "black"
        anchors {
            top: parent.top
            bottom: parent.bottom

            left: isRight ? parent.left : undefined
            right: isRight ? undefined : parent.right
        }
        width: parent.width - parentItem.width
        opacity: 0.5

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true

            onClicked: {
                root.close()
            }
        }
    }

    Item {
        id: parentItem

        anchors {
            top: parent.top
            bottom: parent.bottom

            right: isRight ? parent.right : undefined
            left: isRight ? undefined : parent.left
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true

            acceptedButtons: Qt.NoButton
        }

        FrostedGlassEffect {
            source: backgroundItem

            anchors.fill: parent

            readonly property point overlayPos: backgroundItem.mapFromItem(root, parentItem.x, parentItem.y)
            sourceRect: Qt.rect(overlayPos.x, overlayPos.y, width, height)

            tintStrength: 0.0
            exclusionStrength: 0.1
        }

        KeyNavigableListView {
            id: listView

            anchors.fill: parent
            anchors.topMargin: root.topPadding
            anchors.bottomMargin: root.bottomPadding

            keyNavigationWraps: true

            property var stack: []
            property var currentModel: root.model

            model: currentModel.entries

            Component.onCompleted: {
                resetStack()
            }

            onActiveFocusChanged: {
                if (!activeFocus) {
                    root.close()
                }
            }

            function resetStack() {
                if (stack.length > 0)
                    stack = []

                stack.push(currentModel)
            }

            function goBack() {
                if (stack.length > 1) {
                    stack.pop()
                    currentModel = stack[stack.length - 1]
                }
                else {
                    root.close()
                }
            }

            function loadModel(_model) {
                listView.stack.push(_model)
                listView.currentModel = _model
            }

            header: MenuLabel {
                font.pixelSize: VLCStyle.fontSize_xlarge
                text: listView.currentModel.title

                color: colors.text

                leftPadding: root.leftPadding
                rightPadding: root.rightPadding
                bottomPadding: VLCStyle.margin_normal
            }

            delegate: Button {
                id: button

                readonly property bool yieldsAnotherModel: (!!modelData.model)

                width: listView.width

                leftPadding: root.leftPadding
                rightPadding: root.rightPadding

                function trigger(triggerEnabled) {
                    if (yieldsAnotherModel) {
                        listView.loadModel(modelData.model)
                    }
                    else if (triggerEnabled) {
                        modelData.trigger()
                        root.close()
                    }
                }

                onClicked: trigger(true)

                Keys.onPressed: {
                    if (KeyHelper.matchRight(event)) {
                        trigger(false)
                        event.accepted = true
                    }
                    else if (KeyHelper.matchLeft(event)) {
                        listView.goBack()
                        event.accepted = true
                    }
                    else if (KeyHelper.matchCancel(event)) {
                        root.close()
                        event.accepted = true
                    }
                }

                contentItem: RowLayout {
                    id: rowLayout

                    Item {
                        id: icon

                        Layout.preferredWidth: VLCStyle.icon_small
                        Layout.preferredHeight: VLCStyle.icon_small
                        Layout.alignment: Qt.AlignHCenter

                        Loader {
                            active: (!!modelData.icon.source || !!modelData.fontIcon || modelData.tickMark === true)
                            anchors.fill: parent

                            Component {
                                id: imageIcon
                                Image {
                                    sourceSize: Qt.size(icon.width, icon.height)
                                    source: modelData.icon.source
                                }
                            }

                            Component {
                                id: fontIcon
                                IconLabel {
                                    horizontalAlignment: Text.AlignHCenter
                                    text: modelData.fontIcon
                                    color: colors.text
                                }
                            }

                            Component {
                                id: tickMark
                                ListLabel {
                                    horizontalAlignment: Text.AlignHCenter
                                    text: "✓"
                                    color: colors.text
                                }
                            }

                            sourceComponent: {
                                if (modelData.tickMark === true)
                                    tickMark
                                else if (!!modelData.fontIcon)
                                    fontIcon
                                else
                                    imageIcon
                            }
                        }
                    }

                    ListLabel {
                        id: textLabel

                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignHCenter

                        font.weight: Font.Normal
                        text: modelData.text
                        color: colors.text
                    }

                    ListLabel {
                        visible: modelData.marking.length >= 1

                        Layout.alignment: Qt.AlignHCenter

                        text: {
                            if (button.yieldsAnotherModel)
                                "⮕"
                            else if (!!modelData.marking)
                                modelData.marking
                        }
                        color: colors.text
                    }
                }

                background: Rectangle {
                    visible: button.activeFocus
                    color: colors.accent
                    opacity: 0.8
                }
            }
        }
    }

    states: [
        State {
            name: "visible"
            when: visible

            PropertyChanges {
                target: parentItem
                width: root.width * root.widthRatio
            }
        },
        State {
            name: "hidden"
            when: !visible

            PropertyChanges {
                target: parentItem
                width: 0
            }
        }
    ]

    transitions: [
        Transition {
            from: "*"
            to: "visible"

            NumberAnimation {
                property: "width"
                duration: 125
                easing.type: Easing.InOutSine
            }
        }
    ]
}
