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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include <videolan/vlc.h>

#include "interface.h"

#include "video.h"
#include "video_output.h"

#define OSX_COM_STRUCT intf_sys_s
#define OSX_COM_TYPE intf_sys_t
#include "macosx.h"

#include <QuickTime/QuickTime.h>

/*****************************************************************************
 * vout_sys_t: MacOS X video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the MacOS X specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_sys_s
{
    /* QT sequence information */
    ImageDescriptionHandle h_img_descr ;
    ImageSequence i_seq ;   
    unsigned int c_codec ;
    MatrixRecordPtr p_matrix ;

} vout_sys_t;

typedef struct picture_sys_s
{
    void *p_info;
    unsigned int i_size;

    /* When using I420 output */
    PlanarPixmapInfoYUV420 pixmap_i420 ;

} picture_sys_t;

#define MAX_DIRECTBUFFERS 10

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  vout_Create    ( vout_thread_t * );
static int  vout_Init      ( vout_thread_t * );
static void vout_End       ( vout_thread_t * );
static void vout_Destroy   ( vout_thread_t * );
static int  vout_Manage    ( vout_thread_t * );
static void vout_Display   ( vout_thread_t *, picture_t * );
static void vout_Render    ( vout_thread_t *, picture_t * );

/* OS Specific */
static int  CreateQTSequence ( vout_thread_t *p_vout ) ;
static void DestroyQTSequence( vout_thread_t *p_vout ) ;

static int  NewPicture     ( vout_thread_t *, picture_t * );
static void FreePicture    ( vout_thread_t *, picture_t * );

static void fillout_ImageDescription(ImageDescriptionHandle h_descr,
                        unsigned int i_with, unsigned int i_height,
                        unsigned int c_codec) ;
static void fillout_ScalingMatrix( vout_thread_t *p_vout ) ;

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( vout_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->functions.vout.pf_create     = vout_Create;
    p_function_list->functions.vout.pf_init       = vout_Init;
    p_function_list->functions.vout.pf_end        = vout_End;
    p_function_list->functions.vout.pf_destroy    = vout_Destroy;
    p_function_list->functions.vout.pf_manage     = vout_Manage;
    p_function_list->functions.vout.pf_render     = vout_Render;
    p_function_list->functions.vout.pf_display    = vout_Display;
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
    p_main->p_intf->p_sys->i_changes = 0;
    p_main->p_intf->p_sys->p_vout = p_vout;
    p_vout->p_sys->h_img_descr = (ImageDescriptionHandle)NewHandleClear( sizeof( ImageDescription ) ) ;
    p_vout->p_sys->p_matrix = (MatrixRecordPtr)malloc( sizeof( MatrixRecord ) ) ;
    EnterMovies() ;

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize video thread output method
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;

    I_OUTPUTPICTURES = 0;

    /* Since we can arbitrary scale, stick to the coordinates and aspect. */
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    CreateQTSequence( p_vout ) ;

    switch( p_vout->p_sys->c_codec )
    {
        case 'yuv2':
            p_vout->output.i_chroma = FOURCC_YUY2;
            break;
        case 'y420':
            p_vout->output.i_chroma = FOURCC_I420;
            break;
        case 'NONE':
            intf_ErrMsg( "vout error: no QT codec found" );
            return 0;
        default:
            intf_ErrMsg( "vout error: unknown QT codec" );
            return 0;
    }

    /* Try to initialize up to MAX_DIRECTBUFFERS direct buffers */
    while( I_OUTPUTPICTURES < MAX_DIRECTBUFFERS )
    {
        p_pic = NULL;

        /* Find an empty picture slot */
        for( i_index = 0 ; i_index < VOUT_MAX_PICTURES ; i_index++ )
        {
            if( p_vout->p_picture[ i_index ].i_status == FREE_PICTURE )
            {
                p_pic = p_vout->p_picture + i_index;
                break;
            }
        }

        /* Allocate the picture */
        if( p_pic == NULL || NewPicture( p_vout, p_pic ) )
        {
            break;
        }

        p_pic->i_status = DESTROYED_PICTURE;
        p_pic->i_type   = DIRECT_PICTURE;

        PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

        I_OUTPUTPICTURES++;
    }

    return 0 ;
}

