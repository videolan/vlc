/*****************************************************************************
 * deinterlace.c : deinterlacer plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2009 the VideoLAN team
 * $Id$
 *
 * Author: Sam Hocevar <sam@zoy.org>
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
# include "config.h"
#endif

#include <errno.h>
#include <assert.h>

#ifdef HAVE_ALTIVEC_H
#   include <altivec.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>
#include <vlc_sout.h>
#include <vlc_filter.h>
#include <vlc_cpu.h>

#ifdef CAN_COMPILE_MMXEXT
#   include "mmx.h"
#endif

#include "filter_common.h"

#define DEINTERLACE_DISCARD 1
#define DEINTERLACE_MEAN    2
#define DEINTERLACE_BLEND   3
#define DEINTERLACE_BOB     4
#define DEINTERLACE_LINEAR  5
#define DEINTERLACE_X       6
#define DEINTERLACE_YADIF   7
#define DEINTERLACE_YADIF2X 8

/*****************************************************************************
 * Local protypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );

static int  MouseEvent( vlc_object_t *p_this, char const *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *p_data );

static void RenderDiscard( vout_thread_t *, picture_t *, picture_t *, int );
static void RenderBob    ( vout_thread_t *, picture_t *, picture_t *, int );
static void RenderMean   ( vout_thread_t *, picture_t *, picture_t * );
static void RenderBlend  ( vout_thread_t *, picture_t *, picture_t * );
static void RenderLinear ( vout_thread_t *, picture_t *, picture_t *, int );
static void RenderX      ( picture_t *, picture_t * );
static void RenderYadif  ( vout_thread_t *, picture_t *, picture_t *, int, int );

static void MergeGeneric ( void *, const void *, const void *, size_t );
#if defined(CAN_COMPILE_C_ALTIVEC)
static void MergeAltivec ( void *, const void *, const void *, size_t );
#endif
#if defined(CAN_COMPILE_MMXEXT)
static void MergeMMXEXT  ( void *, const void *, const void *, size_t );
#endif
#if defined(CAN_COMPILE_3DNOW)
static void Merge3DNow   ( void *, const void *, const void *, size_t );
#endif
#if defined(CAN_COMPILE_SSE)
static void MergeSSE2    ( void *, const void *, const void *, size_t );
#endif
#if defined(CAN_COMPILE_MMXEXT) || defined(CAN_COMPILE_SSE)
static void EndMMX       ( void );
#endif
#if defined(CAN_COMPILE_3DNOW)
static void End3DNow     ( void );
#endif
#if defined __ARM_NEON__
static void MergeNEON (void *, const void *, const void *, size_t);
#endif

static void SetFilterMethod( vout_thread_t *p_vout, const char *psz_method );
static vout_thread_t *SpawnRealVout( vout_thread_t *p_vout );

static int OpenFilter( vlc_object_t *p_this );
static void CloseFilter( vlc_object_t *p_this );

/*****************************************************************************
 * Callback prototypes
 *****************************************************************************/
static int FilterCallback( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define MODE_TEXT N_("Deinterlace mode")
#define MODE_LONGTEXT N_("Deinterlace method to use for local playback.")

#define SOUT_MODE_TEXT N_("Streaming deinterlace mode")
#define SOUT_MODE_LONGTEXT N_("Deinterlace method to use for streaming.")

#define FILTER_CFG_PREFIX "sout-deinterlace-"

static const char *const mode_list[] = {
    "discard", "blend", "mean", "bob", "linear", "x", "yadif", "yadif2x" };
static const char *const mode_list_text[] = {
    N_("Discard"), N_("Blend"), N_("Mean"), N_("Bob"), N_("Linear"), "X", "Yadif", "Yadif (2x)" };

vlc_module_begin ()
    set_description( N_("Deinterlacing video filter") )
    set_shortname( N_("Deinterlace" ))
    set_capability( "video filter", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    set_section( N_("Display"),NULL)
    add_string( "filter-deinterlace-mode", "discard", NULL, MODE_TEXT,
                MODE_LONGTEXT, false )
        change_string_list( mode_list, mode_list_text, 0 )
        change_safe ()

    add_shortcut( "deinterlace" )
    set_callbacks( Create, Destroy )

    add_submodule ()
    set_capability( "video filter2", 0 )
    set_section( N_("Streaming"),NULL)
    add_string( FILTER_CFG_PREFIX "mode", "blend", NULL, SOUT_MODE_TEXT,
                SOUT_MODE_LONGTEXT, false )
        change_string_list( mode_list, mode_list_text, 0 )
    add_shortcut( "deinterlace" )
    set_callbacks( OpenFilter, CloseFilter )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "mode", NULL
};

/*****************************************************************************
 * vout_sys_t: Deinterlace video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Deinterlace specific properties of an output thread.
 *****************************************************************************/
#define HISTORY_SIZE (3)
struct vout_sys_t
{
    int        i_mode;        /* Deinterlace mode */
    bool b_double_rate; /* Shall we double the framerate? */
    bool b_half_height; /* Shall be devide the height by 2 */

    mtime_t    last_date;
    mtime_t    next_date;

    vout_thread_t *p_vout;

    vlc_mutex_t filter_lock;

    void (*pf_merge) ( void *, const void *, const void *, size_t );
    void (*pf_end_merge) ( void );

    /* Yadif */
    picture_t *pp_history[HISTORY_SIZE];
};

/*****************************************************************************
 * Control: control facility for the vout (forwards to child vout)
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    return vout_vaControl( p_vout->p_sys->p_vout, i_query, args );
}

/*****************************************************************************
 * Create: allocates Deinterlace video thread output method
 *****************************************************************************
 * This function allocates and initializes a Deinterlace vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_sys_t *p_sys;
    char *psz_mode;

    /* Allocate structure */
    p_sys = p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
        return VLC_ENOMEM;

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = Render;
    p_vout->pf_display = NULL;
    p_vout->pf_control = Control;

    p_sys->i_mode = DEINTERLACE_DISCARD;
    p_sys->b_double_rate = false;
    p_sys->b_half_height = true;
    p_sys->last_date = 0;
    p_sys->p_vout = 0;
    vlc_mutex_init( &p_sys->filter_lock );

#if defined(CAN_COMPILE_C_ALTIVEC)
    if( vlc_CPU() & CPU_CAPABILITY_ALTIVEC )
    {
        p_sys->pf_merge = MergeAltivec;
        p_sys->pf_end_merge = NULL;
    }
    else
#endif
#if defined(CAN_COMPILE_SSE)
    if( vlc_CPU() & CPU_CAPABILITY_SSE2 )
    {
        p_sys->pf_merge = MergeSSE2;
        p_sys->pf_end_merge = EndMMX;
    }
    else
#endif
#if defined(CAN_COMPILE_MMXEXT)
    if( vlc_CPU() & CPU_CAPABILITY_MMXEXT )
    {
        p_sys->pf_merge = MergeMMXEXT;
        p_sys->pf_end_merge = EndMMX;
    }
    else
#endif
#if defined(CAN_COMPILE_3DNOW)
    if( vlc_CPU() & CPU_CAPABILITY_3DNOW )
    {
        p_sys->pf_merge = Merge3DNow;
        p_sys->pf_end_merge = End3DNow;
    }
    else
#endif
#if defined __ARM_NEON__
    if( vlc_CPU() & CPU_CAPABILITY_NEON )
    {
        p_sys->pf_merge = MergeNEON;
        p_sys->pf_end_merge = NULL;
    }
    else
#endif
    {
        p_sys->pf_merge = MergeGeneric;
        p_sys->pf_end_merge = NULL;
    }

    /* Look what method was requested */
    psz_mode = var_CreateGetString( p_vout, "filter-deinterlace-mode" );

    if( !psz_mode )
    {
        msg_Err( p_vout, "configuration variable filter-deinterlace-mode empty" );
        msg_Err( p_vout, "no deinterlace mode provided, using \"discard\"" );

        psz_mode = strdup( "discard" );
    }

    SetFilterMethod( p_vout, psz_mode );

    free( psz_mode );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SetFilterMethod: setup the deinterlace method to use.
 *****************************************************************************/
static void SetFilterMethod( vout_thread_t *p_vout, const char *psz_method )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    if( !strcmp( psz_method, "mean" ) )
    {
        p_sys->i_mode = DEINTERLACE_MEAN;
        p_sys->b_double_rate = false;
        p_sys->b_half_height = true;
    }
    else if( !strcmp( psz_method, "blend" )
             || !strcmp( psz_method, "average" )
             || !strcmp( psz_method, "combine-fields" ) )
    {
        p_sys->i_mode = DEINTERLACE_BLEND;
        p_sys->b_double_rate = false;
        p_sys->b_half_height = false;
    }
    else if( !strcmp( psz_method, "bob" )
             || !strcmp( psz_method, "progressive-scan" ) )
    {
        p_sys->i_mode = DEINTERLACE_BOB;
        p_sys->b_double_rate = true;
        p_sys->b_half_height = false;
    }
    else if( !strcmp( psz_method, "linear" ) )
    {
        p_sys->i_mode = DEINTERLACE_LINEAR;
        p_sys->b_double_rate = true;
        p_sys->b_half_height = false;
    }
    else if( !strcmp( psz_method, "x" ) )
    {
        p_sys->i_mode = DEINTERLACE_X;
        p_sys->b_double_rate = false;
        p_sys->b_half_height = false;
    }
    else if( !strcmp( psz_method, "yadif" ) )
    {
        p_sys->i_mode = DEINTERLACE_YADIF;
        p_sys->b_double_rate = false;
        p_sys->b_half_height = false;
    }
    else if( !strcmp( psz_method, "yadif2x" ) )
    {
        p_sys->i_mode = DEINTERLACE_YADIF2X;
        p_sys->b_double_rate = true;
        p_sys->b_half_height = false;
    }
    else
    {
        const bool b_i422 = p_vout->render.i_chroma == VLC_CODEC_I422 ||
                            p_vout->render.i_chroma == VLC_CODEC_J422;
        if( strcmp( psz_method, "discard" ) )
            msg_Err( p_vout, "no valid deinterlace mode provided, "
                     "using \"discard\"" );

        p_sys->i_mode = DEINTERLACE_DISCARD;
        p_sys->b_double_rate = false;
        p_sys->b_half_height = !b_i422;
    }

    msg_Dbg( p_vout, "using %s deinterlace method", psz_method );
}

