/*****************************************************************************
 * deinterlace.h : deinterlacer plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2011 VLC authors and VideoLAN
 *
 * Author: Sam Hocevar <sam@zoy.org>
 *         Christophe Massiot <massiot@via.ecp.fr>
 *         Laurent Aimar <fenrir@videolan.org>
 *         Juha Jeronen <juha.jeronen@jyu.fi>
 *         ...and others
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

#ifndef VLC_DEINTERLACE_H
#define VLC_DEINTERLACE_H 1

/* Forward declarations */
struct filter_t;
struct picture_t;
struct vlc_object_t;

#include <vlc_common.h>
#include <vlc_mouse.h>

/* Local algorithm headers */
#include "algo_basic.h"
#include "algo_x.h"
#include "algo_yadif.h"
#include "algo_phosphor.h"
#include "algo_ivtc.h"
#include "common.h"

/*****************************************************************************
 * Local data
 *****************************************************************************/

/** Available deinterlace modes. */
static const char *const mode_list[] = {
    "discard", "blend", "mean", "bob", "linear", "x",
    "yadif", "yadif2x", "phosphor", "ivtc" };

/** User labels for the available deinterlace modes. */
static const char *const mode_list_text[] = {
    N_("Discard"), N_("Blend"), N_("Mean"), N_("Bob"), N_("Linear"), "X",
    "Yadif", "Yadif (2x)", N_("Phosphor"), N_("Film NTSC (IVTC)") };

/*****************************************************************************
 * Data structures
 *****************************************************************************/

/**
 * Top-level deinterlace subsystem state.
 */
typedef struct
{
    const vlc_chroma_description_t *chroma;

    /** Merge routine: C, MMX, SSE, ALTIVEC, NEON, ... */
    void (*pf_merge) ( void *, const void *, const void *, size_t );
#if defined (__i386__) || defined (__x86_64__)
    /** Merge finalization routine for SSE */
    void (*pf_end_merge) ( void );
#endif

    struct deinterlace_ctx   context;

    /* Algorithm-specific substructures */
    union {
        phosphor_sys_t phosphor; /**< Phosphor algorithm state. */
        ivtc_sys_t ivtc;         /**< IVTC algorithm state. */
    };
} filter_sys_t;

#endif
