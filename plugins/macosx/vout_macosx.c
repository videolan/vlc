
/*****************************************************************************
 * vout_macosx.c: MacOS X video output plugin
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Florian G. Pflug <fgp@phlo.org>
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

#define MODULE_NAME macosx
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "interface.h"
#include "intf_msg.h"

#include "video.h"
#include "video_output.h"

#include "modules.h"
#include "main.h"

#include "macosx.h"

#include <QuickTime/QuickTime.h>

/*****************************************************************************
 * vout_sys_t: MacOS X video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the MacOS X specific properties of an output thread.
 *****************************************************************************/
typedef unsigned int yuv2_data_t ;
typedef struct vout_sys_s
{
    osx_com_t osx_communication ;

    ImageDescriptionHandle h_img_descr ;
    ImageSequence i_seq ;   
    unsigned int c_codec ;
    MatrixRecordPtr p_matrix ;
    
    yuv2_data_t *p_yuv2 ;
    unsigned i_yuv2_size ;
    PlanarPixmapInfoYUV420 s_ppiy420 ;
} vout_sys_t;


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  vout_Probe     ( probedata_t *p_data );
static int  vout_Create    ( struct vout_thread_s * );
static int  vout_Init      ( struct vout_thread_s * );
static void vout_End       ( struct vout_thread_s * );
static void vout_Destroy   ( struct vout_thread_s * );
static int  vout_Manage    ( struct vout_thread_s * );
static void vout_Display   ( struct vout_thread_s * );

/* OS Specific */
static void fillout_PPIYUV420( picture_t *p_y420, PlanarPixmapInfoYUV420 *p_ppiy420 ) ;
static void fillout_ImageDescription(ImageDescriptionHandle h_descr, unsigned int i_with, unsigned int i_height, unsigned int c_codec) ;
static void fillout_ScalingMatrix( vout_thread_t *p_vout ) ;
static OSErr new_QTSequence(ImageSequence *i_seq, CGrafPtr p_port, ImageDescriptionHandle h_descr, MatrixRecordPtr p_matrix) ;
static int create_QTSequenceBestCodec( vout_thread_t *p_vout ) ;
static void dispose_QTSequence( vout_thread_t *p_vout ) ;
static void convert_Y420_to_YUV2( picture_t *p_y420, yuv2_data_t *p_yuv2 ) ;

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( vout_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = vout_Probe;
    p_function_list->functions.vout.pf_create     = vout_Create;
    p_function_list->functions.vout.pf_init       = vout_Init;
    p_function_list->functions.vout.pf_end        = vout_End;
    p_function_list->functions.vout.pf_destroy    = vout_Destroy;
    p_function_list->functions.vout.pf_manage     = vout_Manage;
    p_function_list->functions.vout.pf_display    = vout_Display;
    p_function_list->functions.vout.pf_setpalette = NULL;
}

/*****************************************************************************
 * intf_Probe: return a score
 *****************************************************************************/
static int vout_Probe( probedata_t *p_data )
{
    if( TestMethod( VOUT_METHOD_VAR, "macosx" ) )
    {
        return( 999 );
    }

    return( 100 );
}

/*****************************************************************************
 * vout_Create: allocates MacOS X video thread output method
 *****************************************************************************
 * This function allocates and initializes a MacOS X vout method.
 *****************************************************************************/
static int vout_Create( vout_thread_t *p_vout )
{
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        intf_ErrMsg( "error: %s", strerror( ENOMEM ) );
        return( 1 );
    }
    p_vout->p_sys->h_img_descr = (ImageDescriptionHandle)NewHandleClear( sizeof( ImageDescription ) ) ;
    p_vout->p_sys->p_matrix = (MatrixRecordPtr)malloc( sizeof( MatrixRecord ) ) ;
    p_vout->p_sys->c_codec = 'NONE' ;

    EnterMovies() ;

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize video thread output method
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    p_vout->b_need_render = 0 ;
    p_vout->i_bytes_per_line = p_vout->i_width ;
    p_vout->p_sys->c_codec = 'NONE' ;
    vlc_mutex_lock( &p_vout->p_sys->osx_communication.lock ) ;
        p_vout->p_sys->osx_communication.i_changes |= OSX_VOUT_INTF_REQUEST_QDPORT ;
    vlc_mutex_unlock( &p_vout->p_sys->osx_communication.lock ) ;
    
    return 0 ;
}