static void GetOutputFormat( vout_thread_t *p_vout,
                             video_format_t *p_dst, const video_format_t *p_src )
{
    *p_dst = *p_src;

    if( p_vout->p_sys->b_half_height )
    {
        p_dst->i_height /= 2;
        p_dst->i_visible_height /= 2;
        p_dst->i_y_offset /= 2;
        p_dst->i_sar_den *= 2;
    }

    if( p_src->i_chroma == VLC_CODEC_I422 ||
        p_src->i_chroma == VLC_CODEC_J422 )
    {
        switch( p_vout->p_sys->i_mode )
        {
        case DEINTERLACE_MEAN:
        case DEINTERLACE_LINEAR:
        case DEINTERLACE_X:
        case DEINTERLACE_YADIF:
        case DEINTERLACE_YADIF2X:
            p_dst->i_chroma = p_src->i_chroma;
            break;
        default:
            p_dst->i_chroma = p_src->i_chroma == VLC_CODEC_I422 ? VLC_CODEC_I420 :
                                                                  VLC_CODEC_J420;
            break;
        }
    }
}

static bool IsChromaSupported( vlc_fourcc_t i_chroma )
{
    return i_chroma == VLC_CODEC_I420 ||
           i_chroma == VLC_CODEC_J420 ||
           i_chroma == VLC_CODEC_YV12 ||
           i_chroma == VLC_CODEC_I422 ||
           i_chroma == VLC_CODEC_J422;
}

/*****************************************************************************
 * Init: initialize Deinterlace video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    I_OUTPUTPICTURES = 0;

    if( !IsChromaSupported( p_vout->render.i_chroma ) )
        return VLC_EGENERIC; /* unknown chroma */

    /* Initialize the output structure, full of directbuffers since we want
     * the decoder to output directly to our structures. */
    p_vout->output.i_chroma = p_vout->render.i_chroma;
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;
    p_vout->fmt_out = p_vout->fmt_in;

    /* Try to open the real video output */
    p_vout->p_sys->p_vout = SpawnRealVout( p_vout );

    if( p_vout->p_sys->p_vout == NULL )
    {
        /* Everything failed */
        msg_Err( p_vout, "cannot open vout, aborting" );

        return VLC_EGENERIC;
    }

    for( int i = 0; i < HISTORY_SIZE; i++ )
        p_vout->p_sys->pp_history[i] = NULL;

    vout_filter_AllocateDirectBuffers( p_vout, VOUT_MAX_PICTURES );

    vout_filter_AddChild( p_vout, p_vout->p_sys->p_vout, MouseEvent );

    var_AddCallback( p_vout, "filter-deinterlace-mode", FilterCallback, NULL );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SpawnRealVout: spawn the real video output.
 *****************************************************************************/
static vout_thread_t *SpawnRealVout( vout_thread_t *p_vout )
{
    msg_Dbg( p_vout, "spawning the real video output" );

    video_format_t fmt;
    GetOutputFormat( p_vout, &fmt, &p_vout->fmt_out );

    return vout_Create( p_vout, &fmt );
}

/*****************************************************************************
 * End: terminate Deinterlace video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    var_DelCallback( p_vout, "filter-deinterlace-mode", FilterCallback, NULL );

    for( int i = 0; i < HISTORY_SIZE; i++ )
    {
        if( p_sys->pp_history[i] )
            picture_Release( p_sys->pp_history[i] );
    }

    if( p_sys->p_vout )
    {
        vout_filter_DelChild( p_vout, p_sys->p_vout, MouseEvent );
        vout_CloseAndRelease( p_sys->p_vout );
    }

    vout_filter_ReleaseDirectBuffers( p_vout );
}

/*****************************************************************************
 * Destroy: destroy Deinterlace video thread output method
 *****************************************************************************
 * Terminate an output method created by DeinterlaceCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vlc_mutex_destroy( &p_vout->p_sys->filter_lock );
    free( p_vout->p_sys );
}

/**
 * Forward mouse event with proper conversion.
 */
static int MouseEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = p_data;
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);

    if( !strcmp( psz_var, "mouse-y" ) && p_vout->p_sys->b_half_height )
        newval.i_int *= 2;

    return var_Set( p_vout, psz_var, newval );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Deinterlace image,
 * waits until it is displayed and switch the two rendering buffers, preparing
 * next frame.
 *****************************************************************************/
