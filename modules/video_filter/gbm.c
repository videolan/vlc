#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_picture.h>
#include <vlc_opengl.h>
#include <vlc_atomic.h>

#include <gbm.h>

#define GBM_DEV_FILE "/dev/dri/renderD128"

struct vlc_gl_gbm
{
    struct gbm_device       *device;
};

struct vlc_gbm_video_context
{
    picture_pool_t          *pool;
    picture_t               *pictures[2];
    struct gbm_surface      *surfaces[2];
};

/**
 * The filter part
 */

struct gbm_context
{
    struct picture_context_t cbs;
    struct gbm_bo *bo;
    struct gbm_surface *surface;
    vlc_atomic_rc_t rc;
    vlc_mutex_t *lock;
    vlc_cond_t *cond;
};

static picture_context_t *picture_context_copy(picture_context_t *input)
{
    struct gbm_context *context = (struct gbm_context *)input;

    vlc_atomic_rc_inc(&context->rc);
    return input;
}

static void picture_context_destroy(picture_context_t *input)
{
    struct gbm_context *context = (struct gbm_context *)input;

    if (vlc_atomic_rc_dec(&context->rc))
    {
        vlc_mutex_lock(context->lock);
        if (context->bo != NULL)
            gbm_surface_release_buffer(context->surface, context->bo);
        vlc_cond_signal(context->cond);
        vlc_mutex_unlock(context->lock);
        free(context);
    }
}

static void Close( vlc_gl_t *gl )
{
    struct vlc_gl_gbm *sys = gl->sys;
    // TODO
    gbm_surface_destroy(sys->surfaces[0]);
    gbm_device_destroy(sys->device);
}

static picture_t *Swap(vlc_gl_t *gl)
{
    struct vlc_gl_gbm *sys = gl->sys;
    picture_t *current_picture = sys->current;

    //context->surface = sys->gl->surface->handle.gbm;
    //context->cbs.destroy = picture_context_destroy;
    //context->cbs.copy = picture_context_copy;
    //output->context = (picture_context_t *)context;
    // TODO
    output->context->vctx = NULL;

    assert(current_picture->context);
    struct vlc_gbm_picture_context *context = container_of(
            picture->context, struct vlc_gbm_picture_context*, context);

    assert(gbm_surface_has_free_buffers(context->surface) >= 0)

    /* context->bo or picture sys ? */
    context->bo = gbm_surface_lock_front_buffer(context->surface);
    if (context->bo == NULL)
    {
        picture_Release(output);
        output = NULL;
    }

    picture_t *new_picture = picture_pool_Wait(sys->pool);
    assert(new_picture->context);
    context = container_of(new_picture->context,
                           struct vlc_gbm_picture_context*, context);

    /* set current */
}

static int Open(vlc_gl_t *gl, unsigned width, unsigned height)
{
    struct vlc_gl_gbm *sys = vlc_obj_malloc(&gl->obj, sizeof *sys);
    if (sys == NULL)
        return VLC_ENOMEM;

    int fd = vlc_open(GBM_DEV_FILE, O_RDWR);
    if (fd == -1)
        return VLC_EGENERIC;

    sys->device = gbm_create_device(fd);
    if (sys->device == NULL)
    {
        vlc_close(fd);
        return VLC_EGENERIC;
    }

    sys->surfaces[0] = gbm_surface_create(sys->device, width, height,
            GBM_FORMAT_XRGB8888, GBM_BO_USE_RENDERING);
    if (sys->surfaces[0] == NULL)
    {
        gmb_device_destroy(sys->device);
        vlc_close(fd);
        return VLC_EGENERIC;
    }

    // TODO: create picture with their context
    // TODO: create pool

    gl->ext = VLC_GL_EXT_EGL;
    gl->makeCurrent = MakeCurrent;
    gl->releaseCurrent = ReleaseCurrent;
    gl->resize = NULL;
    gl->swap = Swap;
    gl->getProcAddress = GetSymbol;
    gl->destroy = Close;
    gl->egl.queryString = QueryString;

    static const struct vlc_video_context_operations ops =
    {
        .destroy = CleanFromVideoContext,
    };

    gl->vctx_out =
        vlc_video_context_Create(gl->device, VLC_VIDEO_CONTEXT_GBM,
                                 sizeof gbm_video_context_t, *ops);

    //filter->pf_video_filter = filter_input;
    //filter->fmt_out.i_codec = VLC_CODEC_GBM;
    //filter->fmt_out.video.i_chroma = VLC_CODEC_GBM;
    //filter->fmt_out.video.i_visible_width = filter->fmt_in.video.i_visible_width;
    //filter->fmt_out.video.i_visible_height = filter->fmt_in.video.i_visible_height;

    return VLC_SUCCESS;

error:
    destroy(obj);
    return VLC_EGENERIC;
}

