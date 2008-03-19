/*****************************************************************************
 * fixups.h: portability fixups included from config.h
 *****************************************************************************
 * Copyright © 1998-2007 the VideoLAN project
 * $Id$
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

/**
 * \file
 * This file is a collection of portability fixes
 */

#ifndef LIBVLC_FIXUPS_H
# define LIBVLC_FIXUPS_H 1

#ifndef HAVE_STRDUP
# define strdup vlc_strdup
#endif

#ifndef HAVE_VASPRINTF
# define vasprintf vlc_vasprintf
#endif

#ifndef HAVE_ASPRINTF
# define asprintf vlc_asprintf
#endif

#ifndef HAVE_STRNDUP
# define strndup vlc_strndup
#endif

#ifndef HAVE_STRNLEN
# define strnlen vlc_strnlen
#endif

#ifndef HAVE_STRLCPY
# define strlcpy vlc_strlcpy
#endif

#ifndef HAVE_ATOF
# define atof vlc_atof
#endif

#ifndef HAVE_STRTOF
# ifdef HAVE_STRTOD
#  define strtof( a, b ) ((float)strtod (a, b))
# endif
#endif

#ifndef HAVE_ATOLL
# define atoll vlc_atoll
#endif

#ifndef HAVE_STRTOLL
# define strtoll vlc_strtoll
#endif

#ifndef HAVE_LLDIV
# define lldiv vlc_lldiv
#endif

#ifndef HAVE_SCANDIR
# define scandir vlc_scandir
# define alphasort vlc_alphasort
#endif

#ifndef HAVE_GETENV
# define getenv vlc_getenv
#endif

#ifndef HAVE_STRCASECMP
# ifndef HAVE_STRICMP
#  define strcasecmp vlc_strcasecmp
# else
#  define strcasecmp stricmp
# endif
#endif

#ifndef HAVE_STRNCASECMP
# ifndef HAVE_STRNICMP
#  define strncasecmp vlc_strncasecmp
# else
#  define strncasecmp strnicmp
# endif
#endif

#ifndef HAVE_STRCASESTR
# ifndef HAVE_STRISTR
#  define strcasestr vlc_strcasestr
# else
#  define strcasestr stristr
# endif
#endif

#ifndef HAVE_LOCALTIME_R
/* If localtime_r() is not provided, we assume localtime() uses
 * thread-specific storage. */
# include <time.h>
static struct tm *localtime_r (const time_t *timep, struct tm *result)
{
    struct tm *s = localtime (timep);
    if (s == NULL)
        return NULL;

    *result = *s;
    return result;
}
#endif

#endif /* !LIBVLC_FIXUPS_H */
