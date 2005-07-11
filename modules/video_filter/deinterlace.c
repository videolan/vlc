/*****************************************************************************
 * deinterlace.c : deinterlacer plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001, 2002, 2003 the VideoLAN team
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/sout.h>
#include "vlc_filter.h"

#ifdef HAVE_ALTIVEC_H
#   include <altivec.h>
#endif

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

/*****************************************************************************
 * Local protypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );

static void RenderDiscard( vout_thread_t *, picture_t *, picture_t *, int );
static void RenderBob    ( vout_thread_t *, picture_t *, picture_t *, int );
static void RenderMean   ( vout_thread_t *, picture_t *, picture_t * );
static void RenderBlend  ( vout_thread_t *, picture_t *, picture_t * );
static void RenderLinear ( vout_thread_t *, picture_t *, picture_t *, int );
static void RenderX      ( vout_thread_t *, picture_t *, picture_t * );

static void MergeGeneric ( void *, const void *, const void *, size_t );
#if defined(CAN_COMPILE_C_ALTIVEC)
static void MergeAltivec ( void *, const void *, const void *, size_t );
#endif
#if defined(CAN_COMPILE_MMXEXT)
static void MergeMMX     ( void *, const void *, const void *, size_t );
#endif
#if defined(CAN_COMPILE_SSE)
static void MergeSSE2    ( void *, const void *, const void *, size_t );
#endif
#if defined(CAN_COMPILE_MMXEXT) || defined(CAN_COMPILE_SSE)
static void EndMMX       ( void );
#endif

static int  SendEvents   ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );

static void SetFilterMethod( vout_thread_t *p_vout, char *psz_method );
static vout_thread_t *SpawnRealVout( vout_thread_t *p_vout );

static int OpenFilter( vlc_object_t *p_this );
static void CloseFilter( vlc_object_t *p_this );

/*****************************************************************************
 * Callback prototypes
 *****************************************************************************/
static int FilterCallback ( vlc_object_t *, char const *,
                            vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define MODE_TEXT N_("Deinterlace mode")
#define MODE_LONGTEXT N_("You can choose the default deinterlace mode")

#define FILTER_CFG_PREFIX "sout-deinterlace-"

static char *mode_list[] = { "discard", "blend", "mean", "bob", "linear", "x" };
static char *mode_list_text[] = { N_("Discard"), N_("Blend"), N_("Mean"),
                                  N_("Bob"), N_("Linear"), "X" };

vlc_module_begin();
    set_description( _("Deinterlacing video filter") );
    set_shortname( N_("Deinterlace" ));
    set_capability( "video filter", 0 );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER );

    add_string( "deinterlace-mode", "discard", NULL, MODE_TEXT,
                MODE_LONGTEXT, VLC_FALSE );
        change_string_list( mode_list, mode_list_text, 0 );

    add_shortcut( "deinterlace" );
    set_callbacks( Create, Destroy );

    add_submodule();
    set_capability( "video filter2", 0 );
    add_string( FILTER_CFG_PREFIX "mode", "blend", NULL, MODE_TEXT,
                MODE_LONGTEXT, VLC_FALSE );
        change_string_list( mode_list, mode_list_text, 0 );
    set_callbacks( OpenFilter, CloseFilter );
vlc_module_end();

static const char *ppsz_filter_options[] = {
    "mode", NULL
};

/*****************************************************************************
 * vout_sys_t: Deinterlace video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Deinterlace specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    int        i_mode;        /* Deinterlace mode */
    vlc_bool_t b_double_rate; /* Shall we double the framerate? */

    mtime_t    last_date;
    mtime_t    next_date;

    vout_thread_t *p_vout;

    vlc_mutex_t filter_lock;

    void (*pf_merge) ( void *, const void *, const void *, size_t );
    void (*pf_end_merge) ( void );
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
    vlc_value_t val;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return VLC_ENOMEM;
    }

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = Render;
    p_vout->pf_display = NULL;
    p_vout->pf_control = Control;

    p_vout->p_sys->i_mode = DEINTERLACE_DISCARD;
    p_vout->p_sys->b_double_rate = VLC_FALSE;
    p_vout->p_sys->last_date = 0;
    p_vout->p_sys->p_vout = 0;
    vlc_mutex_init( p_vout, &p_vout->p_sys->filter_lock );

#if defined(CAN_COMPILE_C_ALTIVEC)
    if( p_vout->p_libvlc->i_cpu & CPU_CAPABILITY_ALTIVEC )
    {
        p_vout->p_sys->pf_merge = MergeAltivec;
        p_vout->p_sys->pf_end_merge = NULL;
    }
    else
#endif
#if defined(CAN_COMPILE_SSE)
    if( p_vout->p_libvlc->i_cpu & CPU_CAPABILITY_SSE2 )
    {
        p_vout->p_sys->pf_merge = MergeSSE2;
        p_vout->p_sys->pf_end_merge = EndMMX;
    }
    else
