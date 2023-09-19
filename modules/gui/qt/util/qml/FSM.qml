/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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

/**
 * @brief a pure QML hierarchical Finite State Machine implementation
 *
 * FSM {
 *   signal aSignal()
 *   signal anotherSignal(a, b, c)
 *
 *   //map signals to a key
 *   signalMap: ({
 *     "aSignal": aSignal,
 *     "anotherSignal": anotherSignal
 *   })
 *
 *   //define what is the initial sub state
 *   initialState: firstState
 *
 *   FSMState {
 *     id: firstState
 *     //transitions defintions for this state
 *     transitions: ({
 *        "aSignal": finalState, //transition to finalState when receiving aSignal
 *        "anotherSignal": [{
 *           action: (a,b,c) => {
 *              //action is executed if transition is taken (anotherSignal is received and
 *              //guard returns true)
 *           },
 *           guard: (a,b,c) => a + b > c, //transition is taken if guard returns true
 *           target: subStateA //target state
 *        }, {
 *           target: subStateB
 *        }]
 *     })
 *   }
 *   FSMState {
 *     id: anotherState
 *
 *     initialState: subStateA
 *     FSMState {
 *       id: subStateA
 *       //states may be nested
 *     }
 *     FSMState {
 *       id: subStateB
 *     }
 *   }
 *   FSMState {
 *     id: finalState
 *   }
 * }
 */
FSMState {
    id: fsm

    //each signal is associated to a key, when a signal is received,
    //transitions of active state for the given key are evaluated
    property var signalMap: ({
    })

    property bool running: true

    property bool started: false

    /**
     * @param {FSMState} state state handling the event
     * @param {string} event name of the event
     * @param {...*} args event arguments
     * @param {Object} t transition definition
     * @return {boolean} true if the state has handled the event
     */
    function _evaluateTransition(state, event, t, ...args) {
        if ("guard" in t) {
            if (!(t.guard instanceof Function)) {
                console.error(`guard property of ${state}::${event} is not a function`)
            }
            if (!t.guard(...args))
                return false
        }

        if ("action" in t) {
            if (!(t.action instanceof Function))
                console.error(`action property of ${state}::${event} is not a function`)
            t.action(...args)
        }

        if ("target" in t)
            _changeState(t.target)

        return true
    }

    /**
     * @param {FSMState} state state handling the event
     * @param {string} event name of the event
     * @param {...*} args event arguments
     * @return {boolean} true if the state has handled the event
     */
    function handleSignal(state, event, ...args) {
        if (!running)
            return false

        if (!state)
            return false

        if (state._state) {
            if (handleSignal(state._state, event, ...args))
                return true
        }

        if (!(event in state.transitions)) {
            return false
        }

        const transitions = state.transitions[event]
        if (transitions === undefined) {
            console.warn(`undefined transition for ${state}::${event}`)
            //FIXME: comparing object to QML type with instanceof fails with 5.12
        } else if (transitions === null || transitions.toString().startsWith("FSMState")) {
            _changeState(transitions)
            return true
        } else if (Array.isArray(transitions)) {
            for (const t of transitions) {
                //stop at the first accepted transition
                if (_evaluateTransition(state, event, t, ...args))
                    return true
            }
            return false
        } else {
            return _evaluateTransition(state, event, transitions, ...args)
        }
    }

    /**
     * @param {FSMState} state
     */
    function _exitState(state) {
        if (!state)
            return

        //exit sub states
        if (state._state)
            _exitState(state._state)

        state._state = null
        state.active = false
        if (state.exit instanceof Function)
            state.exit()
    }

    /**
     * @brief mark the state as active, enter handler is evaluated
     * @param {FSMState} state
     */
    function _activateState(state) {
        if (!state)
            return

        if (!state.active) {
            state.active = true
            if (state.enter instanceof Function)
                state.enter()
        }
    }

    /**
     * @brief enter the target state, enter handler are evaluated
     * inital sub-states are entered recursively
     * @param {FSMState} state
     */
    function _enterState(state) {
        if (!state)
            return

        _activateState(state)
        if (state.initialState) {
            state._state = state.initialState
            _enterState(state._state)
        }
    }

    /**
     * @param {FSMState} state
     * @param {FSMState[]} parentStates
     */
    function _resolveStatesHierarchy(state, parentStates) {
        if (!state)
            return
        state._parentStates = parentStates
        for (let i in state._children) {
            const child = state._children[i]
            if (child instanceof FSMState) {
                state._subStates.push(child)
            }
        }
        for (const s of state._subStates) {
            _resolveStatesHierarchy(s, [...parentStates, state])
        }
    }

    /**
     * @param {FSMState} state
     */
    function _validateFSM(state) {
        if (!state)
            return

        if (!(state instanceof FSMState)) {
            console.warn(`invalid state machine: ${state} is not an FSMState node`)
        }

        for (const key of Object.keys(state.transitions)) {
            if (!Object.keys(fsm.signalMap).includes(key)) {
                console.warn(`transition ${key} ${state} match no signal`, Object.keys(fsm.signalMap))
            }
        }

        for (const s of state._subStates) {
            _validateFSM(s)
        }
    }

    /**
     * @param {FSMState[]} a state list
     * @param {FSMState} a root state
     * @return {FSMState} the common ancestor
     */
    function _findCommonAncestorState(a, b) {
        if (!a || !b)
            return null

        if (!b._parentStates.includes(a))
            return null

        const node = _findCommonAncestorState(a._state, b)
        if (node !== null)
            return node

        return a
    }


    /**
     * @param {FSMState} state target state
     */
    function _changeState(state) {
        const ancestor = _findCommonAncestorState(fsm, state)

        //exit uncommon states
        if (ancestor) {
            _exitState(ancestor._state)
        }

        if (!state) {
            return
        }

        //activate parent state, but do not enter their initialState
        let parentState = fsm
        for (let i in state._parentStates) {
            _activateState(state._parentStates[i])
            parentState._state = state._parentStates[i]
            parentState = state._parentStates[i]
        }

        //enter target state, then enter initial sub-states
        parentState._state = state
        _enterState(state)
    }


    /**
     * reset the FSM to its initial state, exit handlers of the current state are not
     * evaluated. enter hander of initial state will be evaluated
     */
    function reset() {
        function reset_rec(state) {
            if (!state)
                return
            if (state._state) {
                reset_rec(state._state)
                state._state = null
            }
            state.active = false
        }
        reset_rec(fsm)
        _changeState(initialState)
    }

    Component.onCompleted: {
        _resolveStatesHierarchy(fsm, [])
        _validateFSM(fsm)

        for (const signalName of Object.keys(signalMap)) {
            signalMap[signalName].connect((...args) => {
                //use callLater to ensure transitions are ordered.
                //signal are not queued by default, this is an issue
                //if an action/enter/exit function raise another signal
                Qt.callLater(() => {
                    handleSignal(fsm, signalName, ...args)
                })
            })
        }

        _changeState(fsm)
        fsm.started = true
    }
}
