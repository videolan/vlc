/*****************************************************************************
 * kms.c : kernel mode setting plugin for vlc
 *****************************************************************************
 * Copyright © 2018 Intel Corporation
 * Copyright © 2021 Videolabs
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <fcntl.h>
#include <sys/mman.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_fs.h>
#include <vlc_vout_window.h>

#include <assert.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#define KMS_VAR "kms"

#define DEVICE_TEXT "Framebuffer device"
#define DEVICE_LONGTEXT \
    "Framebuffer device to use for rendering (usually /dev/dri/card0)."

typedef enum { drvSuccess, drvTryNext, drvFail } deviceRval;

struct vout_window_sys_t {
    /* modeset information */
    uint32_t        crtc;

    /* other generic stuff */
    int             drm_fd;

    /*
     * buffer information
     */
    uint32_t        width;
    uint32_t        height;

    drmModeRes *modeRes;
};

static deviceRval FindCRTC(vout_window_t *wnd, drmModeRes const *res,
                             drmModeConnector const *conn)
{
    struct vout_window_sys_t *sys = wnd->sys;
    drmModeEncoder *enc;
    int i, j;

    /*
     * Try current encoder and CRTC combo
     */
    if (conn->encoder_id) {
        enc = drmModeGetEncoder(sys->drm_fd, conn->encoder_id);
        if (enc) {
            if (enc->crtc_id) {
                msg_Dbg(wnd, "Got CRTC %d from current encoder", enc->crtc_id);

                sys->crtc = enc->crtc_id;
                drmModeFreeEncoder(enc);
                return drvSuccess;
            }
            drmModeFreeEncoder(enc);
        }
    }

    /*
     * Iterate all available encoders to find CRTC
     */
    for (i = 0; i < conn->count_encoders; i++) {
        enc = drmModeGetEncoder(sys->drm_fd, conn->encoders[i]);

        for (j = 0; enc && j < res->count_crtcs; ++j) {
            if (ffs(enc->possible_crtcs) == j && res->crtcs[j]) {
                sys->crtc = res->crtcs[j];
                drmModeFreeEncoder(enc);
                return drvSuccess;
            }
        }
        drmModeFreeEncoder(enc);
    }

    msg_Err(wnd , "Cannot find CRTC for connector %d", conn->connector_id);
    return drvTryNext;
}


static deviceRval SetupDevice(vout_window_t *wnd, drmModeRes const *res,
                             drmModeConnector const *conn)
{
    struct vout_window_sys_t *sys = wnd->sys;
    deviceRval ret;

    if (conn->connection != DRM_MODE_CONNECTED || conn->count_modes == 0)
        return drvTryNext;

    sys->width = conn->modes[0].hdisplay;
    sys->height = conn->modes[0].vdisplay;
    msg_Dbg(wnd, "Mode resolution for connector %u is %ux%u",
            conn->connector_id, sys->width, sys->height);

    ret = FindCRTC(wnd, res, conn);
    if (ret != drvSuccess) {
        msg_Dbg(wnd, "No valid CRTC for connector %d", conn->connector_id);
        return ret;
    }
    return drvSuccess;
}

static int WindowEnable(vout_window_t *wnd, const vout_window_cfg_t *cfg)
{
    struct vout_window_sys_t *sys = wnd->sys;
    drmModeRes *modeRes = sys->modeRes;

    bool found_connector = false;

    msg_Info(wnd, "Looping over %d resources", modeRes->count_connectors);
    for (int c = 0; c < modeRes->count_connectors && sys->crtc == 0; c++) {
        msg_Info(wnd, "connector %d", c);

        drmModeConnector *conn =
            drmModeGetConnector(sys->drm_fd, modeRes->connectors[c]);
        if (conn == NULL)
            continue;

        found_connector = true;

        int ret = SetupDevice(wnd, modeRes, conn);
        if (ret != drvSuccess) {
            if (ret != drvTryNext) {
                msg_Err(wnd, "Cannot do setup for connector %u:%u", c,
                        modeRes->connectors[c]);

                drmModeFreeConnector(conn);
                drmModeFreeResources(modeRes);
                return VLC_EGENERIC;
            }
            drmModeFreeConnector(conn);
            found_connector = false;
            msg_Dbg(wnd, " - Connector %d: could not setup device", c);
            continue;
        }
        drmModeFreeConnector(conn);
    }

    if (!found_connector)
    {
        msg_Warn(wnd, "Could not find a valid connector");
        return VLC_EGENERIC;
    }

    wnd->handle.crtc = sys->crtc;

    vout_window_ReportSize(wnd, sys->width, sys->height);

    return VLC_SUCCESS;
}

static void WindowDisable(vout_window_t *wnd)
{
    struct vout_window_sys_t *sys = wnd->sys;
    sys->crtc = 0;
}

static void WindowClose(vout_window_t *wnd)
{
    struct vout_window_sys_t *sys = wnd->sys;
    drmModeFreeResources(sys->modeRes);
    drmSetClientCap(sys->drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 0);
    drmDropMaster(sys->drm_fd);
    vlc_close(sys->drm_fd);
    free(sys);
}

static const struct vout_window_operations window_ops =
{
    .destroy = WindowClose,
    .enable = WindowEnable,
    .disable = WindowDisable,
};

static int OpenWindow(vout_window_t *wnd)
{
    char *psz_device;


    struct vout_window_sys_t *sys
        = wnd->sys
        = malloc(sizeof *sys);
    if (sys == NULL)
        return VLC_ENOMEM;

    sys->crtc = 0;

    /*
     * Open framebuffer device
     */
    psz_device = var_InheritString(wnd, KMS_VAR);
    if (psz_device == NULL) {
        msg_Err(wnd, "Don't know which DRM device to open");
        goto error_end;
    }

    sys->drm_fd = vlc_open(psz_device, O_RDWR);
    if (sys->drm_fd == -1) {
        msg_Err(wnd, "cannot open %s", psz_device);
        free(psz_device);
        goto error_end;
    }
    free(psz_device);

    drmSetClientCap(sys->drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

    sys->modeRes = drmModeGetResources(sys->drm_fd);
    if (sys->modeRes == NULL) {
        msg_Err(wnd, "Didn't get DRM resources");
        drmSetClientCap(sys->drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 0);
        goto error_drm;
    }

    wnd->ops = &window_ops;
    wnd->type = VOUT_WINDOW_TYPE_KMS;
    wnd->display.drm_fd = sys->drm_fd;
    /* Note: wnd->handle.crtc will be initialized later */

    return VLC_SUCCESS;
error_drm:
    drmDropMaster(sys->drm_fd);
    vlc_close(sys->drm_fd);

error_end:
    free(sys);
    return VLC_EGENERIC;
}


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_shortname("kms")
    set_subcategory(SUBCAT_VIDEO_VOUT)
    add_loadfile(KMS_VAR, "/dev/dri/card0", DEVICE_TEXT, DEVICE_LONGTEXT)

    set_description("Linux kernel mode setting window provider")
    set_callback(OpenWindow)
    set_capability("vout window", 10)

vlc_module_end ()
