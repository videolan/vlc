/*****************************************************************************
 * gui/hmd.cpp: HMD controller interface for vout
 *****************************************************************************
 * Copyright © 2019 the VideoLAN team
 *
 * Authors: Adrien Maglo <magsoft@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <vlc_interface.h>
#include <vlc_vout.h>
#include <vlc_input.h>
#include <vlc_es.h>
#include <vlc_playlist.h>
#include <vlc_meta.h>
#include <vlc_fs.h>
#include <vlc_url.h>
#include <vlc_image.h>
#include <vlc_input.h>

static int  Open(vlc_object_t *p_this);
static void Close(vlc_object_t *p_this);

static picture_t *LoadImage(image_handler_t *p_imgHandler, video_format_t *fmt_in,
                            video_format_t *fmt_out, const char *psz_filename);
static int PlaylistEvent(vlc_object_t *p_this, char const *psz_var,
                         vlc_value_t oldval, vlc_value_t val, void *p_data);


vlc_module_begin ()
    set_shortname("HMDcontroller")
    set_description("HMD controller")
    set_capability("interface", 0)
    set_category(CAT_INTERFACE)
    set_subcategory(SUBCAT_INTERFACE_CONTROL)
    set_callbacks(Open, Close)
    add_shortcut("hmdcontroller")
vlc_module_end ()


struct intf_sys_t
{
    input_thread_t *p_input;
    picture_t *p_hmdPic;
};


static int Open(vlc_object_t *p_this)
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    intf_sys_t *p_sys = p_intf->p_sys = (intf_sys_t *)malloc(sizeof(intf_sys_t));
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    p_sys->p_input = NULL;

    var_AddCallback( pl_Get(p_intf), "input-current", PlaylistEvent, p_intf );

    //VLC_CODEC_RGBA


    image_handler_t *p_imgHandler = image_HandlerCreate(p_intf);
    video_format_t fmt_in, fmt_out;
    video_format_Init(&fmt_in, 0);
    video_format_Init(&fmt_out, VLC_CODEC_RGBA);

    p_sys->p_hmdPic = LoadImage(p_imgHandler, &fmt_in, &fmt_out, "/home/magsoft/Pictures/hmd.png");

    video_format_Clean(&fmt_in);
    video_format_Clean(&fmt_out);
    image_HandlerDelete(p_imgHandler);

    return VLC_SUCCESS;
}


static void Close(vlc_object_t *p_this)
{



}


static picture_t *LoadImage(image_handler_t *p_imgHandler, video_format_t *fmt_in,
                            video_format_t *fmt_out, const char *psz_filename)
{
    picture_t *p_pic;

    char *psz_url = vlc_path2uri(psz_filename, NULL);
    p_pic = image_ReadUrl(p_imgHandler, psz_url, fmt_in, fmt_out);
    free( psz_url );

    return p_pic;
}


static int PlaylistEvent(vlc_object_t *p_this, char const *psz_var,
                         vlc_value_t oldval, vlc_value_t val, void *p_data)
{
    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    intf_sys_t *p_sys = p_intf->p_sys;
    input_thread_t *p_input = (input_thread_t *)val.p_address;

    (void) p_this; (void) psz_var;

    p_sys->p_input = p_input;

    // TODO: update controller picture
    // if (p_input != NULL)
    //     input_UpdateHMDControllerPicture(p_input, p_sys->p_hmdPic);

    return VLC_SUCCESS;
}

