
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
import QtQuick.Layouts 1.11

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Widgets.RoundImage {
    id: root

    property var labels: []
    property alias progress: progressBar.value
    property alias playCoverVisible: playCover.visible
    property alias playCoverOnlyBorders: playCover.onlyBorders
    property alias playIconSize: playCover.iconSize
    property alias playCoverBorder: playCover.border
    signal playIconClicked

    height: VLCStyle.listAlbumCover_height
    width: VLCStyle.listAlbumCover_width

    RowLayout {
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            topMargin: VLCStyle.margin_xxsmall
            leftMargin: VLCStyle.margin_xxsmall
            rightMargin: VLCStyle.margin_xxsmall
        }

        spacing: VLCStyle.margin_xxsmall

        Repeater {
            model: labels
            VideoQualityLabel {
                Layout.preferredWidth: implicitWidth
                Layout.preferredHeight: implicitHeight
                text: modelData
            }
        }

        Item {
            Layout.fillWidth: true
        }
    }

    Widgets.VideoProgressBar {
        id: progressBar

        visible: !playCover.visible && value > 0
        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }
    }

    Widgets.PlayCover {
        id: playCover

        anchors.fill: parent
        iconSize: VLCStyle.play_root_small

        onIconClicked: root.playIconClicked()
    }
}
