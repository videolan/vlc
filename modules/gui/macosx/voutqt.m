/*****************************************************************************
 * vout.m: MacOS X video output module
 *****************************************************************************
 * Copyright (C) 2001-2004 the VideoLAN team
 * $Id: vout.m 8351 2004-08-02 13:06:38Z hartman $
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Eric Petit <titer@m0k.org>
 *          Benjamin Pracht <bigben AT videolan DOT org>
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
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include <QuickTime/QuickTime.h>

#include <vlc_keys.h>

#include "intf.h"
#include "vout.h"

#define QT_MAX_DIRECTBUFFERS 10
#define VL_MAX_DISPLAYS 16

/*****************************************************************************
 * VLCView interface
 *****************************************************************************/
@interface VLCQTView : NSQuickDrawView
{
    vout_thread_t * p_vout;
}

- (id) initWithVout:(vout_thread_t *)p_vout;

@end

struct vout_sys_t
{
    NSAutoreleasePool *o_pool;
    VLCQTView * o_qtview;
    VLCVoutView       * o_vout_view;

    vlc_bool_t  b_saved_frame;
    vlc_bool_t  b_cpu_has_simd; /* does CPU supports Altivec, MMX, etc... */
    NSRect      s_frame;

    CodecType i_codec;
    CGrafPtr p_qdport;
    ImageSequence i_seq;
    MatrixRecordPtr p_matrix;
    DecompressorComponent img_dc;
    ImageDescriptionHandle h_img_descr;

    /* video geometry in port */
    int i_origx, i_origy;
    int i_width, i_height;
    /* Mozilla plugin-related variables */
    vlc_bool_t b_embedded;
    RgnHandle clip_mask;
};

struct picture_sys_t
{
    void *p_data;
    unsigned int i_size;

    /* When using I420 output */
    PlanarPixmapInfoYUV420 pixmap_i420;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int  InitVideo           ( vout_thread_t * );
static void EndVideo            ( vout_thread_t * );
static int  ManageVideo         ( vout_thread_t * );
static void DisplayVideo        ( vout_thread_t *, picture_t * );
static int  ControlVideo        ( vout_thread_t *, int, va_list );

static int CoToggleFullscreen( vout_thread_t *p_vout );
static int DrawableRedraw( vlc_object_t *p_this, const char *psz_name,
    vlc_value_t oval, vlc_value_t nval, void *param);
static void UpdateEmbeddedGeometry( vout_thread_t *p_vout );
static void QTScaleMatrix       ( vout_thread_t * );
static int  QTCreateSequence    ( vout_thread_t * );
static void QTDestroySequence   ( vout_thread_t * );
static int  QTNewPicture        ( vout_thread_t *, picture_t * );
static void QTFreePicture       ( vout_thread_t *, picture_t * );

/*****************************************************************************
 * OpenVideo: allocates MacOS X video thread output method
 *****************************************************************************
 * This function allocates and initializes a MacOS X vout method.
 *****************************************************************************/
int E_(OpenVideoQT) ( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;
    OSErr err;
    vlc_value_t value_drawable;

    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return( 1 );
    }

    memset( p_vout->p_sys, 0, sizeof( vout_sys_t ) );

    p_vout->p_sys->o_pool = [[NSAutoreleasePool alloc] init];

    p_vout->pf_init = InitVideo;
    p_vout->pf_end = EndVideo;
    p_vout->pf_manage = ManageVideo;
    p_vout->pf_render = NULL;
    p_vout->pf_display = DisplayVideo;
    p_vout->pf_control = ControlVideo;

    /* Are we embedded?  If so, the drawable value will be a pointer to a
     * CGrafPtr that we're expected to use */
    var_Get( p_vout->p_vlc, "drawable", &value_drawable );
    if( value_drawable.i_int != 0 )
        p_vout->p_sys->b_embedded = VLC_TRUE;
    else
        p_vout->p_sys->b_embedded = VLC_FALSE;

