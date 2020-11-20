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
import "qrc:///util/KeyHelper.js" as KeyHelper

Widgets.NavigableFocusScope {
    id: root

    height: VLCStyle.applicationVerticalMargin
            + (menubar.visible ? menubar.height : 0)
            + VLCStyle.globalToolbar_height
            + VLCStyle.localToolbar_height


    property int selectedIndex: 0
    property int subSelectedIndex: 0

    signal itemClicked(int index)

    property alias sortModel: sortControl.model
    property var contentModel

    property alias model: pLBannerSources.model
    signal toogleMenu()

    property var extraLocalActions: undefined
    property alias localMenuDelegate: localMenuGroup.sourceComponent

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
                topMargin: VLCStyle.applicationVerticalMargin
            }

            Item {
                id: globalToolbar
                width: parent.width
                height: VLCStyle.globalToolbar_height
                    + (menubar.visible ? menubar.height : 0)
                anchors.rightMargin: VLCStyle.applicationHorizontalMargin

                property bool colapseTabButtons: globalToolbar.width  > (Math.max(globalToolbarLeft.width, globalToolbarRight.width) + VLCStyle.applicationHorizontalMargin)* 2
                                                 + globalMenuGroup.model.count * VLCStyle.bannerTabButton_width_large

                //drag and dbl click the titlebar in CSD mode
                Loader {
                    anchors.fill: parent
                    active: mainInterface.clientSideDecoration
                    source: "qrc:///widgets/CSDTitlebarTapNDrapHandler.qml"
                }

                Column {
                    anchors.fill: parent
                    anchors.leftMargin: VLCStyle.applicationHorizontalMargin
                    anchors.rightMargin: VLCStyle.applicationHorizontalMargin

                    Menus.Menubar {
                        id: menubar
                        width: parent.width
                        height: implicitHeight
                        visible: mainInterface.hasToolbarMenu
                    }

                    Item {
                        width: parent.width
                        height: VLCStyle.globalToolbar_height

                        RowLayout {
                            id: globalToolbarLeft
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            spacing: VLCStyle.margin_xxxsmall

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

                            Image {
                                sourceSize.width: VLCStyle.icon_small
                                sourceSize.height: VLCStyle.icon_small
                                source: "qrc:///logo/cone.svg"
                                enabled: false
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
                                showText: globalToolbar.colapseTabButtons
                                selected: model.index === selectedIndex
                                onClicked: root.itemClicked(model.index)
                                height: globalMenuGroup.height
                            }
                        }
                    }
                }

                Loader {
                    id: globalToolbarRight
                    anchors {
                        top: parent.top
                        right: parent.right
                        rightMargin: VLCStyle.applicationHorizontalMargin
                    }
                    height: VLCStyle.globalToolbar_height
                    active: mainInterface.clientSideDecoration
                    source: "qrc:///widgets/CSDWindowButtonSet.qml"
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
                        leftMargin: VLCStyle.applicationHorizontalMargin
                    }

                    model: ObjectModel {
                        id: localContextModel

                        property int countExtra: 0

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
                            criteriaRole: "criteria"
                            listWidth: VLCStyle.widthSortBox
                            height: localToolbar.height

                            popupAlignment: Qt.AlignLeft | Qt.AlignBottom

                            visible: root.sortModel !== undefined && root.sortModel.length > 1
                            enabled: visible

                            onSortSelected: {
                                if (contentModel !== undefined) {
                                    contentModel.sortCriteria = modelData[criteriaRole]
                                }
                            }

                            onSortOrderSelected: {
                                if (contentModel !== undefined) {
                                    contentModel.sortOrder = order
                                }
                            }

                            sortKey: contentModel.sortCriteria
                            sortOrder: contentModel.sortOrder
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
                    navigationRightItem: localMenuGroup.visible ? localMenuGroup : playlistGroup
                    navigationUpItem: globalMenuGroup
                }

                Loader {
                    id: localMenuGroup

                    anchors.centerIn: parent
                    focus: !!item && item.focus && item.visible
                    visible: !!item

                    onVisibleChanged: {
                        //reset the focus on the global group when the local group is hidden,
                        //this avoids losing the focus if the subview changes
                        if (!visible && localMenuGroup.focus) {
                            localMenuGroup.focus = false
                            globalMenuGroup.focus = true
                        }
                    }

                    onItemChanged: {
                        if (!item)
                            return
                        if (item.hasOwnProperty("navigationParent")) {
                            item.navigationParent = root
                            item.navigationLeftItem = localContextGroup.enabled ? localContextGroup : undefined
                            item.navigationRightItem = playlistGroup.enabled ? playlistGroup : undefined
                            item.navigationUpItem = globalMenuGroup
                        } else {
                            item.KeyNavigation.left = localContextGroup.enabled ? localContextGroup : undefined
                            item.KeyNavigation.right = playlistGroup.enabled ? playlistGroup : undefined
                            item.KeyNavigation.up = globalMenuGroup
                            item.Keys.pressed.connect(function (event) {
                                if (event.accepted)
                                    return
                                if (KeyHelper.matchDown(event)) {
                                    root.navigationDown()
                                    event.accepted = true
                                }
                            })
                        }
                    }

                }

                Widgets.NavigableRow {
                    id: playlistGroup
                    anchors {
                        top: parent.top
                        right: parent.right
                        bottom: parent.bottom
                        rightMargin: VLCStyle.applicationHorizontalMargin
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
                            id: playlist_btn

                            size: VLCStyle.banner_icon_size
                            iconText: VLCIcons.playlist
                            text: i18n.qtr("Playlist")
                            height: playlistGroup.height

                            onClicked:  mainInterface.playlistVisible = !mainInterface.playlistVisible
                            color: mainInterface.playlistVisible && !playlist_btn.backgroundVisible ? VLCStyle.colors.accent : VLCStyle.colors.buttonText
                        }

                        Widgets.IconToolButton {
                            id: menu_selector

                            size: VLCStyle.banner_icon_size
                            iconText: VLCIcons.ellipsis
                            text: i18n.qtr("Menu")
                            height: playlistGroup.height

                            onClicked: contextMenu.popup(this.mapToGlobal(0, height))

                            QmlGlobalMenu {
                                id: contextMenu
                                ctx: mainctx
                            }
                        }
                    }

                    navigationParent: root
                    navigationLeftItem: localMenuGroup.visible ? localMenuGroup : localContextGroup
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
