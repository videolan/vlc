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
import QtQml.Models 2.2
import QtQml 2.11

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///main/" as MainInterface
import "qrc:///util/" as Util
import "qrc:///style/"

FocusScope {
    id: topFocusScope
    focus: true

    readonly property bool isViewMultiView: false

    signal browse(var tree, int reason)

    Component.onCompleted: resetFocus()
    onActiveFocusChanged: resetFocus()

    function setCurrentItemFocus(reason) {
        deviceSection.setCurrentItemFocus(reason);
    }

    function _centerFlickableOnItem(minY, maxY) {
        if (maxY > flickable.contentItem.contentY + flickable.height) {
            flickable.contentItem.contentY = maxY - flickable.height
        } else if (minY < flickable.contentItem.contentY) {
            flickable.contentItem.contentY = minY
        }
    }

    //FIXME use the right xxxLabel class
    T.Label {
        anchors.centerIn: parent
        visible: (deviceSection.model.count === 0 && lanSection.model.count === 0 )
        font.pixelSize: VLCStyle.fontHeight_xxlarge
        color: topFocusScope.activeFocus ? VLCStyle.colors.accent : VLCStyle.colors.text
        text: I18n.qtr("No network shares found")
    }

    ScrollView {
        id: flickable
        anchors.fill: parent
        focus: true

        Column {
            width: parent.width
            height: implicitHeight

            topPadding: VLCStyle.margin_large
            spacing: VLCStyle.margin_small

            Widgets.SubtitleLabel {
                id: deviceLabel
                text: I18n.qtr("My Machine")
                width: flickable.width
                visible: deviceSection.model.count !== 0
                leftPadding: VLCStyle.margin_xlarge
            }

            NetworkHomeDeviceListView {
                id: deviceSection
                ctx: MainCtx
                sd_source: NetworkDeviceModel.CAT_DEVICES

                width: flickable.width
                visible: deviceSection.model.count !== 0
                onVisibleChanged: topFocusScope.resetFocus()

                onBrowse: topFocusScope.browse(tree, reason)

                Navigation.parentItem: topFocusScope

                Navigation.downAction: function() {
                    if (lanSection.visible == false)
                        return;

                    lanSection.setCurrentItemFocus(Qt.TabFocusReason);
                }

                onActiveFocusChanged: {
                    if (activeFocus)
                        _centerFlickableOnItem(deviceLabel.y, deviceSection.y + deviceSection.height)
                }
            }

            Widgets.SubtitleLabel {
                id: lanLabel
                text: I18n.qtr("My LAN")
                width: flickable.width
                visible: lanSection.model.count !== 0
                leftPadding: VLCStyle.margin_xlarge
                topPadding: deviceLabel.visible ? VLCStyle.margin_small : 0
            }

            NetworkHomeDeviceListView {
                id: lanSection
                ctx: MainCtx
                sd_source: NetworkDeviceModel.CAT_LAN

                width: flickable.width
                visible: lanSection.model.count !== 0
                onVisibleChanged: topFocusScope.resetFocus()

                onBrowse: topFocusScope.browse(tree, reason)

                Navigation.parentItem: topFocusScope

                Navigation.upAction: function() {
                    if (deviceSection.visible == false)
                        return;

                    deviceSection.setCurrentItemFocus(Qt.TabFocusReason);
                }

                onActiveFocusChanged: {
                    if (activeFocus)
                        _centerFlickableOnItem(lanLabel.y, lanSection.y + lanSection.height)
                }
            }
        }

    }

    function resetFocus() {
        var widgetlist = [deviceSection, lanSection]
        var i;
        for (i in widgetlist) {
            if (widgetlist[i].activeFocus && widgetlist[i].visible)
                return
        }

        var found  = false;
        for (i in widgetlist) {
            if (widgetlist[i].visible && !found) {
                widgetlist[i].focus = true
                found = true
            } else {
                widgetlist[i].focus = false
            }
        }
    }
}
