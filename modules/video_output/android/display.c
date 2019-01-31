/*****************************************************************************
 * display.c: Android video output module
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
 *
 * Authors: Thomas Guillem <thomas@gllm.fr>
 *          Felix Abecassis <felix.abecassis@gmail.com>
 *          Ming Hu <tewilove@gmail.com>
 *          Ludovic Fauvet <etix@l0cal.com>
 *          Sébastien Toque <xilasz@gmail.com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>
#include <vlc_filter.h>

#include <vlc_opengl.h> /* for ClearSurface */
#include <GLES2/gl2.h>  /* for ClearSurface */

#include <dlfcn.h>

#include "display.h"
#include "utils.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define USE_ANWP
#define CHROMA_TEXT "Chroma used"
#define CHROMA_LONGTEXT \
    "Force use of a specific chroma for output. Default is RGB32."

#define CFG_PREFIX "android-display-"
static int Open(vout_display_t *vd, const vout_display_cfg_t *cfg,
                video_format_t *fmtp, vlc_video_context *context);
static int OpenOpaque(vout_display_t *vd, const vout_display_cfg_t *cfg,
                      video_format_t *fmtp, vlc_video_context *context);
static void Close(vout_display_t *vd);
static void SubpicturePrepare(vout_display_t *vd, subpicture_t *subpicture);

vlc_module_begin()
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_description("Android video output")
    set_capability("vout display", 260)
    add_shortcut("android-display")
    add_string(CFG_PREFIX "chroma", NULL, CHROMA_TEXT, CHROMA_LONGTEXT, true)
    set_callbacks(Open, Close)
    add_submodule ()
        set_description("Android opaque video output")
        set_capability("vout display", 280)
        add_shortcut("android-opaque")
        set_callbacks(OpenOpaque, Close)
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static const vlc_fourcc_t subpicture_chromas[] =
{
    VLC_CODEC_RGBA,
    0
};

static picture_pool_t   *Pool  (vout_display_t *, unsigned);
static void             Prepare(vout_display_t *, picture_t *, subpicture_t *, vlc_tick_t);
static void             Display(vout_display_t *, picture_t *);
static int              Control(vout_display_t *, int, va_list);

typedef struct android_window android_window;
struct android_window
{
    video_format_t fmt;
    int i_android_hal;
    unsigned int i_angle;
    unsigned int i_pic_count;
    unsigned int i_min_undequeued;
    bool b_opaque;

    enum AWindow_ID id;
    ANativeWindow *p_surface;
    jobject       *p_jsurface;
};

typedef struct buffer_bounds buffer_bounds;
struct buffer_bounds
{
    uint8_t *p_pixels;
    ARect bounds;
};

struct vout_display_sys_t
{
    vout_window_t *embed;
    picture_pool_t *pool;

    int i_display_width;
    int i_display_height;

    AWindowHandler *p_awh;
    native_window_api_t *anw;

    android_window *p_window;
    android_window *p_sub_window;

    bool b_displayed;
    bool b_sub_invalid;
    filter_t *p_spu_blend;
    picture_t *p_sub_pic;
    buffer_bounds *p_sub_buffer_bounds;
    int64_t i_sub_last_order;
    ARect sub_last_region;

    bool b_has_subpictures;
};

#define PRIV_WINDOW_FORMAT_YV12 0x32315659

static inline int ChromaToAndroidHal(vlc_fourcc_t i_chroma)
{
    switch (i_chroma) {
        case VLC_CODEC_YV12:
        case VLC_CODEC_I420:
            return PRIV_WINDOW_FORMAT_YV12;
        case VLC_CODEC_RGB16:
            return WINDOW_FORMAT_RGB_565;
        case VLC_CODEC_RGB32:
            return WINDOW_FORMAT_RGBX_8888;
        case VLC_CODEC_RGBA:
            return WINDOW_FORMAT_RGBA_8888;
        default:
            return -1;
    }
}

static int UpdateVideoSize(vout_display_sys_t *sys, video_format_t *p_fmt)
{
    unsigned int i_width, i_height;
    unsigned int i_sar_num = 1, i_sar_den = 1;
    video_format_t rot_fmt;

    video_format_ApplyRotation(&rot_fmt, p_fmt);

    if (rot_fmt.i_sar_num != 0 && rot_fmt.i_sar_den != 0) {
        i_sar_num = rot_fmt.i_sar_num;
        i_sar_den = rot_fmt.i_sar_den;
    }
    i_width = rot_fmt.i_width;
    i_height = rot_fmt.i_height;

    AWindowHandler_setVideoLayout(sys->p_awh, i_width, i_height,
                                  rot_fmt.i_visible_width,
                                  rot_fmt.i_visible_height,
                                  i_sar_num, i_sar_den);
    return 0;
}

