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
import QtTest
import "qrc:///util/" as Util

TestCase {
    id: root
    name: "FSM"

    property var events: []

    function recEvent(e) {
        events.push(e)
    }

    Util.FSM {
        id: fsm
        initialState: fsmA

        signal signalSelfInternal()
        signal signalSelfExternal()
        signal signalToParent()
        signal signalToSibiling()
        signal signalToBA()
        signal signalToC()
        signal signalInParent()

        signalMap: ({
            signalSelfInternal: signalSelfInternal,
            signalSelfExternal: signalSelfExternal,
            signalToParent: signalToParent,
            signalToSibiling: signalToSibiling,
            signalToBA: signalToBA,
            signalToC: signalToC,
            signalInParent: signalInParent,
        })

        property bool selfTransitionDone: false

        Util.FSMState {
            id: fsmA
            objectName: "fsmA"

            initialState: fsmAA

            function enter() {
                recEvent("+A")
            }

            function exit() {
                recEvent("-A")
            }

            Util.FSMState {
                id: fsmAA
                objectName: "fsmAA"

                initialState: fsmAAA

                function enter() {
                    recEvent("+AA")
                }

                function exit() {
                    recEvent("-AA")
                }

                transitions: ({
                    signalInParent: fsmC,
                })

                Util.FSMState {
                    id: fsmAAA
                    objectName: "fsmAAA"

                    function enter() {
                        recEvent("+AAA")
                    }

                    function exit() {
                        recEvent("-AAA")
                    }

                    transitions: ({
                        signalSelfInternal: {
                            action: () => { fsm.selfTransitionDone = true },
                        },
                        signalSelfExternal: {
                            action: () => { fsm.selfTransitionDone = true },
                            target: fsmAAA,
                        },
                        signalToParent: {
                            action: () => { fsm.selfTransitionDone = true },
                            target: fsmAA
                        },
                        signalToSibiling: fsmAAB,
                        signalToBA: fsmBA,
                        signalToC: fsmC,
                    })
                }

                Util.FSMState {
                    id: fsmAAB
                    objectName: "fsmAAB"

                    function enter() {
                        recEvent("+AAB")
                    }

                    function exit() {
                        recEvent("-AAB")
                    }

                    transitions: ({
                        signalToParent: {
                            action: () => { fsm.selfTransitionDone = true },
                            target: fsmAA
                        }
                    })

                }
            }
        }
        Util.FSMState {
            id: fsmB
            objectName: "fsmB"

            //no initial state
            function enter() { recEvent("+B") }
            function exit() {  recEvent("-B") }

            Util.FSMState {
                id: fsmBA
                objectName: "fsmBA"

                function enter() { recEvent("+BA") }
                function exit() {  recEvent("-BA") }
            }
        }
        Util.FSMState {
            id: fsmC
            objectName: "fsmC"

            function enter() {
                recEvent("+C")
            }

            function exit() {
                recEvent("-C")
            }
        }
    }


    function check_active_inactive(active, inactive)
    {
        for (const s of inactive)
            verify(!s.active, `${s} should be inactive`)

        for (const s of active)
            verify(s.active, `${s} should be active`)
    }

    function check_events(expected)
    {
        verify(
            expected.every((e, idx) => root.events[idx] === e),
            `expected transitions are ${expected}, got "${root.events}"`)
    }

    function init() {
        fsm.reset()

        fsmSeq.reset()
        fsmGuard.reset()
        fsmAction.reset()
        events = []
    }

    function test_initial_state() {
        check_active_inactive([fsmA, fsmAA, fsmAAA], [fsmAAB, fsmB, fsmBA, fsmC])
    }

    function test_reset() {
        fsm.reset()
        tryCompare(fsmAAA, "active", true)
        check_events(["+A", "+AA", "+AAA"])
        check_active_inactive([fsmA, fsmAA, fsmAAA], [fsmAAB, fsmB, fsmBA, fsmC])

    }

    function test_transitions_data() {
        return [
            {tag: "signalToSibiling", transition: fsm.signalToSibiling, events: ["-AAA", "+AAB"],
             active: [fsmAAB, fsmA, fsmAA], inactive: [fsmB, fsmBA, fsmC, fsmAAA]},
            {tag: "signalToBA", transition: fsm.signalToBA, events: ["-AAA", "-AA", "-A", "+B", "+BA"],
             active: [fsmBA, fsmB],  inactive: [fsmC, fsmA, fsmAA, fsmAAB, fsmAAA]},
            {tag:"signalInParent", transition: fsm.signalInParent, events: ["-AAA", "-AA", "-A", "+C"],
             active: [fsmC],  inactive: [fsmB, fsmBA, fsmA, fsmAA, fsmAAB, fsmAAA]},
        ]
    }

    function test_transitions(data) {
        data.transition()
        tryCompare(data.active[0], "active", true)
        check_events(data.events)
        check_active_inactive(data.active, data.inactive)
    }


    function test_self_transitions_data() {
        return [
            {tag: "SelfInternal", transition: fsm.signalSelfInternal, events: [],
             active: [fsmA, fsmAA, fsmAAA], inactive: [fsmB, fsmBA, fsmC, fsmAAB]},
            {tag: "SelfExternal", transition: fsm.signalSelfExternal, events: ["-AAA", "+AAA"],
             active: [fsmA, fsmAA, fsmAAA], inactive: [fsmB, fsmBA, fsmC, fsmAAB]},
            {tag: "signalToParent", transition: fsm.signalToParent, events: ["-AAA", "-AA", "+AA", "+AAA"],
             active: [fsmA, fsmAA, fsmAAA], inactive: [fsmB, fsmBA, fsmC, fsmAAB]},
        ]
    }

    function test_self_transitions(data) {
        fsm.selfTransitionDone = false
        data.transition()
        //state doesn't change, can't wait for state activation
        tryCompare(fsm, "selfTransitionDone", true)
        check_events(data.events)
        check_active_inactive(data.active, data.inactive)
    }

    function test_self_transitions_reset_initial_state() {
        //as a setup, move to AAB
        fsm.signalToSibiling()
        tryCompare(fsmAAB, "active", true)
        root.events = []

        //move to the parent state
        fsm.signalToParent()
        tryCompare(fsmAAA, "active", true)
        check_events(["-AAB", "-AA", "+AA", "+AAA"])
        check_active_inactive([fsmA, fsmAA, fsmAAA], [fsmAAB])
    }

    Util.FSM {
        id: fsmSeq

        signal atob()
        signal atoc()
        signal ctoa()

        signalMap: ({
            atob: atob,
            atoc: atoc,
            ctod: ctoa,
        })

        initialState: seqA
        Util.FSMState {
            id: seqA
            objectName: "seqA"
            function enter() { recEvent("+A")  }
            function exit() { recEvent("-A") }
            transitions: ({
                atob: seqB,
                atoc: seqC,

            })
        }
        Util.FSMState {
            id: seqB
            objectName: "seqB"
            function enter() { recEvent("+B") }
            function exit() { recEvent("-B")  }
            transitions: ({
                atob: seqA,
            })
        }
        Util.FSMState {
            id: seqC
            objectName: "seqC"
            function enter() {
                recEvent("+C")
                fsmSeq.ctoa()
            }
            function exit() { recEvent("-C") }
            transitions: ({
                ctod: seqD,
            })
        }
        Util.FSMState {
            id: seqD
            objectName: "seqD"
            function enter() { recEvent("+D") }
            function exit() { recEvent("-D") }
        }
    }

    function test_sequential_transitions_data() {
        return [
            {tag: "non-recursive", transition: fsmSeq.atob, events: ["-A", "+B"],
             active: [seqB], inactive: [seqA, seqC]},
            //C auto transition to A
            {tag: "recursive", transition: fsmSeq.atoc, events: ["-A", "+C", "-C", "+D"],
             active: [seqD], inactive: [seqB, seqC]},
        ]
    }

    function test_sequential_transitions(data) {
        data.transition()
        tryCompare(data.active[0], "active", true)
        check_events(data.events)
        check_active_inactive(data.active, data.inactive)
    }

    Util.FSM {
        id: fsmGuard

        signal success()
        signal fail()
        signal multiple()
        signal multipleDefault()
        signal multipleFail()
        signal paramBool(var a)
        signal paramMultiple(var a, var b, var c, var d, var e)

        signalMap: ({
            success: fsmGuard.success,
            fail: fsmGuard.fail,
            multiple: fsmGuard.multiple,
            multipleDefault: fsmGuard.multipleDefault,
            multipleFail: fsmGuard.multipleFail,
            paramBool: fsmGuard.paramBool,
            paramMultiple: fsmGuard.paramMultiple,
        })

        initialState: guardInit
        Util.FSMState {
            id: guardInit
            objectName: "guardInit"
            transitions: ({
                success: {
                    guard: () => true,
                    target: guardOK
                },
                fail: {
                    guard: () => false,
                    target: guardFail
                },
                multiple: [{
                    guard: () => false,
                    target: guardFail
                }, {
                    guard: () => true,
                    target: guardOK
                }, {
                    //will not be evaluated
                    target: guardFail
                }],
                multipleFail: [{
                    guard: () => false,
                    target: guardFail
                }, {
                    guard: () => false,
                    target: guardFail
                }],
                multipleDefault: [{
                    guard: () => false,
                    target: guardFail
                }, {
                    target: guardOK
                }, {
                    //will not be evaluated
                    target: guardFail
                }],
                paramBool: {
                    guard: (v) => v,
                    target: guardOK
                },
                paramMultiple: {
                    guard: (a, b, c, d, e) => a === true && b === 42 && c === "test" && d(e),
                    target: guardOK
                },

            })
        }
        Util.FSMState {
            id: guardOK
            objectName: "guardOK"
        }
        Util.FSMState {
            id: guardFail
            objectName: "guardFail"
        }
    }

    function test_guard_transitions_data() {
        return [
            {tag: "success",  transition: fsmGuard.success,
             active: [guardOK], inactive: [guardInit, guardFail]},
            {tag: "fail",  transition: fsmGuard.fail,
             active: [guardInit], inactive: [guardOK, guardFail]},
            {tag: "multiple",  transition: fsmGuard.multiple,
             active: [guardOK], inactive: [guardInit, guardFail]},
            {tag: "multipleDefault",  transition: fsmGuard.multipleDefault,
             active: [guardOK], inactive: [guardInit, guardFail]},
            {tag: "multipleFail",  transition: fsmGuard.multipleFail,
             active: [guardInit], inactive: [guardOK, guardFail]},
            {tag: "param(true)", transition: function() { fsmGuard.paramBool(true) },
             active: [guardOK], inactive: [guardInit, guardFail]},
            {tag: "param(false)", transition: function() { fsmGuard.paramBool(false) },
             active: [guardInit], inactive: [guardOK, guardFail]},
            {tag: "param(mutiple)", transition: function() { fsmGuard.paramMultiple(true, 42, "test", (bar) =>  bar.a === 51, { a: 51}) },
             active: [guardOK], inactive: [guardInit, guardFail]},
            {tag: "param(mutiple fail)", transition: function() { fsmGuard.paramMultiple(false, -1, "nope", (bar) =>  bar.a === 51, { a: 2}) },
             active: [guardInit], inactive: [guardOK, guardFail]},
        ]
    }

    function test_guard_transitions(data) {
        data.transition()
        tryCompare(data.active[0], "active", true)
        check_active_inactive(data.active, data.inactive)
    }


    Util.FSM {
        id: fsmAction

        signal simple()
        signal redefinedInChild()
        signal guardedFalse()
        signal guardedTrue()
        signal guardedMultiple()
        signal signalInAction1()
        signal signalInAction2()
        signal withParam(var a, var b, var c)

        signalMap: ({
            simple: simple,
            redefinedInChild: redefinedInChild,
            guardedFalse: guardedFalse,
            guardedTrue: guardedTrue,
            guardedMultiple: guardedMultiple,
            signalInAction1: signalInAction1,
            signalInAction2: signalInAction2,
            withParam: withParam,
        })

        initialState: actionA

        Util.FSMState {
            id: actionA
            objectName: "actionA"
            initialState: actionAA

            function enter() { recEvent("+A") }
            function exit() { recEvent("-A") }

            transitions: ({
                redefinedInChild: {
                    //child will transition before us, this won't be triggered
                    action: () => recEvent("KO"),
                    target: actionKO
                }
            })

            Util.FSMState {
                id: actionAA
                objectName: "actionAA"
                function enter() { recEvent("+AA") }
                function exit() { recEvent("-AA") }

                transitions: ({
                    simple: {
                        action: () => recEvent("OK"),
                        target: actionOK
                    },
                    guardedFalse: {
                        guard: () => false,
                        action: () => recEvent("KO"),
                        target: actionOK
                    },
                    guardedTrue: {
                        guard: () => true,
                        action: () => recEvent("OK"),
                        target: actionOK
                    },
                    guardedMultiple: [{
                        guard: () => false,
                        action: () => recEvent("KO"),
                        target: actionKO
                    }, {
                        guard: () => true,
                        action: () => recEvent("OK"),
                        target: actionOK
                    },{
                        //this should not be triggered
                        guard: () => true,
                        action: () => recEvent("KO"),
                        target: actionKO
                    }],
                    redefinedInChild: {
                        action: () => recEvent("OK"),
                        target: actionOK
                    },
                    withParam: {
                        action: (a,b,c) => {
                            if (a === 42 && b === "p1" && c.a === 51)
                                recEvent("OK")
                        },
                        target: actionOK
                    },
                    signalInAction1: {
                        action: () => { fsmAction.signalInAction2() },
                        target: actionTransient
                    },
                    signalInAction2: {
                        //this should not be triggered
                        action: () => { recEvent("KO") },
                        target: actionOK
                    },
                })
            }
        }
        Util.FSMState {
            id: actionOK
            objectName: "actionOK"
             function enter() { recEvent("+OK") }
            function exit() { recEvent("-OK") }

        }
        Util.FSMState {
            id: actionKO
            objectName: "actionKO"
            function enter() { recEvent("+KO") }
            function exit() { recEvent("-KO") }
        }
        Util.FSMState {
            id: actionTransient
            objectName: "actionTransient"
            function enter() { recEvent("+T") }
            function exit() { recEvent("-T") }
            transitions: ({
                signalInAction2: actionOK
            })
        }
    }

    function test_action_transitions_data() {
        return [
            {tag: "simple",  transition: fsmAction.simple,
             events: ["OK", "-AA", "-A", "+OK"], active: [actionOK], inactive: [actionA, actionAA]},
            {tag: "guardedFalse",  transition: fsmAction.guardedFalse,
             events: [], active: [actionA, actionAA], inactive: [actionOK]},
            {tag: "guardedTrue",  transition: fsmAction.guardedTrue,
             events: ["OK", "-AA", "-A", "+OK"], active: [actionOK], inactive: [actionA, actionAA]},
            {tag: "guardedMultiple",  transition: fsmAction.guardedMultiple,
             events: ["OK", "-AA", "-A", "+OK"], active: [actionOK], inactive: [actionA, actionAA]},
            {tag: "redefinedInChild",  transition: fsmAction.redefinedInChild,
             events: ["OK", "-AA", "-A", "+OK"], active: [actionOK], inactive: [actionA, actionAA]},
            {tag: "withParam",  transition: function() { fsmAction.withParam(42, "p1", {a: 51}) },
             events: ["OK", "-AA", "-A", "+OK"], active: [actionOK], inactive: [actionA, actionAA]},
            {tag: "signalInAction",  transition: fsmAction.signalInAction1,
             events: ["-AA", "-A", "+T", "-T", "+OK"], active: [actionOK], inactive: [actionA, actionAA]},
        ]
    }

    function test_action_transitions(data) {
        data.transition()
        tryCompare(data.active[0], "active", true)
        check_events(data.events)
        check_active_inactive(data.active, data.inactive)
    }


    //check that the FSM hierarchy may contain other object than FSMState nodes
    Util.FSM {
        id: fsmMixed
        initialState: mixedA

        signal atob()
        signal btoc()

        signalMap: ({
            atob: atob,
            btoc: btoc,
        })

        Util.FSMState {
            id: mixedA
            objectName: "mixedA"

            //whatever
            Rectangle {}

            transitions: ({
                atob: mixedB
            })
        }

        Util.FSMState {
            id: mixedB
            objectName: "mixedB"

            Timer {
                interval: 20
                running: mixedB.active
                onTriggered: fsmMixed.btoc()
            }

            transitions: ({
                btoc: mixedC
            })
        }

        Util.FSMState {
            id: mixedC
            objectName: "mixedC"
        }
    }

    function test_mixed_children() {
        fsmMixed.atob()
        tryCompare(mixedB, "active", true)
        wait(30)
        verify(mixedC.active, true)
    }

}