    p_vout->p_sys->b_cpu_has_simd = (p_vout->p_libvlc->i_cpu & CPU_CAPABILITY_ALTIVEC)
                                  | (p_vout->p_libvlc->i_cpu & CPU_CAPABILITY_MMXEXT);
    msg_Dbg( p_vout, "we do%s have SIMD enabled CPU", p_vout->p_sys->b_cpu_has_simd ? "" : "n't" );
    
    /* Initialize QuickTime */
    p_vout->p_sys->h_img_descr = 
        (ImageDescriptionHandle)NewHandleClear( sizeof(ImageDescription) );
    p_vout->p_sys->p_matrix =
        (MatrixRecordPtr)malloc( sizeof(MatrixRecord) );

    if( ( err = EnterMovies() ) != noErr )
    {
        msg_Err( p_vout, "QT initialization failed: EnterMovies failed: %d", err );
        free( p_vout->p_sys->p_matrix );
        DisposeHandle( (Handle)p_vout->p_sys->h_img_descr );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    /* Damn QT isn't thread safe. so keep a lock in the p_vlc object */
    vlc_mutex_lock( &p_vout->p_vlc->quicktime_lock );

    /* Can we find the right chroma ? */
    if( p_vout->p_sys->b_cpu_has_simd )
    {
        err = FindCodec( kYUVSPixelFormat, bestSpeedCodec,
                        nil, &p_vout->p_sys->img_dc );
    }
    else
    {
        err = FindCodec( kYUV420CodecType, bestSpeedCodec,
                        nil, &p_vout->p_sys->img_dc );
    }
    vlc_mutex_unlock( &p_vout->p_vlc->quicktime_lock );
    
    if( err == noErr && p_vout->p_sys->img_dc != 0 )
    {
        if( p_vout->p_sys->b_cpu_has_simd )
        {
            p_vout->output.i_chroma = VLC_FOURCC('Y','U','Y','2');
            p_vout->p_sys->i_codec = kYUVSPixelFormat;
        }
        else
        {
            p_vout->output.i_chroma = VLC_FOURCC('I','4','2','0');
            p_vout->p_sys->i_codec = kYUV420CodecType;
        }
    }
    else
    {
        msg_Err( p_vout, "QT doesn't support any appropriate chroma" );
    }

    if( p_vout->p_sys->img_dc == 0 )
    {
        free( p_vout->p_sys->p_matrix );
        DisposeHandle( (Handle)p_vout->p_sys->h_img_descr );
        free( p_vout->p_sys );
        return VLC_EGENERIC;        
    }

    if( p_vout->b_fullscreen || !p_vout->p_sys->b_embedded )
    {
        /* Spawn window */
#define o_qtview p_vout->p_sys->o_qtview
        o_qtview = [[VLCQTView alloc] initWithVout: p_vout];
        [o_qtview autorelease];

        p_vout->p_sys->o_vout_view = [VLCVoutView getVoutView: p_vout
                    subView: o_qtview frame: nil];
        if( !p_vout->p_sys->o_vout_view )
        {
            return VLC_EGENERIC;
        }
        [o_qtview lockFocus];
        p_vout->p_sys->p_qdport = [o_qtview qdPort];
        [o_qtview unlockFocus];
    }
#undef o_qtview

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseVideo: destroy video thread output method
 *****************************************************************************/
void E_(CloseVideoQT) ( vlc_object_t *p_this )
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    vout_thread_t * p_vout = (vout_thread_t *)p_this;

    if( p_vout->b_fullscreen || !p_vout->p_sys->b_embedded )
        [p_vout->p_sys->o_vout_view closeVout];

    /* Clean Up Quicktime environment */
    ExitMovies();
    free( p_vout->p_sys->p_matrix );
    DisposeHandle( (Handle)p_vout->p_sys->h_img_descr );

    [o_pool release];
    free( p_vout->p_sys );
}

/*****************************************************************************
 * InitVideo: initialize video thread output method
 *****************************************************************************/
static int InitVideo    ( vout_thread_t *p_vout )
{
    picture_t *p_pic;
    int i_index;

    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure; we already found a codec,
     * and the corresponding chroma we will be using. Since we can
     * arbitrary scale, stick to the coordinates and aspect. */
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    if( p_vout->b_fullscreen || !p_vout->p_sys->b_embedded )
    {
	Rect s_rect;
        p_vout->p_sys->clip_mask = NULL;
	GetPortBounds( p_vout->p_sys->p_qdport, &s_rect );
	p_vout->p_sys->i_origx = s_rect.left;
	p_vout->p_sys->i_origy = s_rect.top;
	p_vout->p_sys->i_width = s_rect.right - s_rect.left;
	p_vout->p_sys->i_height = s_rect.bottom - s_rect.top;
    }
    else
    {
	/* As we are embedded (e.g. running as a Mozilla plugin), use the pointer
	 * stored in the "drawable" value as the CGrafPtr for the QuickDraw
	 * graphics port */
        /* Create the clipping mask */
        p_vout->p_sys->clip_mask = NewRgn();
	UpdateEmbeddedGeometry(p_vout);
	var_AddCallback(p_vout->p_vlc, "drawableredraw", DrawableRedraw, p_vout);
    }

    QTScaleMatrix( p_vout );

    if( QTCreateSequence( p_vout ) )
    {
        msg_Err( p_vout, "unable to initialize QT: QTCreateSequence failed" );
        return( 1 );
    }

    /* Try to initialize up to QT_MAX_DIRECTBUFFERS direct buffers */
    while( I_OUTPUTPICTURES < QT_MAX_DIRECTBUFFERS )
    {
        p_pic = NULL;

        /* Find an empty picture slot */
        for( i_index = 0; i_index < VOUT_MAX_PICTURES; i_index++ )
        {
            if( p_vout->p_picture[ i_index ].i_status == FREE_PICTURE )
            {
                p_pic = p_vout->p_picture + i_index;
                break;
            }
        }

        /* Allocate the picture */
        if( p_pic == NULL || QTNewPicture( p_vout, p_pic ) )
        {
            break;
        }

        p_pic->i_status = DESTROYED_PICTURE;
        p_pic->i_type   = DIRECT_PICTURE;

        PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;
        I_OUTPUTPICTURES++;
    }
    return 0;
}

/*****************************************************************************
 * EndVideo: terminate video thread output method
 *****************************************************************************/
static void EndVideo( vout_thread_t *p_vout )
{
    int i_index;

    QTDestroySequence( p_vout );

    if( !p_vout->b_fullscreen && p_vout->p_sys->b_embedded )
    {
	var_DelCallback(p_vout->p_vlc, "drawableredraw", DrawableRedraw, p_vout);
	DisposeRgn(p_vout->p_sys->clip_mask);
    }

    /* Free the direct buffers we allocated */
    for( i_index = I_OUTPUTPICTURES; i_index; )
    {
        i_index--;
        QTFreePicture( p_vout, PP_OUTPUTPICTURE[ i_index ] );
    }
}

/*****************************************************************************
 * ManageVideo: handle events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int ManageVideo( vout_thread_t *p_vout )
{
    if( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        if( CoToggleFullscreen( p_vout ) )  
        {
            return( 1 );
        }

        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }

    if( p_vout->i_changes & VOUT_SIZE_CHANGE )
    {
	if( p_vout->b_fullscreen || !p_vout->p_sys->b_embedded )
	{
	    /* get the geometry from NSQuickDrawView */
	    Rect s_rect;
	    GetPortBounds( p_vout->p_sys->p_qdport, &s_rect );
	    p_vout->p_sys->i_origx = s_rect.left;
	    p_vout->p_sys->i_origy = s_rect.top;
	    p_vout->p_sys->i_width = s_rect.right - s_rect.left;
	    p_vout->p_sys->i_height = s_rect.bottom - s_rect.top;
	}
	else 
	{
	    /* As we're embedded, get the geometry from Mozilla/Safari NPWindow object */
	    UpdateEmbeddedGeometry( p_vout );
	    SetDSequenceMask(p_vout->p_sys->i_seq,
		p_vout->p_sys->clip_mask);
	}
    }

    if( p_vout->i_changes & VOUT_SIZE_CHANGE ||
        p_vout->i_changes & VOUT_ASPECT_CHANGE )
    {
        QTScaleMatrix( p_vout );
        SetDSequenceMatrix( p_vout->p_sys->i_seq,
                            p_vout->p_sys->p_matrix );
    }
    if( p_vout->i_changes & VOUT_SIZE_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_SIZE_CHANGE;
    }
    if( p_vout->i_changes & VOUT_ASPECT_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_ASPECT_CHANGE;
    }

    // can be nil
    [p_vout->p_sys->o_vout_view manage];

    return( 0 );
}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function sends the currently rendered image to the display.
 *****************************************************************************/
