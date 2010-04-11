/*****************************************************************************
 * override.c: overriden function calls for VLC media player
 *****************************************************************************
 * Copyright (C) 2010 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdbool.h>

void vlc_enable_override (void);

static bool override = false;

void vlc_enable_override (void)
{
    override = true;
}

#if defined (__GNUC__) /* typeof and statement-expression */ \
 && (defined (__ELF__) && !defined (__sun__))
/* Solaris crashes on printf("%s", NULL); which is legal, but annoying. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

static void vlogbug (const char *level, const char *func, const char *fmt,
                     va_list ap)
{
    flockfile (stderr);
    fprintf (stderr, "%s: call to %s(", level, func);
    vfprintf (stderr, fmt, ap);
    fputs (")\n", stderr);
    funlockfile (stderr);
}

static void logbug (const char *level, const char *func, const char *fmt, ...)
{
    va_list ap;

    va_start (ap, fmt);
    vlogbug (level, func, fmt, ap);
    va_end (ap);
}

static void *getsym (const char *name)
{
    void *sym = dlsym (RTLD_NEXT, name);
    if (sym == NULL)
    {
        fprintf (stderr, "Cannot resolve symbol %s!\n", name);
        abort ();
    }
    return sym;
}

#define LOG(level, ...) logbug(level, __func__, __VA_ARGS__)
#define CALL(func, ...) \
    ({ typeof (func) *sym = getsym ( # func); sym (__VA_ARGS__); })


/*** Environment ***
 *
 * "Conforming multi-threaded applications shall not use the environ variable
 *  to access or modify any environment variable while any other thread is
 *  concurrently modifying any environment variable." -- POSIX.
 *
 * Some evil libraries modify the environment. We currently ignore the calls as
 * they could crash the process. This may cause funny behaviour though. */
int putenv (char *str)
{
    if (override)
    {
        LOG("Blocked", "\"%s\"", str);
        return 0;
    }
    return CALL(putenv, str);
}

int setenv (const char *name, const char *value, int overwrite)
{
    if (override)
    {
        LOG("Blocked", "\"%s\", \"%s\", %d", name, value, overwrite);
        return 0;
    }
    return CALL(setenv, name, value, overwrite);
}

int unsetenv (const char *name)
{
    if (override)
    {
        LOG("Blocked", "\"%s\"", name);
        return 0;
    }
    return CALL(unsetenv, name);
}


#endif /* __ELF__ */
