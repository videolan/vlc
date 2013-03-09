/*****************************************************************************
 * postproc.c: video postprocessing using libpostproc
 *****************************************************************************
 * Copyright (C) 1999-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Antoine Cellerier <dionoea at videolan dot org>
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
 * NOTA BENE: this module requires the linking against a library which is
 * known to require licensing under the GNU General Public License version 2
 * (or later). Therefore, the result of compiling this module will normally
 * be subject to the terms of that later license.
 *****************************************************************************/


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_cpu.h>

#include "filter_picture.h"

#ifdef HAVE_POSTPROC_POSTPROCESS_H
#   include <postproc/postprocess.h>
#else
#   include <libpostproc/postprocess.h>
#endif

#ifndef PP_CPU_CAPS_ALTIVEC
#   define PP_CPU_CAPS_ALTIVEC 0
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int OpenPostproc( vlc_object_t * );
static void ClosePostproc( vlc_object_t * );

static picture_t *PostprocPict( filter_t *, picture_t * );

static int PPQCallback( vlc_object_t *, char const *,
                        vlc_value_t, vlc_value_t, void * );
static int PPNameCallback( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );

#define Q_TEXT N_("Post processing quality")
#define Q_LONGTEXT N_( \
    "Quality of post processing. Valid range is 0 (disabled) to 6 (highest)\n"     \
    "Higher levels require more CPU power, but produce higher quality pictures.\n" \
    "With default filter chain, the values map to the following filters:\n"        \
    "1: hb, 2-4: hb+vb, 5-6: hb+vb+dr" )

#define NAME_TEXT N_("FFmpeg post processing filter chains")
#define NAME_LONGTEXT NAME_TEXT

#define FILTER_PREFIX "postproc-"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Video post processing filter") )
    set_shortname( N_("Postproc" ) )
    add_shortcut( "postprocess", "pp" ) /* name is "postproc" */
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    set_capability( "video filter2", 0 )

    set_callbacks( OpenPostproc, ClosePostproc )

    add_integer_with_range( FILTER_PREFIX "q", PP_QUALITY_MAX, 0,
                            PP_QUALITY_MAX, Q_TEXT, Q_LONGTEXT, false )
        change_safe()
    add_string( FILTER_PREFIX "name", "default", NAME_TEXT,
                NAME_LONGTEXT, true )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "q", "name", NULL
};

/*****************************************************************************
 * filter_sys_t : libpostproc video postprocessing descriptor
 *****************************************************************************/
struct filter_sys_t
{
    /* Never changes after init */
    pp_context *pp_context;

    /* Set to NULL if post processing is disabled */
    pp_mode *pp_mode;

    /* Lock when using or changing pp_mode */
    vlc_mutex_t lock;
};


/*****************************************************************************
 * OpenPostproc: probe and open the postproc
 *****************************************************************************/
