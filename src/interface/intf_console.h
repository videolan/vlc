/*****************************************************************************
 * intf_console.h: generic console methods for interface
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: intf_console.h,v 1.4 2001/03/21 13:42:34 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
p_intf_console_t  intf_ConsoleCreate    ( void );
void              intf_ConsoleDestroy   ( p_intf_console_t p_console );

void              intf_ConsoleClear     ( p_intf_console_t p_console );
void              intf_ConsolePrint     ( p_intf_console_t p_console, char *psz_str );
void              intf_ConsoleExec      ( p_intf_console_t p_console, char *psz_str );


