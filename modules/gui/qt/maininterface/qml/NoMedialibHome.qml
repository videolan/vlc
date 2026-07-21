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
import QtQuick
import QtQuick.Window

import VLC.MainInterface
import VLC.Util
import VLC.Style

FocusScope {
    id: root

    property var pagePrefix: [] // behave like a Page

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    Accessible.role: Accessible.Client
    Accessible.name: qsTr("Home View")


    component ConeNButtons: FocusScope {
        id: coneNButtons

        property int orientation: Qt.Vertical

        property real spacing: VLCStyle.margin_large

        implicitWidth: orientation === Qt.Vertical ? Math.max(cone.implicitWidth, button.implicitWidth)
                                                   : cone.implicitWidth + spacing + button.implicitWidth
        implicitHeight: orientation === Qt.Vertical ? cone.implicitHeight + spacing + button.implicitHeight
                                                    : cone.implicitHeight

        states: [
            State {
                name: "vertical"
                AnchorChanges {
                    target: cone

                    anchors.left: undefined
                    anchors.horizontalCenter: coneNButtons.horizontalCenter
                }
                AnchorChanges {
                    target: button

                    anchors.left: undefined
                    anchors.verticalCenter: undefined
                    anchors.top: cone.bottom
                    anchors.horizontalCenter: coneNButtons.horizontalCenter
                }
                PropertyChanges {
                    target: button

                    anchors.topMargin: coneNButtons.spacing
                }
            },
            State {
                name: "horizontal"
                AnchorChanges {
                    target: cone

                    anchors.horizontalCenter: undefined
                    anchors.left: coneNButtons.left
                }
                AnchorChanges {
                    target: button

                    anchors.top: undefined
                    anchors.horizontalCenter: undefined
                    anchors.left: cone.right
                    anchors.verticalCenter: coneNButtons.verticalCenter
                }
                PropertyChanges {
                    target: button

                    anchors.leftMargin: coneNButtons.spacing
                }
            }
        ]

        state: orientation === Qt.Vertical ? "vertical" : "horizontal"


        Image {
            id: cone

            property real _eDPR: MainCtx.effectiveDevicePixelRatio(Window.window)

            sourceSize: Qt.size(0, orientation === Qt.Vertical ? VLCStyle.colWidth(1)
                                                               : button.implicitHeight * 1.618 * _eDPR) // 1.618 = golden ratio approximation

            source: MainCtx.useXmasCone() ? "qrc:///logo/vlc48-xmas.png" // TODO: new xmas cone designs
                                          : "qrc:///misc/cone.svg"

            Connections {
                target: MainCtx

                function onIntfDevicePixelRatioChanged() {
                    // Update the DPR:
                    // Normally, this is not done, as we display the images at the size we
                    // want, and we don't want to re-load all images on DPR change. But
                    // in this case we depend on the implicit size, so we should re-load
                    // the image with the updated DPR:
                    cone._eDPR = MainCtx.effectiveDevicePixelRatio(cone.Window.window)
                }
            }
        }

        OpenFileSplitButton {
            id: button

            focus: true

            Navigation.parentItem: coneNButtons
        }
    }


    ConeNButtons {
        focus: true

        anchors.centerIn: root

        Navigation.parentItem: root
    }
}
