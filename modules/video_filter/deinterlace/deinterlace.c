/*****************************************************************************
 * deinterlace.c : deinterlacer plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2011 VLC authors and VideoLAN
 * $Id$
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <stdint.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_cpu.h>
#include <vlc_mouse.h>

#include "deinterlace.h"
#include "helpers.h"
#include "merge.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define MODE_TEXT N_("Deinterlace mode")

#define SOUT_MODE_TEXT N_("Streaming deinterlace mode")
#define SOUT_MODE_LONGTEXT N_("Deinterlace method to use for streaming.")

#define FILTER_CFG_PREFIX "sout-deinterlace-"

/* Tooltips drop linefeeds (at least in the Qt GUI);
   thus the space before each set of consecutive \n.

   See phosphor.h for phosphor_chroma_list and phosphor_dimmer_list.
*/
#define PHOSPHOR_CHROMA_TEXT N_("Phosphor chroma mode for 4:2:0 input")
#define PHOSPHOR_CHROMA_LONGTEXT N_("Choose handling for colours in those "\
                                    "output frames that fall across input "\
                                    "frame boundaries. \n"\
                                    "\n"\
                                    "Latest: take chroma from new (bright) "\
                                    "field only. Good for interlaced input, "\
                                    "such as videos from a camcorder. \n"\
                                    "\n"\
                                    "AltLine: take chroma line 1 from top "\
                                    "field, line 2 from bottom field, etc. \n"\
                                    "Default, good for NTSC telecined input "\
                                    "(anime DVDs, etc.). \n"\
                                    "\n"\
                                    "Blend: average input field chromas. "\
                                    "May distort the colours of the new "\
                                    "(bright) field, too. \n"\
                                    "\n"\
                                    "Upconvert: output in 4:2:2 format "\
                                    "(independent chroma for each field). "\
                                    "Best simulation, but requires more CPU "\
                                    "and memory bandwidth.")

#define PHOSPHOR_DIMMER_TEXT N_("Phosphor old field dimmer strength")
#define PHOSPHOR_DIMMER_LONGTEXT N_("This controls the strength of the "\
                                    "darkening filter that simulates CRT TV "\
                                    "phosphor light decay for the old field "\
                                    "in the Phosphor framerate doubler. "\
                                    "Default: Low.")

