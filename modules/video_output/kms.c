/*****************************************************************************
 * kms.c : kernel mode setting plugin for vlc
 *****************************************************************************
 * Copyright © 2018 Intel Corporation
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
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>
#include <vlc_fs.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#define KMS_VAR "kms"

#define DEVICE_TEXT "Framebuffer device"
#define DEVICE_LONGTEXT \
    "Framebuffer device to use for rendering (usually /dev/dri/card0)."

#define VLC_CHROMA_TEXT "Image format used by VLC"
#define VLC_CHROMA_LONGTEXT "Chroma fourcc request to VLC for output format"

#define DRM_CHROMA_TEXT "Image format used by DRM"
#define DRM_CHROMA_LONGTEXT "Chroma fourcc override for DRM framebuffer format selection"

/*
 * how many hw buffers are allocated for page flipping. I think
 * 3 is enough so we shouldn't get unexpected stall from kernel.
 */
#define   MAXHWBUF 3

typedef enum { drvSuccess, drvTryNext, drvFail } deviceRval;

struct vout_display_sys_t {
/*
 * buffer information
 */
    uint32_t        width;
    uint32_t        height;
    uint32_t        stride;
    uint32_t        size;
    uint32_t        offsets[PICTURE_PLANE_MAX];

    uint32_t        handle[MAXHWBUF];
    uint8_t         *map[MAXHWBUF];

    uint32_t        fb[MAXHWBUF];
    picture_t       *picture;

    unsigned int    front_buf;

    bool            forced_drm_fourcc;
    uint32_t        drm_fourcc;
    vlc_fourcc_t    vlc_fourcc;

/*
 * modeset information
 */
    uint32_t        crtc;
    uint32_t        plane_id;

/*
 * other generic stuff
 */
    int             drm_fd;
};

typedef struct {
    vout_display_sys_t  *p_voutsys;
} picture_sys_t;


