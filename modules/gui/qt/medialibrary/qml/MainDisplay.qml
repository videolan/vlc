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
import org.videolan.medialib 0.1
import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///main/" as Main
import "qrc:///widgets/" as Widgets
import "qrc:///util/KeyHelper.js" as KeyHelper
import "qrc:///playlist/" as PL
import "qrc:///player/" as Player
import "qrc:///network/" as Network

Widgets.NavigableFocusScope {
    id: root

    //name and properties of the tab to be initially loaded
    property string view: ""
    property var viewProperties: ({})

    onViewChanged: loadView()
    onViewPropertiesChanged: loadView()
    Component.onCompleted: {
        loadView()
        medialib.reload()
    }

    function loadView() {
        var found = stackView.loadView(root.pageModel, root.view, root.viewProperties)

        sourcesBanner.subTabModel = stackView.currentItem.tabModel
        sourcesBanner.sortModel = stackView.currentItem.sortModel
        sourcesBanner.contentModel = stackView.currentItem.contentModel
        sourcesBanner.extraLocalActions = stackView.currentItem.extraLocalActions
        // Restore sourcesBanner state
        sourcesBanner.selectedIndex = pageModel.findIndex(function (e) {
            return e.name === root.view
        })
        if (stackView.currentItem.pageModel !== undefined)
            sourcesBanner.subSelectedIndex = stackView.currentItem.pageModel.findIndex(function (e) {
                return e.name === stackView.currentItem.view
            })
    }

    navigationCancel: function() {
        history.previous(History.Go)
    }

    Component {
        id: musicComp
        MusicDisplay {
            navigationParent: medialibId
            navigationUpItem: sourcesBanner
            navigationRightItem: playlist
            navigationDownItem: miniPlayer.expanded ? miniPlayer : medialibId
        }
    }

    Component {
        id: videoComp
        VideoDisplay {
            navigationParent: medialibId
            navigationUpItem: sourcesBanner
            navigationRightItem: playlist
            navigationDownItem: miniPlayer.expanded ? miniPlayer : medialibId
        }
    }

    Component {
        id: networkComp
        Network.NetworkDisplay {
            navigationParent: medialibId
            navigationUpItem: sourcesBanner
            navigationRightItem: playlist
            navigationDownItem: miniPlayer.expanded ? miniPlayer : medialibId
        }
    }

    readonly property var pageModel: [
        {
            displayText: i18n.qtr("Video"),
            icon: VLCIcons.topbar_video,
            name: "video",
            component: videoComp
        }, {
            displayText: i18n.qtr("Music"),
            icon: VLCIcons.topbar_music,
            name: "music",
            component: musicComp
        }, {
            displayText: i18n.qtr("Network"),
            icon: VLCIcons.topbar_network,
            name: "network",
            component: networkComp
        }
    ]

    property var tabModel: ListModel {
        id: tabModelid
        Component.onCompleted: {
            pageModel.forEach(function(e) {
                append({
                           displayText: e.displayText,
                           icon: e.icon,
                           name: e.name,
                       })
            })
        }
    }

    Rectangle {
        color: VLCStyle.colors.bg
        anchors.fill: parent

        Widgets.NavigableFocusScope {
            focus: true
            id: medialibId
            anchors.fill: parent

            navigationParent: root

            ColumnLayout {
                id: column
                anchors.fill: parent

                Layout.minimumWidth: VLCStyle.minWindowWidth
                spacing: 0

                /* Source selection*/
                Main.BannerSources {
                    id: sourcesBanner

                    Layout.preferredHeight: height
                    Layout.minimumHeight: height
                    Layout.maximumHeight: height
                    Layout.fillWidth: true

                    focus: true
                    model: root.tabModel

                    onItemClicked: {
                        sourcesBanner.subTabModel = undefined
                        var name = root.tabModel.get(index).name
                        selectedIndex = index
                        history.push(["mc", name], History.Go)
                    }

                    onSubItemClicked: {
                        subSelectedIndex = index
                        stackView.currentItem.loadIndex(index)
                        sortModel = stackView.currentItem.sortModel
                        contentModel = stackView.currentItem.contentModel
                        extraLocalActions = stackView.currentItem.extraLocalActions
                    }

                    navigationParent: medialibId
                    navigationDownItem: stackView
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Widgets.StackViewExt {
                        id: stackView
                        anchors {
                            top: parent.top
                            left: parent.left
                            bottom: parent.bottom
                            right: playlist.visible ? playlist.left : parent.right
                            rightMargin: (rootWindow.playlistDocked && rootWindow.playlistVisible)
                                         ? 0
                                         : VLCStyle.applicationHorizontalMargin
                            leftMargin: VLCStyle.applicationHorizontalMargin
                        }

                        Widgets.ScanProgressBar {
                            z: 1
                            anchors {
                                left: parent.left
                                right: parent.right
                                bottom: parent.bottom
                                rightMargin: VLCStyle.margin_small
                                leftMargin: VLCStyle.margin_small
                            }
                        }
                    }


                    PL.PlaylistListView {
                        id: playlist
                        focus: true
                        width: root.width/4
                        visible: rootWindow.playlistDocked && rootWindow.playlistVisible
                        anchors {
                            top: parent.top
                            right: parent.right
                            bottom: parent.bottom
                        }

                        rightPadding: VLCStyle.applicationHorizontalMargin

                        navigationParent: medialibId
                        navigationLeftItem: stackView
                        navigationUpItem: sourcesBanner
                        navigationDownItem: miniPlayer.expanded ? miniPlayer : undefined

                        Rectangle {
                            anchors {
                                top: parent.top
                                left: parent.left
                                bottom: parent.bottom
                            }
                            width: VLCStyle.margin_xxsmall
                            color: VLCStyle.colors.banner
                        }
                    }
                }

                Player.MiniPlayer {
                    id: miniPlayer

                    navigationParent: medialibId
                    navigationUpItem: stackView
                    navigationCancelItem:sourcesBanner
                    onExpandedChanged: {
                        if (!expanded && miniPlayer.activeFocus)
                            stackView.forceActiveFocus()
                    }
                }
            }
        }

    }
}
