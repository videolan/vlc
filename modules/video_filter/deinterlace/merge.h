/*****************************************************************************
 * merge.h : Merge (line blending) routines for the VLC deinterlacer
 *****************************************************************************
 * Copyright (C) 2011 VLC authors and VideoLAN
 *
 * Author: Sam Hocevar <sam@zoy.org>                      (generic C routine)
 *         Sigmund Augdal Helberg <sigmunau@videolan.org> (MMXEXT, 3DNow, SSE2)
 *         Eric Petit <eric.petit@lapsus.org>             (Altivec)
 *         Rémi Denis-Courmont                            (ARM NEON)
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

/**
 * Average two vectors.
 *
 * This callback shall compute the element-wise rounded average of two vectors.
 * This is used for blending scan lines of two fields for deinterlacing.
 *
 * The size of element is specified by the context,
 * namely \see deinterlace_functions.
 * Currently 8-bit and 16-bit elements are supported.
 *
 * \param d Output vector
 * \param s1 First source vector
 * \param s2 Second source vector
 * \param len size of vectors in bytes
 */

typedef void (*merge_cb)(void *d, const void *s1, const void *s2, size_t len);

/**
 * Deinterlacing optimisation callbacks.
 */
struct deinterlace_functions {
    /** Element-wise vector average
     *
     * The first array entries are indexed by the binary order of magnitude
     * of the element size in bytes: 0 for 8-bit, 1 for 16-bit. */
    merge_cb merges[2];
};

/*****************************************************************************
 * Macros
 *****************************************************************************/

/* Note that the Open() call of the deinterlace filter automatically selects
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
#define Merge p_sys->pf_merge

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

#endif
