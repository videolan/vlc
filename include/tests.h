/*****************************************************************************
 * tests.h: several test functions needed by the plugins
 *****************************************************************************
 * Copyright (C) 1996, 1997, 1998, 1999, 2000 VideoLAN
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
int TestVersion( char * psz_version );
int TestProgram( char * psz_program );
int TestMethod( char * psz_var, char * psz_method );
int TestCPU( int i_capabilities );

/*****************************************************************************
 * CPUCapabilities: list the processors MMX support and other capabilities
 *****************************************************************************
 * This function is called to list extensions the CPU may have.
 *****************************************************************************/
static __inline__ int CPUCapabilities( void )
{
#ifdef SYS_BEOS
    return( CPU_CAPABILITY_NONE
            | CPU_CAPABILITY_486
            | CPU_CAPABILITY_586
            | CPU_CAPABILITY_MMX );
#else
    int           i_capabilities = CPU_CAPABILITY_NONE;
#ifdef __i386__
    unsigned int  i_eax, i_ebx, i_ecx, i_edx;
    boolean_t     b_amd;

#define cpuid( a )                 \
    asm volatile ( "cpuid"         \
                 : "=a" ( i_eax ), \
                   "=b" ( i_ebx ), \
                   "=c" ( i_ecx ), \
                   "=d" ( i_edx )  \
                 : "a"  ( a )      \
                 : "cc" );         \

    /* test for a 486 CPU */
    asm volatile ( "pushfl
                    popl %%eax
                    movl %%eax, %%ebx
                    xorl $0x200000, %%eax
                    pushl %%eax
                    popfl
                    pushfl
                    popl %%eax"
                 : "=a" ( i_eax ),
                   "=b" ( i_ebx )
                 :
                 : "cc" );

    if( i_eax == i_ebx )
    {
        return( i_capabilities );
    }

    i_capabilities |= CPU_CAPABILITY_486;

    /* the CPU supports the CPUID instruction - get its level */
    cpuid( 0x00000000 );

    if( !i_eax )
    {
        return( i_capabilities );
    }

    /* FIXME: this isn't correct, since some 486s have cpuid */
    i_capabilities |= CPU_CAPABILITY_586;

    /* borrowed from mpeg2dec */
    b_amd = ( i_ebx == 0x68747541 ) && ( i_ecx == 0x444d4163 )
                    && ( i_edx == 0x69746e65 );

    /* test for the MMX flag */
    cpuid( 0x00000001 );

    if( ! (i_edx & 0x00800000) )
    {
        return( i_capabilities );
    }

    i_capabilities |= CPU_CAPABILITY_MMX;

    if( i_edx & 0x02000000 )
    {
        i_capabilities |= CPU_CAPABILITY_MMXEXT;
    }
    
    /* test for additional capabilities */
    cpuid( 0x80000000 );

    if( i_eax < 0x80000001 )
    {
        return( i_capabilities );
    }

    /* list these additional capabilities */
    cpuid( 0x80000001 );

    if( i_edx & 0x80000000 )
    {
        i_capabilities |= CPU_CAPABILITY_3DNOW;
    }

    if( b_amd && ( i_edx & 0x00400000 ) )
    {
        i_capabilities |= CPU_CAPABILITY_MMXEXT;
    }
#endif /* __i386__ */

    return( i_capabilities );
#endif /* SYS_BEOS */
}