/*****************************************************************************
 * vout_End: terminate video thread output method
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    vlc_mutex_lock( &p_vout->p_sys->osx_communication.lock ) ;
        p_vout->p_sys->osx_communication.i_changes |= OSX_VOUT_INTF_RELEASE_QDPORT ;
    vlc_mutex_unlock( &p_vout->p_sys->osx_communication.lock ) ;
    
    dispose_QTSequence( p_vout ) ;
}

/*****************************************************************************
 * vout_Destroy: destroy video thread output method
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    free( p_vout->p_sys->p_matrix ) ;
    DisposeHandle( (Handle)p_vout->p_sys->h_img_descr ) ;
    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Manage: handle events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int vout_Manage( vout_thread_t *p_vout )
{    
    vlc_mutex_lock( &p_vout->p_sys->osx_communication.lock ) ;
        if ( p_vout->p_sys->osx_communication.i_changes & OSX_INTF_VOUT_QDPORT_CHANGE ) {
            dispose_QTSequence( p_vout ) ;
            create_QTSequenceBestCodec( p_vout ) ;
        }
        else if ( p_vout->p_sys->osx_communication.i_changes & OSX_INTF_VOUT_SIZE_CHANGE ) {
            fillout_ScalingMatrix( p_vout ) ;
            SetDSequenceMatrix( p_vout->p_sys->i_seq, p_vout->p_sys->p_matrix ) ;
        }
        
        p_vout->p_sys->osx_communication.i_changes &= ~( 
            OSX_INTF_VOUT_QDPORT_CHANGE |
            OSX_INTF_VOUT_SIZE_CHANGE
        ) ;
    vlc_mutex_unlock( &p_vout->p_sys->osx_communication.lock ) ;
        
    return 0 ;
}


/*****************************************************************************
 * vout_OSX_Display: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to image, waits until
 * it is displayed and switch the two rendering buffers, preparing next frame.
 *****************************************************************************/
void vout_Display( vout_thread_t *p_vout )
{
    CodecFlags out_flags ;
    
    switch (p_vout->p_sys->c_codec)
    {
        case 'yuv2':
            convert_Y420_to_YUV2(p_vout->p_rendered_pic, p_vout->p_sys->p_yuv2) ;
            DecompressSequenceFrameS(
		p_vout->p_sys->i_seq,
		(void *)p_vout->p_sys->p_yuv2,
		p_vout->p_sys->i_yuv2_size,
		codecFlagUseScreenBuffer,
 		&out_flags,
 		nil
            ) ;
            break ;
        case 'y420':
            fillout_PPIYUV420(p_vout->p_rendered_pic, &p_vout->p_sys->s_ppiy420) ;
            DecompressSequenceFrameS(
		p_vout->p_sys->i_seq,
		(void *)&p_vout->p_sys->s_ppiy420,
		sizeof(PlanarPixmapInfoYUV420),
		codecFlagUseScreenBuffer,
 		&out_flags,
 		nil
            ) ;            
            break ;
    }
}

static void fillout_PPIYUV420( picture_t *p_y420, PlanarPixmapInfoYUV420 *p_ppiy420 )
{
    p_ppiy420->componentInfoY.offset = (void *)p_y420->p_y - (void *)p_ppiy420 ;
    p_ppiy420->componentInfoY.rowBytes = p_y420->i_width ;
    p_ppiy420->componentInfoCb.offset = (void *)p_y420->p_u - (void *)p_ppiy420 ;
    p_ppiy420->componentInfoCb.rowBytes = p_y420->i_width / 2;
    p_ppiy420->componentInfoCr.offset = (void *)p_y420->p_v - (void *)p_ppiy420 ;
    p_ppiy420->componentInfoCr.rowBytes = p_y420->i_width / 2;
}


static void fillout_ImageDescription(ImageDescriptionHandle h_descr, unsigned int i_width, unsigned int i_height, unsigned int c_codec)
{
    ImageDescriptionPtr p_descr ;

    HLock((Handle)h_descr) ;
    p_descr = *h_descr ;
    p_descr->idSize = sizeof(ImageDescription) ;
    p_descr->cType = c_codec ;
    p_descr->resvd1 = 0 ; //Reserved
    p_descr->resvd2 = 0 ; //Reserved
    p_descr->dataRefIndex = 0 ; //Reserved
    p_descr->version = 1 ; //
    p_descr->revisionLevel = 0 ;
    p_descr->vendor = 'appl' ; //How do we get a vendor id??
    p_descr->width = i_width  ;
    p_descr->height = i_height ;
    p_descr->hRes = Long2Fix(72) ;
    p_descr->vRes = Long2Fix(72) ;
    p_descr->spatialQuality = codecLosslessQuality ;
    p_descr->frameCount = 1 ;
    p_descr->clutID = -1 ; //We don't need a color table
    
    switch (c_codec)
    {
        case 'yuv2':
            p_descr->dataSize=i_width * i_height * 2 ;
            p_descr->depth = 24 ;
            break ;
        case 'y420':
            p_descr->dataSize=i_width * i_height * 1.5 ;
            p_descr->depth = 12 ;
            break ;
    }
    
    HUnlock((Handle)h_descr) ;
}

