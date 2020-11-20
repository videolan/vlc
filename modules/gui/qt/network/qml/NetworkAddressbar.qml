
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
import QtQuick.Layouts 1.3
import QtQml.Models 2.11

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

Control {
    id: control

    property var path
    signal homeButtonClicked

    property var _contentModel
    property var _menuModel

    readonly property int maximumWidth: VLCStyle.bannerTabButton_width_large * 4
    readonly property int minimumWidth: VLCStyle.bannerTabButton_width_large

    onPathChanged: createContentModel()
    onAvailableWidthChanged: createContentModel()
    implicitWidth: VLCStyle.bannerTabButton_width_large * 4
    implicitHeight: VLCStyle.dp(24, VLCStyle.scale)
    focus: true
    onActiveFocusChanged: if (activeFocus)
                              contentItem.forceActiveFocus()

    function changeTree(newTree) {
        popup.close()
        history.push(["mc", "network", {
                          "tree": newTree
                      }])
    }

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
                                                    VLCIcons.back) : 0) + VLCStyle.margin_xsmall * 4

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
        onActiveFocusChanged: if (activeFocus)
                                  homeButton.forceActiveFocus()

        AddressbarButton {
            id: homeButton

            Layout.fillHeight: true
            text: VLCIcons.home

            onClicked: control.homeButtonClicked()
            Keys.onPressed: {
                if (event.accepted || event.key !== Qt.Key_Right)
                    return
                if (menuButton.visible)
                    menuButton.forceActiveFocus()
                else
                    contentRepeater.itemAt(0).forceActiveFocus()
                event.accepted = true
            }
        }

        AddressbarButton {
            id: menuButton

            Layout.fillHeight: true
            visible: !!control._menuModel && control._menuModel.length > 0
            text: VLCIcons.back + VLCIcons.back
            font.pixelSize: VLCIcons.pixelSize(VLCStyle.icon_small)
            KeyNavigation.left: homeButton

            onClicked: popup.open()
            Keys.onPressed: {
                if (event.accepted || event.key !== Qt.Key_Right)
                    return
                contentRepeater.itemAt(0).forceActiveFocus()
                event.accepted = true
            }
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
                onActiveFocusChanged: if (activeFocus)
                                          btn.forceActiveFocus()
                Keys.onPressed: {
                    if (event.accepted)
                        return
                    if (event.key === Qt.Key_Right
                            && index !== contentRepeater.count - 1) {
                        contentRepeater.itemAt(index + 1).forceActiveFocus()
                        event.accepted = true
                    }
                    else if (event.key === Qt.Key_Left) {
                        if (index !== 0)
                            contentRepeater.itemAt(index - 1).forceActiveFocus()
                        else if (menuButton.visible)
                            menuButton.forceActiveFocus()
                        else
                            homeButton.forceActiveFocus()
                        event.accepted = true
                    }
                }

                AddressbarButton {
                    id: btn

                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: modelData.display
                    onlyIcon: false
                    highlighted: index === contentRepeater.count - 1

                    onClicked: changeTree(modelData.tree)
                }

                Widgets.IconLabel {
                    Layout.fillHeight: true
                    visible: index !== contentRepeater.count - 1
                    text: VLCIcons.back
                    rotation: 180
                    font.pixelSize: VLCIcons.pixelSize(VLCStyle.icon_small)
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

    Popup {
        id: popup

        y: menuButton.height + VLCStyle.margin_xxsmall
        closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape
        width: VLCStyle.dp(150, VLCStyle.scale)
        implicitHeight: contentItem.implicitHeight + padding * 2
        leftPadding: 0
        rightPadding: 0

        onOpened: {
            updateBgRect()

            menuButton.KeyNavigation.down = optionList
            menuButton.highlighted = true
            optionList.forceActiveFocus()
        }

        onClosed: {
            menuButton.KeyNavigation.down = null
            menuButton.highlighted = false
            menuButton.forceActiveFocus()
        }

        contentItem: ListView {
            id: optionList

            implicitHeight: contentHeight
            model: control._menuModel
            spacing: VLCStyle.margin_xxxsmall
            delegate: ItemDelegate {
                id: delegate

                text: modelData.display
                width: parent.width
                background: Rectangle {
                    color: VLCStyle.colors.accent
                    visible: parent.hovered || parent.activeFocus
                }

                contentItem: Widgets.ListLabel {
                    text: delegate.text
                }

                onClicked: {
                    changeTree(modelData.tree)
                }
            }
        }

        function updateBgRect() {
            glassEffect.popupGlobalPos = mainInterfaceRect.mapFromItem(control,
                                                                       popup.x,
                                                                       popup.y)
        }

        background: Rectangle {
            border.width: VLCStyle.dp(1)
            border.color: VLCStyle.colors.accent

            Widgets.FrostedGlassEffect {
                id: glassEffect
                source: mainInterfaceRect

                anchors.fill: parent
                anchors.margins: VLCStyle.dp(1)

                property point popupGlobalPos
                sourceRect: Qt.rect(popupGlobalPos.x, popupGlobalPos.y,
                                    glassEffect.width, glassEffect.height)

                tint: VLCStyle.colors.bg
                tintStrength: 0.3
            }
        }

        Connections {
            target: mainInterfaceRect

            enabled: popup.visible

            onWidthChanged: {
                popup.updateBgRect()
            }

            onHeightChanged: {
                popup.updateBgRect()
            }
        }

        Connections {
            target: mainInterface

            enabled: popup.visible

            onIntfScaleFactorChanged: {
                popup.updateBgRect()
            }
        }

        Connections {
            target: playlistColumn

            onWidthChanged: {
                popup.updateBgRect()
            }
        }
    }
}