#endif
#if defined(CAN_COMPILE_MMXEXT)
    if( p_vout->p_libvlc->i_cpu & CPU_CAPABILITY_MMX )
    {
        p_vout->p_sys->pf_merge = MergeMMX;
        p_vout->p_sys->pf_end_merge = EndMMX;
    }
    else
#endif
    {
        p_vout->p_sys->pf_merge = MergeGeneric;
        p_vout->p_sys->pf_end_merge = NULL;
    }

    /* Look what method was requested */
    var_Create( p_vout, "deinterlace-mode", VLC_VAR_STRING );
    var_Change( p_vout, "deinterlace-mode", VLC_VAR_INHERITVALUE, &val, NULL );

    if( val.psz_string == NULL )
    {
        msg_Err( p_vout, "configuration variable deinterlace-mode empty" );
        msg_Err( p_vout, "no deinterlace mode provided, using \"discard\"" );

        val.psz_string = strdup( "discard" );
    }

    msg_Dbg( p_vout, "using %s deinterlace mode", val.psz_string );

    SetFilterMethod( p_vout, val.psz_string );

    free( val.psz_string );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SetFilterMethod: setup the deinterlace method to use.
 *****************************************************************************/
static void SetFilterMethod( vout_thread_t *p_vout, char *psz_method )
{
    if( !strcmp( psz_method, "discard" ) )
    {
        p_vout->p_sys->i_mode = DEINTERLACE_DISCARD;
        p_vout->p_sys->b_double_rate = VLC_FALSE;
    }
    else if( !strcmp( psz_method, "mean" ) )
    {
        p_vout->p_sys->i_mode = DEINTERLACE_MEAN;
        p_vout->p_sys->b_double_rate = VLC_FALSE;
    }
    else if( !strcmp( psz_method, "blend" )
             || !strcmp( psz_method, "average" )
             || !strcmp( psz_method, "combine-fields" ) )
    {
        p_vout->p_sys->i_mode = DEINTERLACE_BLEND;
        p_vout->p_sys->b_double_rate = VLC_FALSE;
    }
    else if( !strcmp( psz_method, "bob" )
             || !strcmp( psz_method, "progressive-scan" ) )
    {
        p_vout->p_sys->i_mode = DEINTERLACE_BOB;
        p_vout->p_sys->b_double_rate = VLC_TRUE;
    }
    else if( !strcmp( psz_method, "linear" ) )
    {
        p_vout->p_sys->i_mode = DEINTERLACE_LINEAR;
        p_vout->p_sys->b_double_rate = VLC_TRUE;
    }
    else if( !strcmp( psz_method, "x" ) )
    {
        p_vout->p_sys->i_mode = DEINTERLACE_X;
        p_vout->p_sys->b_double_rate = VLC_FALSE;
    }
    else
    {
        msg_Err( p_vout, "no valid deinterlace mode provided, "
                 "using \"discard\"" );
    }

    msg_Dbg( p_vout, "using %s deinterlace method", psz_method );
}

/*****************************************************************************
 * Init: initialize Deinterlace video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;

    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure, full of directbuffers since we want
     * the decoder to output directly to our structures. */
    switch( p_vout->render.i_chroma )
    {
        case VLC_FOURCC('I','4','2','0'):
        case VLC_FOURCC('I','Y','U','V'):
        case VLC_FOURCC('Y','V','1','2'):
        case VLC_FOURCC('I','4','2','2'):
            p_vout->output.i_chroma = p_vout->render.i_chroma;
            p_vout->output.i_width  = p_vout->render.i_width;
            p_vout->output.i_height = p_vout->render.i_height;
            p_vout->output.i_aspect = p_vout->render.i_aspect;
            break;

        default:
            return VLC_EGENERIC; /* unknown chroma */
            break;
    }

    /* Try to open the real video output */
    p_vout->p_sys->p_vout = SpawnRealVout( p_vout );

    if( p_vout->p_sys->p_vout == NULL )
    {
        /* Everything failed */
        msg_Err( p_vout, "cannot open vout, aborting" );

        return VLC_EGENERIC;
    }

    var_AddCallback( p_vout, "deinterlace-mode", FilterCallback, NULL );

    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );

    ADD_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );

    ADD_PARENT_CALLBACKS( SendEventsToChild );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SpawnRealVout: spawn the real video output.
 *****************************************************************************/
