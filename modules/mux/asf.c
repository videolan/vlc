/*****************************************************************************
 * asf.c: asf muxer module for vlc
 *****************************************************************************
 * Copyright (C) 2003-2004, 2006 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_codecs.h>
#include <vlc_arrays.h>
#include <vlc_rand.h>

#include "../demux/asf/libasf_guid.h"

#define MAX_ASF_TRACKS 128
#define ASF_DATA_PACKET_SIZE 4096  // deprecated -- added sout-asf-packet-size

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-asf-"

#define TITLE_TEXT N_("Title")
#define TITLE_LONGTEXT N_("Title to put in ASF comments." )
#define AUTHOR_TEXT N_("Author")
#define AUTHOR_LONGTEXT N_("Author to put in ASF comments." )
#define COPYRIGHT_TEXT N_("Copyright")
#define COPYRIGHT_LONGTEXT N_("Copyright string to put in ASF comments." )
#define COMMENT_TEXT N_("Comment")
#define COMMENT_LONGTEXT N_("Comment to put in ASF comments." )
#define RATING_TEXT N_("Rating")
#define RATING_LONGTEXT N_("\"Rating\" to put in ASF comments." )
#define PACKETSIZE_TEXT N_("Packet Size")
#define PACKETSIZE_LONGTEXT N_("ASF packet size -- default is 4096 bytes")
#define BITRATE_TEXT N_("Bitrate override")
#define BITRATE_LONGTEXT N_("Do not try to guess ASF bitrate. Setting this, allows you to control how Windows Media Player will cache streamed content. Set to audio+video bitrate in bytes")


vlc_module_begin ()
    set_description( N_("ASF muxer") )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_MUX )
    set_shortname( "ASF" )

    set_capability( "sout mux", 5 )
    add_shortcut( "asf", "asfh" )
    set_callbacks( Open, Close )

    add_string( SOUT_CFG_PREFIX "title", "", TITLE_TEXT, TITLE_LONGTEXT,
                                 true )
    add_string( SOUT_CFG_PREFIX "author",   "", AUTHOR_TEXT,
                                 AUTHOR_LONGTEXT, true )
    add_string( SOUT_CFG_PREFIX "copyright","", COPYRIGHT_TEXT,
                                 COPYRIGHT_LONGTEXT, true )
    add_string( SOUT_CFG_PREFIX "comment",  "", COMMENT_TEXT,
                                 COMMENT_LONGTEXT, true )
    add_string( SOUT_CFG_PREFIX "rating",  "", RATING_TEXT,
                                 RATING_LONGTEXT, true )
    add_integer( SOUT_CFG_PREFIX "packet-size", 4096, PACKETSIZE_TEXT,
                                 PACKETSIZE_LONGTEXT, true )
    add_integer( SOUT_CFG_PREFIX "bitrate-override", 0, BITRATE_TEXT,
                                 BITRATE_LONGTEXT, true )

vlc_module_end ()

/*****************************************************************************
 * Locales prototypes
 *****************************************************************************/
static const char *const ppsz_sout_options[] = {
    "title", "author", "copyright", "comment", "rating", NULL
};

static int Control  ( sout_mux_t *, int, va_list );
static int AddStream( sout_mux_t *, sout_input_t * );
static void DelStream( sout_mux_t *, sout_input_t * );
static int Mux      ( sout_mux_t * );

typedef struct
{
    int          i_id;
    enum es_format_category_e i_cat;

    /* codec information */
    uint16_t     i_tag;     /* for audio */
    vlc_fourcc_t i_fourcc;  /* for video */
    const char         *psz_name; /* codec name */
    int          i_blockalign; /* for audio only */
    bool   b_audio_correction;

    int          i_sequence;

    int          i_extra;
    uint8_t      *p_extra;
    bool         b_extended;

    es_format_t  fmt;

} asf_track_t;