static void Render ( vout_thread_t *p_vout, picture_t *p_pic )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    picture_t *pp_outpic[2];

    /* FIXME are they needed ? */
    p_vout->fmt_out.i_x_offset = p_vout->fmt_in.i_x_offset;
    p_vout->fmt_out.i_y_offset = p_vout->fmt_in.i_y_offset;
    p_vout->fmt_out.i_visible_width = p_vout->fmt_in.i_visible_width;
    p_vout->fmt_out.i_visible_height = p_vout->fmt_in.i_visible_height;

    /* FIXME p_sys->p_vout->* should NOT be changed FIXME */
    p_sys->p_vout->fmt_in.i_x_offset = p_vout->fmt_out.i_x_offset;
    p_sys->p_vout->fmt_in.i_y_offset = p_vout->fmt_out.i_y_offset;
    p_sys->p_vout->fmt_in.i_visible_width = p_vout->fmt_out.i_visible_width;
    p_sys->p_vout->fmt_in.i_visible_height = p_vout->fmt_in.i_visible_height;
    if( p_vout->p_sys->b_half_height )
    {
        p_sys->p_vout->fmt_in.i_y_offset /= 2;
        p_sys->p_vout->fmt_in.i_visible_height /= 2;
    }

    if( p_vout->i_changes & VOUT_ASPECT_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_ASPECT_CHANGE;

        p_vout->fmt_out.i_sar_num = p_vout->fmt_in.i_sar_num;
        p_vout->fmt_out.i_sar_den = p_vout->fmt_in.i_sar_den;

        video_format_t fmt = p_vout->fmt_out;
        if( p_vout->p_sys->b_half_height )
        {
            fmt.i_height /= 2; fmt.i_visible_height /= 2; fmt.i_y_offset /= 2;
            fmt.i_sar_den *= 2;
        }

        p_sys->p_vout = vout_Request( p_vout, p_sys->p_vout, &fmt );
    }
    if( !p_sys->p_vout )
        return;

    pp_outpic[0] = pp_outpic[1] = NULL;

    vlc_mutex_lock( &p_vout->p_sys->filter_lock );

    /* Get a new picture */
    while( ( pp_outpic[0] = vout_CreatePicture( p_vout->p_sys->p_vout,
                                                0, 0, 0 ) )
              == NULL )
    {
        if( !vlc_object_alive( p_vout ) || p_vout->b_error )
        {
            vlc_mutex_unlock( &p_vout->p_sys->filter_lock );
            return;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }

    pp_outpic[0]->date = p_pic->date;

    /* If we are using double rate, get an additional new picture */
    if( p_vout->p_sys->b_double_rate )
    {
        while( ( pp_outpic[1] = vout_CreatePicture( p_vout->p_sys->p_vout,
                                                 0, 0, 0 ) )
                  == NULL )
        {
            if( !vlc_object_alive( p_vout ) || p_vout->b_error )
            {
                vout_DestroyPicture( p_vout->p_sys->p_vout, pp_outpic[0] );
                vlc_mutex_unlock( &p_vout->p_sys->filter_lock );
                return;
            }
            msleep( VOUT_OUTMEM_SLEEP );
        }

        /* 20ms is a bit arbitrary, but it's only for the first image we get */
        if( !p_vout->p_sys->last_date )
            pp_outpic[1]->date = p_pic->date + 20000;
        else
            pp_outpic[1]->date = (3 * p_pic->date - p_vout->p_sys->last_date) / 2;
        p_vout->p_sys->last_date = p_pic->date;
    }

    switch( p_vout->p_sys->i_mode )
    {
        case DEINTERLACE_DISCARD:
            RenderDiscard( p_vout, pp_outpic[0], p_pic, 0 );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[0] );
            break;

        case DEINTERLACE_BOB:
            RenderBob( p_vout, pp_outpic[0], p_pic, p_pic->b_top_field_first ? 0 : 1 );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[0] );
            RenderBob( p_vout, pp_outpic[1], p_pic, p_pic->b_top_field_first ? 1 : 0 );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[1] );
            break;

        case DEINTERLACE_LINEAR:
            RenderLinear( p_vout, pp_outpic[0], p_pic, p_pic->b_top_field_first ? 0 : 1 );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[0] );
            RenderLinear( p_vout, pp_outpic[1], p_pic, p_pic->b_top_field_first ? 1 : 0 );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[1] );
            break;

        case DEINTERLACE_MEAN:
            RenderMean( p_vout, pp_outpic[0], p_pic );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[0] );
            break;

        case DEINTERLACE_BLEND:
            RenderBlend( p_vout, pp_outpic[0], p_pic );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[0] );
            break;

        case DEINTERLACE_X:
            RenderX( pp_outpic[0], p_pic );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[0] );
            break;

        case DEINTERLACE_YADIF:
            RenderYadif( p_vout, pp_outpic[0], p_pic, 0, 0 );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[0] );
            break;

        case DEINTERLACE_YADIF2X:
            RenderYadif( p_vout, pp_outpic[0], p_pic, 0, p_pic->b_top_field_first ? 0 : 1 );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[0] );
            RenderYadif( p_vout, pp_outpic[1], p_pic, 1, p_pic->b_top_field_first ? 1 : 0 );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[1] );
            break;
    }
    vlc_mutex_unlock( &p_vout->p_sys->filter_lock );
}

/*****************************************************************************
 * RenderDiscard: only keep TOP or BOTTOM field, discard the other.
 *****************************************************************************/
static void RenderDiscard( vout_thread_t *p_vout,
                           picture_t *p_outpic, picture_t *p_pic, int i_field )
{
    int i_plane;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in, *p_out_end, *p_out;
        int i_increment;

        p_in = p_pic->p[i_plane].p_pixels
                   + i_field * p_pic->p[i_plane].i_pitch;

        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_visible_lines;

        switch( p_vout->render.i_chroma )
        {
        case VLC_CODEC_I420:
        case VLC_CODEC_J420:
        case VLC_CODEC_YV12:

            for( ; p_out < p_out_end ; )
            {
                vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

                p_out += p_outpic->p[i_plane].i_pitch;
                p_in += 2 * p_pic->p[i_plane].i_pitch;
            }
            break;

        case VLC_CODEC_I422:
        case VLC_CODEC_J422:

            i_increment = 2 * p_pic->p[i_plane].i_pitch;

            if( i_plane == Y_PLANE )
            {
                for( ; p_out < p_out_end ; )
                {
                    vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
                    p_out += p_outpic->p[i_plane].i_pitch;
                    vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
                    p_out += p_outpic->p[i_plane].i_pitch;
                    p_in += i_increment;
                }
            }
            else
            {
                for( ; p_out < p_out_end ; )
                {
                    vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
                    p_out += p_outpic->p[i_plane].i_pitch;
                    p_in += i_increment;
                }
            }
            break;

        default:
            break;
        }
    }
}

/*****************************************************************************
 * RenderBob: renders a BOB picture - simple copy
 *****************************************************************************/
static void RenderBob( vout_thread_t *p_vout,
                       picture_t *p_outpic, picture_t *p_pic, int i_field )
{
    int i_plane;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in, *p_out_end, *p_out;

        p_in = p_pic->p[i_plane].p_pixels;
        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_visible_lines;

        switch( p_vout->render.i_chroma )
        {
            case VLC_CODEC_I420:
            case VLC_CODEC_J420:
            case VLC_CODEC_YV12:
                /* For BOTTOM field we need to add the first line */
                if( i_field == 1 )
                {
                    vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
                    p_in += p_pic->p[i_plane].i_pitch;
                    p_out += p_outpic->p[i_plane].i_pitch;
                }

                p_out_end -= 2 * p_outpic->p[i_plane].i_pitch;

                for( ; p_out < p_out_end ; )
                {
                    vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

                    p_out += p_outpic->p[i_plane].i_pitch;

                    vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

                    p_in += 2 * p_pic->p[i_plane].i_pitch;
                    p_out += p_outpic->p[i_plane].i_pitch;
                }

                vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

                /* For TOP field we need to add the last line */
                if( i_field == 0 )
                {
                    p_in += p_pic->p[i_plane].i_pitch;
                    p_out += p_outpic->p[i_plane].i_pitch;
                    vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
                }
                break;

            case VLC_CODEC_I422:
            case VLC_CODEC_J422:
                /* For BOTTOM field we need to add the first line */
                if( i_field == 1 )
                {
                    vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
                    p_in += p_pic->p[i_plane].i_pitch;
                    p_out += p_outpic->p[i_plane].i_pitch;
                }

                p_out_end -= 2 * p_outpic->p[i_plane].i_pitch;

                if( i_plane == Y_PLANE )
                {
                    for( ; p_out < p_out_end ; )
                    {
                        vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

                        p_out += p_outpic->p[i_plane].i_pitch;

                        vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

                        p_in += 2 * p_pic->p[i_plane].i_pitch;
                        p_out += p_outpic->p[i_plane].i_pitch;
                    }
                }
                else
                {
                    for( ; p_out < p_out_end ; )
                    {
                        vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

                        p_out += p_outpic->p[i_plane].i_pitch;
                        p_in += 2 * p_pic->p[i_plane].i_pitch;
                    }
                }

                vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

                /* For TOP field we need to add the last line */
                if( i_field == 0 )
                {
                    p_in += p_pic->p[i_plane].i_pitch;
                    p_out += p_outpic->p[i_plane].i_pitch;
                    vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
                }
                break;
        }
    }
}

#define Merge p_vout->p_sys->pf_merge
#define EndMerge if(p_vout->p_sys->pf_end_merge) p_vout->p_sys->pf_end_merge

/*****************************************************************************
 * RenderLinear: BOB with linear interpolation
 *****************************************************************************/
