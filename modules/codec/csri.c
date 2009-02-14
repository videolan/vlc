/*****************************************************************************
 * csri.c : Load CSRI subtitle renderer
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: David Lamparter <equinox@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <string.h>
#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>
#include <vlc_codec.h>
#include <vlc_osd.h>
#include <vlc_input.h>

#include <csri/csri.h>
#include <csri/stream.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Create ( vlc_object_t * );
static void Destroy( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("Subtitles (advanced)"))
    set_description( N_("Wrapper for subtitle renderers using CSRI/asa") )
    set_capability( "decoder", 60 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_SCODEC )
    set_callbacks( Create, Destroy )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static subpicture_t *DecodeBlock( decoder_t *, block_t ** );
static void DestroySubpicture( subpicture_t * );
static void PreRender( spu_t *, subpicture_t *, const video_format_t * );
static void UpdateRegions( spu_t *, subpicture_t *,
                           const video_format_t *,  mtime_t );

/*****************************************************************************
 * decoder_sys_t: decoder data
 *****************************************************************************/
struct decoder_sys_t
{
    subpicture_t *p_spu_final;
    video_format_t fmt_cached;
    csri_inst *p_instance;

    struct csri_stream_ext *p_stream_ext;

    void (*pf_push_packet)(csri_inst *inst, const void *packet,
                           size_t packetlen, double pts_start,
                           double pts_end);
};

struct subpicture_sys_t
{
    decoder_t *p_dec;
    void      *p_subs_data;
    int       i_subs_len;
    mtime_t   i_pts;
};

/*****************************************************************************
 * Create: Open CSRI renderer
 *****************************************************************************
 * Comment me.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys;
    csri_rend *p_render;
    struct csri_stream_ext *p_stream_ext;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('s','s','a',' ') )
        return VLC_EGENERIC;

    p_render = csri_renderer_default();
    if (!p_render)
    {
        msg_Err( p_dec, "can't load csri renderer" );
        return VLC_EGENERIC;
    }
    p_stream_ext = csri_query_ext(p_render, CSRI_EXT_STREAM_ASS);
    if (!p_stream_ext)
    {
        msg_Err( p_dec, "csri renderer does not support ASS streaming" );
        return VLC_EGENERIC;
    }

    p_dec->pf_decode_sub = DecodeBlock;

    p_dec->p_sys = p_sys = calloc( 1, sizeof( decoder_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->p_stream_ext = p_stream_ext;
    p_sys->pf_push_packet = p_stream_ext->push_packet;
    p_sys->p_instance = p_stream_ext->init_stream( p_render,
                                                   p_dec->fmt_in.p_extra,
                                                   p_dec->fmt_in.p_extra ? strnlen( p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra ) : 0,
                                                   NULL);
    p_dec->fmt_out.i_cat = SPU_ES;
    p_dec->fmt_out.i_codec = VLC_FOURCC('R','G','B','A');

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: finish
 *****************************************************************************
 * Comment me.
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_stream_ext->discard )
        p_sys->p_stream_ext->discard( p_sys->p_instance, true );
    free( p_sys );
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with complete subtitles units.
 ****************************************************************************/
static subpicture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    subpicture_t *p_spu = NULL;
    block_t *p_block;

    //msg_Dbg( p_dec, "DecodeBlock %p", (void *)pp_block);
    if( !pp_block || *pp_block == NULL )
        return NULL;

    p_block = *pp_block;
    *pp_block = NULL;

    if( p_block->i_buffer == 0 || p_block->p_buffer[0] == '\0' )
    {
        block_Release( p_block );
        return NULL;
    }

    p_spu = decoder_NewSubpicture( p_dec );
    if( !p_spu )
    {
        msg_Warn( p_dec, "can't get spu buffer" );
        block_Release( p_block );
        return NULL;
    }

    p_spu->p_sys = malloc( sizeof( subpicture_sys_t ));
    if( !p_spu->p_sys )
    {
        decoder_DeleteSubpicture( p_dec, p_spu );
        block_Release( p_block );
        return NULL;
    }
    p_spu->p_sys->p_dec = p_dec;

    p_spu->p_sys->i_subs_len = p_block->i_buffer;
    p_spu->p_sys->p_subs_data = malloc( p_block->i_buffer );
    if( !p_spu->p_sys->p_subs_data )
    {
        free( p_spu->p_sys );
        decoder_DeleteSubpicture( p_dec, p_spu );
        block_Release( p_block );
        return NULL;
    }
    memcpy( p_spu->p_sys->p_subs_data, p_block->p_buffer,
            p_block->i_buffer );
    p_spu->p_sys->i_pts = p_block->i_pts;

    p_spu->i_start = p_block->i_pts;
    p_spu->i_stop = p_block->i_pts + p_block->i_length;
    p_spu->b_ephemer = false;
    p_spu->b_absolute = false;

    //msg_Dbg( p_dec, "BS %lf..%lf", p_spu->i_start * 0.000001, p_spu->i_stop * 0.000001);
    p_sys->pf_push_packet( p_sys->p_instance,
                           p_spu->p_sys->p_subs_data, p_spu->p_sys->i_subs_len,
                           p_spu->i_start * 0.000001,
                           p_spu->i_stop * 0.000001);

    p_spu->pf_pre_render = PreRender;
    p_spu->pf_update_regions = UpdateRegions;
    p_spu->pf_destroy = DestroySubpicture;

    block_Release( p_block );

    return p_spu;
}

