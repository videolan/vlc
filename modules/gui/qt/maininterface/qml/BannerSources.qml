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
import QtQuick.Layouts 1.11
import QtGraphicalEffects 1.0
import QtQml.Models 2.11
import QtQuick.Window 2.11

import org.videolan.vlc 0.1
import org.videolan.compat 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///menus/" as Menus
import "qrc:///util/Helpers.js" as Helpers

FocusScope {
    id: root

    height: VLCStyle.applicationVerticalMargin
            + (menubar.visible ? menubar.height : 0)
            + VLCStyle.globalToolbar_height
            + VLCStyle.localToolbar_height

    property int selectedIndex: 0
    property int subSelectedIndex: 0
    property alias sortMenu: sortControl.menu
    property alias sortModel: sortControl.model
    property var contentModel
    property alias isViewMultiView: list_grid_btn.visible
    property alias model: pLBannerSources.model
    property var extraLocalActions: undefined
    property alias localMenuDelegate: localMenuGroup.sourceComponent

    property bool _showCSD: MainCtx.clientSideDecoration && !(MainCtx.intfMainWindow.visibility === Window.FullScreen)

    signal itemClicked(int index)
    signal toogleMenu()


    // Triggered when the toogleView button is selected
    function toggleView () {
        MainCtx.gridView = !MainCtx.gridView
    }

    function search() {
        searchBox.state = "expanded"
    }

    BindingCompat {
        property: "searchPattern"
        value: searchBox.searchPattern
        when: !!contentModel

        Component.onCompleted: {
            // restoreMode is only available in Qt >= 5.14
            if ("restoreMode" in this)
                this.restoreMode = Binding.RestoreBindingOrValue

            target = Qt.binding(function() { return !!contentModel ? contentModel : null })
        }
    }

    Widgets.AcrylicBackground {
        alternativeColor: VLCStyle.colors.topBanner
        anchors.fill: parent
    }


    MouseArea {
        // don't tranfer mouse to underlying components (#26274)
        anchors.fill: parent
        hoverEnabled: true
        preventStealing: true
    }

    Item {
        id: pLBannerSources

        property alias model: globalMenuGroup.model

        anchors.fill: parent

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
                    }

                    Item {
                        width: parent.width
                        height: VLCStyle.globalToolbar_height

                        RowLayout {
                            id: globalToolbarLeft
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: VLCStyle.margin_xsmall
                            spacing: VLCStyle.margin_xxxsmall

                            Widgets.IconToolButton {
                                 id: history_back
                                 size: VLCStyle.icon_banner
                                 iconText: VLCIcons.back
                                 text: I18n.qtr("Previous")
                                 height: VLCStyle.bannerButton_height
                                 width: VLCStyle.bannerButton_width
                                 colorDisabled: VLCStyle.colors.textDisabled
                                 onClicked: History.previous()
                                 enabled: !History.previousEmpty

                                 Navigation.parentItem: root
                                 Navigation.rightItem: globalMenuGroup
                                 Navigation.downItem: localMenuGroup.visible ? localMenuGroup : localToolbarBg
                             }

                            Image {
                                sourceSize.width: VLCStyle.icon_normal
                                sourceSize.height: VLCStyle.icon_normal
                                source: SVGColorImage.colorize("qrc:///misc/cone.svg")
                                                .accent(VLCStyle.colors.accent)
                                                .uri()
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

                            indexFocus: selectedIndex

                            Navigation.parentItem: root
                            Navigation.leftItem: history_back.enabled ? history_back : null
                            Navigation.downItem: localMenuGroup.visible ?  localMenuGroup : playlistGroup

                            delegate: Widgets.BannerTabButton {
                                iconTxt: model.icon
                                color: VLCStyle.colors.setColorAlpha(VLCStyle.colors.buttonHover, 0)
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
                    active: root._showCSD
                    source: VLCStyle.theme.hasCSDImage
                              ? "qrc:///widgets/CSDThemeButtonSet.qml"
                              : "qrc:///widgets/CSDWindowButtonSet.qml"
                }
            }

            //use a raw control, we don't want platforms customisation
            T.Control {
                id: localToolbar

                width: parent.width
                height: VLCStyle.localToolbar_height

                onActiveFocusChanged: {
                    if (activeFocus) {
                        // sometimes when view changes, one of the "focusable" object will become disabled
                        // but because of focus chainning, FocusScope still tries to force active focus on the object
                        // but that will fail, manually assign focus in such cases
                        var focusable = [localContextGroup, localMenuGroup, playlistGroup]
                        if (!focusable.some(function (obj) { return obj.activeFocus; })) {
                            // no object has focus
                            localToolbar.nextItemInFocusChain(true).forceActiveFocus()
                        }
                    }
                }

                background: Rectangle {
                    id: localToolbarBg

                    color: VLCStyle.colors.lowerBanner
                    Rectangle {
                        anchors.left : parent.left
                        anchors.right: parent.right
                        anchors.top  : parent.bottom

                        height: VLCStyle.border

                        color: VLCStyle.colors.border
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
                        enabled: list_grid_btn.visible || sortControl.visible

                        model: ObjectModel {
                            id: localContextModel

                            property int countExtra: 0

                            Widgets.IconToolButton {
                                id: list_grid_btn

                                width: VLCStyle.bannerButton_width
                                height: VLCStyle.bannerButton_height
                                size: VLCStyle.icon_banner
                                iconText: MainCtx.gridView ? VLCIcons.list : VLCIcons.grid
                                text: I18n.qtr("List/Grid")
                                onClicked: MainCtx.gridView = !MainCtx.gridView
                                enabled: true
                            }

                            Widgets.SortControl {
                                id: sortControl

                                width: VLCStyle.bannerButton_width
                                height: VLCStyle.bannerButton_height

                                iconSize: VLCStyle.icon_banner

                                visible: root.sortModel !== undefined && root.sortModel.length > 1

                                enabled: visible

                                textRole: "text"
                                criteriaRole: "criteria"

                                sortKey: contentModel ? contentModel.sortCriteria
                                                      : PlaylistControllerModel.SORT_KEY_NONE

                                sortOrder: contentModel ? contentModel.sortOrder : undefined

                                onSortSelected: {
                                    if (contentModel !== undefined)
                                        contentModel.sortCriteria = key
                                }

                                onSortOrderSelected: {
                                    if (contentModel !== undefined)
                                        contentModel.sortOrder = type
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

                        Navigation.parentItem: root
                        Navigation.rightItem: localMenuGroup.visible ? localMenuGroup : playlistGroup
                        Navigation.upItem: globalMenuGroup
                    }

                    Flickable {
                        id: localMenuView

                        readonly property int availableWidth: parent.width
                                                              - (localContextGroup.width + playlistGroup.width)
                                                              - (VLCStyle.applicationHorizontalMargin * 2)
                                                              - (VLCStyle.margin_xsmall * 2)
                                                              - (VLCStyle.margin_xxsmall * 2)
                        readonly property bool _alignHCenter: ((localToolbarContent.width - contentWidth) / 2) + contentWidth < playlistGroup.x

                        width: Math.min(contentWidth, availableWidth)
                        height: VLCStyle.localToolbar_height
                        clip: contentWidth > width
                        contentWidth: localMenuGroup.width
                        contentHeight: VLCStyle.localToolbar_height // don't allow vertical flickering

                        anchors.right: playlistGroup.left
                        anchors.rightMargin: VLCStyle.margin_xxsmall // only applied when right aligned

                        on_AlignHCenterChanged: {
                            if (_alignHCenter) {
                                anchors.horizontalCenter = localToolbarContent.horizontalCenter
                                anchors.right = undefined
                            } else {
                                anchors.horizontalCenter = undefined
                                anchors.right = playlistGroup.left
                            }
                        }

                        Loader {
                            id: localMenuGroup

                            focus: true
                            visible: !!item
                            enabled: status === Loader.Ready
                            y: status === Loader.Ready ? (VLCStyle.localToolbar_height - item.height) / 2 : 0
                            width: !!item
                                   ? Helpers.clamp(localMenuView.availableWidth,
                                                   localMenuGroup.item.minimumWidth || localMenuGroup.item.implicitWidth,
                                                   localMenuGroup.item.maximumWidth || localMenuGroup.item.implicitWidth)
                                   : 0

                            onVisibleChanged: {
                                //reset the focus on the global group when the local group is hidden,
                                //this avoids losing the focus if the subview changes
                                // FIXME: This block needs refactor for keyboard focus.
                                if (!visible && localMenuGroup.focus) {
                                    localMenuGroup.focus = false
                                    globalMenuGroup.focus = true
                                }
                            }

                            onItemChanged: {
                                if (!item)
                                    return
                                item.Navigation.parentItem = root
                                item.Navigation.leftItem = Qt.binding(function(){ return localContextGroup.enabled ? localContextGroup : null})
                                item.Navigation.rightItem = Qt.binding(function(){ return playlistGroup.enabled ? playlistGroup : null})
                                item.Navigation.upItem = globalMenuGroup
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
                        spacing: VLCStyle.margin_xxxsmall

                        model: ObjectModel {

                            Widgets.SearchBox {
                                id: searchBox
                                visible: !!root.contentModel
                                height: VLCStyle.bannerButton_height
                                buttonWidth: VLCStyle.bannerButton_width
                            }

                            Widgets.IconToolButton {
                                id: playlist_btn

                                size: VLCStyle.icon_banner
                                iconText: VLCIcons.playlist
                                text: I18n.qtr("Playlist")
                                width: VLCStyle.bannerButton_width
                                height: VLCStyle.bannerButton_height
                                highlighted: MainCtx.playlistVisible

                                onClicked:  MainCtx.playlistVisible = !MainCtx.playlistVisible
                            }

                            Widgets.IconToolButton {
                                id: menu_selector

                                visible: !MainCtx.hasToolbarMenu
                                size: VLCStyle.icon_banner
                                iconText: VLCIcons.menu
                                text: I18n.qtr("Menu")
                                width: VLCStyle.bannerButton_width
                                height: VLCStyle.bannerButton_height

                                onClicked: contextMenu.popup(this.mapToGlobal(0, height))

                                QmlGlobalMenu {
                                    id: contextMenu
                                    ctx: MainCtx
                                }
                            }
                        }

                        Navigation.parentItem: root
                        Navigation.leftItem: localMenuGroup.visible ? localMenuGroup : localContextGroup
                        Navigation.upItem: globalMenuGroup
                    }
                }
            }
        }

        Keys.priority: Keys.AfterItem
        Keys.onPressed: root.Navigation.defaultKeyAction(event)
    }
}
