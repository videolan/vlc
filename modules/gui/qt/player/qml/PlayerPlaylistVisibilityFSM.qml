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
import QtQuick

import VLC.MainInterface
import VLC.Util

/**
 * playlist visibility state machine
 *
 * @startuml
 * state Floating {
 * }
 * state Docked {
 *    state Visible {
 *    }
 *    state Hidden {
 *       state followVisible {
 *        }
 *       state embed {
 *       }  
 *    }
 * }
 * @enduml
 *
 */
FSM {
    id: fsm
 
    //incoming signals

    //user clicked on the playlist button
    signal togglePlaylistVisibility()
    //playlist visibility update externally
    signal updatePlaylistVisible()
    signal updatePlaylistDocked()
    signal updateVideoEmbed()

    //exposed internal states
    property alias isPlaylistVisible: fsmVisible.active
 
    initialState: MainCtx.playlistDocked ? fsmDocked : fsmFloating
    
    signalMap: ({
        togglePlaylistVisibility: fsm.togglePlaylistVisibility,
        updatePlaylistVisible: fsm.updatePlaylistVisible,
        updatePlaylistDocked: fsm.updatePlaylistDocked,
        updateVideoEmbed: fsm.updateVideoEmbed,
    })
 
    FSMState {
        id: fsmFloating
 
        transitions: ({
            togglePlaylistVisibility: {
                action: () => { MainCtx.playlistVisible = !MainCtx.playlistVisible }
            },
            updatePlaylistDocked: {
                guard: () => MainCtx.playlistDocked,
                target: fsmDocked
            }
        })
    }
 
    FSMState {
        id: fsmDocked
 
        initialState: (MainCtx.hasEmbededVideo || !MainCtx.playlistVisible )
                      ? fsmHidden : fsmVisible
 
        transitions: ({
            updatePlaylistDocked: {
                guard: () => !MainCtx.playlistDocked,
                target: fsmFloating
            },
        })
 
        FSMState {
            id: fsmVisible

            function enter() {
                MainCtx.playlistVisible = true
            }
 
            transitions: ({
                updateVideoEmbed: {
                    guard: () => MainCtx.hasEmbededVideo,
                    target: fsmHidden
                },
                updatePlaylistVisible: {
                    guard: () => !MainCtx.playlistVisible,
                    target: fsmFollowVisible
                },
                togglePlaylistVisibility: {
                    target: fsmFollowVisible
                },
            })
        }
 
        FSMState {
            id: fsmHidden

            initialState: MainCtx.hasEmbededVideo ? fsmEmbed : fsmFollowVisible

            FSMState {
                id: fsmFollowVisible

                function enter() {
                    MainCtx.playlistVisible = false
                }

                transitions: ({
                    updateVideoEmbed: {
                        guard: () => MainCtx.hasEmbededVideo,
                        target: fsmEmbed
                    },
                    updatePlaylistVisible: {
                        guard: () => MainCtx.playlistVisible,
                        target: fsmVisible
                    },
                    togglePlaylistVisibility: {
                        target: fsmVisible
                    },
                })
            }

            FSMState {
                id: fsmEmbed

                transitions: ({
                    updateVideoEmbed: [{ //guards tested in order{
                        guard: () => !MainCtx.hasEmbededVideo && !MainCtx.playlistVisible,
                        target: fsmFollowVisible
                    }, {
                        guard: () => !MainCtx.hasEmbededVideo && MainCtx.playlistVisible,
                        target: fsmVisible
                    }],
                    togglePlaylistVisibility: {
                        target: fsmVisible
                    },
                    updatePlaylistVisible: {
                        guard: () => MainCtx.playlistVisible,
                        target: fsmVisible
                    },
                })
            }
        }
    }
}
