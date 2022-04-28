/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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
import org.videolan.vlc 0.1

/**
 * playlist visibility state machine
 *
 * @startuml
 * state Floating {
 * }
 * state Docked {
 *   state FollowVisible {
 *      state Visible {
 *      }
 *      state Hidden {
 *      }
 *   }
 *   state ForceHidden {
 *   }
 * }
 * @enduml
 *
 */
Item {
    id: fsm

    //incoming signals
    signal togglePlaylistVisibility()
    signal updatePlaylistVisible()
    signal updatePlaylistDocked()
    signal updateVideoEmbed()

    //outcoming signals
    signal showPlaylist()
    signal hidePlaylist()

    //exposed internal states
    property alias isPlaylistVisible: fsmVisible.enabled

    property var _substate: undefined

    function setState(state, substate) {
        //callLater is used to avoid Connections on a signal to be immediatly
        //re-trigered on the new state
        Qt.callLater(function() {
            if (state._substate === substate)
                return;

            if (state._substate !== undefined) {
                if (state._substate.exited) {
                    state._substate.exited()
                }
                state._substate.enabled = false
            }

            state._substate = substate

            if (state._substate !== undefined) {
                state._substate.enabled = true
                if (state._substate.entered) {
                    state._substate.entered()
                }
            }
        })
    }

    //initial state
    Component.onCompleted: {
        if (MainCtx.playlistDocked)
            fsm.setState(fsm, fsmDocked)
        else
            fsm.setState(fsm, fsmFloating)
    }

    Item {
        id: fsmFloating
        enabled: false

        Connections {
            target: fsm
            //explicitly bind on parent enabled, as Connections doens't behave as Item
            //regarding enabled propagation on children, ditto bellow
            enabled: fsmFloating.enabled

            onTogglePlaylistVisibility: {
                MainCtx.playlistVisible = !MainCtx.playlistVisible
            }

            onUpdatePlaylistDocked: {
                if (MainCtx.playlistDocked) {
                    fsm.setState(fsm, fsmDocked)
                }
            }
        }
    }

    Item {
        id: fsmDocked
        enabled: false

        property var _substate: undefined

        function entered() {
            if(MainCtx.hasEmbededVideo) {
                fsm.setState(fsmDocked, fsmForceHidden)
            } else {
                fsm.setState(fsmDocked, fsmFollowVisible)
            }
        }

        function exited() {
            fsm.setState(fsmDocked, undefined)
        }

        Item {
            id: fsmFollowVisible
            enabled: false

            property var _substate: undefined

            function entered() {
                if(MainCtx.playlistVisible)
                    fsm.setState(this, fsmVisible)
                else
                    fsm.setState(this, fsmHidden)
            }

            function exited() {
                fsm.setState(this, undefined)
            }

            Connections {
                target: fsm
                enabled: fsmFollowVisible.enabled

                onUpdatePlaylistDocked: {
                    if (!MainCtx.playlistDocked) {
                        fsm.setState(fsm, fsmFloating)
                    }
                }

                onUpdateVideoEmbed: {
                    if (MainCtx.hasEmbededVideo) {
                        fsm.setState(fsmDocked, fsmForceHidden)
                    }
                }
            }

            Item {
                id: fsmVisible
                enabled: false

                function entered() {
                    fsm.showPlaylist()
                }

                function exited() {
                    fsm.hidePlaylist()
                }

                Connections {
                    target: fsm
                    enabled: fsmVisible.enabled

                    onUpdatePlaylistVisible: {
                        if (!MainCtx.playlistVisible)
                            fsm.setState(fsmFollowVisible, fsmHidden)
                    }

                    onTogglePlaylistVisibility: fsm.setState(fsmFollowVisible, fsmHidden)
                }
            }

            Item {
                id: fsmHidden
                enabled: false

                Connections {
                    target: fsm
                    enabled: fsmHidden.enabled

                    onUpdatePlaylistVisible: {
                        if (MainCtx.playlistVisible)
                            fsm.setState(fsmFollowVisible, fsmVisible)
                    }

                    onTogglePlaylistVisibility: fsm.setState(fsmFollowVisible, fsmVisible)
                }
            }

        }

        Item {
            id: fsmForceHidden
            enabled: false

            Connections {
                target: fsm
                enabled: fsmForceHidden.enabled

                onUpdateVideoEmbed: {
                    if (!MainCtx.hasEmbededVideo) {
                        fsm.setState(fsmDocked, fsmFollowVisible)
                    }
                }

                onTogglePlaylistVisibility: {
                    MainCtx.playlistVisible = true
                    fsm.setState(fsmDocked, fsmFollowVisible)
                }
            }
        }
    }
}
