/*****************************************************************************
 * flockfile.c: POSIX unlocked I/O stream stubs
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

#include <stdio.h>

/* There is no way to implement this for real. We just pretend it works and
 * hope for the best (especially when outputting to stderr). */

void flockfile (FILE *stream)
{
    (void) stream;
}

int ftrylockfile (FILE *stream)
{
    (void) stream;
    return 0;
}

void funlockfile (FILE *stream)
{
    (void) stream;
}

int getc_unlocked (FILE *stream)
{
    return getc (stream);
}

int getchar_unlocked (void)
{
    return getchar ();
}

int putc_unlocked (int c, FILE *stream)
{
    return putc (c, stream);
}

int putchar_unlocked (int c)
{
    return putchar (c);
}
