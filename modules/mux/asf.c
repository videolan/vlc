/*****************************************************************************
 * asf.c: asf muxer module for vlc
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <vlc/input.h>
#include <vlc/sout.h>

#include "codecs.h"
typedef GUID guid_t;

#define MAX_ASF_TRACKS 128
#define ASF_DATA_PACKET_SIZE 4096  // deprecated -- added sout-asf-packet-size

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-asf-"

#define TITLE_TEXT N_("Title")
#define TITLE_LONGTEXT N_("Allows you to define the title that will be put " \
                          "in ASF comments.")
#define AUTHOR_TEXT N_("Author")
#define AUTHOR_LONGTEXT N_("Allows you to define the author that will be put "\
                           "in ASF comments.")
#define COPYRIGHT_TEXT N_("Copyright")
#define COPYRIGHT_LONGTEXT N_("Allows you to define the copyright string " \
                              "that will be put in ASF comments.")
#define COMMENT_TEXT N_("Comment")
#define COMMENT_LONGTEXT N_("Allows you to define the comment that will be " \
                            "put in ASF comments.")
#define RATING_TEXT N_("Rating")
#define RATING_LONGTEXT N_("Allows you to define the \"rating\" that will " \
                           "be put in ASF comments.")
#define PACKETSIZE_TEXT N_("Packet Size")
#define PACKETSIZE_LONGTEXT N_("The ASF packet size -- default is 4096 bytes")

vlc_module_begin();
    set_description( _("ASF muxer") );
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_MUX );
    set_shortname( "ASF" );

    set_capability( "sout mux", 5 );
    add_shortcut( "asf" );
    add_shortcut( "asfh" );
    set_callbacks( Open, Close );

    add_string( SOUT_CFG_PREFIX "title", "", NULL, TITLE_TEXT, TITLE_LONGTEXT,
                                 VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "author",   "", NULL, AUTHOR_TEXT,
                                 AUTHOR_LONGTEXT, VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "copyright","", NULL, COPYRIGHT_TEXT,
                                 COPYRIGHT_LONGTEXT, VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "comment",  "", NULL, COMMENT_TEXT,
                                 COMMENT_LONGTEXT, VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "rating",  "", NULL, RATING_TEXT,
                                 RATING_LONGTEXT, VLC_TRUE );
    add_integer( "sout-asf-packet-size", 4096, NULL, PACKETSIZE_TEXT, PACKETSIZE_LONGTEXT, VLC_TRUE );

vlc_module_end();

/*****************************************************************************
 * Locales prototypes
 *****************************************************************************/
static const char *ppsz_sout_options[] = {
    "title", "author", "copyright", "comment", "rating", NULL
};

static int Control  ( sout_mux_t *, int, va_list );
static int AddStream( sout_mux_t *, sout_input_t * );
static int DelStream( sout_mux_t *, sout_input_t * );
static int Mux      ( sout_mux_t * );

typedef struct
{
    int          i_id;
    int          i_cat;

    /* codec information */
    uint16_t     i_tag;     /* for audio */
    vlc_fourcc_t i_fourcc;  /* for video */
    char         *psz_name; /* codec name */
    int          i_blockalign; /* for audio only */
    vlc_bool_t   b_audio_correction;

    int          i_sequence;

    int          i_extra;
    uint8_t      *p_extra;

    es_format_t  fmt;

} asf_track_t;

struct sout_mux_sys_t
{
    guid_t          fid;    /* file id */
    int             i_packet_size;
    int64_t         i_packet_count;
    mtime_t         i_dts_first;
    mtime_t         i_dts_last;
    mtime_t         i_preroll_time;
    int64_t         i_bitrate;

    int             i_track;
    asf_track_t     track[MAX_ASF_TRACKS];

    vlc_bool_t      b_write_header;

    block_t         *pk;
    int             i_pk_used;
    int             i_pk_frame;
    mtime_t         i_pk_dts;

    vlc_bool_t      b_asf_http;
    int             i_seq;

    /* meta data */
    char            *psz_title;
    char            *psz_author;
    char            *psz_copyright;
    char            *psz_comment;
    char            *psz_rating;
};

static int MuxGetStream( sout_mux_t *, int *pi_stream, mtime_t *pi_dts );

static block_t *asf_header_create( sout_mux_t *, vlc_bool_t );
static block_t *asf_packet_create( sout_mux_t *, asf_track_t *, block_t * );
static block_t *asf_stream_end_create( sout_mux_t *);
static block_t *asf_packet_flush( sout_mux_t * );

typedef struct
{
    int      i_buffer_size;
    int      i_buffer;
    uint8_t  *p_buffer;

} bo_t;

static void bo_init     ( bo_t *, uint8_t *, int  );
static void bo_add_u8   ( bo_t *, uint8_t  );
static void bo_addle_u16( bo_t *, uint16_t );
static void bo_addle_u32( bo_t *, uint32_t );
static void bo_addle_u64( bo_t *, uint64_t );
static void bo_add_mem  ( bo_t *, uint8_t *, int );