vlc_module_begin ()
    set_description( N_("Deinterlacing video filter") )
    set_shortname( N_("Deinterlace" ))
    set_capability( "video filter2", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    add_string( FILTER_CFG_PREFIX "mode", "blend", SOUT_MODE_TEXT,
                SOUT_MODE_LONGTEXT, false )
        change_string_list( mode_list, mode_list_text )
        change_safe ()
    add_integer( FILTER_CFG_PREFIX "phosphor-chroma", 2, PHOSPHOR_CHROMA_TEXT,
                PHOSPHOR_CHROMA_LONGTEXT, true )
        change_integer_list( phosphor_chroma_list, phosphor_chroma_list_text )
        change_safe ()
    add_integer( FILTER_CFG_PREFIX "phosphor-dimmer", 2, PHOSPHOR_DIMMER_TEXT,
                PHOSPHOR_DIMMER_LONGTEXT, true )
        change_integer_list( phosphor_dimmer_list, phosphor_dimmer_list_text )
        change_safe ()
    add_shortcut( "deinterlace" )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local data
 *****************************************************************************/

/**
 * Available config options for the deinterlacer module.
 *
 * Note that also algorithm-specific options must be listed here,
 * and reading logic for them implemented in Open().
 */
static const char *const ppsz_filter_options[] = {
    "mode", "phosphor-chroma", "phosphor-dimmer",
    NULL
};

/*****************************************************************************
 * SetFilterMethod: setup the deinterlace method to use.
 *****************************************************************************/

/**
 * Setup the deinterlace method to use.
 *
 * FIXME: extract i_chroma from p_filter automatically?
 *
 * @param p_filter The filter instance.
 * @param mode Desired method. See mode_list for available choices.
 * @see mode_list
 */
static void SetFilterMethod( filter_t *p_filter, const char *mode, bool pack )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if( mode == NULL )
        mode = "blend";

    p_sys->i_mode = DEINTERLACE_BLEND; /* default */
    p_sys->b_double_rate = false;
    p_sys->b_half_height = false;
    p_sys->b_use_frame_history = false;

    if( !strcmp( mode, "discard" ) )
    {
        p_sys->i_mode = DEINTERLACE_DISCARD;
        p_sys->b_half_height = true;
    }
    else if( !strcmp( mode, "bob" ) || !strcmp( mode, "progressive-scan" ) )
    {
        p_sys->i_mode = DEINTERLACE_BOB;
        p_sys->b_double_rate = true;
    }
    else if( !strcmp( mode, "linear" ) )
    {
        p_sys->i_mode = DEINTERLACE_LINEAR;
        p_sys->b_double_rate = true;
    }
    else if( !strcmp( mode, "mean" ) )
    {
        p_sys->i_mode = DEINTERLACE_MEAN;
        p_sys->b_half_height = true;
    }
    else if( !strcmp( mode, "blend" ) )
    {
    }
    else if( pack )
    {
        msg_Err( p_filter, "unknown or incompatible deinterlace mode \"%s\""
                 " for packed format", mode );
        mode = "blend";
    }
    else if( !strcmp( mode, "yadif" ) )
    {
        p_sys->i_mode = DEINTERLACE_YADIF;
        p_sys->b_use_frame_history = true;
    }
    else if( !strcmp( mode, "yadif2x" ) )
    {
        p_sys->i_mode = DEINTERLACE_YADIF2X;
        p_sys->b_double_rate = true;
        p_sys->b_use_frame_history = true;
    }
    else if( p_sys->chroma->pixel_size > 1 )
    {
        msg_Err( p_filter, "unknown or incompatible deinterlace mode \"%s\""
                 " for high depth format", mode );
        mode = "blend";
    }
    else if( !strcmp( mode, "x" ) )
    {
        p_sys->i_mode = DEINTERLACE_X;
    }
    else if( !strcmp( mode, "phosphor" ) )
    {
        p_sys->i_mode = DEINTERLACE_PHOSPHOR;
        p_sys->b_double_rate = true;
        p_sys->b_use_frame_history = true;
    }
    else if( !strcmp( mode, "ivtc" ) )
    {
        p_sys->i_mode = DEINTERLACE_IVTC;
        p_sys->b_use_frame_history = true;
    }
    else
        msg_Err( p_filter, "unknown deinterlace mode \"%s\"", mode );

    msg_Dbg( p_filter, "using %s deinterlace method", mode );
}

/**
 * Get the output video format of the chosen deinterlace method
 * for the given input video format.
 *
 * Note that each algorithm is allowed to specify its output format,
 * which may (for some input formats) differ from the input format.
 *
 * @param p_filter The filter instance.
 * @param[out] p_dst Output video format. The structure must be allocated by ca
 * @param[in] p_src Input video format.
 * @see SetFilterMethod()
 */
static void GetOutputFormat( filter_t *p_filter,
                      video_format_t *p_dst, const video_format_t *p_src )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    *p_dst = *p_src;

    if( p_sys->b_half_height )
    {
        p_dst->i_height /= 2;
        p_dst->i_visible_height /= 2;
        p_dst->i_y_offset /= 2;
        p_dst->i_sar_den *= 2;
    }

    if( p_sys->b_double_rate )
    {
        p_dst->i_frame_rate *= 2;
    }

    if( p_sys->i_mode == DEINTERLACE_PHOSPHOR  &&
        2 * p_sys->chroma->p[1].h.num == p_sys->chroma->p[1].h.den &&
        2 * p_sys->chroma->p[2].h.num == p_sys->chroma->p[2].h.den &&
        p_sys->phosphor.i_chroma_for_420 == PC_UPCONVERT )
    {
        p_dst->i_chroma = p_src->i_chroma == VLC_CODEC_J420 ? VLC_CODEC_J422 :
                                                              VLC_CODEC_I422;
    }
    else
    {
        p_dst->i_chroma = p_src->i_chroma;
    }

}

/*****************************************************************************
 * video filter2 functions
 *****************************************************************************/

#define DEINTERLACE_DST_SIZE 3

