/*****************************************************************************
 * kms_drm.c : Direct rendering management plugin for vlc
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

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_picture.h>
#include <vlc_vout_window.h>
#include "vlc_drm.h"

#include <assert.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

#define DRM_CHROMA_TEXT "Image format used by DRM"
#define DRM_CHROMA_LONGTEXT "Chroma fourcc override for DRM framebuffer format selection"

typedef enum { drvSuccess, drvTryNext, drvFail } deviceRval;

/*
 * how many hw buffers are allocated for page flipping. I think
 * 3 is enough so we shouldn't get unexpected stall from kernel.
 */
#define   MAXHWBUF 3

typedef struct vout_display_sys_t {
    picture_t       *buffers[MAXHWBUF];

    unsigned int    front_buf;

    bool            forced_drm_fourcc;
    uint32_t        drm_fourcc;

/*
 * modeset information
 */
    uint32_t        plane_id;
} vout_display_sys_t;

/** fourccmatching, matching drm to vlc fourccs and see if it was present
 * in HW. Here really is two lists, one in RGB and second in YUV. They're
 * listed in order of preference.
 *
 * fourccmatching::drm DRM fourcc code from drm_fourcc.h
 * fourccmatching::plane_id from which plane this DRM fourcc was found
 * fourccmatching::present if this mode was available in HW
 * fourccmatching::isYUV as name suggest..
 */
static struct
{
    uint32_t     drm;
    uint32_t     plane_id;
    bool         present;
    bool         isYUV;
} fourccmatching[] = {
    { .drm = DRM_FORMAT_XRGB8888, .isYUV = false },
    { .drm = DRM_FORMAT_RGB565, .isYUV = false },
#if defined DRM_FORMAT_P010
    { .drm = DRM_FORMAT_P010, .isYUV = true },
#endif
    { .drm = DRM_FORMAT_NV12, .isYUV = true },
    { .drm = DRM_FORMAT_YUYV, .isYUV = true },
    { .drm = DRM_FORMAT_YVYU, .isYUV = true },
    { .drm = DRM_FORMAT_UYVY, .isYUV = true },
    { .drm = DRM_FORMAT_VYUY, .isYUV = true },
};


static void CheckFourCCList(uint32_t drmfourcc, uint32_t plane_id)
{
    unsigned i;

    for (i = 0; i < ARRAY_SIZE(fourccmatching); i++) {
        if (fourccmatching[i].drm == drmfourcc) {
            if (fourccmatching[i].present)
                /* this drmfourcc already has a plane_id found where it
                 * could be used, we'll stay with earlier findings.
                 */
                break;

            fourccmatching[i].present = true;
            fourccmatching[i].plane_id = plane_id;
            break;
        }
    }
}

