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
import QtQuick.Layouts 1.3

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

Widgets.NavigableFocusScope {
    id: resumePanel

    implicitWidth: layout.implicitWidth
    implicitHeight: layout.implicitHeight

    visible: false

    signal hidden()

    function showResumePanel ()
    {
        resumePanel.visible = true
        continueBtn.forceActiveFocus()
        resumeTimeout.start()
    }

    function hideResumePanel() {
        resumeTimeout.stop()
        resumePanel.visible = false
        hidden()
    }

    Timer {
        id: resumeTimeout
        interval: 10000
        onTriggered: {
            resumePanel.visible = false
            hidden()
        }
    }

    Connections {
        target: player
        onCanRestorePlaybackChanged: {
            if (player.canRestorePlayback) {
                showResumePanel()
            } else {
                hideResumePanel()
            }
        }
    }

    Component.onCompleted: {
        if (player.canRestorePlayback) {
            showResumePanel()
        }
    }

    RowLayout {
        id: layout
        anchors.fill: parent

        Text {
            Layout.preferredHeight: implicitHeight
            Layout.fillWidth: true

            color: VLCStyle.colors.playerFg
            font.pixelSize: VLCStyle.fontSize_normal
            font.bold: true

            text: i18n.qtr("Do you want to restart the playback where you left off?")
        }

        Widgets.TabButtonExt {
            id: continueBtn
            Layout.preferredHeight: implicitHeight
            Layout.preferredWidth: implicitWidth
            text: i18n.qtr("Continue")
            font.bold: true
            focus: true
            onClicked: {
                player.restorePlaybackPos()
                hideResumePanel()
            }

            KeyNavigation.right: closeBtn
        }

        Widgets.TabButtonExt {
            id: closeBtn
            Layout.preferredHeight: implicitHeight
            Layout.preferredWidth: implicitWidth
            text: "X"
            font.bold: true
            onClicked: hideResumePanel()

            KeyNavigation.left: continueBtn
        }
    }
}

