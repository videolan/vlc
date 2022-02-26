/*****************************************************************************
 * freebsd/cpu.c: CPU detection code for FreeBSD
 *****************************************************************************
 * Copyright (C) 2022 Rémi Denis-Courmont
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

#include <sys/types.h>
#include <sys/sysctl.h>

#include <vlc_common.h>
#include <vlc_cpu.h>

#if defined (__powerpc__) /* both 32- and 64-bit */
unsigned vlc_CPU_raw(void)
{
    unsigned int flags = 0;
    int opt;
    size_t optlen = sizeof (opt);

    if (sysctlbyname("hw.altivec", &opt, &optlen, NULL, 0) == 0 && opt != 0)
        flags |= VLC_CPU_ALTIVEC;

    return flags;
}
#endif
