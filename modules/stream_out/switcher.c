/*****************************************************************************
 * switcher.c: MPEG2 video switcher module
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
#include <math.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_vout.h>

#include <vlc_charset.h>
#include <vlc_network.h>

#define HAVE_MMX
#ifdef HAVE_LIBAVCODEC_AVCODEC_H
#   include <libavcodec/avcodec.h>
#elif defined(HAVE_FFMPEG_AVCODEC_H)
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#ifdef HAVE_POSTPROC_POSTPROCESS_H
#   include <postproc/postprocess.h>
#else
#   include <libpostproc/postprocess.h>
#endif

#define SOUT_CFG_PREFIX "sout-switcher-"
#define MAX_PICTURES 10
#define MAX_AUDIO 30
#define MAX_THRESHOLD 99999999

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int      Open    ( vlc_object_t * );
static void     Close   ( vlc_object_t * );

static sout_stream_id_t *Add( sout_stream_t *, es_format_t * );
static int Del( sout_stream_t *, sout_stream_id_t * );
static int Send( sout_stream_t *, sout_stream_id_t *, block_t * );

static mtime_t Process( sout_stream_t *p_stream, sout_stream_id_t *id,
                        mtime_t i_max_dts );
static int UnpackFromFile( sout_stream_t *p_stream, const char *psz_file,
                           int i_width, int i_height,
                           picture_t *p_pic );
static void NetCommand( sout_stream_t *p_stream );
static mtime_t VideoCommand( sout_stream_t *p_stream, sout_stream_id_t *id );
static block_t *VideoGetBuffer( sout_stream_t *p_stream, sout_stream_id_t *id,
                                block_t *p_buffer );
static block_t *AudioGetBuffer( sout_stream_t *p_stream, sout_stream_id_t *id,
                                block_t *p_buffer );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FILES_TEXT N_("Files")
#define FILES_LONGTEXT N_( \
    "Full paths of the files separated by colons." )
#define SIZES_TEXT N_("Sizes")
#define SIZES_LONGTEXT N_( \
    "List of sizes separated by colons (720x576:480x576)." )
#define RATIO_TEXT N_("Aspect ratio")
#define RATIO_LONGTEXT N_( \
    "Aspect ratio (4:3, 16:9)." )
#define PORT_TEXT N_("Command UDP port")
#define PORT_LONGTEXT N_( \
    "UDP port to listen to for commands." )
#define COMMAND_TEXT N_("Command")
#define COMMAND_LONGTEXT N_( \
    "Initial command to execute." )
#define GOP_TEXT N_("GOP size")
#define GOP_LONGTEXT N_( \
    "Number of P frames between two I frames." )
#define QSCALE_TEXT N_("Quantizer scale")
#define QSCALE_LONGTEXT N_( \
    "Fixed quantizer scale to use." )
#define AUDIO_TEXT N_("Mute audio")
#define AUDIO_LONGTEXT N_( \
    "Mute audio when command is not 0." )

vlc_module_begin();
    set_description( N_("MPEG2 video switcher stream output") );
    set_capability( "sout stream", 50 );
    add_shortcut( "switcher" );
    set_callbacks( Open, Close );

    add_string( SOUT_CFG_PREFIX "files", "", NULL, FILES_TEXT,
                FILES_LONGTEXT, false );
    add_string( SOUT_CFG_PREFIX "sizes", "", NULL, SIZES_TEXT,
                SIZES_LONGTEXT, false );
    add_string( SOUT_CFG_PREFIX "aspect-ratio", "4:3", NULL, RATIO_TEXT,
                RATIO_LONGTEXT, false );
    add_integer( SOUT_CFG_PREFIX "port", 5001, NULL,
                 PORT_TEXT, PORT_LONGTEXT, true );
    add_integer( SOUT_CFG_PREFIX "command", 0, NULL,
                 COMMAND_TEXT, COMMAND_LONGTEXT, true );
    add_integer( SOUT_CFG_PREFIX "gop", 8, NULL,
                 GOP_TEXT, GOP_LONGTEXT, true );
    add_integer( SOUT_CFG_PREFIX "qscale", 5, NULL,
                 QSCALE_TEXT, QSCALE_LONGTEXT, true );
    add_bool( SOUT_CFG_PREFIX "mute-audio", 1, NULL,
              AUDIO_TEXT, AUDIO_LONGTEXT, true );
vlc_module_end();

static const char *const ppsz_sout_options[] = {
    "files", "sizes", "aspect-ratio", "port", "command", "gop", "qscale",
    "mute-audio", NULL
};

struct sout_stream_sys_t
{
    sout_stream_t   *p_out;
    int             i_gop;
    int             i_qscale;
    int             i_aspect;
    sout_stream_id_t *pp_audio_ids[MAX_AUDIO];
    bool      b_audio;

    /* Pictures */
    picture_t       p_pictures[MAX_PICTURES];
    int             i_nb_pictures;

    /* Command */
    int             i_fd;
    int             i_cmd, i_old_cmd;
};