static void bo_addle_str16( bo_t *, char * );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_mux_t     *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t *p_sys;
    vlc_value_t    val;
    int i;

    msg_Dbg( p_mux, "Asf muxer opened" );
    sout_CfgParse( p_mux, SOUT_CFG_PREFIX, ppsz_sout_options, p_mux->p_cfg );

    p_mux->pf_control   = Control;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;

    p_mux->p_sys = p_sys = malloc( sizeof( sout_mux_sys_t ) );
    p_sys->b_asf_http = p_mux->psz_mux && !strcmp( p_mux->psz_mux, "asfh" );
    if( p_sys->b_asf_http )
    {
        msg_Dbg( p_mux, "creating asf stream to be used with mmsh" );
    }
    p_sys->pk = NULL;
    p_sys->i_pk_used    = 0;
    p_sys->i_pk_frame   = 0;
    p_sys->i_dts_first  = -1;
    p_sys->i_dts_last   = 0;
    p_sys->i_preroll_time = 2000;
    p_sys->i_bitrate    = 0;
    p_sys->i_seq        = 0;

    p_sys->b_write_header = VLC_TRUE;
    p_sys->i_track = 0;
    p_sys->i_packet_size = config_GetInt( p_mux, "sout-asf-packet-size" );
    msg_Dbg( p_mux, "Packet size %d", p_sys->i_packet_size);
    p_sys->i_packet_count= 0;

    /* Generate a random fid */
    srand( mdate() & 0xffffffff );
    p_sys->fid.Data1 = 0xbabac001;
    p_sys->fid.Data2 = ( (uint64_t)rand() << 16 ) / RAND_MAX;
    p_sys->fid.Data3 = ( (uint64_t)rand() << 16 ) / RAND_MAX;
    for( i = 0; i < 8; i++ )
    {
        p_sys->fid.Data4[i] = ( (uint64_t)rand() << 8 ) / RAND_MAX;
    }

    /* Meta data */
    var_Get( p_mux, SOUT_CFG_PREFIX "title", &val );
    p_sys->psz_title = val.psz_string;

    var_Get( p_mux, SOUT_CFG_PREFIX "author", &val );
    p_sys->psz_author = val.psz_string;

    var_Get( p_mux, SOUT_CFG_PREFIX "copyright", &val );
    p_sys->psz_copyright = val.psz_string;

    var_Get( p_mux, SOUT_CFG_PREFIX "comment", &val );
    p_sys->psz_comment = val.psz_string;

    var_Get( p_mux, SOUT_CFG_PREFIX "rating", &val );
    p_sys->psz_rating = val.psz_string;

    msg_Dbg( p_mux, "meta data: title='%s' author='%s' copyright='%s' "
             "comment='%s' rating='%s'",
             p_sys->psz_title, p_sys->psz_author, p_sys->psz_copyright,
             p_sys->psz_comment, p_sys->psz_rating );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_mux_t     *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    block_t  *out;
    int i;

    msg_Dbg( p_mux, "Asf muxer closed" );

    /* Flush last packet if any */
    if( (out = asf_packet_flush( p_mux ) ) )
    {
        sout_AccessOutWrite( p_mux->p_access, out );
    }

    if( ( out = asf_stream_end_create( p_mux ) ) )
    {
        sout_AccessOutWrite( p_mux->p_access, out );
    }

    /* rewrite header */
    if( !sout_AccessOutSeek( p_mux->p_access, 0 ) )
    {
        out = asf_header_create( p_mux, VLC_FALSE );
        sout_AccessOutWrite( p_mux->p_access, out );
    }

    for( i = 0; i < p_sys->i_track; i++ )
    {
        free( p_sys->track[i].p_extra );
        es_format_Clean( &p_sys->track[i].fmt );
    }
    free( p_sys );
}

/*****************************************************************************
 * Capability:
 *****************************************************************************/
static int Control( sout_mux_t *p_mux, int i_query, va_list args )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    vlc_bool_t *pb_bool;
    char **ppsz;

    switch( i_query )
    {
       case MUX_CAN_ADD_STREAM_WHILE_MUXING:
           pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t * );
           if( p_sys->b_asf_http ) *pb_bool = VLC_TRUE;
           else *pb_bool = VLC_FALSE;
           return VLC_SUCCESS;

       case MUX_GET_ADD_STREAM_WAIT:
           pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t * );
           *pb_bool = VLC_TRUE;
           return VLC_SUCCESS;

       case MUX_GET_MIME:
           ppsz = (char**)va_arg( args, char ** );
           if( p_sys->b_asf_http )
               *ppsz = strdup( "video/x-ms-asf-stream" );
           else
               *ppsz = strdup( "video/x-ms-asf" );
           return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * AddStream:
 *****************************************************************************/