/*****************************************************************************
 * vout_End: terminate video thread output method
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    int i_index;

    DestroyQTSequence( p_vout ) ;
    p_main->p_intf->p_sys->p_vout = NULL;
    p_main->p_intf->p_sys->i_changes |= OSX_VOUT_INTF_RELEASE_QDPORT ;

    /* Free the direct buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        FreePicture( p_vout, PP_OUTPUTPICTURE[ i_index ] );
    }
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
    if( p_main->p_intf->p_sys->i_changes
         & OSX_INTF_VOUT_QDPORT_CHANGE )
    {
        intf_ErrMsg( "vout error: this change is unhandled yet !" );
        return 1;
    }
    else if( p_main->p_intf->p_sys->i_changes
              & OSX_INTF_VOUT_SIZE_CHANGE )
    {
        if( p_vout->p_sys->c_codec != 'NONE' )
        {
            fillout_ScalingMatrix( p_vout ) ;
            SetDSequenceMatrix( p_vout->p_sys->i_seq,
                                p_vout->p_sys->p_matrix ) ;
        }
    }

    /* Clear flags */
    p_main->p_intf->p_sys->i_changes &= ~( 
        OSX_INTF_VOUT_QDPORT_CHANGE |
        OSX_INTF_VOUT_SIZE_CHANGE
    ) ;

    return 0 ;
}


/*****************************************************************************
 * vout_Render: renders previously calculated output
 *****************************************************************************/
void vout_Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    ;
}

 /*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to image, waits until
 * it is displayed and switch the two rendering buffers, preparing next frame.
 *****************************************************************************/
void vout_Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    CodecFlags out_flags ;

    switch (p_vout->p_sys->c_codec)
    {
        case 'yuv2':
        case 'y420':
            DecompressSequenceFrameS(
		p_vout->p_sys->i_seq,
		p_pic->p_sys->p_info,
		p_pic->p_sys->i_size,
		codecFlagUseScreenBuffer,
 		&out_flags,
 		nil
            ) ;
            break ;
        default:
            intf_WarnMsg( 1, "vout_macosx: vout_Display called, but no codec available" ) ;
            break;
    }
}

static int CreateQTSequence( vout_thread_t *p_vout )
{
    p_vout->p_sys->c_codec = 'NONE' ;
    p_main->p_intf->p_sys->i_changes |= OSX_VOUT_INTF_REQUEST_QDPORT ;
    
    while ( p_main->p_intf->p_sys->p_qdport == nil
             && !p_vout->b_die )
    {
printf("WAITING for QD port ...\n");
        if( p_main->p_intf->p_sys->i_changes
             & OSX_INTF_VOUT_QDPORT_CHANGE )
        {
            p_main->p_intf->p_sys->i_changes &= ~( OSX_INTF_VOUT_QDPORT_CHANGE ) ;
            intf_ErrMsg( "got a QDPORT_CHANGE" );
            break;
        }
        msleep( 300000 );
    }

    if ( p_main->p_intf->p_sys->p_qdport == nil)
    {
printf("BLAAAAAAAAAAH\n");
        p_vout->p_sys->c_codec = 'NONE' ;
        return 1 ;
    }

    SetPort( p_main->p_intf->p_sys->p_qdport ) ;
    fillout_ScalingMatrix( p_vout ) ;

    fillout_ImageDescription( p_vout->p_sys->h_img_descr,
                              p_vout->output.i_width, p_vout->output.i_height,
                              'y420' ) ;

    if( !DecompressSequenceBeginS( &p_vout->p_sys->i_seq,
            p_vout->p_sys->h_img_descr, NULL, 0,
            p_main->p_intf->p_sys->p_qdport,
            NULL, //device to display (is set implicit via the qdPort)
            NULL, //src-rect
            p_vout->p_sys->p_matrix, //matrix
            0, //just do plain copying
            NULL, //no mask region
            codecFlagUseScreenBuffer, codecLosslessQuality,
            (DecompressorComponent) bestSpeedCodec) )
    {
printf("OK !!!\n");
        p_vout->p_sys->c_codec = 'y420' ;
        return 0 ;
    }

#if 0
    /* For yuv2 */
    {
        p_vout->p_sys->c_codec = 'yuv2' ;
    }
#endif
   
printf("FUXK..\n");
    p_vout->p_sys->c_codec = 'NONE' ;
    return 1 ;
}

