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
import QtQuick.Templates 2.4 as T
import QtQuick.Layouts 1.11

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

T.Control {
    id: control

    property var path

    property var _contentModel
    property var _menuModel

    readonly property int maximumWidth: VLCStyle.bannerTabButton_width_large * 4
    readonly property int minimumWidth: VLCStyle.bannerTabButton_width_large

    signal browse(var tree, int reason)
    signal homeButtonClicked(int reason)

    onPathChanged: createContentModel()
    onAvailableWidthChanged: createContentModel()
    implicitWidth: VLCStyle.bannerTabButton_width_large * 4
    implicitHeight: VLCStyle.dp(24, VLCStyle.scale)
    focus: true
    onActiveFocusChanged: if (activeFocus) contentItem.forceActiveFocus(focusReason)

    function createContentModel() {
        var contentModel = []
        var menuModel = []
        if (path.length < 1)
            return
        var leftWidth = control.availableWidth
        var i = path.length
        while (--i >= 0) {
            var textWidth = fontMetrics.advanceWidth(path[i].display)
                    + (i !== path.length - 1 ? iconMetrics.advanceWidth(
                                                    VLCIcons.breadcrumb_sep) : 0) + VLCStyle.margin_xsmall * 4

            if (i < path.length - 1 && textWidth > leftWidth)
                menuModel.push(path[i])
            else
                contentModel.unshift(path[i])
            leftWidth -= textWidth
        }
        control._contentModel = contentModel
        control._menuModel = menuModel
    }

    background: Rectangle {
        border.width: VLCStyle.dp(1, VLCStyle.scale)
        border.color: VLCStyle.colors.setColorAlpha(VLCStyle.colors.text, .4)
        color: VLCStyle.colors.bg
    }

    contentItem: RowLayout {
        spacing: VLCStyle.margin_xxsmall
        width: control.availableWidth
        onActiveFocusChanged: if (activeFocus) homeButton.forceActiveFocus(focusReason)

        AddressbarButton {
            id: homeButton

            text: VLCIcons.home
            font.pixelSize: VLCStyle.icon_addressBar

            Layout.fillHeight: true

            Navigation.parentItem: control
            Navigation.rightAction: function () {
                if (menuButton.visible)
                    menuButton.forceActiveFocus(Qt.TabFocusReason)
                else
                    contentRepeater.itemAt(0).forceActiveFocus(Qt.TabFocusReason)
            }
            Keys.priority: Keys.AfterItem
            Keys.onPressed: Navigation.defaultKeyAction(event)

            onClicked: control.homeButtonClicked(focusReason)
        }

        AddressbarButton {
            id: menuButton

            visible: !!control._menuModel && control._menuModel.length > 0
            text: VLCIcons.breadcrumb_prev
            font.pixelSize: VLCStyle.icon_addressBar

            Layout.fillHeight: true

            Navigation.parentItem: control
            Navigation.leftItem: homeButton
            Navigation.rightAction: function () {
                contentRepeater.itemAt(0).forceActiveFocus(Qt.TabFocusReason)
            }
            Keys.priority: Keys.AfterItem
            Keys.onPressed: Navigation.defaultKeyAction(event)

            onClicked: popup.show()
        }

        Repeater {
            id: contentRepeater
            model: control._contentModel
            delegate: RowLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.maximumWidth: implicitWidth
                focus: true
                spacing: VLCStyle.margin_xxxsmall
                onActiveFocusChanged: if (activeFocus) btn.forceActiveFocus(focusReason)

                Navigation.parentItem: control
                Navigation.leftAction: function() {
                    if (index !== 0)
                        contentRepeater.itemAt(index - 1).forceActiveFocus(Qt.BacktabFocusReason)
                    else if (menuButton.visible)
                        menuButton.forceActiveFocus(Qt.BacktabFocusReason)
                    else
                        homeButton.forceActiveFocus(Qt.BacktabFocusReason)
                }

                Navigation.rightAction: function () {
                    if (index !== contentRepeater.count - 1)
                        contentRepeater.itemAt(index + 1).forceActiveFocus(Qt.TabFocusReason)
                    else
                        control.Navigation.defaultNavigationRight()
                }

                Keys.priority: Keys.AfterItem
                Keys.onPressed: Navigation.defaultKeyAction(event)

                AddressbarButton {
                    id: btn

                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: modelData.display
                    onlyIcon: false
                    highlighted: index === contentRepeater.count - 1

                    onClicked: browse(modelData.tree, focusReason)
                }

                Widgets.IconLabel {
                    Layout.fillHeight: true
                    visible: index !== contentRepeater.count - 1
                    text: VLCIcons.breadcrumb_sep
                    font.pixelSize: VLCStyle.icon_addressBar
                    color: VLCStyle.colors.text
                    opacity: .6
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Item {
            Layout.fillWidth: true
        }
    }

    FontMetrics {
        id: fontMetrics
        font.pixelSize: VLCStyle.fontSize_large
    }

    FontMetrics {
        id: iconMetrics
        font {
            pixelSize: VLCStyle.fontSize_large
            family: VLCIcons.fontFamily
        }
    }

    StringListMenu {
        id: popup

        function show() {
            var model = control._menuModel.map(function (modelData) {
                return modelData.display
            })

            var point = control.mapToGlobal(0, menuButton.height + VLCStyle.margin_xxsmall)

            popup.popup(point, model)
        }

        onSelected: {
            browse(control._menuModel[index].tree, focusReason)
        }
    }
}
