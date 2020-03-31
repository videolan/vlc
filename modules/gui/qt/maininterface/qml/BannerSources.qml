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
import QtQml.Models 2.11

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///menus/" as Menus


Widgets.NavigableFocusScope {
    id: root

    height: (VLCStyle.icon_normal + VLCStyle.margin_xxsmall) * 2 + VLCStyle.applicationVerticalMargin

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
        medialib.gridView = !medialib.gridView
    }

    function search() {
        if (searchBox.visible)
            searchBox.expanded = true
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

            spacing: VLCStyle.margin_xxsmall

            Item {
                id: globalToolbar
                width: parent.width
                height: VLCStyle.icon_normal

                Widgets.NavigableRow {
                    id: historyGroup

                    anchors {
                        top: parent.top
                        left: parent.left
                        bottom: parent.bottom
                    }

                    model: ObjectModel {
                        Image {
                            sourceSize.width: VLCStyle.icon_normal
                            sourceSize.height: VLCStyle.icon_normal
                            source: "qrc:///logo/cone.svg"
                            enabled: false
                        }

                        Widgets.IconToolButton {
                            id: history_back
                            size: VLCStyle.icon_normal
                            iconText: VLCIcons.topbar_previous
                            text: i18n.qtr("Previous")
                            onClicked: history.previous()
                            enabled: !history.previousEmpty
                        }

                        Widgets.IconToolButton {
                            id: history_next
                            size: VLCStyle.icon_normal
                            iconText: VLCIcons.topbar_next
                            text: i18n.qtr("Next")
                            onClicked: history.next()
                            enabled: !history.nextEmpty
                        }
                    }

                    navigationParent: root
                    navigationRightItem: globalMenuGroup
                    navigationDownItem: localContextGroup
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
                    navigationLeftItem: historyGroup
                    navigationRightItem: globalCtxGroup
                    navigationDownItem: localMenuGroup.visible ?  localMenuGroup : playlistGroup

                    delegate: Widgets.TabButtonExt {
                        iconTxt: model.icon
                        selected: model.index === selectedIndex
                        onClicked: root.itemClicked(model.index)
                    }
                }

                Widgets.NavigableRow {
                    id: globalCtxGroup

                    anchors {
                        top: parent.top
                        right: parent.right
                        bottom: parent.bottom
                    }

                    model: ObjectModel {
                        Widgets.SearchBox {
                            id: searchBox
                            contentModel: root.contentModel
                            visible: root.contentModel !== undefined
                            enabled: visible
                        }

                        Widgets.IconToolButton {
                            id: menu_selector

                            size: VLCStyle.icon_normal
                            iconText: VLCIcons.menu
                            text: i18n.qtr("Menu")

                            onClicked: mainMenu.openBelow(this)

                            Menus.MainDropdownMenu {
                                id: mainMenu
                                onClosed: {
                                    if (mainMenu.activeFocus)
                                        menu_selector.forceActiveFocus()
                                }
                            }
                        }
                    }

                    navigationParent: root
                    navigationLeftItem: globalMenuGroup
                    navigationDownItem: playlistGroup
                }
            }

            Item {
                id: localToolbar
                width: parent.width
                height: VLCStyle.icon_normal

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
                            id: list_grid_btn
                            size: VLCStyle.icon_normal
                            iconText: medialib.gridView ? VLCIcons.list : VLCIcons.grid
                            text: i18n.qtr("List/Grid")
                            onClicked: medialib.gridView = !medialib.gridView
                        }

                        Widgets.SortControl {
                            id: sortControl

                            textRole: "text"
                            listWidth: VLCStyle.widthSortBox

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
                    navigationUpItem: historyGroup.navigable ? historyGroup : globalMenuGroup
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

                    delegate: Widgets.TabButtonExt {
                        text: model.displayText
                        selected: model.index === subSelectedIndex
                        onClicked:  root.subItemClicked(model.index)
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

                    model: ObjectModel {
                        Widgets.IconToolButton {
                            id: playlist_btn

                            size: VLCStyle.icon_normal
                            iconText: VLCIcons.playlist
                            text: i18n.qtr("Playlist")

                            onClicked:  mainInterface.playlistVisible = !mainInterface.playlistVisible
                        }
                    }

                    navigationParent: root
                    navigationLeftItem: localMenuGroup
                    navigationUpItem: globalCtxGroup.navigable ? globalCtxGroup : globalMenuGroup
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
