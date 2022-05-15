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
#include <vlc_window.h>

#include <assert.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#define KMS_DEVICE_VAR "kms-device"

#define DEVICE_TEXT "Framebuffer device"
#define DEVICE_LONGTEXT \
    "Framebuffer device to use for rendering (usually /dev/dri/card0 or /dev/drm0)."

#define KMS_CONNECTOR_TEXT "DRM Connector"
#define KMS_CONNECTOR_LONGTEXT \
    "The DRM connector to use, using the naming conn-i where <conn> is the " \
    "connector type name and <i> is the connector type id"

typedef enum { drvSuccess, drvTryNext, drvFail } deviceRval;

static const char * const connector_type_names[] = {
    [DRM_MODE_CONNECTOR_Unknown]      = "Unknown",
    [DRM_MODE_CONNECTOR_VGA]          = "VGA",
    [DRM_MODE_CONNECTOR_DVII]         = "DVI-I",
    [DRM_MODE_CONNECTOR_DVID]         = "DVI-D",
    [DRM_MODE_CONNECTOR_DVIA]         = "DVI-A",
    [DRM_MODE_CONNECTOR_Composite]    = "Composite",
    [DRM_MODE_CONNECTOR_SVIDEO]       = "SVIDEO",
    [DRM_MODE_CONNECTOR_LVDS]         = "LVDS",
    [DRM_MODE_CONNECTOR_Component]    = "Component",
    [DRM_MODE_CONNECTOR_9PinDIN]      = "DIN",
    [DRM_MODE_CONNECTOR_DisplayPort]  = "DP",
    [DRM_MODE_CONNECTOR_HDMIA]        = "HDMI-A",
    [DRM_MODE_CONNECTOR_HDMIB]        = "HDMI-B",
    [DRM_MODE_CONNECTOR_TV]           = "TV",
    [DRM_MODE_CONNECTOR_eDP]          = "eDP",
    [DRM_MODE_CONNECTOR_VIRTUAL]      = "Virtual",
    [DRM_MODE_CONNECTOR_DSI]          = "DSI",
    [DRM_MODE_CONNECTOR_DPI]          = "DPI",
    [DRM_MODE_CONNECTOR_WRITEBACK]    = "Writeback",
#ifdef DRM_MODE_CONNECTOR_SPI
    [DRM_MODE_CONNECTOR_SPI]          = "SPI",
#endif
#ifdef DRM_MODE_CONNECTOR_USB
    [DRM_MODE_CONNECTOR_USB]          = "USB",
#endif
};


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
    drmModeModeInfo *mode;
    drmModeConnector *conn;
    uint32_t connector;
    drmModeCrtc *saved_crtc;

    uint32_t framebuffer;
    uint32_t main_buffer;
};

