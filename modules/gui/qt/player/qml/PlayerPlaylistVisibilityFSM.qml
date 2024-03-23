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
import org.videolan.vlc 0.1

import "qrc:///util/" as Util

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
Util.FSM {
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
    property alias isPlaylistVisible: fsmVisible.active

    initialState: MainCtx.playlistDocked ? fsmDocked : fsmFloating

    signalMap: ({
        togglePlaylistVisibility: fsm.togglePlaylistVisibility,
        updatePlaylistVisible: fsm.updatePlaylistVisible,
        updatePlaylistDocked: fsm.updatePlaylistDocked,
        updateVideoEmbed: fsm.updateVideoEmbed
    })

    Util.FSMState {
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

    Util.FSMState {
        id: fsmDocked

        initialState: MainCtx.hasEmbededVideo ? fsmForceHidden : fsmFollowVisible

        Util.FSMState {
            id: fsmFollowVisible

            initialState: MainCtx.playlistVisible ? fsmVisible : fsmHidden

            transitions: ({
                updatePlaylistDocked: {
                    guard: () => !MainCtx.playlistDocked,
                    target: fsmFloating
                },
                updateVideoEmbed: {
                    guard: () => MainCtx.hasEmbededVideo,
                    target: fsmForceHidden
                }
            })

            Util.FSMState {
                id: fsmVisible

                transitions: ({
                    updatePlaylistVisible: {
                        guard: () => !MainCtx.playlistVisible,
                        target: fsmHidden
                    },
                    togglePlaylistVisibility: {
                        action: () => { MainCtx.playlistVisible = false },
                        target: fsmHidden
                    }
                })
            }

            Util.FSMState {
                id: fsmHidden

                transitions: ({
                    updatePlaylistVisible: {
                        guard: () => MainCtx.playlistVisible,
                        target: fsmVisible
                    },
                    togglePlaylistVisibility: {
                        action: () => { MainCtx.playlistVisible = true },
                        target: fsmVisible
                    }
                })
            }
        }

        Util.FSMState {
            id: fsmForceHidden

            transitions: ({
                updateVideoEmbed: {
                    guard: () => !MainCtx.hasEmbededVideo,
                    target: fsmFollowVisible
                },
                updatePlaylistVisible: {
                    guard: () => MainCtx.playlistVisible,
                    target: fsmFollowVisible
                },
                togglePlaylistVisibility: {
                    action: () => { MainCtx.playlistVisible = true },
                    target: fsmFollowVisible
                }
            })
        }
    }
}