static picture_t *Convert( filter_t *filter, picture_t *input )
{
    struct vlc_gl_gbm *sys = filter->p_sys;
    struct gbm_context *context = malloc(sizeof *context);
    picture_resource_t pict_resource = {
        .p_sys = input->p_sys,
    };

    for (int i = 0; i < PICTURE_PLANE_MAX; i++)
    {
        pict_resource.p[i].p_pixels = input->p[i].p_pixels;
        pict_resource.p[i].i_lines = input->p[i].i_lines;
        pict_resource.p[i].i_pitch = input->p[i].i_pitch;
    }

    if (context == NULL)
        return NULL;

    picture_t *output = picture_NewFromResource(&filter->fmt_out.video, &pict_resource);

    if (output == NULL)
    {
        free(context);
        return NULL;
    }

    picture_CopyProperties( output, input );

    context->surface = sys->gl->surface->handle.gbm;
    context->cbs.destroy = picture_context_destroy;
    context->cbs.copy = picture_context_copy;
    output->context = (picture_context_t *)context;
    // TODO
    output->context->vctx = NULL;

    assert(gbm_surface_has_free_buffers(context->surface) >= 0)
    context->bo = gbm_surface_lock_front_buffer(context->surface);
    if (context->bo == NULL)
    {
        picture_Release(output);
        output = NULL;
    }
    // free(input);
    picture_Release(input);
    return output;
}

static picture_t *ConvFilter(filter_t *filter, picture_t *input)
{
    struct vlc_gbm_picture_context *context =
        (struct vlc_gbm_picture_context *)input->context;

    picture_t   *output = NULL;

    if (context->bo != NULL)
    {
        uint32_t width = gbm_bo_get_width(context->bo);
        uint32_t height = gbm_bo_get_height(context->bo);
        uint32_t stride = gbm_bo_get_stride(context->bo);
        uint32_t offset;

        void *buffer = NULL;
        void *base = gbm_bo_map(context->bo, 0, 0, width, height,
                                GBM_BO_TRANSFER_READ, &offset, &buffer);
        if (base != NULL)
        {
            output = filter_NewPicture(filter);
            if (output != NULL)
            {
                assert(gbm_bo_get_format(context->bo) == GBM_FORMAT_XRGB8888);
                msg_Info(filter, "stride = %u", stride);
                msg_Info(filter, "offset = %u", offset);
                msg_Info(filter, "ASSERT: %u = %u",
                         output->format.i_visible_width, width);
                msg_Info(filter, "ASSERT: %u = %u",
                         output->format.i_visible_height, height);
                msg_Info(filter, "plane count = %u", gbm_bo_get_plane_count(context->bo));
                msg_Info(filter, "bit pp = %u", gbm_bo_get_bpp(context->bo));
                assert(output->i_planes == 1);
                assert(output->format.i_visible_width == width);
                assert(output->format.i_visible_height == height);

                for(size_t line=0; line < height; ++line)
                {
                    /* Why memcpy ? */
                    memcpy((char*)output->p[0].p_pixels + line*width, (const char*)base + offset * line, width);
                }

                picture_CopyProperties( output, input );
            }
            gbm_bo_unmap(context->bo, base);

        }
        else
            msg_Err(filter, "gbm_bo_map failed");
    }
    /* Should clone picture */
    picture_Release(input);
    return output;
}

static int OpenConv(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;

    if (filter->fmt_in.i_codec != VLC_CODEC_GBM)
        return VLC_EGENERIC;

    if (filter->fmt_out.i_codec != VLC_CODEC_RGB32)
        return VLC_EGENERIC;

    fprintf(stderr, "CPU filter converting %4.4s -> %4.4s\n",
            (char *)&filter->fmt_in.i_codec,
            (char *)&filter->fmt_out.i_codec);

    filter->fmt_out.i_codec
        = filter->fmt_out.video.i_chroma
        = VLC_CODEC_RGBA;

    filter->fmt_out.video.i_visible_width
        = filter->fmt_out.video.i_width
        = filter->fmt_in.video.i_visible_width;

    filter->fmt_out.video.i_visible_height
        = filter->fmt_out.video.i_height
        = filter->fmt_in.video.i_visible_height;

    filter->pf_video_filter = ConvFilter;

    return VLC_SUCCESS;
}

static void CloseConv(vlc_object_t *obj)
{
}

vlc_module_begin()

    set_shortname( N_("egl_gbm") )
    set_description( N_("GBM Opengl filter") )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    add_shortcut( "egl_gbm" )
    set_capability( "opengl es2 offscreen", 100)
    set_callback( Open )

    add_submodule()
        set_shortname( N_("gbm_converter") )
        set_description( N_("Put an offscreen surface in CPU") )
        set_capability( "video converter", 100 )
        set_callbacks( OpenConv, CloseConv)

vlc_module_end()
