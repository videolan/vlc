/*****************************************************************************
 * encoder.c :  encoder wrapper plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: encoder.c,v 1.1 2003/01/22 10:41:57 fenrir Exp $
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

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/input.h>

#include "encoder.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static int  Manage    ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );
static void Display   ( vout_thread_t *, picture_t * );

static void SetPalette( vout_thread_t *, uint16_t *, uint16_t *, uint16_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Encoder wrapper module") );
    set_capability( "video output", 0 );
    set_callbacks( Create, Destroy );
    add_shortcut( "encoder" );
vlc_module_end();

/*****************************************************************************
 * vout_sys_t: video output descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the SVGAlib specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    vlc_fourcc_t    i_codec;
    video_encoder_t *p_encoder;

    int             i_buffer;
    void            *p_buffer;

    input_thread_t  *p_input;
    es_descriptor_t *p_es;
};

/*****************************************************************************
 * Create: allocates video thread
 *****************************************************************************
 * This function allocates and initializes a vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t   *p_vout = (vout_thread_t *)p_this;
    char *psz_sout;
    char *psz_sout_vcodec;
    vlc_fourcc_t i_codec;

    psz_sout = config_GetPsz( p_vout, "sout" );
    if( !psz_sout || !*psz_sout )
    {
        /* avoid bad infinite loop */
        msg_Err( p_vout, "encoder video output should be used only in sout mode" );
        if( psz_sout ) free( psz_sout );
        return VLC_EGENERIC;
    }
    free( psz_sout );

    psz_sout_vcodec = config_GetPsz( p_vout, "sout-vcodec" );
    if( !psz_sout_vcodec || !*psz_sout_vcodec )
    {
        msg_Err( p_vout, "you have to specify a video codec using sout-vcodec" );
        if( psz_sout_vcodec ) free( psz_sout_vcodec );
        return VLC_EGENERIC;
    }
    if( !strcmp( psz_sout_vcodec, "mpeg4" ) )
    {
        i_codec  = VLC_FOURCC( 'm', 'p', '4', 'v' );
    }
    else if( !strcmp( psz_sout_vcodec, "mpeg2" ) )
    {
        i_codec  = VLC_FOURCC( 'm', 'p', '2', 'v' );
    }
    else if( !strcmp( psz_sout_vcodec, "mpeg1" ) )
    {
        i_codec  = VLC_FOURCC( 'm', 'p', '1', 'v' );
    }
    else
    {
        int i;
        int c[4];
        msg_Warn( p_vout, "unknown codec %s used as a fourcc", psz_sout_vcodec );
        for( i = 0; i < 4; i++ )
        {
            if( psz_sout_vcodec[i] )
                c[i] = psz_sout_vcodec[i];
            else
                c[i] = ' ';
        }
        i_codec = VLC_FOURCC( c[0], c[1], c[2], c[3] );
    }
    free( psz_sout_vcodec );

    /* Allocate instance and initialize some members */
    if( !( p_vout->p_sys = malloc( sizeof( vout_sys_t ) ) ) )
    {
        return VLC_ENOMEM;
    }
    memset( p_vout->p_sys, 0, sizeof( vout_sys_t ) );

    /* *** save parameters *** */
    p_vout->p_sys->i_codec = i_codec;

    /* *** set exported functions *** */
    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_render = Render;
    p_vout->pf_manage = Manage;
    p_vout->pf_display = Display;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize video thread
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    vout_sys_t   *p_sys = p_vout->p_sys;
    video_encoder_t *p_encoder = p_vout->p_sys->p_encoder;
    char *psz_sout_vcodec;

    int i_index;
    picture_t *p_pic;

    /* *** create a video encoder object *** */
    p_vout->p_sys->p_encoder =
        p_encoder = vlc_object_create( p_vout, sizeof( video_encoder_t ) );

    /* *** set wanted input format *** */
    p_encoder->i_codec  = p_vout->p_sys->i_codec;

    /* *** set prefered properties *** */
    /* encoder can modify all these values except i_codec */
    p_encoder->i_chroma = p_vout->render.i_chroma;
    p_encoder->i_width  = p_vout->render.i_width;
    p_encoder->i_height = p_vout->render.i_height;
    p_encoder->i_aspect = p_vout->render.i_aspect;
    p_encoder->i_buffer_size = 0;

    /* *** requuest this module *** */
    p_encoder->p_module = module_Need( p_encoder,
                                       "video encoder",
                                       "$video-encoder" );
    if( !p_encoder->p_module )
    {
        msg_Warn( p_vout,
                  "no suitable encoder to %4.4s",
                  (char*)&p_encoder->i_codec );
        vlc_object_destroy( p_encoder );
        return VLC_EGENERIC;
    }

    /* *** init the codec *** */
    if( p_encoder->pf_init( p_encoder ) )
    {
        msg_Err( p_vout, "failed to initialize video encoder plugin" );
        vlc_object_destroy( p_encoder );
        return VLC_EGENERIC;
    }

    /* *** alloacted buffer *** */
    if( p_encoder->i_buffer_size <= 0 )
    {
        p_encoder->i_buffer_size = 10 * p_encoder->i_width * p_encoder->i_height;
    }
    p_sys->i_buffer = p_encoder->i_buffer_size;
    if( !( p_sys->p_buffer = malloc( p_encoder->i_buffer_size ) ) )
    {
        msg_Err( p_vout, "out of memory" );
        return VLC_ENOMEM;
    }

    /* *** create a new standalone ES *** */
    /* find a p_input  */
    p_sys->p_input = vlc_object_find( p_vout, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    if( !p_sys->p_input )
    {
        msg_Err( p_vout, "cannot find p_input" );
        return VLC_EGENERIC;
    }

    vlc_mutex_lock( &p_sys->p_input->stream.stream_lock );

    /* avoid bad loop (else output will also be reencoded until it segfault :p*/
    /* XXX do it after the lock (if we have multiple video stream...) */
    psz_sout_vcodec = config_GetPsz( p_vout, "sout-vcodec" );
    config_PutPsz( p_vout, "sout-vcodec", NULL );

    /* add a new stream */
    p_sys->p_es = input_AddES( p_sys->p_input,
                               NULL, /* we aren't attached to a program */
                               12,   /* es_id */
                               0 );  /* no extra data */
    if( !p_sys->p_es )
    {
        msg_Err( p_vout, "cannot create es" );
        vlc_mutex_unlock( &p_sys->p_input->stream.stream_lock );
        return VLC_EGENERIC;
    }
    p_sys->p_es->i_stream_id = 1;
    p_sys->p_es->i_fourcc  = p_encoder->i_codec;
    p_sys->p_es->i_cat = VIDEO_ES;
    if( input_SelectES( p_sys->p_input, p_sys->p_es ) )
    {
        input_DelES( p_sys->p_input, p_sys->p_es );
        vlc_mutex_unlock( &p_sys->p_input->stream.stream_lock );
        msg_Err( p_vout, "cannot select es" );
        return VLC_EGENERIC;
    }
    /* restore value as we could have multiple video stream (have you a 42*12 GHz ?) */
    config_PutPsz( p_vout, "sout-vcodec", psz_sout_vcodec );
    vlc_mutex_unlock( &p_sys->p_input->stream.stream_lock );


    I_OUTPUTPICTURES = 0;

    p_vout->output.pf_setpalette = SetPalette;
    /* remember that this value could have been modified by encoder */
    p_vout->output.i_chroma = p_vout->p_sys->p_encoder->i_chroma;
    p_vout->output.i_width  = p_vout->p_sys->p_encoder->i_width;
    p_vout->output.i_height = p_vout->p_sys->p_encoder->i_height;
    p_vout->output.i_aspect = p_vout->p_sys->p_encoder->i_aspect;

    /* Try to initialize 1 direct buffer */
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
    if( p_pic == NULL )
    {
        return VLC_SUCCESS;
    }

    vout_AllocatePicture( p_vout, p_pic, p_vout->output.i_width,
                          p_vout->output.i_height,
                          p_vout->output.i_chroma );

    if( p_pic->i_planes == 0 )
    {
        return VLC_SUCCESS;
    }

    p_pic->i_status = DESTROYED_PICTURE;
    p_pic->i_type   = DIRECT_PICTURE;

    PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

    I_OUTPUTPICTURES++;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate video thread
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    vout_sys_t      *p_sys = p_vout->p_sys;
    video_encoder_t *p_encoder = p_vout->p_sys->p_encoder;

    /* *** stop encoder *** */
    p_encoder->pf_end( p_encoder );

    vlc_object_release( p_sys->p_input );

    /* *** unload encoder plugin *** */
    module_Unneed( p_encoder,
                   p_encoder->p_module );
    vlc_object_destroy( p_encoder );

    free( p_sys->p_buffer );
}

