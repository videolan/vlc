/*****************************************************************************
 * tests.h: several test functions needed by the plugins
 *****************************************************************************
 * Copyright (C) 1996, 1997, 1998, 1999, 2000 VideoLAN
 *
 * Authors:
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
#define CPU_CAPABILITY_MMX     1<<0
#define CPU_CAPABILITY_3DNOW   1<<1
#define CPU_CAPABILITY_MMXEXT  1<<2

#define cpuid( a, eax, ebx, ecx, edx ) \
    asm volatile ( "pushl %%ebx      \n\
                    cpuid            \n\
                    popl %%ebx"        \
                 : "=a" ( eax ),       \
                   "=c" ( ecx ),       \
                   "=d" ( edx )        \
                 : "a"  ( a )          \
                 : "cc" );

/*****************************************************************************
 * TestVersion: tests if the given string equals the current version
 *****************************************************************************/
int TestVersion( char * psz_version );
int TestProgram( char * psz_program );
int TestMethod( char * psz_var, char * psz_method );

/*****************************************************************************
 * TestCPU: tests if the processor has MMX support and other capabilities
 *****************************************************************************
 * This function is called to check extensions the CPU may have.
 *****************************************************************************/
static __inline__ int TestCPU( void )
{
#ifndef __i386__
    return( CPU_CAPABILITY_NONE );
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
    {
        return( CPU_CAPABILITY_NONE );
    }

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
    {
        return( CPU_CAPABILITY_NONE );
    }

    /* the CPU supports the CPUID instruction - get its level */
    cpuid( 0, i_reg, i_dummy, i_dummy, i_dummy );

    if( !i_reg )
    {
        return( CPU_CAPABILITY_NONE );
    }

    /* test for the MMX flag */
    cpuid( 1, i_dummy, i_dummy, i_dummy, i_reg );

    if( ! (i_reg & 0x00800000) )
    {
        return( CPU_CAPABILITY_NONE );
    }

    return( CPU_CAPABILITY_MMX );
#endif
}

