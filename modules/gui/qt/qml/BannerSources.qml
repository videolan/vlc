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
import QtQuick.Layouts 1.3
import org.videolan.vlc 0.1


import "qrc:///style/"
import "qrc:///utils/" as Utils
import "qrc:///menus/" as Menus


Utils.NavigableFocusScope {
    id: root
    height: VLCStyle.icon_normal + VLCStyle.margin_small

    property int selectedIndex: 0
    property alias model: pLBannerSources.model
    signal toogleMenu()

    // Triggered when the toogleView button is selected
    function toggleView () {
        medialib.gridView = !medialib.gridView
    }

    Rectangle {
        id: pLBannerSources

        anchors.fill: parent

        color: VLCStyle.colors.banner
        property alias model: buttonView.model

        RowLayout {
            anchors.fill: parent

            Utils.IconToolButton {
                id: history_back
                size: VLCStyle.icon_normal
                text: VLCIcons.dvd_prev

                focus: true
                KeyNavigation.right: buttonView
                onClicked: history.pop(History.Go)
            }

            /* Button for the sources */
            TabBar {
                id: buttonView

                focusPolicy: Qt.StrongFocus

                Layout.preferredHeight: VLCStyle.icon_normal
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                KeyNavigation.left: history_back

                Component.onCompleted: {
                    buttonView.contentItem.focus= true
                }

                background: Rectangle {
                    color: "transparent"
                }

                property alias model: sourcesButtons.model
                /* Repeater to display each button */
                Repeater {
                    id: sourcesButtons
                    focus: true

                    TabButton {
                        id: control
                        text: model.displayText

                        //initial focus
                        focusPolicy: Qt.StrongFocus
                        //focus: index === 1
                        focus: model.selected

                        Component.onCompleted: {
                            if (model.selected) {
                                buttonView.currentIndex = index
                            }
                        }

                        checkable: true
                        padding: 0
                        onClicked: {
                            checked =  !control.checked;
                            root.selectedIndex = model.index
                        }

                        font.pixelSize: VLCStyle.fontSize_normal

                        background: Rectangle {
                            height: parent.height
                            width: parent.contentItem.width
                            //color: (control.hovered || control.activeFocus) ? VLCStyle.colors.bgHover : VLCStyle.colors.banner
                            color: VLCStyle.colors.banner
                        }

                        contentItem: Row {
                            Image {
                                id: icon
                                anchors {
                                    verticalCenter: parent.verticalCenter
                                    rightMargin: VLCStyle.margin_xsmall
                                    leftMargin: VLCStyle.margin_small
                                }
                                height: VLCStyle.icon_normal
                                width: VLCStyle.icon_normal
                                source: model.pic
                                fillMode: Image.PreserveAspectFit
                            }

                            Label {
                                text: control.text
                                font: control.font
                                color: control.hovered ?  VLCStyle.colors.textActiveSource : VLCStyle.colors.text
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignHCenter

                                anchors {
                                    verticalCenter: parent.verticalCenter
                                    rightMargin: VLCStyle.margin_xsmall
                                    leftMargin: VLCStyle.margin_small
                                }

                                Rectangle {
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                        bottom: parent.bottom
                                    }
                                    height: 2
                                    visible: control.activeFocus || control.checked
                                    color: control.activeFocus ? VLCStyle.colors.accent  : VLCStyle.colors.bgHover
                                }
                            }
                        }
                    }
                }
            }


            ToolBar {
                Layout.preferredHeight: VLCStyle.icon_normal
                //Layout.preferredWidth: VLCStyle.icon_normal * 3
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                background: Item{
                    width: parent.implicitWidth
                    height: parent.implicitHeight
                }

                Row {
                    Utils.IconToolButton {
                        id: playlist_btn

                        size: VLCStyle.icon_normal
                        text: VLCIcons.playlist

                        KeyNavigation.left: buttonView

                        onClicked: root.toogleMenu()
                    }

                    Utils.IconToolButton {
                        id: menu_selector

                        size: VLCStyle.icon_normal
                        text: VLCIcons.menu

                        KeyNavigation.left: playlist_btn

                        onClicked: mainMenu.openBelow(this)

                        Menus.MainDropdownMenu {
                            id: mainMenu
                            onClosed: menu_selector.forceActiveFocus()
                        }
                    }
                }
            }
        }
    }

    Keys.priority: Keys.AfterItem
    Keys.onPressed: {
        if (!event.accepted)
            defaultKeyAction(event, 0)
    }
}
