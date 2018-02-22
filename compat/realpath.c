/*****************************************************************************
 * realpath.c: POSIX realpath replacement
 *****************************************************************************
 * Copyright © 2016 VLC authors and VideoLAN
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
# include <config.h>
#endif

#include <stdlib.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#endif

char *realpath(const char * restrict relpath, char * restrict resolved_path)
{
    if (relpath == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

#ifdef _WIN32
    size_t len = MultiByteToWideChar( CP_UTF8, 0, relpath, -1, NULL, 0 );
    if (len == 0)
        return NULL;

    wchar_t *wrelpath = malloc(len * sizeof (wchar_t));
    if (wrelpath == NULL)
        return NULL;

    MultiByteToWideChar( CP_UTF8, 0, relpath, -1, wrelpath, len );

    wchar_t *wfullpath = _wfullpath( NULL, wrelpath, _MAX_PATH );
    free(wrelpath);
    if (wfullpath != NULL)
    {
        len = WideCharToMultiByte( CP_UTF8, 0, wfullpath, -1, NULL, 0, NULL, NULL );
        if (len != 0)
        {
            if (resolved_path != NULL)
                len = len >= _MAX_PATH ? _MAX_PATH : len;
            else
                resolved_path = (char *)malloc(len);

            if (resolved_path != NULL)
                WideCharToMultiByte( CP_UTF8, 0, wfullpath, -1, resolved_path, len, NULL, NULL );
            free(wfullpath);
            return resolved_path;
        }
        free(wfullpath);
    }
#else
    (void)resolved_path;
#endif
    errno = EACCES;
    return NULL;
}
