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
 *    state MinimalView {
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
    signal updateMinimalView()
    signal updateVideoEmbed()
 
    //exposed internal states
    property alias isVisible: fsmVisible.active

    property int lockCount: 0 // Track the number of locks
    property int timeoutDuration: 3000

    initialState: ((Player.isInteractive && MainCtx.hasEmbededVideo) || MainCtx.minimalView) 
                  ? fsmHidden : fsmVisible
 
    signalMap: ({
        askShow: fsm.askShow,
        mouseMove: fsm.mouseMove,
        keyboardMove: fsm.keyboardMove,
        lock: fsm.lock,
        timeout: fsm.timeout,
        unlock: fsm.unlock,
        updateMinimalView: fsm.updateMinimalView,
        updateVideoEmbed: fsm.updateVideoEmbed,
    })
 
    FSMState {
        id: fsmVisible

        initialState: fsmNormalVisible
 
        transitions: ({
            updateMinimalView: {
                guard: () => MainCtx.minimalView,
                target: fsmHidden
            }
        })
 
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
                            fsm.timeoutDuration = 3000;
                        },
                        target: fsmVideoMode
                    },
                    keyboardMove: {
                        guard: () => !Player.isInteractive,
                        action: () => { 
                            fsm.timeoutDuration = 5000
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
                updateMinimalView: {
                    guard: () => MainCtx.minimalView,
                    target: fsmMinimalView
                }
            })
        }
    }
 
    FSMState {
        id: fsmHidden

        function enter() {
            fsm.lockCount = 0;
            fsm.forceUnlock();
        }
    
        initialState: MainCtx.minimalView ? fsmMinimalView : fsmNormalHidden
    
        FSMState {
            id: fsmNormalHidden
    
            transitions: ({
                mouseMove: {
                    guard: () => !Player.isInteractive,
                    action: () => {
                        fsm.timeoutDuration = 3000
                    },
                    target: fsmVisible
                },
                keyboardMove: {
                    guard: () => !Player.isInteractive,
                    action: () => {
                        fsm.timeoutDuration = 5000
                    },
                    target: fsmVisible
                },
                askShow: {
                    target: fsmVisible
                },
                updateMinimalView: {
                    guard: () => MainCtx.minimalView,
                    target: fsmMinimalView
                }
            })
        }
    
        FSMState {
            id: fsmMinimalView

            transitions: ({
                updateMinimalView: {
                    guard: () => !MainCtx.minimalView,
                    target: fsmVisible,
                    action: () => {
                        fsm.timeoutDuration = 3000
                    }
                }
            })
        }
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