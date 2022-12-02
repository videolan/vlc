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
import QtQuick.Templates 2.4 as T
import QtQuick.Layouts 1.11
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Dialog {
    id: control

    property Item rootWindow: null

    focus: true
    modal: true

    x: (rootWindow.x + rootWindow.width - width) / 2
    y: (rootWindow.y + rootWindow.height - height) / 2
    padding: VLCStyle.margin_normal
    margins: VLCStyle.margin_large

    implicitWidth: contentWidth > 0 ? contentWidth + leftPadding + rightPadding : 0
    implicitHeight: (header && header.visible ? header.implicitHeight + spacing : 0)
                    + (footer && footer.visible ? footer.implicitHeight + spacing : 0)
                    + (contentHeight > 0 ? contentHeight + topPadding + bottomPadding : 0)

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        palette: VLCStyle.palette
        colorSet: ColorContext.Window
    }

    Overlay.modal: GaussianBlur {
        source: ShaderEffectSource {
            sourceItem: control.rootWindow
            live: true
        }
        radius: 12
        samples: 16
    }

    background: Rectangle {
        color: theme.bg.primary
    }

    //FIXME use the right xxxLabel class
    header: T.Label {
        text: control.title
        visible: control.title
        elide: Label.ElideRight
        font.bold: true
        color: theme.fg.primary
        padding: 6
        background: Rectangle {
            x: 1; y: 1
            width: parent.width - 2
            height: parent.height - 1
            color: theme.bg.primary
        }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0 }
    }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0 }
    }
}
