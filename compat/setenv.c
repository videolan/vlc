/*****************************************************************************
 * setenv.c: POSIX setenv() & unsetenv() replacement
 *****************************************************************************
 * Copyright © 2011 Rémi Denis-Courmont
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
#include <string.h>

int setenv (const char *name, const char *value, int override)
{
#ifdef HAVE_GETENV
    if (override == 0 && getenv (name) != NULL)
        return 0;

    size_t namelen = strlen (name);
    size_t valuelen = strlen (value);
    char *var = malloc (namelen + valuelen + 2);

    if (var == NULL)
        return -1;

    sprintf (var, "%s=%s", name, value);
    /* This leaks memory. This is unavoidable. */
    return putenv (var);
#else
    return -1;
#endif
}

int unsetenv (const char *name)
{
    return setenv (name, "", 1);
}
