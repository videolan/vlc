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
import QtQuick.Templates 2.4 as T
import QtQuick.Layouts 1.3
import QtQml.Models 2.11

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///util/KeyHelper.js" as KeyHelper

T.MenuItem {
    id: control

    //implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
    //                        implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: contentId.implicitHeight + topPadding + bottomPadding

    padding: VLCStyle.margin_xsmall
    spacing: VLCStyle.margin_xsmall

    icon.width: VLCStyle.icon_small
    icon.height: VLCStyle.icon_small
    icon.color: enabled ? VLCStyle.colors.playerFg : VLCStyle.colors.playerFgInactive

    font.pixelSize: VLCStyle.fontSize_normal

    leftPadding: VLCStyle.applicationHorizontalMargin

    property var parentMenu: undefined

    //workaround QTBUG-7018 for Qt < 5.12.2
    activeFocusOnTab: control.enabled

    indicator: ColorImage {
        id: indicatorId

        width: control.icon.width
        height: control.icon.height

        x: control.mirrored ? control.width - width - control.rightPadding : control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2

        visible: true
        source: control.checked ? "qrc:/qt-project.org/imports/QtQuick/Controls.2/images/check.png"
                : icon.source ? icon.source
                : ""
        color: control.enabled ? VLCStyle.colors.playerFg : VLCStyle.colors.playerFgInactive
    }

    arrow: ColorImage {
        x: control.mirrored ? control.padding : control.width - width - control.padding
        y: control.topPadding + (control.availableHeight - height) / 2

        width: VLCStyle.icon_xsmall
        height: VLCStyle.icon_xsmall

        visible: control.subMenu
        mirror: control.mirrored
        color: control.enabled ? VLCStyle.colors.playerFg : VLCStyle.colors.playerFgInactive
        source: "qrc:/qt-project.org/imports/QtQuick/Controls.2/images/arrow-indicator.png"
    }

    contentItem:  IconLabel {
        id: contentId
        implicitHeight: VLCStyle.fontHeight_normal

        readonly property real arrowPadding: control.subMenu && control.arrow ? control.arrow.width + control.spacing : 0
        readonly property real indicatorPadding: control.indicator.width + control.spacing
        leftPadding: !control.mirrored ? indicatorPadding : arrowPadding
        rightPadding: control.mirrored ? indicatorPadding : arrowPadding

        width: parent.width

        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        alignment: Qt.AlignLeft

        //icon: control.icon
        text: control.text
        font: control.font
        color: control.enabled ? VLCStyle.colors.playerFg : VLCStyle.colors.playerFgInactive
    }


    background: Widgets.FocusBackground {
        implicitHeight: VLCStyle.fontHeight_normal
        active: control.highlighted
    }

    //hack around QTBUG-79115
    Keys.priority: Keys.BeforeItem
    Keys.onLeftPressed: {
        if (parentMenu && parentMenu.parentMenu) {
            parentMenu._emitMenuClose = false
        }
        event.accepted = false
    }

    Keys.onRightPressed:  {
        if (parentMenu && subMenu) {
            parentMenu._emitMenuClose = false
        }
        event.accepted = false
    }

    Keys.onReleased: {
        if (KeyHelper.matchCancel(event)) {
            event.accepted = true
            parentMenu.dismiss()
        }
    }

    onTriggered: {
        if (parentMenu && subMenu) {
            parentMenu._emitMenuClose = false
        }
    }
}
