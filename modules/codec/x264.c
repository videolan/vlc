/*****************************************************************************
 * x264.c: h264 video encoder
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>

#include <x264.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("h264 video encoder using x264 library"));
    set_capability( "encoder", 200 );
    set_callbacks( Open, Close );
vlc_module_end();


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static block_t *Encode( encoder_t *, picture_t * );

struct encoder_sys_t
{
    x264_t          *h;
    x264_param_t    param;
    x264_picture_t  *pic;

    int             i_buffer;
    uint8_t         *p_buffer;
};

/*****************************************************************************
 * Open: probe the encoder
 *****************************************************************************/
static int  Open ( vlc_object_t *p_this )
{
    encoder_t     *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;
    int           i_nal;
    x264_nal_t    *nal;

    int i;

    if( p_enc->fmt_out.i_cat != VIDEO_ES ||
        p_enc->fmt_out.i_codec != VLC_FOURCC( 'h', '2', '6', '4' ) )
    {
        return VLC_EGENERIC;
    }
    if( p_enc->fmt_in.video.i_width % 16 != 0 ||
        p_enc->fmt_in.video.i_height % 16!= 0 )
    {
        msg_Warn( p_enc, "invalid size %ix%i",
                  p_enc->fmt_in.video.i_width,
                  p_enc->fmt_in.video.i_height );
        return VLC_EGENERIC;
    }

    p_enc->pf_encode_video = Encode;
    p_enc->pf_encode_audio = NULL;
    p_enc->p_sys = p_sys = malloc( sizeof( encoder_sys_t ) );

    x264_param_default( &p_sys->param );
    p_sys->param.i_width  = p_enc->fmt_in.video.i_width;
    p_sys->param.i_height = p_enc->fmt_in.video.i_height;
    p_sys->param.analyse.inter = X264_ANALYSE_I16x16 | X264_ANALYSE_I4x4  |
                                 X264_ANALYSE_P16x16 |
                                 X264_ANALYSE_P16x8 | X264_ANALYSE_P8x16 |
                                 X264_ANALYSE_P8x8 | X264_ANALYSE_SMART_PSUB;
    p_sys->param.i_idrframe = 1;
    p_sys->param.b_cabac = 1;

    if( p_enc->fmt_in.video.i_aspect > 0 )
    {
        p_sys->param.vui.i_sar_width = p_enc->fmt_in.video.i_aspect *
                                       p_enc->fmt_in.video.i_height *
                                       p_enc->fmt_in.video.i_height /
                                       p_enc->fmt_in.video.i_width;
        p_sys->param.vui.i_sar_height = p_enc->fmt_in.video.i_height;
    }
    if( p_enc->fmt_in.video.i_frame_rate_base > 0 )
    {
        p_sys->param.f_fps = (float)p_enc->fmt_in.video.i_frame_rate /
                             (float)p_enc->fmt_in.video.i_frame_rate_base;
    }
    if( !(p_enc->p_libvlc->i_cpu & CPU_CAPABILITY_MMX) )
    {
        p_sys->param.cpu &= ~X264_CPU_MMX;
    }
    if( !(p_enc->p_libvlc->i_cpu & CPU_CAPABILITY_MMXEXT) )
    {
        p_sys->param.cpu &= ~X264_CPU_MMXEXT;
    }
    if( !(p_enc->p_libvlc->i_cpu & CPU_CAPABILITY_SSE) )
    {
        p_sys->param.cpu &= ~(X264_CPU_SSE|X264_CPU_SSE2);
    }
    if( p_enc->i_key_int > 0 )
    {
        p_sys->param.i_iframe = p_enc->i_key_int;
    }

    p_enc->fmt_in.i_codec = VLC_FOURCC('I','4','2','0');
    if( p_enc->i_qmin == p_enc->i_qmax )
    {
        p_sys->param.i_qp_constant = p_enc->i_qmin;
    }
    switch( p_enc->i_hq )
    {
        case -1:
            p_sys->param.analyse.inter = 0;
            break;
        case -2:
            p_sys->param.analyse.inter = X264_ANALYSE_I16x16|X264_ANALYSE_I4x4|X264_ANALYSE_P16x16;
            break;
        case -3:
            p_sys->param.analyse.inter = X264_ANALYSE_I16x16|X264_ANALYSE_I4x4|X264_ANALYSE_P16x16|
                                         X264_ANALYSE_P16x8|X264_ANALYSE_P8x16|X264_ANALYSE_P8x8|X264_ANALYSE_SMART_PSUB;
            break;
        case -4:
            p_sys->param.analyse.inter = X264_ANALYSE_I16x16|X264_ANALYSE_I4x4|X264_ANALYSE_P16x16|
                                         X264_ANALYSE_P16x8|X264_ANALYSE_P8x16|X264_ANALYSE_P8x8|
                                         X264_ANALYSE_P8x4|X264_ANALYSE_P4x8|X264_ANALYSE_P4x4|
                                         X264_ANALYSE_SMART_PSUB;
            break;
    }

    /* Open the encoder */
    p_sys->h = x264_encoder_open( &p_sys->param );

    /* alloc mem */
    p_sys->pic      = x264_picture_new( p_sys->h );
    p_sys->i_buffer = 4 * p_enc->fmt_in.video.i_width * p_enc->fmt_in.video.i_height + 1000;
    p_sys->p_buffer = malloc( p_sys->i_buffer );

    /* get the globals headers */
    p_enc->fmt_out.i_extra = 0;
    p_enc->fmt_out.p_extra = NULL;
#if 0
    x264_encoder_headers( p_sys->h, &nal, &i_nal );
    for( i = 0; i < i_nal; i++ )
    {
        int i_size = p_sys->i_buffer;

        x264_nal_encode( p_sys->p_buffer, &i_size, 1, &nal[i] );

        p_enc->fmt_out.p_extra = realloc( p_enc->fmt_out.p_extra, p_enc->fmt_out.i_extra + i_size );

        memcpy( p_enc->fmt_out.p_extra + p_enc->fmt_out.i_extra,
                p_sys->p_buffer, i_size );

        p_enc->fmt_out.i_extra += i_size;
    }
#endif

    return VLC_SUCCESS;
}

