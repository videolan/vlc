/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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

import "qrc:///style/"

FocusScope{
    id: root

    signal menuExit()
    signal play()
    signal clear()
    signal selectionMode()
    signal moveMode()

    Keys.onPressed: {
        if (event.matches(StandardKey.MoveToPreviousChar)  //left
            || event.matches(StandardKey.MoveToNextChar) //right
            || event.matches(StandardKey.Back)
            || event.matches(StandardKey.Cancel) //esc
        ) {
            _exitMenu();
            event.accepted = true
            return;
        }
    }

    width: VLCStyle.icon_large
    height: VLCStyle.icon_large * 5
    property int _hiddentX: VLCStyle.icon_large

    function _exitMenu() {
        root.state = "hidden"
        menuExit()
    }

    Item {
        id: overlay
        anchors.fill: parent

        Column {
            anchors.right: parent.right
            spacing: VLCStyle.margin_xsmall

            RoundButton {
                id: playButton

                height: activeFocus ? VLCStyle.icon_normal * 1.3 : VLCStyle.icon_normal
                width: activeFocus ? VLCStyle.icon_normal * 1.3 : VLCStyle.icon_normal
                x: root._hiddentX

                KeyNavigation.down: clearButton
                contentItem: Label {
                    text: VLCIcons.play
                    font.family: VLCIcons.fontFamily
                    font.pixelSize: VLCStyle.icon_normal
                    color: "black"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    play()
                    _exitMenu()
                }
                focus: true
                background: Rectangle {
                    radius: parent.radius
                    implicitHeight: parent.width
                    implicitWidth: parent.height
                    color: "palegreen"
                }
            }
            RoundButton {
                id: clearButton

                height: activeFocus ? VLCStyle.icon_normal * 1.3 : VLCStyle.icon_normal
                width: activeFocus ? VLCStyle.icon_normal * 1.3 : VLCStyle.icon_normal
                x: root._hiddentX

                KeyNavigation.down: selectButton
                contentItem: Label {
                    text: VLCIcons.clear
                    font.family: VLCIcons.fontFamily
                    font.pixelSize: VLCStyle.icon_normal
                    color: "black"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    clear()
                    _exitMenu()
                }
                background: Rectangle {
                    radius: parent.radius
                    implicitHeight: parent.width
                    implicitWidth: parent.height
                    color: "pink"
                }
            }
            RoundButton {
                id: selectButton

                height: activeFocus ? VLCStyle.icon_normal * 1.3 : VLCStyle.icon_normal
                width: activeFocus ? VLCStyle.icon_normal * 1.3 : VLCStyle.icon_normal
                x: root._hiddentX

                KeyNavigation.down: moveButton
                contentItem: Label {
                    text: VLCIcons.playlist
                    font.family: VLCIcons.fontFamily
                    font.pixelSize: VLCStyle.icon_normal
                    color: "black"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                checkable: true
                checked: false
                onClicked:  root.state = checked ? "select" : "normal"
                onCheckedChanged: selectionMode(checked)
                background: Rectangle {
                    radius: parent.radius
                    implicitHeight: parent.width
                    implicitWidth: parent.height
                    color: "lightblue"
                }
            }
            RoundButton {
                id: moveButton

                height: activeFocus ? VLCStyle.icon_normal * 1.3 : VLCStyle.icon_normal
                width: activeFocus ? VLCStyle.icon_normal * 1.3 : VLCStyle.icon_normal
                x: root._hiddentX

                KeyNavigation.down: backButton

                contentItem: Label {
                    text: VLCIcons.space
                    font.family: VLCIcons.fontFamily
                    font.pixelSize: VLCStyle.icon_normal
                    color: "black"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                checkable: true
                checked: false
                onClicked:  root.state = checked ? "move" : "normal"
                onCheckedChanged: moveMode(checked)
                background: Rectangle {
                    radius: parent.radius
                    implicitHeight: parent.width
                    implicitWidth: parent.height
                    color: "lightyellow"
                }
            }
            RoundButton {
                id: backButton
                height: activeFocus ? VLCStyle.icon_normal * 1.3 : VLCStyle.icon_normal
                width: activeFocus ? VLCStyle.icon_normal * 1.3 : VLCStyle.icon_normal
                x: root._hiddentX

                contentItem: Label {
                    text: VLCIcons.exit
                    font.family: VLCIcons.fontFamily
                    font.pixelSize: VLCStyle.icon_normal
                    color: "black"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked:  _exitMenu()

                background: Rectangle {
                    radius: parent.radius
                    implicitHeight: parent.width
                    implicitWidth: parent.height
                    color: "lightgrey"
                }
            }
        }
    }

    state: "hidden"
    states: [
        State {
            name: "hidden"
            PropertyChanges { target: selectButton; checked: false }
            PropertyChanges { target: moveButton; checked: false }
        },
        State {
            name: "normal"
            PropertyChanges { target: moveButton; checked: false }
            PropertyChanges { target: selectButton; checked: false }
        },
        State {
            name: "select"
            PropertyChanges { target: selectButton; checked: true }
            PropertyChanges { target: moveButton; checked: false }
        },
        State {
            name: "move"
            PropertyChanges { target: selectButton; checked: false }
            PropertyChanges { target: moveButton; checked: true }
        }
    ]

    transitions: [
        Transition {
            from: "hidden"; to: "*"
            ParallelAnimation {
                SequentialAnimation {
                    NumberAnimation { target: playButton; properties: "x"; duration: 200; from: _hiddentX; to: 0 }
                }
                SequentialAnimation {
                    PauseAnimation { duration: 25 }
                    NumberAnimation { target: clearButton; properties: "x"; duration: 200; from: _hiddentX; to: 0 }
                }
                SequentialAnimation {
                    PauseAnimation { duration: 75 }
                    NumberAnimation { target: selectButton; properties: "x"; duration: 200; from: _hiddentX; to: 0 }
                }
                SequentialAnimation {
                    PauseAnimation { duration: 50 }
                    NumberAnimation { target: moveButton; properties: "x"; duration: 200; from: _hiddentX; to: 0 }
                }
                SequentialAnimation {
                    PauseAnimation { duration: 100 }
                    NumberAnimation { target: backButton; properties: "x"; duration: 200; from: _hiddentX; to: 0 }
                }
            }
        },
        Transition {
            from: "*"; to: "hidden"
            ParallelAnimation {
                SequentialAnimation {
                    PauseAnimation { duration: 100 }
                    NumberAnimation { target: playButton; properties: "x"; duration: 200; from: 0; to: _hiddentX }
                }
                SequentialAnimation {
                    PauseAnimation { duration: 75 }
                    NumberAnimation { target: clearButton; properties: "x"; duration: 200; from: 0; to: _hiddentX }
                }
                SequentialAnimation {
                    PauseAnimation { duration: 50 }
                    NumberAnimation { target: selectButton; properties: "x"; duration: 200; from: 0; to: _hiddentX }
                }
                SequentialAnimation {
                    PauseAnimation { duration: 25 }
                    NumberAnimation { target: moveButton; properties: "x"; duration: 200; from: 0; to: _hiddentX }
                }
                SequentialAnimation {
                    NumberAnimation { target: backButton; properties: "x"; duration: 200; from: 0; to: _hiddentX }
                }
            }
        }
    ]
}
