/*****************************************************************************
 * xvid.c: an encoder for libxvidcore, the Xvid video codec
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: xvid.c,v 1.3 2003/02/22 16:10:31 fenrir Exp $
 *
 * Authors: Laurent Aimar
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
#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/input.h>
#include <vlc/decoder.h>

#include <stdlib.h>

#include "codecs.h"
#include "encoder.h"

#include <xvid.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenEncoder ( vlc_object_t * );
static void CloseEncoder ( vlc_object_t * );

static int  Init     ( video_encoder_t *p_encoder );
static int  Encode   ( video_encoder_t *p_encoder,
                       picture_t *p_pic, void *p_data, size_t *pi_data );
static void End      ( video_encoder_t *p_encoder );


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static char * ppsz_xvid_quant_algo[] = { "MPEG", "H263",NULL };
static char * ppsz_xvid_me[] = { "", "zero", "logarithmic", "fullsearch", "pmvfast", "epzs", NULL };

vlc_module_begin();
    set_description( _("XviD video encoder (MPEG-4)") );
    set_capability( "video encoder", 50 );
    set_callbacks( OpenEncoder, CloseEncoder );

    add_shortcut( "xvid" );
    add_category_hint( "general setting", NULL, VLC_TRUE );
        add_integer( "encoder-xvid-bitrate", 1000, NULL, "bitrate (kb/s)", "bitrate (kb/s)", VLC_TRUE );
        add_integer( "encoder-xvid-min-quantizer", 2, NULL, "min quantizer", "range 1-31", VLC_TRUE );
        add_integer( "encoder-xvid-max-quantizer", 31, NULL, "max quantizer", "1-31", VLC_TRUE );
        add_integer( "encoder-xvid-max-key-interval", -1, NULL, "max key interval", "maximum   value  of   frames  between   two  keyframes", VLC_TRUE );
    add_category_hint( "advanced setting", NULL, VLC_TRUE );
        add_integer( "encoder-xvid-reaction-delay-factor", -1, NULL, "rc reaction delay factor", "rate controler parameters", VLC_TRUE);
        add_integer( "encoder-xvid-averaging-period", -1, NULL, "rc averaging period", "rate controler parameters", VLC_TRUE );
        add_integer( "encoder-xvid-buffer", -1, NULL, "rc buffer", "rate controler parameters", VLC_TRUE );
    add_category_hint( "advanced frame setting", NULL, VLC_TRUE );
        add_string_from_list( "encoder-xvid-quantization", "MPEG", ppsz_xvid_quant_algo, NULL, "quantization algorithm", "", VLC_TRUE );
        add_bool( "encoder-xvid-halfpel", 1, NULL, "half pixel  motion estimation.", "", VLC_TRUE );
        add_bool( "encoder-xvid-4mv", 0, NULL, "fourc vector per macroblock(need halfpel)", "", VLC_TRUE );
        add_bool( "encoder-xvid-lumi-mask", 0, NULL, "use a lumimasking algorithm", "", VLC_TRUE );
        add_bool( "encoder-xvid-adaptive-quant", 0, NULL, "perform  an  adaptative quantization", "", VLC_TRUE );
        add_bool( "encoder-xvid-interlacing", 0, NULL, "use MPEG4  interlaced mode", "", VLC_TRUE );
        add_string_from_list( "encoder-xvid-me", "", ppsz_xvid_me, NULL, "motion estimation", "", VLC_TRUE );
        add_bool( "encoder-xvid-motion-advanceddiamond", 1, NULL, "motion advanceddiamond", "", VLC_TRUE );
        add_bool( "encoder-xvid-motion-halfpeldiamond", 1, NULL, "motion halfpel diamond", "", VLC_TRUE );
        add_bool( "encoder-xvid-motion-halfpelrefine", 1, NULL, "motion halfpelrefine", "", VLC_TRUE );
        add_bool( "encoder-xvid-motion-extsearch", 1, NULL, "motion extsearch", "", VLC_TRUE );
        add_bool( "encoder-xvid-motion-earlystop", 1, NULL, "motion earlystop", "", VLC_TRUE );
        add_bool( "encoder-xvid-motion-quickstop", 1, NULL, "motion quickstop", "", VLC_TRUE );
        add_bool( "encoder-xvid-motion-usesquares", 0, NULL, "use a square search", "", VLC_TRUE );
vlc_module_end();


struct encoder_sys_t
{
    void *handle;
    XVID_ENC_FRAME xframe;
};

/*****************************************************************************
 * OpenEncoder:
 *****************************************************************************
 *  Check the library and init it
 *  see if it can encode to the requested codec
 *****************************************************************************/