static void DestroyQTSequence( vout_thread_t *p_vout )
{
    if (p_vout->p_sys->c_codec == 'NONE')
    	return ;
    	
    CDSequenceEnd( p_vout->p_sys->i_seq ) ;
    p_vout->p_sys->c_codec = 'NONE' ;
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
            p_descr->dataSize=i_width * i_height * 3 / 2 ;
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
        	
	GetPortBounds( p_main->p_intf->p_sys->p_qdport, &s_rect ) ;
//	if (((s_rect.right - s_rect.left) / ((float) p_vout->i_width)) < ((s_rect.bottom - s_rect.top) / ((float) p_vout->i_height)))
		factor_x = FixDiv(Long2Fix(s_rect.right - s_rect.left), Long2Fix(p_vout->output.i_width)) ;
//	else
		factor_y = FixDiv(Long2Fix(s_rect.bottom - s_rect.top), Long2Fix(p_vout->output.i_height)) ;
	
	SetIdentityMatrix(p_vout->p_sys->p_matrix) ;
	ScaleMatrix( p_vout->p_sys->p_matrix, factor_x, factor_y, Long2Fix(0), Long2Fix(0) ) ;
}

/*****************************************************************************
 * NewPicture: allocate a picture
 *****************************************************************************
 * Returns 0 on success, -1 otherwise
 *****************************************************************************/
static int NewPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* We know the chroma, allocate a buffer which will be used
     * directly by the decoder */
    p_pic->p_sys = malloc( sizeof( picture_sys_t ) );

    if( p_pic->p_sys == NULL )
    {
        return -1;
    }

    switch( p_vout->output.i_chroma )
    {
        case FOURCC_I420:

            p_pic->p_sys->p_info = (void *)&p_pic->p_sys->pixmap_i420 ;
            p_pic->p_sys->i_size = sizeof(PlanarPixmapInfoYUV420) ;

            p_pic->Y_PIXELS = memalign( 16, p_vout->output.i_width
                                           * p_vout->output.i_height * 3 / 2 );
            p_pic->p[Y_PLANE].i_lines = p_vout->output.i_height;
            p_pic->p[Y_PLANE].i_pitch = p_vout->output.i_width;
            p_pic->p[Y_PLANE].i_pixel_bytes = 1;
            p_pic->p[Y_PLANE].b_margin = 0;

            p_pic->U_PIXELS = p_pic->Y_PIXELS + p_vout->output.i_width
                                                 * p_vout->output.i_height;
            p_pic->p[U_PLANE].i_lines = p_vout->output.i_height / 2;
            p_pic->p[U_PLANE].i_pitch = p_vout->output.i_width / 2;
            p_pic->p[U_PLANE].i_pixel_bytes = 1;
            p_pic->p[U_PLANE].b_margin = 0;

            p_pic->V_PIXELS = p_pic->U_PIXELS + p_vout->output.i_width
                                                 * p_vout->output.i_height / 4;
            p_pic->p[V_PLANE].i_lines = p_vout->output.i_height / 2;
            p_pic->p[V_PLANE].i_pitch = p_vout->output.i_width / 2;
            p_pic->p[V_PLANE].i_pixel_bytes = 1;
            p_pic->p[V_PLANE].b_margin = 0;

            p_pic->i_planes = 3;

#define P p_pic->p_sys->pixmap_i420
            P.componentInfoY.offset = (void *)p_pic->Y_PIXELS
                                       - p_pic->p_sys->p_info;
            P.componentInfoCb.offset = (void *)p_pic->U_PIXELS
                                        - p_pic->p_sys->p_info;
            P.componentInfoCr.offset = (void *)p_pic->V_PIXELS
                                        - p_pic->p_sys->p_info;

            P.componentInfoY.rowBytes = p_vout->output.i_width ;
            P.componentInfoCb.rowBytes = p_vout->output.i_width / 2 ;
            P.componentInfoCr.rowBytes = p_vout->output.i_width / 2 ;
#undef P

            break;

        case FOURCC_YUY2:

            /* XXX: TODO */
            free( p_pic->p_sys );
            intf_ErrMsg( "vout error: YUV2 not supported yet" );
            p_pic->i_planes = 0;
            break;

        default:
            /* Unknown chroma, tell the guy to get lost */
            free( p_pic->p_sys );
            intf_ErrMsg( "vout error: never heard of chroma 0x%.8x (%4.4s)",
                         p_vout->output.i_chroma,
                         (char*)&p_vout->output.i_chroma );
            p_pic->i_planes = 0;
            return -1;
    }

    return 0;
}

/*****************************************************************************
 * FreePicture: destroy a picture allocated with NewPicture
 *****************************************************************************/
static void FreePicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    switch (p_vout->p_sys->c_codec)
    {
        case 'yuv2':
            free( p_pic->p_sys->p_info ) ;
            p_pic->p_sys->i_size = 0 ;
            break ;
        case 'y420':
            break ;
        default:
	    break ;            
    }

    free( p_pic->p_sys );
}

