/*****************************************************************************
 * flaschentaschen.c: Flaschen-Taschen video output display for vlc
 * cf. https://github.com/hzeller/flaschen-taschen
 *****************************************************************************
 * Copyright (C) 2000-2009 VLC authors and VideoLAN
 * Copyright (C) 2016 François Revol <revol@free.fr>
 *
 * Includes code from vdummy.c and aa.c:
 * Authors: Samuel Hocevar <sam@zoy.org>
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
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

#include <errno.h>
#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif

#include <vlc_common.h>
#include <vlc_fs.h>
#include <vlc_network.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>

#define T_FLDISPLAY N_("Flaschen-Taschen display address")
#define LT_FLDISPLAY N_( \
    "IP address or hostname of the Flaschen-Taschen display. " \
    "Something like ft.noise or ftkleine.noise")

#define T_WIDTH N_("Width")
#define LT_WIDTH N_("Video width")

#define T_HEIGHT N_("Height")
#define LT_HEIGHT N_("Video height")

static int Open( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("Flaschen") )
    set_description( N_("Flaschen-Taschen video output") )
    set_capability( "vout display", 0 )
    set_callbacks( Open, Close )
    add_shortcut( "flaschen" )

    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VOUT )
    add_string( "flaschen-display", NULL, T_FLDISPLAY, LT_FLDISPLAY, true )
    add_integer("flaschen-width", 25, T_WIDTH, LT_WIDTH, false)
    add_integer("flaschen-height", 20, T_HEIGHT, LT_HEIGHT, false)
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct vout_display_sys_t {
    int             fd;

    picture_pool_t *pool;
};
static picture_pool_t *Pool(vout_display_t *, unsigned count);
static void            Display(vout_display_t *, picture_t *, subpicture_t *);
static int             Control(vout_display_t *, int, va_list);

/*****************************************************************************
 * Open: activates flaschen vout display method
 *****************************************************************************/
static int Open(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys;
    int fd;
    unsigned port = 1337;

    vd->sys = sys = calloc(1, sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;
    sys->pool = NULL;
    sys->fd = -1;

    /* */
    video_format_t fmt = vd->fmt;
    fmt.i_chroma = VLC_CODEC_RGB24;
    /* TODO: check if this works on big-endian systems */
    fmt.i_rmask = 0xff0000;
    fmt.i_gmask = 0x00ff00;
    fmt.i_bmask = 0x0000ff;
    fmt.i_width  = var_InheritInteger(vd, "flaschen-width");
    fmt.i_height = var_InheritInteger(vd, "flaschen-height");
    fmt.i_visible_width = fmt.i_width;
    fmt.i_visible_height = fmt.i_height;

    /* p_vd->info is not modified */

    char *display = var_InheritString(vd, "flaschen-display");
    if (display == NULL) {
        msg_Err(vd, "missing flaschen-display");
        free(sys);
        return VLC_EGENERIC;
    }
    msg_Dbg(vd, "using display at %s (%dx%d)", display, fmt.i_width, fmt.i_height);

    fd = net_ConnectDgram( vd, display, port, -1, IPPROTO_UDP );

    if( fd == -1 )
    {
        msg_Err( vd,
                 "cannot create UDP socket for %s port %u",
                 display, port );
        free(display);
        free(sys);
        return VLC_EGENERIC;
    }
    free(display);
    sys->fd = fd;

    /* Ignore any unexpected incoming packet */
    setsockopt (fd, SOL_SOCKET, SO_RCVBUF, &(int){ 0 }, sizeof (int));


    vd->fmt     = fmt;

    vd->pool    = Pool;
    vd->prepare = NULL;
    vd->display = Display;
    vd->control = Control;

    vout_display_DeleteWindow(vd, NULL);

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys = vd->sys;

    if (sys->pool)
        picture_pool_Release(sys->pool);

    net_Close(sys->fd);
    free(sys);
}

static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    vout_display_sys_t *sys = vd->sys;
    if (!sys->pool)
        sys->pool = picture_pool_NewFromFormat(&vd->fmt, count);
    return sys->pool;
}

static void Display(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
#ifdef IOV_MAX
    const long iovmax = IOV_MAX;
#else
    const long iovmax = sysconf(_SC_IOV_MAX);
#endif
    vout_display_sys_t *sys = vd->sys;
    int result;
    VLC_UNUSED(subpicture);

    char buffer[64];
    int header_len = snprintf(buffer, sizeof(buffer), "P6\n%d %d\n255\n",
                              vd->fmt.i_width, vd->fmt.i_height);
    /* TODO: support offset_{x,y,z}? (#FT:...) */
    /* Note the protocol doesn't include any picture order field. */
    /* (maybe add as comment?) */

    int iovcnt = 1 + vd->fmt.i_height;
    if (unlikely(iovcnt > iovmax))
        return;

    struct iovec iov[iovcnt];
    iov[0].iov_base = buffer;
    iov[0].iov_len = header_len;

    uint8_t *src = picture->p->p_pixels;
    for (int i = 1; i < iovcnt; i++)
    {
        iov[i].iov_base = src;
        iov[i].iov_len = vd->fmt.i_width * 3;
        src += picture->p->i_pitch;
    }

    struct msghdr hdr = {
        .msg_name = NULL,
        .msg_namelen = 0,
        .msg_iov = iov,
        .msg_iovlen = iovcnt,
        .msg_control = NULL,
        .msg_controllen = 0,
        .msg_flags = 0 };

    result = sendmsg(sys->fd, &hdr, 0);
    if (result < 0)
        msg_Err(vd, "sendmsg: error %s in vout display flaschen", vlc_strerror_c(errno));
    else if (result < (int)(header_len + vd->fmt.i_width * vd->fmt.i_height * 3))
        msg_Err(vd, "sendmsg only sent %d bytes in vout display flaschen", result);
        /* we might want to drop some frames? */

    picture_Release(picture);
}

/**
 * Control for vout display
 */
static int Control(vout_display_t *vd, int query, va_list args)
{
    VLC_UNUSED(args);

    switch (query) {
    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
    case VOUT_DISPLAY_CHANGE_ZOOM:
    case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        return VLC_EGENERIC;

    default:
        msg_Err(vd, "Unsupported query in vout display flaschen");
        return VLC_EGENERIC;
    }
}