static void AndroidPicture_Destroy(picture_t *pic)
{
    free(pic->p_sys);
}

static picture_t *PictureAlloc(vout_display_sys_t *sys, video_format_t *fmt,
                               bool b_opaque)
{
    picture_t *p_pic;
    picture_resource_t rsc;
    picture_sys_t *p_picsys = calloc(1, sizeof(*p_picsys));

    if (unlikely(p_picsys == NULL))
        return NULL;


    memset(&rsc, 0, sizeof(picture_resource_t));
    rsc.p_sys = p_picsys;

    if (b_opaque)
    {
        p_picsys->hw.b_vd_ref = true;
        p_picsys->hw.p_surface = sys->p_window->p_surface;
        p_picsys->hw.p_jsurface =  sys->p_window->p_jsurface;
        p_picsys->hw.i_index = -1;
        vlc_mutex_init(&p_picsys->hw.lock);
        rsc.pf_destroy = AndroidOpaquePicture_DetachVout;
    }
    else
    {
        p_picsys->sw.p_vd_sys = sys;
        rsc.pf_destroy = AndroidPicture_Destroy;
    }

    p_pic = picture_NewFromResource(fmt, &rsc);
    if (!p_pic)
    {
        free(p_picsys);
        return NULL;
    }
    return p_pic;
}

static void FixSubtitleFormat(vout_display_sys_t *sys)
{
    video_format_t *p_subfmt;
    video_format_t fmt;
    int i_width, i_height;
    int i_video_width, i_video_height;
    int i_display_width, i_display_height;
    double aspect;

    if (!sys->p_sub_window)
        return;
    p_subfmt = &sys->p_sub_window->fmt;

    video_format_ApplyRotation(&fmt, &sys->p_window->fmt);

    if (fmt.i_visible_width == 0 || fmt.i_visible_height == 0) {
        i_video_width = fmt.i_width;
        i_video_height = fmt.i_height;
    } else {
        i_video_width = fmt.i_visible_width;
        i_video_height = fmt.i_visible_height;
    }

    if (fmt.i_sar_num > 0 && fmt.i_sar_den > 0) {
        if (fmt.i_sar_num >= fmt.i_sar_den)
            i_video_width = i_video_width * fmt.i_sar_num / fmt.i_sar_den;
        else
            i_video_height = i_video_height * fmt.i_sar_den / fmt.i_sar_num;
    }

    if (sys->p_window->i_angle == 90 || sys->p_window->i_angle == 180) {
        i_display_width = sys->i_display_height;
        i_display_height = sys->i_display_width;
        aspect = i_video_height / (double) i_video_width;
    } else {
        i_display_width = sys->i_display_width;
        i_display_height = sys->i_display_height;
        aspect = i_video_width / (double) i_video_height;
    }

    if (i_display_width / aspect < i_display_height) {
        i_width = i_display_width;
        i_height = i_display_width / aspect;
    } else {
        i_width = i_display_height * aspect;
        i_height = i_display_height;
    }

    // Use the biggest size available
    if (i_width * i_height < i_video_width * i_video_height) {
        i_width = i_video_width;
        i_height = i_video_height;
    }

    p_subfmt->i_width =
    p_subfmt->i_visible_width = i_width;
    p_subfmt->i_height =
    p_subfmt->i_visible_height = i_height;
    p_subfmt->i_x_offset = 0;
    p_subfmt->i_y_offset = 0;
    p_subfmt->i_sar_num = 1;
    p_subfmt->i_sar_den = 1;
    sys->b_sub_invalid = true;
}

