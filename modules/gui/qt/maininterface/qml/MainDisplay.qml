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
import QtQuick.Layouts 1.11
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1
import org.videolan.compat 0.1

import "qrc:///style/"
import "qrc:///main/" as Main
import "qrc:///widgets/" as Widgets
import "qrc:///playlist/" as PL
import "qrc:///player/" as P

import "qrc:///util/" as Util
import "qrc:///util/Helpers.js" as Helpers

FocusScope {
    id: root

    //name and properties of the tab to be initially loaded
    property var view: ({
        "name": "",
        "properties": {}
    })

    property alias g_mainDisplay: root
    property bool _inhibitMiniPlayer: false
    property bool _showMiniPlayer: false
    property var _oldViewProperties: ({}) // saves last state of the views

    onViewChanged: {
        _oldViewProperties[view.name] = view.properties
        loadView()
    }

    Component.onCompleted: {
        loadView()
        if (MainCtx.mediaLibraryAvailable && !MainCtx.hasFirstrun)
            // asynchronous call
            MainCtx.mediaLibrary.reload()
    }

    function loadView() {
        var found = stackView.loadView(root.pageModel, root.view.name, root.view.properties)

        var item = stackView.currentItem

        item.Navigation.parentItem = medialibId
        item.Navigation.upItem = sourcesBanner
        item.Navigation.rightItem = playlistColumn

        item.Navigation.downItem = Qt.binding(function() {
            return miniPlayer.visible ? miniPlayer : medialibId
        })

        sourcesBanner.localMenuDelegate = Qt.binding(function () {
            return !!item.localMenuDelegate ? item.localMenuDelegate : null
        })

        // NOTE: sortMenu is declared with the SortMenu type, so when it's undefined we have to
        //       return null to avoid a QML warning.
        sourcesBanner.sortMenu = Qt.binding(function () {
            if (item.sortMenu)
                return item.sortMenu
            else
                return null
        })

        sourcesBanner.sortModel = Qt.binding(function () { return item.sortModel })
        sourcesBanner.contentModel = Qt.binding(function () { return item.contentModel })

        sourcesBanner.extraLocalActions = Qt.binding(function () { return item.extraLocalActions })

        sourcesBanner.isViewMultiView = Qt.binding(function () {
            return item.isViewMultiView === undefined || item.isViewMultiView
        })

        // Restore sourcesBanner state
        sourcesBanner.selectedIndex = pageModel.filter(function (e) {
            return e.listed
        }).findIndex(function (e) {
            return e.name === root.view
        })

        if (item.pageModel !== undefined)
            sourcesBanner.subSelectedIndex = item.pageModel.findIndex(function (e) {
                return e.name === item.view
            })

        if (Player.hasVideoOutput && MainCtx.hasEmbededVideo)
            _showMiniPlayer = true
    }

    Navigation.cancelAction: function() {
        History.previous()
    }

    Keys.onPressed: {
        if (KeyHelper.matchSearch(event)) {
            sourcesBanner.search()
            event.accepted = true
        }
        //unhandled keys are forwarded as hotkeys
        if (!event.accepted)
            MainCtx.sendHotkey(event.key, event.modifiers);
    }

    readonly property var pageModel: [
        {
            listed: MainCtx.mediaLibraryAvailable,
            displayText: I18n.qtr("Video"),
            icon: VLCIcons.topbar_video,
            name: "video",
            url: "qrc:///medialibrary/VideoDisplay.qml"
        }, {
            listed: MainCtx.mediaLibraryAvailable,
            displayText: I18n.qtr("Music"),
            icon: VLCIcons.topbar_music,
            name: "music",
            url: "qrc:///medialibrary/MusicDisplay.qml"
        }, {
            listed: !MainCtx.mediaLibraryAvailable,
            displayText: I18n.qtr("Home"),
            icon: VLCIcons.home,
            name: "home",
            url: "qrc:///main/NoMedialibHome.qml"
        }, {
            listed: true,
            displayText: I18n.qtr("Browse"),
            icon: VLCIcons.topbar_network,
            name: "network",
            url: "qrc:///network/NetworkDisplay.qml"
        }, {
            listed: true,
            displayText: I18n.qtr("Discover"),
            icon: VLCIcons.topbar_discover,
            name: "discover",
            url: "qrc:///network/DiscoverDisplay.qml"
        }, {
            listed: false,
            name: "mlsettings",
            url: "qrc:///medialibrary/MLFoldersSettings.qml"
        }
    ]


    property ListModel tabModel: ListModel {
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
        History.push(["player"])
    }

    function play(backend, ids) {
        showPlayer();

        backend.addAndPlay(ids);
    }

    Util.ModelSortSettingHandler {
        id: modelSortSettingHandler
    }

    Connections {
        target: sourcesBanner

        onContentModelChanged: modelSortSettingHandler.set(sourcesBanner.contentModel, History.viewPath)
    }

    Rectangle {
        color: VLCStyle.colors.bg
        anchors.fill: parent

        FocusScope {
            focus: true
            id: medialibId
            anchors.fill: parent

            Navigation.parentItem: root

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
                        var name = root.tabModel.get(index).name
                        selectedIndex = index
                        if (_oldViewProperties[name] === undefined)
                            History.push(["mc", name])
                        else
                            History.push(["mc", name, _oldViewProperties[name]])
                    }

                    Navigation.parentItem: medialibId

                    Navigation.downAction: function() {
                        stackView.currentItem.setCurrentItemFocus(Qt.TabFocusReason);
                    }
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

                            // NOTE: The StackView must be above the indexing bar and the mini
                            //       player.
                            bottomMargin: (loaderProgress.active) ? miniPlayer.height
                                                                    + loaderProgress.height
                                                                  : miniPlayer.height

                            right: playlistColumn.visible ? playlistColumn.left : parent.right
                            rightMargin: (MainCtx.playlistDocked && MainCtx.playlistVisible)
                                         ? 0
                                         : VLCStyle.applicationHorizontalMargin
                            leftMargin: VLCStyle.applicationHorizontalMargin
                        }

                        // This item is the root of a large hierarchy
                        // which requires many batches to be rendered.
                        // When the miniPlayer effect is active, this
                        // item (source item) gets rendered in an offscreen
                        // surface. If we don't enable layer here,
                        // it (along with children) gets rendered again
                        // in the assigned window.
                        // If layer is enabled, instead of rendering one
                        // more time with many batches, a dynamic texture
                        // from the offscreen surface is used. This behavior
                        // reduces the amount of batches from 2x to x+1.
                        // A side effect is having to draw a large texture
                        // with blending on, but this must be cheaper redrawing
                        // all the batches.
                        // TODO: Reconsider this behavior when batching is optimized.
                        layer.enabled: miniPlayer.visible && miniPlayer.effectAvailable

                        // Enable clipping so that the effect does not sit
                        // on top of the source.
                        clip: miniPlayer.visible && miniPlayer.effectAvailable
                    }

                    FocusScope {
                        id: playlistColumn
                        anchors {
                            top: parent.top
                            right: parent.right
                        }
                        focus: false

                        implicitWidth: Helpers.clamp(root.width / resizeHandle.widthFactor,
                                                     playlist.minimumWidth,
                                                     root.width / 2)
                        width: 0
                        height: parent.height - miniPlayer.height

                        visible: false

                        state: (MainCtx.playlistDocked && MainCtx.playlistVisible) ? "expanded" : ""

                        states: State {
                            name: "expanded"
                            PropertyChanges {
                                target: playlistColumn
                                width: playlistColumn.implicitWidth
                                visible: true
                            }
                        }

                        transitions: Transition {
                            from: ""; to: "expanded";
                            reversible: true

                            SequentialAnimation {
                                PropertyAction { property: "visible" }

                                NumberAnimation {
                                    property: "width"
                                    duration: VLCStyle.duration_short
                                    easing.type: Easing.InOutSine
                                }
                            }
                        }

                        Rectangle {
                            id: playlistLeftBorder

                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left

                            width: VLCStyle.border
                            color: VLCStyle.colors.border
                        }

                        PL.PlaylistListView {
                            id: playlist

                            anchors {
                                top: parent.top
                                bottom: parent.bottom
                                left: playlistLeftBorder.right
                                right: parent.right
                            }

                            focus: true

                            rightPadding: VLCStyle.applicationHorizontalMargin
                            bottomPadding: topPadding + Math.max(VLCStyle.applicationVerticalMargin - miniPlayer.height, 0)

                            Navigation.parentItem: medialibId
                            Navigation.upItem: sourcesBanner
                            Navigation.downItem: miniPlayer.visible ? miniPlayer : null

                            Navigation.leftAction: function() {
                                stackView.currentItem.setCurrentItemFocus(Qt.TabFocusReason);
                            }

                            Navigation.cancelAction: function() {
                                MainCtx.playlistVisible = false
                                stackView.forceActiveFocus()
                            }

                            Widgets.HorizontalResizeHandle {
                                id: resizeHandle

                                property bool _inhibitMainInterfaceUpdate: false

                                anchors {
                                    top: parent.top
                                    bottom: parent.bottom
                                    left: parent.left
                                }

                                atRight: false
                                targetWidth: playlistColumn.width
                                sourceWidth: root.width

                                onWidthFactorChanged: {
                                    if (!_inhibitMainInterfaceUpdate)
                                        MainCtx.setPlaylistWidthFactor(widthFactor)
                                }

                                Component.onCompleted:  _updateFromMainInterface()

                                function _updateFromMainInterface() {
                                    if (widthFactor == MainCtx.playlistWidthFactor)
                                        return

                                    _inhibitMainInterfaceUpdate = true
                                    widthFactor = MainCtx.playlistWidthFactor
                                    _inhibitMainInterfaceUpdate = false
                                }

                                Connections {
                                    target: MainCtx

                                    onPlaylistWidthFactorChanged: {
                                        resizeHandle._updateFromMainInterface()
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Loader {
                id: loaderProgress

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: miniPlayer.top

                active: (MainCtx.mediaLibraryAvailable && MainCtx.mediaLibrary.idle === false)

                source: "qrc:///widgets/ScanProgressBar.qml"

                onItemChanged: {
                    if (item === null) return

                    // NOTE: These are required for the FrostedGlassEffect.

                    item.source = Qt.binding(function() { return stackView })

                    item.sourceRect = Qt.binding(function() {
                        return stackView.mapFromItem(parent, x, y, width, height)
                    })
                }
            }

            P.PIPPlayer {
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
                visible: !root._inhibitMiniPlayer && root._showMiniPlayer && MainCtx.hasEmbededVideo
                enabled: !root._inhibitMiniPlayer && root._showMiniPlayer && MainCtx.hasEmbededVideo

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


            P.MiniPlayer {
                id: miniPlayer

                BindingCompat on state {
                    when: root._inhibitMiniPlayer && !miniPlayer.visible
                    value: ""
                }

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom

                z: 3
                Navigation.parentItem: medialibId
                Navigation.upItem: stackView
                Navigation.cancelItem:sourcesBanner
                onVisibleChanged: {
                    if (!visible && miniPlayer.activeFocus)
                        stackView.forceActiveFocus()
                }

                effectSource: stackView
                effectSourceRect: effectSource.mapFromItem(parent,
                                                           x,
                                                           y,
                                                           width,
                                                           height)
            }

            Connections {
                target: Player
                onHasVideoOutputChanged: {
                    if (Player.hasVideoOutput && MainCtx.hasEmbededVideo) {
                        if (History.current.view !== "player")
                            g_mainDisplay.showPlayer()
                    } else {
                        _showMiniPlayer = false;
                    }
                }
            }
        }
    }
}