static int AddStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t   *p_sys = p_mux->p_sys;
    asf_track_t      *tk;
    bo_t             bo;

    msg_Dbg( p_mux, "adding input" );
    if( p_sys->i_track >= MAX_ASF_TRACKS )
    {
        msg_Dbg( p_mux, "cannot add this track (too much track)" );
        return VLC_EGENERIC;
    }

    tk = p_input->p_sys = &p_sys->track[p_sys->i_track];
    tk->i_id  = p_sys->i_track + 1;
    tk->i_cat = p_input->p_fmt->i_cat;
    tk->i_sequence = 0;
    tk->b_audio_correction = 0;

    switch( tk->i_cat )
    {
        case AUDIO_ES:
        {
            int i_blockalign = p_input->p_fmt->audio.i_blockalign;
            int i_bitspersample = p_input->p_fmt->audio.i_bitspersample;
            int i_extra = 0;

            switch( p_input->p_fmt->i_codec )
            {
                case VLC_FOURCC( 'a', '5', '2', ' ' ):
                    tk->i_tag = WAVE_FORMAT_A52;
                    tk->psz_name = "A/52";
                    i_bitspersample = 0;
                    break;
                case VLC_FOURCC( 'm', 'p', 'g', 'a' ):
#if 1
                    tk->psz_name = "MPEG Audio Layer 3";
                    tk->i_tag = WAVE_FORMAT_MPEGLAYER3;
                    i_bitspersample = 0;
                    i_blockalign = 1;
                    i_extra = 12;
                    break;
#else
                    tk->psz_name = "MPEG Audio Layer 1/2";
                    tk->i_tag = WAVE_FORMAT_MPEG;
                    i_bitspersample = 0;
                    i_blockalign = 1;
                    i_extra = 22;
                    break;
#endif
                case VLC_FOURCC( 'w', 'm', 'a', '1' ):
                    tk->psz_name = "Windows Media Audio v1";
                    tk->i_tag = WAVE_FORMAT_WMA1;
                    tk->b_audio_correction = VLC_TRUE;
                    break;
                case VLC_FOURCC( 'w', 'm', 'a', ' ' ):
                case VLC_FOURCC( 'w', 'm', 'a', '2' ):
                    tk->psz_name= "Windows Media Audio (v2) 7, 8 and 9 Series";
                    tk->i_tag = WAVE_FORMAT_WMA2;
                    tk->b_audio_correction = VLC_TRUE;
                    break;
                case VLC_FOURCC( 'w', 'm', 'a', 'p' ):
                    tk->psz_name = "Windows Media Audio 9 Professional";
                    tk->i_tag = WAVE_FORMAT_WMAP;
                    tk->b_audio_correction = VLC_TRUE;
                    break;
                case VLC_FOURCC( 'w', 'm', 'a', 'l' ):
                    tk->psz_name = "Windows Media Audio 9 Lossless";
                    tk->i_tag = WAVE_FORMAT_WMAL;
                    tk->b_audio_correction = VLC_TRUE;
                    break;
                    /* raw codec */
                case VLC_FOURCC( 'u', '8', ' ', ' ' ):
                    tk->psz_name = "Raw audio 8bits";
                    tk->i_tag = WAVE_FORMAT_PCM;
                    i_blockalign= p_input->p_fmt->audio.i_channels;
                    i_bitspersample = 8;
                    break;
                case VLC_FOURCC( 's', '1', '6', 'l' ):
                    tk->psz_name = "Raw audio 16bits";
                    tk->i_tag = WAVE_FORMAT_PCM;
                    i_blockalign= 2 * p_input->p_fmt->audio.i_channels;
                    i_bitspersample = 16;
                    break;
                case VLC_FOURCC( 's', '2', '4', 'l' ):
                    tk->psz_name = "Raw audio 24bits";
                    tk->i_tag = WAVE_FORMAT_PCM;
                    i_blockalign= 3 * p_input->p_fmt->audio.i_channels;
                    i_bitspersample = 24;
                    break;
                case VLC_FOURCC( 's', '3', '2', 'l' ):
                    tk->psz_name = "Raw audio 32bits";
                    tk->i_tag = WAVE_FORMAT_PCM;
                    i_blockalign= 4 * p_input->p_fmt->audio.i_channels;
                    i_bitspersample = 32;
                    break;
                default:
                    return VLC_EGENERIC;
            }

            tk->i_extra = sizeof( WAVEFORMATEX ) +
                          p_input->p_fmt->i_extra + i_extra;
            tk->p_extra = malloc( tk->i_extra );
            bo_init( &bo, tk->p_extra, tk->i_extra );
            bo_addle_u16( &bo, tk->i_tag );
            bo_addle_u16( &bo, p_input->p_fmt->audio.i_channels );
            bo_addle_u32( &bo, p_input->p_fmt->audio.i_rate );
            bo_addle_u32( &bo, p_input->p_fmt->i_bitrate / 8 );
            bo_addle_u16( &bo, i_blockalign );
            tk->i_blockalign = i_blockalign;
            bo_addle_u16( &bo, i_bitspersample );
            if( p_input->p_fmt->i_extra > 0 )
            {
                bo_addle_u16( &bo, p_input->p_fmt->i_extra );
                bo_add_mem  ( &bo, p_input->p_fmt->p_extra,
                              p_input->p_fmt->i_extra );
            }
            else
            {
                bo_addle_u16( &bo, i_extra );
                if( tk->i_tag == WAVE_FORMAT_MPEGLAYER3 )
                {
                    msg_Dbg( p_mux, "adding mp3 header" );
                    bo_addle_u16( &bo, 1 );     /* wId */
                    bo_addle_u32( &bo, 2 );     /* fdwFlags */
                    bo_addle_u16( &bo, 1152 );  /* nBlockSize */
                    bo_addle_u16( &bo, 1 );     /* nFramesPerBlock */
                    bo_addle_u16( &bo, 1393 );  /* nCodecDelay */
                }
                else if( tk->i_tag == WAVE_FORMAT_MPEG )
                {
                    msg_Dbg( p_mux, "adding mp2 header" );
                    bo_addle_u16( &bo, 2 );     /* fwHeadLayer */
                    bo_addle_u32( &bo, p_input->p_fmt->i_bitrate );
                    bo_addle_u16( &bo, p_input->p_fmt->audio.i_channels == 2 ?1:8 );
                    bo_addle_u16( &bo, 0 );     /* fwHeadModeExt */
                    bo_addle_u16( &bo, 1 );     /* wHeadEmphasis */
                    bo_addle_u16( &bo, 16 );    /* fwHeadFlags */
                    bo_addle_u32( &bo, 0 );     /* dwPTSLow */
                    bo_addle_u32( &bo, 0 );     /* dwPTSHigh */
                }
            }

            if( p_input->p_fmt->i_bitrate > 24000 )
            {
                p_sys->i_bitrate += p_input->p_fmt->i_bitrate;
            }
            else
            {
                p_sys->i_bitrate += 512000;
            }
            break;
        }
        case VIDEO_ES:
        {
            tk->i_extra = 11 + sizeof( BITMAPINFOHEADER ) +
                          p_input->p_fmt->i_extra;
            tk->p_extra = malloc( tk->i_extra );
            bo_init( &bo, tk->p_extra, tk->i_extra );
            bo_addle_u32( &bo, p_input->p_fmt->video.i_width );
            bo_addle_u32( &bo, p_input->p_fmt->video.i_height );
            bo_add_u8   ( &bo, 0x02 );  /* flags */
            bo_addle_u16( &bo, sizeof( BITMAPINFOHEADER ) +
                               p_input->p_fmt->i_extra );
            bo_addle_u32( &bo, sizeof( BITMAPINFOHEADER ) +
                               p_input->p_fmt->i_extra );
            bo_addle_u32( &bo, p_input->p_fmt->video.i_width );
            bo_addle_u32( &bo, p_input->p_fmt->video.i_height );
            bo_addle_u16( &bo, 1 );
            bo_addle_u16( &bo, 24 );
            if( p_input->p_fmt->i_codec == VLC_FOURCC('m','p','4','v') )
            {
                tk->psz_name = "MPEG-4 Video";
                tk->i_fourcc = VLC_FOURCC( 'M', 'P', '4', 'S' );
            }
            else if( p_input->p_fmt->i_codec == VLC_FOURCC('D','I','V','3') )
            {
                tk->psz_name = "MSMPEG-4 V3 Video";
                tk->i_fourcc = VLC_FOURCC( 'M', 'P', '4', '3' );
            }
            else if( p_input->p_fmt->i_codec == VLC_FOURCC('D','I','V','2') )
            {
                tk->psz_name = "MSMPEG-4 V2 Video";
                tk->i_fourcc = VLC_FOURCC( 'M', 'P', '4', '2' );
            }
            else if( p_input->p_fmt->i_codec == VLC_FOURCC('D','I','V','1') )
            {
                tk->psz_name = "MSMPEG-4 V1 Video";
                tk->i_fourcc = VLC_FOURCC( 'M', 'P', 'G', '4' );
            }
            else if( p_input->p_fmt->i_codec == VLC_FOURCC('W','M','V','1') )
            {
                tk->psz_name = "Windows Media Video 1";
                tk->i_fourcc = VLC_FOURCC( 'W', 'M', 'V', '1' );
            }
            else if( p_input->p_fmt->i_codec == VLC_FOURCC('W','M','V','2') )
            {
                tk->psz_name = "Windows Media Video 2";
                tk->i_fourcc = VLC_FOURCC( 'W', 'M', 'V', '2' );
            }
            else if( p_input->p_fmt->i_codec == VLC_FOURCC('W','M','V','3') )
            {
                tk->psz_name = "Windows Media Video 3";
                tk->i_fourcc = VLC_FOURCC( 'W', 'M', 'V', '3' );
            }
            else
            {
                tk->psz_name = _("Unknown Video");
                tk->i_fourcc = p_input->p_fmt->i_codec;
            }
            bo_add_mem( &bo, (uint8_t*)&tk->i_fourcc, 4 );
            bo_addle_u32( &bo, 0 );
            bo_addle_u32( &bo, 0 );
            bo_addle_u32( &bo, 0 );
            bo_addle_u32( &bo, 0 );
            bo_addle_u32( &bo, 0 );
            if( p_input->p_fmt->i_extra > 0 )
            {
                bo_add_mem  ( &bo, p_input->p_fmt->p_extra,
                              p_input->p_fmt->i_extra );
            }

            if( p_input->p_fmt->i_bitrate > 50000 )
            {
                p_sys->i_bitrate += p_input->p_fmt->i_bitrate;
            }
            else
            {
                p_sys->i_bitrate += 1000000;
            }
            break;
        }
        default:
            msg_Err(p_mux, "unhandled track type" );
            return VLC_EGENERIC;
    }

    es_format_Copy( &tk->fmt, p_input->p_fmt );

    p_sys->i_track++;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * DelStream:
 *****************************************************************************/
