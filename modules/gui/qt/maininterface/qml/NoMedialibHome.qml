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
import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style
import VLC.Dialogs

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

        implicitWidth: orientation === Qt.Vertical ? Math.max(cone.implicitWidth, buttons.implicitWidth)
                                                   : cone.implicitWidth + spacing + buttons.implicitWidth
        implicitHeight: orientation === Qt.Vertical ? cone.implicitHeight + spacing + buttons.implicitHeight
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
                    target: buttons

                    anchors.left: undefined
                    anchors.verticalCenter: undefined
                    anchors.top: cone.bottom
                    anchors.horizontalCenter: coneNButtons.horizontalCenter
                }
                PropertyChanges {
                    target: buttons

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
                    target: buttons

                    anchors.top: undefined
                    anchors.horizontalCenter: undefined
                    anchors.left: cone.right
                    anchors.verticalCenter: coneNButtons.verticalCenter
                }
                PropertyChanges {
                    target: buttons

                    anchors.leftMargin: coneNButtons.spacing
                }
            }
        ]

        state: orientation === Qt.Vertical ? "vertical" : "horizontal"


        Image {
            id: cone

            property real _eDPR: MainCtx.effectiveDevicePixelRatio(Window.window)

            sourceSize: Qt.size(0, orientation === Qt.Vertical ? VLCStyle.colWidth(1)
                                                               : buttons.implicitHeight * 1.618 * _eDPR) // 1.618 = golden ratio approximation

            source: MainCtx.useXmasCone() ? "qrc:///logo/vlc48-xmas.png" // TODO: new xmas cone designs
                                          : SVGColorImage.colorize("qrc:///misc/cone.svg").accent(theme.accent).uri()

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

        Row {
            id: buttons

            spacing: coneNButtons.spacing

            Widgets.ActionButtonPrimary {
                id: fileButton

                focus: true

                text: qsTr("Open File")

                // NOTE: Use the same width for the buttons (give more width if necessary) to have bilateral symmetry:
                width: Math.max(fileButton.implicitWidth, discButton.implicitWidth)

                Navigation.parentItem: coneNButtons
                Navigation.rightItem: discButton

                // TODO: The full-fledged open dialog is advertised as "Open Multiple Files" in the menu.
                //       In the future, we can have the "Open File" button as a combo box button that has these options,
                //       with the default being a simple open dialog:
                //       - Default: simple open dialog.
                //       - Combo box option 1 (user clicks the down button, and the combo box reveals all buttons): open multiple files (Ctrl+Shift+O).
                //       - Combo box option 2: open location from clipboard (Ctrl+V) / simple text edit dialog.
                onClicked: DialogsProvider.simpleOpenDialog()
            }

            Widgets.ActionButtonPrimary {
                id: discButton

                text: qsTr("Open Disc")

                // NOTE: Use the same width for the buttons (give more width if necessary) to have bilateral symmetry:
                width: Math.max(fileButton.implicitWidth, discButton.implicitWidth)

                Navigation.parentItem: coneNButtons
                Navigation.leftItem: fileButton

                onClicked: DialogsProvider.openDiscDialog()
            }
        }
    }


    ConeNButtons {
        focus: true

        anchors.centerIn: root

        Navigation.parentItem: root
    }
}
