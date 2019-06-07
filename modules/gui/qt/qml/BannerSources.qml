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

    height: pLBannerSources.height

    property int selectedIndex: 0
    property int subSelectedIndex: 0

    signal itemClicked(int index)
    signal subItemClicked(int index)

    property alias sortModel: combo.model
    property var contentModel

    property alias model: pLBannerSources.model
    property alias subTabModel: model_music_id.model
    signal toogleMenu()

    // Triggered when the toogleView button is selected
    function toggleView () {
        medialib.gridView = !medialib.gridView
    }

    Rectangle {
        id: pLBannerSources

        anchors {
            left: parent.left
            right: parent.right
        }
        height: col.height

        color: VLCStyle.colors.banner
        property alias model: buttonView.model

        Column
        {
            id: col
            anchors {
                left: parent.left
                right: parent.right
            }

            spacing: VLCStyle.margin_xxsmall

            /* Button for the sources */
            TabBar {
                id: buttonView

                anchors {
                    horizontalCenter: parent.horizontalCenter
                }

                Layout.preferredHeight: VLCStyle.icon_normal
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                spacing: VLCStyle.margin_small

                focus: true

                // KeyNavigation states
                states: [
                    State {
                        name: "no_history"
                        when: history.nextEmpty && history.previousEmpty
                        PropertyChanges {
                            target: buttonView
                            KeyNavigation.left: searchBox
                            KeyNavigation.down: searchBox
                        }
                    },
                    State {
                        name: "has_previous_history"
                        when: !history.previousEmpty
                        PropertyChanges {
                            target: buttonView
                            KeyNavigation.left: history_back
                            KeyNavigation.down: history_back
                        }
                    },
                    State {
                        name: "has_only_next_history"
                        when: !history.nextEmpty && history.previousEmpty
                        PropertyChanges {
                            target: buttonView
                            KeyNavigation.left: history_next
                            KeyNavigation.down: history_next
                        }
                    }
                ]

                Component.onCompleted: {
                    buttonView.contentItem.focus = true
                }

                background: Rectangle {
                    color: "transparent"
                }

                property alias model: sourcesButtons.model
                /* Repeater to display each button */
                Repeater {
                    id: sourcesButtons

                    TabButton {
                        id: control
                        text: model.displayText

                        padding: 0
                        width: contentItem.implicitWidth

                        onClicked: {
                            root.itemClicked(model.index)
                        }

                        font.pixelSize: VLCStyle.fontSize_normal

                        background: Rectangle {
                            height: parent.height
                            width: parent.contentItem.width
                            //color: (control.hovered || control.activeFocus) ? VLCStyle.colors.bgHover : VLCStyle.colors.banner
                            color: VLCStyle.colors.banner
                        }

                        contentItem: Item {
                            implicitWidth: tabRow.width
                            implicitHeight: tabRow.height

                            Rectangle {
                                anchors.fill: tabRow
                                visible: control.activeFocus || control.hovered
                                color: VLCStyle.colors.accent
                            }

                            Row {
                                id: tabRow
                                padding: VLCStyle.margin_xxsmall
                                spacing: VLCStyle.margin_xxsmall

                                Label {
                                    id: icon
                                    anchors {
                                        verticalCenter: parent.verticalCenter
                                    }
                                    color: VLCStyle.colors.buttonText

                                    font.pixelSize: VLCStyle.icon_topbar
                                    font.family: VLCIcons.fontFamily
                                    horizontalAlignment: Text.AlignHCenter
                                    leftPadding: VLCStyle.margin_xsmall
                                    rightPadding: VLCStyle.margin_xsmall

                                    text: model.icon
                                }

                                Label {
                                    text: control.text
                                    font: control.font
                                    color: VLCStyle.colors.text
                                    padding: VLCStyle.margin_xxsmall

                                    anchors {
                                        bottom: parent.bottom
                                    }
                                }
                            }

                            Rectangle {
                                anchors {
                                    left: tabRow.left
                                    right: tabRow.right
                                    bottom: tabRow.bottom
                                }
                                height: 2
                                visible: root.selectedIndex === model.index
                                color: "transparent"
                                border.color: VLCStyle.colors.accent
                            }
                        }
                    }
                }
            }

            RowLayout {
                width: parent.width
                spacing: 0

                Utils.IconToolButton {
                    id: history_back
                    size: VLCStyle.icon_normal
                    Layout.minimumWidth: width
                    text: VLCIcons.topbar_previous
                    KeyNavigation.right: history_next
                    onClicked: history.pop(History.Go)
                }

                Utils.IconToolButton {
                    id: history_next
                    size: VLCStyle.icon_normal
                    Layout.minimumWidth: width
                    text: VLCIcons.topbar_next
                    KeyNavigation.right: bar
                    KeyNavigation.up: buttonView
                }

                TabBar {
                    id: bar

                    visible: model_music_id.model !== undefined
                    enabled: model_music_id.model !== undefined

                    Component.onCompleted: {
                        bar.contentItem.focus= true
                    }

                    /* List of sub-sources for Music */
                    Repeater {
                        id: model_music_id

                        //Column {
                        TabButton {
                            id: control
                            text: model.displayText
                            font.pixelSize: VLCStyle.fontSize_normal
                            background: Rectangle {
                                color: VLCStyle.colors.banner
                            }
                            contentItem: Item {
                                implicitWidth: subSectionName.width
                                implicitHeight: subSectionName.height

                                Rectangle {
                                    anchors.fill: subSectionName
                                    visible: control.activeFocus || control.hovered
                                    color: VLCStyle.colors.accent
                                }

                                Label {
                                    id: subSectionName
                                    padding: VLCStyle.margin_xxsmall
                                    text: control.text
                                    font: control.font
                                    color: VLCStyle.colors.text
                                }

                                Rectangle {
                                    anchors {
                                        left: subSectionName.left
                                        right: subSectionName.right
                                        bottom: subSectionName.bottom
                                    }
                                    height: 2
                                    visible: root.subSelectedIndex === model.index

                                    color: VLCStyle.colors.accent
                                }
                            }
                            onClicked: {
                                root.subItemClicked(model.index)
                            }
                            activeFocusOnTab: true
                        }
                    }

                    KeyNavigation.right: searchBox
                    KeyNavigation.up: buttonView
                }

                /* Spacer */
                Item {
                    Layout.fillWidth: true
                }

                TextField {
                    Layout.preferredWidth: VLCStyle.widthSearchInput
                    Layout.preferredHeight: VLCStyle.heightInput
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight

                    id: searchBox
                    font.pixelSize: VLCStyle.fontSize_normal

                    color: VLCStyle.colors.buttonText
                    placeholderText: qsTr("filter")
                    hoverEnabled: true

                    background: Rectangle {
                        color: VLCStyle.colors.button
                        border.color: {
                            if ( searchBox.text.length < 3 && searchBox.text.length !== 0 )
                                return VLCStyle.colors.alert
                            else if ( searchBox.hovered || searchBox.activeFocus )
                                return VLCStyle.colors.accent
                            else
                                return VLCStyle.colors.buttonBorder
                       }
                    }

                    onTextChanged: {
                        if (contentModel !== undefined)
                            contentModel.searchPattern = text;
                    }

                    KeyNavigation.right: combo
                    KeyNavigation.up: buttonView
                }

                /* Selector to choose a specific sorting operation */
                Utils.ComboBoxExt {
                    id: combo

                    //Layout.fillHeight: true
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                    Layout.preferredWidth: VLCStyle.widthSortBox
                    Layout.preferredHeight: VLCStyle.heightInput
                    textRole: "text"
                    onCurrentIndexChanged: {
                        if (model !== undefined && contentModel !== undefined) {
                            var sorting = model.get(currentIndex);
                            contentModel.sortCriteria = sorting.criteria
                        }
                    }

                    KeyNavigation.right: playlist_btn
                    KeyNavigation.up: buttonView
                }

                ToolBar {
                    id: tools
                    Layout.minimumWidth: width
                    Layout.preferredHeight: VLCStyle.icon_normal
                    //Layout.preferredWidth: VLCStyle.icon_normal * 3
                    Layout.alignment: Qt.AlignRight
                    background: Item{
                        width: parent.implicitWidth
                        height: parent.implicitHeight
                    }

                    Row {
                        Utils.IconToolButton {
                            id: playlist_btn

                            size: VLCStyle.icon_normal
                            text: VLCIcons.playlist

                            onClicked: root.toogleMenu()

                            KeyNavigation.right: menu_selector
                            KeyNavigation.up: buttonView
                        }

                        Utils.IconToolButton {
                            id: menu_selector

                            size: VLCStyle.icon_normal
                            text: VLCIcons.menu

                            onClicked: mainMenu.openBelow(this)

                            Menus.MainDropdownMenu {
                                id: mainMenu
                                onClosed: menu_selector.forceActiveFocus()
                            }

                            KeyNavigation.up: buttonView
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