static void DisplayVideo( vout_thread_t *p_vout, picture_t *p_pic )
{
    OSErr err;
    CodecFlags flags;
    if( (NULL == p_vout->p_sys->clip_mask) || !EmptyRgn(p_vout->p_sys->clip_mask) )
    {
	//CGrafPtr oldPort;
	//Rect oldBounds;

	/* since there is not way to lock a QuickDraw port for exclusive use
	   there is a potential problem that the frame will be displayed
	   in the wrong place if other embedded plugins redraws as the port
	   origin may be changed */
	//GetPort(&oldPort);
	//GetPortBounds(p_vout->p_sys->p_qdport, &oldBounds);
	SetPort(p_vout->p_sys->p_qdport);
	SetOrigin(p_vout->p_sys->i_origx, p_vout->p_sys->i_origy);
	if( ( err = DecompressSequenceFrameWhen(
			p_vout->p_sys->i_seq,
			p_pic->p_sys->p_data,
			p_pic->p_sys->i_size,
			codecFlagUseImageBuffer, &flags, NULL, NULL ) == noErr ) )
	{
	    QDFlushPortBuffer( p_vout->p_sys->p_qdport, p_vout->p_sys->clip_mask );
	    //QDFlushPortBuffer( p_vout->p_sys->p_qdport, NULL );
	}
	else
	{
	    msg_Warn( p_vout, "QT failed to display the frame sequence: %d", err );
	}
	//SetPortBounds(p_vout->p_sys->p_qdport, &oldBounds);
	//SetPort(oldPort);
    }
}