static int DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    msg_Dbg( p_mux, "removing input" );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Mux:
 *****************************************************************************/
static int Mux( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    if( p_sys->b_write_header )
    {
        block_t *out = asf_header_create( p_mux, VLC_TRUE );

        out->i_flags |= BLOCK_FLAG_HEADER;
        sout_AccessOutWrite( p_mux->p_access, out );

        p_sys->b_write_header = VLC_FALSE;
    }

    for( ;; )
    {
        sout_input_t  *p_input;
        asf_track_t   *tk;
        int           i_stream;
        mtime_t       i_dts;
        block_t *data;
        block_t *pk;

        if( MuxGetStream( p_mux, &i_stream, &i_dts ) )
        {
            /* not enough data */
            return VLC_SUCCESS;
        }

        if( p_sys->i_dts_first < 0 )
        {
            p_sys->i_dts_first = i_dts;
        }
        if( p_sys->i_dts_last < i_dts )
        {
            p_sys->i_dts_last = i_dts;
        }

        p_input = p_mux->pp_inputs[i_stream];
        tk      = (asf_track_t*)p_input->p_sys;

        data = block_FifoGet( p_input->p_fifo );

        if( ( pk = asf_packet_create( p_mux, tk, data ) ) )
        {
            sout_AccessOutWrite( p_mux->p_access, pk );
        }
    }

    return VLC_SUCCESS;
}

