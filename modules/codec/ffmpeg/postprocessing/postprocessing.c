/*****************************************************************************
 * postprocessing.c
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: postprocessing.c,v 1.1 2002/08/04 22:13:06 fenrir Exp $
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
#include <stdlib.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include "postprocessing.h"
#include "postprocessing_common.h"


static int Open ( vlc_object_t *p_this );

static u32 pp_getmode( int i_quality, int b_autolevel );
static int pp_postprocess( picture_t *,
                           QT_STORE_T *, unsigned int,
                           unsigned int i_mode );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
#ifdef MODULE_NAME_IS_postprocessing_c
    set_description( _("C Post Processing module") );
    set_capability( "postprocessing", 50 );
    add_shortcut( "c" );
#elif defined( MODULE_NAME_IS_postprocessing_mmx )
    set_description( _("MMX Post Processing module") );
    set_capability( "postprocessing", 100 );
    add_requirement( MMX );
    add_shortcut( "mmx" );
#elif defined( MODULE_NAME_IS_postprocessing_mmxext )
    set_description( _("MMXEXT Post Processing module") );
    set_capability( "postprocessing", 150 );
    add_requirement( MMXEXT );
    add_shortcut( "mmxext" );
    add_shortcut( "mmx2" );
#endif
    set_callbacks( Open, NULL );
vlc_module_end();

/*****************************************************************************
 * Module initializer
 *****************************************************************************/
static int Open ( vlc_object_t *p_this )
{
    postprocessing_t *p_pp = (postprocessing_t *)p_this;

    p_pp->pf_getmode = pp_getmode;
    p_pp->pf_postprocess = pp_postprocess;
    
    return VLC_SUCCESS;
}


static u32 pp_getmode( int i_quality, int b_autolevel )
{
    u32 i_mode;
    i_quality = i_quality < 0 ? 0 : i_quality;
    i_quality = i_quality > 6 ? 6 : i_quality;

    switch( i_quality )
    {
        case( 0 ):
            i_mode = 0;
            break;
        case( 1 ):
            i_mode = PP_DEBLOCK_Y_H;
            break;
        case( 2 ):
            i_mode = PP_DEBLOCK_Y_H|PP_DEBLOCK_Y_V;
            break;
        case( 3 ):
            i_mode = PP_DEBLOCK_Y_H|PP_DEBLOCK_Y_V|
                     PP_DEBLOCK_C_H;
            break;
        case( 4 ):
            i_mode = PP_DEBLOCK_Y_H|PP_DEBLOCK_Y_V|
                     PP_DEBLOCK_C_H|PP_DEBLOCK_C_V;
            break;
        case( 5 ):
            i_mode = PP_DEBLOCK_Y_H|PP_DEBLOCK_Y_V|
                     PP_DEBLOCK_C_H|PP_DEBLOCK_C_V|
                     PP_DERING_Y;
            break;
        case( 6 ):
            i_mode = PP_DEBLOCK_Y_H|PP_DEBLOCK_Y_V|
                     PP_DEBLOCK_C_H|PP_DEBLOCK_C_V|
                     PP_DERING_Y|PP_DERING_C;
            break;
        default:
            i_mode = 0;
    }
    if( b_autolevel )
    {
        i_mode |= PP_AUTOLEVEL;
    }

    return( i_mode );
}

/*****************************************************************************
 * pp_postprocess : make post-filter as defined in MPEG4-ISO 
 *****************************************************************************
 *****************************************************************************/

static int pp_postprocess( picture_t *p_pic,
                           QT_STORE_T *p_QP_store, unsigned int i_QP_stride,
                           unsigned int i_mode )
{
    /* Some sanity checks */
//    if( ( p_pic->i_height&0x0f )||( p_pic->i_width&0x0f )||
    if( ( p_pic->p_heap->i_chroma != VLC_FOURCC( 'I', '4', '2', '0' ) )&&
        ( p_pic->p_heap->i_chroma != VLC_FOURCC( 'Y', 'V', '1', '2' ) ) )
    {
        return( PP_ERR_INVALID_PICTURE );
    }

    if( ( !p_QP_store )||( i_QP_stride < p_pic->p_heap->i_width >> 4 ) )
    {
        return( PP_ERR_INVALID_QP );
    }

    /* First do vertical deblocking and then horizontal */
    /* Luminance */

    if( i_mode&PP_DEBLOCK_Y_V )
    {
        E_( pp_deblock_V )( p_pic->Y_PIXELS,
                            p_pic->p_heap->i_width, p_pic->p_heap->i_height, p_pic->Y_PITCH,
                            p_QP_store, i_QP_stride,
                            0 );
    }
    if( i_mode&PP_DEBLOCK_Y_H )
    {
        E_( pp_deblock_H )( p_pic->Y_PIXELS,
                            p_pic->p_heap->i_width, p_pic->p_heap->i_height, p_pic->Y_PITCH,
                            p_QP_store, i_QP_stride,
                            0 );
    }

    /* Chrominance */
    if( i_mode&PP_DEBLOCK_C_V )
    {
        E_( pp_deblock_V )( p_pic->U_PIXELS,
                            p_pic->p_heap->i_width >> 1, p_pic->p_heap->i_height >> 1, 
                            p_pic->U_PITCH,
                            p_QP_store, i_QP_stride,
                            1 );
        E_( pp_deblock_V )( p_pic->V_PIXELS,
                            p_pic->p_heap->i_width >> 1, p_pic->p_heap->i_height >> 1, 
                            p_pic->V_PITCH,
                            p_QP_store, i_QP_stride,
                            1 );
    }
    if( i_mode&PP_DEBLOCK_C_H )
    {
        E_( pp_deblock_H )( p_pic->U_PIXELS,
                            p_pic->p_heap->i_width >> 1, p_pic->p_heap->i_height >> 1, 
                            p_pic->U_PITCH,
                            p_QP_store, i_QP_stride,
                            1 );
        E_( pp_deblock_H )( p_pic->V_PIXELS,
                            p_pic->p_heap->i_width >> 1, p_pic->p_heap->i_height >> 1, 
                            p_pic->V_PITCH,
                            p_QP_store, i_QP_stride,
                            1 );
    }
 
    /* After deblocking do dering */   
    /* TODO check for min size */
    
    if( i_mode&PP_DERING_Y )
    {
        E_( pp_dering_Y )( p_pic->Y_PIXELS,
                           p_pic->p_heap->i_width, p_pic->p_heap->i_height, 
                           p_pic->Y_PITCH,
                           p_QP_store, i_QP_stride );
    }
    if( i_mode&PP_DERING_C )
    {
        E_( pp_dering_C )( p_pic->U_PIXELS,
                           p_pic->p_heap->i_width >> 1, p_pic->p_heap->i_height >> 1, 
                           p_pic->U_PITCH,
                           p_QP_store, i_QP_stride );

        E_( pp_dering_C )( p_pic->V_PIXELS,
                           p_pic->p_heap->i_width >> 1, p_pic->p_heap->i_height >> 1, 
                           p_pic->V_PITCH,
                           p_QP_store, i_QP_stride );

    }
#if defined( MODULE_NAME_IS_postprocessing_mmx )||defined( MODULE_NAME_IS_postprocessing_mmxext )
    /* We have used MMX so return to safe FPU state */
    __asm__ __volatile__ ( "emms"  );
#endif
    return( PP_ERR_OK );
}