/*****************************************************************************
 * ControlVideo: control facility for the vout
 *****************************************************************************/
static int ControlVideo( vout_thread_t *p_vout, int i_query, va_list args )
{
    vlc_bool_t b_arg;

    switch( i_query )
    {
        case VOUT_SET_STAY_ON_TOP:
            b_arg = va_arg( args, vlc_bool_t );
            [p_vout->p_sys->o_vout_view setOnTop: b_arg];
            return VLC_SUCCESS;

        case VOUT_CLOSE:
        case VOUT_REPARENT:
        default:
            return vout_vaControlDefault( p_vout, i_query, args );
    }
}

/*****************************************************************************
 * CoToggleFullscreen: toggle fullscreen 
 *****************************************************************************
 * Returns 0 on success, 1 otherwise
 *****************************************************************************/
static int CoToggleFullscreen( vout_thread_t *p_vout )
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

    QTDestroySequence( p_vout );

    if( !p_vout->b_fullscreen )
    {
	if( !p_vout->p_sys->b_embedded )
	{
	    /* Save window size and position */
	    p_vout->p_sys->s_frame.size =
		[p_vout->p_sys->o_vout_view frame].size;
	    p_vout->p_sys->s_frame.origin =
		[[p_vout->p_sys->o_vout_view getWindow] frame].origin;
	    p_vout->p_sys->b_saved_frame = VLC_TRUE;
	}
	else
	{
	    var_DelCallback(p_vout->p_vlc, "drawableredraw", DrawableRedraw, p_vout);
	    DisposeRgn(p_vout->p_sys->clip_mask);
	}
    }
    [p_vout->p_sys->o_vout_view closeVout];

    p_vout->b_fullscreen = !p_vout->b_fullscreen;

    if( p_vout->b_fullscreen || !p_vout->p_sys->b_embedded )
    {
	Rect s_rect;
        p_vout->p_sys->clip_mask = NULL;
#define o_qtview p_vout->p_sys->o_qtview
	o_qtview = [[VLCQTView alloc] initWithVout: p_vout];
	[o_qtview autorelease];
	
	if( p_vout->p_sys->b_saved_frame )
	{
	    p_vout->p_sys->o_vout_view = [VLCVoutView getVoutView: p_vout
		subView: o_qtview
		frame: &p_vout->p_sys->s_frame];
	}
	else
	{
	    p_vout->p_sys->o_vout_view = [VLCVoutView getVoutView: p_vout
		subView: o_qtview frame: nil];
	}

	/* Retrieve the QuickDraw port */
	[o_qtview lockFocus];
	p_vout->p_sys->p_qdport = [o_qtview qdPort];
	[o_qtview unlockFocus];
#undef o_qtview
	GetPortBounds( p_vout->p_sys->p_qdport, &s_rect );
	p_vout->p_sys->i_origx = s_rect.left;
	p_vout->p_sys->i_origy = s_rect.top;
	p_vout->p_sys->i_width = s_rect.right - s_rect.left;
	p_vout->p_sys->i_height = s_rect.bottom - s_rect.top;
    }
    else
    {
        /* Create the clipping mask */
        p_vout->p_sys->clip_mask = NewRgn();
	UpdateEmbeddedGeometry(p_vout);
	var_AddCallback(p_vout->p_vlc, "drawableredraw", DrawableRedraw, p_vout);
    }
    QTScaleMatrix( p_vout );

    if( QTCreateSequence( p_vout ) )
    {
        msg_Err( p_vout, "unable to initialize QT: QTCreateSequence failed" );
        return( 1 );
    }

    [o_pool release];
    return 0;
}