static vlc_fourcc_t ChromaNegotiation(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    vout_window_t *wnd = vd->cfg->window;

    unsigned i, c, propi;
    uint32_t planetype;
    const char types[][16] = { "OVERLAY", "PRIMARY", "CURSOR", "UNKNOWN" };
    drmModePlaneRes *plane_res = NULL;
    drmModePlane *plane;
    drmModeObjectProperties *props;
    drmModePropertyPtr pp;
    bool YUVFormat;

    int drm_fd = wnd->display.drm_fd;

    drmModeRes *resources = drmModeGetResources(drm_fd);
    if (resources == NULL)
        return 0;

    int crtc_index = -1;
    for (int crtc_id=0; crtc_id < resources->count_crtcs; ++crtc_id)
    {
        if (resources->crtcs[crtc_id] == wnd->handle.crtc)
        {
            crtc_index = crtc_id;
            break;
        }
    }
    drmModeFreeResources(resources);

    /*
     * For convenience print out in debug prints all supported
     * DRM modes so they can be seen if use verbose mode.
     */
    plane_res = drmModeGetPlaneResources(drm_fd);
    sys->plane_id = 0;

    if (plane_res != NULL && plane_res->count_planes > 0) {
        msg_Dbg(vd, "List of DRM supported modes on this machine:");
        for (c = 0; c < plane_res->count_planes; c++) {

            plane = drmModeGetPlane(drm_fd, plane_res->planes[c]);
            if (plane != NULL && plane->count_formats > 0) {
                if ((plane->possible_crtcs & (1 << crtc_index)) == 0)
                {
                    drmModeFreePlane(plane);
                    continue;
                }

                props = drmModeObjectGetProperties(drm_fd,
                                                   plane->plane_id,
                                                   DRM_MODE_OBJECT_PLANE);

                planetype = 3;
                pp = NULL;
                for (propi = 0; propi < props->count_props; propi++) {
                    pp = drmModeGetProperty(drm_fd, props->props[propi]);
                    if (strcmp(pp->name, "type") == 0) {
                        break;
                    }
                    drmModeFreeProperty(pp);
                }

                if (pp != NULL) {
                    drmModeFreeProperty(pp);
                    planetype = props->prop_values[propi];
                }

                for (i = 0; i < plane->count_formats; i++) {
                    CheckFourCCList(plane->formats[i], plane->plane_id);

                    if (sys->forced_drm_fourcc && sys->plane_id == 0 &&
                            plane->formats[i] == sys->drm_fourcc) {
                        sys->plane_id = plane->plane_id;
                    }

                    /*
                     * we don't advertise about cursor plane because
                     * of its special limitations.
                     */
                    if (planetype != DRM_PLANE_TYPE_CURSOR) {
                        msg_Dbg(vd, "plane id %d type %s pipe %c format %2d: %.4s",
                                plane->plane_id, types[planetype],
                                ('@'+ffs(plane->possible_crtcs)), i,
                                (char*)&plane->formats[i]);
                    }
                }
                drmModeFreePlane(plane);
                drmModeFreeObjectProperties(props);
            } else {
                msg_Err(vd, "Couldn't get list of DRM formats");
                drmModeFreePlaneResources(plane_res);
                return 0;
            }
        }
        drmModeFreePlaneResources(plane_res);
    }

    vlc_fourcc_t fourcc = vd->source->i_chroma;

    if (sys->forced_drm_fourcc) {
        for (c = i = 0; c < ARRAY_SIZE(fourccmatching); c++) {
            if (fourccmatching[c].drm == sys->drm_fourcc) {
                fourcc = vlc_fourcc_drm(sys->drm_fourcc);
                break;
            }
        }
        if (sys->plane_id == 0) {
            msg_Err(vd, "Forced DRM fourcc (%.4s) not available in kernel.",
                    (char*)&sys->drm_fourcc);
            return 0;
        }
        return fourcc;
    }

    /*
     * favor yuv format according to YUVFormat flag.
     * check for exact match first, then YUVFormat and then !YUVFormat
     */
    uint_fast32_t drm_fourcc = vlc_drm_format(vd->source);

    for (c = i = 0; c < ARRAY_SIZE(fourccmatching); c++) {
        if (fourccmatching[c].drm == drm_fourcc) {
            if (!sys->forced_drm_fourcc && fourccmatching[c].present) {
                sys->drm_fourcc = fourccmatching[c].drm;
                sys->plane_id = fourccmatching[c].plane_id;
             }

            if (!sys->drm_fourcc) {
                msg_Err(vd, "Forced VLC fourcc (%.4s) not matching anything available in kernel, please set manually",
                        (char*)&fourcc);
                return 0;
            }
            return fourcc;
        }
    }

    YUVFormat = vlc_fourcc_IsYUV(fourcc);
    for (c = i = 0; c < ARRAY_SIZE(fourccmatching); c++) {
        if (fourccmatching[c].isYUV == YUVFormat
                && fourccmatching[c].present) {
            if (!sys->forced_drm_fourcc) {
                sys->drm_fourcc = fourccmatching[c].drm;
                sys->plane_id = fourccmatching[c].plane_id;
             }

            return vlc_fourcc_drm(fourccmatching[c].drm);
        }
    }

    for (i = 0; c < ARRAY_SIZE(fourccmatching); c++) {
        if (!fourccmatching[c].isYUV != YUVFormat
                && fourccmatching[c].present) {
            if (!sys->forced_drm_fourcc) {
                sys->drm_fourcc = fourccmatching[c].drm;
                sys->plane_id = fourccmatching[c].plane_id;
             }

            return vlc_fourcc_drm(fourccmatching[c].drm);
        }
    }

    return 0;
}

