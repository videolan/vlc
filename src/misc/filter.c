/*****************************************************************************
 * filter.c : filter_t helpers.
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 *
 * Author: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#include <assert.h>

#include <vlc_common.h>
#include <libvlc.h>
#include <vlc_filter.h>
#include <vlc_modules.h>
#include "../misc/variables.h"

/* */

static int TriggerFilterCallback(vlc_object_t *p_this, char const *psz_var,
                                 vlc_value_t oldval, vlc_value_t newval,
                                 void *p_data)
{
    (void) p_this; (void) oldval;
    var_Set((filter_t *)p_data, psz_var, newval);
    return 0;
}

#undef filter_AddProxyCallbacks
void filter_AddProxyCallbacks( vlc_object_t *obj, filter_t *filter,
                               vlc_callback_t restart_cb )
{
    char **names = var_GetAllNames(VLC_OBJECT(filter));
    if (names == NULL)
        return;

    for (char **pname = names; *pname != NULL; pname++)
    {
        char *name = *pname;
        int var_type = var_Type(filter, name);
        if (var_Type(obj, name) || config_GetType(name) == 0)
        {
            free(name);
            continue;
        }
        var_Create(obj, name,
                   var_type | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND);
        if ((var_type & VLC_VAR_ISCOMMAND))
            var_AddCallback(obj, name, TriggerFilterCallback, filter);
        else
            var_AddCallback(obj, name, restart_cb, obj);
        free(name);
    }
    free(names);
}

#undef filter_DelProxyCallbacks
void filter_DelProxyCallbacks( vlc_object_t *obj, filter_t *filter,
                               vlc_callback_t restart_cb )
{
    char **names = var_GetAllNames(VLC_OBJECT(filter));
    if (names == NULL)
        return;

    for (char **pname = names; *pname != NULL; pname++)
    {
        char *name = *pname;
        if (!(var_Type(obj, name) & VLC_VAR_ISCOMMAND))
        {
            free(name);
            continue;
        }
        int filter_var_type = var_Type(filter, name);

        if (filter_var_type & VLC_VAR_ISCOMMAND)
            var_DelCallback(obj, name, TriggerFilterCallback, filter);
        else if (filter_var_type)
            var_DelCallback(obj, name, restart_cb, obj);
        var_Destroy(obj, name);
        free(name);
    }
    free(names);
}

/* */

vlc_blender_t *filter_NewBlend( vlc_object_t *p_this,
                           const video_format_t *p_dst_chroma )
{
    vlc_blender_t *p_blend = vlc_custom_create( p_this, sizeof(*p_blend), "blend" );
    if( !p_blend )
        return NULL;

    es_format_Init( &p_blend->fmt_in, VIDEO_ES, 0 );

    es_format_Init( &p_blend->fmt_out, VIDEO_ES, 0 );

    p_blend->fmt_out.i_codec        =
    p_blend->fmt_out.video.i_chroma = p_dst_chroma->i_chroma;
    p_blend->fmt_out.video.i_rmask  = p_dst_chroma->i_rmask;
    p_blend->fmt_out.video.i_gmask  = p_dst_chroma->i_gmask;
    p_blend->fmt_out.video.i_bmask  = p_dst_chroma->i_bmask;

    /* The blend module will be loaded when needed with the real
    * input format */
    p_blend->p_module = NULL;

    return p_blend;
}

int filter_ConfigureBlend( vlc_blender_t *p_blend,
                           int i_dst_width, int i_dst_height,
                           const video_format_t *p_src )
{
    /* */
    if( p_blend->p_module &&
        p_blend->fmt_in.video.i_chroma != p_src->i_chroma )
    {
        /* The chroma is not the same, we need to reload the blend module */
        module_unneed( p_blend, p_blend->p_module );
        p_blend->p_module = NULL;
    }

    /* */

    p_blend->fmt_in.i_codec = p_src->i_chroma;
    p_blend->fmt_in.video   = *p_src;

    /* */
    p_blend->fmt_out.video.i_width          =
    p_blend->fmt_out.video.i_visible_width  = i_dst_width;
    p_blend->fmt_out.video.i_height         =
    p_blend->fmt_out.video.i_visible_height = i_dst_height;

    /* */
    if( !p_blend->p_module )
        p_blend->p_module = module_need( p_blend, "video blending", NULL, false );
    if( !p_blend->p_module )
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

int filter_Blend( vlc_blender_t *p_blend,
                  picture_t *p_dst, int i_dst_x, int i_dst_y,
                  const picture_t *p_src, int i_alpha )
{
    if( !p_blend->p_module )
        return VLC_EGENERIC;

    p_blend->pf_video_blend( p_blend, p_dst, p_src, i_dst_x, i_dst_y, i_alpha );
    return VLC_SUCCESS;
}

void filter_DeleteBlend( vlc_blender_t *p_blend )
{
    if( p_blend->p_module )
        module_unneed( p_blend, p_blend->p_module );

    vlc_object_delete(p_blend);
}

/* */
#include <vlc_video_splitter.h>

video_splitter_t *video_splitter_New( vlc_object_t *p_this,
                                      const char *psz_name,
                                      const video_format_t *p_fmt )
{
    video_splitter_t *p_splitter = vlc_custom_create( p_this,
                                       sizeof(*p_splitter), "video splitter" );
    if( !p_splitter )
        return NULL;

    video_format_Copy( &p_splitter->fmt, p_fmt );

    /* */
    p_splitter->p_module = module_need( p_splitter, "video splitter", psz_name, true );
    if( ! p_splitter->p_module )
    {
        video_splitter_Delete( p_splitter );
        return NULL;
    }

    return p_splitter;
}

void video_splitter_Delete( video_splitter_t *p_splitter )
{
    if( p_splitter->p_module )
        module_unneed( p_splitter, p_splitter->p_module );

    video_format_Clean( &p_splitter->fmt );
    vlc_object_delete(p_splitter);
}
