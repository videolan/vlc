/**
 * Copyright (C) 2001 Billy Biggs <vektor@dumbterm.net>,
 *                    Håkan Hjort <d95hjort@dtek.chalmers.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef NAV_PRINT_H_INCLUDED
#define NAV_PRINT_H_INCLUDED

#include <stdio.h>
#include <dvdread/nav_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This file provides example functions for printing information about the NAV
 * packet to stdout.
 */

void navPrint_PCI(pci_t *pci);
void navPrint_DSI(dsi_t *dsi);

#ifdef __cplusplus
};
#endif
#endif /* NAV_PRINT_H_INCLUDED */
