/*****************************************************************************
 * openhmd.c: HMD module using OpenHMD
 *****************************************************************************
 * Copyright (C) 2002-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Adrien Maglo <magsoft@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>
#include <locale.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_hmd.h>
#include <vlc_viewpoint.h>
#include <vlc_tick.h>

#include <openhmd/openhmd.h>


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *p_this);
static void Close(vlc_object_t *);
static int headTrackingCallback(vlc_object_t *p_this, char const *psz_var,
                                vlc_value_t oldval, vlc_value_t newval,
                                void *p_data);

/* vlc_hmd_driver_t API */
static vlc_viewpoint_t GetViewpoint(vlc_hmd_driver_t *driver);
static vlc_hmd_state_e GetState(vlc_hmd_driver_t *driver);
static vlc_hmd_cfg_t GetConfig(vlc_hmd_driver_t *driver);

#define HEAD_TRACKING_TEXT N_("No head tracking")
#define HEAD_TRACKING_LONGTEXT N_("Disable the HMD head tracking")

#define PREFIX "openhmd-"

vlc_module_begin()
    set_shortname(N_("OpenHMD"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_description(N_("OpenHMD head mounted display handler"))
    set_capability("hmd driver", 10)

    add_bool(PREFIX "no-head-tracking", false,
             HEAD_TRACKING_TEXT, HEAD_TRACKING_LONGTEXT, false)

    add_shortcut("openhmd")
    set_callbacks(Open, Close)
vlc_module_end()


struct vlc_hmd_driver_sys_t
{
    vlc_thread_t thread;
    vlc_mutex_t lock;
    vlc_cond_t thread_cond;
    bool b_thread_running;
    bool b_init_completed;
    bool b_init_successfull;
    bool b_headTracking;

    ohmd_context* ctx;
    ohmd_device* hmd;

    vlc_mutex_t vp_lock;
    vlc_viewpoint_t vp;

    vlc_hmd_cfg_t cfg;
};


static void Release(vlc_hmd_driver_t *p_hmd);
static void *HMDThread(void *p_data);


static int Open(vlc_object_t *p_this)
{
    vlc_hmd_driver_t *p_hmd = p_this;

    struct vlc_hmd_driver_sys_t *sys =
    p_hmd->sys = vlc_obj_calloc(p_this, 1, sizeof(*sys));

    if (unlikely(p_hmd->sys == NULL))
        return VLC_ENOMEM;

    vlc_mutex_init(&sys->lock);
    vlc_cond_init(&sys->thread_cond);

    vlc_mutex_init(&sys->vp_lock);

    sys->b_thread_running = true;
    sys->b_init_completed = false;

    if (vlc_clone(&sys->thread, HMDThread, p_hmd, 0) != VLC_SUCCESS)
    {
        Release(p_hmd);
        return VLC_EGENERIC;
    }

    sys->b_headTracking = !var_CreateGetBool(p_hmd, PREFIX "no-head-tracking");

    var_AddCallback(p_hmd, PREFIX "no-head-tracking",
                    headTrackingCallback, sys);

    // Wait for the OpenHMD initialization in its thread to complete.
    // Return an error and release resources if needed.
    int i_ret;

    while (true)
    {
        vlc_mutex_lock(&sys->lock);
        vlc_cond_wait(&sys->thread_cond, &sys->lock);

        if (sys->b_init_completed)
        {
            if (sys->b_init_successfull)
            {
                i_ret = VLC_SUCCESS;
                vlc_mutex_unlock(&sys->lock);
            }
            else
            {
                i_ret = VLC_EGENERIC;
                vlc_mutex_unlock(&sys->lock);
                vlc_join(sys->thread, NULL);
                Release(p_hmd);
            }
            break;
        }

        vlc_mutex_unlock(&sys->lock);
    }

    p_hmd->get_viewpoint = GetViewpoint;
    p_hmd->get_state = GetState;
    p_hmd->get_config = GetConfig;

    return i_ret;
}


static void Close(vlc_object_t *p_this)
{
    vlc_hmd_driver_t* p_hmd = (vlc_hmd_driver_t*)p_this;
    struct vlc_hmd_driver_sys_t* sys = p_hmd->sys;

    vlc_mutex_lock(&sys->lock);
    sys->b_thread_running = false;
    vlc_cond_signal(&sys->thread_cond);
    vlc_mutex_unlock(&sys->lock);
    vlc_join(sys->thread, NULL);

    Release(p_hmd);
}


static void Release(vlc_hmd_driver_t* p_hmd)
{
    struct vlc_hmd_driver_sys_t* sys = p_hmd->sys;

    var_DelCallback(p_hmd, PREFIX "no-head-tracking", headTrackingCallback, sys);
    var_Destroy(p_hmd, PREFIX "no-head-tracking");

    vlc_mutex_destroy(&sys->lock);
    vlc_cond_destroy(&sys->thread_cond);
}


/* Quaternion to Euler conversion.
 * Original code from:
 * http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToEuler/ */
static void quaternionToEuler(float *q, vlc_viewpoint_t *vp)
{
    float sqx = q[0] * q[0];
    float sqy = q[1] * q[1];
    float sqz = q[2] * q[2];
    float sqw = q[3] * q[3];

    float unit = sqx + sqy + sqz + sqw; // if normalised is one, otherwise is correction factor
    float test = q[0] * q[1] + q[2] * q[3];

    if (test > 0.499 * unit)
    {
        // singularity at north pole
        vp->yaw = 2 * atan2(q[0], q[3]);
        vp->roll = M_PI / 2;
        vp->pitch = 0;
    }
    else if (test < -0.499 * unit)
    {
        // singularity at south pole
        vp->yaw = -2 * atan2(q[0], q[3]);
        vp->roll = -M_PI / 2;
        vp->pitch = 0;
    }
    else
    {
        vp->yaw = atan2(2 * q[1] * q[3] - 2 * q[0] * q[2], sqx - sqy - sqz + sqw);
        vp->roll = asin(2 * test / unit);
        vp->pitch = atan2(2 * q[0] * q[3] - 2 * q[1] * q[2], -sqx + sqy - sqz + sqw);
    }

    vp->yaw = -vp->yaw * 180 / M_PI;
    vp->pitch = -vp->pitch * 180 / M_PI;
    vp->roll = vp->roll * 180 / M_PI;
    vp->fov = FIELD_OF_VIEW_DEGREES_DEFAULT;
}

static void* HMDThread(void *p_data)
{
    vlc_hmd_driver_t* p_hmd = (vlc_hmd_driver_t*)p_data;
    struct vlc_hmd_driver_sys_t* sys = (struct vlc_hmd_driver_sys_t*)p_hmd->sys;
    bool b_init_successfull = true;

    sys->ctx = ohmd_ctx_create();

    // We should rather use the thread-safe uselocale but it is not avaible on Windows...
    setlocale(LC_ALL, "C");

    int num_devices = ohmd_ctx_probe(sys->ctx);
    if (num_devices < 0) {
        msg_Err(p_hmd, "Failed to probe devices: %s", ohmd_ctx_get_error(sys->ctx));
        ohmd_ctx_destroy(sys->ctx);
        b_init_successfull = false;
    }

    if (b_init_successfull)
    {
        ohmd_device_settings* settings = ohmd_device_settings_create(sys->ctx);

        int auto_update = 1;
        ohmd_device_settings_seti(settings, OHMD_IDS_AUTOMATIC_UPDATE, &auto_update);

        sys->hmd = ohmd_list_open_device_s(sys->ctx, 0, settings);

        ohmd_device_settings_destroy(settings);

        if (sys->hmd == NULL)
        {
            msg_Err(p_hmd, "Failed to open device: %s", ohmd_ctx_get_error(sys->ctx));
            ohmd_ctx_destroy(sys->ctx);
            b_init_successfull = false;
        }
    }

    if (b_init_successfull)
    {
        ohmd_device_geti(sys->hmd, OHMD_SCREEN_HORIZONTAL_RESOLUTION,
                         &p_hmd->cfg.i_screen_width);
        ohmd_device_geti(sys->hmd, OHMD_SCREEN_VERTICAL_RESOLUTION,
                         &p_hmd->cfg.i_screen_height);

        ohmd_device_getf(sys->hmd, OHMD_SCREEN_HORIZONTAL_SIZE,
                         &p_hmd->cfg.viewport_scale[0]);
        ohmd_device_getf(sys->hmd, OHMD_SCREEN_VERTICAL_SIZE,
                         &p_hmd->cfg.viewport_scale[1]);
        p_hmd->cfg.viewport_scale[0] /= 2.0f;

        ohmd_device_getf(sys->hmd, OHMD_UNIVERSAL_DISTORTION_K,
                         p_hmd->cfg.distorsion_coefs);
        ohmd_device_getf(sys->hmd, OHMD_UNIVERSAL_ABERRATION_K,
                         p_hmd->cfg.aberr_scale);

        float sep;
        ohmd_device_getf(sys->hmd, OHMD_LENS_HORIZONTAL_SEPARATION, &sep);

        p_hmd->cfg.left.lens_center[0] =
            p_hmd->cfg.viewport_scale[0] - sep / 2.0f;
        ohmd_device_getf(sys->hmd, OHMD_LENS_VERTICAL_POSITION,
                         &p_hmd->cfg.left.lens_center[1]);

        p_hmd->cfg.right.lens_center[0] = sep / 2.0f;
        ohmd_device_getf(sys->hmd, OHMD_LENS_VERTICAL_POSITION,
                         &p_hmd->cfg.right.lens_center[1]);

        // Asume calibration was for lens view to which ever edge of screen is further away from lens center
        if (p_hmd->cfg.left.lens_center[0] > p_hmd->cfg.right.lens_center[0])
            p_hmd->cfg.warp_scale = p_hmd->cfg.left.lens_center[0];
        else
            p_hmd->cfg.warp_scale = p_hmd->cfg.right.lens_center[0];

        p_hmd->cfg.warp_adj = 1.0f;

        ohmd_device_getf(sys->hmd, OHMD_LEFT_EYE_GL_PROJECTION_MATRIX,
                         p_hmd->cfg.left.projection);
        ohmd_device_getf(sys->hmd, OHMD_RIGHT_EYE_GL_PROJECTION_MATRIX,
                         p_hmd->cfg.right.projection);

        ohmd_device_getf(sys->hmd, OHMD_LEFT_EYE_GL_MODELVIEW_MATRIX,
                         p_hmd->cfg.left.modelview);
        ohmd_device_getf(sys->hmd, OHMD_RIGHT_EYE_GL_MODELVIEW_MATRIX,
                         p_hmd->cfg.right.modelview);
    }

    vlc_mutex_lock(&sys->lock);
    sys->b_init_completed = true;
    sys->b_init_successfull = b_init_successfull;
    vlc_cond_signal(&sys->thread_cond);
    vlc_mutex_unlock(&sys->lock);

    // Quit the thread in the case of an initialization error.
    if (!b_init_successfull)
        return NULL;

    /* Main OpenHMD thread. */
    while (true)
    {
        ohmd_ctx_update(sys->ctx);

        vlc_viewpoint_t vp;
        if (sys->b_headTracking)
            ohmd_device_getf(sys->hmd, OHMD_ROTATION_QUAT, vp.quat);

        quaternionToEuler(vp.quat, &vp);

        vlc_mutex_lock(&sys->vp_lock);
        sys->vp = vp;
        vlc_mutex_unlock(&sys->vp_lock);

        vlc_mutex_lock(&sys->lock);
        vlc_tick_t timeout = vlc_tick_now() + 3000;
        vlc_cond_timedwait(&sys->thread_cond, &sys->lock, timeout);

        if (!sys->b_thread_running)
        {
            vlc_mutex_unlock(&sys->lock);
            break;
        }

        vlc_mutex_unlock(&sys->lock);
    }

    ohmd_ctx_destroy(sys->ctx);

    /* Ugly hack: sleep to be sure the device is closed correctly.
     * This fix an issue with the Vive that does not switch on when it
     * has just been switched on by a previous instance of the module. */
    //msleep(500000);

    return NULL;
}


static int headTrackingCallback(vlc_object_t *p_this, char const *psz_var,
                                vlc_value_t oldval, vlc_value_t newval,
                                void *p_data)
{
    VLC_UNUSED(p_this);
    VLC_UNUSED(psz_var);
    VLC_UNUSED(oldval);
    struct vlc_hmd_driver_sys_t *sys = (struct vlc_hmd_driver_sys_t *)p_data;

    vlc_mutex_lock( &sys->lock );
    sys->b_headTracking = !newval.b_bool;
    vlc_mutex_unlock( &sys->lock );

    return VLC_SUCCESS;
}

static vlc_viewpoint_t GetViewpoint(vlc_hmd_driver_t *driver)
{
    struct vlc_hmd_driver_sys_t *sys = driver->sys;

    vlc_mutex_lock( &sys->vp_lock );
    vlc_viewpoint_t vp = sys->vp;
    vlc_mutex_unlock( &sys->vp_lock );
    return vp;
}

static vlc_hmd_state_e GetState(vlc_hmd_driver_t *driver)
{
    struct vlc_hmd_driver_sys_t *sys = driver->sys;

    // TODO
    return VLC_HMD_STATE_ENABLED;
}

static vlc_hmd_cfg_t GetConfig(vlc_hmd_driver_t *driver)
{
    return driver->cfg;
}