static vout_thread_t *SpawnRealVout( vout_thread_t *p_vout )
{
    vout_thread_t *p_real_vout = NULL;
    video_format_t fmt = {0};

    msg_Dbg( p_vout, "spawning the real video output" );

    fmt.i_width = fmt.i_visible_width = p_vout->output.i_width;
    fmt.i_height = fmt.i_visible_height = p_vout->output.i_height;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    fmt.i_chroma = p_vout->output.i_chroma;
    fmt.i_aspect = p_vout->output.i_aspect;
    fmt.i_sar_num = p_vout->output.i_aspect * fmt.i_height / fmt.i_width;
    fmt.i_sar_den = VOUT_ASPECT_FACTOR;

    switch( p_vout->render.i_chroma )
    {
    case VLC_FOURCC('I','4','2','0'):
    case VLC_FOURCC('I','Y','U','V'):
    case VLC_FOURCC('Y','V','1','2'):
        switch( p_vout->p_sys->i_mode )
        {
        case DEINTERLACE_MEAN:
        case DEINTERLACE_DISCARD:
            fmt.i_height = fmt.i_visible_height = p_vout->output.i_height / 2;
            p_real_vout = vout_Create( p_vout, &fmt );
            break;

        case DEINTERLACE_BOB:
        case DEINTERLACE_BLEND:
        case DEINTERLACE_LINEAR:
        case DEINTERLACE_X:
            p_real_vout = vout_Create( p_vout, &fmt );
            break;
        }
        break;

    case VLC_FOURCC('I','4','2','2'):
        fmt.i_chroma = VLC_FOURCC('I','4','2','0');
        p_real_vout = vout_Create( p_vout, &fmt );
        break;

    default:
        break;
    }

    return p_real_vout;
}

/*****************************************************************************
 * End: terminate Deinterlace video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    int i_index;

    /* Free the fake output buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        free( PP_OUTPUTPICTURE[ i_index ]->p_data_orig );
    }

    if( p_vout->p_sys->p_vout )
    {
        DEL_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );
        vlc_object_detach( p_vout->p_sys->p_vout );
        vout_Destroy( p_vout->p_sys->p_vout );
    }

    DEL_PARENT_CALLBACKS( SendEventsToChild );
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

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Deinterlace image,
 * waits until it is displayed and switch the two rendering buffers, preparing
 * next frame.
 *****************************************************************************/