static void RenderLinear( vout_thread_t *p_vout,
                          picture_t *p_outpic, picture_t *p_pic, int i_field )
{
    int i_plane;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in, *p_out_end, *p_out;

        p_in = p_pic->p[i_plane].p_pixels;
        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_visible_lines;

        /* For BOTTOM field we need to add the first line */
        if( i_field == 1 )
        {
            vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
            p_in += p_pic->p[i_plane].i_pitch;
            p_out += p_outpic->p[i_plane].i_pitch;
        }

        p_out_end -= 2 * p_outpic->p[i_plane].i_pitch;

        for( ; p_out < p_out_end ; )
        {
            vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

            p_out += p_outpic->p[i_plane].i_pitch;

            Merge( p_out, p_in, p_in + 2 * p_pic->p[i_plane].i_pitch,
                   p_pic->p[i_plane].i_pitch );

            p_in += 2 * p_pic->p[i_plane].i_pitch;
            p_out += p_outpic->p[i_plane].i_pitch;
        }

        vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );

        /* For TOP field we need to add the last line */
        if( i_field == 0 )
        {
            p_in += p_pic->p[i_plane].i_pitch;
            p_out += p_outpic->p[i_plane].i_pitch;
            vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
        }
    }
    EndMerge();
}

static void RenderMean( vout_thread_t *p_vout,
                        picture_t *p_outpic, picture_t *p_pic )
{
    int i_plane;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in, *p_out_end, *p_out;

        p_in = p_pic->p[i_plane].p_pixels;

        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_visible_lines;

        /* All lines: mean value */
        for( ; p_out < p_out_end ; )
        {
            Merge( p_out, p_in, p_in + p_pic->p[i_plane].i_pitch,
                   p_pic->p[i_plane].i_pitch );

            p_out += p_outpic->p[i_plane].i_pitch;
            p_in += 2 * p_pic->p[i_plane].i_pitch;
        }
    }
    EndMerge();
}

static void RenderBlend( vout_thread_t *p_vout,
                         picture_t *p_outpic, picture_t *p_pic )
{
    int i_plane;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in, *p_out_end, *p_out;

        p_in = p_pic->p[i_plane].p_pixels;

        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + p_outpic->p[i_plane].i_pitch
                             * p_outpic->p[i_plane].i_visible_lines;

        switch( p_vout->render.i_chroma )
        {
            case VLC_CODEC_I420:
            case VLC_CODEC_J420:
            case VLC_CODEC_YV12:
                /* First line: simple copy */
                vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
                p_out += p_outpic->p[i_plane].i_pitch;

                /* Remaining lines: mean value */
                for( ; p_out < p_out_end ; )
                {
                    Merge( p_out, p_in, p_in + p_pic->p[i_plane].i_pitch,
                           p_pic->p[i_plane].i_pitch );

                    p_out += p_outpic->p[i_plane].i_pitch;
                    p_in += p_pic->p[i_plane].i_pitch;
                }
                break;

            case VLC_CODEC_I422:
            case VLC_CODEC_J422:
                /* First line: simple copy */
                vlc_memcpy( p_out, p_in, p_pic->p[i_plane].i_pitch );
                p_out += p_outpic->p[i_plane].i_pitch;

                /* Remaining lines: mean value */
                if( i_plane == Y_PLANE )
                {
                    for( ; p_out < p_out_end ; )
                    {
                        Merge( p_out, p_in, p_in + p_pic->p[i_plane].i_pitch,
                               p_pic->p[i_plane].i_pitch );

                        p_out += p_outpic->p[i_plane].i_pitch;
                        p_in += p_pic->p[i_plane].i_pitch;
                    }
                }

                else
                {
                    for( ; p_out < p_out_end ; )
                    {
                        Merge( p_out, p_in, p_in + p_pic->p[i_plane].i_pitch,
                               p_pic->p[i_plane].i_pitch );

                        p_out += p_outpic->p[i_plane].i_pitch;
                        p_in += 2*p_pic->p[i_plane].i_pitch;
                    }
                }
                break;
        }
    }
    EndMerge();
}

#undef Merge

static void MergeGeneric( void *_p_dest, const void *_p_s1,
                          const void *_p_s2, size_t i_bytes )
{
    uint8_t* p_dest = (uint8_t*)_p_dest;
    const uint8_t *p_s1 = (const uint8_t *)_p_s1;
    const uint8_t *p_s2 = (const uint8_t *)_p_s2;
    uint8_t* p_end = p_dest + i_bytes - 8;

    while( p_dest < p_end )
    {
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
    }

    p_end += 8;

    while( p_dest < p_end )
    {
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
    }
}

#if defined(CAN_COMPILE_MMXEXT)
static void MergeMMXEXT( void *_p_dest, const void *_p_s1, const void *_p_s2,
                         size_t i_bytes )
{
    uint8_t* p_dest = (uint8_t*)_p_dest;
    const uint8_t *p_s1 = (const uint8_t *)_p_s1;
    const uint8_t *p_s2 = (const uint8_t *)_p_s2;
    uint8_t* p_end = p_dest + i_bytes - 8;
    while( p_dest < p_end )
    {
        __asm__  __volatile__( "movq %2,%%mm1;"
                               "pavgb %1, %%mm1;"
                               "movq %%mm1, %0" :"=m" (*p_dest):
                                                 "m" (*p_s1),
                                                 "m" (*p_s2) );
        p_dest += 8;
        p_s1 += 8;
        p_s2 += 8;
    }

    p_end += 8;

    while( p_dest < p_end )
    {
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
    }
}
#endif

#if defined(CAN_COMPILE_3DNOW)
static void Merge3DNow( void *_p_dest, const void *_p_s1, const void *_p_s2,
                        size_t i_bytes )
{
    uint8_t* p_dest = (uint8_t*)_p_dest;
    const uint8_t *p_s1 = (const uint8_t *)_p_s1;
    const uint8_t *p_s2 = (const uint8_t *)_p_s2;
    uint8_t* p_end = p_dest + i_bytes - 8;
    while( p_dest < p_end )
    {
        __asm__  __volatile__( "movq %2,%%mm1;"
                               "pavgusb %1, %%mm1;"
                               "movq %%mm1, %0" :"=m" (*p_dest):
                                                 "m" (*p_s1),
                                                 "m" (*p_s2) );
        p_dest += 8;
        p_s1 += 8;
        p_s2 += 8;
    }

    p_end += 8;

    while( p_dest < p_end )
    {
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
    }
}
#endif

#if defined(CAN_COMPILE_SSE)
static void MergeSSE2( void *_p_dest, const void *_p_s1, const void *_p_s2,
                       size_t i_bytes )
{
    uint8_t* p_dest = (uint8_t*)_p_dest;
    const uint8_t *p_s1 = (const uint8_t *)_p_s1;
    const uint8_t *p_s2 = (const uint8_t *)_p_s2;
    uint8_t* p_end;
    while( (uintptr_t)p_s1 % 16 )
    {
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
    }
    p_end = p_dest + i_bytes - 16;
    while( p_dest < p_end )
    {
        __asm__  __volatile__( "movdqu %2,%%xmm1;"
                               "pavgb %1, %%xmm1;"
                               "movdqu %%xmm1, %0" :"=m" (*p_dest):
                                                 "m" (*p_s1),
                                                 "m" (*p_s2) );
        p_dest += 16;
        p_s1 += 16;
        p_s2 += 16;
    }

    p_end += 16;

    while( p_dest < p_end )
    {
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
    }
}
#endif

#if defined(CAN_COMPILE_MMXEXT) || defined(CAN_COMPILE_SSE)
static void EndMMX( void )
{
    __asm__ __volatile__( "emms" :: );
}
#endif

#if defined(CAN_COMPILE_3DNOW)
static void End3DNow( void )
{
    __asm__ __volatile__( "femms" :: );
}
#endif