static int OpenEncoder ( vlc_object_t *p_this )
{
    video_encoder_t *p_encoder = (video_encoder_t*)p_this;

    XVID_INIT_PARAM xinit;
    int i_err;


    if( p_encoder->i_codec != VLC_FOURCC( 'm', 'p', '4', 'v' ) )
    {
        /* unsupported codec */
        return VLC_EGENERIC;
    }

    msg_Dbg( p_encoder, "XviD encoder compiled for API_VERSION:0x%x", API_VERSION );

    /* init the library */
    xinit.cpu_flags = 0;
    if( ( i_err = xvid_init( NULL, 0, &xinit, NULL ) ) != XVID_ERR_OK )
    {
        msg_Err( p_encoder, "cannot init xvid library (err:0x%x)", i_err );
        return VLC_EGENERIC;
    }
    /* check API_VERSION  */
    if( xinit.api_version != API_VERSION )
    {
        msg_Err( p_encoder, "API_VERSION version mismatch (library:0x%x)", xinit.api_version );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_encoder, "XviD api_version:0x%x core_build:0x%x cpu_flags:0x%x",
            xinit.api_version,
            xinit.core_build,
            xinit.cpu_flags );

    /* force some parameters */
    if( p_encoder->i_chroma != VLC_FOURCC( 'Y', 'V', '1', '2' )&&
        p_encoder->i_chroma != VLC_FOURCC( 'I', '4', '2', '0' ) )
    {
        p_encoder->i_chroma = VLC_FOURCC( 'I', '4', '2', '0' );
    }

    /* set exported functions */
    p_encoder->pf_init = Init;
    p_encoder->pf_encode = Encode;
    p_encoder->pf_end = End;

    return VLC_SUCCESS;
}

static void CloseEncoder ( vlc_object_t *p_this )
{
    ;
}

/*****************************************************************************
 * Init:
 *****************************************************************************
 *
 *****************************************************************************/
