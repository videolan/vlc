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
import "qrc:///qml/"
import "qrc:///utils/" as Utils
import "qrc:///playlist/" as PL
import "qrc:///player/" as Player

Utils.NavigableFocusScope {
    id: root

    //name and properties of the tab to be initially loaded
    property string view: ""
    property var viewProperties: ({})

    onViewChanged: loadView()
    onViewPropertiesChanged: loadView()
    Component.onCompleted: loadView()

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


    Component {
        id: musicComp
        MCMusicDisplay {
            navigationParent: medialibId
            navigationUpItem: sourcesBanner
            navigationRightItem: playlist
            navigationDownItem: miniPlayer.expanded ? miniPlayer : medialibId
            navigationCancelItem: stackViewZone
        }
    }

    Component {
        id: videoComp
        MCVideoDisplay {
            navigationParent: medialibId
            navigationUpItem: sourcesBanner
            navigationRightItem: playlist
            navigationDownItem: miniPlayer.expanded ? miniPlayer : medialibId
            navigationCancelItem: stackViewZone
        }
    }

    Component {
        id: networkComp
        MCNetworkDisplay {
            navigationParent: medialibId
            navigationUpItem: sourcesBanner
            navigationRightItem: playlist
            navigationDownItem: miniPlayer.expanded ? miniPlayer : medialibId
            navigationCancelItem: stackViewZone
        }
    }

    readonly property var pageModel: [
        {
            displayText: qsTr("Video"),
            icon: VLCIcons.topbar_video,
            name: "video",
            component: videoComp
        }, {
            displayText: qsTr("Music"),
            icon: VLCIcons.topbar_music,
            name: "music",
            component: musicComp
        }, {
            displayText: qsTr("Network"),
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

        Utils.NavigableFocusScope {
            focus: true
            id: medialibId
            anchors.fill: parent

            ColumnLayout {
                id: column
                anchors.fill: parent

                Layout.minimumWidth: VLCStyle.minWidthMediacenter
                spacing: 0

                /* Source selection*/
                BannerSources {
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

                    navigationParent: root
                    navigationDownItem: stackView
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Utils.NavigableFocusScope {
                        id: stackViewZone
                        anchors.fill: parent

                        onActionUp: sourcesBanner.focus = true
                        onActionDown: {
                            if (miniPlayer.expanded)
                                miniPlayer.focus = true
                        }

                        Keys.onPressed: {
                            if (!event.accepted)
                                defaultKeyAction(event, 0)
                        }
                        Keys.onReleased: {
                            if (!event.accepted && (event.key === Qt.Key_Return || event.key === Qt.Key_Space)) {
                                event.accepted = true
                                stackView.focus = true
                            }
                        }
                    }

                    Rectangle {
                        visible: stackViewZone.focus
                        anchors.fill: stackViewZone
                        z: 42
                        color: VLCStyle.colors.setColorAlpha(VLCStyle.colors.accent, 0.08)
                        border.width: VLCStyle.selectedBorder
                        border.color: VLCStyle.colors.accent
                    }

                    Utils.StackViewExt {
                        id: stackView
                        anchors {
                            top: parent.top
                            left: parent.left
                            bottom: parent.bottom
                            right: playlist.visible ? playlist.left : parent.right
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

                        navigationParent: medialibId
                        navigationLeftItem: stackView
                        navigationUpItem: sourcesBanner
                        navigationDownItem: miniPlayer.expanded ? miniPlayer : undefined
                        navigationCancelItem: stackViewZone

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

            Utils.ScanProgressBar {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
            }
        }

    }
}
