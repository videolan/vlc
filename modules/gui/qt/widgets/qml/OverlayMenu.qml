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
import org.videolan.vlc 0.1

import "qrc:///style/"

FocusScope {
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

    property alias effectSource: effect.source

    property alias scrollBarActive: scrollBar.active

    visible: false

    function open() {
        listView.currentModel = root.model
        visible = true
        focus = true
    }

    function close() {
        visible = false
        focus = false
    }

    Keys.onPressed: {
        if (KeyHelper.matchCancel(event)) {
            close()
            event.accepted = true
        }
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
            id: effect
            anchors.fill: parent

            source: backgroundItem

            tint: VLCStyle.colors.topBanner

            tintStrength: 0.0
            exclusionStrength: 0.1
        }

        ListView {
            id: listView

            anchors.fill: parent
            anchors.topMargin: root.topPadding
            anchors.bottomMargin: root.bottomPadding

            ScrollBar.vertical: ScrollBar { id: scrollBar; active: true }

            focus: true

            keyNavigationWraps: true

            property var stack: []
            property var currentModel: root.model
            property int oldCurrentIndex

            model: currentModel.entries

            Component.onCompleted: {
                resetStack()
            }

            onActiveFocusChanged: if (!activeFocus) root.close()

            function resetStack() {
                if (stack.length > 0)
                    stack = []

                stack.push(currentModel)
            }

            function goBack() {
                if (stack.length > 1) {
                    stack.pop()
                    currentModel = stack[stack.length - 1]
                    listView.currentIndex = listView.oldCurrentIndex
                } else {
                    root.close()
                }
            }

            function loadModel(_model) {
                listView.oldCurrentIndex = listView.currentIndex
                listView.stack.push(_model)
                listView.currentModel = _model
            }

            Keys.onPressed: {
                if (root.isRight ? KeyHelper.matchLeft(event)
                                 : KeyHelper.matchRight(event)) {
                    goBack()
                    event.accepted = true
                }
            }

            header: MenuLabel {
                font.pixelSize: VLCStyle.fontSize_xlarge
                text: listView.currentModel.title

                color: colors.text

                leftPadding: root.leftPadding
                rightPadding: root.rightPadding
                bottomPadding: VLCStyle.margin_normal
            }

            delegate: T.AbstractButton {
                id: button

                implicitWidth: Math.max(background ? background.implicitWidth : 0,
                                        (contentItem ? contentItem.implicitWidth : 0) + leftPadding + rightPadding)
                implicitHeight: Math.max(background ? background.implicitHeight : 0,
                                         (contentItem ? contentItem.implicitHeight : 0) + topPadding + bottomPadding)
                baselineOffset: contentItem ? contentItem.y + contentItem.baselineOffset : 0

                readonly property bool yieldsAnotherModel: (!!modelData.model)

                enabled: modelData.enabled

                width: listView.width

                topPadding: VLCStyle.margin_xsmall
                bottomPadding: VLCStyle.margin_xsmall
                leftPadding: root.leftPadding
                rightPadding: root.rightPadding

                spacing: VLCStyle.margin_xsmall

                function trigger(triggerEnabled) {
                    if (yieldsAnotherModel) {
                        listView.loadModel(modelData.model)
                    } else if (triggerEnabled) {
                        modelData.trigger()
                        root.close()
                    }
                }

                onClicked: trigger(true)

                Keys.onPressed: {
                    if (root.isRight ? KeyHelper.matchRight(event)
                                     : KeyHelper.matchLeft(event)) {
                        trigger(false)
                        event.accepted = true
                    }
                }

                contentItem: RowLayout {
                    id: rowLayout

                    opacity: enabled ? 1.0 : 0.5
                    spacing: button.spacing

                    width: scrollBar.active ? (parent.width - scrollBar.width)
                                            : parent.width

                    Loader {
                        id: icon

                        Layout.preferredWidth: VLCStyle.icon_small
                        Layout.preferredHeight: VLCStyle.icon_small
                        Layout.alignment: Qt.AlignHCenter

                        active: (!!modelData.icon.source || !!modelData.fontIcon || modelData.tickMark === true)

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

                    ListLabel {
                        id: textLabel

                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignHCenter

                        font.weight: Font.Normal
                        text: modelData.text
                        color: colors.text
                    }

                    ListLabel {
                        Layout.alignment: Qt.AlignHCenter

                        horizontalAlignment: Text.AlignHCenter

                        visible: text.length > 0

                        text: (typeof modelData.marking === 'string') ? modelData.marking
                                                                      : button.yieldsAnotherModel ? "➜"
                                                                                                  : ""

                        color: colors.text
                    }
                }
            }

            highlight: Rectangle {
                color: colors.accent
                opacity: 0.8
            }

            highlightResizeDuration: 0
            highlightMoveDuration: 0
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
                duration: VLCStyle.duration_short
                easing.type: Easing.InOutSine
            }
        }
    ]
}