#ifdef CAN_COMPILE_C_ALTIVEC
static void MergeAltivec( void *_p_dest, const void *_p_s1,
                          const void *_p_s2, size_t i_bytes )
{
    uint8_t *p_dest = (uint8_t *)_p_dest;
    uint8_t *p_s1   = (uint8_t *)_p_s1;
    uint8_t *p_s2   = (uint8_t *)_p_s2;
    uint8_t *p_end  = p_dest + i_bytes - 15;

    /* Use C until the first 16-bytes aligned destination pixel */
    while( (uintptr_t)p_dest & 0xF )
    {
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
    }

    if( ( (int)p_s1 & 0xF ) | ( (int)p_s2 & 0xF ) )
    {
        /* Unaligned source */
        vector unsigned char s1v, s2v, destv;
        vector unsigned char s1oldv, s2oldv, s1newv, s2newv;
        vector unsigned char perm1v, perm2v;

        perm1v = vec_lvsl( 0, p_s1 );
        perm2v = vec_lvsl( 0, p_s2 );
        s1oldv = vec_ld( 0, p_s1 );
        s2oldv = vec_ld( 0, p_s2 );

        while( p_dest < p_end )
        {
            s1newv = vec_ld( 16, p_s1 );
            s2newv = vec_ld( 16, p_s2 );
            s1v    = vec_perm( s1oldv, s1newv, perm1v );
            s2v    = vec_perm( s2oldv, s2newv, perm2v );
            s1oldv = s1newv;
            s2oldv = s2newv;
            destv  = vec_avg( s1v, s2v );
            vec_st( destv, 0, p_dest );

            p_s1   += 16;
            p_s2   += 16;
            p_dest += 16;
        }
    }
    else
    {
        /* Aligned source */
        vector unsigned char s1v, s2v, destv;

        while( p_dest < p_end )
        {
            s1v   = vec_ld( 0, p_s1 );
            s2v   = vec_ld( 0, p_s2 );
            destv = vec_avg( s1v, s2v );
            vec_st( destv, 0, p_dest );

            p_s1   += 16;
            p_s2   += 16;
            p_dest += 16;
        }
    }

    p_end += 15;

    while( p_dest < p_end )
    {
        *p_dest++ = ( (uint16_t)(*p_s1++) + (uint16_t)(*p_s2++) ) >> 1;
    }
}
#endif

#ifdef __ARM_NEON__
static void MergeNEON (void *restrict out, const void *in1,
                       const void *in2, size_t n)
{
    uint8_t *outp = out;
    const uint8_t *in1p = in1;
    const uint8_t *in2p = in2;
    size_t mis = ((uintptr_t)outp) & 15;

    if (mis)
    {
        MergeGeneric (outp, in1p, in2p, mis);
        outp += mis;
        in1p += mis;
        in2p += mis;
        n -= mis;
    }

    uint8_t *end = outp + (n & ~15);

    if ((((uintptr_t)in1p)|((uintptr_t)in2p)) & 15)
        while (outp < end)
            asm volatile (
                "vld1.u8  {q0-q1}, [%[in1]]!\n"
                "vld1.u8  {q2-q3}, [%[in2]]!\n"
                "vhadd.u8 q4, q0, q2\n"
                "vld1.u8  {q6-q7}, [%[in1]]!\n"
                "vhadd.u8 q5, q1, q3\n"
                "vld1.u8  {q8-q9}, [%[in2]]!\n"
                "vhadd.u8 q10, q6, q8\n"
                "vhadd.u8 q11, q7, q9\n"
                "vst1.u8  {q4-q5}, [%[out],:128]!\n"
                "vst1.u8  {q10-q11}, [%[out],:128]!\n"
                : [out] "+r" (outp), [in1] "+r" (in1p), [in2] "+r" (in2p)
                :
                : "q0", "q1", "q2", "memory");
    else
         while (outp < end)
            asm volatile (
                "vld1.u8  {q0-q1}, [%[in1],:128]!\n"
                "vld1.u8  {q2-q3}, [%[in2],:128]!\n"
                "vhadd.u8 q4, q0, q2\n"
                "vld1.u8  {q6-q7}, [%[in1],:128]!\n"
                "vhadd.u8 q5, q1, q3\n"
                "vld1.u8  {q8-q9}, [%[in2],:128]!\n"
                "vhadd.u8 q10, q6, q8\n"
                "vhadd.u8 q11, q7, q9\n"
                "vst1.u8  {q4-q5}, [%[out],:128]!\n"
                "vst1.u8  {q10-q11}, [%[out],:128]!\n"
                : [out] "+r" (outp), [in1] "+r" (in1p), [in2] "+r" (in2p)
                :
                : "q0", "q1", "q2", "memory");
    n &= 15;
    if (n)
        MergeGeneric (outp, in1p, in2p, n);
}
#endif

/*****************************************************************************
 * RenderX: This algo works on a 8x8 block basic, it copies the top field
 * and apply a process to recreate the bottom field :
 *  If a 8x8 block is classified as :
 *   - progressive: it applies a small blend (1,6,1)
 *   - interlaced:
 *    * in the MMX version: we do a ME between the 2 fields, if there is a
 *    good match we use MC to recreate the bottom field (with a small
 *    blend (1,6,1) )
 *    * otherwise: it recreates the bottom field by an edge oriented
 *    interpolation.
  *****************************************************************************/

/* XDeint8x8Detect: detect if a 8x8 block is interlaced.
 * XXX: It need to access to 8x10
 * We use more than 8 lines to help with scrolling (text)
 * (and because XDeint8x8Frame use line 9)
 * XXX: smooth/uniform area with noise detection doesn't works well
 * but it's not really a problem because they don't have much details anyway
 */
static inline int ssd( int a ) { return a*a; }
static inline int XDeint8x8DetectC( uint8_t *src, int i_src )
{
    int y, x;
    int ff, fr;
    int fc;

    /* Detect interlacing */
    fc = 0;
    for( y = 0; y < 7; y += 2 )
    {
        ff = fr = 0;
        for( x = 0; x < 8; x++ )
        {
            fr += ssd(src[      x] - src[1*i_src+x]) +
                  ssd(src[i_src+x] - src[2*i_src+x]);
            ff += ssd(src[      x] - src[2*i_src+x]) +
                  ssd(src[i_src+x] - src[3*i_src+x]);
        }
        if( ff < 6*fr/8 && fr > 32 )
            fc++;

        src += 2*i_src;
    }

    return fc < 1 ? false : true;
}
#ifdef CAN_COMPILE_MMXEXT
static inline int XDeint8x8DetectMMXEXT( uint8_t *src, int i_src )
{

    int y, x;
    int32_t ff, fr;
    int fc;

    /* Detect interlacing */
    fc = 0;
    pxor_r2r( mm7, mm7 );
    for( y = 0; y < 9; y += 2 )
    {
        ff = fr = 0;
        pxor_r2r( mm5, mm5 );
        pxor_r2r( mm6, mm6 );
        for( x = 0; x < 8; x+=4 )
        {
            movd_m2r( src[        x], mm0 );
            movd_m2r( src[1*i_src+x], mm1 );
            movd_m2r( src[2*i_src+x], mm2 );
            movd_m2r( src[3*i_src+x], mm3 );

            punpcklbw_r2r( mm7, mm0 );
            punpcklbw_r2r( mm7, mm1 );
            punpcklbw_r2r( mm7, mm2 );
            punpcklbw_r2r( mm7, mm3 );

            movq_r2r( mm0, mm4 );

            psubw_r2r( mm1, mm0 );
            psubw_r2r( mm2, mm4 );

            psubw_r2r( mm1, mm2 );
            psubw_r2r( mm1, mm3 );

            pmaddwd_r2r( mm0, mm0 );
            pmaddwd_r2r( mm4, mm4 );
            pmaddwd_r2r( mm2, mm2 );
            pmaddwd_r2r( mm3, mm3 );
            paddd_r2r( mm0, mm2 );
            paddd_r2r( mm4, mm3 );
            paddd_r2r( mm2, mm5 );
            paddd_r2r( mm3, mm6 );
        }

        movq_r2r( mm5, mm0 );
        psrlq_i2r( 32, mm0 );
        paddd_r2r( mm0, mm5 );
        movd_r2m( mm5, fr );

        movq_r2r( mm6, mm0 );
        psrlq_i2r( 32, mm0 );
        paddd_r2r( mm0, mm6 );
        movd_r2m( mm6, ff );

        if( ff < 6*fr/8 && fr > 32 )
            fc++;

        src += 2*i_src;
    }
    return fc;
}
#endif

