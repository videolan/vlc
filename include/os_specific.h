/*****************************************************************************
 * os_specific.h: OS specific features
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: os_specific.h,v 1.3 2002/04/25 21:52:42 sam Exp $
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

#ifndef _NEED_OS_SPECIFIC_H
#   define _NEED_OS_SPECIFIC_H 1
#endif

#if defined( SYS_BEOS )
#   include "beos_specific.h"
#elif defined( SYS_DARWIN )
#   include "darwin_specific.h"
#elif defined( WIN32 )
#   include "win32_specific.h"
#else
#   undef _NEED_OS_SPECIFIC_H
#endif

#   ifdef __cplusplus
extern "C" {
#   endif

/*****************************************************************************
 * main_sys_t: system specific descriptor
 ****************************************************************************/
struct main_sys_s;

#ifndef __PLUGIN__
    extern struct main_sys_s *p_main_sys;
#else
#   define p_main_sys (p_symbols->p_main_sys)
#endif

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
#ifdef _NEED_OS_SPECIFIC_H
    void system_Init ( int *pi_argc, char *ppsz_argv[], char *ppsz_env[] );
    void system_Configure  ( void );
    void system_End  ( void );
#else
#   define system_Init(...) {}
#   define system_Configure(...) {}
#   define system_End(...) {}
#endif

#   ifdef __cplusplus
}
#   endif