static int MuxGetStream( sout_mux_t *p_mux, int *pi_stream, mtime_t *pi_dts )
{
    mtime_t i_dts;
    int     i_stream;
    int     i;

    for( i = 0, i_dts = 0, i_stream = -1; i < p_mux->i_nb_inputs; i++ )
    {
        sout_input_t  *p_input = p_mux->pp_inputs[i];
        block_t *p_data;

        if( p_input->p_fifo->i_depth <= 0 )
        {
            if( p_input->p_fmt->i_cat == AUDIO_ES ||
                p_input->p_fmt->i_cat == VIDEO_ES )
            {
                /* We need that audio+video fifo contain at least 1 packet */
                return VLC_EGENERIC;
            }
            /* SPU */
            continue;
        }

        p_data = block_FifoShow( p_input->p_fifo );
        if( i_stream == -1 || p_data->i_dts < i_dts )
        {
            i_stream = i;
            i_dts    = p_data->i_dts;
        }
    }

    *pi_stream = i_stream;
    *pi_dts = i_dts;

    return VLC_SUCCESS;
}

/****************************************************************************
 * Asf header construction
 ****************************************************************************/

/****************************************************************************
 * Buffer out
 ****************************************************************************/
static void bo_init( bo_t *p_bo, uint8_t *p_buffer, int i_size )
{
    p_bo->i_buffer_size = i_size;
    p_bo->i_buffer = 0;
    p_bo->p_buffer = p_buffer;
}
static void bo_add_u8( bo_t *p_bo, uint8_t i )
{
    if( p_bo->i_buffer < p_bo->i_buffer_size )
    {
        p_bo->p_buffer[p_bo->i_buffer] = i;
    }
    p_bo->i_buffer++;
}
static void bo_addle_u16( bo_t *p_bo, uint16_t i )
{
    bo_add_u8( p_bo, i &0xff );
    bo_add_u8( p_bo, ( ( i >> 8) &0xff ) );
}
static void bo_addle_u32( bo_t *p_bo, uint32_t i )
{
    bo_addle_u16( p_bo, i &0xffff );
    bo_addle_u16( p_bo, ( ( i >> 16) &0xffff ) );
}
static void bo_addle_u64( bo_t *p_bo, uint64_t i )
{
    bo_addle_u32( p_bo, i &0xffffffff );
    bo_addle_u32( p_bo, ( ( i >> 32) &0xffffffff ) );
}

static void bo_add_mem( bo_t *p_bo, uint8_t *p_mem, int i_size )
{
    int i_copy = __MIN( i_size, p_bo->i_buffer_size - p_bo->i_buffer );

    if( i_copy > 0 )
    {
        memcpy( &p_bo->p_buffer[p_bo->i_buffer], p_mem, i_copy );
    }
    p_bo->i_buffer += i_size;
}

static void bo_addle_str16( bo_t *bo, char *str )
{
    bo_addle_u16( bo, strlen( str ) + 1 );
    for( ;; )
    {
        uint16_t c = (uint8_t)*str++;
        bo_addle_u16( bo, c );
        if( c == '\0' ) break;
    }
}

static void bo_addle_str16_nosize( bo_t *bo, char *str )
{
    for( ;; )
    {
        uint16_t c = (uint8_t)*str++;
        bo_addle_u16( bo, c );
        if( c == '\0' ) break;
    }
}

/****************************************************************************
 * GUID definitions
 ****************************************************************************/
static void bo_add_guid( bo_t *p_bo, const guid_t *id )
{
    int i;
    bo_addle_u32( p_bo, id->Data1 );
    bo_addle_u16( p_bo, id->Data2 );
    bo_addle_u16( p_bo, id->Data3 );
    for( i = 0; i < 8; i++ )
    {
        bo_add_u8( p_bo, id->Data4[i] );
    }
}

static const guid_t asf_object_header_guid =
{0x75B22630, 0x668E, 0x11CF, {0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C}};
static const guid_t asf_object_data_guid =
{0x75B22636, 0x668E, 0x11CF, {0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C}};
static const guid_t asf_object_file_properties_guid =
{0x8cabdca1, 0xa947, 0x11cf, {0x8e, 0xe4, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65}};
static const guid_t asf_object_stream_properties_guid =
{0xB7DC0791, 0xA9B7, 0x11CF, {0x8E, 0xE6, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65}};
static const guid_t asf_object_header_extention_guid =
{0x5FBF03B5, 0xA92E, 0x11CF, {0x8E, 0xE3, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65}};
static const guid_t asf_object_stream_type_audio =
{0xF8699E40, 0x5B4D, 0x11CF, {0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B}};
static const guid_t asf_object_stream_type_video =
{0xbc19efc0, 0x5B4D, 0x11CF, {0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B}};
static const guid_t asf_guid_audio_conceal_none =
{0x20FB5700, 0x5B55, 0x11CF, {0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B}};
static const guid_t asf_guid_audio_conceal_spread =
{0xBFC3CD50, 0x618F, 0x11CF, {0x8B, 0xB2, 0x00, 0xAA, 0x00, 0xB4, 0xE2, 0x20}};
static const guid_t asf_guid_video_conceal_none =
{0x20FB5700, 0x5B55, 0x11CF, {0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B}};
static const guid_t asf_guid_reserved_1 =
{0xABD3D211, 0xA9BA, 0x11cf, {0x8E, 0xE6, 0x00, 0xC0, 0x0C ,0x20, 0x53, 0x65}};
static const guid_t asf_object_codec_list_guid =
{0x86D15240, 0x311D, 0x11D0, {0xA3, 0xA4, 0x00, 0xA0, 0xC9, 0x03, 0x48, 0xF6}};
static const guid_t asf_object_codec_list_reserved_guid =
{0x86D15241, 0x311D, 0x11D0, {0xA3, 0xA4, 0x00, 0xA0, 0xC9, 0x03, 0x48, 0xF6}};
static const guid_t asf_object_content_description_guid =
{0x75B22633, 0x668E, 0x11CF, {0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c}};
static const guid_t asf_object_index_guid =
{0x33000890, 0xE5B1, 0x11CF, {0x89, 0xF4, 0x00, 0xA0, 0xC9, 0x03, 0x49, 0xCB}};
static const guid_t asf_object_metadata_guid =
{0xC5F8CBEA, 0x5BAF, 0x4877, {0x84, 0x67, 0xAA, 0x8C, 0x44, 0xFA, 0x4C, 0xCA}};

