
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>
#include <fcntl.h>
#include <gbm.h>

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>
#include <vlc_opengl.h>
#include <vlc_fs.h>
#include <vlc_vout_window.h>


static int create( vout_window_t * );
static void destroy( vout_window_t * );

static int enable(struct vout_window_t *, const vout_window_cfg_t *);
static void disable(struct vout_window_t *);
static void resize(struct vout_window_t *, unsigned width, unsigned height);


#define GBM_DEV_FILE "/dev/dri/renderD128"

static const struct  vout_window_operations ops = {
    .enable = enable,
    .disable = disable,
    .resize = resize,
    .destroy = destroy,
};

static int create( vout_window_t *wnd )
{
    vlc_object_t *obj = VLC_OBJECT(wnd);

    struct gbm_device **device = &wnd->display.gbm; // Aka shortcuts for lazy
    struct gbm_surface **window = &wnd->handle.gbm;

    wnd->sys = NULL;

    *device = NULL;
    *window = NULL;

    {
        wnd->sys = malloc(sizeof(int));
        if (wnd->sys == NULL)
            return VLC_ENOMEM;

        int fd = vlc_open(GBM_DEV_FILE, O_RDWR);
        *(int *)wnd->sys = fd;
        if (fd == -1)
        {
            msg_Err(obj, "===== Could not open gbm device " GBM_DEV_FILE);
            return VLC_EGENERIC;
        }
        else
            msg_Err(obj, "===== Opened gbm device " GBM_DEV_FILE);

        *device = gbm_create_device(fd);
        if (*device == NULL)
            close(fd);
    }
    if (*device == NULL) // Feels bad to goto from a scope to an other
    {
        msg_Err(obj, "===== Create device Could not create gbm device " GBM_DEV_FILE);
        goto error;
    }

    wnd->ops = &ops;
    wnd->type = VOUT_WINDOW_TYPE_GBM;
    return VLC_SUCCESS;

error:
    destroy(wnd);
    return VLC_EGENERIC;
}

static void destroy( vout_window_t *wnd )
{
    struct gbm_device **device = &wnd->display.gbm; // Aka shortcuts for lazy
    struct gbm_surface **window = &wnd->handle.gbm;

    msg_Err(VLC_OBJECT(wnd), "===== Close gbm module IN\n");
    if (*window != NULL)
    {
        gbm_surface_destroy(*window);
        *window = NULL;
    }
    if (*device != NULL)
    {
        gbm_device_destroy(*device);
        close(*(int *)wnd->sys);
        free(wnd->sys);
    }
    msg_Err(VLC_OBJECT(wnd), "===== Close gbm module OK\n");
}

static int enable(struct vout_window_t *wnd, const vout_window_cfg_t *cfg)
{
    struct gbm_device **device = &wnd->display.gbm; // Aka shortcuts for lazy
    struct gbm_surface **window = &wnd->handle.gbm;

    *window = gbm_surface_create(*device, cfg->width, cfg->height,
                                 GBM_FORMAT_XRGB8888, GBM_BO_USE_RENDERING);
    if (*window == NULL)
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static void disable(struct vout_window_t *wnd)
{
    struct gbm_surface **window = &wnd->handle.gbm;

    gbm_surface_destroy(*window);
}

static void resize(struct vout_window_t *wnd, unsigned width, unsigned height)
{
    VLC_UNUSED(wnd);
    VLC_UNUSED(width);
    VLC_UNUSED(height);
}

vlc_module_begin()
    set_description( N_("A gbm pseudo window with EGL for OpenGL") )
    set_shortname(N_("Video gbm"))

    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)

    set_capability("vout window", 0)
    set_callback(create)
    add_shortcut("gbm")
vlc_module_end()