/*****************************************************************************
 * Destroy: destroy video thread
 *****************************************************************************
 * Terminate an output method created by Create
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    /* Destroy structure */
    free( p_vout->p_sys );
    p_vout->p_sys = NULL;
}

/*****************************************************************************
 * Manage: handle events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Render:
 *****************************************************************************
 *
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
}

/*****************************************************************************
 * Display: displays previously rendered output
 *****************************************************************************
 * This function sends the currently rendered image to the VGA card.
 *****************************************************************************/
static void Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    video_encoder_t *p_encoder = p_vout->p_sys->p_encoder;

    int     i_err;
    size_t  i_data;

    i_data = p_sys->i_buffer;
    i_err  = p_encoder->pf_encode( p_encoder,
                                   p_pic,
                                   p_sys->p_buffer,
                                   &i_data );
    if( i_err )
    {
        msg_Err( p_vout, "failed to encode a frame (err:0x%x)", i_err );
        return;
    }

    if( i_data > 0 && p_sys->p_es->p_decoder_fifo )
    {
        pes_packet_t *p_pes;
        data_packet_t   *p_data;

        if( !( p_pes = input_NewPES( p_sys->p_input->p_method_data ) ) )
        {
            msg_Err( p_vout, "cannot allocate new PES" );
            return;
        }
        if( !( p_data = input_NewPacket( p_sys->p_input->p_method_data, i_data ) ) )
        {
            msg_Err( p_vout, "cannot allocate new data_packet" );
            return;
        }
        p_pes->i_dts = p_pic->date;
        p_pes->i_pts = p_pic->date;
        p_pes->p_first = p_pes->p_last = p_data;
        p_pes->i_nb_data = 1;
        p_pes->i_pes_size = i_data;

        p_vout->p_vlc->pf_memcpy( p_data->p_payload_start,
                                  p_sys->p_buffer,
                                  i_data );

        input_DecodePES( p_sys->p_es->p_decoder_fifo, p_pes );
    }

}

/*****************************************************************************
 * SetPalette: set a 8bpp palette
 *****************************************************************************
 * TODO: support 8 bits clut (for Mach32 cards and others).
 *****************************************************************************/
static void SetPalette( vout_thread_t *p_vout, uint16_t *red, uint16_t *green, uint16_t *blue )
{
    ;
}