/* This is the filter function. See Open(). */
picture_t *Deinterlace( filter_t *p_filter, picture_t *p_pic )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    picture_t *p_dst[DEINTERLACE_DST_SIZE];

    /* Request output picture */
    p_dst[0] = filter_NewPicture( p_filter );
    if( p_dst[0] == NULL )
    {
        picture_Release( p_pic );
        return NULL;
    }
    picture_CopyProperties( p_dst[0], p_pic );

    /* Any unused p_dst pointers must be NULL, because they are used to
       check how many output frames we have. */
    for( int i = 1; i < DEINTERLACE_DST_SIZE; ++i )
        p_dst[i] = NULL;

    /* Update the input frame history, if the currently active algorithm
       needs it. */
    if( p_sys->b_use_frame_history )
    {
        /* Duplicate the picture
         * TODO when the vout rework is finished, picture_Hold() might be enough
         * but becarefull, the pitches must match */
        picture_t *p_dup = picture_NewFromFormat( &p_pic->format );
        if( p_dup )
            picture_Copy( p_dup, p_pic );

        /* Slide the history */
        if( p_sys->pp_history[0] )
            picture_Release( p_sys->pp_history[0] );
        for( int i = 1; i < HISTORY_SIZE; i++ )
            p_sys->pp_history[i-1] = p_sys->pp_history[i];
        p_sys->pp_history[HISTORY_SIZE-1] = p_dup;
    }

    /* Slide the metadata history. */
    for( int i = 1; i < METADATA_SIZE; i++ )
    {
        p_sys->meta.pi_date[i-1]            = p_sys->meta.pi_date[i];
        p_sys->meta.pi_nb_fields[i-1]       = p_sys->meta.pi_nb_fields[i];
        p_sys->meta.pb_top_field_first[i-1] = p_sys->meta.pb_top_field_first[i];
    }
    /* The last element corresponds to the current input frame. */
    p_sys->meta.pi_date[METADATA_SIZE-1]            = p_pic->date;
    p_sys->meta.pi_nb_fields[METADATA_SIZE-1]       = p_pic->i_nb_fields;
    p_sys->meta.pb_top_field_first[METADATA_SIZE-1] = p_pic->b_top_field_first;

    /* Remember the frame offset that we should use for this frame.
       The value in p_sys will be updated to reflect the correct value
       for the *next* frame when we call the renderer. */
    int i_frame_offset = p_sys->i_frame_offset;
    int i_meta_idx     = (METADATA_SIZE-1) - i_frame_offset;

    /* These correspond to the current *outgoing* frame. */
    bool b_top_field_first;
    int i_nb_fields;
    if( i_frame_offset != CUSTOM_PTS )
    {
        /* Pick the correct values from the history. */
        b_top_field_first = p_sys->meta.pb_top_field_first[i_meta_idx];
        i_nb_fields       = p_sys->meta.pi_nb_fields[i_meta_idx];
    }
    else
    {
        /* Framerate doublers must not request CUSTOM_PTS, as they need the
           original field timings, and need Deinterlace() to allocate the
           correct number of output frames. */
        assert( !p_sys->b_double_rate );

        /* NOTE: i_nb_fields is only used for framerate doublers, so it is
                 unused in this case. b_top_field_first is only passed to the
                 algorithm. We assume that algorithms that request CUSTOM_PTS
                 will, if necessary, extract the TFF/BFF information themselves.
        */
        b_top_field_first = p_pic->b_top_field_first; /* this is not guaranteed
                                                         to be meaningful */
        i_nb_fields       = p_pic->i_nb_fields;       /* unused */
    }

    /* For framerate doublers, determine field duration and allocate
       output frames. */
    mtime_t i_field_dur = 0;
    int i_double_rate_alloc_end = 0; /* One past last for allocated output
                                        frames in p_dst[]. Used only for
                                        framerate doublers. Will be inited
                                        below. Declared here because the
                                        PTS logic needs the result. */
    if( p_sys->b_double_rate )
    {
        /* Calculate one field duration. */
        int i = 0;
        int iend = METADATA_SIZE-1;
        /* Find oldest valid logged date.
           The current input frame doesn't count. */
        for( ; i < iend; i++ )
            if( p_sys->meta.pi_date[i] > VLC_TS_INVALID )
                break;
        if( i < iend )
        {
            /* Count how many fields the valid history entries
               (except the new frame) represent. */
            int i_fields_total = 0;
            for( int j = i ; j < iend; j++ )
                i_fields_total += p_sys->meta.pi_nb_fields[j];
            /* One field took this long. */
            i_field_dur = (p_pic->date - p_sys->meta.pi_date[i]) / i_fields_total;
        }
        /* Note that we default to field duration 0 if it could not be
           determined. This behaves the same as the old code - leaving the
           extra output frame dates the same as p_pic->date if the last cached
           date was not valid.
        */

        i_double_rate_alloc_end = i_nb_fields;
        if( i_nb_fields > DEINTERLACE_DST_SIZE )
        {
            /* Note that the effective buffer size depends also on the constant
               private_picture in vout_wrapper.c, since that determines the
               maximum number of output pictures filter_NewPicture() will
               successfully allocate for one input frame.
            */
            msg_Err( p_filter, "Framerate doubler: output buffer too small; "\
                               "fields = %d, buffer size = %d. Dropping the "\
                               "remaining fields.",
                               i_nb_fields, DEINTERLACE_DST_SIZE );
            i_double_rate_alloc_end = DEINTERLACE_DST_SIZE;
        }

        /* Allocate output frames. */
        for( int i = 1; i < i_double_rate_alloc_end ; ++i )
        {
            p_dst[i-1]->p_next =
            p_dst[i]           = filter_NewPicture( p_filter );
            if( p_dst[i] )
            {
                picture_CopyProperties( p_dst[i], p_pic );
            }
            else
            {
                msg_Err( p_filter, "Framerate doubler: could not allocate "\
                                   "output frame %d", i+1 );
                i_double_rate_alloc_end = i; /* Inform the PTS logic about the
                                                correct end position. */
                break; /* If this happens, the rest of the allocations
                          aren't likely to work, either... */
            }
        }
        /* Now we have allocated *up to* the correct number of frames;
           normally, exactly the correct number. Upon alloc failure,
           we may have succeeded in allocating *some* output frames,
           but fewer than were desired. In such a case, as many will
           be rendered as were successfully allocated.

           Note that now p_dst[i] != NULL
           for 0 <= i < i_double_rate_alloc_end. */
    }
    assert( p_sys->b_double_rate  ||  p_dst[1] == NULL );
    assert( i_nb_fields > 2  ||  p_dst[2] == NULL );

    /* Render */
    switch( p_sys->i_mode )
    {
        case DEINTERLACE_DISCARD:
            RenderDiscard( p_dst[0], p_pic, 0 );
            break;

        case DEINTERLACE_BOB:
            RenderBob( p_dst[0], p_pic, !b_top_field_first );
            if( p_dst[1] )
                RenderBob( p_dst[1], p_pic, b_top_field_first );
            if( p_dst[2] )
                RenderBob( p_dst[2], p_pic, !b_top_field_first );
            break;;

        case DEINTERLACE_LINEAR:
            RenderLinear( p_filter, p_dst[0], p_pic, !b_top_field_first );
            if( p_dst[1] )
                RenderLinear( p_filter, p_dst[1], p_pic, b_top_field_first );
            if( p_dst[2] )
                RenderLinear( p_filter, p_dst[2], p_pic, !b_top_field_first );
            break;

        case DEINTERLACE_MEAN:
            RenderMean( p_filter, p_dst[0], p_pic );
            break;

        case DEINTERLACE_BLEND:
            RenderBlend( p_filter, p_dst[0], p_pic );
            break;

        case DEINTERLACE_X:
            RenderX( p_dst[0], p_pic );
            break;

        case DEINTERLACE_YADIF:
            if( RenderYadif( p_filter, p_dst[0], p_pic, 0, 0 ) )
                goto drop;
            break;

        case DEINTERLACE_YADIF2X:
            if( RenderYadif( p_filter, p_dst[0], p_pic, 0, !b_top_field_first ) )
                goto drop;
            if( p_dst[1] )
                RenderYadif( p_filter, p_dst[1], p_pic, 1, b_top_field_first );
            if( p_dst[2] )
                RenderYadif( p_filter, p_dst[2], p_pic, 2, !b_top_field_first );
            break;

        case DEINTERLACE_PHOSPHOR:
            if( RenderPhosphor( p_filter, p_dst[0], 0,
                                !b_top_field_first ) )
                goto drop;
            if( p_dst[1] )
                RenderPhosphor( p_filter, p_dst[1], 1,
                                b_top_field_first );
            if( p_dst[2] )
                RenderPhosphor( p_filter, p_dst[2], 2,
                                !b_top_field_first );
            break;

        case DEINTERLACE_IVTC:
            /* Note: RenderIVTC will automatically drop the duplicate frames
                     produced by IVTC. This is part of normal operation. */
            if( RenderIVTC( p_filter, p_dst[0] ) )
                goto drop;
            break;
    }

    /* Set output timestamps, if the algorithm didn't request CUSTOM_PTS
       for this frame. */
    assert( i_frame_offset <= METADATA_SIZE  ||  i_frame_offset == CUSTOM_PTS );
    if( i_frame_offset != CUSTOM_PTS )
    {
        mtime_t i_base_pts = p_sys->meta.pi_date[i_meta_idx];

        /* Note: in the usual case (i_frame_offset = 0  and
                 b_double_rate = false), this effectively does nothing.
                 This is needed to correct the timestamp
                 when i_frame_offset > 0. */
        p_dst[0]->date = i_base_pts;

        if( p_sys->b_double_rate )
        {
            /* Processing all actually allocated output frames. */
            for( int i = 1; i < i_double_rate_alloc_end; ++i )
            {
                /* XXX it's not really good especially for the first picture, but
                 * I don't think that delaying by one frame is worth it */
                if( i_base_pts > VLC_TS_INVALID )
                    p_dst[i]->date = i_base_pts + i * i_field_dur;
                else
                    p_dst[i]->date = VLC_TS_INVALID;
            }
        }
    }

    for( int i = 0; i < DEINTERLACE_DST_SIZE; ++i )
    {
        if( p_dst[i] )
        {
            p_dst[i]->b_progressive = true;
            p_dst[i]->i_nb_fields = 2;
        }
    }

    picture_Release( p_pic );
    return p_dst[0];