static void DestroyFB(vout_display_sys_t const *sys, uint32_t const buf)
{
    struct drm_mode_destroy_dumb destroy_req = { .handle = sys->handle[buf] };

    munmap(sys->map[buf], sys->size);
    drmModeRmFB(sys->drm_fd, sys->fb[buf]);
    drmIoctl(sys->drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
}

static deviceRval CreateFB(vout_display_t *vd, const int buf)
{
    vout_display_sys_t *sys = vd->sys;
    struct drm_mode_create_dumb create_req = { .width = sys->width,
                                               .height = sys->height,
                                               .bpp = 32 };
    struct drm_mode_destroy_dumb destroy_req;
    struct drm_mode_map_dumb modify_req = {};
    unsigned int tile_width = 512, tile_height = 16;
    deviceRval ret;
    uint32_t i;
    uint32_t offsets[] = {0,0,0,0}, handles[] = {0,0,0,0},
            pitches[] = {0,0,0,0};

    switch(sys->drm_fourcc) {
#ifdef DRM_FORMAT_P010
    case DRM_FORMAT_P010:
#endif
#ifdef DRM_FORMAT_P012
    case DRM_FORMAT_P012:
#endif
#ifdef DRM_FORMAT_P016
    case DRM_FORMAT_P016:
#endif
#if defined(DRM_FORMAT_P010) || defined(DRM_FORMAT_P012) || defined(DRM_FORMAT_P016)
        sys->stride = vlc_align(sys->width*2, tile_width);
        sys->offsets[1] = sys->stride*vlc_align(sys->height, tile_height);
        create_req.height = 2*vlc_align(sys->height, tile_height);
        break;
#endif
    case DRM_FORMAT_NV12:
        sys->stride = vlc_align(sys->width, tile_width);
        sys->offsets[1] = sys->stride*vlc_align(sys->height, tile_height);
        create_req.height = 2*vlc_align(sys->height, tile_height);
        break;
    default:
        create_req.height = vlc_align(sys->height, tile_height);

        /*
         * width *4 so there's enough space for anything.
         */
        sys->stride = vlc_align(sys->width*4, tile_width);
        break;
    }

    ret = drmIoctl(sys->drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req);
    if (ret < 0) {
        msg_Err(vd, "Cannot create dumb buffer");
        return drvFail;
    }

    sys->size = create_req.size;
    sys->handle[buf] = create_req.handle;

    /*
     * create framebuffer object for the dumb-buffer
     * index 0 has to be filled in any case.
     */
    for (i = 0; i < ARRAY_SIZE(handles) && (sys->offsets[i] || i < 1); i++) {
        handles[i] = create_req.handle;
        pitches[i] = sys->stride;
        offsets[i] = sys->offsets[i];
    }

    ret = drmModeAddFB2(sys->drm_fd, sys->width, sys->height, sys->drm_fourcc,
                        handles, pitches, offsets, &sys->fb[buf], 0);

    if (ret) {
        msg_Err(vd, "Cannot create frame buffer");
        ret = drvFail;
        goto err_destroy;
    }

    modify_req.handle = sys->handle[buf];
    ret = drmIoctl(sys->drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &modify_req);
    if (ret) {
        msg_Err(vd, "Cannot map dumb buffer");
        ret = drvFail;
        goto err_fb;
    }

    sys->map[buf] = mmap(0, sys->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                         sys->drm_fd, modify_req.offset);

    if (sys->map[buf] == MAP_FAILED) {
        msg_Err(vd, "Cannot mmap dumb buffer");
        ret = drvFail;
        goto err_fb;
    }
    return drvSuccess;

err_fb:
    drmModeRmFB(sys->drm_fd, sys->fb[buf]);
    sys->fb[buf] = 0;

err_destroy:
    memset(&destroy_req, 0, sizeof(destroy_req));
    destroy_req.handle = sys->handle[buf];
    drmIoctl(sys->drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
    return ret;
}


static deviceRval FindCRTC(vout_display_t *vd, drmModeRes const *res,
                             drmModeConnector const *conn)
{
    vout_display_sys_t *sys = vd->sys;
    drmModeEncoder *enc;
    int i, j;

    /*
     * Try current encoder and CRTC combo
     */
    if (conn->encoder_id) {
        enc = drmModeGetEncoder(sys->drm_fd, conn->encoder_id);
        if (enc) {
            if (enc->crtc_id) {
                msg_Dbg(vd, "Got CRTC %d from current encoder", enc->crtc_id);

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
                msg_Dbg(vd, "Got CRTC %d", res->crtcs[j]);
                sys->crtc = res->crtcs[j];
                drmModeFreeEncoder(enc);
                return drvSuccess;
            }
        }
        drmModeFreeEncoder(enc);
    }

    msg_Err(vd , "Cannot find CRTC for connector %d", conn->connector_id);
    return drvTryNext;
}


static deviceRval SetupDevice(vout_display_t *vd, drmModeRes const *res,
                             drmModeConnector const *conn)
{
    vout_display_sys_t *sys = vd->sys;
    deviceRval ret;
    int c, c2;

    if (conn->connection != DRM_MODE_CONNECTED || conn->count_modes == 0)
        return drvTryNext;

    sys->width = conn->modes[0].hdisplay;
    sys->height = conn->modes[0].vdisplay;
    msg_Dbg(vd, "Mode resolution for connector %u is %ux%u",
            conn->connector_id, sys->width, sys->height);

    ret = FindCRTC(vd, res, conn);
    if (ret != drvSuccess) {
        msg_Dbg(vd , "No valid CRTC for connector %d", conn->connector_id);
        return ret;
    }

    for (c = 0; c < MAXHWBUF; c++) {
        ret = CreateFB(vd, c);
        if (ret != drvSuccess) {
            msg_Err(vd, "Cannot create framebuffer %d for connector %d", c,
                    conn->connector_id);
            for (c2 = 0; c2 < c; c2++)
                DestroyFB(sys, c2);

            return ret;
        } else {
            msg_Dbg(vd, "Created HW framebuffer %d/%d", c+1, MAXHWBUF);
        }
    }
    return drvSuccess;
}


/** fourccmatching, matching drm to vlc fourccs and see if it was present
 * in HW. Here really is two lists, one in RGB and second in YUV. They're
 * listed in order of preference.
 *
 * fourccmatching::drm DRM fourcc code from drm_fourcc.h
 * fourccmatching::vlc VLC fourcc code from vlc_fourcc.h under title 'Chromas'
 * fourccmatching::plane_id from which plane this DRM fourcc was found
 * fourccmatching::present if this mode was available in HW
 * fourccmatching::isYUV as name suggest..
 */
static struct
{
    uint32_t     drm;
    vlc_fourcc_t vlc;
    uint32_t     plane_id;
    bool         present;
    bool         isYUV;
} fourccmatching[] = {
    { .drm = DRM_FORMAT_XRGB8888, .vlc = VLC_CODEC_RGB32, .isYUV = false },
    { .drm = DRM_FORMAT_RGB565, .vlc = VLC_CODEC_RGB16, .isYUV = false },
#if defined DRM_FORMAT_P010
    { .drm = DRM_FORMAT_P010, .vlc = VLC_CODEC_P010, .isYUV = true },
#endif
    { .drm = DRM_FORMAT_NV12, .vlc = VLC_CODEC_NV12, .isYUV = true },
    { .drm = DRM_FORMAT_YUYV, .vlc = VLC_CODEC_YUYV, .isYUV = true },
    { .drm = DRM_FORMAT_YVYU, .vlc = VLC_CODEC_YVYU, .isYUV = true },
    { .drm = DRM_FORMAT_UYVY, .vlc = VLC_CODEC_UYVY, .isYUV = true },
    { .drm = DRM_FORMAT_VYUY, .vlc = VLC_CODEC_VYUY, .isYUV = true },
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


static bool ChromaNegotiation(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    unsigned i, c, propi;
    uint32_t planetype;
    const char types[][16] = { "OVERLAY", "PRIMARY", "CURSOR", "UNKNOWN" };
    drmModePlaneRes *plane_res = NULL;
    drmModePlane *plane;
    drmModeObjectProperties *props;
    drmModePropertyPtr pp;
    bool YUVFormat;

    /*
     * For convenience print out in debug prints all supported
     * DRM modes so they can be seen if use verbose mode.
     */
    plane_res = drmModeGetPlaneResources(sys->drm_fd);
    sys->plane_id = 0;

    if (plane_res != NULL && plane_res->count_planes > 0) {
        msg_Dbg(vd, "List of DRM supported modes on this machine:");
        for (c = 0; c < plane_res->count_planes; c++) {

            plane = drmModeGetPlane(sys->drm_fd, plane_res->planes[c]);
            if (plane != NULL && plane->count_formats > 0) {
                props = drmModeObjectGetProperties(sys->drm_fd,
                                                   plane->plane_id,
                                                   DRM_MODE_OBJECT_PLANE);

                planetype = 3;
                pp = NULL;
                for (propi = 0; propi < props->count_props; propi++) {
                    pp = drmModeGetProperty(sys->drm_fd, props->props[propi]);
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
            } else {
                msg_Err(vd, "Couldn't get list of DRM formats");
                drmModeFreePlaneResources(plane_res);
                return false;
            }
        }
        drmModeFreePlaneResources(plane_res);
    }

    if (sys->forced_drm_fourcc) {
        for (c = i = 0; c < ARRAY_SIZE(fourccmatching); c++) {
            if (fourccmatching[c].drm == sys->drm_fourcc) {
                sys->vlc_fourcc = fourccmatching[c].vlc;
                break;
            }
        }
        if (sys->plane_id == 0) {
            msg_Err(vd, "Forced DRM fourcc (%.4s) not available in kernel.",
                    (char*)&sys->drm_fourcc);
            return false;
        }
        return true;
    }

    /*
     * favor yuv format according to YUVFormat flag.
     * check for exact match first, then YUVFormat and then !YUVFormat
     */
    for (c = i = 0; c < ARRAY_SIZE(fourccmatching); c++) {
        if (fourccmatching[c].vlc == sys->vlc_fourcc) {
            if (!sys->forced_drm_fourcc && fourccmatching[c].present) {
                sys->drm_fourcc = fourccmatching[c].drm;
                sys->plane_id = fourccmatching[c].plane_id;
             }

            if (!sys->drm_fourcc) {
                msg_Err(vd, "Forced VLC fourcc (%.4s) not matching anything available in kernel, please set manually",
                        (char*)&sys->vlc_fourcc);
                return false;
            }
            return true;
        }
    }

    YUVFormat = vlc_fourcc_IsYUV(sys->vlc_fourcc);
    for (c = i = 0; c < ARRAY_SIZE(fourccmatching); c++) {
        if (fourccmatching[c].isYUV == YUVFormat
                && fourccmatching[c].present) {
            if (!sys->forced_drm_fourcc) {
                sys->drm_fourcc = fourccmatching[c].drm;
                sys->plane_id = fourccmatching[c].plane_id;
             }

            sys->vlc_fourcc = fourccmatching[c].vlc;
            return true;
        }
    }

    for (i = 0; c < ARRAY_SIZE(fourccmatching); c++) {
        if (!fourccmatching[c].isYUV != YUVFormat
                && fourccmatching[c].present) {
            if (!sys->forced_drm_fourcc) {
                sys->drm_fourcc = fourccmatching[c].drm;
                sys->plane_id = fourccmatching[c].plane_id;
             }

            sys->vlc_fourcc = fourccmatching[c].vlc;
            return true;
        }
    }

    return false;
}

static void CustomDestroyPicture(picture_t *p_picture)
{
    picture_sys_t *psys = (picture_sys_t*)p_picture->p_sys;
    vout_display_sys_t *sys = (vout_display_sys_t *)psys->p_voutsys;
    int c;

    for (c = 0; c < MAXHWBUF; c++)
        DestroyFB(sys, c);

    drmSetClientCap(sys->drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 0);
    drmDropMaster(sys->drm_fd);
    vlc_close(sys->drm_fd);
    sys->drm_fd = 0;
    free(p_picture->p_sys);
}

static int OpenDisplay(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    char *psz_device;
    int ret;
    uint64_t dumbRet;
    drmModeConnector *conn;
    drmModeRes *modeRes;
    int c;
    bool found_connector = false;

    /*
     * Open framebuffer device
     */
    psz_device = var_InheritString(vd, KMS_VAR);
    if (psz_device == NULL) {
        msg_Err(vd, "Don't know which DRM device to open");
        return VLC_EGENERIC;
    }

    sys->drm_fd = vlc_open(psz_device, O_RDWR);
    if (sys->drm_fd == -1) {
        msg_Err(vd, "cannot open %s", psz_device);
        free(psz_device);
        return VLC_EGENERIC;
    }
    free(psz_device);

    drmSetClientCap(sys->drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

    if (!ChromaNegotiation(vd))
        goto err_out;

    msg_Dbg(vd, "Using VLC chroma '%.4s', DRM chroma '%.4s'",
            (char*)&sys->vlc_fourcc, (char*)&sys->drm_fourcc);

    ret = drmGetCap(sys->drm_fd, DRM_CAP_DUMB_BUFFER, &dumbRet);
    if (ret < 0 || !dumbRet) {
        msg_Err(vd, "Device '%s' does not support dumb buffers", psz_device);
        goto err_out;
    }

    modeRes = drmModeGetResources(sys->drm_fd);
    if (modeRes == NULL) {
        msg_Err(vd, "Didn't get DRM resources");
        goto err_out;
    }

    for (c = 0; c < modeRes->count_connectors && sys->crtc == 0; c++) {

        conn = drmModeGetConnector(sys->drm_fd, modeRes->connectors[c]);
        if (conn == NULL)
            continue;

        found_connector = true;

        ret = SetupDevice(vd, modeRes, conn);
        if (ret != drvSuccess) {
            if (ret != drvTryNext) {
                msg_Err(vd, "Cannot do setup for connector %u:%u", c,
                        modeRes->connectors[c]);

                drmModeFreeConnector(conn);
                drmModeFreeResources(modeRes);
                goto err_out;
            }
            drmModeFreeConnector(conn);
            found_connector = false;
            continue;
        }
        drmModeFreeConnector(conn);
    }
    drmModeFreeResources(modeRes);

    if (!found_connector)
        goto err_out;

    picture_sys_t *psys = calloc(1, sizeof(*psys));
    if (psys == NULL)
        goto err_out;

    picture_resource_t rsc = {
        .p_sys = psys,
        .pf_destroy = CustomDestroyPicture,
    };

    for (size_t i = 0; i < PICTURE_PLANE_MAX; i++) {
        rsc.p[i].p_pixels = sys->map[0] + sys->offsets[i];
        rsc.p[i].i_lines  = sys->height;
        rsc.p[i].i_pitch  = sys->stride;
    }

    psys->p_voutsys = sys;

    sys->picture = picture_NewFromResource(&vd->fmt, &rsc);

    if (!sys->picture)
    {
        free(psys);
        goto err_out;
    }

    return VLC_SUCCESS;
err_out:
    drmDropMaster(sys->drm_fd);
    vlc_close(sys->drm_fd);
    sys->drm_fd = 0;
    return VLC_EGENERIC;
}


static int Control(vout_display_t *vd, int query, va_list args)
{
    (void) vd; (void) args;

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
    picture_Copy( sys->picture, pic );
}


static void Display(vout_display_t *vd, picture_t *picture)
{
    VLC_UNUSED(picture);
    vout_display_sys_t *sys = vd->sys;
    int i;

    if (drmModeSetPlane(sys->drm_fd, sys->plane_id, sys->crtc,
                         sys->fb[sys->front_buf], 0,
                         0, 0, sys->width, sys->height,
                         0, 0, sys->width << 16, sys->height << 16)) {
        msg_Err(vd, "Cannot do set plane for plane id %u, fb %x",
                sys->plane_id,
                sys->fb[sys->front_buf]);
    } else {
        sys->front_buf++;
        sys->front_buf %= MAXHWBUF;

        for (i = 0; i < PICTURE_PLANE_MAX; i++)
            sys->picture->p[i].p_pixels =
                    sys->map[sys->front_buf]+sys->offsets[i];
    }
}


/**
 * Terminate an output method created by Open
 */
static void Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->picture)
        picture_Release(sys->picture);

    if (sys->drm_fd)
        drmDropMaster(sys->drm_fd);
}

/**
 * This function allocates and initializes a KMS vout method.
 */
static int Open(vout_display_t *vd, const vout_display_cfg_t *cfg,
                video_format_t *fmtp, vlc_video_context *context)
{
    vout_display_sys_t *sys;
    vlc_fourcc_t local_vlc_chroma;
    uint32_t local_drm_chroma;
    video_format_t fmt = {};
    char *chroma;

    if (vout_display_cfg_IsWindowed(cfg))
        return VLC_EGENERIC;

    /*
     * Allocate instance and initialize some members
     */
    vd->sys = sys = vlc_obj_calloc(VLC_OBJECT(vd), 1, sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    chroma = var_InheritString(vd, "kms-vlc-chroma");
    if (chroma) {
        local_vlc_chroma = vlc_fourcc_GetCodecFromString(VIDEO_ES, chroma);

        if (local_vlc_chroma) {
            sys->vlc_fourcc = local_vlc_chroma;
            msg_Dbg(vd, "Forcing VLC to use chroma '%4s'", chroma);
         } else {
            sys->vlc_fourcc = fmtp->i_chroma;
            msg_Dbg(vd, "Chroma %4s invalid, using default", chroma);
         }

        free(chroma);
        chroma = NULL;
    } else {
        sys->vlc_fourcc = fmtp->i_chroma;
        msg_Dbg(vd, "Chroma not defined, using default");
    }

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

    if (OpenDisplay(vd) != VLC_SUCCESS) {
        Close(vd);
        return VLC_EGENERIC;
    }

    video_format_ApplyRotation(&fmt, fmtp);

    fmt.i_width = fmt.i_visible_width  = sys->width;
    fmt.i_height = fmt.i_visible_height = sys->height;
    fmt.i_chroma = sys->vlc_fourcc;
    *fmtp = fmt;

    vd->prepare = Prepare;
    vd->display = Display;
    vd->control = Control;
    vd->close = Close;

    (void) context;
    return VLC_SUCCESS;
}


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_shortname("kms")
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    add_loadfile(KMS_VAR, "/dev/dri/card0", DEVICE_TEXT, DEVICE_LONGTEXT)

    add_string( "kms-vlc-chroma", NULL, VLC_CHROMA_TEXT, VLC_CHROMA_LONGTEXT,
                true)
    add_string( "kms-drm-chroma", NULL, DRM_CHROMA_TEXT, DRM_CHROMA_LONGTEXT,
                true)
    set_description("Linux kernel mode setting video output")
    set_callback_display(Open, 30)
vlc_module_end ()
