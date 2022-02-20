/*****************************************************************************
 * getauxval.c: Linux getauxval() replacement
 *****************************************************************************
 * Copyright © 2022 Rémi Denis-Courmont
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

#include <stddef.h>

extern char **environ;

unsigned long getauxval(unsigned long type)
{
	const unsigned long *auxv;
	size_t i = 0;

	while (environ[i++] != NULL);

	auxv = (const void *)&environ[i];

	for (i = 0; auxv[i]; i += 2)
		if (auxv[i] == type)
			return auxv[i + 1];

	return 0;
}
