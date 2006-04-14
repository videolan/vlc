/*****************************************************************************
 * i18n: I18n tests
 *****************************************************************************
 * Copyright (C) 2006 Rémi Denis-Courmont
 * $Id: i18n_atof.c 14675 2006-03-08 12:25:29Z courmisch $
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

#include "../pyunit.h"
#include <vlc/vlc.h>
#include "charset.h"

PyObject *i18n_atof_test( PyObject *self, PyObject *args )
{
    const char dot9[] = "999999.999999";
    const char comma9[] = "999999,999999";
    const char sharp9[] = "999999#999999";
    char *end;

    ASSERT (i18n_atof("0") == 0.,"");
    ASSERT (i18n_atof("1") == 1.,"");
    ASSERT (i18n_atof("1.") == 1.,"");
    ASSERT (i18n_atof("1,") == 1.,"");
    ASSERT (i18n_atof("1#") == 1.,"");
    ASSERT (i18n_atof(dot9) == 999999.999999,"");
    ASSERT (i18n_atof(comma9) == 999999.999999,"");
    ASSERT (i18n_atof(sharp9) == 999999.,"");
    ASSERT (i18n_atof("invalid") == 0.,"");

    ASSERT (us_atof("0") == 0.,"");
    ASSERT (us_atof("1") == 1.,"");
    ASSERT (us_atof("1.") == 1.,"");
    ASSERT (us_atof("1,") == 1.,"");
    ASSERT (us_atof("1#") == 1.,"");
    ASSERT (us_atof(dot9) == 999999.999999,"");
    ASSERT (us_atof(comma9) == 999999.,"");
    ASSERT (us_atof(sharp9) == 999999.,"");
    ASSERT (us_atof("invalid") == 0.,"");
    ASSERT ((i18n_strtod(dot9, &end ) == 999999.999999)
           && (*end == '\0'),"");
    ASSERT ((i18n_strtod(comma9, &end ) == 999999.999999)
           && (*end == '\0'),"");
    ASSERT ((i18n_strtod(sharp9, &end ) == 999999.)
           && (*end == '#'),"");
    ASSERT ((us_strtod(dot9, &end ) == 999999.999999)
           && (*end == '\0'),"");
    ASSERT ((us_strtod(comma9, &end ) == 999999.)
           && (*end == ','),"");
    ASSERT ((us_strtod(sharp9, &end ) == 999999.)
           && (*end == '#'),"");

    Py_INCREF( Py_None);
    return Py_None;
}