/* If we're embedded, the application is expected to indicate a
 * window change (move/resize/etc) via the "drawableredraw" value.
 * If that's the case, set the VOUT_SIZE_CHANGE flag so we do
 * actually handle the window change. */

static int DrawableRedraw( vlc_object_t *p_this, const char *psz_name,
    vlc_value_t oval, vlc_value_t nval, void *param)
{
    /* ignore changes until we are ready for them */
    if( (oval.i_int != nval.i_int) && (nval.i_int == 1) )
    {
	vout_thread_t *p_vout = (vout_thread_t *)param;
	/* prevent QT from rendering any more video until we have updated
	   the geometry */
	SetEmptyRgn(p_vout->p_sys->clip_mask);
	SetDSequenceMask(p_vout->p_sys->i_seq,
	    p_vout->p_sys->clip_mask);

	p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }
    return VLC_SUCCESS;
}

/* Embedded video get their drawing region from the host application
 * by the drawable values here.  Read those variables, and store them
 * in the p_vout->p_sys structure so that other functions (such as
 * DisplayVideo and ManageVideo) can use them later. */

static void UpdateEmbeddedGeometry( vout_thread_t *p_vout )
{
    vlc_value_t val;
    vlc_value_t valt, vall, valb, valr, valx, valy, valw, valh,
		valportx, valporty;

    var_Get( p_vout->p_vlc, "drawable", &val );
    var_Get( p_vout->p_vlc, "drawablet", &valt );
    var_Get( p_vout->p_vlc, "drawablel", &vall );
    var_Get( p_vout->p_vlc, "drawableb", &valb );
    var_Get( p_vout->p_vlc, "drawabler", &valr );
    var_Get( p_vout->p_vlc, "drawablex", &valx );
    var_Get( p_vout->p_vlc, "drawabley", &valy );
    var_Get( p_vout->p_vlc, "drawablew", &valw );
    var_Get( p_vout->p_vlc, "drawableh", &valh );
    var_Get( p_vout->p_vlc, "drawableportx", &valportx );
    var_Get( p_vout->p_vlc, "drawableporty", &valporty );

    /* portx, porty contains values for SetOrigin() function
       which isn't used, instead use QT Translate matrix */
    p_vout->p_sys->i_origx = valportx.i_int;
    p_vout->p_sys->i_origy = valporty.i_int;
    p_vout->p_sys->p_qdport = (CGrafPtr) val.i_int;
    p_vout->p_sys->i_width = valw.i_int;
    p_vout->p_sys->i_height = valh.i_int;

    /* update video clipping mask */
    /*SetRectRgn( p_vout->p_sys->clip_mask , vall.i_int ,
		valt.i_int, valr.i_int, valb.i_int );*/
    SetRectRgn( p_vout->p_sys->clip_mask , vall.i_int + valportx.i_int ,
		valt.i_int + valporty.i_int , valr.i_int + valportx.i_int ,
		valb.i_int + valporty.i_int );

    /* reset drawableredraw variable indicating we are ready
       to take changes in video geometry */
    val.i_int=0;
    var_Set( p_vout->p_vlc, "drawableredraw", val );
}