/****************************************************************************
 * Encode:
 ****************************************************************************/
static block_t *Encode( encoder_t *p_enc, picture_t *p_pict )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    int        i_nal;
    x264_nal_t *nal;
    block_t *p_block;
    int i_out;
    int i;

    /* copy the picture */
    for( i = 0; i < 3; i++ )
    {
        uint8_t *src = p_pict->p[i].p_pixels;
        uint8_t *dst = p_sys->pic->plane[i];
        int j;

        for( j = 0;j < p_pict->p[i].i_lines; j++ )
        {
            memcpy( dst, src, __MIN( p_sys->pic->i_stride[i], p_pict->p[i].i_pitch ) );

            src += p_pict->p[i].i_pitch;
            dst += p_sys->pic->i_stride[i];
        }
    }

    x264_encoder_encode( p_sys->h, &nal, &i_nal, p_sys->pic );
    for( i = 0, i_out = 0; i < i_nal; i++ )
    {
        int i_size = p_sys->i_buffer - i_out;
        x264_nal_encode( p_sys->p_buffer + i_out, &i_size, 1, &nal[i] );

        i_out += i_size;
    }

    p_block = block_New( p_enc, i_out );
    p_block->i_dts = p_pict->date;
    p_block->i_pts = p_pict->date;
    memcpy( p_block->p_buffer, p_sys->p_buffer, i_out );

    /* TODO */
    /* p_block->i_flags |= BLOCK_FLAG_TYPE_I; */

    return p_block;
}

/*****************************************************************************
 * CloseEncoder: ffmpeg encoder destruction
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    encoder_t     *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;


    x264_picture_delete( p_sys->pic );
    x264_encoder_close( p_sys->h );
    free( p_sys->p_buffer );
    free( p_sys );
}
