/*****************************************************************************
 * i18n_atof.c: Test for vlc_atof_c()
 *****************************************************************************
 * Copyright (C) 2006 Rémi Denis-Courmont
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
#include "vlc_charset.h"

#undef NDEBUG
#include <assert.h>

int main (void)
{
    const char dot9[] = "999999.999999";
    const char comma9[] = "999999,999999";
    const char sharp9[] = "999999#999999";
    char *end;

    assert(vlc_atof_c("0") == 0.);
    assert(vlc_atof_c("1") == 1.);
    assert(vlc_atof_c("1.") == 1.);
    assert(vlc_atof_c("1,") == 1.);
    assert(vlc_atof_c("1#") == 1.);
    assert(vlc_atof_c(dot9) == 999999.999999);
    assert(vlc_atof_c(comma9) == 999999.);
    assert(vlc_atof_c(sharp9) == 999999.);
    assert(vlc_atof_c("invalid") == 0.);

    assert((vlc_strtod_c(dot9, &end ) == 999999.999999) && (*end == '\0'));
    assert((vlc_strtod_c(comma9, &end ) == 999999.) && (*end == ','));
    assert((vlc_strtod_c(sharp9, &end ) == 999999.) && (*end == '#'));

    return 0;
}