/*****************************************************************************
 * QTScaleMatrix: scale matrix 
 *****************************************************************************/
static void QTScaleMatrix( vout_thread_t *p_vout )
{
    vlc_value_t val;
    Fixed factor_x, factor_y;
    unsigned int i_offset_x = 0;
    unsigned int i_offset_y = 0;
    int i_width = p_vout->p_sys->i_width;
    int i_height = p_vout->p_sys->i_height;

    var_Get( p_vout, "macosx-stretch", &val );
    if( val.b_bool )
    {
        factor_x = FixDiv( Long2Fix( i_width ),
                           Long2Fix( p_vout->output.i_width ) );
        factor_y = FixDiv( Long2Fix( i_height ),
                           Long2Fix( p_vout->output.i_height ) );

    }
    else if( i_height * p_vout->fmt_in.i_visible_width *
             p_vout->fmt_in.i_sar_num <
             i_width * p_vout->fmt_in.i_visible_height *
             p_vout->fmt_in.i_sar_den )
    {
        int i_adj_width = i_height * p_vout->fmt_in.i_visible_width *
                          p_vout->fmt_in.i_sar_num /
                          ( p_vout->fmt_in.i_sar_den *
                            p_vout->fmt_in.i_visible_height );

        factor_x = FixDiv( Long2Fix( i_adj_width ),
                           Long2Fix( p_vout->fmt_in.i_visible_width ) );
        factor_y = FixDiv( Long2Fix( i_height ),
                           Long2Fix( p_vout->fmt_in.i_visible_height ) );

        i_offset_x = (i_width - i_adj_width) / 2;
    }
    else
    {
        int i_adj_height = i_width * p_vout->fmt_in.i_visible_height *
                           p_vout->fmt_in.i_sar_den /
                           ( p_vout->fmt_in.i_sar_num *
                             p_vout->fmt_in.i_visible_width );

        factor_x = FixDiv( Long2Fix( i_width ),
                           Long2Fix( p_vout->fmt_in.i_visible_width ) );
        factor_y = FixDiv( Long2Fix( i_adj_height ),
                           Long2Fix( p_vout->fmt_in.i_visible_height ) );

        i_offset_y = (i_height - i_adj_height) / 2;
    }

    SetIdentityMatrix( p_vout->p_sys->p_matrix );

    ScaleMatrix( p_vout->p_sys->p_matrix,
                 factor_x, factor_y,
                 Long2Fix(0), Long2Fix(0) );

    TranslateMatrix( p_vout->p_sys->p_matrix,
                 Long2Fix(i_offset_x), Long2Fix(i_offset_y) );
}

