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

#undef access_New
/*****************************************************************************
 * access_New:
 *****************************************************************************/
access_t *access_New( vlc_object_t *p_obj, input_thread_t *p_parent_input,
                      const char *psz_access, const char *psz_demux,
                      const char *psz_location )
{
    access_t *p_access = vlc_custom_create( p_obj, sizeof (*p_access),
                                            "access" );

    if( p_access == NULL )
        return NULL;

    /* */

    p_access->p_input = p_parent_input;

    p_access->psz_access = strdup( psz_access );
    p_access->psz_location = strdup( psz_location );
    p_access->psz_filepath = get_path( psz_location );
    p_access->psz_demux  = strdup( psz_demux );
    if( p_access->psz_access == NULL || p_access->psz_location == NULL
     || p_access->psz_demux == NULL )
        goto error;

    msg_Dbg( p_obj, "creating access '%s' location='%s', path='%s'",
             psz_access, psz_location,
             p_access->psz_filepath ? p_access->psz_filepath : "(null)" );

    p_access->pf_read    = NULL;
    p_access->pf_block   = NULL;
    p_access->pf_seek    = NULL;
    p_access->pf_control = NULL;
    p_access->p_sys      = NULL;

    access_InitFields( p_access );

    p_access->p_module = module_need( p_access, "access", psz_access, true );
    if( p_access->p_module == NULL )
        goto error;

    return p_access;

error:
    free( p_access->psz_access );
    free( p_access->psz_location );
    free( p_access->psz_filepath );
    free( p_access->psz_demux );
    vlc_object_release( p_access );
    return NULL;
}

/*****************************************************************************
 * access_Delete:
 *****************************************************************************/
void access_Delete( access_t *p_access )
{
    module_unneed( p_access, p_access->p_module );

    free( p_access->psz_access );
    free( p_access->psz_location );
    free( p_access->psz_filepath );
    free( p_access->psz_demux );

    vlc_object_release( p_access );
}


/*****************************************************************************
 * access_GetParentInput:
 *****************************************************************************/
input_thread_t * access_GetParentInput( access_t *p_access )
{
    return p_access->p_input ? vlc_object_hold((vlc_object_t *)p_access->p_input) : NULL;
}