static int Init     ( video_encoder_t *p_encoder )
{
    encoder_sys_t *p_sys;

    char *psz;
    int i_err;
    XVID_ENC_PARAM xparam;

    if( !( p_encoder->p_sys = p_sys = malloc( sizeof( encoder_sys_t ) ) ) )
    {
        msg_Err( p_encoder, "out of memory" );
        return VLC_EGENERIC;
    }
    memset( p_sys, 0, sizeof( encoder_sys_t ) );

    /* *** open a instance of the codec *** */
    xparam.width    = p_encoder->i_width;
    xparam.height   = p_encoder->i_height;

    /* framerat = xparam.fbase / xparam.fincr */
    xparam.fincr    = 1;
    xparam.fbase    = 25;

    /* desired bitrate */
    xparam.rc_bitrate = config_GetInt( p_encoder, "encoder-xvid-bitrate" ) * 1000;
    /* setting to success in achive xparam.rc_bitrate */
    xparam.rc_reaction_delay_factor = config_GetInt( p_encoder, "encoder-xvid-reaction-delay-factor" );
    xparam.rc_averaging_period = config_GetInt( p_encoder, "encoder-xvid-averaging-period" );
    xparam.rc_buffer = config_GetInt( p_encoder, "encoder-xvid-buffer" );

    /* limit  the range  of allowed  quantizers [1..31] */
    xparam.min_quantizer = config_GetInt( p_encoder, "encoder-xvid-min-quantizer" );;
    xparam.min_quantizer = __MAX( __MIN( xparam.min_quantizer, 31 ), 1 );

    xparam.max_quantizer = config_GetInt( p_encoder, "encoder-xvid-max-quantizer" );
    xparam.max_quantizer = __MAX( __MIN( xparam.max_quantizer, 31 ), 1 );

    /* maximum   value  of   frames  between   two  keyframes */
    xparam.max_key_interval = config_GetInt( p_encoder, "encoder-xvid-max-key-interval" );

    switch( xvid_encore( NULL, XVID_ENC_CREATE, &xparam, NULL ) )
    {
        case XVID_ERR_OK:
            msg_Dbg( p_encoder, "encoder creation successed" );
            break;

        case XVID_ERR_MEMORY:
            msg_Err( p_encoder, "encoder creation failed, out of memory" );
            return VLC_EGENERIC;
        case XVID_ERR_FORMAT:
            msg_Err( p_encoder, "encoder creation failed, bad format" );
            return VLC_EGENERIC;

        case XVID_ERR_FAIL:
        default:
            msg_Err( p_encoder, "encoder creation failed" );
            return VLC_EGENERIC;
    }
    /* get the handle */
    p_sys->handle = xparam.handle;

    /* *** set xframe parameters *** */
#define xframe p_sys->xframe
    /* set xframe.general options  */
    xframe.general = 0;
    psz = config_GetPsz( p_encoder, "encoder-xvid-quantization" );
    if( psz )
    {
        if( !strcmp( psz, "H263" ) )
        {
            xframe.general |= XVID_H263QUANT;
        }
        else
        {
            xframe.general |= XVID_MPEGQUANT;
        }
        free( psz );
    }
    if( config_GetInt( p_encoder, "encoder-xvid-halfpel" ) )
        xframe.general |= XVID_HALFPEL;
    if( config_GetInt( p_encoder, "encoder-xvid-4mv" ) )
        xframe.general |= XVID_INTER4V;
    if( config_GetInt( p_encoder, "encoder-xvid-lumi-mask" ) )
        xframe.general |= XVID_LUMIMASKING;
    if( config_GetInt( p_encoder, "encoder-xvid-adaptive-quant" ) )
        xframe.general |= XVID_ADAPTIVEQUANT;
    if( config_GetInt( p_encoder, "encoder-xvid-interlacing" ) )
        xframe.general |= XVID_INTERLACING;

    psz = config_GetPsz( p_encoder, "encoder-xvid-me" );
    if( psz )
    {
        if( !strcmp( psz, "zero" ) )
        {
            xframe.general |= XVID_ME_ZERO;
        }
        else if( !strcmp( psz, "logarithmic" ) )
        {
            xframe.general |= XVID_ME_LOGARITHMIC;
        }
        else if( !strcmp( psz, "fullsearch" ) )
        {
            xframe.general |= XVID_ME_FULLSEARCH;
        }
        else if( !strcmp( psz, "pmvfast" ) )
        {
            xframe.general |= XVID_ME_PMVFAST;
        }
        else if( !strcmp( psz, "epzs" ) )
        {
            xframe.general |= XVID_ME_EPZS;
        }
        free( psz );
    }

    xframe.motion = 0;
    if( config_GetInt( p_encoder, "encoder-xvid-motion-advanceddiamond" ) )
        xframe.motion |= PMV_ADVANCEDDIAMOND16 | PMV_ADVANCEDDIAMOND8;
    if( config_GetInt( p_encoder, "encoder-xvid-motion-halfpeldiamond" ) )
        xframe.motion |= PMV_HALFPELDIAMOND16 | PMV_HALFPELDIAMOND8;
    if( config_GetInt( p_encoder, "encoder-xvid-motion-halfpelrefine") )
        xframe.motion |= PMV_HALFPELREFINE16 | PMV_HALFPELREFINE8;
    if( config_GetInt( p_encoder, "encoder-xvid-motion-extsearch" ) )
        xframe.motion |= PMV_EXTSEARCH16 | PMV_EXTSEARCH8;
    if( config_GetInt( p_encoder, "encoder-xvid-motion-earlystop") )
        xframe.motion |= PMV_EARLYSTOP16 | PMV_EARLYSTOP8;
    if( config_GetInt( p_encoder, "encoder-xvid-motion-quickstop" ) )
        xframe.motion |= PMV_QUICKSTOP16 | PMV_QUICKSTOP8;
    if( config_GetInt( p_encoder, "encoder-xvid-motion-usesquares" ) )
        xframe.motion |= PMV_USESQUARES16 | PMV_USESQUARES8;

    /* no user quant matrix */
    xframe.quant_intra_matrix = NULL;
    xframe.quant_inter_matrix = NULL;

    switch( p_encoder->i_chroma )
    {
        case VLC_FOURCC( 'Y', 'V', '1', '2' ):
            xframe.colorspace = XVID_CSP_YV12;

        case VLC_FOURCC( 'I', '4', '2', '0' ):
            xframe.colorspace = XVID_CSP_I420;
    }
    /* != 0 -> force quant */
    xframe.quant = 0;
#undef  xframe

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Encode: encode a single frame
 *****************************************************************************
 *
 *****************************************************************************/
static int Encode   ( video_encoder_t *p_encoder,
                      picture_t *p_pic, void *p_data, size_t *pi_data )
{
    int i_err;
#define xframe p_encoder->p_sys->xframe

    xframe.image = p_pic->p->p_pixels;

    xframe.bitstream = p_data;
    xframe.length = -1;

    /* let codec decided between I-frame and P-frame */
    xframe.intra = -1;

    i_err = xvid_encore( p_encoder->p_sys->handle, XVID_ENC_ENCODE, &xframe, NULL );

    *pi_data = xframe.length;
#undef  xframe
    return VLC_SUCCESS;
}

/*****************************************************************************
 * End
 *****************************************************************************
 *
 *****************************************************************************/
static void End      ( video_encoder_t *p_encoder )
{
    int i_err;

    /* *** close the codec */
    i_err = xvid_encore(p_encoder->p_sys->handle, XVID_ENC_DESTROY, NULL, NULL);

    /* *** free memory */
    free( p_encoder->p_sys );

    msg_Dbg( p_encoder, "closing encoder (err:0x%x)", i_err );
    return;
}


