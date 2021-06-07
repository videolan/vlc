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

import "qrc:///style/"
import "qrc:///main/" as Main
import "qrc:///widgets/" as Widgets
import "qrc:///util/KeyHelper.js" as KeyHelper
import "qrc:///util/Helpers.js" as Helpers
import "qrc:///playlist/" as PL
import "qrc:///player/" as Player

Widgets.NavigableFocusScope {
    id: root

    //name and properties of the tab to be initially loaded
    property string view: ""
    property var viewProperties: ({})

    property alias g_mainDisplay: root
    property bool _inhibitMiniPlayer: false
    property bool _showMiniPlayer: false
    property var _defaultPages: ({}) // saves last page of view

    onViewChanged: {
        viewProperties = _defaultPages[root.view] !== undefined ? ({"defaultPage": _defaultPages[root.view]}) : ({})
        loadView()
    }
    onViewPropertiesChanged: loadView()
    Component.onCompleted: {
        loadView()
        if (medialib)
            // asynchronous call
            medialib.reload()
    }

    function loadView() {
        var found = stackView.loadView(root.pageModel, root.view, root.viewProperties)
        if (stackView.currentItem.view !== undefined)
            _defaultPages[root.view] = stackView.currentItem.view

        stackView.currentItem.navigationParent = medialibId
        stackView.currentItem.navigationUpItem = sourcesBanner
        stackView.currentItem.navigationRightItem = playlistColumn
        stackView.currentItem.navigationDownItem = Qt.binding(function() {
            return miniPlayer.expanded ? miniPlayer : medialibId
        })

        sourcesBanner.localMenuDelegate = Qt.binding(function () { return !!stackView.currentItem.localMenuDelegate ? stackView.currentItem.localMenuDelegate : null })
        sourcesBanner.sortModel = Qt.binding(function () { return stackView.currentItem.sortModel  })
        sourcesBanner.contentModel = Qt.binding(function () { return stackView.currentItem.contentModel })
        sourcesBanner.extraLocalActions = Qt.binding(function () { return stackView.currentItem.extraLocalActions })
        sourcesBanner.isViewMultiView = Qt.binding(function () {
            return stackView.currentItem.isViewMultiView === undefined || stackView.currentItem.isViewMultiView
        })
        // Restore sourcesBanner state
        sourcesBanner.selectedIndex = pageModel.filter(function (e) {
            return e.listed;
        }).findIndex(function (e) {
            return e.name === root.view
        })
        if (stackView.currentItem.pageModel !== undefined)
            sourcesBanner.subSelectedIndex = stackView.currentItem.pageModel.findIndex(function (e) {
                return e.name === stackView.currentItem.view
            })

        if (player.hasVideoOutput && mainInterface.hasEmbededVideo)
            _showMiniPlayer = true
    }

    navigationCancel: function() {
        history.previous()
    }

    Keys.onPressed: {
        if (KeyHelper.matchSearch(event)) {
            sourcesBanner.search()
            event.accepted = true
        }
        //unhandled keys are forwarded as hotkeys
        if (!event.accepted)
            mainInterface.sendHotkey(event.key, event.modifiers);
    }

    readonly property var pageModel: [
        {
            listed: !!medialib,
            displayText: i18n.qtr("Video"),
            icon: VLCIcons.topbar_video,
            name: "video",
            url: "qrc:///medialibrary/VideoDisplay.qml"
        }, {
            listed: !!medialib,
            displayText: i18n.qtr("Music"),
            icon: VLCIcons.topbar_music,
            name: "music",
            url: "qrc:///medialibrary/MusicDisplay.qml"
        }, {
            listed: !medialib,
            displayText: i18n.qtr("Home"),
            icon: VLCIcons.home,
            name: "home",
            url: "qrc:///main/NoMedialibHome.qml"
        }, {
            listed: true,
            displayText: i18n.qtr("Browse"),
            icon: VLCIcons.topbar_network,
            name: "network",
            url: "qrc:///network/NetworkDisplay.qml"
        }, {
            listed: true,
            displayText: i18n.qtr("Discover"),
            icon: VLCIcons.topbar_discover,
            name: "discover",
            url: "qrc:///network/DiscoverDisplay.qml"
        }, {
            listed: false,
            name: "mlsettings",
            url: "qrc:///medialibrary/MLFoldersSettings.qml"
        }
    ]


    property var tabModel: ListModel {
        id: tabModelid
        Component.onCompleted: {
            pageModel.forEach(function(e) {
                if (!e.listed)
                    return
                append({
                           displayText: e.displayText,
                           icon: e.icon,
                           name: e.name,
                       })
            })
        }
    }


    function showPlayer() {
        root._inhibitMiniPlayer = true
        history.push(["player"])
    }

    function play(backend, ids) {
        showPlayer();

        backend.addAndPlay(ids);
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
                id: mainColumn
                anchors.fill: parent

                Layout.minimumWidth: VLCStyle.minWindowWidth
                spacing: 0

                /* Source selection*/
                Main.BannerSources {
                    id: sourcesBanner
                    z: 2
                    Layout.preferredHeight: height
                    Layout.minimumHeight: height
                    Layout.maximumHeight: height
                    Layout.fillWidth: true

                    model: root.tabModel

                    onItemClicked: {
                        sourcesBanner.localMenuDelegate = null
                        var name = root.tabModel.get(index).name
                        selectedIndex = index
                        history.push(["mc", name])
                    }

                    navigationParent: medialibId
                    navigationDownItem: stackView
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    z: 0

                    Widgets.StackViewExt {
                        id: stackView

                        focus: true

                        anchors {
                            top: parent.top
                            left: parent.left
                            bottom: parent.bottom
                            bottomMargin: miniPlayer.height
                            right: playlistColumn.visible ? playlistColumn.left : playlistColumn.right
                            rightMargin: (mainInterface.playlistDocked && mainInterface.playlistVisible)
                                         ? 0
                                         : VLCStyle.applicationHorizontalMargin
                            leftMargin: VLCStyle.applicationHorizontalMargin
                        }

                        Loader {
                            z: 1
                            anchors {
                                left: parent.left
                                right: parent.right
                                bottom: parent.bottom
                                rightMargin: VLCStyle.margin_small
                                leftMargin: VLCStyle.margin_small
                                topMargin: VLCStyle.dp(10, VLCStyle.scale)
                                bottomMargin: VLCStyle.dp(10, VLCStyle.scale)
                            }
                            active: !!medialib && !medialib.idle
                            source: "qrc:///widgets/ScanProgressBar.qml"
                        }
                    }

                    Widgets.NavigableFocusScope {
                        id: playlistColumn
                        anchors {
                            top: parent.top
                            right: parent.right
                        }
                        focus: false

                        height: parent.height - miniPlayer.implicitHeight

                        property bool expanded: mainInterface.playlistDocked && mainInterface.playlistVisible

                        state: playlistColumn.expanded ? "expanded" : "collapsed"

                        states: [
                            State {
                                name: "expanded"
                                PropertyChanges {
                                    target: playlistColumn
                                    width: Helpers.clamp(root.width / resizeHandle.widthFactor,
                                                         playlist.minimumWidth,
                                                         root.width / 2)
                                    visible: true
                                }
                            },
                            State {
                                name: "collapsed"
                                PropertyChanges {
                                    target: playlistColumn
                                    width: 0
                                    visible: false
                                }
                            }
                        ]

                        transitions: [
                            Transition {
                                from: "*"
                                to: "collapsed"

                                SequentialAnimation {
                                    SmoothedAnimation { target: playlistColumn; property: "width"; easing.type: Easing.OutSine; duration: 150; }
                                    PropertyAction { target: playlistColumn; property: "visible" }
                                }
                            },

                            Transition {
                                from: "*"
                                to: "expanded"

                                SequentialAnimation {
                                    PropertyAction { target: playlistColumn; property: "visible" }
                                    SmoothedAnimation { target: playlistColumn; property: "width"; easing.type: Easing.InSine; duration: 150; }
                                }
                            }
                        ]


                        PL.PlaylistListView {
                            id: playlist

                            anchors.fill: parent
                            focus: true

                            rightPadding: VLCStyle.applicationHorizontalMargin

                            navigationParent: medialibId
                            navigationLeftItem: stackView
                            navigationUpItem: sourcesBanner
                            navigationDownItem: miniPlayer.expanded ? miniPlayer : undefined
                            navigationCancel: function() {
                                mainInterface.playlistVisible = false
                                stackView.forceActiveFocus()
                            }


                            Widgets.HorizontalResizeHandle {
                                id: resizeHandle
                                anchors {
                                    top: parent.top
                                    bottom: parent.bottom
                                    left: parent.left

                                    leftMargin: -(width / 2)
                                }

                                atRight: false
                                targetWidth: playlistColumn.width
                                sourceWidth: root.width

                                onWidthFactorChanged: mainInterface.setPlaylistWidthFactor(widthFactor)
                                Component.onCompleted:  {
                                    //don't bind just provide the initial value, HorizontalResizeHandle.widthFactor updates itself
                                    widthFactor = mainInterface.playlistWidthFactor
                                }
                            }
                        }
                    }
                }
            }

            Player.PIPPlayer {
                id: playerPip
                anchors {
                    bottom: miniPlayer.top
                    left: parent.left
                    bottomMargin: VLCStyle.margin_normal
                    leftMargin: VLCStyle.margin_normal + VLCStyle.applicationHorizontalMargin
                }

                width: VLCStyle.dp(320, VLCStyle.scale)
                height: VLCStyle.dp(180, VLCStyle.scale)
                z: 2
                visible: !root._inhibitMiniPlayer && root._showMiniPlayer
                enabled: !root._inhibitMiniPlayer && root._showMiniPlayer

                dragXMin: 0
                dragXMax: root.width - playerPip.width
                dragYMin: sourcesBanner.y + sourcesBanner.height
                dragYMax: miniPlayer.y - playerPip.height

                //keep the player visible on resize
                Connections {
                    target: root
                    onWidthChanged: {
                        if (playerPip.x > playerPip.dragXMax)
                            playerPip.x = playerPip.dragXMax
                    }
                    onHeightChanged: {
                        if (playerPip.y > playerPip.dragYMax)
                            playerPip.y = playerPip.dragYMax
                    }
                }
            }


            Player.MiniPlayer {
                id: miniPlayer

                visible: !root._inhibitMiniPlayer

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom

                z: 3
                navigationParent: medialibId
                navigationUpItem: stackView
                navigationCancelItem:sourcesBanner
                onExpandedChanged: {
                    if (!expanded && miniPlayer.activeFocus)
                        stackView.forceActiveFocus()
                }

                mainContent: mainColumn
            }

            Connections {
                target: player
                onHasVideoOutputChanged: {
                    if (player.hasVideoOutput && mainInterface.hasEmbededVideo) {
                        g_mainDisplay.showPlayer()
                    } else {
                        _showMiniPlayer = false;
                    }
                }
            }
        }
    }
}