struct sout_stream_id_t
{
    void            *id;
    bool      b_switcher_video;
    bool      b_switcher_audio;
    es_format_t     f_src;
    block_t         *p_queued;

    /* ffmpeg part */
    AVCodec         *ff_enc;
    AVCodecContext  *ff_enc_c;
    AVFrame         *p_frame;
    uint8_t         *p_buffer_out;
    int             i_nb_pred;
    int16_t         *p_samples;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;
    vlc_value_t       val;
    char              *psz_files, *psz_sizes;
    int               i_height = 0, i_width = 0;

    p_sys = malloc( sizeof(sout_stream_sys_t) );
    memset( p_sys, 0, sizeof(sout_stream_sys_t) );

    p_sys->p_out = sout_StreamNew( p_stream->p_sout, p_stream->psz_next );
    if( !p_sys->p_out )
    {
        msg_Err( p_stream, "cannot create chain" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    config_ChainParse( p_stream, SOUT_CFG_PREFIX, ppsz_sout_options,
                   p_stream->p_cfg );

    var_Get( p_stream, SOUT_CFG_PREFIX "files", &val );
    psz_files = val.psz_string;
    var_Get( p_stream, SOUT_CFG_PREFIX "sizes", &val );
    psz_sizes = val.psz_string;

    p_sys->i_nb_pictures = 0;
    while ( psz_files && *psz_files )
    {
        char * psz_file = psz_files;
        char * psz_size = psz_sizes;

        while ( *psz_files && *psz_files != ':' )
            psz_files++;
        if ( *psz_files == ':' )
           *psz_files++ = '\0';

        if ( *psz_sizes )
        {
            while ( *psz_sizes && *psz_sizes != ':' )
                psz_sizes++;
            if ( *psz_sizes == ':' )
                *psz_sizes++ = '\0';
            if ( sscanf( psz_size, "%dx%d", &i_width, &i_height ) != 2 )
            {
                msg_Err( p_stream, "bad size %s for file %s", psz_size,
                         psz_file );
                free( p_sys );
                return VLC_EGENERIC;
            }
        }

        if ( UnpackFromFile( p_stream, psz_file, i_width, i_height,
                             &p_sys->p_pictures[p_sys->i_nb_pictures] ) < 0 )
        {
            free( p_sys );
            return VLC_EGENERIC;
        }
        p_sys->i_nb_pictures++;
    }

    var_Get( p_stream, SOUT_CFG_PREFIX "aspect-ratio", &val );
    if ( val.psz_string )
    {
        char *psz_parser = strchr( val.psz_string, ':' );

        if( psz_parser )
        {
            *psz_parser++ = '\0';
            p_sys->i_aspect = atoi( val.psz_string ) * VOUT_ASPECT_FACTOR
                / atoi( psz_parser );
        }
        else
        {
            msg_Warn( p_stream, "bad aspect ratio %s", val.psz_string );
            p_sys->i_aspect = 4 * VOUT_ASPECT_FACTOR / 3;
        }

        free( val.psz_string );
    }
    else
    {
        p_sys->i_aspect = 4 * VOUT_ASPECT_FACTOR / 3;
    }

    var_Get( p_stream, SOUT_CFG_PREFIX "port", &val );
    p_sys->i_fd = net_ListenUDP1( p_stream, NULL, val.i_int );
    if ( p_sys->i_fd < 0 )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    var_Get( p_stream, SOUT_CFG_PREFIX "command", &val );
    p_sys->i_cmd = val.i_int;
    p_sys->i_old_cmd = 0;

    var_Get( p_stream, SOUT_CFG_PREFIX "gop", &val );
    p_sys->i_gop = val.i_int;

    var_Get( p_stream, SOUT_CFG_PREFIX "qscale", &val );
    p_sys->i_qscale = val.i_int;

    var_Get( p_stream, SOUT_CFG_PREFIX "mute-audio", &val );
    p_sys->b_audio = val.b_bool;

    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;
    p_stream->p_sys     = p_sys;

    avcodec_init();
    avcodec_register_all();

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t       *p_stream = (sout_stream_t *)p_this;
    sout_stream_sys_t   *p_sys = p_stream->p_sys;

    sout_StreamDelete( p_sys->p_out );

    free( p_sys );
}

/*****************************************************************************
 * Add: Add an input elementary stream
 *****************************************************************************/
static sout_stream_id_t *Add( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_stream_sys_t   *p_sys = p_stream->p_sys;
    sout_stream_id_t    *id;

    id = malloc( sizeof( sout_stream_id_t ) );
    memset( id, 0, sizeof( sout_stream_id_t ) );
    id->id = NULL;

    if ( p_fmt->i_cat == VIDEO_ES
            && (p_fmt->i_codec == VLC_FOURCC('m', 'p', 'g', 'v')
                 || p_fmt->i_codec == VLC_FOURCC('f', 'a', 'k', 'e')) )
    {
        id->b_switcher_video = true;
        p_fmt->i_codec = VLC_FOURCC('m', 'p', 'g', 'v');
        msg_Dbg( p_stream,
                 "creating video switcher for fcc=`%4.4s' cmd:%d",
                 (char*)&p_fmt->i_codec, p_sys->i_cmd );
    }
    else if ( p_fmt->i_cat == AUDIO_ES
               && p_fmt->i_codec == VLC_FOURCC('m', 'p', 'g', 'a')
               && p_sys->b_audio )
    {
        int i_ff_codec = CODEC_ID_MP2;
        int i;

        id->b_switcher_audio = true;
        msg_Dbg( p_stream,
                 "creating audio switcher for fcc=`%4.4s' cmd:%d",
                 (char*)&p_fmt->i_codec, p_sys->i_cmd );

        /* Allocate the encoder right now. */
        if( i_ff_codec == 0 )
        {
            msg_Err( p_stream, "cannot find encoder" );
            return NULL;
        }

        id->ff_enc = avcodec_find_encoder( i_ff_codec );
        if( !id->ff_enc )
        {
            msg_Err( p_stream, "cannot find encoder (avcodec)" );
            return NULL;
        }

        id->ff_enc_c = avcodec_alloc_context();

        /* Set CPU capabilities */
        unsigned i_cpu = vlc_CPU();
        id->ff_enc_c->dsp_mask = 0;
        if( !(i_cpu & CPU_CAPABILITY_MMX) )
        {
            id->ff_enc_c->dsp_mask |= FF_MM_MMX;
        }
        if( !(i_cpu & CPU_CAPABILITY_MMXEXT) )
        {
            id->ff_enc_c->dsp_mask |= FF_MM_MMXEXT;
        }
        if( !(i_cpu & CPU_CAPABILITY_3DNOW) )
        {
            id->ff_enc_c->dsp_mask |= FF_MM_3DNOW;
        }
        if( !(i_cpu & CPU_CAPABILITY_SSE) )
        {
            id->ff_enc_c->dsp_mask |= FF_MM_SSE;
            id->ff_enc_c->dsp_mask |= FF_MM_SSE2;
        }

        id->ff_enc_c->sample_rate = p_fmt->audio.i_rate;
        id->ff_enc_c->channels    = p_fmt->audio.i_channels;
        id->ff_enc_c->bit_rate    = p_fmt->i_bitrate;

        if( avcodec_open( id->ff_enc_c, id->ff_enc ) )
        {
            msg_Err( p_stream, "cannot open encoder" );
            return NULL;
        }

        id->p_buffer_out = malloc( AVCODEC_MAX_AUDIO_FRAME_SIZE * 2 );
        id->p_samples = malloc( id->ff_enc_c->frame_size
                                 * p_fmt->audio.i_channels * sizeof(int16_t) );
        memset( id->p_samples, 0,
                id->ff_enc_c->frame_size * p_fmt->audio.i_channels
                 * sizeof(int16_t) );

        for ( i = 0; i < MAX_AUDIO; i++ )
        {
            if ( p_sys->pp_audio_ids[i] == NULL )
            {
                p_sys->pp_audio_ids[i] = id;
                break;
            }
        }
        if ( i == MAX_AUDIO )
        {
            msg_Err( p_stream, "too many audio streams!" );
            free( id );
            return NULL;
        }
    }
    else
    {
        msg_Dbg( p_stream, "do not know what to do when switching (fcc=`%4.4s')",
                 (char*)&p_fmt->i_codec );
    }

    /* src format */
    memcpy( &id->f_src, p_fmt, sizeof( es_format_t ) );

    /* open output stream */
    id->id = p_sys->p_out->pf_add( p_sys->p_out, p_fmt );

    if ( id->id == NULL )
    {
        free( id );
        return NULL;
    }

    return id;
}

/*****************************************************************************
 * Del: Del an elementary stream
 *****************************************************************************/
static int Del( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t   *p_sys = p_stream->p_sys;

    if ( id->b_switcher_audio )
    {
        int i;
        for ( i = 0; i < MAX_AUDIO; i++ )
        {
            if ( p_sys->pp_audio_ids[i] == id )
            {
                p_sys->pp_audio_ids[i] = NULL;
                break;
            }
        }
    }

    if ( id->ff_enc )
    {
        avcodec_close( id->ff_enc_c );
        av_free( id->ff_enc_c );
        av_free( id->p_frame );
        free( id->p_buffer_out );
    }

    if ( id->id )
    {
        p_sys->p_out->pf_del( p_sys->p_out, id->id );
    }
    free( id );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Send: Process an input packet
 *****************************************************************************/
static int Send( sout_stream_t *p_stream, sout_stream_id_t *id,
                 block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if ( !id->id )
    {
        block_Release( p_buffer );
        return VLC_EGENERIC;
    }

    if ( !id->b_switcher_video && !id->b_switcher_audio )
    {
        return p_sys->p_out->pf_send( p_sys->p_out, id->id, p_buffer );
    }

    block_ChainAppend( &id->p_queued, p_buffer );

    if ( id->b_switcher_video )
    {
        /* Check for commands for every video frame. */
        NetCommand( p_stream );

        while ( id->p_queued != NULL )
        {
            mtime_t i_dts = 0;
            int i;

            if ( p_sys->i_old_cmd != p_sys->i_cmd )
            {
                i_dts = VideoCommand( p_stream, id );
            }

            i_dts = Process( p_stream, id, i_dts );

            for ( i = 0; i < MAX_AUDIO; i++ )
            {
                if ( p_sys->pp_audio_ids[i] != NULL )
                    Process( p_stream, p_sys->pp_audio_ids[i], i_dts );
            }
        }
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Process: Process and dispatch buffers
 *****************************************************************************/
static mtime_t Process( sout_stream_t *p_stream, sout_stream_id_t *id,
                        mtime_t i_max_dts )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    mtime_t i_dts = 0;
    block_t *p_blocks = NULL;
    block_t *p_blocks_out = NULL;

    /* Find out the blocks we need. */
    while ( id->p_queued != NULL
             && (!i_max_dts || id->p_queued->i_dts <= i_max_dts) )
    {
        block_t *p_next = id->p_queued->p_next;
        id->p_queued->p_next = NULL;
        i_dts = id->p_queued->i_dts;
        block_ChainAppend( &p_blocks, id->p_queued );
        id->p_queued = p_next;
    }

    if ( p_sys->i_old_cmd == 0 )
    {
        /* Full forward */
        if ( p_blocks != NULL )
            p_sys->p_out->pf_send( p_sys->p_out, id->id, p_blocks );
        return i_dts;
    }

    if ( p_sys->i_old_cmd == -1 )
    {
        /* No output at all */
        while ( p_blocks != NULL )
        {
            block_t * p_next = p_blocks->p_next;
            block_Release( p_blocks );
            p_blocks = p_next;
        }
        return i_dts;
    }

    while ( p_blocks != NULL )
    {
        block_t * p_next = p_blocks->p_next;
        block_t * p_out;

        if ( id->b_switcher_video )
        {
            p_out = VideoGetBuffer( p_stream, id, p_blocks );
        }
        else
        {
            p_out = AudioGetBuffer( p_stream, id, p_blocks );
        }
        p_blocks = p_next;
        if ( p_out != NULL )
        {
            block_ChainAppend( &p_blocks_out, p_out );
        }
    }

    if ( p_blocks_out != NULL )
        p_sys->p_out->pf_send( p_sys->p_out, id->id, p_blocks_out );
    return i_dts;
}

/*****************************************************************************
 * UnpackFromFile: Read a YUV picture and store it in our format
 *****************************************************************************/
static int UnpackFromFile( sout_stream_t *p_stream, const char *psz_file,
                           int i_width, int i_height,
                           picture_t *p_pic )
{
    int i, j;
    FILE *p_file = utf8_fopen( psz_file, "r" );

    if ( p_file == NULL )
    {
        msg_Err( p_stream, "file %s not found", psz_file );
        return -1;
    }

    vout_InitPicture( VLC_OBJECT(p_stream), p_pic, VLC_FOURCC('I','4','2','0'),
                      i_width, i_height,
                      i_width * VOUT_ASPECT_FACTOR / i_height );
    for ( i = 0; i < p_pic->i_planes; i++ )
    {
        p_pic->p[i].p_pixels = malloc( p_pic->p[i].i_lines *
                                           p_pic->p[i].i_pitch );
        memset( p_pic->p[i].p_pixels, 0, p_pic->p[i].i_lines *
                    p_pic->p[i].i_pitch );
    }

    for ( i = 0; i < i_height; i++ )
    {
        int i_chroma;
        uint8_t p_buffer[i_width * 2];
        uint8_t *p_char = p_buffer;
        uint8_t *p_y = &p_pic->p[0].p_pixels[i * p_pic->p[0].i_pitch];
        uint8_t *p_u = &p_pic->p[1].p_pixels[i/2 * p_pic->p[1].i_pitch];
        uint8_t *p_v = &p_pic->p[2].p_pixels[i/2 * p_pic->p[2].i_pitch];

        if ( fread( p_buffer, 2, i_width, p_file ) != (size_t)i_width )
        {
            msg_Err( p_stream, "premature end of file %s", psz_file );
            fclose( p_file );
            for ( i = 0; i < p_pic->i_planes; i++ )
            {
                free( p_pic->p[i].p_pixels );
            }
            return -1;
        }

        i_chroma = 0;
        for ( j = 0; j < i_width; j++ )
        {
            uint8_t **pp_chroma = i_chroma ? &p_v : &p_u;
            i_chroma = !i_chroma;
            if ( i & 1 )
                **pp_chroma = (**pp_chroma + *p_char + 1) / 2;
            else
                **pp_chroma = *p_char;
            (*pp_chroma)++;
            p_char++;
            *p_y++ = *p_char++;
        }
    }

    fclose( p_file );
    return 0;
}

/*****************************************************************************
 * NetCommand: Get a command from the network
 *****************************************************************************/
static void NetCommand( sout_stream_t *p_stream )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    char psz_buffer[11];
    int i_len = recv( p_sys->i_fd, psz_buffer, sizeof( psz_buffer ) - 1, 0 );

    if ( i_len > 0 )
    {
        psz_buffer[i_len] = '\0';
        int i_cmd = strtol( psz_buffer, NULL, 0 );
        if ( i_cmd < -1 || i_cmd > p_sys->i_nb_pictures )
        {
            msg_Err( p_stream, "got a wrong command (%d)", i_cmd );
            return;
        }

        p_sys->i_cmd = i_cmd;

        msg_Dbg( p_stream, "new command: %d old:%d", p_sys->i_cmd,
                 p_sys->i_old_cmd );
    }
}

/*****************************************************************************
 * VideoCommand: Create/Delete a video encoder
 *****************************************************************************/
static mtime_t VideoCommand( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if ( p_sys->i_cmd == 0 && !(id->p_queued->i_flags & BLOCK_FLAG_TYPE_I) )
    {
        mtime_t i_dts = id->p_queued->i_dts;
        block_t *p_block = id->p_queued->p_next;

        while ( p_block != NULL )
        {
            if ( p_block->i_flags & BLOCK_FLAG_TYPE_I )
                return i_dts;
            i_dts = p_block->i_dts;
            p_block = p_block->p_next;
        }

        return 0;
    }

    p_sys->i_old_cmd = p_sys->i_cmd;

    if ( id->ff_enc )
    {
        avcodec_close( id->ff_enc_c );
        av_free( id->ff_enc_c );
        av_free( id->p_frame );
        free( id->p_buffer_out );
        id->ff_enc = NULL;
    }

    if ( p_sys->i_cmd > 0 )
    {
        /* Create a new encoder. */
        int i_ff_codec = CODEC_ID_MPEG2VIDEO;
        int i_aspect_num, i_aspect_den;

        if( i_ff_codec == 0 )
        {
            msg_Err( p_stream, "cannot find encoder" );
            return 0;
        }

        id->ff_enc = avcodec_find_encoder( i_ff_codec );
        if( !id->ff_enc )
        {
            msg_Err( p_stream, "cannot find encoder (avcodec)" );
            return 0;
        }

        id->ff_enc_c = avcodec_alloc_context();

        /* Set CPU capabilities */
        unsigned i_cpu = vlc_CPU();
        id->ff_enc_c->dsp_mask = 0;
        if( !(i_cpu & CPU_CAPABILITY_MMX) )
        {
            id->ff_enc_c->dsp_mask |= FF_MM_MMX;
        }
        if( !(i_cpu & CPU_CAPABILITY_MMXEXT) )
        {
            id->ff_enc_c->dsp_mask |= FF_MM_MMXEXT;
        }
        if( !(i_cpu & CPU_CAPABILITY_3DNOW) )
        {
            id->ff_enc_c->dsp_mask |= FF_MM_3DNOW;
        }
        if( !(i_cpu & CPU_CAPABILITY_SSE) )
        {
            id->ff_enc_c->dsp_mask |= FF_MM_SSE;
            id->ff_enc_c->dsp_mask |= FF_MM_SSE2;
        }

        id->ff_enc_c->width = p_sys->p_pictures[p_sys->i_cmd-1].format.i_width;
        id->ff_enc_c->height = p_sys->p_pictures[p_sys->i_cmd-1].format.i_height;
        av_reduce( &i_aspect_num, &i_aspect_den,
                   p_sys->i_aspect,
                   VOUT_ASPECT_FACTOR, 1 << 30 /* something big */ );
        av_reduce( &id->ff_enc_c->sample_aspect_ratio.num,
                   &id->ff_enc_c->sample_aspect_ratio.den,
                   i_aspect_num * (int64_t)id->ff_enc_c->height,
                   i_aspect_den * (int64_t)id->ff_enc_c->width, 1 << 30 );

#if LIBAVCODEC_BUILD >= 4754
        id->ff_enc_c->time_base.num = 1;
        id->ff_enc_c->time_base.den = 25; /* FIXME */
#else
        id->ff_enc_c->frame_rate    = 25; /* FIXME */
        id->ff_enc_c->frame_rate_base = 1;
#endif

        id->ff_enc_c->gop_size = 200;
        id->ff_enc_c->max_b_frames = 0;

        id->ff_enc_c->flags |= CODEC_FLAG_QSCALE
                            | CODEC_FLAG_INPUT_PRESERVED
                            | CODEC_FLAG_LOW_DELAY;

        id->ff_enc_c->mb_decision = FF_MB_DECISION_SIMPLE;
        id->ff_enc_c->pix_fmt = PIX_FMT_YUV420P;

        if( avcodec_open( id->ff_enc_c, id->ff_enc ) )
        {
            msg_Err( p_stream, "cannot open encoder" );
            return 0;
        }

        id->p_buffer_out = malloc( id->ff_enc_c->width * id->ff_enc_c->height * 3 );
        id->p_frame = avcodec_alloc_frame();
        id->p_frame->linesize[0] = p_sys->p_pictures[p_sys->i_cmd-1].p[0].i_pitch;
        id->p_frame->linesize[1] = p_sys->p_pictures[p_sys->i_cmd-1].p[1].i_pitch;
        id->p_frame->linesize[2] = p_sys->p_pictures[p_sys->i_cmd-1].p[2].i_pitch;
        id->p_frame->data[0] = p_sys->p_pictures[p_sys->i_cmd-1].p[0].p_pixels;
        id->p_frame->data[1] = p_sys->p_pictures[p_sys->i_cmd-1].p[1].p_pixels;
        id->p_frame->data[2] = p_sys->p_pictures[p_sys->i_cmd-1].p[2].p_pixels;

        id->i_nb_pred = p_sys->i_gop;
    }

    return 0;
}

/*****************************************************************************
 * VideoGetBuffer: Build an alternate video buffer
 *****************************************************************************/
static block_t *VideoGetBuffer( sout_stream_t *p_stream, sout_stream_id_t *id,
                                block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    int i_out;
    block_t *p_out;

    id->p_frame->quality = p_sys->i_qscale * powf(2.0, FF_LAMBDA_SHIFT + 7.0)
                            / 139.0;
    id->p_frame->interlaced_frame = 0;
    id->p_frame->top_field_first = 1;
    id->p_frame->pts = p_buffer->i_dts;

    if ( id->i_nb_pred >= p_sys->i_gop )
    {
        id->p_frame->pict_type = FF_I_TYPE;
#if 0
        id->p_frame->me_threshold = 0;
        id->p_frame->mb_threshold = 0;
#endif
        id->i_nb_pred = 0;
    }
    else
    {
        id->p_frame->pict_type = FF_P_TYPE;
#if 0
        if ( id->p_frame->mb_type != NULL )
        {
            id->p_frame->me_threshold = MAX_THRESHOLD;
            id->p_frame->mb_threshold = MAX_THRESHOLD;
        }
#endif
        id->i_nb_pred++;
    }

    i_out = avcodec_encode_video( id->ff_enc_c, id->p_buffer_out,
                                  id->ff_enc_c->width * id->ff_enc_c->height * 3,
                                  id->p_frame );

    if ( i_out <= 0 )
        return NULL;

#if 0
    if ( id->p_frame->mb_type == NULL
          && id->ff_enc_c->coded_frame->pict_type != FF_I_TYPE )
    {
        int mb_width = (id->ff_enc_c->width + 15) / 16;
        int mb_height = (id->ff_enc_c->height + 15) / 16;
        int h_chroma_shift, v_chroma_shift;
        int i;

        avcodec_get_chroma_sub_sample( id->ff_enc_c->pix_fmt, &h_chroma_shift,
                                       &v_chroma_shift );

        id->p_frame->motion_subsample_log2
            = id->ff_enc_c->coded_frame->motion_subsample_log2;
        id->p_frame->mb_type = malloc( ((mb_width + 1) * (mb_height + 1) + 1)
                                    * sizeof(uint32_t) );
        vlc_memcpy( id->p_frame->mb_type, id->ff_enc_c->coded_frame->mb_type,
                    (mb_width + 1) * mb_height * sizeof(id->p_frame->mb_type[0]));

        for ( i = 0; i < 2; i++ )
        {
            int stride = ((16 * mb_width )
                    >> id->ff_enc_c->coded_frame->motion_subsample_log2) + 1;
            int height = ((16 * mb_height)
                    >> id->ff_enc_c->coded_frame->motion_subsample_log2);
            int b8_stride = mb_width * 2 + 1;

            if ( id->ff_enc_c->coded_frame->motion_val[i] )
            {
                id->p_frame->motion_val[i] = malloc( 2 * stride * height
                                                * sizeof(int16_t) );
                vlc_memcpy( id->p_frame->motion_val[i],
                            id->ff_enc_c->coded_frame->motion_val[i],
                            2 * stride * height * sizeof(int16_t) );
            }
            if ( id->ff_enc_c->coded_frame->ref_index[i] )
            {
                id->p_frame->ref_index[i] = malloc( b8_stride * 2 * mb_height
                                               * sizeof(int8_t) );
                vlc_memcpy( id->p_frame->ref_index[i],
                            id->ff_enc_c->coded_frame->ref_index[i],
                            b8_stride * 2 * mb_height * sizeof(int8_t));
            }
        }
    }
#endif

    p_out = block_New( p_stream, i_out );
    vlc_memcpy( p_out->p_buffer, id->p_buffer_out, i_out );
    p_out->i_length = p_buffer->i_length;
    p_out->i_pts = p_buffer->i_dts;
    p_out->i_dts = p_buffer->i_dts;
    p_out->i_rate = p_buffer->i_rate;

    switch ( id->ff_enc_c->coded_frame->pict_type )
    {
    case FF_I_TYPE:
        p_out->i_flags |= BLOCK_FLAG_TYPE_I;
        break;
    case FF_P_TYPE:
        p_out->i_flags |= BLOCK_FLAG_TYPE_P;
        break;
    case FF_B_TYPE:
        p_out->i_flags |= BLOCK_FLAG_TYPE_B;
        break;
    default:
        break;
    }

    block_Release( p_buffer );

    return p_out;
}

/*****************************************************************************
 * AudioGetBuffer: Build an alternate audio buffer
 *****************************************************************************/
static block_t *AudioGetBuffer( sout_stream_t *p_stream, sout_stream_id_t *id,
                                block_t *p_buffer )
{
    int i_out;
    block_t *p_out;

    i_out = avcodec_encode_audio( id->ff_enc_c, id->p_buffer_out,
                                  2 * AVCODEC_MAX_AUDIO_FRAME_SIZE,
                                  id->p_samples );

    if ( i_out <= 0 )
        return NULL;

    p_out = block_New( p_stream, i_out );
    vlc_memcpy( p_out->p_buffer, id->p_buffer_out, i_out );
    p_out->i_length = p_buffer->i_length;
    p_out->i_pts = p_buffer->i_dts;
    p_out->i_dts = p_buffer->i_dts;
    p_out->i_rate = p_buffer->i_rate;

    block_Release( p_buffer );

    return p_out;
}
