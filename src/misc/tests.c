/*****************************************************************************
 * tests.c: several test functions needed by the plugins
 * Functions are prototyped in tests.h.
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 *
 * Authors: Samuel Hocevar <sam@via.ecp.fr>
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
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include "config.h"
#include "common.h"

#include "intf_msg.h"
#include "tests.h"

#include "main.h"

/*****************************************************************************
 * TestVersion: tests if the given string equals the current version
 *****************************************************************************/
int TestVersion( char * psz_version )
{
    return( !strcmp( psz_version, VERSION ) );
}

/*****************************************************************************
 * TestProgram: tests if the given string equals the program name
 *****************************************************************************/
int TestProgram( char * psz_program )
{
    return( !strcmp( psz_program, p_main->psz_arg0 ) );
}

/*****************************************************************************
 * TestMethod: tests if the given method was requested
 *****************************************************************************/
int TestMethod( char * psz_var, char * psz_method )
{
    return( !strcmp( psz_method, main_GetPszVariable( psz_var, "" ) ) );
}

/*****************************************************************************
 * TestMMX: tests if the processor has MMX support.
 *****************************************************************************
 * This function is called if HAVE_MMX is enabled, to check whether the
 * CPU really supports MMX.
 *****************************************************************************/
int TestMMX( void )
{
#ifndef __i386__
    return( 0 );
#else
/* FIXME: under beos, gcc does not support the following inline assembly */ 
#ifdef SYS_BEOS
    return( 1 );
#else

    int i_reg, i_dummy = 0;

    /* test for a 386 CPU */
    asm volatile ( "pushfl
                    popl %%eax
                    movl %%eax, %%ecx
                    xorl $0x40000, %%eax
                    pushl %%eax
                    popfl
                    pushfl
                    popl %%eax
                    xorl %%ecx, %%eax
                    andl $0x40000, %%eax"
                 : "=a" ( i_reg ) );

    if( !i_reg )
        return( 0 );

    /* test for a 486 CPU */
    asm volatile ( "movl %%ecx, %%eax
                    xorl $0x200000, %%eax
                    pushl %%eax
                    popfl
                    pushfl
                    popl %%eax
                    xorl %%ecx, %%eax
                    pushl %%ecx
                    popfl
                    andl $0x200000, %%eax"
                 : "=a" ( i_reg ) );

    if( !i_reg )
        return( 0 );

    /* the CPU supports the CPUID instruction - get its level */
    asm volatile ( "cpuid"
                 : "=a" ( i_reg ),
                   "=b" ( i_dummy ),
                   "=c" ( i_dummy ),
                   "=d" ( i_dummy )
                 : "a"  ( 0 ),       /* level 0 */
                   "b"  ( i_dummy ) ); /* buggy compiler shouldn't complain */

    /* this shouldn't happen on a normal CPU */
    if( !i_reg )
        return( 0 );

    /* test for the MMX flag */
    asm volatile ( "cpuid
                    andl $0x00800000, %%edx" /* X86_FEATURE_MMX */
                 : "=a" ( i_dummy ),
                   "=b" ( i_dummy ),
                   "=c" ( i_dummy ),
                   "=d" ( i_reg )
                 : "a"  ( 1 ),       /* level 1 */
                   "b"  ( i_dummy ) ); /* buggy compiler shouldn't complain */

    if( !i_reg )
        return( 0 );

    return( 1 );
#endif
#endif
}


