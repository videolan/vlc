/*****************************************************************************
 * Copyright © 2018 VLC authors and VideoLAN
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/**
 * \file iokit-inhibit.c
 * \brief macOS display and idle sleep inhibitor using IOKit
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_inhibit.h>

#include <IOKit/pwr_mgt/IOPMLib.h>

struct vlc_inhibit_sys
{
    // Activity IOPMAssertion to wake the display if sleeping
    IOPMAssertionID act_assertion_id;

    // Inhibition IOPMAssertion to keep display or machine from sleeping
    IOPMAssertionID inh_assertion_id;
};

static void UpdateInhibit(vlc_inhibit_t *ih, unsigned mask)
{
    vlc_inhibit_sys_t* sys = ih->p_sys;

    // Release existing inhibition, if any
    if (sys->inh_assertion_id != kIOPMNullAssertionID) {
        msg_Dbg(ih, "Releasing previous IOPMAssertion");
        if (IOPMAssertionRelease(sys->inh_assertion_id) != kIOReturnSuccess) {
            msg_Err(ih, "Failed releasing previous IOPMAssertion, "
                "not acquiring new one!");
        }
        sys->inh_assertion_id = kIOPMNullAssertionID;
    }

    // Order is important here, if we prevent display sleep, it means
    // we automatically prevent idle sleep too.

    IOReturn ret;
    if ((mask & VLC_INHIBIT_DISPLAY) == VLC_INHIBIT_DISPLAY) {

        // Display inhibition
        CFStringRef activity_reason = CFSTR("VLC video playback");

        msg_Dbg(ih, "Inhibiting display sleep");

        // Wake up display
        ret = IOPMAssertionDeclareUserActivity(activity_reason,
                                               kIOPMUserActiveLocal,
                                               &(sys->act_assertion_id));
        if (ret != kIOReturnSuccess) {
            msg_Warn(ih, "Failed to declare user activity (%i)", ret);
        }

        // Actual display inhibition assertion
        ret = IOPMAssertionCreateWithName(kIOPMAssertPreventUserIdleDisplaySleep,
                                          kIOPMAssertionLevelOn,
                                          activity_reason,
                                          &(sys->inh_assertion_id));

    } else if ((mask & VLC_INHIBIT_SUSPEND) == VLC_INHIBIT_SUSPEND) {

        // Idle sleep inhibition
        CFStringRef activity_reason = CFSTR("VLC audio playback");

        msg_Dbg(ih, "Inhibiting idle sleep");

        ret = IOPMAssertionCreateWithName(kIOPMAssertPreventUserIdleSystemSleep,
                                          kIOPMAssertionLevelOn,
                                          activity_reason,
                                          &(sys->inh_assertion_id));

    } else if ((mask & VLC_INHIBIT_NONE) == VLC_INHIBIT_NONE) {
        msg_Dbg(ih, "Removed previous inhibition");
        return;
    } else {
         msg_Warn(ih, "Unhandled inhibiton mask (%i)", mask);
         return;
    }

    if (ret != kIOReturnSuccess) {
        msg_Err(ih, "Failed creating IOPMAssertion (%i)", ret);
        return;
    }
}

static int OpenInhibit(vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;

    // Intialize module private storage
    vlc_inhibit_sys_t *sys = ih->p_sys =
            vlc_obj_malloc(obj, sizeof(vlc_inhibit_sys_t));
    if (unlikely(ih->p_sys == NULL))
        return VLC_ENOMEM;

    sys->act_assertion_id = kIOPMNullAssertionID;
    sys->inh_assertion_id = kIOPMNullAssertionID;

    ih->inhibit = UpdateInhibit;
    return VLC_SUCCESS;
}

static void CloseInhibit(vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t*)obj;
    vlc_inhibit_sys_t* sys = ih->p_sys;

    // Release remaining IOPMAssertion for inhibition, if any
    if (sys->inh_assertion_id != kIOPMNullAssertionID) {
        msg_Dbg(ih, "Releasing remaining IOPMAssertion (inhibition)");

        if (IOPMAssertionRelease(sys->inh_assertion_id) != kIOReturnSuccess) {
            msg_Warn(ih, "Failed releasing IOPMAssertion on termination");
        }
        sys->inh_assertion_id = kIOPMNullAssertionID;
    }

    // Release remaining IOPMAssertion for activity, if any
    if (sys->act_assertion_id != kIOPMNullAssertionID) {
        msg_Dbg(ih, "Releasing remaining IOPMAssertion (activity)");

        if (IOPMAssertionRelease(sys->act_assertion_id) != kIOReturnSuccess) {
            msg_Warn(ih, "Failed releasing IOPMAssertion on termination");
        }
        sys->act_assertion_id = kIOPMNullAssertionID;
    }
}

vlc_module_begin()
    set_shortname(N_("macOS sleep inhibition"))
    set_description(N_("macOS screen and idle sleep inhibition"))
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)
    set_capability("inhibit", 10)
    set_callbacks(OpenInhibit, CloseInhibit)
vlc_module_end()