static inline void XDeint8x8MergeC( uint8_t *dst, int i_dst,
                                    uint8_t *src1, int i_src1,
                                    uint8_t *src2, int i_src2 )
{
    int y, x;

    /* Progressive */
    for( y = 0; y < 8; y += 2 )
    {
        memcpy( dst, src1, 8 );
        dst  += i_dst;

        for( x = 0; x < 8; x++ )
            dst[x] = (src1[x] + 6*src2[x] + src1[i_src1+x] + 4 ) >> 3;
        dst += i_dst;

        src1 += i_src1;
        src2 += i_src2;
    }
}

#ifdef CAN_COMPILE_MMXEXT
static inline void XDeint8x8MergeMMXEXT( uint8_t *dst, int i_dst,
                                         uint8_t *src1, int i_src1,
                                         uint8_t *src2, int i_src2 )
{
    static const uint64_t m_4 = INT64_C(0x0004000400040004);
    int y, x;

    /* Progressive */
    pxor_r2r( mm7, mm7 );
    for( y = 0; y < 8; y += 2 )
    {
        for( x = 0; x < 8; x +=4 )
        {
            movd_m2r( src1[x], mm0 );
            movd_r2m( mm0, dst[x] );

            movd_m2r( src2[x], mm1 );
            movd_m2r( src1[i_src1+x], mm2 );

            punpcklbw_r2r( mm7, mm0 );
            punpcklbw_r2r( mm7, mm1 );
            punpcklbw_r2r( mm7, mm2 );
            paddw_r2r( mm1, mm1 );
            movq_r2r( mm1, mm3 );
            paddw_r2r( mm3, mm3 );
            paddw_r2r( mm2, mm0 );
            paddw_r2r( mm3, mm1 );
            paddw_m2r( m_4, mm1 );
            paddw_r2r( mm1, mm0 );
            psraw_i2r( 3, mm0 );
            packuswb_r2r( mm7, mm0 );
            movd_r2m( mm0, dst[i_dst+x] );
        }
        dst += 2*i_dst;
        src1 += i_src1;
        src2 += i_src2;
    }
}

#endif

/* For debug */
static inline void XDeint8x8Set( uint8_t *dst, int i_dst, uint8_t v )
{
    int y;
    for( y = 0; y < 8; y++ )
        memset( &dst[y*i_dst], v, 8 );
}

/* XDeint8x8FieldE: Stupid deinterlacing (1,0,1) for block that miss a
 * neighbour
 * (Use 8x9 pixels)
 * TODO: a better one for the inner part.
 */
static inline void XDeint8x8FieldEC( uint8_t *dst, int i_dst,
                                     uint8_t *src, int i_src )
{
    int y, x;

    /* Interlaced */
    for( y = 0; y < 8; y += 2 )
    {
        memcpy( dst, src, 8 );
        dst += i_dst;

        for( x = 0; x < 8; x++ )
            dst[x] = (src[x] + src[2*i_src+x] ) >> 1;
        dst += 1*i_dst;
        src += 2*i_src;
    }
}
#ifdef CAN_COMPILE_MMXEXT
static inline void XDeint8x8FieldEMMXEXT( uint8_t *dst, int i_dst,
                                          uint8_t *src, int i_src )
{
    int y;

    /* Interlaced */
    for( y = 0; y < 8; y += 2 )
    {
        movq_m2r( src[0], mm0 );
        movq_r2m( mm0, dst[0] );
        dst += i_dst;

        movq_m2r( src[2*i_src], mm1 );
        pavgb_r2r( mm1, mm0 );

        movq_r2m( mm0, dst[0] );

        dst += 1*i_dst;
        src += 2*i_src;
    }
}
#endif

/* XDeint8x8Field: Edge oriented interpolation
 * (Need -4 and +5 pixels H, +1 line)
 */
static inline void XDeint8x8FieldC( uint8_t *dst, int i_dst,
                                    uint8_t *src, int i_src )
{
    int y, x;

    /* Interlaced */
    for( y = 0; y < 8; y += 2 )
    {
        memcpy( dst, src, 8 );
        dst += i_dst;

        for( x = 0; x < 8; x++ )
        {
            uint8_t *src2 = &src[2*i_src];
            /* I use 8 pixels just to match the MMX version, but it's overkill
             * 5 would be enough (less isn't good) */
            const int c0 = abs(src[x-4]-src2[x-2]) + abs(src[x-3]-src2[x-1]) +
                           abs(src[x-2]-src2[x+0]) + abs(src[x-1]-src2[x+1]) +
                           abs(src[x+0]-src2[x+2]) + abs(src[x+1]-src2[x+3]) +
                           abs(src[x+2]-src2[x+4]) + abs(src[x+3]-src2[x+5]);

            const int c1 = abs(src[x-3]-src2[x-3]) + abs(src[x-2]-src2[x-2]) +
                           abs(src[x-1]-src2[x-1]) + abs(src[x+0]-src2[x+0]) +
                           abs(src[x+1]-src2[x+1]) + abs(src[x+2]-src2[x+2]) +
                           abs(src[x+3]-src2[x+3]) + abs(src[x+4]-src2[x+4]);

            const int c2 = abs(src[x-2]-src2[x-4]) + abs(src[x-1]-src2[x-3]) +
                           abs(src[x+0]-src2[x-2]) + abs(src[x+1]-src2[x-1]) +
                           abs(src[x+2]-src2[x+0]) + abs(src[x+3]-src2[x+1]) +
                           abs(src[x+4]-src2[x+2]) + abs(src[x+5]-src2[x+3]);

            if( c0 < c1 && c1 <= c2 )
                dst[x] = (src[x-1] + src2[x+1]) >> 1;
            else if( c2 < c1 && c1 <= c0 )
                dst[x] = (src[x+1] + src2[x-1]) >> 1;
            else
                dst[x] = (src[x+0] + src2[x+0]) >> 1;
        }

        dst += 1*i_dst;
        src += 2*i_src;
    }
}
#ifdef CAN_COMPILE_MMXEXT
static inline void XDeint8x8FieldMMXEXT( uint8_t *dst, int i_dst,
                                         uint8_t *src, int i_src )
{
    int y, x;

    /* Interlaced */
    for( y = 0; y < 8; y += 2 )
    {
        memcpy( dst, src, 8 );
        dst += i_dst;

        for( x = 0; x < 8; x++ )
        {
            uint8_t *src2 = &src[2*i_src];
            int32_t c0, c1, c2;

            movq_m2r( src[x-2], mm0 );
            movq_m2r( src[x-3], mm1 );
            movq_m2r( src[x-4], mm2 );

            psadbw_m2r( src2[x-4], mm0 );
            psadbw_m2r( src2[x-3], mm1 );
            psadbw_m2r( src2[x-2], mm2 );

            movd_r2m( mm0, c2 );
            movd_r2m( mm1, c1 );
            movd_r2m( mm2, c0 );

            if( c0 < c1 && c1 <= c2 )
                dst[x] = (src[x-1] + src2[x+1]) >> 1;
            else if( c2 < c1 && c1 <= c0 )
                dst[x] = (src[x+1] + src2[x-1]) >> 1;
            else
                dst[x] = (src[x+0] + src2[x+0]) >> 1;
        }

        dst += 1*i_dst;
        src += 2*i_src;
    }
}
#endif

/* NxN arbitray size (and then only use pixel in the NxN block)
 */
static inline int XDeintNxNDetect( uint8_t *src, int i_src,
                                   int i_height, int i_width )
{
    int y, x;
    int ff, fr;
    int fc;


    /* Detect interlacing */
    /* FIXME way too simple, need to be more like XDeint8x8Detect */
    ff = fr = 0;
    fc = 0;
    for( y = 0; y < i_height - 2; y += 2 )
    {
        const uint8_t *s = &src[y*i_src];
        for( x = 0; x < i_width; x++ )
        {
            fr += ssd(s[      x] - s[1*i_src+x]);
            ff += ssd(s[      x] - s[2*i_src+x]);
        }
        if( ff < fr && fr > i_width / 2 )
            fc++;
    }

    return fc < 2 ? false : true;
}

static inline void XDeintNxNFrame( uint8_t *dst, int i_dst,
                                   uint8_t *src, int i_src,
                                   int i_width, int i_height )
{
    int y, x;

    /* Progressive */
    for( y = 0; y < i_height; y += 2 )
    {
        memcpy( dst, src, i_width );
        dst += i_dst;

        if( y < i_height - 2 )
        {
            for( x = 0; x < i_width; x++ )
                dst[x] = (src[x] + 2*src[1*i_src+x] + src[2*i_src+x] + 2 ) >> 2;
        }
        else
        {
            /* Blend last line */
            for( x = 0; x < i_width; x++ )
                dst[x] = (src[x] + src[1*i_src+x] ) >> 1;
        }
        dst += 1*i_dst;
        src += 2*i_src;
    }
}