#define ALIGN_16_PIXELS( x ) ( ( ( x ) + 15 ) / 16 * 16 )
static void SetupPictureYV12(picture_t *p_picture, uint32_t i_in_stride)
{
    /* according to document of android.graphics.ImageFormat.YV12 */
    int i_stride = ALIGN_16_PIXELS(i_in_stride);
    int i_c_stride = ALIGN_16_PIXELS(i_stride / 2);

    p_picture->p->i_pitch = i_stride;

    /* Fill chroma planes for planar YUV */
    for (int n = 1; n < p_picture->i_planes; n++)
    {
        const plane_t *o = &p_picture->p[n-1];
        plane_t *p = &p_picture->p[n];

        p->p_pixels = o->p_pixels + o->i_lines * o->i_pitch;
        p->i_pitch  = i_c_stride;
        p->i_lines  = p_picture->format.i_height / 2;
        /*
          Explicitly set the padding lines of the picture to black (127 for YUV)
          since they might be used by Android during rescaling.
        */
        int visible_lines = p_picture->format.i_visible_height / 2;
        if (visible_lines < p->i_lines)
            memset(&p->p_pixels[visible_lines * p->i_pitch], 127, (p->i_lines - visible_lines) * p->i_pitch);
    }

    if (vlc_fourcc_AreUVPlanesSwapped(p_picture->format.i_chroma,
                                      VLC_CODEC_YV12))
        picture_SwapUV( p_picture );
}

static void AndroidWindow_DisconnectSurface(vout_display_sys_t *sys,
                                            android_window *p_window)
{
    if (p_window->p_surface) {
        AWindowHandler_releaseANativeWindow(sys->p_awh, p_window->id);
        p_window->p_surface = NULL;
    }
}

static int AndroidWindow_ConnectSurface(vout_display_sys_t *sys,
                                        android_window *p_window)
{
    if (!p_window->p_surface) {
        p_window->p_surface = AWindowHandler_getANativeWindow(sys->p_awh,
                                                              p_window->id);
        if (!p_window->p_surface)
            return -1;
        if (p_window->b_opaque)
            p_window->p_jsurface = AWindowHandler_getSurface(sys->p_awh,
                                                             p_window->id);
    }

    return 0;
}

static android_window *AndroidWindow_New(vout_display_t *vd,
                                         const video_format_t *p_fmt,
                                         enum AWindow_ID id)
{
    vout_display_sys_t *sys = vd->sys;
    android_window *p_window = NULL;

    p_window = calloc(1, sizeof(android_window));
    if (!p_window)
        goto error;

    p_window->id = id;
    p_window->b_opaque = p_fmt->i_chroma == VLC_CODEC_ANDROID_OPAQUE;
    if (!p_window->b_opaque) {
        p_window->i_android_hal = ChromaToAndroidHal(p_fmt->i_chroma);
        if (p_window->i_android_hal == -1)
            goto error;
    }

    switch (p_fmt->orientation)
    {
        case ORIENT_ROTATED_90:
            p_window->i_angle = 90;
            break;
        case ORIENT_ROTATED_180:
            p_window->i_angle = 180;
            break;
        case ORIENT_ROTATED_270:
            p_window->i_angle = 270;
            break;
        default:
            p_window->i_angle = 0;
    }
    video_format_ApplyRotation(&p_window->fmt, p_fmt);
    p_window->i_pic_count = 1;

    if (AndroidWindow_ConnectSurface(sys, p_window) != 0)
    {
        if (id == AWindow_Video)
            msg_Err(vd, "can't get Video Surface");
        else if (id == AWindow_Subtitles)
            msg_Err(vd, "can't get Subtitles Surface");
        goto error;
    }

    return p_window;
error:
    free(p_window);
    return NULL;
}

static void AndroidWindow_Destroy(vout_display_t *vd,
                                  android_window *p_window)
{
    AndroidWindow_DisconnectSurface(vd->sys, p_window);
    free(p_window);
}

static int AndroidWindow_SetupANW(vout_display_sys_t *sys,
                                  android_window *p_window,
                                  bool b_java_configured)
{
    p_window->i_pic_count = 1;
    p_window->i_min_undequeued = 0;

    if (!b_java_configured && sys->anw->setBuffersGeometry)
        return sys->anw->setBuffersGeometry(p_window->p_surface,
                                            p_window->fmt.i_width,
                                            p_window->fmt.i_height,
                                            p_window->i_android_hal);
    else
        return 0;
}

static int AndroidWindow_Setup(vout_display_sys_t *sys,
                               android_window *p_window,
                               unsigned int i_pic_count)
{
    bool b_java_configured = false;

    if (i_pic_count != 0)
        p_window->i_pic_count = i_pic_count;

    if (!p_window->b_opaque) {
        int align_pixels;
        picture_t *p_pic = PictureAlloc(sys, &p_window->fmt, false);

        // For RGB (32 or 16) we need to align on 8 or 4 pixels, 16 pixels for YUV
        align_pixels = (16 / p_pic->p[0].i_pixel_pitch) - 1;
        p_window->fmt.i_height = p_pic->format.i_height;
        p_window->fmt.i_width = (p_pic->format.i_width + align_pixels) & ~align_pixels;
        picture_Release(p_pic);

        if (AndroidWindow_SetupANW(sys, p_window, b_java_configured) != 0)
            return -1;
    } else {
        sys->p_window->i_pic_count = 31; // TODO
        sys->p_window->i_min_undequeued = 0;
    }

    return 0;
}

