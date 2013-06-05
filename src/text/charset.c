/*****************************************************************************
 * charset.c: Locale's character encoding stuff.
 *****************************************************************************
 * See also unicode.c for Unicode to locale conversion helpers.
 *
 * Copyright (C) 2003-2008 VLC authors and VideoLAN
 *
 * Authors: Christophe Massiot
 *          Rémi Denis-Courmont
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

#include <vlc_common.h>

#if !defined _WIN32
# include <locale.h>
#else
# include <windows.h>
#endif

#ifdef __APPLE__
#   include <string.h>
#   include <xlocale.h>
#endif

#include "libvlc.h"
#include <vlc_charset.h>

/**
 * us_strtod() has the same prototype as ANSI C strtod() but it uses the
 * POSIX/C decimal format, regardless of the current numeric locale.
 */
double us_strtod( const char *str, char **end )
{
    locale_t loc = newlocale (LC_NUMERIC_MASK, "C", NULL);
    locale_t oldloc = uselocale (loc);
    double res = strtod (str, end);

    if (loc != (locale_t)0)
    {
        uselocale (oldloc);
        freelocale (loc);
    }
    return res;
}


/**
 * us_strtof() has the same prototype as ANSI C strtof() but it uses the
 * POSIX/C decimal format, regardless of the current numeric locale.
 */
float us_strtof( const char *str, char **end )
{
    locale_t loc = newlocale (LC_NUMERIC_MASK, "C", NULL);
    locale_t oldloc = uselocale (loc);
    float res = strtof (str, end);

    if (loc != (locale_t)0)
    {
        uselocale (oldloc);
        freelocale (loc);
    }
    return res;
}


/**
 * us_atof() has the same prototype as ANSI C atof() but it expects a dot
 * as decimal separator, regardless of the system locale.
 */
double us_atof( const char *str )
{
    return us_strtod( str, NULL );
}


/**
 * us_vasprintf() has the same prototype as vasprintf(), but doesn't use
 * the system locale.
 */
int us_vasprintf( char **ret, const char *format, va_list ap )
{
    locale_t loc = newlocale( LC_NUMERIC_MASK, "C", NULL );
    locale_t oldloc = uselocale( loc );

    int i_rc = vasprintf( ret, format, ap );

    if ( loc != (locale_t)0 )
    {
        uselocale( oldloc );
        freelocale( loc );
    }

    return i_rc;
}


/**
 * us_asprintf() has the same prototype as asprintf(), but doesn't use
 * the system locale.
 */
int us_asprintf( char **ret, const char *format, ... )
{
    va_list ap;
    int i_rc;

    va_start( ap, format );
    i_rc = us_vasprintf( ret, format, ap );
    va_end( ap );

    return i_rc;
}
