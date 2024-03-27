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
import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Dialog {
    id: control

    property Item rootWindow: null

    focus: true
    modal: true


    anchors.centerIn: Overlay.overlay

    padding: VLCStyle.margin_normal
    margins: VLCStyle.margin_large

    implicitWidth: contentWidth > 0 ? contentWidth + leftPadding + rightPadding : 0
    implicitHeight: (header && header.visible ? header.implicitHeight + spacing : 0)
                    + (footer && footer.visible ? footer.implicitHeight + spacing : 0)
                    + (contentHeight > 0 ? contentHeight + topPadding + bottomPadding : 0)

    closePolicy: Popup.CloseOnEscape

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        palette: VLCStyle.palette
        colorSet: ColorContext.Window
    }

    Overlay.modal: Item {
        FastBlur {
            anchors.fill: parent
            anchors.topMargin: MainCtx.windowExtendedMargin
            anchors.leftMargin: MainCtx.windowExtendedMargin
            anchors.rightMargin: MainCtx.windowExtendedMargin
            anchors.bottomMargin: MainCtx.windowExtendedMargin

            source: ShaderEffectSource {
                sourceItem: control.rootWindow
                live: true
                hideSource: true
            }
            radius: 12
        }
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
