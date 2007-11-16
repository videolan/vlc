/*****************************************************************************
 * parser.c :  OSD import module
 *****************************************************************************
 * Copyright (C) 2007 M2X
 * $Id$
 *
 * Authors: Jean-Paul Saman
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

#ifndef _OSD_MENU_PARSER_H_
#define _OSD_MENU_PARSER_H_

static const char *ppsz_button_states[] = { "unselect", "select", "pressed" };

/* OSD Menu structure support routines for internal use by
 * OSD Menu configuration file parsers only.
 */
osd_menu_t   *osd_MenuNew( osd_menu_t *, const char *, int, int );
osd_button_t *osd_ButtonNew( const char *, int, int );
osd_state_t  *osd_StateNew( osd_menu_t *, const char *, const char * );

void osd_MenuFree  ( osd_menu_t * );
void osd_ButtonFree( osd_menu_t *, osd_button_t * );
void osd_StatesFree( osd_menu_t *, osd_state_t * );

#endif