typedef struct
{
    vlc_guid_t      fid;    /* file id */
    int             i_packet_size;
    int64_t         i_packet_count;
    vlc_tick_t      i_dts_first;
    vlc_tick_t      i_dts_last;
    int64_t         i_preroll_time; /* in milliseconds */
    int64_t         i_bitrate;
    int64_t         i_bitrate_override;

    vlc_array_t     tracks;

    bool            b_write_header;

    block_t         *pk;
    int             i_pk_used;
    int             i_pk_frame;
    vlc_tick_t      i_pk_dts;

    bool      b_asf_http;
    int             i_seq;

    /* meta data */
    char            *psz_title;
    char            *psz_author;
    char            *psz_copyright;
    char            *psz_comment;
    char            *psz_rating;
} sout_mux_sys_t;

static block_t *asf_header_create( sout_mux_t *, bool );
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

static void bo_addle_str16( bo_t *, const char * );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_mux_t     *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t *p_sys;

    msg_Dbg( p_mux, "asf muxer opened" );
    config_ChainParse( p_mux, SOUT_CFG_PREFIX, ppsz_sout_options, p_mux->p_cfg );

    p_mux->pf_control   = Control;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;

    p_mux->p_sys = p_sys = malloc( sizeof( sout_mux_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;
    p_sys->b_asf_http = p_mux->psz_mux && !strcmp( p_mux->psz_mux, "asfh" );
    if( p_sys->b_asf_http )
    {
        msg_Dbg( p_mux, "creating asf stream to be used with mmsh" );
    }
    p_sys->pk = NULL;
    p_sys->i_pk_used    = 0;
    p_sys->i_pk_frame   = 0;
    p_sys->i_dts_first  =
    p_sys->i_dts_last   = VLC_TICK_INVALID;
    p_sys->i_preroll_time = 2000;
    p_sys->i_bitrate    = 0;
    p_sys->i_bitrate_override = 0;
    p_sys->i_seq        = 0;
    vlc_array_init( &p_sys->tracks );

    p_sys->b_write_header = true;
    p_sys->i_packet_size = var_InheritInteger( p_mux, "sout-asf-packet-size" );
    p_sys->i_bitrate_override = var_InheritInteger( p_mux, "sout-asf-bitrate-override" );
    msg_Dbg( p_mux, "Packet size %d", p_sys->i_packet_size);
    if (p_sys->i_bitrate_override)
        msg_Dbg( p_mux, "Bitrate override %"PRId64, p_sys->i_bitrate_override);
    p_sys->i_packet_count= 0;

    /* Generate a random fid */
    p_sys->fid.Data1 = 0xbabac001;
    vlc_rand_bytes(&p_sys->fid.Data2, sizeof(p_sys->fid.Data2));
    vlc_rand_bytes(&p_sys->fid.Data3, sizeof(p_sys->fid.Data3));
    vlc_rand_bytes(p_sys->fid.Data4, sizeof(p_sys->fid.Data4));

    /* Meta data */
    p_sys->psz_title = var_GetString( p_mux, SOUT_CFG_PREFIX "title" );
    p_sys->psz_author = var_GetString( p_mux, SOUT_CFG_PREFIX "author" );
    p_sys->psz_copyright = var_GetString( p_mux, SOUT_CFG_PREFIX "copyright" );
    p_sys->psz_comment = var_GetString( p_mux, SOUT_CFG_PREFIX "comment" );
    p_sys->psz_rating = var_GetString( p_mux, SOUT_CFG_PREFIX "rating" );

    msg_Dbg( p_mux, "meta data: title='%s', author='%s', copyright='%s', "
             "comment='%s', rating='%s'",
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
    if( sout_AccessOutSeek( p_mux->p_access, 0 ) == VLC_SUCCESS )
    {
        out = asf_header_create( p_mux, false );
        sout_AccessOutWrite( p_mux->p_access, out );
    }


    for( size_t i = 0; i < vlc_array_count( &p_sys->tracks ); i++ )
    {
        asf_track_t *track = vlc_array_item_at_index( &p_sys->tracks, i );
        free( track->p_extra );
        es_format_Clean( &track->fmt );
        free( track );
    }

    vlc_array_clear( &p_sys->tracks );
    free( p_sys->psz_title );
    free( p_sys->psz_author );
    free( p_sys->psz_copyright );
    free( p_sys->psz_comment );
    free( p_sys->psz_rating );
    free( p_sys );
}

/*****************************************************************************
 * Capability:
 *****************************************************************************/
static int Control( sout_mux_t *p_mux, int i_query, va_list args )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    bool *pb_bool;
    char **ppsz;

    switch( i_query )
    {
       case MUX_CAN_ADD_STREAM_WHILE_MUXING:
           pb_bool = va_arg( args, bool * );
           *pb_bool = false;
           return VLC_SUCCESS;

       case MUX_GET_MIME:
           ppsz = va_arg( args, char ** );
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
    if( vlc_array_count( &p_sys->tracks ) >= MAX_ASF_TRACKS )
    {
        msg_Dbg( p_mux, "cannot add this track (too much tracks)" );
        return VLC_EGENERIC;
    }

    tk = p_input->p_sys = malloc( sizeof( asf_track_t ) );
    if( unlikely(p_input->p_sys == NULL) )
        return VLC_ENOMEM;
    memset( tk, 0, sizeof( *tk ) );
    tk->i_cat = p_input->p_fmt->i_cat;
    tk->i_sequence = 0;
    tk->b_audio_correction = 0;
    tk->b_extended = false;

    switch( tk->i_cat )
    {
        case AUDIO_ES:
        {
            int i_blockalign = p_input->p_fmt->audio.i_blockalign;
            int i_bitspersample = p_input->p_fmt->audio.i_bitspersample;
            int i_extra = 0;

            switch( p_input->p_fmt->i_codec )
            {
                case VLC_CODEC_A52:
                    tk->i_tag = WAVE_FORMAT_A52;
                    tk->psz_name = "A/52";
                    i_bitspersample = 0;
                    break;
                case VLC_CODEC_MP4A:
                    tk->i_tag = WAVE_FORMAT_AAC;
                    tk->psz_name = "MPEG-4 Audio";
                    i_bitspersample = 0;
                    break;
                case VLC_CODEC_MP3:
                    tk->psz_name = "MPEG Audio Layer 3";
                    tk->i_tag = WAVE_FORMAT_MPEGLAYER3;
                    i_bitspersample = 0;
                    i_blockalign = 1;
                    i_extra = 12;
                    break;
                case VLC_CODEC_MPGA:
                    tk->psz_name = "MPEG Audio Layer 1/2";
                    tk->i_tag = WAVE_FORMAT_MPEG;
                    i_bitspersample = 0;
                    i_blockalign = 1;
                    i_extra = 22;
                    break;
                case VLC_CODEC_WMA1:
                    tk->psz_name = "Windows Media Audio v1";
                    tk->i_tag = WAVE_FORMAT_WMA1;
                    tk->b_audio_correction = true;
                    break;
                case VLC_CODEC_WMA2:
                    tk->psz_name= "Windows Media Audio (v2) 7, 8 and 9 Series";
                    tk->i_tag = WAVE_FORMAT_WMA2;
                    tk->b_audio_correction = true;
                    break;
                case VLC_CODEC_WMAP:
                    tk->psz_name = "Windows Media Audio 9 Professional";
                    tk->i_tag = WAVE_FORMAT_WMAP;
                    tk->b_audio_correction = true;
                    break;
                case VLC_CODEC_WMAL:
                    tk->psz_name = "Windows Media Audio 9 Lossless";
                    tk->i_tag = WAVE_FORMAT_WMAL;
                    tk->b_audio_correction = true;
                    break;
                    /* raw codec */
                case VLC_CODEC_U8:
                    tk->psz_name = "Raw audio 8bits";
                    tk->i_tag = WAVE_FORMAT_PCM;
                    i_blockalign= p_input->p_fmt->audio.i_channels;
                    i_bitspersample = 8;
                    break;
                case VLC_CODEC_S16L:
                    tk->psz_name = "Raw audio 16bits";
                    tk->i_tag = WAVE_FORMAT_PCM;
                    i_blockalign= 2 * p_input->p_fmt->audio.i_channels;
                    i_bitspersample = 16;
                    break;
                case VLC_CODEC_S24L:
                    tk->psz_name = "Raw audio 24bits";
                    tk->i_tag = WAVE_FORMAT_PCM;
                    i_blockalign= 3 * p_input->p_fmt->audio.i_channels;
                    i_bitspersample = 24;
                    break;
                case VLC_CODEC_S32L:
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
            if( !tk->p_extra )
                return VLC_ENOMEM;
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
                p_sys->i_bitrate += 128000;
            }
            if (p_sys->i_bitrate_override)
                p_sys->i_bitrate = p_sys->i_bitrate_override;
            break;
        }
        case VIDEO_ES:
        {
            const es_format_t *p_fmt = p_input->p_fmt;
            uint8_t *p_codec_extra = NULL;
            int     i_codec_extra = 0;

            if( p_fmt->i_codec == VLC_CODEC_MP4V )
            {
                tk->psz_name = "MPEG-4 Video";
                tk->i_fourcc = VLC_FOURCC( 'M', 'P', '4', 'S' );
            }
            else if( p_fmt->i_codec == VLC_CODEC_DIV3 )
            {
                tk->psz_name = "MSMPEG-4 V3 Video";
                tk->i_fourcc = VLC_FOURCC( 'M', 'P', '4', '3' );
            }
            else if( p_fmt->i_codec == VLC_CODEC_DIV2 )
            {
                tk->psz_name = "MSMPEG-4 V2 Video";
                tk->i_fourcc = VLC_FOURCC( 'M', 'P', '4', '2' );
            }
            else if( p_fmt->i_codec == VLC_CODEC_DIV1 )
            {
                tk->psz_name = "MSMPEG-4 V1 Video";
                tk->i_fourcc = VLC_FOURCC( 'M', 'P', 'G', '4' );
            }
            else if( p_fmt->i_codec == VLC_CODEC_WMV1 )
            {
                tk->psz_name = "Windows Media Video 7";
                tk->i_fourcc = VLC_FOURCC( 'W', 'M', 'V', '1' );
            }
            else if( p_fmt->i_codec == VLC_CODEC_WMV2 )
            {
                tk->psz_name = "Windows Media Video 8";
                tk->i_fourcc = VLC_FOURCC( 'W', 'M', 'V', '2' );
            }
            else if( p_fmt->i_codec == VLC_CODEC_WMV3 )
            {
                tk->psz_name = "Windows Media Video 9";
                tk->i_fourcc = VLC_FOURCC( 'W', 'M', 'V', '3' );
                tk->b_extended = true;
            }
            else if( p_fmt->i_codec == VLC_CODEC_VC1 )
            {
                tk->psz_name = "Windows Media Video 9 Advanced Profile";
                tk->i_fourcc = VLC_FOURCC( 'W', 'V', 'C', '1' );
                tk->b_extended = true;

                if( p_fmt->i_extra > 0 )
                {
                    p_codec_extra = malloc( 1 + p_fmt->i_extra );
                    if( p_codec_extra )
                    {
                        i_codec_extra = 1 + p_fmt->i_extra;
                        p_codec_extra[0] = 0x01;
                        memcpy( &p_codec_extra[1], p_fmt->p_extra, p_fmt->i_extra );
                    }
                }
            }
            else if( p_fmt->i_codec == VLC_CODEC_H264 )
            {
                tk->psz_name = "H.264/MPEG-4 AVC";
                tk->i_fourcc = VLC_FOURCC('h','2','6','4');
            }
            else
            {
                tk->psz_name = _("Unknown Video");
                tk->i_fourcc = p_fmt->i_original_fourcc ? p_fmt->i_original_fourcc : p_fmt->i_codec;
            }
            if( !i_codec_extra && p_fmt->i_extra > 0 )
            {
                p_codec_extra = malloc( p_fmt->i_extra );
                if( p_codec_extra )
                {
                    i_codec_extra = p_fmt->i_extra;
                    memcpy( p_codec_extra, p_fmt->p_extra, p_fmt->i_extra );
                }
            }

            tk->i_extra = 11 + sizeof( VLC_BITMAPINFOHEADER ) + i_codec_extra;
            tk->p_extra = malloc( tk->i_extra );
            if( !tk->p_extra )
            {
                free( p_codec_extra );
                return VLC_ENOMEM;
            }
            bo_init( &bo, tk->p_extra, tk->i_extra );
            bo_addle_u32( &bo, p_fmt->video.i_width );
            bo_addle_u32( &bo, p_fmt->video.i_height );
            bo_add_u8   ( &bo, 0x02 );  /* flags */
            bo_addle_u16( &bo, sizeof( VLC_BITMAPINFOHEADER ) + i_codec_extra );
            bo_addle_u32( &bo, sizeof( VLC_BITMAPINFOHEADER ) + i_codec_extra );
            bo_addle_u32( &bo, p_fmt->video.i_width );
            bo_addle_u32( &bo, p_fmt->video.i_height );
            bo_addle_u16( &bo, 1 );
            bo_addle_u16( &bo, 24 );
            bo_add_mem( &bo, (uint8_t*)&tk->i_fourcc, 4 );
            bo_addle_u32( &bo, 0 );
            bo_addle_u32( &bo, 0 );
            bo_addle_u32( &bo, 0 );
            bo_addle_u32( &bo, 0 );
            bo_addle_u32( &bo, 0 );
            if( i_codec_extra > 0 )
            {
                bo_add_mem( &bo, p_codec_extra, i_codec_extra );
                free( p_codec_extra );
            }

            if( p_fmt->i_bitrate > 50000 )
            {
                p_sys->i_bitrate += p_fmt->i_bitrate;
            }
            else
            {
                p_sys->i_bitrate += 512000;
            }
            if( p_sys->i_bitrate_override )
                p_sys->i_bitrate = p_sys->i_bitrate_override;
            break;
        }
        default:
            msg_Err(p_mux, "unhandled track type" );
            free( tk );
            return VLC_EGENERIC;
    }

    if( vlc_array_append( &p_sys->tracks, tk ) )
    {
        free( tk->p_extra );
        free( tk );
        return VLC_EGENERIC;
    }

    es_format_Copy( &tk->fmt, p_input->p_fmt );

    tk->i_id = vlc_array_index_of_item( &p_sys->tracks, tk ) + 1;

    if( p_sys->b_asf_http )
        p_sys->b_write_header = true;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DelStream:
 *****************************************************************************/
static void DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    /* if bitrate ain't defined in commandline, reduce it when tracks are deleted
     */
    sout_mux_sys_t   *p_sys = p_mux->p_sys;
    asf_track_t      *tk = p_input->p_sys;
    msg_Dbg( p_mux, "removing input" );
    if(!p_sys->i_bitrate_override)
    {
        if( tk->i_cat == AUDIO_ES )
        {
             if( p_input->p_fmt->i_bitrate > 24000 )
                 p_sys->i_bitrate -= p_input->p_fmt->i_bitrate;
             else
                 p_sys->i_bitrate -= 128000;
        }
        else if(tk->i_cat == VIDEO_ES )
        {
             if( p_input->p_fmt->i_bitrate > 50000 )
                 p_sys->i_bitrate -= p_input->p_fmt->i_bitrate;
             else
                 p_sys->i_bitrate -= 512000;
        }
    }

    if( p_sys->b_asf_http )
    {
        vlc_array_remove( &p_sys->tracks, vlc_array_index_of_item( &p_sys->tracks, tk ) );
        p_sys->b_write_header = true;
    }
}

/*****************************************************************************
 * Mux:
 *****************************************************************************/
static int Mux( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    if( p_sys->b_write_header )
    {
        block_t *out = asf_header_create( p_mux, true );

        out->i_flags |= BLOCK_FLAG_HEADER;
        sout_AccessOutWrite( p_mux->p_access, out );

        p_sys->b_write_header = false;
    }

    for( ;; )
    {
        sout_input_t  *p_input;
        asf_track_t   *tk;
        vlc_tick_t    i_dts;
        block_t *data;
        block_t *pk;

        int i_stream = sout_MuxGetStream( p_mux, 1, &i_dts );
        if( i_stream < 0 )
        {
            /* not enough data */
            return VLC_SUCCESS;
        }

        if( p_sys->i_dts_first == VLC_TICK_INVALID )
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

        /* Convert VC1 to ASF special format */
        if( tk->i_fourcc == VLC_FOURCC( 'W', 'V', 'C', '1' ) )
        {
            while( data->i_buffer >= 4 &&
                   ( data->p_buffer[0] != 0x00 || data->p_buffer[1] != 0x00 ||
                     data->p_buffer[2] != 0x01 ||
                     ( data->p_buffer[3] != 0x0D && data->p_buffer[3] != 0x0C ) ) )
            {
                data->i_buffer--;
                data->p_buffer++;
            }
            if( data->i_buffer >= 4 )
            {
                data->i_buffer -= 4;
                data->p_buffer += 4;
            }
        }

        if( ( pk = asf_packet_create( p_mux, tk, data ) ) )
        {
            sout_AccessOutWrite( p_mux->p_access, pk );
        }
    }

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

static void bo_addle_str16( bo_t *bo, const char *str )
{
    bo_addle_u16( bo, strlen( str ) + 1 );
    for( ;; )
    {
        uint16_t c = (uint8_t)*str++;
        bo_addle_u16( bo, c );
        if( c == '\0' ) break;
    }
}

static void bo_addle_str16_nosize( bo_t *bo, const char *str )
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
static void bo_add_guid( bo_t *p_bo, const vlc_guid_t *id )
{
    bo_addle_u32( p_bo, id->Data1 );
    bo_addle_u16( p_bo, id->Data2 );
    bo_addle_u16( p_bo, id->Data3 );
    for( int i = 0; i < 8; i++ )
    {
        bo_add_u8( p_bo, id->Data4[i] );
    }
}

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

static block_t *asf_header_create( sout_mux_t *p_mux, bool b_broadcast )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    vlc_tick_t i_duration = 0;
    int i_size, i_header_ext_size;
    int i_ci_size, i_cm_size = 0, i_cd_size = 0;
    block_t *out;
    bo_t bo;

    msg_Dbg( p_mux, "Asf muxer creating header" );

    if( p_sys->i_dts_first != VLC_TICK_INVALID && p_sys->i_dts_last > p_sys->i_dts_first )
    {
        i_duration = p_sys->i_dts_last - p_sys->i_dts_first;
    }

    /* calculate header size */
    i_size = 30 + 104;
    i_ci_size = 44;
    for( size_t i = 0; i < vlc_array_count( &p_sys->tracks ); i++ )
    {
        asf_track_t *tk = vlc_array_item_at_index( &p_sys->tracks, i );
        /* update also track-id */
        tk->i_id = i + 1;

        i_size += 78 + tk->i_extra;
        i_ci_size += 8 + 2 * strlen( tk->psz_name );
        if( tk->i_cat == AUDIO_ES ) i_ci_size += 4;
        else if( tk->i_cat == VIDEO_ES ) i_ci_size += 6;

        /* Error correction data field */
        if( tk->b_audio_correction ) i_size += 8;
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

    i_header_ext_size = 46;

    /* size of the metadata object */
    for( size_t i = 0; i < vlc_array_count( &p_sys->tracks ); i++ )
    {
        const asf_track_t *p_track = vlc_array_item_at_index( &p_sys->tracks, i );
        if( p_track->i_cat == VIDEO_ES &&
            p_track->fmt.video.i_sar_num != 0 &&
            p_track->fmt.video.i_sar_den != 0 )
        {
            i_cm_size = 26 + 2 * (16 + 2 * sizeof("AspectRatio?"));
        }
        if( p_track->b_extended )
            i_header_ext_size += 88;

    }

    i_header_ext_size += i_cm_size;

    i_size += i_ci_size + i_cd_size + i_header_ext_size ;

    if( p_sys->b_asf_http )
    {
        out = block_Alloc( i_size + 50 + 12 );
        bo_init( &bo, out->p_buffer, i_size + 50 + 12 );
        asf_chunk_add( &bo, 0x4824, i_size + 50, 0xc00, p_sys->i_seq++ );
    }
    else
    {
        out = block_Alloc( i_size + 50 );
        bo_init( &bo, out->p_buffer, i_size + 50 );
    }

    /* header object */
    bo_add_guid ( &bo, &asf_object_header_guid );
    bo_addle_u64( &bo, i_size );
    bo_addle_u32( &bo, 2 + vlc_array_count( &p_sys->tracks ) + 1 +
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
    bo_addle_u64( &bo, MSFTIME_FROM_VLC_TICK(i_duration) );   /* play duration (100ns) */
    bo_addle_u64( &bo, MSFTIME_FROM_VLC_TICK(i_duration) );   /* send duration (100ns) */
    bo_addle_u64( &bo, p_sys->i_preroll_time ); /* preroll duration (ms) */
    bo_addle_u32( &bo, b_broadcast ? 0x01 : 0x02 /* seekable */ ); /* flags */
    bo_addle_u32( &bo, p_sys->i_packet_size );  /* packet size min */
    bo_addle_u32( &bo, p_sys->i_packet_size );  /* packet size max */
    /* NOTE: According to p6-9 of the ASF specification the bitrate cannot be 0,
     * therefor apply this workaround to make sure it is not 0. If the bitrate is
     * 0 the file will play in WMP11, but not in Sliverlight and WMP12 */
    bo_addle_u32( &bo, p_sys->i_bitrate > 0 ? p_sys->i_bitrate : 1 ); /* maxbitrate */

    /* header extension */
    bo_add_guid ( &bo, &asf_object_header_extension_guid );
    bo_addle_u64( &bo, i_header_ext_size );
    bo_add_guid ( &bo, &asf_guid_reserved_1 );
    bo_addle_u16( &bo, 6 );
    bo_addle_u32( &bo, i_header_ext_size - 46 );

    /* extended stream properties */
    for( size_t i = 0; i < vlc_array_count( &p_sys->tracks ); i++ )
    {
        const asf_track_t *p_track = vlc_array_item_at_index( &p_sys->tracks, i );
        const es_format_t *p_fmt = &p_track->fmt;

        if( !p_track->b_extended )
            continue;

        uint64_t i_avg_duration = 0;
        if( p_fmt->i_cat == VIDEO_ES &&
            p_fmt->video.i_frame_rate > 0 && p_fmt->video.i_frame_rate_base > 0 )
            i_avg_duration = ( INT64_C(10000000) * p_fmt->video.i_frame_rate_base +
                               p_fmt->video.i_frame_rate/2 ) / p_fmt->video.i_frame_rate;

        bo_add_guid ( &bo, &asf_object_extended_stream_properties_guid );
        bo_addle_u64( &bo, 88 );
        bo_addle_u64( &bo, 0 );
        bo_addle_u64( &bo, 0 );
        bo_addle_u32( &bo, p_fmt->i_bitrate );  /* Bitrate */
        bo_addle_u32( &bo, p_sys->i_preroll_time ); /* Buffer size */
        bo_addle_u32( &bo, 0 );                 /* Initial buffer fullness */
        bo_addle_u32( &bo, p_fmt->i_bitrate );  /* Alternate Bitrate */
        bo_addle_u32( &bo, 0 );                 /* Alternate Buffer size */
        bo_addle_u32( &bo, 0 );                 /* Alternate Initial buffer fullness */
        bo_addle_u32( &bo, 0 );                 /* Maximum object size (0 = unknown) */
        bo_addle_u32( &bo, 0x02 );              /* Flags (seekable) */
        bo_addle_u16( &bo, p_track->i_id ); /* Stream number */
        bo_addle_u16( &bo, 0 ); /* Stream language index */
        bo_addle_u64( &bo, i_avg_duration );    /* Average time per frame */
        bo_addle_u16( &bo, 0 ); /* Stream name count */
        bo_addle_u16( &bo, 0 ); /* Payload extension system count */
    }

    /* metadata object (part of header extension) */
    if( i_cm_size )
    {
        unsigned int i_dst_num, i_dst_den;

        asf_track_t *tk = NULL;
        for( size_t i = 0; i < vlc_array_count( &p_sys->tracks ); i++ )
        {
            tk = vlc_array_item_at_index( &p_sys->tracks, i );
            if( tk->i_cat == VIDEO_ES &&
                tk->fmt.video.i_sar_num != 0 &&
                tk->fmt.video.i_sar_den != 0 )
            {
                vlc_ureduce( &i_dst_num, &i_dst_den,
                             tk->fmt.video.i_sar_num,
                             tk->fmt.video.i_sar_den, 0 );
                break;
            }
        }
        assert( tk != NULL );

        msg_Dbg( p_mux, "pixel aspect-ratio: %i/%i", i_dst_num, i_dst_den );

        bo_add_guid ( &bo, &asf_object_metadata_guid );
        bo_addle_u64( &bo, i_cm_size );
        bo_addle_u16( &bo, 2 ); /* description records count */
        /* 1st description record */
        bo_addle_u16( &bo, 0 ); /* reserved */
        bo_addle_u16( &bo, tk->i_id ); /* stream number (0 for the whole file) */
        bo_addle_u16( &bo, 2 * sizeof("AspectRatioX") ); /* name length */
        bo_addle_u16( &bo, 0x3 /* DWORD */ ); /* data type */
        bo_addle_u32( &bo, 4 ); /* data length */
        bo_addle_str16_nosize( &bo, "AspectRatioX" );
        bo_addle_u32( &bo, i_dst_num ); /* data */
        /* 2nd description record */
        bo_addle_u16( &bo, 0 ); /* reserved */
        bo_addle_u16( &bo, tk->i_id ); /* stream number (0 for the whole file) */
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
    for( size_t i = 0; i < vlc_array_count( &p_sys->tracks ); i++ )
    {
        asf_track_t *tk = vlc_array_item_at_index( &p_sys->tracks, i);

        bo_add_guid ( &bo, &asf_object_stream_properties_guid );
        bo_addle_u64( &bo, 78 + tk->i_extra + (tk->b_audio_correction ? 8:0) );

        if( tk->i_cat == AUDIO_ES )
        {
            bo_add_guid( &bo, &asf_object_stream_type_audio );
            if( tk->b_audio_correction )
                bo_add_guid( &bo, &asf_guid_audio_conceal_spread );
            else
                bo_add_guid( &bo, &asf_no_error_correction_guid );
        }
        else if( tk->i_cat == VIDEO_ES )
        {
            bo_add_guid( &bo, &asf_object_stream_type_video );
            bo_add_guid( &bo, &asf_no_error_correction_guid );
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
    bo_add_guid ( &bo, &asf_guid_reserved_2 );
    bo_addle_u32( &bo, vlc_array_count( &p_sys->tracks ) );
    for( size_t i = 0; i < vlc_array_count( &p_sys->tracks ); i++ )
    {
        asf_track_t *tk = vlc_array_item_at_index( &p_sys->tracks ,i);

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
    bo_addle_u32( &bo, MS_FROM_VLC_TICK(p_sys->i_pk_dts - p_sys->i_dts_first) +
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
            p_sys->pk = block_Alloc( p_sys->i_packet_size + i_preheader );
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
        bo_addle_u32( &bo, MS_FROM_VLC_TICK(data->i_dts - p_sys->i_dts_first) +
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
        out = block_Alloc( 12 );
        bo_init( &bo, out->p_buffer, 12 );
        asf_chunk_add( &bo, 0x4524, 0, 0x00, p_sys->i_seq++ );
    }
    else
    {
        /* Create index */
        out = block_Alloc( 56 );
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
