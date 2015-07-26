/*****************************************************************************
 * access.c
 *****************************************************************************
 * Copyright (C) 1999-2008 VLC authors and VideoLAN
 * $Id$
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

#include "access.h"
#include <libvlc.h>
#include <vlc_url.h>
#include <vlc_modules.h>

/* Decode URL (which has had its scheme stripped earlier) to a file path. */
char *get_path(const char *location)
{
    char *url, *path;

    /* Prepending "file://" is a bit hackish. But then again, we do not want
     * to hard-code the list of schemes that use file paths in make_path().
     */
    if (asprintf(&url, "file://%s", location) == -1)
        return NULL;

    path = make_path (url);
    free (url);
    return path;
}

/*****************************************************************************
 * access_New:
 *****************************************************************************/
access_t *access_New(vlc_object_t *parent, input_thread_t *input,
                     const char *mrl)
{
    const char *p = strstr(mrl, "://");
    if (p == NULL)
        return NULL;

    access_t *access = vlc_custom_create(parent, sizeof (*access), "access");
    char *scheme = strndup(mrl, p - mrl);
    char *location = strdup(p + 3);

    if (unlikely(access == NULL || scheme == NULL || location == NULL))
    {
        free(location);
        free(scheme);
        vlc_object_release(access);
        return NULL;
    }

    access->p_input = input;
    access->psz_access = scheme;
    access->psz_location = location;
    access->psz_filepath = get_path(location);
    access->pf_read    = NULL;
    access->pf_block   = NULL;
    access->pf_readdir = NULL;
    access->pf_seek    = NULL;
    access->pf_control = NULL;
    access->p_sys      = NULL;
    access_InitFields(access);

    msg_Dbg(access, "creating access '%s' location='%s', path='%s'", scheme,
            location, access->psz_filepath ? access->psz_filepath : "(null)");

    access->p_module = module_need(access, "access", scheme, true);
    if (access->p_module == NULL)
    {
        free(access->psz_filepath);
        free(access->psz_location);
        free(access->psz_access);
        vlc_object_release(access);
        access = NULL;
    }
    assert(access == NULL || access->pf_control != NULL);
    return access;
}

access_t *vlc_access_NewMRL(vlc_object_t *parent, const char *mrl)
{
    return access_New(parent, NULL, mrl);
}

void vlc_access_Delete(access_t *access)
{
    module_unneed(access, access->p_module);

    free(access->psz_access);
    free(access->psz_location);
    free(access->psz_filepath);
    vlc_object_release(access);
}

/*****************************************************************************
 * access_vaDirectoryControlHelper:
 *****************************************************************************/
int access_vaDirectoryControlHelper( access_t *p_access, int i_query, va_list args )
{
    VLC_UNUSED( p_access );

    switch( i_query )
    {
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            *va_arg( args, bool* ) = false;
            break;
        case ACCESS_GET_PTS_DELAY:
            *va_arg( args, int64_t * ) = 0;
            break;
        default:
            return VLC_EGENERIC;
     }
     return VLC_SUCCESS;
}
