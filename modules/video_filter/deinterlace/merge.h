/*****************************************************************************
 * merge.h : Merge (line blending) routines for the VLC deinterlacer
 *****************************************************************************
 * Copyright (C) 2011 VLC authors and VideoLAN
 * $Id$
 *
 * Author: Sam Hocevar <sam@zoy.org>                      (generic C routine)
 *         Sigmund Augdal Helberg <sigmunau@videolan.org> (MMXEXT, 3DNow, SSE2)
 *         Eric Petit <eric.petit@lapsus.org>             (Altivec)
 *         Rémi Denis-Courmont <remi@remlab.net>          (ARM NEON)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_DEINTERLACE_MERGE_H
#define VLC_DEINTERLACE_MERGE_H 1

/**
 * \file
 * Merge (line blending) routines for the VLC deinterlacer.
 */

/*****************************************************************************
 * Macros
 *****************************************************************************/

/* Convenient Merge() and EndMerge() macros to pick the most appropriate
   merge implementation automatically.

   Note that you'll need to include vlc_filter.h and deinterlace.h
   to use these.

 * Note that the Open() call of the deinterlace filter automatically selects
 * the most appropriate merge routine based on the CPU capabilities.
 * You can call the most appropriate version automatically, from a function
 * in the deinterlace filter, by using the Merge() macro.
 *
 * Note that the filter instance (p_filter) must be available for the Merge()
 * macro to work, because it needs the detection result from the filter's
 * Open().
 *
 * Macro syntax:
 *   Merge( _p_dest, _p_s1, _p_s2, i_bytes );
 *
  * i_bytes > 0; no other restrictions. This holds for all versions of the
 * merge routine.
 *
 */
#define Merge p_filter->p_sys->pf_merge

/*
 * EndMerge() macro, which must be called after the merge is
 * finished, if the Merge() macro was used to perform the merge.
 */
#if defined(__i386__) || defined(__x86_64__)
# define EndMerge() \
    if(p_filter->p_sys->pf_end_merge) (p_filter->p_sys->pf_end_merge)()
#else
# define EndMerge() (void)0
#endif

/*****************************************************************************
 * Merge routines
 *****************************************************************************/

/**
 * Generic routine to blend 8 bit pixels from two picture lines.
 * No inline assembler acceleration.
 *
 * @param _p_dest Target line. Blend result = (A + B)/2.
 * @param _p_s1 Source line A.
 * @param _p_s2 Source line B.
 * @param i_bytes Number of bytes to merge.
 * @see Open()
 */
void Merge8BitGeneric( void *_p_dest, const void *_p_s1, const void *_p_s2,
                       size_t i_bytes );

/**
 * Generic routine to blend 16 bit pixels from two picture lines.
 * No inline assembler acceleration.
 *
 * @param _p_dest Target line. Blend result = (A + B)/2.
 * @param _p_s1 Source line A.
 * @param _p_s2 Source line B.
 * @param i_bytes Number of *bytes* to merge.
 * @see Open()
 */
void Merge16BitGeneric( void *_p_dest, const void *_p_s1, const void *_p_s2,
                        size_t i_bytes );

#if defined(CAN_COMPILE_C_ALTIVEC)
/**
 * Altivec routine to blend pixels from two picture lines.
 *
 * @param _p_dest Target
 * @param _p_s1 Source line A
 * @param _p_s2 Source line B
 * @param i_bytes Number of bytes to merge
 */
void MergeAltivec ( void *, const void *, const void *, size_t );
#endif

#if defined(CAN_COMPILE_MMXEXT)
/**
 * MMXEXT routine to blend pixels from two picture lines.
 *
 * @param _p_dest Target
 * @param _p_s1 Source line A
 * @param _p_s2 Source line B
 * @param i_bytes Number of bytes to merge
 */
void MergeMMXEXT  ( void *, const void *, const void *, size_t );
#endif

#if defined(CAN_COMPILE_3DNOW)
/**
 * 3DNow routine to blend pixels from two picture lines.
 *
 * @param _p_dest Target
 * @param _p_s1 Source line A
 * @param _p_s2 Source line B
 * @param i_bytes Number of bytes to merge
 */
void Merge3DNow   ( void *, const void *, const void *, size_t );
#endif

#if defined(CAN_COMPILE_SSE)
/**
 * SSE2 routine to blend pixels from two picture lines.
 *
 * @param _p_dest Target
 * @param _p_s1 Source line A
 * @param _p_s2 Source line B
 * @param i_bytes Number of bytes to merge
 */
void Merge8BitSSE2( void *, const void *, const void *, size_t );
/**
 * SSE2 routine to blend pixels from two picture lines.
 *
 * @param _p_dest Target
 * @param _p_s1 Source line A
 * @param _p_s2 Source line B
 * @param i_bytes Number of bytes to merge
 */
void Merge16BitSSE2( void *, const void *, const void *, size_t );
#endif

#if defined(CAN_COMPILE_ARM)
/**
 * ARM NEON routine to blend pixels from two picture lines.
 */
void merge8_arm_neon (void *, const void *, const void *, size_t);
void merge16_arm_neon (void *, const void *, const void *, size_t);

/**
 * ARMv6 SIMD routine to blend pixels from two picture lines.
 */
void merge8_armv6 (void *, const void *, const void *, size_t);
void merge16_armv6 (void *, const void *, const void *, size_t);
#endif

/*****************************************************************************
 * EndMerge routines
 *****************************************************************************/

#if defined(CAN_COMPILE_MMXEXT) || defined(CAN_COMPILE_SSE)
/**
 * MMX merge finalization routine.
 *
 * Must be called after an MMX merge is finished.
 * This exits MMX mode (by executing the "emms" instruction).
 *
 * The EndMerge() macro detects whether this is needed, and calls if it is,
 * so just use that.
 */
void EndMMX       ( void );
#endif

#if defined(CAN_COMPILE_3DNOW)
/**
 * 3DNow merge finalization routine.
 *
 * Must be called after a 3DNow merge is finished.
 * This exits 3DNow mode (by executing the "femms" instruction).
 *
 * The EndMerge() macro detects whether this is needed, and calls if it is,
 * so just use that.
 */
void End3DNow     ( void );
#endif

#endif