static void AndroidWindow_UnlockPicture(vout_display_sys_t *sys,
                                        android_window *p_window,
                                        picture_t *p_pic)
{
    picture_sys_t *p_picsys = p_pic->p_sys;

    if (!p_picsys->b_locked)
        return;

    sys->anw->unlockAndPost(p_window->p_surface);

    p_picsys->b_locked = false;
}

static int AndroidWindow_LockPicture(vout_display_sys_t *sys,
                                     android_window *p_window,
                                     picture_t *p_pic)
{
    picture_sys_t *p_picsys = p_pic->p_sys;

    if (p_picsys->b_locked)
        return -1;

    if (sys->anw->winLock(p_window->p_surface,
                          &p_picsys->sw.buf, NULL) != 0)
        return -1;

    if (p_picsys->sw.buf.width < 0 ||
        p_picsys->sw.buf.height < 0 ||
        (unsigned)p_picsys->sw.buf.width < p_window->fmt.i_width ||
        (unsigned)p_picsys->sw.buf.height < p_window->fmt.i_height)
    {
        p_picsys->b_locked = true;
        AndroidWindow_UnlockPicture(sys, p_window, p_pic);
        return -1;
    }

    p_pic->p[0].p_pixels = p_picsys->sw.buf.bits;
    p_pic->p[0].i_lines = p_picsys->sw.buf.height;
    p_pic->p[0].i_pitch = p_pic->p[0].i_pixel_pitch * p_picsys->sw.buf.stride;

    if (p_picsys->sw.buf.format == PRIV_WINDOW_FORMAT_YV12)
        SetupPictureYV12(p_pic, p_picsys->sw.buf.stride);

    p_picsys->b_locked = true;
    return 0;
}

static void SetRGBMask(video_format_t *p_fmt)
{
    switch(p_fmt->i_chroma) {
        case VLC_CODEC_RGB16:
            p_fmt->i_bmask = 0x0000001f;
            p_fmt->i_gmask = 0x000007e0;
            p_fmt->i_rmask = 0x0000f800;
            break;

        case VLC_CODEC_RGB32:
        case VLC_CODEC_RGBA:
            p_fmt->i_rmask = 0x000000ff;
            p_fmt->i_gmask = 0x0000ff00;
            p_fmt->i_bmask = 0x00ff0000;
            break;
    }
}

