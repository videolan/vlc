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
    id: root

    property int maximumRows: (MainCtx.gridView) ? 2 : 5

    property var sortModel: [
        { text: I18n.qtr("Alphabetic"), criteria: "name"},
        { text: I18n.qtr("Url"),        criteria: "mrl" }
    ]

    property alias model: deviceSection.model

    focus: true

    signal seeAll(var title, var sd_source, int reason)

    signal browse(var tree, int reason)

    Component.onCompleted: resetFocus()
    onActiveFocusChanged: resetFocus()

    function setCurrentItemFocus(reason) {
        deviceSection.setCurrentItemFocus(reason);
    }

    function _centerFlickableOnItem(item) {
        if (item.activeFocus === false)
            return

        var minY
        var maxY

        var index = item.currentIndex

        // NOTE: We want to include the header when we're on the first row.
        if ((MainCtx.gridView && index < item.nbItemPerRow) || index < 1) {
            minY = item.y

            maxY = minY + item.getItemY(index) + item.rowHeight
        } else {
            minY = item.y + item.getItemY(index)

            maxY = minY + item.rowHeight
        }

        // TODO: We could implement a scrolling animation like in ExpandGridView.
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
        color: root.activeFocus ? VLCStyle.colors.accent : VLCStyle.colors.text
        text: I18n.qtr("No network shares found")
    }

    ScrollView {
        id: flickable
        anchors.fill: parent
        focus: true

        Column {
            width: parent.width
            height: implicitHeight

            spacing: VLCStyle.margin_small

            BrowseDeviceView {
                id: deviceSection

                width: flickable.width
                height: contentHeight

                maximumRows: root.maximumRows

                visible: (model.count !== 0)

                model: NetworkDeviceModel {
                    ctx: MainCtx

                    sd_source: NetworkDeviceModel.CAT_DEVICES
                    source_name: "*"

                    maximumCount: deviceSection.maximumCount
                }

                title: I18n.qtr("My Machine")

                Navigation.parentItem: root

                Navigation.downAction: function() {
                    if (lanSection.visible)
                        lanSection.setCurrentItemFocus(Qt.TabFocusReason)
                    else
                        root.Navigation.defaultNavigationDown()
                }

                onBrowse: root.browse(tree, reason)

                onSeeAll: root.seeAll(title, model.sd_source, reason)

                onActiveFocusChanged: _centerFlickableOnItem(deviceSection)
                onCurrentIndexChanged: _centerFlickableOnItem(deviceSection)
            }

            BrowseDeviceView {
                id: lanSection

                width: flickable.width
                height: contentHeight

                maximumRows: root.maximumRows

                visible: (model.count !== 0)

                model: NetworkDeviceModel {
                    ctx: MainCtx

                    sd_source: NetworkDeviceModel.CAT_LAN
                    source_name: "*"

                    maximumCount: lanSection.maximumCount
                }

                title: I18n.qtr("My LAN")

                parentFilter: deviceSection.modelFilter

                Navigation.parentItem: root

                Navigation.upAction: function() {
                    if (deviceSection.visible)
                        deviceSection.setCurrentItemFocus(Qt.TabFocusReason)
                    else
                        root.Navigation.defaultNavigationUp()
                }

                onBrowse: root.browse(tree, reason)

                onSeeAll: root.seeAll(title, model.sd_source, reason)

                onActiveFocusChanged: _centerFlickableOnItem(lanSection)
                onCurrentIndexChanged: _centerFlickableOnItem(lanSection)
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