/****************************************************************************
 * Misc
 ****************************************************************************/
static void asf_chunk_add( bo_t *bo,
                           int i_type, int i_len, int i_flags, int i_seq )
{
    bo_addle_u16( bo, i_type );
    bo_addle_u16( bo, i_len + 8 );
    bo_addle_u32( bo, i_seq );
    bo_addle_u16( bo, i_flags );
    bo_addle_u16( bo, i_len + 8 );
}

static block_t *asf_header_create( sout_mux_t *p_mux, vlc_bool_t b_broadcast )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    asf_track_t    *tk;
    mtime_t i_duration = 0;
    int i_size, i_header_ext_size, i;
    int i_ci_size, i_cm_size = 0, i_cd_size = 0;
    block_t *out;
    bo_t bo;

    msg_Dbg( p_mux, "Asf muxer creating header" );

    if( p_sys->i_dts_first > 0 )
    {
        i_duration = p_sys->i_dts_last - p_sys->i_dts_first;
        if( i_duration < 0 ) i_duration = 0;
    }

    /* calculate header size */
    i_size = 30 + 104;
    i_ci_size = 44;
    for( i = 0; i < p_sys->i_track; i++ )
    {
        i_size += 78 + p_sys->track[i].i_extra;
        i_ci_size += 8 + 2 * strlen( p_sys->track[i].psz_name );
        if( p_sys->track[i].i_cat == AUDIO_ES ) i_ci_size += 4;
        else if( p_sys->track[i].i_cat == VIDEO_ES ) i_ci_size += 6;

        /* Error correction data field */
        if( p_sys->track[i].b_audio_correction ) i_size += 8;
    }

    /* size of the content description object */
    if( *p_sys->psz_title || *p_sys->psz_author || *p_sys->psz_copyright ||
        *p_sys->psz_comment || *p_sys->psz_rating )
    {
        i_cd_size = 34 + 2 * ( strlen( p_sys->psz_title ) + 1 +
                             strlen( p_sys->psz_author ) + 1 +
                             strlen( p_sys->psz_copyright ) + 1 +
                             strlen( p_sys->psz_comment ) + 1 +
                             strlen( p_sys->psz_rating ) + 1 );
    }

    /* size of the metadata object */
    for( i = 0; i < p_sys->i_track; i++ )
    {
        if( p_sys->track[i].i_cat == VIDEO_ES )
        {
            i_cm_size = 26 + 2 * (16 + 2 * sizeof("AspectRatio?"));
            break;
        }
    }

    i_header_ext_size = i_cm_size ? i_cm_size + 46 : 0;
    i_size += i_ci_size + i_cd_size + i_header_ext_size ;

    if( p_sys->b_asf_http )
    {
        out = block_New( p_mux, i_size + 50 + 12 );
        bo_init( &bo, out->p_buffer, i_size + 50 + 12 );
        asf_chunk_add( &bo, 0x4824, i_size + 50, 0xc00, p_sys->i_seq++ );
    }
    else
    {
        out = block_New( p_mux, i_size + 50 );
        bo_init( &bo, out->p_buffer, i_size + 50 );
    }

    /* header object */
    bo_add_guid ( &bo, &asf_object_header_guid );
    bo_addle_u64( &bo, i_size );
    bo_addle_u32( &bo, 2 + p_sys->i_track +
                  (i_cd_size ? 1 : 0) + (i_cm_size ? 1 : 0) );
    bo_add_u8   ( &bo, 1 );
    bo_add_u8   ( &bo, 2 );

    /* sub object */

    /* file properties */
    bo_add_guid ( &bo, &asf_object_file_properties_guid );
    bo_addle_u64( &bo, 104 );
    bo_add_guid ( &bo, &p_sys->fid );
    bo_addle_u64( &bo, i_size + 50 + p_sys->i_packet_count *
                                p_sys->i_packet_size ); /* file size */
    bo_addle_u64( &bo, 0 );                 /* creation date */
    bo_addle_u64( &bo, b_broadcast ? 0xffffffffLL : p_sys->i_packet_count );
    bo_addle_u64( &bo, i_duration * 10 );   /* play duration (100ns) */
    bo_addle_u64( &bo, i_duration * 10 );   /* send duration (100ns) */
    bo_addle_u64( &bo, p_sys->i_preroll_time ); /* preroll duration (ms) */
    bo_addle_u32( &bo, b_broadcast ? 0x01 : 0x02 /* seekable */ ); /* flags */
    bo_addle_u32( &bo, p_sys->i_packet_size );  /* packet size min */
    bo_addle_u32( &bo, p_sys->i_packet_size );  /* packet size max */
    bo_addle_u32( &bo, p_sys->i_bitrate );      /* maxbitrate */

    /* header extention */
    if( i_header_ext_size )
    {
        bo_add_guid ( &bo, &asf_object_header_extention_guid );
        bo_addle_u64( &bo, i_header_ext_size );
        bo_add_guid ( &bo, &asf_guid_reserved_1 );
        bo_addle_u16( &bo, 6 );
        bo_addle_u32( &bo, i_header_ext_size - 46 );
    }

    /* metadata object (part of header extension) */
    if( i_cm_size )
    {
        int64_t i_num, i_den;
        int i_dst_num, i_dst_den;

        for( i = 0; i < p_sys->i_track; i++ )
            if( p_sys->track[i].i_cat == VIDEO_ES ) break;

        i_num = p_sys->track[i].fmt.video.i_aspect *
            (int64_t)p_sys->track[i].fmt.video.i_height;
        i_den = VOUT_ASPECT_FACTOR * p_sys->track[i].fmt.video.i_width;
        vlc_ureduce( &i_dst_num, &i_dst_den, i_num, i_den, 0 );

        msg_Dbg( p_mux, "pixel aspect-ratio: %i/%i", i_dst_num, i_dst_den );

        bo_add_guid ( &bo, &asf_object_metadata_guid );
        bo_addle_u64( &bo, i_cm_size );
        bo_addle_u16( &bo, 2 ); /* description records count */
        /* 1st description record */
        bo_addle_u16( &bo, 0 ); /* reserved */
        bo_addle_u16( &bo, i + 1 ); /* stream number (0 for the whole file) */
        bo_addle_u16( &bo, 2 * sizeof("AspectRatioX") ); /* name length */
        bo_addle_u16( &bo, 0x3 /* DWORD */ ); /* data type */
        bo_addle_u32( &bo, 4 ); /* data length */
        bo_addle_str16_nosize( &bo, "AspectRatioX" );
        bo_addle_u32( &bo, i_dst_num ); /* data */
        /* 2nd description record */
        bo_addle_u16( &bo, 0 ); /* reserved */
        bo_addle_u16( &bo, i + 1 ); /* stream number (0 for the whole file) */
        bo_addle_u16( &bo, 2 * sizeof("AspectRatioY") ); /* name length */
        bo_addle_u16( &bo, 0x3 /* DWORD */ ); /* data type */
        bo_addle_u32( &bo, 4 ); /* data length */
        bo_addle_str16_nosize( &bo, "AspectRatioY" );
        bo_addle_u32( &bo, i_dst_den ); /* data */
    }

    /* content description header */
    if( i_cd_size > 0 )
    {
        bo_add_guid ( &bo, &asf_object_content_description_guid );
        bo_addle_u64( &bo, i_cd_size );
        bo_addle_u16( &bo, 2 * strlen( p_sys->psz_title ) + 2 );
        bo_addle_u16( &bo, 2 * strlen( p_sys->psz_author ) + 2 );
        bo_addle_u16( &bo, 2 * strlen( p_sys->psz_copyright ) + 2 );
        bo_addle_u16( &bo, 2 * strlen( p_sys->psz_comment ) + 2 );
        bo_addle_u16( &bo, 2 * strlen( p_sys->psz_rating ) + 2 );

        bo_addle_str16_nosize( &bo, p_sys->psz_title );
        bo_addle_str16_nosize( &bo, p_sys->psz_author );
        bo_addle_str16_nosize( &bo, p_sys->psz_copyright );
        bo_addle_str16_nosize( &bo, p_sys->psz_comment );
        bo_addle_str16_nosize( &bo, p_sys->psz_rating );
    }

    /* stream properties */
    for( i = 0; i < p_sys->i_track; i++ )
    {
        tk = &p_sys->track[i];

        bo_add_guid ( &bo, &asf_object_stream_properties_guid );
        bo_addle_u64( &bo, 78 + tk->i_extra + (tk->b_audio_correction ? 8:0) );

        if( tk->i_cat == AUDIO_ES )
        {
            bo_add_guid( &bo, &asf_object_stream_type_audio );
            if( tk->b_audio_correction )
                bo_add_guid( &bo, &asf_guid_audio_conceal_spread );
            else
                bo_add_guid( &bo, &asf_guid_audio_conceal_none );
        }
        else if( tk->i_cat == VIDEO_ES )
        {
            bo_add_guid( &bo, &asf_object_stream_type_video );
            bo_add_guid( &bo, &asf_guid_video_conceal_none );
        }
        bo_addle_u64( &bo, 0 );         /* time offset */
        bo_addle_u32( &bo, tk->i_extra );
        /* correction data length */
        bo_addle_u32( &bo, tk->b_audio_correction ? 8 : 0 );
        bo_addle_u16( &bo, tk->i_id );  /* stream number */
        bo_addle_u32( &bo, 0 );
        bo_add_mem  ( &bo, tk->p_extra, tk->i_extra );

        /* Error correction data field */
        if( tk->b_audio_correction )
        {
            bo_add_u8( &bo, 0x1 ); /* span */
            bo_addle_u16( &bo, tk->i_blockalign );  /* virtual packet length */
            bo_addle_u16( &bo, tk->i_blockalign );  /* virtual chunck length */
            bo_addle_u16( &bo, 1 );  /* silence length */
            bo_add_u8( &bo, 0x0 ); /* data */
        }
    }

    /* Codec Infos */
    bo_add_guid ( &bo, &asf_object_codec_list_guid );
    bo_addle_u64( &bo, i_ci_size );
    bo_add_guid ( &bo, &asf_object_codec_list_reserved_guid );
    bo_addle_u32( &bo, p_sys->i_track );
    for( i = 0; i < p_sys->i_track; i++ )
    {
        tk = &p_sys->track[i];

        if( tk->i_cat == VIDEO_ES ) bo_addle_u16( &bo, 1 /* video */ );
        else if( tk->i_cat == AUDIO_ES ) bo_addle_u16( &bo, 2 /* audio */ );
        else bo_addle_u16( &bo, 0xFFFF /* unknown */ );

        bo_addle_str16( &bo, tk->psz_name );
        bo_addle_u16( &bo, 0 );
        if( tk->i_cat == AUDIO_ES )
        {
            bo_addle_u16( &bo, 2 );
            bo_addle_u16( &bo, tk->i_tag );
        }
        else if( tk->i_cat == VIDEO_ES )
        {
            bo_addle_u16( &bo, 4 );
            bo_add_mem  ( &bo, (uint8_t*)&tk->i_fourcc, 4 );
        }
    }

    /* data object */
    bo_add_guid ( &bo, &asf_object_data_guid );
    bo_addle_u64( &bo, 50 + p_sys->i_packet_count * p_sys->i_packet_size );
    bo_add_guid ( &bo, &p_sys->fid );
    bo_addle_u64( &bo, p_sys->i_packet_count );
    bo_addle_u16( &bo, 0x101 );

    return out;
}

