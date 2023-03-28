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
import QtQuick.Templates 2.4 as T

import org.videolan.vlc 0.1
import "qrc:///style/"

Row {
    id: root

    property var labels

    onLabelsChanged: {
        // try to reuse items, texts are assigned with Binding
        // extra items are hidden, Row should take care of them
        if (repeater.count < labels.length)
            repeater.model = labels.length
    }

    spacing: VLCStyle.margin_xxsmall

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Badge
    }

    Repeater {
        id: repeater

        delegate: T.Label {
            id: label

            bottomPadding: VLCStyle.margin_xxxsmall
            topPadding: VLCStyle.margin_xxxsmall
            leftPadding: VLCStyle.margin_xxxsmall
            rightPadding: VLCStyle.margin_xxxsmall

            visible: index < root.labels.length
            text: index >= root.labels.length ? "" :  root.labels[index]

            font.pixelSize: VLCStyle.fontSize_normal

            color: theme.fg.primary

            background: Rectangle {
                anchors.fill: label
                color: theme.bg.primary
                opacity: 0.5
                radius: VLCStyle.dp(3, VLCStyle.scale)
            }

            Accessible.ignored: true
        }
    }
}
