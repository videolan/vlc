/*****************************************************************************
 * i18n_atof.c: Test for i18n_atof
 *****************************************************************************
 * Copyright (C) 2006 Rémi Denis-Courmont
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

#include <vlc/vlc.h>
#include "charset.h"

#undef NDEBUG
#include <assert.h>

int main (void)
{
    assert (i18n_atof("0") == 0.);
    assert (i18n_atof("1") == 1.);
    assert (i18n_atof("1.") == 1.);
    assert (i18n_atof("1,") == 1.);
    assert (i18n_atof("1#") == 1.);
    assert (i18n_atof("999999.999999") == 999999.999999);
    assert (i18n_atof("999999,999999") == 999999.999999);
    assert (i18n_atof("999999#999999") == 999999.);

    assert (i18n_atof("invalid") == 0.);
    return 0;
}
