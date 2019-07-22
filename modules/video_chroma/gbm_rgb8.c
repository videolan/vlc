
#include <vlc_common.h>
#include <vlc_module.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_cpu.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <gbm.h>

static int create( vlc_object_t * );

static void gbm_rgb_filter( filter_t *, picture_t *, picture_t * );

vlc_module_begin()
    set_description( N_("Conversion from gbm surface to original format") )
    set_capability( "video converter", 280 )
    set_callback( create )
vlc_module_end()


static int create( vlc_object_t *obj )
{
    filter_t *filter = (filter_t *)obj;

    if( !vlc_CPU_capable() )
        return VLC_EGENERIC;

    filter->pf_video_filter = gbm_rbg_filter;
    return VLC_SUCCESS;
}

static void gbm_rgb_filter( filter_t *filter, picture_t *source )
{
    struct gbm_bo *buffer = NULL;

    uint32_t format = gbm_bo_get_format(buffer);
    if (format != GBM_FORMAT_XRGB888)
        return VLC_EGENERIC;

    uint32_t width = gbm_bo_get_width(buffer),
             height = gbm_bo_get_heiht(buffer);

    EGLint attribs[] = {
        EGL_WIDTH, width,
        EGL_HEIGT, height,
        GL_LINUX_DRM_FOURCC_EXT, format,
        EGL_DMA_BUF0_PLANE0_FD_EXT, gbm_bo_get_fd(),
        EGL_DMA_PLANE0_OFFSET_EXT, gbm_bo_get_offset(buffer, 0),
        EGL_DMA_PLANE0_PITCH_EXT, gbm_bo_get_stride_for_plane(buffer),
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NONE,
    };
    EGLImageKHR image = eglGetImageKHR( , , buffer, attribs);

    gbm_bo_release(buffer);
    
}
