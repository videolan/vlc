/*****************************************************************************
 * tests.h: several test functions needed by the plugins
 *****************************************************************************
 * Copyright (C) 1996, 1997, 1998, 1999, 2000 VideoLAN
 * $Id: tests.h,v 1.9 2001/03/21 13:42:33 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

#define CPU_CAPABILITY_NONE    0
#define CPU_CAPABILITY_486     1<<0
#define CPU_CAPABILITY_586     1<<1
#define CPU_CAPABILITY_PPRO    1<<2
#define CPU_CAPABILITY_MMX     1<<3
#define CPU_CAPABILITY_3DNOW   1<<4
#define CPU_CAPABILITY_MMXEXT  1<<5
#define CPU_CAPABILITY_ALTIVEC 1<<16

/*****************************************************************************
 * TestVersion: tests if the given string equals the current version
 *****************************************************************************/
int TestVersion  ( char * psz_version );
int TestProgram  ( char * psz_program );
int TestMethod   ( char * psz_var, char * psz_method );
int TestCPU      ( int i_capabilities );