static inline void XDeintNxNField( uint8_t *dst, int i_dst,
                                   uint8_t *src, int i_src,
                                   int i_width, int i_height )
{
    int y, x;

    /* Interlaced */
    for( y = 0; y < i_height; y += 2 )
    {
        memcpy( dst, src, i_width );
        dst += i_dst;

        if( y < i_height - 2 )
        {
            for( x = 0; x < i_width; x++ )
                dst[x] = (src[x] + src[2*i_src+x] ) >> 1;
        }
        else
        {
            /* Blend last line */
            for( x = 0; x < i_width; x++ )
                dst[x] = (src[x] + src[i_src+x]) >> 1;
        }
        dst += 1*i_dst;
        src += 2*i_src;
    }
}

static inline void XDeintNxN( uint8_t *dst, int i_dst, uint8_t *src, int i_src,
                              int i_width, int i_height )
{
    if( XDeintNxNDetect( src, i_src, i_width, i_height ) )
        XDeintNxNField( dst, i_dst, src, i_src, i_width, i_height );
    else
        XDeintNxNFrame( dst, i_dst, src, i_src, i_width, i_height );
}


static inline int median( int a, int b, int c )
{
    int min = a, max =a;
    if( b < min )
        min = b;
    else
        max = b;

    if( c < min )
        min = c;
    else if( c > max )
        max = c;

    return a + b + c - min - max;
}


/* XDeintBand8x8:
 */
static inline void XDeintBand8x8C( uint8_t *dst, int i_dst,
                                   uint8_t *src, int i_src,
                                   const int i_mbx, int i_modx )
{
    int x;

    for( x = 0; x < i_mbx; x++ )
    {
        int s;
        if( ( s = XDeint8x8DetectC( src, i_src ) ) )
        {
            if( x == 0 || x == i_mbx - 1 )
                XDeint8x8FieldEC( dst, i_dst, src, i_src );
            else
                XDeint8x8FieldC( dst, i_dst, src, i_src );
        }
        else
        {
            XDeint8x8MergeC( dst, i_dst,
                             &src[0*i_src], 2*i_src,
                             &src[1*i_src], 2*i_src );
        }

        dst += 8;
        src += 8;
    }

    if( i_modx )
        XDeintNxN( dst, i_dst, src, i_src, i_modx, 8 );
}
#ifdef CAN_COMPILE_MMXEXT
static inline void XDeintBand8x8MMXEXT( uint8_t *dst, int i_dst,
                                        uint8_t *src, int i_src,
                                        const int i_mbx, int i_modx )
{
    int x;

    /* Reset current line */
    for( x = 0; x < i_mbx; x++ )
    {
        int s;
        if( ( s = XDeint8x8DetectMMXEXT( src, i_src ) ) )
        {
            if( x == 0 || x == i_mbx - 1 )
                XDeint8x8FieldEMMXEXT( dst, i_dst, src, i_src );
            else
                XDeint8x8FieldMMXEXT( dst, i_dst, src, i_src );
        }
        else
        {
            XDeint8x8MergeMMXEXT( dst, i_dst,
                                  &src[0*i_src], 2*i_src,
                                  &src[1*i_src], 2*i_src );
        }

        dst += 8;
        src += 8;
    }

    if( i_modx )
        XDeintNxN( dst, i_dst, src, i_src, i_modx, 8 );
}
#endif

static void RenderX( picture_t *p_outpic, picture_t *p_pic )
{
    int i_plane;

    /* Copy image and skip lines */
    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        const int i_mby = ( p_outpic->p[i_plane].i_visible_lines + 7 )/8 - 1;
        const int i_mbx = p_outpic->p[i_plane].i_visible_pitch/8;

        const int i_mody = p_outpic->p[i_plane].i_visible_lines - 8*i_mby;
        const int i_modx = p_outpic->p[i_plane].i_visible_pitch - 8*i_mbx;

        const int i_dst = p_outpic->p[i_plane].i_pitch;
        const int i_src = p_pic->p[i_plane].i_pitch;

        int y, x;

        for( y = 0; y < i_mby; y++ )
        {
            uint8_t *dst = &p_outpic->p[i_plane].p_pixels[8*y*i_dst];
            uint8_t *src = &p_pic->p[i_plane].p_pixels[8*y*i_src];

#ifdef CAN_COMPILE_MMXEXT
            if( vlc_CPU() & CPU_CAPABILITY_MMXEXT )
                XDeintBand8x8MMXEXT( dst, i_dst, src, i_src, i_mbx, i_modx );
            else
#endif
                XDeintBand8x8C( dst, i_dst, src, i_src, i_mbx, i_modx );
        }

        /* Last line (C only)*/
        if( i_mody )
        {
            uint8_t *dst = &p_outpic->p[i_plane].p_pixels[8*y*i_dst];
            uint8_t *src = &p_pic->p[i_plane].p_pixels[8*y*i_src];

            for( x = 0; x < i_mbx; x++ )
            {
                XDeintNxN( dst, i_dst, src, i_src, 8, i_mody );

                dst += 8;
                src += 8;
            }

            if( i_modx )
                XDeintNxN( dst, i_dst, src, i_src, i_modx, i_mody );
        }
    }

#ifdef CAN_COMPILE_MMXEXT
    if( vlc_CPU() & CPU_CAPABILITY_MMXEXT )
        emms();
#endif
}

/*****************************************************************************
 * Yadif (Yet Another DeInterlacing Filter).
 *****************************************************************************/
/* */
struct vf_priv_s {
    /*
     * 0: Output 1 frame for each frame.
     * 1: Output 1 frame for each field.
     * 2: Like 0 but skips spatial interlacing check.
     * 3: Like 1 but skips spatial interlacing check.
     *
     * In vlc, only & 0x02 has meaning, as we do the & 0x01 ourself.
     */
    int mode;
};

/* I am unsure it is the right one */
typedef intptr_t x86_reg;

#define FFABS(a) ((a) >= 0 ? (a) : (-(a)))
#define FFMAX(a,b)      __MAX(a,b)
#define FFMAX3(a,b,c)   FFMAX(FFMAX(a,b),c)
#define FFMIN(a,b)      __MIN(a,b)
#define FFMIN3(a,b,c)   FFMIN(FFMIN(a,b),c)

/* yadif.h comes from vf_yadif.c of mplayer project */
#include "yadif.h"

