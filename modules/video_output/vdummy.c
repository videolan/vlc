/*****************************************************************************
 * vdummy.c: Dummy video output display method for testing purposes
 *****************************************************************************
 * Copyright (C) 2000-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>

#define CHROMA_TEXT N_("Dummy image chroma format")
#define CHROMA_LONGTEXT N_( \
    "Force the dummy video output to create images using a specific chroma " \
    "format instead of trying to improve performances by using the most " \
    "efficient one.")

static int OpenDummy(vout_display_t *vd, const vout_display_cfg_t *cfg,
                     video_format_t *fmtp, vlc_video_context *context);
static int OpenStats(vout_display_t *vd, const vout_display_cfg_t *cfg,
                     video_format_t *fmtp, vlc_video_context *context);
static void Close(vout_display_t *vd);

vlc_module_begin ()
    set_shortname( N_("Dummy") )
    set_description( N_("Dummy video output") )
    set_capability( "vout display", 0 )
    set_callbacks( OpenDummy, Close )
    add_shortcut( "dummy" )

    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VOUT )
    add_string( "dummy-chroma", NULL, CHROMA_TEXT, CHROMA_LONGTEXT, true )

    add_submodule ()
    set_description( N_("Statistics video output") )
    set_capability( "vout display", 0 )
    add_shortcut( "stats" )
    set_callbacks( OpenStats, Close )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct vout_display_sys_t {
};
static void            DisplayStat(vout_display_t *, picture_t *);
static int             Control(vout_display_t *, int, va_list);

/*****************************************************************************
 * OpenVideo: activates dummy vout display method
 *****************************************************************************/
static int Open(vout_display_t *vd, video_format_t *fmt,
                void (*display)(vout_display_t *, picture_t *))
{
    vout_display_sys_t *sys;

    vd->sys = sys = calloc(1, sizeof(*sys));
    if (!sys)
        return VLC_EGENERIC;

    /* p_vd->info is not modified */

    char *chroma = var_InheritString(vd, "dummy-chroma");
    if (chroma) {
        vlc_fourcc_t fcc = vlc_fourcc_GetCodecFromString(VIDEO_ES, chroma);
        if (fcc != 0) {
            msg_Dbg(vd, "forcing chroma 0x%.8x (%4.4s)", fcc, (char*)&fcc);
            fmt->i_chroma = fcc;
        }
        free(chroma);
    }
    vd->prepare = NULL;
    vd->display = display;
    vd->control = Control;

    return VLC_SUCCESS;
}

static int OpenDummy(vout_display_t *vd, const vout_display_cfg_t *cfg,
                     video_format_t *fmtp, vlc_video_context *context)
{
    (void) cfg; (void) context;
    return Open(vd, fmtp, NULL);
}

static int OpenStats(vout_display_t *vd, const vout_display_cfg_t *cfg,
                     video_format_t *fmtp, vlc_video_context *context)
{
    (void) cfg; (void) context;
    return Open(vd, fmtp, DisplayStat);
}

static void Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    free(sys);
}

static void DisplayStat(vout_display_t *vd, picture_t *picture)
{
    plane_t *p = picture->p;

    VLC_UNUSED(vd);

    if (picture->format.i_width * picture->format.i_height >= sizeof (vlc_tick_t)
     && (p->i_pitch * p->i_lines) >= (ssize_t)sizeof (vlc_tick_t)) {
        vlc_tick_t date;
        memcpy(&date, p->p_pixels, sizeof(date));
        msg_Dbg(vd, "VOUT got %"PRIu64" ms offset",
                MS_FROM_VLC_TICK(vlc_tick_now() - date));
    }
}

static int Control(vout_display_t *vd, int query, va_list args)
{
    VLC_UNUSED(vd);
    VLC_UNUSED(query);
    VLC_UNUSED(args);
    return VLC_SUCCESS;
}
