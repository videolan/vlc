/*****************************************************************************
 * rawvid.c : raw video input module for vlc
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Antoine Cellerier <dionoea at videolan d.t org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_vout.h>                                     /* vout_InitFormat */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define FPS_TEXT N_("Frames per Second")
#define FPS_LONGTEXT N_("This is the desired frame rate when " \
    "playing raw video streams.")

#define WIDTH_TEXT N_("Width")
#define WIDTH_LONGTEXT N_("This specifies the width in pixels of the raw " \
    "video stream.")

#define HEIGHT_TEXT N_("Height")
#define HEIGHT_LONGTEXT N_("This specifies the height in pixels of the raw " \
    "video stream.")

#define CHROMA_TEXT N_("Force chroma (Use carefully)")
#define CHROMA_LONGTEXT N_("Force chroma. This is a four character string.")

#define ASPECT_RATIO_TEXT N_("Aspect ratio")
#define ASPECT_RATIO_LONGTEXT N_( \
    "Aspect ratio (4:3, 16:9). Default is square pixels." )

vlc_module_begin();
    set_shortname( "Raw Video" );
    set_description( N_("Raw video demuxer") );
    set_capability( "demux", 10 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_callbacks( Open, Close );
    add_shortcut( "rawvideo" );
    add_float( "rawvid-fps", 0, 0, FPS_TEXT, FPS_LONGTEXT, false );
    add_integer( "rawvid-width", 0, 0, WIDTH_TEXT, WIDTH_LONGTEXT, 0 );
    add_integer( "rawvid-height", 0, 0, HEIGHT_TEXT, HEIGHT_LONGTEXT, 0 );
    add_string( "rawvid-chroma", NULL, NULL, CHROMA_TEXT, CHROMA_LONGTEXT,
                true );
    add_string( "rawvid-aspect-ratio", NULL, NULL,
                ASPECT_RATIO_TEXT, ASPECT_RATIO_LONGTEXT, true );
vlc_module_end();

/*****************************************************************************
 * Definitions of structures used by this plugin
 *****************************************************************************/
struct demux_sys_t
{
    int    frame_size;
    float  f_fps;

    es_out_id_t *p_es_video;
    es_format_t  fmt_video;

    mtime_t i_pcr;

    bool b_y4m;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t * );
static int Control( demux_t *, int i_query, va_list args );

struct preset_t
{
    const char *psz_ext;
    int i_width;
    int i_height;
    double f_fps;
    const char *psz_aspect_ratio;
    const char *psz_chroma;
};

static const struct preset_t p_presets[] =
{
    { "sqcif", 128, 96, 29.97, "4:3", "YV12" },
    { "qcif", 176, 144, 29.97, "4:3", "YV12" },
    { "cif", 352, 288, 29.97, "4:3", "YV12" },
    { "4cif", 704, 576, 29.97, "4:3", "YV12" },
    { "16cif", 1408, 1152, 29.97, "4:3", "YV12" },
    { "yuv", 176, 144, 25, "4:3", "YV12" },
    { "", 0, 0, 0., "", "" }
};

/*****************************************************************************
 * Open: initializes raw DV demux structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    int i_width, i_height;
    char *psz_ext;
    char *psz_chroma;
    uint32_t i_chroma;
    char *psz_aspect_ratio;
    unsigned int i_aspect = 0;
    const struct preset_t *p_preset = NULL;
    const uint8_t *p_peek;
    bool b_valid = false;
    bool b_y4m = false;

    if( stream_Peek( p_demux->s, &p_peek, 9 ) == 9 )
    {
        /* http://wiki.multimedia.cx/index.php?title=YUV4MPEG2 */
        if( !strncmp( (char *)p_peek, "YUV4MPEG2", 9 ) )
        {
            b_valid = true;
            b_y4m = true;
        }
    }

    psz_ext = strrchr( p_demux->psz_path, '.' );
    if( psz_ext )
    {
        psz_ext++;
        for( p_preset = p_presets; *p_preset->psz_ext; p_preset++ )
            if( !strcasecmp( psz_ext, p_preset->psz_ext ) )
            {
                b_valid = true;
                break;
            }
    }
    if( !b_valid && !p_demux->b_force )
        return VLC_EGENERIC;

    /* Set p_input field */
    p_demux->pf_demux   = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys      = p_sys = malloc( sizeof( demux_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->i_pcr = 1;

    p_sys->b_y4m = b_y4m;
    p_sys->f_fps = var_CreateGetFloat( p_demux, "rawvid-fps" );
    i_width = var_CreateGetInteger( p_demux, "rawvid-width" );
    i_height = var_CreateGetInteger( p_demux, "rawvid-height" );
    psz_chroma = var_CreateGetString( p_demux, "rawvid-chroma" );
    psz_aspect_ratio = var_CreateGetString( p_demux, "rawvid-aspect-ratio" );

    if( b_y4m )
    {
        char *psz;
        char *buf;
        int a = 1;
        int b = 1;
        psz = stream_ReadLine( p_demux->s );

        /* TODO: handle interlacing */

#define READ_FRAC( key, num, den ) \
        buf = strstr( psz+9, key );\
        if( buf )\
        {\
            char *end = strchr( buf+1, ' ' );\
            char *sep;\
            if( end ) *end = '\0';\
            sep = strchr( buf+1, ':' );\
            if( sep )\
            {\
                *sep = '\0';\
                den = atoi( sep+1 );\
            }\
            else\
            {\
                den = 1;\
            }\
            num = atoi( buf+2 );\
            if( sep ) *sep = ':';\
            if( end ) *end = ' ';\
        }
        READ_FRAC( " W", i_width, a )
        READ_FRAC( " H", i_height, a )
        READ_FRAC( " F", a, b )
        p_sys->f_fps = (double)a/(double)b;
        READ_FRAC( " A", a, b )
        if( b != 0 ) i_aspect = a * VOUT_ASPECT_FACTOR / b;

        buf = strstr( psz+9, " C" );
        if( buf )
        {
            char *end = strchr( buf+1, ' ' );
            if( end ) *end = '\0';
            buf+=2;
            if( !strncmp( buf, "420jpeg", 7 ) )
            {
                psz_chroma = strdup( "I420" );
            }
            else if( !strncmp( buf, "420paldv", 8 ) )
            {
                psz_chroma = strdup( "I420" );
            }
            else if( !strncmp( buf, "420", 3 ) )
            {
                psz_chroma = strdup( "I420" );
            }
            else if( !strncmp( buf, "422", 3 ) )
            {
                psz_chroma = strdup( "I422" );
            }
            else if( !strncmp( buf, "444", 3 ) )
            {
                psz_chroma = strdup( "I444" );
            }
            else if( !strncmp( buf, "mono", 4 ) )
            {
                psz_chroma = strdup( "GREY" );
            }
            else
            {
                msg_Warn( p_demux, "Unknown YUV4MPEG2 chroma type \"%s\"",
                          buf );
            }
            if( end ) *end = ' ';
        }

        free( psz );
    }

    if( p_preset && *p_preset->psz_ext )
    {
        if( !i_width ) i_width = p_preset->i_width;
        if( !i_height ) i_height = p_preset->i_height;
        if( !p_sys->f_fps ) p_sys->f_fps = p_preset->f_fps;
        if( !*psz_aspect_ratio )
        {
            free( psz_aspect_ratio );
            psz_aspect_ratio = strdup( psz_aspect_ratio );
        }
        if( !*psz_chroma )
        {
            free( psz_chroma );
            psz_chroma = strdup( psz_chroma );
        }
    }

    if( i_width <= 0 || i_height <= 0 )
    {
        msg_Err( p_demux, "width and height must be strictly positive." );
        free( psz_aspect_ratio );
        free( psz_chroma );
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( !i_aspect )
    {
        if( psz_aspect_ratio && *psz_aspect_ratio )
        {
            char *psz_parser = strchr( psz_aspect_ratio, ':' );
            if( psz_parser )
            {
                *psz_parser++ = '\0';
                i_aspect = atoi( psz_aspect_ratio ) * VOUT_ASPECT_FACTOR
                           / atoi( psz_parser );
            }
            else
            {
                i_aspect = atof( psz_aspect_ratio ) * VOUT_ASPECT_FACTOR;
            }
        }
        else
        {
            i_aspect = i_width * VOUT_ASPECT_FACTOR / i_height;
        }
    }
    free( psz_aspect_ratio );

    if( psz_chroma && strlen( psz_chroma ) >= 4 )
    {
        memcpy( &i_chroma, psz_chroma, 4 );
        msg_Dbg( p_demux, "Forcing chroma to 0x%.8x (%4.4s)", i_chroma,
                 (char*)&i_chroma );
    }
    else
    {
        i_chroma = VLC_FOURCC('Y','V','1','2');
        msg_Dbg( p_demux, "Using default chroma 0x%.8x (%4.4s)", i_chroma,
                 (char*)&i_chroma );
    }
    free( psz_chroma );

    es_format_Init( &p_sys->fmt_video, VIDEO_ES, i_chroma );
    vout_InitFormat( &p_sys->fmt_video.video, i_chroma, i_width, i_height,
                     i_aspect );
    if( !p_sys->fmt_video.video.i_bits_per_pixel )
    {
        msg_Err( p_demux, "Unsupported chroma 0x%.8x (%4.4s)", i_chroma,
                 (char*)&i_chroma );
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_sys->frame_size = i_width * i_height
                        * p_sys->fmt_video.video.i_bits_per_pixel / 8;
    p_sys->p_es_video = es_out_Add( p_demux->out, &p_sys->fmt_video );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys  = p_demux->p_sys;
    free( p_sys );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys  = p_demux->p_sys;
    block_t     *p_block;

    /* Call the pace control */
    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->i_pcr );

    if( p_sys->b_y4m )
    {
        /* Skip the frame header */
        unsigned char psz_buf[10];
        psz_buf[9] = '\0';
        stream_Read( p_demux->s, psz_buf, strlen( "FRAME" ) );
        while( psz_buf[0] != 0x0a )
        {
            if( stream_Read( p_demux->s, psz_buf, 1 ) < 1 )
                return 0;
        }
    }

    if( ( p_block = stream_Block( p_demux->s, p_sys->frame_size ) ) == NULL )
    {
        /* EOF */
        return 0;
    }

    p_block->i_dts = p_block->i_pts = p_sys->i_pcr;
    es_out_Send( p_demux->out, p_sys->p_es_video, p_block );

    p_sys->i_pcr += ( INT64_C(1000000) / p_sys->f_fps );

    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys  = p_demux->p_sys;

    /* XXX: DEMUX_SET_TIME is precise here */
    return demux_vaControlHelper( p_demux->s, 0, -1,
                                   p_sys->frame_size * p_sys->f_fps * 8,
                                   p_sys->frame_size, i_query, args );
}