static void DestroySubpicture( subpicture_t *p_subpic )
{
    //msg_Dbg( p_subpic->p_sys->p_dec, "drop spu %p", (void *)p_subpic );
    free( p_subpic->p_sys->p_subs_data );
    free( p_subpic->p_sys );
}

static void PreRender( spu_t *p_spu, subpicture_t *p_subpic,
                       const video_format_t *p_fmt )
{
    decoder_t *p_dec = p_subpic->p_sys->p_dec;
    p_dec->p_sys->p_spu_final = p_subpic;
    VLC_UNUSED(p_fmt);
    VLC_UNUSED(p_spu);
}

static void UpdateRegions( spu_t *p_spu, subpicture_t *p_subpic,
                           const video_format_t *p_fmt, mtime_t i_ts )
{
    decoder_t *p_dec = p_subpic->p_sys->p_dec;
    decoder_sys_t *p_sys = p_dec->p_sys;

    subpicture_region_t *p_spu_region;
    video_format_t fmt;

    /* TODO maybe checking if we really need redrawing */
    subpicture_region_ChainDelete( p_subpic->p_region );
    p_subpic->p_region = NULL;

    /* FIXME check why this is needed */
    if( p_subpic != p_sys->p_spu_final )
        return;

#if 0
    msg_Warn( p_dec, "---- fmt: %dx%d %dx%d chroma=%4.4s",
            p_fmt->i_width, p_fmt->i_height,
            p_fmt->i_visible_width, p_fmt->i_visible_height,
            (const char*)&p_fmt->i_chroma );
#endif
    /* XXX On x86 at least our RGBA is mapped to their BGRA
     * TODO confirm that is the same on big endian cpu */
    fmt = *p_fmt;
    fmt.i_chroma = VLC_FOURCC('R','G','B','A');
    fmt.i_width = fmt.i_visible_width;
    fmt.i_height = fmt.i_visible_height;
    fmt.i_bits_per_pixel = 0;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    fmt.i_sar_num = 1;
    fmt.i_sar_den = 1;

    if( memcmp(&fmt, &p_sys->fmt_cached, sizeof(fmt)) )
    {
        //msg_Warn( p_dec, "---- fmt: new %dx%d", fmt.i_width, fmt.i_height );

        struct csri_fmt csri_fmt;
        memset(&csri_fmt, 0, sizeof(csri_fmt));
        csri_fmt.pixfmt = CSRI_F_BGRA;
        csri_fmt.width = fmt.i_width;
        csri_fmt.height = fmt.i_height;
        if( csri_request_fmt( p_sys->p_instance, &csri_fmt ) )
            msg_Dbg( p_dec, "csri error: format not supported" );

        p_sys->fmt_cached = fmt;
    }

    p_subpic->i_original_picture_height = fmt.i_height;
    p_subpic->i_original_picture_width = fmt.i_width;

    p_spu_region = p_subpic->p_region = subpicture_region_New( &fmt );

    if( p_spu_region )
    {
        struct csri_frame csri_frame;

        /* */
        p_spu_region->i_align = SUBPICTURE_ALIGN_TOP | SUBPICTURE_ALIGN_LEFT;
        memset( p_spu_region->p_picture->Y_PIXELS, 0x00, p_spu_region->p_picture->Y_PITCH * p_sys->fmt_cached.i_height );

        /* */
        const mtime_t i_stream_date = p_subpic->p_sys->i_pts + (i_ts - p_subpic->i_start);

        //msg_Dbg( p_dec, "TS %lf", ts * 0.000001 );
        memset( &csri_frame, 0, sizeof(csri_frame) );
        csri_frame.pixfmt = CSRI_F_BGRA;
        csri_frame.planes[0] = (unsigned char*)p_spu_region->p_picture->Y_PIXELS;
        csri_frame.strides[0] = p_spu_region->p_picture->Y_PITCH;
        csri_render( p_sys->p_instance, &csri_frame, i_stream_date * 0.000001 );
    }
}

