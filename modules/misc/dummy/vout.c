/*****************************************************************************
 * vout.c: Dummy video output display method for testing purposes
 *****************************************************************************
 * Copyright (C) 2000-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_vout_display.h>
#include "dummy.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct vout_display_sys_t {
    picture_pool_t *pool;
};
static picture_pool_t *Pool(vout_display_t *, unsigned count);
static void            Display(vout_display_t *, picture_t *);
static void            DisplayStat(vout_display_t *, picture_t *);
static int             Control(vout_display_t *, int, va_list);
static void            Manage (vout_display_t *);

/*****************************************************************************
 * OpenVideo: activates dummy vout display method
 *****************************************************************************/
static int OpenVideoCommon(vlc_object_t *object, bool display_stat)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys;

    vd->sys = sys = calloc(1, sizeof(*sys));
    if (!sys)
        return VLC_EGENERIC;
    sys->pool = NULL;

    /* p_vd->info is not modified */

    char *chroma = var_CreateGetNonEmptyString(vd, "dummy-chroma");
    if (chroma) {
        vlc_fourcc_t fcc = vlc_fourcc_GetCodecFromString(VIDEO_ES, chroma);
        if (fcc != 0) {
            msg_Dbg(vd, "forcing chroma 0x%.8x (%4.4s)", fcc, (char*)&fcc);
            vd->fmt.i_chroma = fcc;
        }
        free(chroma);
    }
    vd->pool    = Pool;
    vd->prepare = NULL;
    vd->display = display_stat ? DisplayStat : Display;
    vd->control = Control;
    vd->manage  = Manage;

    return VLC_SUCCESS;
}
int OpenVideo(vlc_object_t *object)
{
    return OpenVideoCommon(object, false);
}
int OpenVideoStat(vlc_object_t *object)
{
    return OpenVideoCommon(object, true);
}

void CloseVideo(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys = vd->sys;

    picture_pool_Delete(sys->pool);
    free(sys);
}

static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    vout_display_sys_t *sys = vd->sys;
    if (!sys->pool)
        sys->pool = picture_pool_NewFromFormat(&vd->fmt, count);
    return sys->pool;
}

static void Display(vout_display_t *vd, picture_t *picture)
{
    VLC_UNUSED(vd);
    picture_Release(picture);
}

static void DisplayStat(vout_display_t *vd, picture_t *picture)
{
    VLC_UNUSED(vd);
    if (vd->fmt.i_width*vd->fmt.i_height >= sizeof(mtime_t)) {
        mtime_t date;
        memcpy(&date, picture->p->p_pixels, sizeof(date));
        msg_Dbg(vd, "VOUT got %"PRIu64" ms offset",
                (mdate() - date) / 1000 );
    }
    picture_Release(picture);
}

static int Control(vout_display_t *vd, int query, va_list args)
{
    VLC_UNUSED(vd);
    VLC_UNUSED(query);
    VLC_UNUSED(args);
    return VLC_SUCCESS;
}

static void Manage(vout_display_t *vd)
{
    VLC_UNUSED(vd);
}