/****************************************************************************
 *
 ****************************************************************************/
static block_t *asf_packet_flush( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    int i_pad, i_preheader = p_sys->b_asf_http ? 12 : 0;
    block_t *pk;
    bo_t bo;

    if( !p_sys->pk ) return 0;

    i_pad = p_sys->i_packet_size - p_sys->i_pk_used;
    memset( p_sys->pk->p_buffer + p_sys->i_pk_used, 0, i_pad );

    bo_init( &bo, p_sys->pk->p_buffer, 14 + i_preheader );

    if( p_sys->b_asf_http )
        asf_chunk_add( &bo, 0x4424, p_sys->i_packet_size, 0x0, p_sys->i_seq++);

    bo_add_u8   ( &bo, 0x82 );
    bo_addle_u16( &bo, 0 );
    bo_add_u8( &bo, 0x11 );
    bo_add_u8( &bo, 0x5d );
    bo_addle_u16( &bo, i_pad );
    bo_addle_u32( &bo, (p_sys->i_pk_dts - p_sys->i_dts_first) / 1000 +
                  p_sys->i_preroll_time );
    bo_addle_u16( &bo, 0 /* data->i_length */ );
    bo_add_u8( &bo, 0x80 | p_sys->i_pk_frame );

    pk = p_sys->pk;
    p_sys->pk = NULL;

    p_sys->i_packet_count++;

    return pk;
}

