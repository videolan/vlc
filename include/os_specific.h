/*****************************************************************************
 * os_specific.h: OS specific features
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: os_specific.h,v 1.1 2002/04/02 23:43:57 gbazin Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
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

#ifdef SYS_BEOS
#   include "beos_specific.h"
#endif
#ifdef SYS_DARWIN
#   include "darwin_specific.h"
#endif
#ifdef WIN32
#   include "win32_specific.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * main_sys_t: system specific descriptor
 ****************************************************************************/
struct main_sys_s;

#ifndef PLUGIN
extern struct main_sys_s *p_main_sys;
#else
#   define p_main_sys (p_symbols->p_main_sys)
#endif


/*****************************************************************************
 * Prototypes
 *****************************************************************************/
void system_Init ( int *pi_argc, char *ppsz_argv[], char *ppsz_env[] );
void system_Configure  ( void );
void system_End  ( void );

#ifdef __cplusplus
}
#endif
