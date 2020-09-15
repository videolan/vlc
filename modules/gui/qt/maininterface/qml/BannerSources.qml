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
import QtGraphicalEffects 1.0
import org.videolan.vlc 0.1
import QtQml.Models 2.11

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///menus/" as Menus


Widgets.NavigableFocusScope {
    id: root

    height: VLCStyle.globalToolbar_height + VLCStyle.localToolbar_height + VLCStyle.applicationVerticalMargin

    property int selectedIndex: 0
    property int subSelectedIndex: 0

    signal itemClicked(int index)
    signal subItemClicked(int index)

    property alias sortModel: sortControl.model
    property var contentModel

    property alias model: pLBannerSources.model
    property alias subTabModel: localMenuGroup.model
    signal toogleMenu()

    property var extraLocalActions: undefined

    // Triggered when the toogleView button is selected
    function toggleView () {
        mainInterface.gridView = !mainInterface.gridView
    }

    function search() {
        if (searchBox.visible)
            searchBox.expanded = true
    }

    DropShadow {
        id: primaryShadow

        anchors.fill: pLBannerSources
        source: pLBannerSources
        horizontalOffset: 0
        verticalOffset: VLCStyle.dp(1, VLCStyle.scale)
        radius: VLCStyle.dp(9, VLCStyle.scale)
        spread: 0
        samples: ( radius * 2 ) + 1
        color: Qt.rgba(0, 0, 0, .22)
    }

    DropShadow {
        id: secondaryShadow

        anchors.fill: pLBannerSources
        source: pLBannerSources
        horizontalOffset: 0
        verticalOffset: VLCStyle.dp(0, VLCStyle.scale)
        radius: VLCStyle.dp(2, VLCStyle.scale)
        spread: 0
        samples: ( radius * 2 ) + 1
        color: Qt.rgba(0, 0, 0, .18)
    }

    Rectangle {
        id: pLBannerSources

        anchors.fill: parent

        color: VLCStyle.colors.banner
        property alias model: globalMenuGroup.model

        Column {

            id: col
            anchors {
                fill: parent
                leftMargin: VLCStyle.applicationHorizontalMargin
                rightMargin: VLCStyle.applicationHorizontalMargin
                topMargin: VLCStyle.applicationVerticalMargin
            }

            Item {
                id: globalToolbar
                width: parent.width
                height: VLCStyle.globalToolbar_height

                RowLayout {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: VLCStyle.margin_normal
                    spacing: VLCStyle.margin_xxxsmall

                    Image {
                        sourceSize.width: VLCStyle.icon_small
                        sourceSize.height: VLCStyle.icon_small
                        source: "qrc:///logo/cone.svg"
                        enabled: false
                    }

                    Widgets.SubtitleLabel {
                        text: "VLC"
                        font.pixelSize: VLCStyle.fontSize_xxlarge
                    }

                }

                /* Button for the sources */
                Widgets.NavigableRow {
                    id: globalMenuGroup

                    anchors {
                        top: parent.top
                        bottom: parent.bottom
                        horizontalCenter: parent.horizontalCenter
                    }

                    focus: true

                    navigationParent: root
                    navigationDownItem: localMenuGroup.visible ?  localMenuGroup : playlistGroup

                    delegate: Widgets.BannerTabButton {
                        iconTxt: model.icon
                        selected: model.index === selectedIndex
                        onClicked: root.itemClicked(model.index)
                        height: globalMenuGroup.height
                    }
                }
            }

            Rectangle {
                id: localToolbar

                color: VLCStyle.colors.bg
                width: parent.width
                height: VLCStyle.localToolbar_height

                Widgets.NavigableRow {
                    id: localContextGroup
                    anchors {
                        top: parent.top
                        left: parent.left
                        bottom: parent.bottom
                    }

                    model: ObjectModel {
                        id: localContextModel

                        property int countExtra: 0

                        Widgets.IconToolButton {
                             id: history_back
                             size: VLCStyle.banner_icon_size
                             iconText: VLCIcons.topbar_previous
                             text: i18n.qtr("Previous")
                             height: localToolbar.height
                             colorDisabled: VLCStyle.colors.textDisabled
                             onClicked: history.previous()
                             enabled: !history.previousEmpty
                         }

                        Widgets.IconToolButton {
                            id: list_grid_btn
                            size: VLCStyle.banner_icon_size
                            iconText: mainInterface.gridView ? VLCIcons.list : VLCIcons.grid
                            text: i18n.qtr("List/Grid")
                            height: localToolbar.height
                            onClicked: mainInterface.gridView = !mainInterface.gridView
                            enabled: true
                        }

                        Widgets.SortControl {
                            id: sortControl

                            textRole: "text"
                            listWidth: VLCStyle.widthSortBox
                            height: localToolbar.height

                            popupAlignment: Qt.AlignLeft | Qt.AlignBottom

                            visible: root.sortModel !== undefined && root.sortModel.length > 1
                            enabled: visible

                            onSortSelected: {
                                if (contentModel !== undefined) {
                                    contentModel.sortCriteria = modelData.criteria
                                }
                            }
                        }
                    }

                    Connections {
                        target: root
                        onExtraLocalActionsChanged : {
                            for (var i = 0; i < localContextModel.countExtra; i++) {
                                localContextModel.remove(localContextModel.count - localContextModel.countExtra, localContextModel.countExtra)
                            }

                            if (root.extraLocalActions && root.extraLocalActions instanceof ObjectModel) {
                                for (i = 0; i < root.extraLocalActions.count; i++)
                                    localContextModel.append(root.extraLocalActions.get(i))
                                localContextModel.countExtra = root.extraLocalActions.count
                            } else {
                                localContextModel.countExtra = 0
                            }
                        }
                    }

                    navigationParent: root
                    navigationRightItem: localMenuGroup
                    navigationUpItem: globalMenuGroup
                }

                Widgets.NavigableRow {
                    id: localMenuGroup
                    anchors {
                        top: parent.top
                        bottom: parent.bottom
                        horizontalCenter: parent.horizontalCenter
                    }

                    visible: !!model
                    enabled: !!model
                    onVisibleChanged: {
                        //reset the focus on the global group when the local group is hidden,
                        //this avoids losing the focus if the subview changes
                        if (!visible && localMenuGroup.focus) {
                                        localMenuGroup.focus = false
                                        globalMenuGroup.focus = true
                        }
                    }

                    delegate: Widgets.BannerTabButton {
                        text: model.displayText
                        selected: model.index === subSelectedIndex
                        onClicked:  root.subItemClicked(model.index)
                        height: localMenuGroup.height
                        color: VLCStyle.colors.bg
                        colorSelected: VLCStyle.colors.bg
                    }

                    navigationParent: root
                    navigationLeftItem: localContextGroup.enabled ? localContextGroup : undefined
                    navigationRightItem: playlistGroup.enabled ? playlistGroup : undefined
                    navigationUpItem:  globalMenuGroup
                }

                Widgets.NavigableRow {
                    id: playlistGroup
                    anchors {
                        top: parent.top
                        right: parent.right
                        bottom: parent.bottom
                    }
                    spacing: VLCStyle.margin_xxxsmall

                    model: ObjectModel {

                        Widgets.SearchBox {
                            id: searchBox
                            contentModel: root.contentModel
                            visible: root.contentModel !== undefined
                            enabled: visible
                            height: playlistGroup.height
                        }

                        Widgets.IconToolButton {
                            id: menu_selector

                            size: VLCStyle.banner_icon_size
                            iconText: VLCIcons.ellipsis
                            text: i18n.qtr("Menu")
                            height: playlistGroup.height

                            onClicked: mainMenu.openBelow(this)

                            Menus.MainDropdownMenu {
                                id: mainMenu
                                onClosed: {
                                    if (mainMenu.activeFocus)
                                        menu_selector.forceActiveFocus()
                                }
                            }
                        }

                        Widgets.IconToolButton {
                            id: playlist_btn

                            size: VLCStyle.banner_icon_size
                            iconText: VLCIcons.playlist
                            text: i18n.qtr("Playlist")
                            height: playlistGroup.height

                            onClicked:  mainInterface.playlistVisible = !mainInterface.playlistVisible
                            color: mainInterface.playlistVisible && !playlist_btn.backgroundVisible ? VLCStyle.colors.accent : VLCStyle.colors.buttonText
                        }
                    }

                    navigationParent: root
                    navigationLeftItem: localMenuGroup
                    navigationUpItem: globalMenuGroup
                }
            }
        }

        Keys.priority: Keys.AfterItem
        Keys.onPressed: {
            if (!event.accepted)
                defaultKeyAction(event, 0)
        }
    }
}