static int OpenCommon(vout_display_t *vd, const vout_display_cfg_t *cfg,
                      video_format_t *fmtp)
{
    vout_display_sys_t *sys;
    video_format_t fmt, sub_fmt;

    vout_window_t *embed = cfg->window;
    if (embed->type != VOUT_WINDOW_TYPE_ANDROID_NATIVE)
        return VLC_EGENERIC;

    fmt = *fmtp;

    if (embed == NULL)
        return VLC_EGENERIC;
    assert(embed->handle.anativewindow);
    AWindowHandler *p_awh = embed->handle.anativewindow;

    if (!AWindowHandler_canSetVideoLayout(p_awh))
    {
        /* It's better to use gles2 if we are not able to change the video
         * layout */
        return VLC_EGENERIC;
    }

    /* Allocate structure */
    vd->sys = sys = (struct vout_display_sys_t*)calloc(1, sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    sys->embed = embed;
    sys->p_awh = p_awh;
    sys->anw = AWindowHandler_getANativeWindowAPI(sys->p_awh);

    sys->i_display_width = cfg->display.width;
    sys->i_display_height = cfg->display.height;

    if (fmt.i_chroma != VLC_CODEC_ANDROID_OPAQUE) {
        /* Setup chroma */
        char *psz_fcc = var_InheritString(vd, CFG_PREFIX "chroma");
        if (psz_fcc) {
            fmt.i_chroma = vlc_fourcc_GetCodecFromString(VIDEO_ES, psz_fcc);
            free(psz_fcc);
        } else
            fmt.i_chroma = VLC_CODEC_RGB32;

        switch(fmt.i_chroma) {
            case VLC_CODEC_YV12:
                /* avoid swscale usage by asking for I420 instead since the
                 * vout already has code to swap the buffers */
                fmt.i_chroma = VLC_CODEC_I420;
            case VLC_CODEC_I420:
                break;
            case VLC_CODEC_RGB16:
            case VLC_CODEC_RGB32:
            case VLC_CODEC_RGBA:
                SetRGBMask(&fmt);
                video_format_FixRgb(&fmt);
                break;
            default:
                goto error;
        }
    }

    sys->p_window = AndroidWindow_New(vd, &fmt, AWindow_Video);
    if (!sys->p_window)
        goto error;

    if (AndroidWindow_Setup(sys, sys->p_window, 0) != 0)
        goto error;

    /* use software rotation if we don't do opaque */
    if (!sys->p_window->b_opaque)
        video_format_TransformTo(&fmt, ORIENT_NORMAL);

    msg_Dbg(vd, "using %s", sys->p_window->b_opaque ? "opaque" : "ANW");

    video_format_ApplyRotation(&sub_fmt, &fmt);
    sub_fmt.i_chroma = subpicture_chromas[0];
    SetRGBMask(&sub_fmt);
    video_format_FixRgb(&sub_fmt);
    sys->p_sub_window = AndroidWindow_New(vd, &sub_fmt, AWindow_Subtitles);
    if (sys->p_sub_window) {

        FixSubtitleFormat(sys);
        sys->i_sub_last_order = -1;

        /* Export the subpicture capability of this vout. */
        vd->info.subpicture_chromas = subpicture_chromas;
    }
    else if (!vd->obj.force && sys->p_window->b_opaque)
    {
        msg_Warn(vd, "cannot blend subtitles with an opaque surface, "
                     "trying next vout");
        goto error;
    }

    *fmtp = fmt;
    /* Setup vout_display */
    vd->pool    = Pool;
    vd->prepare = Prepare;
    vd->display = Display;
    vd->control = Control;
    vd->info.is_slow = !sys->p_window->b_opaque;

    return VLC_SUCCESS;

error:
    Close(vd);
    return VLC_EGENERIC;
}

static int Open(vout_display_t *vd, const vout_display_cfg_t *cfg,
                video_format_t *fmtp, vlc_video_context *context)
{
    if (fmtp->i_chroma == VLC_CODEC_ANDROID_OPAQUE)
        return VLC_EGENERIC;

    /* There are two cases:
     * 1. the projection_mode is PROJECTION_MODE_RECTANGULAR
     * 2. gles2 vout failed */
    fmtp->projection_mode = PROJECTION_MODE_RECTANGULAR;

    (void) context;
    return OpenCommon(vd, cfg, fmtp);
}

static int OpenOpaque(vout_display_t *vd, const vout_display_cfg_t *cfg,
                      video_format_t *fmtp, vlc_video_context *context)
{
    if (fmtp->i_chroma != VLC_CODEC_ANDROID_OPAQUE)
        return VLC_EGENERIC;

    if (!vd->obj.force
        && (fmtp->projection_mode != PROJECTION_MODE_RECTANGULAR
            || fmtp->orientation != ORIENT_NORMAL))
    {
        /* Let the gles2 vout handle orientation and projection */
        return VLC_EGENERIC;
    }

    (void) context;
    return OpenCommon(vd, cfg, fmtp);
}

static void ClearSurface(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->p_window->b_opaque)
    {
        /* Clear the surface to black with OpenGL ES 2 */
        char *modlist = var_InheritString(sys->embed, "gles2");
        vlc_gl_t *gl = vlc_gl_Create(vd->cfg, VLC_OPENGL_ES2, modlist);
        free(modlist);
        if (gl == NULL)
            return;

        if (vlc_gl_MakeCurrent(gl))
            goto end;

        vlc_gl_Resize(gl, 1, 1);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        vlc_gl_Swap(gl);

        vlc_gl_ReleaseCurrent(gl);

end:
        vlc_gl_Release(gl);
    }
    else
    {
        android_window *p_window = sys->p_window;
        ANativeWindow_Buffer buf;

        if (sys->anw->setBuffersGeometry(p_window->p_surface, 1, 1,
                                         WINDOW_FORMAT_RGB_565) == 0
          && sys->anw->winLock(p_window->p_surface, &buf, NULL) == 0)
        {
            uint16_t *p_bit = buf.bits;
            p_bit[0] = 0x0000;
            sys->anw->unlockAndPost(p_window->p_surface);
        }
    }
}