static int OpenPostproc( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;
    vlc_value_t val, val_orig, text;
    int i_flags = 0;

    if( p_filter->fmt_in.video.i_chroma != p_filter->fmt_out.video.i_chroma ||
        p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height ||
        p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width )
    {
        msg_Err( p_filter, "Filter input and output formats must be identical" );
        return VLC_EGENERIC;
    }

    /* Set CPU capabilities */
#if defined(__i386__) || defined(__x86_64__)
    if( vlc_CPU_MMX() )
        i_flags |= PP_CPU_CAPS_MMX;
    if( vlc_CPU_MMXEXT() )
        i_flags |= PP_CPU_CAPS_MMX2;
    if( vlc_CPU_3dNOW() )
        i_flags |= PP_CPU_CAPS_3DNOW;
#elif defined(__ppc__) || defined(__ppc64__) || defined(__powerpc__)
    if( vlc_CPU_ALTIVEC() )
        i_flags |= PP_CPU_CAPS_ALTIVEC;
#endif

    switch( p_filter->fmt_in.video.i_chroma )
    {
        case VLC_CODEC_I444:
        case VLC_CODEC_J444:
        /* case VLC_CODEC_YUVA:
           FIXME: Should work but alpha plane needs to be copied manually and
                  I'm kind of feeling too lazy to write the code to do that ATM
                  (i_pitch vs i_visible_pitch...). */
            i_flags |= PP_FORMAT_444;
            break;
        case VLC_CODEC_I422:
        case VLC_CODEC_J422:
            i_flags |= PP_FORMAT_422;
            break;
        case VLC_CODEC_I411:
            i_flags |= PP_FORMAT_411;
            break;
        case VLC_CODEC_I420:
        case VLC_CODEC_J420:
        case VLC_CODEC_YV12:
            i_flags |= PP_FORMAT_420;
            break;
        default:
            msg_Err( p_filter, "Unsupported input chroma (%4.4s)",
                      (char*)&p_filter->fmt_in.video.i_chroma );
            return VLC_EGENERIC;
    }

    p_sys = malloc( sizeof( filter_sys_t ) );
    if( !p_sys ) return VLC_ENOMEM;
    p_filter->p_sys = p_sys;

    p_sys->pp_context = pp_get_context( p_filter->fmt_in.video.i_width,
                                        p_filter->fmt_in.video.i_height,
                                        i_flags );
    if( !p_sys->pp_context )
    {
        msg_Err( p_filter, "Error while creating post processing context." );
        free( p_sys );
        return VLC_EGENERIC;
    }

    config_ChainParse( p_filter, FILTER_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    var_Create( p_filter, FILTER_PREFIX "q", VLC_VAR_INTEGER |
                VLC_VAR_HASCHOICE | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND );

    text.psz_string = _("Post processing");
    var_Change( p_filter, FILTER_PREFIX "q", VLC_VAR_SETTEXT, &text, NULL );

    var_Get( p_filter, FILTER_PREFIX "q", &val_orig );
    var_Change( p_filter, FILTER_PREFIX "q", VLC_VAR_DELCHOICE, &val_orig, NULL );

    val.psz_string = var_GetNonEmptyString( p_filter, FILTER_PREFIX "name" );
    if( val_orig.i_int )
    {
        p_sys->pp_mode = pp_get_mode_by_name_and_quality( val.psz_string ?
                                                          val.psz_string :
                                                          "default",
                                                          val_orig.i_int );

        if( !p_sys->pp_mode )
        {
            msg_Err( p_filter, "Error while creating post processing mode." );
            free( val.psz_string );
            pp_free_context( p_sys->pp_context );
            free( p_sys );
            return VLC_EGENERIC;
        }
    }
    else
    {
        p_sys->pp_mode = NULL;
    }
    free( val.psz_string );

    for( val.i_int = 0; val.i_int <= PP_QUALITY_MAX; val.i_int++ )
    {
        switch( val.i_int )
        {
            case 0:
                text.psz_string = _("Disable");
                break;
            case 1:
                text.psz_string = _("Lowest");
                break;
            case PP_QUALITY_MAX:
                text.psz_string = _("Highest");
                break;
            default:
                text.psz_string = NULL;
                break;
        }
        var_Change( p_filter, FILTER_PREFIX "q", VLC_VAR_ADDCHOICE,
                    &val, text.psz_string?&text:NULL );
    }

    vlc_mutex_init( &p_sys->lock );

    /* Add the callback at the end to prevent crashes */
    var_AddCallback( p_filter, FILTER_PREFIX "q", PPQCallback, NULL );
    var_AddCallback( p_filter, FILTER_PREFIX "name", PPNameCallback, NULL );

    p_filter->pf_video_filter = PostprocPict;

    msg_Warn( p_filter, "Quantification table was not set by video decoder. "
                        "Postprocessing won't look good." );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * ClosePostproc
 *****************************************************************************/
static void ClosePostproc( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    /* delete the callback before destroying the mutex */
    var_DelCallback( p_filter, FILTER_PREFIX "q", PPQCallback, NULL );
    var_DelCallback( p_filter, FILTER_PREFIX "name", PPNameCallback, NULL );

    /* Destroy the resources */
    vlc_mutex_destroy( &p_sys->lock );
    pp_free_context( p_sys->pp_context );
    if( p_sys->pp_mode ) pp_free_mode( p_sys->pp_mode );
    free( p_sys );
}

/*****************************************************************************
 * PostprocPict
 *****************************************************************************/
static picture_t *PostprocPict( filter_t *p_filter, picture_t *p_pic )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    const uint8_t *src[3];
    uint8_t *dst[3];
    int i_plane;
    int i_src_stride[3], i_dst_stride[3];

    picture_t *p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }

    /* Lock to prevent issues if pp_mode is changed */
    vlc_mutex_lock( &p_sys->lock );
    if( !p_sys->pp_mode )
    {
        vlc_mutex_unlock( &p_sys->lock );
        picture_CopyPixels( p_outpic, p_pic );
        return CopyInfoAndRelease( p_outpic, p_pic );
    }


    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        src[i_plane] = p_pic->p[i_plane].p_pixels;
        dst[i_plane] = p_outpic->p[i_plane].p_pixels;

        /* I'm not sure what happens if i_pitch != i_visible_pitch ...
         * at least it shouldn't crash. */
        i_src_stride[i_plane] = p_pic->p[i_plane].i_pitch;
        i_dst_stride[i_plane] = p_outpic->p[i_plane].i_pitch;
    }

    pp_postprocess( src, i_src_stride, dst, i_dst_stride,
                    p_filter->fmt_in.video.i_width,
                    p_filter->fmt_in.video.i_height, NULL, 0,
                    p_sys->pp_mode, p_sys->pp_context, 0 );
    vlc_mutex_unlock( &p_sys->lock );

    return CopyInfoAndRelease( p_outpic, p_pic );
}

/*****************************************************************************
 * PPChangeMode: change the current mode and quality
 *****************************************************************************/
static void PPChangeMode( filter_t *p_filter, const char *psz_name,
                          int i_quality )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    vlc_mutex_lock( &p_sys->lock );
    if( i_quality > 0 )
    {
        pp_mode *pp_mode = pp_get_mode_by_name_and_quality( psz_name ?
                                                              psz_name :
                                                              "default",
                                                              i_quality );
        if( pp_mode )
        {
            pp_free_mode( p_sys->pp_mode );
            p_sys->pp_mode = pp_mode;
        }
        else
            msg_Warn( p_filter, "Error while changing post processing mode. "
                      "Keeping previous mode." );
    }
    else
    {
        pp_free_mode( p_sys->pp_mode );
        p_sys->pp_mode = NULL;
    }
    vlc_mutex_unlock( &p_sys->lock );
}

static int PPQCallback( vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(psz_var); VLC_UNUSED(oldval); VLC_UNUSED(p_data);
    filter_t *p_filter = (filter_t *)p_this;

    char *psz_name = var_GetNonEmptyString( p_filter, FILTER_PREFIX "name" );
    PPChangeMode( p_filter, psz_name, newval.i_int );
    free( psz_name );
    return VLC_SUCCESS;
}

static int PPNameCallback( vlc_object_t *p_this, const char *psz_var,
                           vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(psz_var); VLC_UNUSED(oldval); VLC_UNUSED(p_data);
    filter_t *p_filter = (filter_t *)p_this;

    int i_quality = var_GetInteger( p_filter, FILTER_PREFIX "q" );
    PPChangeMode( p_filter, *newval.psz_string ? newval.psz_string : NULL,
                  i_quality );
    return VLC_SUCCESS;
}
