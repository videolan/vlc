/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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
import QtQuick.Window 2.11
import QtQuick.Layouts 1.11
import QtQuick.Controls 2.4

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Window {
    id: root

    flags: Qt.Dialog

    property bool modal: false
    modality: modal ? Qt.ApplicationModal : Qt.NonModal

    width: VLCStyle.appWidth * 0.75
    height: VLCStyle.appHeight * 0.85

    color: VLCStyle.colors.bg

    property alias contentComponent: loader.sourceComponent
    property alias standardButtons: buttonBox.standardButtons

    signal accepted()
    signal rejected(bool byButton)
    signal applied()
    signal discarded()
    signal reset()

    onAccepted: hide()
    onRejected: if (byButton) hide()
    onApplied: hide()
    onDiscarded: hide()
    onReset: hide()

    onClosing: {
        rejected(false)
    }

    function open() {
        show()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: VLCStyle.margin_small

        Loader {
            id: loader
            Layout.fillHeight: true
            Layout.fillWidth: true

            clip: true
            sourceComponent: contentComponent
        }

        DialogButtonBox {
            id: buttonBox

            padding: 0
            spacing: VLCStyle.margin_small

            Layout.fillWidth: true
            Layout.minimumHeight: VLCStyle.icon_normal

            standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel

            background: null

            onAccepted: root.accepted()
            onRejected: root.rejected(true)
            onApplied: root.applied()
            onDiscarded: root.discarded()
            onReset: root.reset()

            delegate: Widgets.TextToolButton {
                id: button

                // NOTE: We specify a dedicated background with borders to improve clarity.
                background: Widgets.AnimatedBackground {
                    backgroundColor: (button.hovered) ? VLCStyle.colors.buttonHover
                                                      : VLCStyle.colors.bgAlt

                    border.width: VLCStyle.border

                    border.color: (button.visualFocus) ? activeBorderColor
                                                       : VLCStyle.colors.buttonBorder
                }
            }
        }
    }
}
