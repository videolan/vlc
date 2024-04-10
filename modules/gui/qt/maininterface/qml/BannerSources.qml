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
import QtQml.Models
import QtQuick.Window

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///playlist/" as PL
import "qrc:///widgets/" as Widgets
import "qrc:///menus/" as Menus
import "qrc:///util/Helpers.js" as Helpers

T.ToolBar {
    id: root


    property int selectedIndex: 0
    property alias sortMenu: sortControl.menu
    property alias model: pLBannerSources.model
    property alias localMenuDelegate: localMenuGroup.sourceComponent

    // For now, used for d&d functionality
    // Not strictly necessary to set
    property PL.PlaylistListView plListView: null

    property bool _showCSD: MainCtx.clientSideDecoration && !(MainCtx.intfMainWindow.visibility === Window.FullScreen)

    signal itemClicked(int index)
    signal toogleMenu()


    height: VLCStyle.applicationVerticalMargin
            + (menubar.visible ? menubar.height : 0)
            + VLCStyle.globalToolbar_height
            + VLCStyle.localToolbar_height

    hoverEnabled: true

    // Triggered when the toogleView button is selected
    function toggleView () {
        MainCtx.gridView = !MainCtx.gridView
    }

    ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    Binding {
        target: MainCtx.search
        property: "pattern"
        value: searchBox.searchPattern
    }

    Connections {
        target: MainCtx.search
        function onAskShow() {
            searchBox.expandAndFocus()
        }
    }

    background: Widgets.AcrylicBackground {
        tintColor: theme.bg.primary
        alternativeColor: theme.bg.secondary
    }

    contentItem:  Column {
        id: pLBannerSources

        property alias model: globalMenuGroup.model

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
                    active: root._showCSD
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
                        visible: MainCtx.hasToolbarMenu
                        enabled: visible
                    }

                    Item {
                        width: parent.width
                        height: VLCStyle.globalToolbar_height

                        Accessible.role: Accessible.ToolBar

                        RowLayout {
                            id: globalToolbarLeft
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: VLCStyle.margin_xsmall
                            spacing: VLCStyle.margin_normal

                            Widgets.IconToolButton {
                                 id: history_back
                                 font.pixelSize: VLCStyle.icon_banner
                                 text: VLCIcons.back
                                 description: qsTr("Previous")
                                 height: VLCStyle.bannerButton_height
                                 width: VLCStyle.bannerButton_width
                                 onClicked: History.previous()
                                 enabled: !History.previousEmpty

                                 onEnabledChanged: {
                                    if (!enabled && focus)
                                        globalMenuGroup.focus = true
                                 }

                                 Navigation.parentItem: root
                                 Navigation.rightItem: globalMenuGroup
                                 Navigation.downItem: localMenuGroup.visible ? localMenuGroup : localToolbar
                            }

                            Widgets.BannerCone {
                                color: theme.accent
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

                            //indexFocus: root.selectedIndex

                            Accessible.role: Accessible.PageTabList

                            Navigation.parentItem: root
                            Navigation.leftItem: history_back
                            Navigation.downItem: localMenuView.visible ?  localMenuView : localToolbar

                            delegate: Widgets.BannerTabButton {
                                iconTxt: model.icon
                                showText: globalToolbar.colapseTabButtons
                                selected: History.match(History.viewPath, ["mc", model.name])
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
                    active: root._showCSD
                    source: VLCStyle.palette.hasCSDImage
                              ? "qrc:///widgets/CSDThemeButtonSet.qml"
                              : "qrc:///widgets/CSDWindowButtonSet.qml"
                }
            }

            //use a raw control, we don't want platforms customisation
            T.ToolBar {
                id: localToolbar

                width: parent.width
                height: VLCStyle.localToolbar_height

                background: Rectangle {
                    id: localToolbarBg

                    color: theme.bg.secondary
                    Rectangle {
                        anchors.left : parent.left
                        anchors.right: parent.right
                        anchors.top  : parent.bottom

                        height: VLCStyle.border

                        color: theme.border
                    }
                }


                contentItem: Item {
                    id: localToolbarContent

                    Widgets.NavigableRow {
                        id: localContextGroup
                        anchors {
                            verticalCenter: parent.verticalCenter
                            left: parent.left
                            leftMargin: VLCStyle.applicationHorizontalMargin + VLCStyle.margin_xsmall
                        }

                        spacing: VLCStyle.margin_normal
                        enabled: list_grid_btn.visible || sortControl.visible

                        onEnabledChanged: {
                            if (!enabled && focus) {
                                if (localMenuView.enabled)
                                    localMenuView.focus = true
                                else
                                    playlistGroup.focus = true
                            }
                        }

                        model: ObjectModel {
                            Widgets.IconToolButton {
                                id: list_grid_btn

                                visible: MainCtx.hasGridListMode
                                width: VLCStyle.bannerButton_width
                                height: VLCStyle.bannerButton_height
                                font.pixelSize: VLCStyle.icon_banner
                                text: MainCtx.gridView ? VLCIcons.list : VLCIcons.grid
                                description: qsTr("List/Grid")
                                onClicked: MainCtx.gridView = !MainCtx.gridView
                                enabled: true
                            }

                            Widgets.SortControl {
                                id: sortControl

                                width: VLCStyle.bannerButton_width
                                height: VLCStyle.bannerButton_height

                                font.pixelSize: VLCStyle.icon_banner

                                visible: MainCtx.sort.available

                                enabled: visible

                                model: MainCtx.sort.model

                                sortKey:  MainCtx.sort.criteria
                                sortOrder: MainCtx.sort.order

                                onSortSelected: {
                                    MainCtx.sort.criteria = key
                                }
                                onSortOrderSelected: MainCtx.sort.order = type
                            }
                        }

                        Navigation.parentItem: root
                        Navigation.rightItem: localMenuView
                        Navigation.upItem: history_back.enabled ? history_back : globalMenuGroup
                    }

                    T.Pane {
                        id: localMenuView

                        readonly property int _availableWidth: parent.width
                                                              - (localContextGroup.width + playlistGroup.width)
                                                              - (VLCStyle.applicationHorizontalMargin * 2)
                                                              - (VLCStyle.margin_xsmall * 2)
                                                              - (VLCStyle.margin_xxsmall * 2)

                        readonly property bool _alignHCenter: ((localToolbarContent.width - contentItem.contentWidth) / 2) + contentItem.contentWidth
                                                              < playlistGroup.x

                        width: Math.min(contentItem.contentWidth, _availableWidth)
                        height: VLCStyle.localToolbar_height
                        enabled: localMenuGroup.sourceComponent !== null
                        visible: enabled

                        anchors.right: playlistGroup.left
                        anchors.rightMargin: VLCStyle.margin_xxsmall // only applied when right aligned

                        onEnabledChanged: {
                            if (!enabled && focus) {
                                playlistGroup.focus = true
                            }
                        }

                        on_AlignHCenterChanged: {
                            if (_alignHCenter) {
                                anchors.right = undefined
                                anchors.horizontalCenter = parent.horizontalCenter
                            } else {
                                anchors.horizontalCenter = undefined
                                anchors.right = playlistGroup.left
                            }
                        }

                        Navigation.parentItem: root
                        Navigation.leftItem: localContextGroup
                        Navigation.rightItem: playlistGroup
                        Navigation.upItem: globalMenuGroup

                        contentItem: Flickable {

                            clip: contentWidth > width

                            contentWidth: localMenuGroup.width
                            contentHeight: VLCStyle.localToolbar_height // don't allow vertical flickering

                            ScrollBar.horizontal: ScrollBar {
                                y: localMenuView.height - height
                                width: localMenuView.availableWidth
                                policy: ScrollBar.AsNeeded
                            }

                            Loader {
                                id: localMenuGroup

                                focus: true

                                enabled: status === Loader.Ready
                                y: status === Loader.Ready ? (VLCStyle.localToolbar_height - item.height) / 2 : 0
                                width: !!item
                                       ? Helpers.clamp(localMenuView._availableWidth,
                                                       localMenuGroup.item.minimumWidth || localMenuGroup.item.implicitWidth,
                                                       localMenuGroup.item.maximumWidth || localMenuGroup.item.implicitWidth)
                                       : 0

                                onItemChanged: {
                                    if (!item)
                                        return
                                    item.Navigation.parentItem = localMenuView
                                }
                            }
                        }
                    }

                    Widgets.NavigableRow {
                        id: playlistGroup
                        anchors {
                            verticalCenter: parent.verticalCenter
                            right: parent.right
                            rightMargin: VLCStyle.applicationHorizontalMargin + VLCStyle.margin_xsmall
                        }
                        spacing: VLCStyle.margin_normal

                        model: ObjectModel {

                            Widgets.SearchBox {
                                id: searchBox

                                // set max width so that search field not overflows with small screens
                                // assumes all other sibling is a button of 'VLCStyle.bannerButton_width' width
                                maxSearchFieldWidth: root.width
                                                     - (VLCStyle.bannerButton_width * playlistGroup.count)
                                                     - (playlistGroup.spacing * (playlistGroup.count - 1))
                                                     - playlistGroup.anchors.rightMargin
                                                     - VLCStyle.margin_small // padding to left

                                visible: MainCtx.search.available
                                height: VLCStyle.bannerButton_height
                                buttonWidth: VLCStyle.bannerButton_width
                            }

                            Widgets.IconToolButton {
                                id: playlist_btn

                                checked: MainCtx.playlistVisible

                                font.pixelSize: VLCStyle.icon_banner
                                text: VLCIcons.playlist
                                description: qsTr("Playlist")
                                width: VLCStyle.bannerButton_width
                                height: VLCStyle.bannerButton_height
                                highlighted: MainCtx.playlistVisible

                                onClicked:  MainCtx.playlistVisible = !MainCtx.playlistVisible

                                DropArea {
                                    anchors.fill: parent

                                    onContainsDragChanged: {
                                        if (containsDrag) {
                                            _timer.restart()

                                            if (plListView)
                                                MainCtx.setCursor(Qt.DragCopyCursor)
                                        } else {
                                            _timer.stop()

                                            if (plListView)
                                                MainCtx.restoreCursor()
                                        }
                                    }

                                    onEntered: (drag) => {
                                        if (drag.hasUrls || Helpers.isValidInstanceOf(drag.source, Widgets.DragItem)) {
                                            drag.accept() // Not actually necessary, as it is accepted by default
                                        } else {
                                            drag.accepted = false
                                        }
                                    }

                                    onDropped: (drop) => {
                                        if (plListView)
                                            plListView.acceptDrop(plListView.model.count, drop)
                                    }

                                    Timer {
                                        id: _timer
                                        interval: VLCStyle.duration_humanMoment

                                        onTriggered: {
                                            MainCtx.playlistVisible = true
                                        }
                                    }
                                }
                            }

                            Widgets.IconToolButton {
                                id: menu_selector

                                visible: !MainCtx.hasToolbarMenu
                                font.pixelSize: VLCStyle.icon_banner
                                text: VLCIcons.more
                                description: qsTr("Menu")
                                width: VLCStyle.bannerButton_width
                                height: VLCStyle.bannerButton_height
                                checked: contextMenu.shown

                                onClicked: contextMenu.popup(this.mapToGlobal(0, height))

                                QmlGlobalMenu {
                                    id: contextMenu
                                    ctx: MainCtx
                                }
                            }
                        }

                        Navigation.parentItem: root
                        Navigation.leftItem: localMenuView
                        Navigation.upItem: globalMenuGroup
                    }
                }
            }


        Keys.priority: Keys.AfterItem
        Keys.onPressed: (event) => root.Navigation.defaultKeyAction(event)
    }
}