static void Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    /* Check if SPU regions have been properly cleared, and clear them if they
     * were not. */
    if (sys->b_has_subpictures)
    {
        SubpicturePrepare(vd, NULL);
        AndroidWindow_UnlockPicture(sys, sys->p_sub_window, sys->p_sub_pic);
    }

    if (sys->pool)
        picture_pool_Release(sys->pool);

    if (sys->p_window)
    {
        if (sys->b_displayed)
            ClearSurface(vd);
        AndroidWindow_Destroy(vd, sys->p_window);
    }

    if (sys->p_sub_pic)
        picture_Release(sys->p_sub_pic);
    if (sys->p_spu_blend)
        filter_DeleteBlend(sys->p_spu_blend);
    free(sys->p_sub_buffer_bounds);
    if (sys->p_sub_window)
        AndroidWindow_Destroy(vd, sys->p_sub_window);

    if (sys->embed)
        AWindowHandler_setVideoLayout(sys->p_awh, 0, 0, 0, 0, 0, 0);

    free(sys);
}

static int PoolLockPicture(picture_t *p_pic)
{
    picture_sys_t *p_picsys = p_pic->p_sys;
    vout_display_sys_t *sys = p_picsys->sw.p_vd_sys;

    if (AndroidWindow_LockPicture(sys, sys->p_window, p_pic) != 0)
        return -1;

    return 0;
}

static void PoolUnlockPicture(picture_t *p_pic)
{
    picture_sys_t *p_picsys = p_pic->p_sys;
    vout_display_sys_t *sys = p_picsys->sw.p_vd_sys;

    AndroidWindow_UnlockPicture(sys, sys->p_window, p_pic);
}

static int PoolLockOpaquePicture(picture_t *p_pic)
{
    picture_sys_t *p_picsys = p_pic->p_sys;

    p_picsys->b_locked = true;
    return 0;
}

static void PoolUnlockOpaquePicture(picture_t *p_pic)
{
    picture_sys_t *p_picsys = p_pic->p_sys;

    AndroidOpaquePicture_Release(p_picsys, false);
}

static picture_pool_t *PoolAlloc(vout_display_t *vd, unsigned requested_count)
{
    vout_display_sys_t *sys = vd->sys;
    picture_pool_t *pool = NULL;
    picture_t **pp_pics = NULL;
    unsigned int i = 0;

    msg_Dbg(vd, "PoolAlloc: request %d frames", requested_count);
    if (AndroidWindow_Setup(sys, sys->p_window, requested_count) != 0)
        goto error;

    requested_count = sys->p_window->i_pic_count;
    msg_Dbg(vd, "PoolAlloc: got %d frames", requested_count);

    UpdateVideoSize(sys, &sys->p_window->fmt);

    pp_pics = calloc(requested_count, sizeof(picture_t));

    for (i = 0; i < requested_count; i++)
    {
        picture_t *p_pic = PictureAlloc(sys, &sys->p_window->fmt,
                                        sys->p_window->b_opaque);
        if (!p_pic)
            goto error;

        pp_pics[i] = p_pic;
    }

    picture_pool_configuration_t pool_cfg;
    memset(&pool_cfg, 0, sizeof(pool_cfg));
    pool_cfg.picture_count = requested_count;
    pool_cfg.picture       = pp_pics;
    if (sys->p_window->b_opaque)
    {
        pool_cfg.lock      = PoolLockOpaquePicture;
        pool_cfg.unlock    = PoolUnlockOpaquePicture;
    }
    else
    {
        pool_cfg.lock      = PoolLockPicture;
        pool_cfg.unlock    = PoolUnlockPicture;
    }
    pool = picture_pool_NewExtended(&pool_cfg);

error:
    if (!pool && pp_pics) {
        for (unsigned j = 0; j < i; j++)
            picture_Release(pp_pics[j]);
    }
    free(pp_pics);
    return pool;
}