static deviceRval FindCRTC(vlc_window_t *wnd, drmModeRes const *res,
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
            if ((enc->possible_crtcs & (1 << j)) != 0 && res->crtcs[j]) {
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


static deviceRval SetupDevice(vlc_window_t *wnd, drmModeRes const *res,
                             drmModeConnector const *conn)
{
    struct vout_window_sys_t *sys = wnd->sys;
    deviceRval ret;

    if (conn->connection != DRM_MODE_CONNECTED || conn->count_modes == 0)
        return drvTryNext;

    sys->width = conn->modes[0].hdisplay;
    sys->height = conn->modes[0].vdisplay;
    sys->mode = &conn->modes[0];
    msg_Dbg(wnd, "Mode resolution for connector %u is %ux%u",
            conn->connector_id, sys->width, sys->height);

    ret = FindCRTC(wnd, res, conn);
    if (ret != drvSuccess) {
        msg_Dbg(wnd, "No valid CRTC for connector %d", conn->connector_id);
        return ret;
    }
    return drvSuccess;
}

static void UpdateOutputs(vlc_window_t *wnd)
{
    struct vout_window_sys_t *sys = wnd->sys;
    drmModeRes *modeRes = sys->modeRes;

    msg_Info(wnd, "Updating list of connectors:");
    for (int c = 0; c < modeRes->count_connectors && sys->crtc == 0; c++)
    {
        drmModeConnector *conn =
            drmModeGetConnector(sys->drm_fd, modeRes->connectors[c]);
        if (conn == NULL)
            continue;

        if (conn->connector_type >= ARRAY_SIZE(connector_type_names) ||
            conn->connection != DRM_MODE_CONNECTED)
        {
            drmModeFreeConnector(conn);
            continue;
        }

        char name[32];
        snprintf(name, sizeof name, "%s-%d",
                 connector_type_names[conn->connector_type],
                 conn->connector_type_id);

        /* Iterate all available encoders to find CRTC */
        for (int i = 0; i < conn->count_encoders; i++)
        {
            drmModeEncoder *enc =
                drmModeGetEncoder(sys->drm_fd, conn->encoders[i]);

            size_t crtc_index = sys->crtc;
            if ((enc->possible_crtcs & (1 << crtc_index)) != 0) {

                drmModeFreeEncoder(enc);
                vlc_window_ReportOutputDevice(wnd, name, name);
                break;
            }
            drmModeFreeEncoder(enc);
        }
        drmModeFreeConnector(conn);
    }
}

static int WindowEnable(vlc_window_t *wnd, const vlc_window_cfg_t *cfg)
{
    struct vout_window_sys_t *sys = wnd->sys;
    (void)cfg;
    drmModeRes *modeRes = sys->modeRes;

    bool found_connector = false;

    // TODO: use kms-connector as a list of connector for clones
    char *connector_request = var_InheritString(wnd, "kms-connector");
    msg_Info(wnd, "Looping over %d resources", modeRes->count_connectors);
    for (int c = 0; c < modeRes->count_connectors && sys->crtc == 0; c++) {

        drmModeConnector *conn =
            drmModeGetConnector(sys->drm_fd, modeRes->connectors[c]);
        if (conn == NULL)
            continue;

        if (conn->connector_type >= ARRAY_SIZE(connector_type_names))
        {
            drmModeFreeConnector(conn);
            continue;
        }

        char name[32];
        snprintf(name, sizeof name, "%s-%d",
                 connector_type_names[conn->connector_type],
                 conn->connector_type_id);

        if (connector_request != NULL && strcmp(name, connector_request) != 0)
        {
            msg_Info(wnd, "connector %s skipped", name);
            drmModeFreeConnector(conn);
            continue;
        }

        msg_Info(wnd, "connector %d: %s", c, name);

        found_connector = true;

        int ret = SetupDevice(wnd, modeRes, conn);
        if (ret != drvSuccess) {
            if (ret != drvTryNext) {
                msg_Err(wnd, "Cannot do setup for connector %s %u:%u",
                        name, c, modeRes->connectors[c]);

                drmModeFreeConnector(conn);
                drmModeFreeResources(modeRes);
                free(connector_request);
                return VLC_EGENERIC;
            }
            drmModeFreeConnector(conn);
            found_connector = false;
            msg_Dbg(wnd, " - Connector %d: could not setup device", c);
            continue;
        }
        sys->connector = conn->connector_id;
        sys->conn = conn;
        break;
    }
    free(connector_request);

    if (!found_connector)
    {
        msg_Warn(wnd, "Could not find a valid connector");
        return VLC_EGENERIC;
    }

    wnd->handle.crtc = sys->crtc;
    /* Store current KMS state before modifying */
    sys->saved_crtc = drmModeGetCrtc(sys->drm_fd, sys->crtc);

    /*
     * Create a new framebuffer to avoid compositing the planes into the saved
     * framebuffer for the current CRTC.
     */

    struct drm_mode_create_dumb request = {
        .width = sys->width, .height = sys->height, .bpp = 32
    };

    int ret = drmIoctl(sys->drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &request);
    if (ret != drvSuccess)
        goto error_create_dumb;

    uint32_t new_fb;
    ret = drmModeAddFB(sys->drm_fd, sys->width, sys->height, 24, 32, request.pitch,
            request.handle, &new_fb);
    if (ret != drvSuccess)
        goto error_add_fb;

    ret = drmModeSetCrtc(sys->drm_fd, sys->crtc, new_fb, 0, 0, &sys->connector, 1, sys->mode);
    if (ret != drvSuccess)
        goto error_set_crtc;

    sys->framebuffer = new_fb;
    sys->main_buffer = request.handle;

    vlc_window_ReportSize(wnd, sys->width, sys->height);

    return VLC_SUCCESS;

error_set_crtc:
    drmModeRmFB(sys->drm_fd, new_fb);

error_add_fb:
    {
        struct drm_mode_destroy_dumb destroy_request = {
            .handle = request.handle
        };
        ret = drmIoctl(sys->drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_request);
        /* This must be a programmation error if we cannot destroy the resources
         * we created. */
        assert(ret == drvSuccess);
    }

error_create_dumb:
    if (sys->saved_crtc != NULL)
        drmModeFreeCrtc(sys->saved_crtc);
    sys->saved_crtc = NULL;
    drmModeFreeConnector(sys->conn);
    sys->conn = NULL;

    return VLC_EGENERIC;
}

static void WindowDisable(vlc_window_t *wnd)
{
    struct vout_window_sys_t *sys = wnd->sys;
    sys->crtc = 0;

    /* Restore previous CRTC state */
    if (sys->saved_crtc)
    {
        int ret = drmModeSetCrtc(sys->drm_fd,
            sys->saved_crtc->crtc_id,
            sys->saved_crtc->buffer_id,
            sys->saved_crtc->x, sys->saved_crtc->y,
            &sys->connector, 1,
            &sys->saved_crtc->mode);
        assert(ret == drvSuccess);
        drmModeFreeCrtc(sys->saved_crtc);
    }
    sys->saved_crtc = NULL;

    drmModeFreeConnector(sys->conn);

    drmModeRmFB(sys->drm_fd, sys->framebuffer);

    struct drm_mode_destroy_dumb destroy_request = {
        .handle = sys->main_buffer,
    };
    int ret = drmIoctl(sys->drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_request);
    /* This must be a programmation error if we cannot destroy the resources
     * we created. */
    assert(ret == drvSuccess);
    (void)ret;
}

static void WindowClose(vlc_window_t *wnd)
{
    struct vout_window_sys_t *sys = wnd->sys;
    drmModeFreeResources(sys->modeRes);
    drmSetClientCap(sys->drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 0);
    drmDropMaster(sys->drm_fd);
    vlc_close(sys->drm_fd);
    free(sys);
}

static const struct vlc_window_operations window_ops =
{
    .destroy = WindowClose,
    .enable = WindowEnable,
    .disable = WindowDisable,
};

static int OpenWindow(vlc_window_t *wnd)
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
    psz_device = var_InheritString(wnd, KMS_DEVICE_VAR);
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

    drmVersionPtr version;
    if ((version = drmGetVersion(sys->drm_fd)) != NULL)
    {
        const char *date = version->date ? version->date : "unknown";
        const char *desc = version->desc ? version->desc : "unknown";

        msg_Dbg(wnd, "Using DRM driver %s version %d.%d.%d (build %s): %s",
                version->name, version->version_major, version->version_minor,
                version->version_patchlevel, date, desc);

        drmFreeVersion(version);
    }
    else
    {
        msg_Err(wnd, "device %s doesn't support DRM", psz_device);
        free(psz_device);
        goto error_drm;
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
    wnd->type = VLC_WINDOW_TYPE_KMS;
    wnd->display.drm_fd = sys->drm_fd;
    /* Note: wnd->handle.crtc will be initialized later */

    UpdateOutputs(wnd);

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

    add_obsolete_string("kms") /* Since 4.0.0 */
    add_loadfile(KMS_DEVICE_VAR, DRM_DIR_NAME "/" DRM_PRIMARY_MINOR_NAME "0",
                 DEVICE_TEXT, DEVICE_LONGTEXT)
    add_string("kms-connector", "", KMS_CONNECTOR_TEXT, KMS_CONNECTOR_LONGTEXT)

    set_description("Linux kernel mode setting window provider")
    set_callback(OpenWindow)
    set_capability("vout window", 9)

vlc_module_end ()