static void fillout_ScalingMatrix( vout_thread_t *p_vout)
{
	Rect s_rect ;
	Fixed factor_x ;
	Fixed factor_y ;
        	
	GetPortBounds( p_vout->p_sys->osx_communication.p_qdport, &s_rect ) ;
//	if (((s_rect.right - s_rect.left) / ((float) p_vout->i_width)) < ((s_rect.bottom - s_rect.top) / ((float) p_vout->i_height)))
		factor_x = FixDiv(Long2Fix(s_rect.right - s_rect.left), Long2Fix(p_vout->i_width)) ;
//	else
		factor_y = FixDiv(Long2Fix(s_rect.bottom - s_rect.top), Long2Fix(p_vout->i_height)) ;
	
	SetIdentityMatrix(p_vout->p_sys->p_matrix) ;
	ScaleMatrix( p_vout->p_sys->p_matrix, factor_x, factor_y, Long2Fix(0), Long2Fix(0) ) ;
}

static OSErr new_QTSequence( ImageSequence *i_seq, CGrafPtr p_qdport, ImageDescriptionHandle h_descr, MatrixRecordPtr p_matrix )
{
    return DecompressSequenceBeginS(
        i_seq, 
        h_descr,
        NULL,
        0,
        p_qdport,
        NULL, //device to display (is set implicit via the qdPort)
        NULL, //src-rect
        p_matrix, //matrix
        0, //just do plain copying
        NULL, //no mask region
        codecFlagUseScreenBuffer,
        codecLosslessQuality,
        (DecompressorComponent) bestSpeedCodec
    ) ;
}

static int create_QTSequenceBestCodec( vout_thread_t *p_vout )
{
    if ( p_vout->p_sys->osx_communication.p_qdport == nil)
    {
        p_vout->p_sys->c_codec = 'NONE' ;
        return 1 ;
    }

    SetPort( p_vout->p_sys->osx_communication.p_qdport ) ;
    fillout_ScalingMatrix( p_vout ) ;
    fillout_ImageDescription(
        p_vout->p_sys->h_img_descr,
        p_vout->i_width,
        p_vout->i_height,
        'y420'
    ) ;
    if ( !new_QTSequence(
            &p_vout->p_sys->i_seq,
            p_vout->p_sys->osx_communication.p_qdport,
            p_vout->p_sys->h_img_descr,
            p_vout->p_sys->p_matrix
        ) )
    {
        p_vout->p_sys->c_codec = 'y420' ;
        return 0 ;
    }
   
    p_vout->p_sys->c_codec = 'NONE' ;
    return 1 ;
}

static void dispose_QTSequence( vout_thread_t *p_vout )
{
    if (p_vout->p_sys->c_codec == 'NONE')
    	return ;
    	
    CDSequenceEnd( p_vout->p_sys->i_seq ) ;
    switch (p_vout->p_sys->c_codec)
    {
        case 'yuv2':
            free( (void *)p_vout->p_sys->p_yuv2 ) ;
            p_vout->p_sys->i_yuv2_size = 0 ;
            break ;
        default:
	    break ;            
    }
    p_vout->p_sys->c_codec = 'NONE' ;
}

static void convert_Y420_to_YUV2( picture_t *p_y420, yuv2_data_t *p_yuv2 )
{
    unsigned int width = p_y420->i_width, height = p_y420->i_height ;
    unsigned int x, y ;
    
    for( x=0; x < height; x++ )
    {
        for( y=0; y < (width/2); y++ )
        {
            p_yuv2[(width/2)*x + y] =
                (p_y420->p_y[width*x + 2*y]) << 24 |
                ((p_y420->p_u[(width/2)*(x/2) + y] ^ 0x80) << 16) |
                (p_y420->p_y[width*x + 2*y + 1] << 8) |
                (p_y420->p_v[(width/2)*(x/2) + y] ^ 0x80) ;   
        }
    }
}