static void SubtitleRegionToBounds(subpicture_t *subpicture,
                                   ARect *p_out_bounds)
{
    if (subpicture) {
        for (subpicture_region_t *r = subpicture->p_region; r != NULL; r = r->p_next) {
            ARect new_bounds;

            new_bounds.left = r->i_x;
            new_bounds.top = r->i_y;
            if (new_bounds.left < 0)
                new_bounds.left = 0;
            if (new_bounds.top < 0)
                new_bounds.top = 0;
            new_bounds.right = r->fmt.i_visible_width + r->i_x;
            new_bounds.bottom = r->fmt.i_visible_height + r->i_y;
            if (r == &subpicture->p_region[0])
                *p_out_bounds = new_bounds;
            else {
                if (p_out_bounds->left > new_bounds.left)
                    p_out_bounds->left = new_bounds.left;
                if (p_out_bounds->right < new_bounds.right)
                    p_out_bounds->right = new_bounds.right;
                if (p_out_bounds->top > new_bounds.top)
                    p_out_bounds->top = new_bounds.top;
                if (p_out_bounds->bottom < new_bounds.bottom)
                    p_out_bounds->bottom = new_bounds.bottom;
            }
        }
    } else {
        p_out_bounds->left = p_out_bounds->top = 0;
        p_out_bounds->right = p_out_bounds->bottom = 0;
    }
}

static void SubtitleGetDirtyBounds(vout_display_t *vd,
                                   subpicture_t *subpicture,
                                   ARect *p_out_bounds)
{
    vout_display_sys_t *sys = vd->sys;
    int i = 0;
    bool b_found = false;

    /* Try to find last bounds set by current locked buffer.
     * Indeed, even if we can lock only one buffer at a time, differents
     * buffers can be locked. This functions will find the last bounds set by
     * the current buffer. */
    if (sys->p_sub_buffer_bounds) {
        for (; sys->p_sub_buffer_bounds[i].p_pixels != NULL; ++i) {
            buffer_bounds *p_bb = &sys->p_sub_buffer_bounds[i];
            if (p_bb->p_pixels == sys->p_sub_pic->p[0].p_pixels) {
                *p_out_bounds = p_bb->bounds;
                b_found = true;
                break;
            }
        }
    }

    if (!b_found
     || p_out_bounds->left < 0
     || p_out_bounds->right < 0
     || (unsigned int) p_out_bounds->right > sys->p_sub_pic->format.i_width
     || p_out_bounds->bottom < 0
     || p_out_bounds->top < 0
     || (unsigned int) p_out_bounds->top > sys->p_sub_pic->format.i_height)
    {
        /* default is full picture */
        p_out_bounds->left = 0;
        p_out_bounds->top = 0;
        p_out_bounds->right = sys->p_sub_pic->format.i_width;
        p_out_bounds->bottom = sys->p_sub_pic->format.i_height;
    }

    /* buffer not found, add it to the array */
    if (!sys->p_sub_buffer_bounds
     || sys->p_sub_buffer_bounds[i].p_pixels == NULL) {
        buffer_bounds *p_bb = realloc(sys->p_sub_buffer_bounds,
                                      (i + 2) * sizeof(buffer_bounds));
        if (p_bb) {
            sys->p_sub_buffer_bounds = p_bb;
            sys->p_sub_buffer_bounds[i].p_pixels = sys->p_sub_pic->p[0].p_pixels;
            sys->p_sub_buffer_bounds[i+1].p_pixels = NULL;
        }
    }

    /* set buffer bounds */
    if (sys->p_sub_buffer_bounds
     && sys->p_sub_buffer_bounds[i].p_pixels != NULL)
        SubtitleRegionToBounds(subpicture, &sys->p_sub_buffer_bounds[i].bounds);
}

static void SubpicturePrepare(vout_display_t *vd, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    ARect memset_bounds;

    SubtitleRegionToBounds(subpicture, &memset_bounds);

    if( subpicture )
    {
        if( subpicture->i_order == sys->i_sub_last_order
         && memcmp( &memset_bounds, &sys->sub_last_region, sizeof(ARect) ) == 0 )
            return;

        sys->i_sub_last_order = subpicture->i_order;
        sys->sub_last_region = memset_bounds;
    }

    if (AndroidWindow_LockPicture(sys, sys->p_sub_window, sys->p_sub_pic) != 0)
        return;

    /* Clear the subtitles surface. */
    SubtitleGetDirtyBounds(vd, subpicture, &memset_bounds);
    const int x_pixels_offset = memset_bounds.left
                                * sys->p_sub_pic->p[0].i_pixel_pitch;
    const int i_line_size = (memset_bounds.right - memset_bounds.left)
                            * sys->p_sub_pic->p->i_pixel_pitch;
    for (int y = memset_bounds.top; y < memset_bounds.bottom; y++)
        memset(&sys->p_sub_pic->p[0].p_pixels[y * sys->p_sub_pic->p[0].i_pitch
                                              + x_pixels_offset], 0, i_line_size);

    if (subpicture)
        picture_BlendSubpicture(sys->p_sub_pic, sys->p_spu_blend, subpicture);
}