/*****************************************************************************
 * QTCreateSequence: create a new sequence 
 *****************************************************************************
 * Returns 0 on success, 1 otherwise
 *****************************************************************************/
static int QTCreateSequence( vout_thread_t *p_vout )
{
    OSErr err;
    ImageDescriptionPtr p_descr;

    HLock( (Handle)p_vout->p_sys->h_img_descr );
    p_descr = *p_vout->p_sys->h_img_descr;

    p_descr->idSize = sizeof(ImageDescription);
    p_descr->cType = p_vout->p_sys->i_codec;
    p_descr->version = 2;
    p_descr->revisionLevel = 0;
    p_descr->vendor = 'mpla';
    p_descr->width = p_vout->output.i_width;
    p_descr->height = p_vout->output.i_height;
    p_descr->hRes = Long2Fix(72);
    p_descr->vRes = Long2Fix(72);
    p_descr->spatialQuality = codecLosslessQuality;
    p_descr->frameCount = 1;
    p_descr->clutID = -1;
    p_descr->dataSize = 0;
    p_descr->depth = 24;

    HUnlock( (Handle)p_vout->p_sys->h_img_descr );

    if( ( err = DecompressSequenceBeginS( 
                              &p_vout->p_sys->i_seq,
                              p_vout->p_sys->h_img_descr,
                              NULL,
                              (p_descr->width * p_descr->height * 16) / 8,
                              p_vout->p_sys->p_qdport,
                              NULL, NULL,
                              p_vout->p_sys->p_matrix,
                              srcCopy, p_vout->p_sys->clip_mask,
                              codecFlagUseImageBuffer,
                              codecLosslessQuality,
                              bestSpeedCodec ) ) )
    {
        msg_Err( p_vout, "Failed to initialize QT: DecompressSequenceBeginS failed: %d", err );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * QTDestroySequence: destroy sequence 
 *****************************************************************************/
static void QTDestroySequence( vout_thread_t *p_vout )
{
    CDSequenceEnd( p_vout->p_sys->i_seq );
}

/*****************************************************************************
 * QTNewPicture: allocate a picture
 *****************************************************************************
 * Returns 0 on success, 1 otherwise
 *****************************************************************************/
static int QTNewPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* We know the chroma, allocate a buffer which will be used
     * directly by the decoder */
    p_pic->p_sys = malloc( sizeof( picture_sys_t ) );

    if( p_pic->p_sys == NULL )
    {
        return( -1 );
    }

    vout_InitPicture( VLC_OBJECT( p_vout), p_pic, p_vout->output.i_chroma,
                      p_vout->output.i_width, p_vout->output.i_height,
                      p_vout->output.i_aspect );

    switch( p_vout->output.i_chroma )
    {
        case VLC_FOURCC('Y','U','Y','2'):
            p_pic->p_sys->i_size = p_vout->output.i_width * p_vout->output.i_height * 2;

            /* Allocate the memory buffer */
            p_pic->p_data = vlc_memalign( &p_pic->p_data_orig,
                                          16, p_pic->p_sys->i_size );

            p_pic->p[0].p_pixels = p_pic->p_data;
            p_pic->p[0].i_lines = p_vout->output.i_height;
            p_pic->p[0].i_visible_lines = p_vout->output.i_height;
            p_pic->p[0].i_pitch = p_vout->output.i_width * 2;
            p_pic->p[0].i_pixel_pitch = 1;
            p_pic->p[0].i_visible_pitch = p_vout->output.i_width * 2;
            p_pic->i_planes = 1;

            p_pic->p_sys->p_data = (void *)p_pic->p[0].p_pixels;

            break;
            
        case VLC_FOURCC('I','4','2','0'):
            p_pic->p_sys->p_data = (void *)&p_pic->p_sys->pixmap_i420;
            p_pic->p_sys->i_size = sizeof(PlanarPixmapInfoYUV420);
            
            /* Allocate the memory buffer */
            p_pic->p_data = vlc_memalign( &p_pic->p_data_orig,
                                          16, p_vout->output.i_width * p_vout->output.i_height * 3 / 2 );

            /* Y buffer */
            p_pic->Y_PIXELS = p_pic->p_data; 
            p_pic->p[Y_PLANE].i_lines = p_vout->output.i_height;
            p_pic->p[Y_PLANE].i_visible_lines = p_vout->output.i_height;
            p_pic->p[Y_PLANE].i_pitch = p_vout->output.i_width;
            p_pic->p[Y_PLANE].i_pixel_pitch = 1;
            p_pic->p[Y_PLANE].i_visible_pitch = p_vout->output.i_width;

            /* U buffer */
            p_pic->U_PIXELS = p_pic->Y_PIXELS + p_vout->output.i_height * p_vout->output.i_width;
            p_pic->p[U_PLANE].i_lines = p_vout->output.i_height / 2;
            p_pic->p[U_PLANE].i_visible_lines = p_vout->output.i_height / 2;
            p_pic->p[U_PLANE].i_pitch = p_vout->output.i_width / 2;
            p_pic->p[U_PLANE].i_pixel_pitch = 1;
            p_pic->p[U_PLANE].i_visible_pitch = p_vout->output.i_width / 2;

            /* V buffer */
            p_pic->V_PIXELS = p_pic->U_PIXELS + p_vout->output.i_height * p_vout->output.i_width / 4;
            p_pic->p[V_PLANE].i_lines = p_vout->output.i_height / 2;
            p_pic->p[V_PLANE].i_visible_lines = p_vout->output.i_height / 2;
            p_pic->p[V_PLANE].i_pitch = p_vout->output.i_width / 2;
            p_pic->p[V_PLANE].i_pixel_pitch = 1;
            p_pic->p[V_PLANE].i_visible_pitch = p_vout->output.i_width / 2;

            /* We allocated 3 planes */
            p_pic->i_planes = 3;

#define P p_pic->p_sys->pixmap_i420
            P.componentInfoY.offset = (void *)p_pic->Y_PIXELS
                                       - p_pic->p_sys->p_data;
            P.componentInfoCb.offset = (void *)p_pic->U_PIXELS
                                        - p_pic->p_sys->p_data;
            P.componentInfoCr.offset = (void *)p_pic->V_PIXELS
                                        - p_pic->p_sys->p_data;

            P.componentInfoY.rowBytes = p_vout->output.i_width;
            P.componentInfoCb.rowBytes = p_vout->output.i_width / 2;
            P.componentInfoCr.rowBytes = p_vout->output.i_width / 2;
#undef P
            break;
        
        default:
            /* Unknown chroma, tell the guy to get lost */
            free( p_pic->p_sys );
            msg_Err( p_vout, "Unknown chroma format 0x%.8x (%4.4s)",
                     p_vout->output.i_chroma, (char*)&p_vout->output.i_chroma );
            p_pic->i_planes = 0;
            return( -1 );
    }

    return( 0 );
}

/*****************************************************************************
 * QTFreePicture: destroy a picture allocated with QTNewPicture
 *****************************************************************************/
static void QTFreePicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    switch( p_vout->output.i_chroma )
    {
        case VLC_FOURCC('I','4','2','0'):
            free( p_pic->p_data_orig );
            break;
    }

    free( p_pic->p_sys );
}

/*****************************************************************************
 * VLCQTView implementation
 *****************************************************************************/
@implementation VLCQTView

- (id) initWithVout:(vout_thread_t *)_p_vout
{
    p_vout = _p_vout;
    return [super init];
}

- (void)drawRect:(NSRect)rect
{
    [[NSColor blackColor] set];
    NSRectFill( rect );
    [super drawRect: rect];

    p_vout->i_changes |= VOUT_SIZE_CHANGE;
}

@end