drop:
    picture_Release( p_dst[0] );
    for( int i = 1; i < DEINTERLACE_DST_SIZE; ++i )
    {
        if( p_dst[i] )
            picture_Release( p_dst[i] );
    }
    picture_Release( p_pic );
    return NULL;
}

/*****************************************************************************
 * Flush
 *****************************************************************************/

void Flush( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    for( int i = 0; i < METADATA_SIZE; i++ )
    {
        p_sys->meta.pi_date[i] = VLC_TS_INVALID;
        p_sys->meta.pi_nb_fields[i] = 2;
        p_sys->meta.pb_top_field_first[i] = true;
    }
    p_sys->i_frame_offset = 0; /* reset to default value (first frame after
                                  flush cannot have offset) */
    for( int i = 0; i < HISTORY_SIZE; i++ )
    {
        if( p_sys->pp_history[i] )
            picture_Release( p_sys->pp_history[i] );
        p_sys->pp_history[i] = NULL;
    }
    IVTCClearState( p_filter );
}

/*****************************************************************************
 * Mouse event callback
 *****************************************************************************/

int Mouse( filter_t *p_filter,
           vlc_mouse_t *p_mouse,
           const vlc_mouse_t *p_old, const vlc_mouse_t *p_new )
{
    VLC_UNUSED(p_old);
    *p_mouse = *p_new;
    if( p_filter->p_sys->b_half_height )
        p_mouse->i_y *= 2;
    return VLC_SUCCESS;
}