static picture_pool_t *Pool(vout_display_t *vd, unsigned requested_count)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->pool == NULL)
        sys->pool = PoolAlloc(vd, requested_count);
    return sys->pool;
}

static void Prepare(vout_display_t *vd, picture_t *picture,
                    subpicture_t *subpicture, vlc_tick_t date)
{
    vout_display_sys_t *sys = vd->sys;
    VLC_UNUSED(picture);

    if (subpicture && sys->p_sub_window) {
        if (sys->b_sub_invalid) {
            sys->b_sub_invalid = false;
            if (sys->p_sub_pic) {
                picture_Release(sys->p_sub_pic);
                sys->p_sub_pic = NULL;
            }
            if (sys->p_spu_blend) {
                filter_DeleteBlend(sys->p_spu_blend);
                sys->p_spu_blend = NULL;
            }
            free(sys->p_sub_buffer_bounds);
            sys->p_sub_buffer_bounds = NULL;
        }

        if (!sys->p_sub_pic
         && AndroidWindow_Setup(sys, sys->p_sub_window, 1) == 0)
            sys->p_sub_pic = PictureAlloc(sys, &sys->p_sub_window->fmt, false);
        if (!sys->p_spu_blend && sys->p_sub_pic)
            sys->p_spu_blend = filter_NewBlend(VLC_OBJECT(vd),
                                               &sys->p_sub_pic->format);

        if (sys->p_sub_pic && sys->p_spu_blend)
            sys->b_has_subpictures = true;
    }
    /* As long as no subpicture was received, do not call
       SubpictureDisplay since JNI calls and clearing the subtitles
       surface are expensive operations. */
    if (sys->b_has_subpictures)
    {
        SubpicturePrepare(vd, subpicture);
        if (!subpicture)
        {
            /* The surface has been cleared and there is no new
               subpicture to upload, do not clear again until a new
               subpicture is received. */
            sys->b_has_subpictures = false;
        }
    }
    if (sys->p_window->b_opaque
     && AndroidOpaquePicture_CanReleaseAtTime(picture->p_sys))
    {
        vlc_tick_t now = vlc_tick_now();
        if (date > now)
        {
            if (date - now <= VLC_TICK_FROM_SEC(1))
                AndroidOpaquePicture_ReleaseAtTime(picture->p_sys, date);
            else /* The picture will be displayed from the Display callback */
                msg_Warn(vd, "picture way too early to release at time");
        }
    }
}

static void Display(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;
    VLC_UNUSED(picture);

    if (sys->p_window->b_opaque)
        AndroidOpaquePicture_Release(picture->p_sys, true);
    else
        AndroidWindow_UnlockPicture(sys, sys->p_window, picture);

    if (sys->p_sub_pic)
        AndroidWindow_UnlockPicture(sys, sys->p_sub_window, sys->p_sub_pic);

    sys->b_displayed = true;
}

static void CopySourceAspect(video_format_t *p_dest,
                             const video_format_t *p_src)
{
    p_dest->i_sar_num = p_src->i_sar_num;
    p_dest->i_sar_den = p_src->i_sar_den;
}

static int Control(vout_display_t *vd, int query, va_list args)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query) {
    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    {
        msg_Dbg(vd, "change source crop/aspect");

        if (query == VOUT_DISPLAY_CHANGE_SOURCE_CROP) {
            video_format_CopyCrop(&sys->p_window->fmt, &vd->source);
        } else
            CopySourceAspect(&sys->p_window->fmt, &vd->source);

        UpdateVideoSize(sys, &sys->p_window->fmt);
        FixSubtitleFormat(sys);
        return VLC_SUCCESS;
    }
    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
    {
        const vout_display_cfg_t *cfg = va_arg(args, const vout_display_cfg_t *);

        sys->i_display_width = cfg->display.width;
        sys->i_display_height = cfg->display.height;
        msg_Dbg(vd, "change display size: %dx%d", sys->i_display_width,
                                                  sys->i_display_height);
        FixSubtitleFormat(sys);
        return VLC_SUCCESS;
    }
    case VOUT_DISPLAY_RESET_PICTURES:
        vlc_assert_unreachable();
    default:
        msg_Warn(vd, "Unknown request in android-display: %d", query);
    case VOUT_DISPLAY_CHANGE_ZOOM:
    case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        return VLC_EGENERIC;
    }
}
