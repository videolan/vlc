/*****************************************************************************
 * bytes_swap.h: fast routines to swap bytes order.
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: bytes_swap.h,v 1.1 2001/11/25 22:52:21 gbazin Exp $
 *
 * Authors: This code was borrowed from the GNU C Library.
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

/* Swap bytes in 16 bit value.  */
#define __bswap_constant_16(x) \
     ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8))

#if defined __GNUC__ && __GNUC__ >= 2
# define __bswap_16(x) \
     (__extension__							      \
      ({ register unsigned short int __v;				      \
	 if (__builtin_constant_p (x))					      \
	   __v = __bswap_constant_16 (x);				      \
	 else								      \
	   __asm__ __volatile__ ("rorw $8, %w0"				      \
				 : "=r" (__v)				      \
				 : "0" ((unsigned short int) (x))	      \
				 : "cc");				      \
	 __v; }))
#else
/* This is better than nothing.  */
# define __bswap_16(x) __bswap_constant_16 (x)
#endif


/* Swap bytes in 32 bit value.  */
#define __bswap_constant_32(x) \
     ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) |		      \
      (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))

#if defined __GNUC__ && __GNUC__ >= 2
/* To swap the bytes in a word the i486 processors and up provide the
   `bswap' opcode.  On i386 we have to use three instructions.  */
# if !defined __i486__ && !defined __pentium__ && !defined __pentiumpro__
#  define __bswap_32(x) \
     (__extension__							      \
      ({ register unsigned int __v;					      \
	 if (__builtin_constant_p (x))					      \
	   __v = __bswap_constant_32 (x);				      \
	 else								      \
	   __asm__ __volatile__ ("rorw $8, %w0;"			      \
				 "rorl $16, %0;"			      \
				 "rorw $8, %w0"				      \
				 : "=r" (__v)				      \
				 : "0" ((unsigned int) (x))		      \
				 : "cc");				      \
	 __v; }))
# else
#  define __bswap_32(x) \
     (__extension__							      \
      ({ register unsigned int __v;					      \
	 if (__builtin_constant_p (x))					      \
	   __v = __bswap_constant_32 (x);				      \
	 else								      \
	   __asm__ __volatile__ ("bswap %0"				      \
				 : "=r" (__v)				      \
				 : "0" ((unsigned int) (x)));		      \
	 __v; }))
# endif
#else
# define __bswap_32(x) __bswap_constant_32 (x)
#endif


#if defined __GNUC__ && __GNUC__ >= 2
/* Swap bytes in 64 bit value.  */
#define __bswap_constant_64(x) \
     ((((x) & 0xff00000000000000ull) >> 56)				      \
      | (((x) & 0x00ff000000000000ull) >> 40)				      \
      | (((x) & 0x0000ff0000000000ull) >> 24)				      \
      | (((x) & 0x000000ff00000000ull) >> 8)				      \
      | (((x) & 0x00000000ff000000ull) << 8)				      \
      | (((x) & 0x0000000000ff0000ull) << 24)				      \
      | (((x) & 0x000000000000ff00ull) << 40)				      \
      | (((x) & 0x00000000000000ffull) << 56))

# define __bswap_64(x) \
     (__extension__							      \
      ({ union { __extension__ unsigned long long int __ll;		      \
		 unsigned long int __l[2]; } __w, __r;			      \
         if (__builtin_constant_p (x))					      \
	   __r.__ll = __bswap_constant_64 (x);				      \
	 else								      \
	   {								      \
	     __w.__ll = (x);						      \
	     __r.__l[0] = __bswap_32 (__w.__l[1]);			      \
	     __r.__l[1] = __bswap_32 (__w.__l[0]);			      \
	   }								      \
	 __r.__ll; }))
#endif