static int Control(vout_display_t *vd, int query)
{
    (void) vd;

    switch (query) {
        case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
        case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        case VOUT_DISPLAY_CHANGE_ZOOM:
        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
            return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}


static void Prepare(vout_display_t *vd, picture_t *pic, subpicture_t *subpic,
                    vlc_tick_t date)
{
    VLC_UNUSED(subpic); VLC_UNUSED(date);
    vout_display_sys_t *sys = vd->sys;

    picture_Copy(sys->buffers[sys->front_buf], pic);
}

static void Display(vout_display_t *vd, picture_t *picture)
{
    VLC_UNUSED(picture);
    vout_display_sys_t *sys = vd->sys;
    vout_window_t *wnd = vd->cfg->window;
    picture_t *pic = sys->buffers[sys->front_buf];
    uint32_t fb_id = vlc_drm_dumb_get_fb_id(pic);

    vout_display_place_t place;
    vout_display_PlacePicture(&place, vd->fmt, vd->cfg);

    int ret = drmModeSetPlane(wnd->display.drm_fd,
            sys->plane_id, wnd->handle.crtc, fb_id, 0,
            place.x, place.y, place.width, place.height,
            vd->fmt->i_x_offset << 16, vd->fmt->i_y_offset << 16,
            vd->fmt->i_visible_width << 16, vd->fmt->i_visible_height << 16);
    if (ret != drvSuccess)
    {
        msg_Err(vd, "Cannot do set plane for plane id %u, fb %"PRIu32,
                sys->plane_id, fb_id);
        assert(ret != -EINVAL);
        return;
    }

    sys->front_buf++;
    if (sys->front_buf == MAXHWBUF)
        sys->front_buf = 0;
}

/**
 * Terminate an output method created by Open
 */
static void Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    for (size_t i = 0; i < ARRAY_SIZE(sys->buffers); i++)
        picture_Release(sys->buffers[i]);
}

static const struct vlc_display_operations ops = {
    .close = Close,
    .prepare = Prepare,
    .display = Display,
    .control = Control,
};

/**
 * This function allocates and initializes a KMS vout method.
 */
static int Open(vout_display_t *vd,
                video_format_t *fmtp, vlc_video_context *context)
{
    vout_display_sys_t *sys;
    uint32_t local_drm_chroma;
    video_format_t fmt;
    char *chroma;

    if (vd->cfg->window->type != VOUT_WINDOW_TYPE_KMS)
        return VLC_EGENERIC;

    /*
     * Allocate instance and initialize some members
     */
    vd->sys = sys = vlc_obj_calloc(VLC_OBJECT(vd), 1, sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    chroma = var_InheritString(vd, "kms-drm-chroma");
    if (chroma) {
        local_drm_chroma = VLC_FOURCC(chroma[0], chroma[1], chroma[2],
                                      chroma[3]);

        if (local_drm_chroma) {
            sys->forced_drm_fourcc = true;
            sys->drm_fourcc = local_drm_chroma;
            msg_Dbg(vd, "Setting DRM chroma to '%4s'", chroma);
        }
        else
            msg_Dbg(vd, "Chroma %4s invalid, using default", chroma);

        free(chroma);
        chroma = NULL;
    }

    int fd = vd->cfg->window->display.drm_fd;
    vlc_fourcc_t fourcc = ChromaNegotiation(vd);
    if (!fourcc)
        return VLC_EGENERIC;

    msg_Dbg(vd, "Using VLC chroma '%.4s', DRM chroma '%.4s'",
            (char*)&fourcc, (char*)&sys->drm_fourcc);

    video_format_ApplyRotation(&fmt, vd->fmt);
    fmt.i_chroma = fourcc;

    for (size_t i = 0; i < ARRAY_SIZE(sys->buffers); i++) {
        sys->buffers[i] = vlc_drm_dumb_alloc_fb(vd->obj.logger, fd, &fmt);
        if (sys->buffers[i] == NULL) {
            while (i > 0)
                picture_Release(sys->buffers[--i]);
            return -ENOBUFS;
        }
    }

    *fmtp = fmt;
    vd->ops = &ops;

    (void) context;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_shortname("drm")
    /* Keep kms here for compatibility with previous video output. */
    add_shortcut("drm", "kms_drm", "kms")
    set_subcategory(SUBCAT_VIDEO_VOUT)

    add_obsolete_string("kms-vlc-chroma") /* since 4.0.0 */
    add_string( "kms-drm-chroma", NULL, DRM_CHROMA_TEXT, DRM_CHROMA_LONGTEXT)
    set_description("Direct rendering management video output")
    set_callback_display(Open, 30)
vlc_module_end ()