static void Render ( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *pp_outpic[2];

    pp_outpic[0] = pp_outpic[1] = NULL;

    vlc_mutex_lock( &p_vout->p_sys->filter_lock );

    /* Get a new picture */
    while( ( pp_outpic[0] = vout_CreatePicture( p_vout->p_sys->p_vout,
                                                0, 0, 0 ) )
              == NULL )
    {
        if( p_vout->b_die || p_vout->b_error )
        {
            vlc_mutex_unlock( &p_vout->p_sys->filter_lock );
            return;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }

    vout_DatePicture( p_vout->p_sys->p_vout, pp_outpic[0], p_pic->date );

    /* If we are using double rate, get an additional new picture */
    if( p_vout->p_sys->b_double_rate )
    {
        while( ( pp_outpic[1] = vout_CreatePicture( p_vout->p_sys->p_vout,
                                                 0, 0, 0 ) )
                  == NULL )
        {
            if( p_vout->b_die || p_vout->b_error )
            {
                vout_DestroyPicture( p_vout->p_sys->p_vout, pp_outpic[0] );
                vlc_mutex_unlock( &p_vout->p_sys->filter_lock );
                return;
            }
            msleep( VOUT_OUTMEM_SLEEP );
        }

        /* 20ms is a bit arbitrary, but it's only for the first image we get */
        if( !p_vout->p_sys->last_date )
        {
            vout_DatePicture( p_vout->p_sys->p_vout, pp_outpic[1],
                              p_pic->date + 20000 );
        }
        else
        {
            vout_DatePicture( p_vout->p_sys->p_vout, pp_outpic[1],
                      (3 * p_pic->date - p_vout->p_sys->last_date) / 2 );
        }
        p_vout->p_sys->last_date = p_pic->date;
    }

    switch( p_vout->p_sys->i_mode )
    {
        case DEINTERLACE_DISCARD:
            RenderDiscard( p_vout, pp_outpic[0], p_pic, 0 );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[0] );
            break;

        case DEINTERLACE_BOB:
            RenderBob( p_vout, pp_outpic[0], p_pic, 0 );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[0] );
            RenderBob( p_vout, pp_outpic[1], p_pic, 1 );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[1] );
            break;

        case DEINTERLACE_LINEAR:
            RenderLinear( p_vout, pp_outpic[0], p_pic, 0 );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[0] );
            RenderLinear( p_vout, pp_outpic[1], p_pic, 1 );
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
            RenderX( p_vout, pp_outpic[0], p_pic );
            vout_DisplayPicture( p_vout->p_sys->p_vout, pp_outpic[0] );
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
        case VLC_FOURCC('I','4','2','0'):
        case VLC_FOURCC('I','Y','U','V'):
        case VLC_FOURCC('Y','V','1','2'):

            for( ; p_out < p_out_end ; )
            {
                p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                          p_pic->p[i_plane].i_pitch );

                p_out += p_pic->p[i_plane].i_pitch;
                p_in += 2 * p_pic->p[i_plane].i_pitch;
            }
            break;

        case VLC_FOURCC('I','4','2','2'):

            i_increment = 2 * p_pic->p[i_plane].i_pitch;

            if( i_plane == Y_PLANE )
            {
                for( ; p_out < p_out_end ; )
                {
                    p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                              p_pic->p[i_plane].i_pitch );
                    p_out += p_pic->p[i_plane].i_pitch;
                    p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                              p_pic->p[i_plane].i_pitch );
                    p_out += p_pic->p[i_plane].i_pitch;
                    p_in += i_increment;
                }
            }
            else
            {
                for( ; p_out < p_out_end ; )
                {
                    p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                              p_pic->p[i_plane].i_pitch );
                    p_out += p_pic->p[i_plane].i_pitch;
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
            case VLC_FOURCC('I','4','2','0'):
            case VLC_FOURCC('I','Y','U','V'):
            case VLC_FOURCC('Y','V','1','2'):
                /* For BOTTOM field we need to add the first line */
                if( i_field == 1 )
                {
                    p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                              p_pic->p[i_plane].i_pitch );
                    p_in += p_pic->p[i_plane].i_pitch;
                    p_out += p_pic->p[i_plane].i_pitch;
                }

                p_out_end -= 2 * p_outpic->p[i_plane].i_pitch;

                for( ; p_out < p_out_end ; )
                {
                    p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                              p_pic->p[i_plane].i_pitch );

                    p_out += p_pic->p[i_plane].i_pitch;

                    p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                              p_pic->p[i_plane].i_pitch );

                    p_in += 2 * p_pic->p[i_plane].i_pitch;
                    p_out += p_pic->p[i_plane].i_pitch;
                }

                p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                          p_pic->p[i_plane].i_pitch );

                /* For TOP field we need to add the last line */
                if( i_field == 0 )
                {
                    p_in += p_pic->p[i_plane].i_pitch;
                    p_out += p_pic->p[i_plane].i_pitch;
                    p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                              p_pic->p[i_plane].i_pitch );
                }
                break;

            case VLC_FOURCC('I','4','2','2'):
                /* For BOTTOM field we need to add the first line */
                if( i_field == 1 )
                {
                    p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                              p_pic->p[i_plane].i_pitch );
                    p_in += p_pic->p[i_plane].i_pitch;
                    p_out += p_pic->p[i_plane].i_pitch;
                }

                p_out_end -= 2 * p_outpic->p[i_plane].i_pitch;

                if( i_plane == Y_PLANE )
                {
                    for( ; p_out < p_out_end ; )
                    {
                        p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                                  p_pic->p[i_plane].i_pitch );

                        p_out += p_pic->p[i_plane].i_pitch;

                        p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                                  p_pic->p[i_plane].i_pitch );

                        p_in += 2 * p_pic->p[i_plane].i_pitch;
                        p_out += p_pic->p[i_plane].i_pitch;
                    }
                }
                else
                {
                    for( ; p_out < p_out_end ; )
                    {
                        p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                                  p_pic->p[i_plane].i_pitch );

                        p_out += p_pic->p[i_plane].i_pitch;
                        p_in += 2 * p_pic->p[i_plane].i_pitch;
                    }
                }

                p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                          p_pic->p[i_plane].i_pitch );

                /* For TOP field we need to add the last line */
                if( i_field == 0 )
                {
                    p_in += p_pic->p[i_plane].i_pitch;
                    p_out += p_pic->p[i_plane].i_pitch;
                    p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                              p_pic->p[i_plane].i_pitch );
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
            p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                      p_pic->p[i_plane].i_pitch );
            p_in += p_pic->p[i_plane].i_pitch;
            p_out += p_pic->p[i_plane].i_pitch;
        }

        p_out_end -= 2 * p_outpic->p[i_plane].i_pitch;

        for( ; p_out < p_out_end ; )
        {
            p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                      p_pic->p[i_plane].i_pitch );

            p_out += p_pic->p[i_plane].i_pitch;

            Merge( p_out, p_in, p_in + 2 * p_pic->p[i_plane].i_pitch,
                   p_pic->p[i_plane].i_pitch );

            p_in += 2 * p_pic->p[i_plane].i_pitch;
            p_out += p_pic->p[i_plane].i_pitch;
        }

        p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                  p_pic->p[i_plane].i_pitch );

        /* For TOP field we need to add the last line */
        if( i_field == 0 )
        {
            p_in += p_pic->p[i_plane].i_pitch;
            p_out += p_pic->p[i_plane].i_pitch;
            p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                      p_pic->p[i_plane].i_pitch );
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

            p_out += p_pic->p[i_plane].i_pitch;
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
            case VLC_FOURCC('I','4','2','0'):
            case VLC_FOURCC('I','Y','U','V'):
            case VLC_FOURCC('Y','V','1','2'):
                /* First line: simple copy */
                p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                          p_pic->p[i_plane].i_pitch );
                p_out += p_pic->p[i_plane].i_pitch;

                /* Remaining lines: mean value */
                for( ; p_out < p_out_end ; )
                {
                    Merge( p_out, p_in, p_in + p_pic->p[i_plane].i_pitch,
                           p_pic->p[i_plane].i_pitch );

                    p_out += p_pic->p[i_plane].i_pitch;
                    p_in += p_pic->p[i_plane].i_pitch;
                }
                break;

            case VLC_FOURCC('I','4','2','2'):
                /* First line: simple copy */
                p_vout->p_vlc->pf_memcpy( p_out, p_in,
                                          p_pic->p[i_plane].i_pitch );
                p_out += p_pic->p[i_plane].i_pitch;

                /* Remaining lines: mean value */
                if( i_plane == Y_PLANE )
                {
                    for( ; p_out < p_out_end ; )
                    {
                        Merge( p_out, p_in, p_in + p_pic->p[i_plane].i_pitch,
                               p_pic->p[i_plane].i_pitch );

                        p_out += p_pic->p[i_plane].i_pitch;
                        p_in += p_pic->p[i_plane].i_pitch;
                    }
                }

                else
                {
                    for( ; p_out < p_out_end ; )
                    {
                        Merge( p_out, p_in, p_in + p_pic->p[i_plane].i_pitch,
                               p_pic->p[i_plane].i_pitch );

                        p_out += p_pic->p[i_plane].i_pitch;
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
static void MergeMMX( void *_p_dest, const void *_p_s1, const void *_p_s2,
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

#if defined(CAN_COMPILE_SSE)
static void MergeSSE2( void *_p_dest, const void *_p_s1, const void *_p_s2,
                       size_t i_bytes )
{
    uint8_t* p_dest = (uint8_t*)_p_dest;
    const uint8_t *p_s1 = (const uint8_t *)_p_s1;
    const uint8_t *p_s2 = (const uint8_t *)_p_s2;
    uint8_t* p_end;
    while( (int)p_s1 % 16 )
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

#ifdef CAN_COMPILE_C_ALTIVEC
static void MergeAltivec( void *_p_dest, const void *_p_s1,
                          const void *_p_s2, size_t i_bytes )
{
    uint8_t *p_dest = (uint8_t *)_p_dest;
    uint8_t *p_s1   = (uint8_t *)_p_s1;
    uint8_t *p_s2   = (uint8_t *)_p_s2;
    uint8_t *p_end  = p_dest + i_bytes - 15;

    /* Use C until the first 16-bytes aligned destination pixel */
    while( (int)p_dest & 0xF )
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

    return fc < 1 ? VLC_FALSE : VLC_TRUE;
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

/* XDeint8x8Frame: apply a small blend between field (1,6,1).
 * This won't destroy details, and help if there is a bit of interlacing.
 * (It helps with paning to avoid flickers)
 * (Use 8x9 pixels)
 */
#if 0
static inline void XDeint8x8FrameC( uint8_t *dst, int i_dst,
                                    uint8_t *src, int i_src )
{
    int y, x;

    /* Progressive */
    for( y = 0; y < 8; y += 2 )
    {
        memcpy( dst, src, 8 );
        dst += i_dst;

        for( x = 0; x < 8; x++ )
            dst[x] = (src[x] + 6*src[1*i_src+x] + src[2*i_src+x] + 4 ) >> 3;
        dst += 1*i_dst;
        src += 2*i_src;
    }
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
    static const uint64_t m_4 = I64C(0x0004000400040004);
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

#if 0
static inline int XDeint8x8SsdC( uint8_t *pix1, int i_pix1,
                                 uint8_t *pix2, int i_pix2 )
{
    int y, x;
    int s = 0;

    for( y = 0; y < 8; y++ )
        for( x = 0; x < 8; x++ )
            s += ssd( pix1[y*i_pix1+x] - pix2[y*i_pix2+x] );
    return s;
}

#ifdef CAN_COMPILE_MMXEXT
static inline int XDeint8x8SsdMMXEXT( uint8_t *pix1, int i_pix1,
                                      uint8_t *pix2, int i_pix2 )
{
    int y;
    int32_t s;

    pxor_r2r( mm7, mm7 );
    pxor_r2r( mm6, mm6 );

    for( y = 0; y < 8; y++ )
    {
        movq_m2r( pix1[0], mm0 );
        movq_m2r( pix2[0], mm1 );

        movq_r2r( mm0, mm2 );
        movq_r2r( mm1, mm3 );

        punpcklbw_r2r( mm7, mm0 );
        punpckhbw_r2r( mm7, mm2 );
        punpcklbw_r2r( mm7, mm1 );
        punpckhbw_r2r( mm7, mm3 );

        psubw_r2r( mm1, mm0 );
        psubw_r2r( mm3, mm2 );

        pmaddwd_r2r( mm0, mm0 );
        pmaddwd_r2r( mm2, mm2 );

        paddd_r2r( mm2, mm0 );
        paddd_r2r( mm0, mm6 );

        pix1 += i_pix1;
        pix2 += i_pix2;
    }

    movq_r2r( mm6, mm7 );
    psrlq_i2r( 32, mm7 );
    paddd_r2r( mm6, mm7 );
    movd_r2m( mm7, s );

    return s;
}
#endif
#endif

#if 0
/* A little try with motion, but doesn't work better that pure intra (and slow) */
#ifdef CAN_COMPILE_MMXEXT
/* XDeintMC:
 *  Bilinear MC QPel
 *  TODO: mmx version (easier in sse2)
 */
static inline void XDeintMC( uint8_t *dst, int i_dst,
                             uint8_t *src, int i_src,
                             int mvx, int mvy,
                             int i_width, int i_height )
{
    const int d4x = mvx&0x03;
    const int d4y = mvy&0x03;

    const int cA = (4-d4x)*(4-d4y);
    const int cB = d4x    *(4-d4y);
    const int cC = (4-d4x)*d4y;
    const int cD = d4x    *d4y;

    int y, x;
    uint8_t *srcp;


    src  += (mvy >> 2) * i_src + (mvx >> 2);
    srcp = &src[i_src];

    for( y = 0; y < i_height; y++ )
    {
        for( x = 0; x < i_width; x++ )
        {
            dst[x] = ( cA*src[x]  + cB*src[x+1] +
                       cC*srcp[x] + cD*srcp[x+1] + 8 ) >> 4;
        }
        dst  += i_dst;

        src   = srcp;
        srcp += i_src;
    }
}
static int XDeint8x4SadMMXEXT( uint8_t *pix1, int i_pix1,
                               uint8_t *pix2, int i_pix2 )
{
    int32_t s;

    movq_m2r( pix1[0*i_pix1], mm0 );
    movq_m2r( pix1[1*i_pix1], mm1 );

    psadbw_m2r( pix2[0*i_pix2], mm0 );
    psadbw_m2r( pix2[1*i_pix2], mm1 );

    movq_m2r( pix1[2*i_pix1], mm2 );
    movq_m2r( pix1[3*i_pix1], mm3 );
    psadbw_m2r( pix2[2*i_pix2], mm2 );
    psadbw_m2r( pix2[3*i_pix2], mm3 );

    paddd_r2r( mm1, mm0 );
    paddd_r2r( mm3, mm2 );
    paddd_r2r( mm2, mm0 );
    movd_r2m( mm0, s );

    return s;
}

static inline int XDeint8x4TestQpel( uint8_t *src, int i_src,
                                     uint8_t *ref, int i_stride,
                                     int mx, int my,
                                     int xmax, int ymax )
{
    uint8_t buffer[8*4];

    if( abs(mx) >= 4*xmax || abs(my) >= 4*ymax )
        return 255*255*255;

    XDeintMC( buffer, 8, ref, i_stride, mx, my, 8, 4 );
    return XDeint8x4SadMMXEXT( src, i_src, buffer, 8 );
}
static inline int XDeint8x4TestInt( uint8_t *src, int i_src,
                                    uint8_t *ref, int i_stride,
                                    int mx, int my,
                                    int xmax, int ymax )
{
    if( abs(mx) >= xmax || abs(my) >= ymax )
        return 255*255*255;

    return XDeint8x4SadMMXEXT( src, i_src, &ref[my*i_stride+mx], i_stride );
}

static inline void XDeint8x8FieldMotion( uint8_t *dst, int i_dst,
                                         uint8_t *src, int i_src,
                                         int *mpx, int *mpy,
                                         int xmax, int ymax )
{
    static const int dx[8] = { 0,  0, -1, 1, -1, -1,  1, 1 };
    static const int dy[8] = {-1,  1,  0, 0, -1,  1, -1, 1 };
    uint8_t *next = &src[i_src];
    const int i_src2 = 2*i_src;
    int mvx, mvy;
    int mvs, s;
    int i_step;

    uint8_t *rec = &dst[i_dst];

    /* We construct with intra method the missing field */
    XDeint8x8FieldMMXEXT( dst, i_dst, src, i_src );

    /* Now we will try to find a match with ME with the other field */

    /* ME: A small/partial EPZS
     * We search only for small MV (with high motion intra will be perfect */
    if( xmax > 4 ) xmax = 4;
    if( ymax > 4 ) ymax = 4;

    /* Init with NULL Mv */
    mvx = mvy = 0;
    mvs = XDeint8x4SadMMXEXT( rec, i_src2, next, i_src2 );

    /* Try predicted Mv */
    if( (s=XDeint8x4TestInt( rec, i_src2, next, i_src2, *mpx, *mpy, xmax, ymax)) < mvs )
    {
        mvs = s;
        mvx = *mpx;
        mvy = *mpy;
    }
    /* Search interger pel (small mv) */
    for( i_step = 0; i_step < 4; i_step++ )
    {
        int c = 4;
        int s;
        int i;

        for( i = 0; i < 4; i++ )
        {
            s = XDeint8x4TestInt( rec, i_src2,
                                  next, i_src2, mvx+dx[i], mvy+dy[i],
                                  xmax, ymax );
            if( s < mvs )
            {
                mvs = s;
                c = i;
            }
        }
        if( c == 4 )
            break;

        mvx += dx[c];
        mvy += dy[c];
    }
    *mpx = mvx;
    *mpy = mvy;

    mvx <<= 2;
    mvy <<= 2;

    if( mvs > 4 && mvs < 256 )
    {
        /* Search Qpel */
        /* XXX: for now only HPEL (too slow) */
        for( i_step = 0; i_step < 4; i_step++ )
        {
            int c = 8;
            int s;
            int i;

            for( i = 0; i < 8; i++ )
            {
                s = XDeint8x4TestQpel( rec, i_src2, next, i_src2,
                                       mvx+dx[i], mvy+dy[i],
                                       xmax, ymax );
                if( s < mvs )
                {
                    mvs = s;
                    c = i;
                }
            }
            if( c == 8 )
                break;

            mvx += dx[c];
            mvy += dy[c];
        }
    }

    if( mvs < 128 )
    {
        uint8_t buffer[8*4];
        XDeintMC( buffer, 8, next, i_src2, mvx, mvy, 8, 4 );
        XDeint8x8MergeMMXEXT( dst, i_dst, src, 2*i_src, buffer, 8 );

        //XDeint8x8Set( dst, i_dst, 0 );
    }
}
#endif
#endif

#if 0
/* Kernel interpolation (1,-5,20,20,-5,1)
 * Loose a bit more details+add aliasing than edge interpol but avoid
 * more artifacts
 */
static inline uint8_t clip1( int a )
{
    if( a <= 0 )
        return 0;
    else if( a >= 255 )
        return 255;
    else
        return a;
}
static inline void XDeint8x8Field( uint8_t *dst, int i_dst,
                                   uint8_t *src, int i_src )
{
    int y, x;

    /* Interlaced */
    for( y = 0; y < 8; y += 2 )
    {
        const int i_src2 = i_src*2;

        memcpy( dst, src, 8 );
        dst += i_dst;

        for( x = 0; x < 8; x++ )
        {
            int pix;

            pix =   1*(src[-2*i_src2+x]+src[3*i_src2+x]) +
                   -5*(src[-1*i_src2+x]+src[2*i_src2+x])
                  +20*(src[ 0*i_src2+x]+src[1*i_src2+x]);

            dst[x] = clip1( ( pix + 16 ) >> 5 );
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

    return fc < 2 ? VLC_FALSE : VLC_TRUE;
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

static void RenderX( vout_thread_t *p_vout,
                     picture_t *p_outpic, picture_t *p_pic )
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
            if( p_vout->p_libvlc->i_cpu & CPU_CAPABILITY_MMXEXT )
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
    if( p_vout->p_libvlc->i_cpu & CPU_CAPABILITY_MMXEXT )
        emms();
#endif
}

/*****************************************************************************
 * SendEvents: forward mouse and keyboard events to the parent p_vout
 *****************************************************************************/
static int SendEvents( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *_p_vout )
{
    vout_thread_t *p_vout = (vout_thread_t *)_p_vout;
    vlc_value_t sentval = newval;

    if( !strcmp( psz_var, "mouse-y" ) )
    {
        switch( p_vout->p_sys->i_mode )
        {
            case DEINTERLACE_MEAN:
            case DEINTERLACE_DISCARD:
                sentval.i_int *= 2;
                break;
        }
    }

    var_Set( p_vout, psz_var, sentval );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * FilterCallback: called when changing the deinterlace method on the fly.
 *****************************************************************************/
static int FilterCallback( vlc_object_t *p_this, char const *psz_cmd,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;
    int i_old_mode = p_vout->p_sys->i_mode;

    msg_Dbg( p_vout, "using %s deinterlace mode", newval.psz_string );

    vlc_mutex_lock( &p_vout->p_sys->filter_lock );

    SetFilterMethod( p_vout, newval.psz_string );

    switch( p_vout->render.i_chroma )
    {
    case VLC_FOURCC('I','4','2','2'):
        vlc_mutex_unlock( &p_vout->p_sys->filter_lock );
        return VLC_SUCCESS;
        break;

    case VLC_FOURCC('I','4','2','0'):
    case VLC_FOURCC('I','Y','U','V'):
    case VLC_FOURCC('Y','V','1','2'):
        switch( p_vout->p_sys->i_mode )
        {
        case DEINTERLACE_MEAN:
        case DEINTERLACE_DISCARD:
            if( ( i_old_mode == DEINTERLACE_MEAN )
                || ( i_old_mode == DEINTERLACE_DISCARD ) )
            {
                vlc_mutex_unlock( &p_vout->p_sys->filter_lock );
                return VLC_SUCCESS;
            }
            break;

        case DEINTERLACE_BOB:
        case DEINTERLACE_BLEND:
        case DEINTERLACE_LINEAR:
            if( ( i_old_mode == DEINTERLACE_BOB )
                || ( i_old_mode == DEINTERLACE_BLEND )
                || ( i_old_mode == DEINTERLACE_LINEAR ) )
            {
                vlc_mutex_unlock( &p_vout->p_sys->filter_lock );
                return VLC_SUCCESS;
            }
            break;
        }
        break;

    default:
        break;
    }

    /* We need to kill the old vout */

    DEL_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );

    vlc_object_detach( p_vout->p_sys->p_vout );
    vout_Destroy( p_vout->p_sys->p_vout );

    /* Try to open a new video output */
    p_vout->p_sys->p_vout = SpawnRealVout( p_vout );

    if( p_vout->p_sys->p_vout == NULL )
    {
        /* Everything failed */
        msg_Err( p_vout, "cannot open vout, aborting" );

        vlc_mutex_unlock( &p_vout->p_sys->filter_lock );
        return VLC_EGENERIC;
    }

    ADD_CALLBACKS( p_vout->p_sys->p_vout, SendEvents );

    vlc_mutex_unlock( &p_vout->p_sys->filter_lock );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * SendEventsToChild: forward events to the child/children vout
 *****************************************************************************/
static int SendEventsToChild( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    var_Set( p_vout->p_sys->p_vout, psz_var, newval );
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
    p_pic_dst = p_filter->pf_vout_buffer_new( p_filter );
    if( p_pic_dst == NULL )
    {
        msg_Warn( p_filter, "can't get output picture" );
        return NULL;
    }

    switch( p_vout->p_sys->i_mode )
    {
        case DEINTERLACE_DISCARD:
#if 0
            RenderDiscard( p_vout, p_pic_dst, p_pic, 0 );
#endif
            msg_Err( p_vout, "discarding lines is not supported yet" );
            p_pic_dst->pf_release( p_pic_dst );
            return p_pic;
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
            p_pic_dst->pf_release( p_pic_dst );
            return p_pic;
            break;

        case DEINTERLACE_MEAN:
            RenderMean( p_vout, p_pic_dst, p_pic );
            break;

        case DEINTERLACE_BLEND:
            RenderBlend( p_vout, p_pic_dst, p_pic );
            break;

        case DEINTERLACE_X:
            RenderX( p_vout, p_pic_dst, p_pic );
            break;
    }

    p_pic_dst->date = p_pic->date;
    p_pic_dst->b_force = p_pic->b_force;
    p_pic_dst->i_nb_fields = p_pic->i_nb_fields;
    p_pic_dst->b_progressive = VLC_TRUE;
    p_pic_dst->b_top_field_first = p_pic->b_top_field_first;

    p_pic->pf_release( p_pic );
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

    if( ( p_filter->fmt_in.video.i_chroma != VLC_FOURCC('I','4','2','0') &&
          p_filter->fmt_in.video.i_chroma != VLC_FOURCC('I','Y','U','V') &&
          p_filter->fmt_in.video.i_chroma != VLC_FOURCC('Y','V','1','2') ) ||
        p_filter->fmt_in.video.i_chroma != p_filter->fmt_out.video.i_chroma )
    {
        return VLC_EGENERIC;
    }

    /* Impossible to use VLC_OBJECT_VOUT here because it would be used
     * by spu filters */
    p_vout = vlc_object_create( p_filter, sizeof(vout_thread_t) );
    vlc_object_attach( p_vout, p_filter );
    p_filter->p_sys = (filter_sys_t *)p_vout;
    p_vout->render.i_chroma = p_filter->fmt_in.video.i_chroma;

    sout_CfgParse( p_filter, FILTER_CFG_PREFIX, ppsz_filter_options,
                   p_filter->p_cfg );
    var_Get( p_filter, FILTER_CFG_PREFIX "mode", &val );
    var_Create( p_filter, "deinterlace-mode", VLC_VAR_STRING );
    var_Set( p_filter, "deinterlace-mode", val );

    if ( Create( VLC_OBJECT(p_vout) ) != VLC_SUCCESS )
    {
        vlc_object_detach( p_vout );
        vlc_object_release( p_vout );
        return VLC_EGENERIC;
    }

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

