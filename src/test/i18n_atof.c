/*****************************************************************************
 * i18n_atof.c: Test for us_atof
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

    assert (us_atof("0") == 0.);
    assert (us_atof("1") == 1.);
    assert (us_atof("1.") == 1.);
    assert (us_atof("1,") == 1.);
    assert (us_atof("1#") == 1.);
    assert (us_atof(dot9) == 999999.999999);
    assert (us_atof(comma9) == 999999.);
    assert (us_atof(sharp9) == 999999.);
    assert (us_atof("invalid") == 0.);

    assert ((us_strtod(dot9, &end ) == 999999.999999)
            && (*end == '\0'));
    assert ((us_strtod(comma9, &end ) == 999999.)
            && (*end == ','));
    assert ((us_strtod(sharp9, &end ) == 999999.)
            && (*end == '#'));

    return 0;
}
