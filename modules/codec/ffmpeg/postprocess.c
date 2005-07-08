/*****************************************************************************
 * postprocess.c: video postprocessing using the ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>

/* ffmpeg header */
#ifdef HAVE_FFMPEG_AVCODEC_H
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#include "ffmpeg.h"

#ifdef LIBAVCODEC_PP

#ifdef HAVE_POSTPROC_POSTPROCESS_H
#   include <postproc/postprocess.h>
#else
#   include <libpostproc/postprocess.h>
#endif

#ifndef PP_CPU_CAPS_ALTIVEC
#   define PP_CPU_CAPS_ALTIVEC 0
#endif

/*****************************************************************************
 * video_postproc_sys_t : ffmpeg video postprocessing descriptor
 *****************************************************************************/
typedef struct video_postproc_sys_t
{
    pp_context_t *pp_context;
    pp_mode_t    *pp_mode;

    vlc_bool_t   *pb_pp;

    int i_width;
    int i_height;

} video_postproc_sys_t;

static int PPQCallback( vlc_object_t *p_this, char const *psz_cmd,
                        vlc_value_t oldval, vlc_value_t newval, void *p_data );

/*****************************************************************************
 * OpenPostproc: probe and open the postproc
 *****************************************************************************/
void *E_(OpenPostproc)( decoder_t *p_dec, vlc_bool_t *pb_pp )
{
    video_postproc_sys_t *p_sys;
    vlc_value_t val, val_orig, text;

    p_sys = malloc( sizeof(video_postproc_sys_t) );
    p_sys->pp_context = NULL;
    p_sys->pp_mode = NULL;

    *pb_pp = VLC_FALSE;
    p_sys->pb_pp = pb_pp;

    /* Create object variable if not already done */
    if( var_Type( p_dec, "ffmpeg-pp-q" ) == 0 )
    {
        var_Create( p_dec, "ffmpeg-pp-q",
                    VLC_VAR_INTEGER | VLC_VAR_HASCHOICE | VLC_VAR_DOINHERIT );
        text.psz_string = _("Post processing");
        var_Change( p_dec, "ffmpeg-pp-q", VLC_VAR_SETTEXT, &text, NULL );

        var_Get( p_dec, "ffmpeg-pp-q", &val_orig );
        var_Change( p_dec, "ffmpeg-pp-q", VLC_VAR_DELCHOICE, &val_orig, NULL );

        val.i_int = 0; text.psz_string = _("Disable");
        var_Change( p_dec, "ffmpeg-pp-q", VLC_VAR_ADDCHOICE, &val, &text );
        val.i_int = 1; text.psz_string = _("1 (Lowest)");
        var_Change( p_dec, "ffmpeg-pp-q", VLC_VAR_ADDCHOICE, &val, &text );
        val.i_int = 2;
        var_Change( p_dec, "ffmpeg-pp-q", VLC_VAR_ADDCHOICE, &val, NULL );
        val.i_int = 3;
        var_Change( p_dec, "ffmpeg-pp-q", VLC_VAR_ADDCHOICE, &val, NULL );
        val.i_int = 4;
        var_Change( p_dec, "ffmpeg-pp-q", VLC_VAR_ADDCHOICE, &val, NULL );
        val.i_int = 5;
        var_Change( p_dec, "ffmpeg-pp-q", VLC_VAR_ADDCHOICE, &val, NULL );
        val.i_int = 6; text.psz_string = _("6 (Highest)");
        var_Change( p_dec, "ffmpeg-pp-q", VLC_VAR_ADDCHOICE, &val, &text );
        var_AddCallback( p_dec, "ffmpeg-pp-q", PPQCallback, p_sys );
    }

    /* ***** Load post processing if enabled ***** */
    var_Get( p_dec, "ffmpeg-pp-q", &val );
    var_Set( p_dec, "ffmpeg-pp-q", val_orig );
    if( val_orig.i_int )
        *pb_pp = VLC_TRUE;

    return p_sys;
}

/*****************************************************************************
 * InitPostproc: 
 *****************************************************************************/
