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
import QtQuick.Controls.impl 2.4
import QtQuick.Layouts 1.3

import "qrc:///widgets/" as Widgets
import "qrc:///util/KeyHelper.js" as KeyHelper
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: overlayMenu
    visible: false

    property alias models: playlistMenu.models
    property alias currentModel: playlistMenu.currentModel

    property int leftPadding: 0
    property int rightPadding: 0

    onActiveFocusChanged: {
        if (!activeFocus) {
            overlayMenu.close()
        }
    }

    function close() {
        overlayMenu.visible = false
        view.forceActiveFocus()
    }

    function open() {
        playlistMenu.currentModel = "rootmenu"
        playlistMenu.menuHierachy = []
        overlayMenu.visible = true
        overlayMenu.forceActiveFocus()
    }

    function pushMenu(menu) {
        playlistMenu.menuHierachy.push(playlistMenu.currentModel)
        playlistMenu.currentModel = menu
    }

    property real drawerRatio: 0
    Behavior on drawerRatio {
        NumberAnimation {
            duration: 150
        }
    }
    onVisibleChanged: {
        drawerRatio = visible ? 0.9 : 0
    }

    Rectangle {
        color: "black"
        anchors {
            left: parent.left
            top: parent.top
            bottom: parent.bottom
        }
        width: parent.width * (1 - drawerRatio)
        opacity: 0.4
    }


    Rectangle {
        color: "black"
        anchors {
            right: parent.right
            top: parent.top
            bottom: parent.bottom
        }
        width: parent.width * drawerRatio
        opacity: 0.9


        //avoid mouse event to be propagated to the widget below
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
        }


        Widgets.KeyNavigableListView {
            id: playlistMenu
            anchors.fill: parent
            focus: true

            property var models: {
                "rootmenu" : {
                    "title" : "",
                    "entries" : []
                }
            }
            property string currentModel: "rootmenu"
            property var menuHierachy: []

            model: models[currentModel]["entries"]
            modelCount: models[currentModel]["entries"].length

            header: Label {
                text: models[currentModel]["title"]
                color: "white"
                font.pixelSize: VLCStyle.fontSize_xlarge
                font.bold: true

                leftPadding: VLCStyle.margin_small
                rightPadding: VLCStyle.margin_small
                topPadding: VLCStyle.margin_xsmall
                bottomPadding: VLCStyle.margin_xsmall
                height: VLCStyle.fontHeight_xlarge + topPadding + bottomPadding
            }

            delegate: Button {
                id: control
                text: modelData.text
                width: playlistMenu.width

                leftPadding: VLCStyle.margin_small + root.leftPadding
                rightPadding: VLCStyle.margin_small + root.rightPadding

                icon.width: VLCStyle.fontHeight_normal
                icon.height: VLCStyle.fontHeight_normal

                contentItem: Label {
                    text: control.text
                    color: "white"
                    font.pixelSize: VLCStyle.fontSize_normal
                    leftPadding: VLCStyle.icon_small

                }

                background: Rectangle {
                    implicitWidth: 100
                    implicitHeight: VLCStyle.fontHeight_normal
                    color: control.activeFocus ? "orange" : "transparent"

                    ColorImage {
                        width: control.icon.width
                        height: control.icon.height

                        x: control.mirrored ? control.width - width - control.rightPadding : control.leftPadding
                        y: control.topPadding + (control.availableHeight - height) / 2

                        source: control.checked ? "qrc:/qt-project.org/imports/QtQuick/Controls.2/images/check.png"
                            : modelData.icon.source ? modelData.icon.source
                            : ""
                        visible: true
                        color: control.enabled ? VLCStyle.colors.playerFg : VLCStyle.colors.playerFgInactive
                    }

                    ColorImage {
                        x: control.mirrored ? control.leftPadding : control.width - width - control.rightPadding
                        y: control.topPadding + (control.availableHeight - height) / 2

                        width: VLCStyle.icon_xsmall
                        height: VLCStyle.icon_xsmall

                        visible: !!modelData["subMenu"]
                        mirror: control.mirrored
                        color: control.enabled ? VLCStyle.colors.playerFg : VLCStyle.colors.playerFgInactive
                        source: "qrc:/qt-project.org/imports/QtQuick/Controls.2/images/arrow-indicator.png"
                    }
                }

                onClicked: {

                    if (!!modelData["subMenu"]) {
                        pushMenu(modelData["subMenu"])
                    } else {
                        modelData.trigger()
                        overlayMenu.close()
                    }
                }

                Keys.onPressed:  {
                    if (KeyHelper.matchRight(event)) {
                        if (!!modelData["subMenu"]) {
                            pushMenu(modelData["subMenu"])
                            event.accepted = true
                        }
                    } else if (KeyHelper.matchLeft(event)) {
                        if (playlistMenu.menuHierachy.length > 0) {
                            playlistMenu.currentModel = playlistMenu.menuHierachy.pop()
                            event.accepted = true
                        } else {
                            overlayMenu.close()
                        }
                    }
                }

                Keys.onReleased: {
                    if (KeyHelper.matchCancel(event)) {
                        event.accepted = true
                        if (playlistMenu.menuHierachy.length > 0) {
                            playlistMenu.currentModel = playlistMenu.menuHierachy.pop()
                        } else {
                            overlayMenu.close()
                        }
                    }
                }
            }
        }
    }
}