/*****************************************************************************
 * Open
 *****************************************************************************/

int Open( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys;

    const vlc_fourcc_t fourcc = p_filter->fmt_in.video.i_chroma;
    const vlc_chroma_description_t *chroma = vlc_fourcc_GetChromaDescription( fourcc );
    if( chroma == NULL || chroma->pixel_size > 2 )
    {
notsupp:
        msg_Err( p_filter, "unsupported chroma %4.4s", (char*)&fourcc );
        return VLC_EGENERIC;
    }

    unsigned pixel_size = chroma->pixel_size;
    bool packed = false;
    if( chroma->plane_count != 3 )
    {
        packed = true;
        switch( fourcc )
        {
            case VLC_CODEC_YUYV:
            case VLC_CODEC_UYVY:
            case VLC_CODEC_YVYU:
            case VLC_CODEC_VYUY:
            case VLC_CODEC_NV12:
            case VLC_CODEC_NV21:
                pixel_size = 1;
                break;
            default:
                goto notsupp;
        }
    }
    assert( vlc_fourcc_IsYUV( fourcc ) );

    /* */
    p_sys = p_filter->p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->chroma = chroma;

    config_ChainParse( p_filter, FILTER_CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );
    char *psz_mode = var_InheritString( p_filter, FILTER_CFG_PREFIX "mode" );
    SetFilterMethod( p_filter, psz_mode, packed );
    free( psz_mode );

    for( int i = 0; i < METADATA_SIZE; i++ )
    {
        p_sys->meta.pi_date[i] = VLC_TS_INVALID;
        p_sys->meta.pi_nb_fields[i] = 2;
        p_sys->meta.pb_top_field_first[i] = true;
    }
    p_sys->i_frame_offset = 0; /* start with default value (first-ever frame
                                  cannot have offset) */
    for( int i = 0; i < HISTORY_SIZE; i++ )
        p_sys->pp_history[i] = NULL;

    IVTCClearState( p_filter );

#if defined(CAN_COMPILE_C_ALTIVEC)
    if( pixel_size == 1 && vlc_CPU_ALTIVEC() )
        p_sys->pf_merge = MergeAltivec;
    else
#endif
#if defined(CAN_COMPILE_SSE2)
    if( vlc_CPU_SSE2() )
    {
        p_sys->pf_merge = pixel_size == 1 ? Merge8BitSSE2 : Merge16BitSSE2;
        p_sys->pf_end_merge = EndMMX;
    }
    else
#endif
#if defined(CAN_COMPILE_MMXEXT)
    if( pixel_size == 1 && vlc_CPU_MMXEXT() )
    {
        p_sys->pf_merge = MergeMMXEXT;
        p_sys->pf_end_merge = EndMMX;
    }
    else
#endif
#if defined(CAN_COMPILE_3DNOW)
    if( pixel_size == 1 && vlc_CPU_3dNOW() )
    {
        p_sys->pf_merge = Merge3DNow;
        p_sys->pf_end_merge = End3DNow;
    }
    else
#endif
#if defined(CAN_COMPILE_ARM)
    if( vlc_CPU_ARM_NEON() )
        p_sys->pf_merge = pixel_size == 1 ? merge8_arm_neon : merge16_arm_neon;
    else
    if( vlc_CPU_ARMv6() )
        p_sys->pf_merge = pixel_size == 1 ? merge8_armv6 : merge16_armv6;
    else
#endif
    {
        p_sys->pf_merge = pixel_size == 1 ? Merge8BitGeneric : Merge16BitGeneric;
#if defined(__i386__) || defined(__x86_64__)
        p_sys->pf_end_merge = NULL;
#endif
    }

    /* */
    if( p_sys->i_mode == DEINTERLACE_PHOSPHOR )
    {
        int i_c420 = var_GetInteger( p_filter,
                                     FILTER_CFG_PREFIX "phosphor-chroma" );
        if( i_c420 != PC_LATEST  &&  i_c420 != PC_ALTLINE  &&
            i_c420 != PC_BLEND   && i_c420 != PC_UPCONVERT )
        {
            msg_Dbg( p_filter, "Phosphor 4:2:0 input chroma mode not set"\
                               "or out of range (valid: 1, 2, 3 or 4), "\
                               "using default" );
            i_c420 = PC_ALTLINE;
        }
        msg_Dbg( p_filter, "using Phosphor 4:2:0 input chroma mode %d",
                           i_c420 );
        /* This maps directly to the phosphor_chroma_t enum. */
        p_sys->phosphor.i_chroma_for_420 = i_c420;

        int i_dimmer = var_GetInteger( p_filter,
                                       FILTER_CFG_PREFIX "phosphor-dimmer" );
        if( i_dimmer < 1  ||  i_dimmer > 4 )
        {
            msg_Dbg( p_filter, "Phosphor dimmer strength not set "\
                               "or out of range (valid: 1, 2, 3 or 4), "\
                               "using default" );
            i_dimmer = 2; /* low */
        }
        msg_Dbg( p_filter, "using Phosphor dimmer strength %d", i_dimmer );
        /* The internal value ranges from 0 to 3. */
        p_sys->phosphor.i_dimmer_strength = i_dimmer - 1;
    }
    else
    {
        p_sys->phosphor.i_chroma_for_420 = PC_ALTLINE;
        p_sys->phosphor.i_dimmer_strength = 1;
    }

    /* */
    video_format_t fmt;
    GetOutputFormat( p_filter, &fmt, &p_filter->fmt_in.video );
    if( !p_filter->b_allow_fmt_out_change &&
        ( fmt.i_chroma != p_filter->fmt_in.video.i_chroma ||
          fmt.i_height != p_filter->fmt_in.video.i_height ) )
    {
        Close( VLC_OBJECT(p_filter) );
        return VLC_EGENERIC;
    }
    p_filter->fmt_out.video = fmt;
    p_filter->fmt_out.i_codec = fmt.i_chroma;
    p_filter->pf_video_filter = Deinterlace;
    p_filter->pf_video_flush  = Flush;
    p_filter->pf_video_mouse  = Mouse;

    msg_Dbg( p_filter, "deinterlacing" );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: clean up the filter
 *****************************************************************************/

void Close( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;

    Flush( p_filter );
    free( p_filter->p_sys );
}