static void RenderYadif( vout_thread_t *p_vout, picture_t *p_dst, picture_t *p_src, int i_order, int i_field )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    /* */
    assert( i_order == 0 || i_order == 1 );
    assert( i_field == 0 || i_field == 1 );

    if( i_order == 0 )
    {
        /* Duplicate the picture
         * TODO when the vout rework is finished, picture_Hold() might be enough
         * but becarefull, the pitches must match */
        picture_t *p_dup = picture_NewFromFormat( &p_src->format );
        if( p_dup )
            picture_Copy( p_dup, p_src );

        /* Slide the history */
        if( p_sys->pp_history[0] )
            picture_Release( p_sys->pp_history[0]  );
        for( int i = 1; i < HISTORY_SIZE; i++ )
            p_sys->pp_history[i-1] = p_sys->pp_history[i];
        p_sys->pp_history[HISTORY_SIZE-1] = p_dup;
    }

    /* As the pitches must match, use ONLY pictures coming from picture_New()! */
    picture_t *p_prev = p_sys->pp_history[0];
    picture_t *p_cur  = p_sys->pp_history[1];
    picture_t *p_next = p_sys->pp_history[2];

    /* Filter if we have all the pictures we need */
    if( p_prev && p_cur && p_next )
    {
        /* */
        void (*filter)(struct vf_priv_s *p, uint8_t *dst, uint8_t *prev, uint8_t *cur, uint8_t *next, int w, int refs, int parity);
#if defined(HAVE_YADIF_SSE2)
        if( vlc_CPU() & CPU_CAPABILITY_SSE2 )
            filter = yadif_filter_line_mmx2;
        else
#endif
            filter = yadif_filter_line_c;

        for( int n = 0; n < p_dst->i_planes; n++ )
        {
            const plane_t *prevp = &p_prev->p[n];
            const plane_t *curp  = &p_cur->p[n];
            const plane_t *nextp = &p_next->p[n];
            plane_t *dstp        = &p_dst->p[n];

            for( int y = 1; y < dstp->i_visible_lines - 1; y++ )
            {
                if( (y % 2) == i_field )
                {
                    vlc_memcpy( &dstp->p_pixels[y * dstp->i_pitch],
                                &curp->p_pixels[y * curp->i_pitch], dstp->i_visible_pitch );
                }
                else
                {
                    struct vf_priv_s cfg;
                    /* Spatial checks only when enough data */
                    cfg.mode = (y >= 2 && y < dstp->i_visible_lines - 2) ? 0 : 2;

                    assert( prevp->i_pitch == curp->i_pitch && curp->i_pitch == nextp->i_pitch );
                    filter( &cfg,
                            &dstp->p_pixels[y * dstp->i_pitch],
                            &prevp->p_pixels[y * prevp->i_pitch],
                            &curp->p_pixels[y * curp->i_pitch],
                            &nextp->p_pixels[y * nextp->i_pitch],
                            dstp->i_visible_pitch,
                            curp->i_pitch,
                            (i_field ^ (i_order == i_field)) & 1 );
                }

                /* We duplicate the first and last lines */
                if( y == 1 )
                    vlc_memcpy(&dstp->p_pixels[(y-1) * dstp->i_pitch], &dstp->p_pixels[y * dstp->i_pitch], dstp->i_pitch);
                else if( y == dstp->i_visible_lines - 2 )
                    vlc_memcpy(&dstp->p_pixels[(y+1) * dstp->i_pitch], &dstp->p_pixels[y * dstp->i_pitch], dstp->i_pitch);
            }
        }

        /* */
        p_dst->date = (p_next->date - p_cur->date) * i_order / 2 + p_cur->date;
    }
    else
    {
        /* Fallback to something simple
         * XXX it is wrong when we have 2 pictures, we should not output a picture */
        RenderX( p_dst, p_src );
    }
}

/*****************************************************************************
 * FilterCallback: called when changing the deinterlace method on the fly.
 *****************************************************************************/
static int FilterCallback( vlc_object_t *p_this, char const *psz_cmd,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    VLC_UNUSED(psz_cmd); VLC_UNUSED(p_data); VLC_UNUSED(oldval);
    vout_thread_t * p_vout = (vout_thread_t *)p_this;
    vout_sys_t *p_sys = p_vout->p_sys;

    msg_Dbg( p_vout, "using %s deinterlace mode", newval.psz_string );

    vlc_mutex_lock( &p_sys->filter_lock );
    const bool b_old_half_height = p_sys->b_half_height;

    SetFilterMethod( p_vout, newval.psz_string );

    if( !b_old_half_height == !p_sys->b_half_height )
    {
        vlc_mutex_unlock( &p_sys->filter_lock );
        return VLC_SUCCESS;
    }

    /* We need to kill the old vout */
    if( p_sys->p_vout )
    {
        vout_filter_DelChild( p_vout, p_sys->p_vout, MouseEvent );
        vout_CloseAndRelease( p_sys->p_vout );
    }

    /* Try to open a new video output */
    p_sys->p_vout = SpawnRealVout( p_vout );

    if( p_sys->p_vout == NULL )
    {
        /* Everything failed */
        msg_Err( p_vout, "cannot open vout, aborting" );

        vlc_mutex_unlock( &p_sys->filter_lock );
        return VLC_EGENERIC;
    }

    vout_filter_AddChild( p_vout, p_sys->p_vout, MouseEvent );

    vlc_mutex_unlock( &p_sys->filter_lock );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * video filter2 functions
 *****************************************************************************/
static picture_t *Deinterlace( filter_t *p_filter, picture_t *p_pic )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_filter->p_sys;
    picture_t *p_pic_dst;

    /* Request output picture */
    p_pic_dst = filter_NewPicture( p_filter );
    if( p_pic_dst == NULL )
    {
        picture_Release( p_pic );
        return NULL;
    }

    switch( p_vout->p_sys->i_mode )
    {
        case DEINTERLACE_DISCARD:
            RenderDiscard( p_vout, p_pic_dst, p_pic, 0 );
            break;

        case DEINTERLACE_BOB:
#if 0
            RenderBob( p_vout, pp_outpic[0], p_pic, 0 );
            RenderBob( p_vout, pp_outpic[1], p_pic, 1 );
            break;
#endif

        case DEINTERLACE_LINEAR:
#if 0
            RenderLinear( p_vout, pp_outpic[0], p_pic, 0 );
            RenderLinear( p_vout, pp_outpic[1], p_pic, 1 );
#endif
            msg_Err( p_vout, "doubling the frame rate is not supported yet" );
            picture_Release( p_pic_dst );
            picture_Release( p_pic );
            return NULL;

        case DEINTERLACE_MEAN:
            RenderMean( p_vout, p_pic_dst, p_pic );
            break;

        case DEINTERLACE_BLEND:
            RenderBlend( p_vout, p_pic_dst, p_pic );
            break;

        case DEINTERLACE_X:
            RenderX( p_pic_dst, p_pic );
            break;

        case DEINTERLACE_YADIF:
            msg_Err( p_vout, "delaying frames is not supported yet" );
            picture_Release( p_pic_dst );
            picture_Release( p_pic );
            return NULL;

        case DEINTERLACE_YADIF2X:
            msg_Err( p_vout, "doubling the frame rate is not supported yet" );
            picture_Release( p_pic_dst );
            picture_Release( p_pic );
            return NULL;
    }

    picture_CopyProperties( p_pic_dst, p_pic );
    p_pic_dst->b_progressive = true;

    picture_Release( p_pic );
    return p_pic_dst;
}

/*****************************************************************************
 * OpenFilter:
 *****************************************************************************/
static int OpenFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    vout_thread_t *p_vout;
    vlc_value_t val;

    if( !IsChromaSupported( p_filter->fmt_in.video.i_chroma ) )
        return VLC_EGENERIC;

    /* Impossible to use VLC_OBJECT_VOUT here because it would be used
     * by spu filters */
    p_vout = vlc_object_create( p_filter, sizeof(vout_thread_t) );
    vlc_object_attach( p_vout, p_filter );
    p_filter->p_sys = (filter_sys_t *)p_vout;
    p_vout->render.i_chroma = p_filter->fmt_in.video.i_chroma;

    config_ChainParse( p_filter, FILTER_CFG_PREFIX, ppsz_filter_options,
                   p_filter->p_cfg );
    var_Get( p_filter, FILTER_CFG_PREFIX "mode", &val );

    var_Create( p_filter, "filter-deinterlace-mode", VLC_VAR_STRING );
    var_Set( p_filter, "filter-deinterlace-mode", val );
    free( val.psz_string );

    if( Create( VLC_OBJECT(p_vout) ) != VLC_SUCCESS )
    {
        vlc_object_detach( p_vout );
        vlc_object_release( p_vout );
        return VLC_EGENERIC;
    }

    video_format_t fmt;
    GetOutputFormat( p_vout, &fmt, &p_filter->fmt_in.video );
    if( !p_filter->b_allow_fmt_out_change &&
        ( fmt.i_chroma != p_filter->fmt_in.video.i_chroma ||
          fmt.i_height != p_filter->fmt_in.video.i_height ) )
    {
        CloseFilter( VLC_OBJECT(p_filter) );
        return VLC_EGENERIC;
    }
    p_filter->fmt_out.video = fmt;
    p_filter->fmt_out.i_codec = fmt.i_chroma;
    p_filter->pf_video_filter = Deinterlace;

    msg_Dbg( p_filter, "deinterlacing" );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseFilter: clean up the filter
 *****************************************************************************/
static void CloseFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    vout_thread_t *p_vout = (vout_thread_t *)p_filter->p_sys;

    Destroy( VLC_OBJECT(p_vout) );
    vlc_object_detach( p_vout );
    vlc_object_release( p_vout );
}

