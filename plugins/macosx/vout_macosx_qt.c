/*****************************************************************************
 * vout_macosx.c: MacOS X video output plugin
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 *
 * Authors: Colin Delacroix <colin@zoy.org>
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

#include "macosx_qt_common.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  vout_Probe     ( probedata_t *p_data );
static int  vout_Create    ( struct vout_thread_s * );
static int  vout_Init      ( struct vout_thread_s * );
static void vout_End       ( struct vout_thread_s * );
static void vout_Destroy   ( struct vout_thread_s * );
static int  vout_Manage    ( struct vout_thread_s * );
void vout_OSX_Display   ( struct vout_thread_s * );

/* OS specific */
static int vout_OSX_create_sequence( vout_thread_t *p_vout ) ;

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
    p_function_list->functions.vout.pf_display    = vout_OSX_Display;
    p_function_list->functions.vout.pf_setpalette = NULL;
}

/*****************************************************************************
 * intf_Probe: return a score
 *****************************************************************************/
static int vout_Probe( probedata_t *p_data )
{
    if( TestMethod( VOUT_METHOD_VAR, "macosx_qt" ) )
    {
        return( 999 );
    }

    return( 90 );
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

    EnterMovies() ;

    return( 0 );
}

/*****************************************************************************
 * MakeWindow: open and set-up a Mac OS main window
 *****************************************************************************/
static int vout_OSX_create_sequence( vout_thread_t *p_vout )
{
    ImageDescriptionPtr descr_ptr ;
    OSErr qterror ;

    p_vout->p_sys->h_img_descr = (ImageDescriptionHandle)NewHandleClear(sizeof(ImageDescription)) ;
    p_vout->p_sys->i_img_size = p_vout->i_width * p_vout->i_height * 1.5 ; //before: 2
    if (! (p_vout->p_sys->p_img  = malloc(p_vout->p_sys->i_img_size))) {
    	intf_ErrMsg("couldn't allocate image:") ;
    	return 1 ;
    }

    HLock((Handle)p_vout->p_sys->h_img_descr) ; 
    descr_ptr = *p_vout->p_sys->h_img_descr ;
    descr_ptr->idSize = sizeof(ImageDescription) ;
    descr_ptr->cType = 'y420' ; //before: yuv2
    descr_ptr->resvd1 = 0 ; //Reserved
    descr_ptr->resvd2 = 0 ; //Reserved
    descr_ptr->dataRefIndex = 0 ; //Reserved
    descr_ptr->version = 1 ; //
    descr_ptr->revisionLevel = 0 ;
    descr_ptr->vendor = 'appl' ; //How do we get a vendor id??
    descr_ptr->width = p_vout->i_width  ;
    descr_ptr->height = p_vout->i_height ;
    descr_ptr->hRes = Long2Fix(72) ;
    descr_ptr->vRes = Long2Fix(72) ;
    descr_ptr->spatialQuality = codecLosslessQuality ;
    descr_ptr->dataSize = p_vout->p_sys->i_img_size ;
    descr_ptr->frameCount = 1 ;
    //memcpy(descr_ptr->name, "\pComponent Video\0") ;
    descr_ptr->depth = 12 ; //before: 24
    descr_ptr->clutID = -1 ; //We don't need a color table
    HUnlock((Handle)p_vout->p_sys->h_img_descr) ;

    SetPortWindowPort(p_main->p_intf->p_sys->p_window) ;
    qterror = DecompressSequenceBeginS(&p_vout->p_sys->i_seq, 
                                p_vout->p_sys->h_img_descr,
                                (void *)p_vout->p_sys->p_img,
                                p_vout->p_sys->i_img_size,
                                GetWindowPort(p_main->p_intf->p_sys->p_window),
                                NULL, //device to display (is set implicit via the qdPort)
                                NULL, //src-rect
                                NULL, //matrix
                                0, //just do plain copying
                                NULL, //no mask region
                                codecFlagUseScreenBuffer,
                                codecLosslessQuality,
                                (DecompressorComponent) bestSpeedCodec) ;
   intf_WarnMsg(1, "DecompressSequenceBeginS: %d\n", qterror) ;
   
   if (qterror)
   {
	return(1) ; 
   }

   p_vout->b_need_render = 0 ;
   p_vout->i_bytes_per_line = p_vout->i_width ;

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize video thread output method
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    if ( vout_OSX_create_sequence( p_vout ) )
    {
        intf_ErrMsg( "vout error: can't create quicktime view" );
        free( p_vout->p_sys );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * vout_End: terminate video thread output method
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
	CDSequenceEnd(p_vout->p_sys->i_seq) ;
}

/*****************************************************************************
 * vout_Destroy: destroy video thread output method
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
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
    return( 0 );
}


/*****************************************************************************
 * vout_OSX_Display: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to image, waits until
 * it is displayed and switch the two rendering buffers, preparing next frame.
 *****************************************************************************/
void vout_OSX_Display( vout_thread_t *p_vout )
{
	OSErr qterror ;
//	unsigned int x,y, height=p_vout->i_height, width=p_vout->i_width ;
	CodecFlags out_flags ;
	PlanarPixmapInfoYUV420 y420_info ;

	if (!p_main->p_intf->p_sys->b_active)
		return ;

/*	for(x=0; x < height; x++)
	{
		for(y=0; y < (width/2); y++)
		{
			p_vout->p_sys->p_img[(width/2)*x + y] =
				(p_vout->p_rendered_pic->p_y[width*x + 2*y]) << 24 |
				((p_vout->p_rendered_pic->p_u[(width/2)*(x/2) + y] ^ 0x80) << 16) |
				(p_vout->p_rendered_pic->p_y[width*x + 2*y + 1] << 8) |
				(p_vout->p_rendered_pic->p_v[(width/2)*(x/2) + y] ^ 0x80) ;   
		}
	}
*/

	y420_info.componentInfoY.offset = p_vout->p_rendered_pic->p_y - p_vout->p_sys->p_img ;
	y420_info.componentInfoY.rowBytes = p_vout->i_width ;
	y420_info.componentInfoCb.offset = p_vout->p_rendered_pic->p_u - p_vout->p_sys->p_img ;
	y420_info.componentInfoCb.rowBytes = p_vout->i_width / 2;
	y420_info.componentInfoCr.offset = p_vout->p_rendered_pic->p_v - p_vout->p_sys->p_img ;
	y420_info.componentInfoCr.rowBytes = p_vout->i_width / 2;

	memcpy(p_vout->p_sys->p_img, &y420_info, sizeof(PlanarPixmapInfoYUV420)) ;

	qterror = DecompressSequenceFrameWhen(
		p_vout->p_sys->i_seq,
		(void *)p_vout->p_sys->p_img,
		p_vout->p_sys->i_img_size,
		codecFlagUseScreenBuffer,
 		&out_flags,
 		nil,
 		nil) ;

// 	intf_WarnMsg(1, "DecompressSequenceFrameWhen: %d", qterror) ;
 }