int E_(InitPostproc)( decoder_t *p_dec, void *p_data,
                      int i_width, int i_height, int pix_fmt )
{
    video_postproc_sys_t *p_sys = (video_postproc_sys_t *)p_data;
    int32_t i_cpu = p_dec->p_libvlc->i_cpu;
    int i_flags = 0;

    /* Set CPU capabilities */
    if( i_cpu & CPU_CAPABILITY_MMX )
    {
        i_flags |= PP_CPU_CAPS_MMX;
    }
    if( i_cpu & CPU_CAPABILITY_MMXEXT )
    {
        i_flags |= PP_CPU_CAPS_MMX2;
    }
    if( i_cpu & CPU_CAPABILITY_3DNOW )
    {
        i_flags |= PP_CPU_CAPS_3DNOW;
    }
    if( i_cpu & CPU_CAPABILITY_ALTIVEC )
    {
        i_flags |= PP_CPU_CAPS_ALTIVEC;
    }

    switch( pix_fmt )
    {
    case PIX_FMT_YUV444P:
        i_flags |= PP_FORMAT_444;
        break;
    case PIX_FMT_YUV422P:
        i_flags |= PP_FORMAT_422;
        break;
    case PIX_FMT_YUV411P:
        i_flags |= PP_FORMAT_411;
        break;
    default:
        i_flags |= PP_FORMAT_420;
        break;
    }

    p_sys->pp_context = pp_get_context( i_width, i_height, i_flags );
    p_sys->i_width = i_width;
    p_sys->i_height = i_height;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * PostprocPict: 
 *****************************************************************************/
int E_(PostprocPict)( decoder_t *p_dec, void *p_data,
                      picture_t *p_pic, AVFrame *p_ff_pic )
{
    video_postproc_sys_t *p_sys = (video_postproc_sys_t *)p_data;

    uint8_t *src[3], *dst[3];
    int i_plane, i_src_stride[3], i_dst_stride[3];

    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        src[i_plane] = p_ff_pic->data[i_plane];
        dst[i_plane] = p_pic->p[i_plane].p_pixels;

        i_src_stride[i_plane] = p_ff_pic->linesize[i_plane];
        i_dst_stride[i_plane] = p_pic->p[i_plane].i_pitch;
    }

    pp_postprocess( src, i_src_stride, dst, i_dst_stride,
                    p_sys->i_width, p_sys->i_height,
                    p_ff_pic->qscale_table, p_ff_pic->qstride,
                    p_sys->pp_mode, p_sys->pp_context,
                    p_ff_pic->pict_type );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ClosePostproc: 
 *****************************************************************************/
void E_(ClosePostproc)( decoder_t *p_dec, void *p_data )
{
    video_postproc_sys_t *p_sys = (video_postproc_sys_t *)p_data;

    if( p_sys && p_sys->pp_mode )
    {
        pp_free_mode( p_sys->pp_mode );
        if( p_sys->pp_context ) pp_free_context( p_sys->pp_context );
    }

    var_DelCallback( p_dec, "ffmpeg-pp-q", PPQCallback, p_sys );

    if( p_sys ) free( p_sys );
}

/*****************************************************************************
 * object variables callbacks: a bunch of object variables are used by the
 * interfaces to interact with the decoder.
 *****************************************************************************/
static int PPQCallback( vlc_object_t *p_this, char const *psz_cmd,
                        vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    video_postproc_sys_t *p_sys = (video_postproc_sys_t *)p_data;

    if( newval.i_int > 0 )
    {
        int  i_quality = newval.i_int;
        char *psz_name = config_GetPsz( p_dec, "ffmpeg-pp-name" );
        pp_mode_t *pp_mode;

        if( !psz_name )
        {
            psz_name = strdup( "default" );
        }
        else if( *psz_name == '\0' )
        {
            free( psz_name );
            psz_name = strdup( "default" );
        }

        pp_mode = pp_get_mode_by_name_and_quality( psz_name, i_quality );

        if( !pp_mode )
        {
            msg_Err( p_dec, "failed getting mode for postproc" );
            newval.i_int = 0;
        }
        else
        {
            msg_Dbg( p_dec, "postprocessing enabled" );
        }
        free( psz_name );

        p_sys->pp_mode = pp_mode;
    }
    else
    {
        msg_Dbg( p_dec, "postprocessing disabled" );
    }

    *p_sys->pb_pp = newval.i_int;

    return VLC_SUCCESS;
}

#endif /* LIBAVCODEC_PP */
