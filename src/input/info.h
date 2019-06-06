/*****************************************************************************
 * info.h
 *****************************************************************************
 * Copyright (C) 2010 Laurent Aimar
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#ifndef LIBVLC_INPUT_INFO_H
#define LIBVLC_INPUT_INFO_H 1

#include "vlc_input_item.h"

static inline info_t *info_New(const char *name)
{
    info_t *info = malloc(sizeof(*info));
    if (!info)
        return NULL;

    info->psz_name = strdup(name);
    info->psz_value = NULL;
    return info;
}

static inline void info_Delete(info_t *i)
{
    free(i->psz_name);
    free(i->psz_value);
    free(i);
}

static inline info_category_t *info_category_New(const char *name)
{
    info_category_t *cat = malloc(sizeof(*cat));
    if (!cat)
        return NULL;
    cat->psz_name = strdup(name);
    vlc_list_init(&cat->infos);
    return cat;
}

static inline info_t *info_category_FindInfo(const info_category_t *cat,
                                             const char *name)
{
    info_t *info;

    info_foreach(info, &cat->infos)
        if (!strcmp(info->psz_name, name))
            return info;
    return NULL;
}

static inline void info_category_ReplaceInfo(info_category_t *cat,
                                             info_t *info)
{
    info_t *old = info_category_FindInfo(cat, info->psz_name);
    if (old) {
        vlc_list_remove(&old->node);
        info_Delete(old);
    }
    vlc_list_append(&info->node, &cat->infos);
}

static inline info_t *info_category_VaAddInfo(info_category_t *cat,
                                              const char *name,
                                              const char *format, va_list args)
{
    info_t *info = info_category_FindInfo(cat, name);
    if (!info) {
        info = info_New(name);
        if (!info)
            return NULL;
        vlc_list_append(&info->node, &cat->infos);
    } else
        free(info->psz_value);
    if (vasprintf(&info->psz_value, format, args) == -1)
        info->psz_value = NULL;
    return info;
}

static inline info_t *info_category_AddInfo(info_category_t *cat,
                                            const char *name,
                                            const char *format, ...)
{
    va_list args;

    va_start(args, format);
    info_t *info = info_category_VaAddInfo(cat, name, format, args);
    va_end(args);

    return info;
}

static inline int info_category_DeleteInfo(info_category_t *cat, const char *name)
{
    info_t *info = info_category_FindInfo(cat, name);
    if (info != NULL) {
        vlc_list_remove(&info->node);
        info_Delete(info);
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

static inline void info_category_Delete(info_category_t *cat)
{
    info_t *info;

    while ((info = vlc_list_first_entry_or_null(&cat->infos, info_t, node))) {
        vlc_list_remove(&info->node);
        info_Delete(info);
    }
    free(cat->psz_name);
    free(cat);
}

#endif