static block_t *asf_packet_create( sout_mux_t *p_mux,
                                   asf_track_t *tk, block_t *data )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    int     i_data = data->i_buffer;
    int     i_pos  = 0;
    uint8_t *p_data= data->p_buffer;
    block_t *first = NULL, **last = &first;
    int     i_preheader = p_sys->b_asf_http ? 12 : 0;

    while( i_pos < i_data )
    {
        bo_t bo;
        int i_payload;

        if( p_sys->pk == NULL )
        {
            p_sys->pk = block_New( p_mux, p_sys->i_packet_size + i_preheader );
            /* reserve 14 bytes for the packet header */
            p_sys->i_pk_used = 14 + i_preheader;
            p_sys->i_pk_frame = 0;
            p_sys->i_pk_dts = data->i_dts;
        }

        bo_init( &bo, &p_sys->pk->p_buffer[p_sys->i_pk_used],
                 p_sys->i_packet_size - p_sys->i_pk_used );

        /* add payload (header size = 17) */
        i_payload = __MIN( i_data - i_pos,
                           p_sys->i_packet_size - p_sys->i_pk_used - 17 );

        if( tk->b_audio_correction && p_sys->i_pk_frame && i_payload < i_data )
        {
            /* Don't know why yet but WMP doesn't like splitted WMA packets */
            *last = asf_packet_flush( p_mux );
            last  = &(*last)->p_next;
            continue;
        }

        bo_add_u8   ( &bo, !(data->i_flags & BLOCK_FLAG_TYPE_P ||
                      data->i_flags & BLOCK_FLAG_TYPE_B) ?
                      0x80 | tk->i_id : tk->i_id );
        bo_add_u8   ( &bo, tk->i_sequence );
        bo_addle_u32( &bo, i_pos );
        bo_add_u8   ( &bo, 0x08 );  /* flags */
        bo_addle_u32( &bo, i_data );
        bo_addle_u32( &bo, (data->i_dts - p_sys->i_dts_first) / 1000 +
                      p_sys->i_preroll_time );
        bo_addle_u16( &bo, i_payload );
        bo_add_mem  ( &bo, &p_data[i_pos], i_payload );
        i_pos += i_payload;
        p_sys->i_pk_used += 17 + i_payload;

        p_sys->i_pk_frame++;

        if( p_sys->i_pk_used + 17 >= p_sys->i_packet_size )
        {
            /* Not enough data for another payload, flush the packet */
            *last = asf_packet_flush( p_mux );
            last  = &(*last)->p_next;
        }
    }

    tk->i_sequence++;
    block_Release( data );

    return first;
}

static block_t *asf_stream_end_create( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    block_t *out = NULL;
    bo_t bo;

    if( p_sys->b_asf_http )
    {
        out = block_New( p_mux, 12 );
        bo_init( &bo, out->p_buffer, 12 );
        asf_chunk_add( &bo, 0x4524, 0, 0x00, p_sys->i_seq++ );
    }
    else
    {
        /* Create index */
        out = block_New( p_mux, 56 );
        bo_init( &bo, out->p_buffer, 56 );
        bo_add_guid ( &bo, &asf_object_index_guid );
        bo_addle_u64( &bo, 56 );
        bo_add_guid ( &bo, &p_sys->fid );
        bo_addle_u64( &bo, 10000000 );
        bo_addle_u32( &bo, 5 );
        bo_addle_u32( &bo, 0 );
    }

    return out;
}
