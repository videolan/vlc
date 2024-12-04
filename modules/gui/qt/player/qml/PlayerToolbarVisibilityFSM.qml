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
 * player toolbars visibility state machine
 *
 * @startuml
 * state Visible {
 *    state NormalVisible {
 *        state AudioMode {
 *        }
 *       state VideoMode {
 *       } 
 *    }
 *    state Locked {
 *    }
 * }
 * state Hidden {
 *    state NormalHidden {
 *    }
 * }
 * @enduml
 *
 */

FSM {
    id: fsm
 
    //incoming signals
    signal askShow()
    signal mouseMove()
    signal keyboardMove()
    signal lock()
    signal unlock()
    signal forceUnlock()
    signal timeout()
    signal updateVideoEmbed()
 
    //exposed internal states
    property alias isVisible: fsmVisible.active

    property int lockCount: 0 // Track the number of locks
    property int timeoutDuration: MainCtx.mouseHideTimeout

    initialState: ((Player.isInteractive && MainCtx.hasEmbededVideo))
                  ? fsmHidden : fsmVisible
 
    signalMap: ({
        askShow: fsm.askShow,
        mouseMove: fsm.mouseMove,
        keyboardMove: fsm.keyboardMove,
        lock: fsm.lock,
        timeout: fsm.timeout,
        unlock: fsm.unlock,
        updateVideoEmbed: fsm.updateVideoEmbed,
    })
 
    FSMState {
        id: fsmVisible

        initialState: fsmNormalVisible
 
        FSMState {
            id: fsmNormalVisible

            initialState: MainCtx.hasEmbededVideo ? fsmVideoMode : fsmAudioMode
 
            transitions: ({
                lock: {
                    action: () => { fsm.lockCount++; },
                    target: fsmLocked
                }
            })
 
            FSMState {
                id: fsmAudioMode
 
                transitions: ({
                    updateVideoEmbed: {
                        guard: () => MainCtx.hasEmbededVideo,
                        target: fsmVideoMode
                    }
                })
            }
 
            FSMState {
                id: fsmVideoMode

                function enter() {
                    timeout.interval = fsm.timeoutDuration;

                    if (!Player.isInteractive)
                        timeout.restart();
                }

                function exit() {
                  timeout.stop();
                }
 
                transitions: ({
                    updateVideoEmbed: {
                        guard: () => !MainCtx.hasEmbededVideo,
                        target: fsmAudioMode
                    },
                    askShow: {
                        target: fsmHidden,
                    },
                    timeout: {
                        target: fsmHidden,
                    },
                    mouseMove: {
                        guard: () => !Player.isInteractive,
                        action: () => { 
                            fsm.timeoutDuration = MainCtx.mouseHideTimeout;
                        },
                        target: fsmVideoMode
                    },
                    keyboardMove: {
                        guard: () => !Player.isInteractive,
                        action: () => { 
                            fsm.timeoutDuration = MainCtx.mouseHideTimeout * 2
                        },
                        target: fsmVideoMode
                    }
                })
            }
        }
 
        FSMState {
            id: fsmLocked
 
            transitions: ({
                lock: {
                    action: () => { fsm.lockCount++; }
                },
                unlock: [{
                    action: () => { fsm.lockCount--; },
                    guard: () => fsm.lockCount === 1,
                    target: fsmNormalVisible,
                }, {
                    guard: () => fsm.lockCount > 1,
                    action: () => { fsm.lockCount--; }
                }],

            })
        }
    }
 
    FSMState {
        id: fsmHidden

        function enter() {
            fsm.lockCount = 0;
            fsm.forceUnlock();
        }

        transitions: ({
            mouseMove: {
                guard: () => !Player.isInteractive,
                action: () => {
                    fsm.timeoutDuration = MainCtx.mouseHideTimeout
                },
                target: fsmVisible
            },
            keyboardMove: {
                guard: () => !Player.isInteractive,
                action: () => {
                    fsm.timeoutDuration = MainCtx.mouseHideTimeout * 2
                },
                target: fsmVisible
            },
            askShow: {
                target: fsmVisible
            },
        })
    }
 
    Timer {
        id: timeout
        interval: fsm.timeoutDuration
        repeat: false
        onTriggered: {
            fsm.timeout();
        }
    }
}
