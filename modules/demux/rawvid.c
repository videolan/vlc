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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
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
    set_description( _("Raw video demuxer") );
    set_capability( "demux2", 2 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_callbacks( Open, Close );
    add_shortcut( "rawvideo" );
    add_float( "rawvid-fps", 25, 0, FPS_TEXT, FPS_LONGTEXT, VLC_FALSE );
    add_integer( "rawvid-width", 176, 0, WIDTH_TEXT, WIDTH_LONGTEXT, 0 );
    add_integer( "rawvid-height", 144, 0, HEIGHT_TEXT, HEIGHT_LONGTEXT, 0 );
    add_string( "rawvid-chroma", NULL, NULL, CHROMA_TEXT, CHROMA_LONGTEXT,
                VLC_TRUE );
    add_string( "rawvid-aspect-ratio", NULL, NULL,
                ASPECT_RATIO_TEXT, ASPECT_RATIO_LONGTEXT, VLC_TRUE );
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
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t * );
static int Control( demux_t *, int i_query, va_list args );

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
    unsigned int i_aspect;

    /* Check for YUV file extension */
    psz_ext = strrchr( p_demux->psz_path, '.' );
    if( ( !psz_ext || strcasecmp( psz_ext, ".yuv") ) &&
        strcmp(p_demux->psz_demux, "rawvid") )
    {
        return VLC_EGENERIC;
    }

    /* Set p_input field */
    p_demux->pf_demux   = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys      = p_sys = malloc( sizeof( demux_sys_t ) );
    p_sys->i_pcr = 1;

    p_sys->f_fps = var_CreateGetFloat( p_demux, "rawvid-fps" );

    i_width = var_CreateGetInteger( p_demux, "rawvid-width" );
    i_height = var_CreateGetInteger( p_demux, "rawvid-height" );
    if( i_width <= 0 || i_height <= 0 )
    {
        msg_Err( p_demux, "width and height must be strictly positive." );
        free( p_sys );
        return VLC_EGENERIC;
    }

    psz_chroma = var_CreateGetString( p_demux, "rawvid-chroma" );
    psz_aspect_ratio = var_CreateGetString( p_demux, "rawvid-aspect-ratio" );

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

    if( ( p_block = stream_Block( p_demux->s, p_sys->frame_size ) ) == NULL )
    {
        /* EOF */
        return 0;
    }

    p_block->i_dts = p_block->i_pts = p_sys->i_pcr;
    es_out_Send( p_demux->out, p_sys->p_es_video, p_block );

    p_sys->i_pcr += ( I64C(1000000) / p_sys->f_fps );

    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys  = p_demux->p_sys;

    /* XXX: DEMUX_SET_TIME is precise here */
    return demux2_vaControlHelper( p_demux->s, 0, -1,
                                   p_sys->frame_size * p_sys->f_fps * 8,
                                   p_sys->frame_size, i_query, args );
}
