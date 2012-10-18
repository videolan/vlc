/*****************************************************************************
 * flockfile.c: POSIX unlocked I/O stream stubs
 *****************************************************************************
 * Copyright © 2011-2012 Rémi Denis-Courmont
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

#include <stdio.h>

#ifdef WIN32
# ifndef HAVE__LOCK_FILE
#  warning Broken SDK: VLC logs will be garbage.
#  define _lock_file(s) ((void)(s))
#  define _unlock_file(s) ((void)(s))
# endif

void flockfile (FILE *stream)
{
    _lock_file (stream);
}

int ftrylockfile (FILE *stream)
{
    flockfile (stream); /* Move along people, there is nothing to see here. */
    return 0;
}

void funlockfile (FILE *stream)
{
    _unlock_file (stream);
}

int getc_unlocked (FILE *stream)
{
    return _getc_nolock (stream);
}

int getchar_unlocked (void)
{
    return _getchar_nolock ();
}

int putc_unlocked (int c, FILE *stream)
{
    return _putc_nolock (c, stream);
}

int putchar_unlocked (int c)
{
    return _putchar_nolock (c);
}

#else
# error flockfile not implemented on your platform!
#endif
