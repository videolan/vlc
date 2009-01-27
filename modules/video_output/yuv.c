/*****************************************************************************
 * yuv.c : yuv video output
 *****************************************************************************
 * Copyright (C) 2008, M2X BV
 * $Id$
 *
 * Authors: Jean-Paul Saman <jpsaman@videolan.org>
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
#include <vlc_vout.h>
#include <vlc_charset.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t *p_vout );
static void Display   ( vout_thread_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define YUV_FILE_TEXT N_("device, fifo or filename")
#define YUV_FILE_LONGTEXT N_("device, fifo or filename to write yuv frames too." )

#define CHROMA_TEXT N_("Chroma used.")
#define CHROMA_LONGTEXT N_( \
    "Force use of a specific chroma for output. Default is I420." )

#define YUV4MPEG2_TEXT N_( "YUV4MPEG2 header (default disabled)" )
#define YUV4MPEG2_LONGTEXT N_( "The YUV4MPEG2 header is compatible " \
    "with mplayer yuv video ouput and requires YV12/I420 fourcc. By default "\
    "vlc writes the fourcc of the picture frame into the output destination." )

#define CFG_PREFIX "yuv-"

vlc_module_begin ()
    set_shortname( N_( "YUV output" ) )
    set_description( N_( "YUV video output" ) )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VOUT )
    set_capability( "video output", 0 )

    add_string( CFG_PREFIX "file", "stream.yuv", NULL,
                YUV_FILE_TEXT, YUV_FILE_LONGTEXT, false )
    add_string( CFG_PREFIX "chroma", NULL, NULL,
                CHROMA_TEXT, CHROMA_LONGTEXT, true )
    add_bool  ( CFG_PREFIX "yuv4mpeg2", false, NULL,
                YUV4MPEG2_TEXT, YUV4MPEG2_LONGTEXT, true )

    set_callbacks( Create, Destroy )
vlc_module_end ()

static const char *const ppsz_vout_options[] = {
    "file", "chroma", "yuv4mpeg2", NULL
};

static void WriteYUV( vout_thread_t *p_vout, video_format_t fmt,
                      picture_t *p_pic );

/*****************************************************************************
 * vout_sys_t: video output descriptor
 *****************************************************************************/
struct vout_sys_t
{
    char *psz_file;
    FILE *p_fd;
    bool  b_header;
    bool  b_yuv4mpeg2;
    vlc_fourcc_t i_chroma;
};

/*****************************************************************************
 * Create: allocates video thread
 *****************************************************************************
 * This function allocates and initializes a vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = ( vout_thread_t * )p_this;
    vout_sys_t *p_sys;
    char *psz_fcc;

    config_ChainParse( p_vout, CFG_PREFIX, ppsz_vout_options,
                       p_vout->p_cfg );

    /* Allocate instance and initialize some members */
    p_vout->p_sys = p_sys = malloc( sizeof( vout_sys_t ) );
    if( !p_vout->p_sys )
        return VLC_ENOMEM;

    p_sys->b_header = false;
    p_sys->p_fd = NULL;

    p_sys->b_yuv4mpeg2 = var_CreateGetBool( p_this, CFG_PREFIX "yuv4mpeg2" );
    p_sys->i_chroma = VLC_FOURCC('I','4','2','0');

    p_sys->psz_file =
            var_CreateGetString( p_this, CFG_PREFIX "file" );
    p_sys->p_fd = utf8_fopen( p_sys->psz_file, "wb" );
    if( !p_sys->p_fd )
    {
        free( p_sys->psz_file );
        free( p_sys );
        return VLC_EGENERIC;
    }

    psz_fcc = var_CreateGetNonEmptyString( p_this, CFG_PREFIX "chroma" );
    if( psz_fcc && (strlen( psz_fcc ) == 4) )
    {
        p_sys->i_chroma = VLC_FOURCC( psz_fcc[0], psz_fcc[1],
                                      psz_fcc[2], psz_fcc[3] );
    }
    free( psz_fcc );

    if( p_sys->b_yuv4mpeg2 )
    {
        switch( p_sys->i_chroma )
        {
            case VLC_FOURCC('Y','V','1','2'):
            case VLC_FOURCC('I','4','2','0'):
            case VLC_FOURCC('I','Y','U','V'):
            case VLC_FOURCC('J','4','2','0'):
                break;
            default:
                msg_Err( p_this,
                    "YUV4MPEG2 mode needs chroma YV12 not %4s as requested",
                    (char *)&(p_sys->i_chroma) );
                fclose( p_vout->p_sys->p_fd );
                free( p_vout->p_sys->psz_file );
                free( p_vout->p_sys );
                return VLC_EGENERIC;
        }
    }

    msg_Dbg( p_this, "using chroma %4s", (char *)&(p_sys->i_chroma) );

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = NULL;
    p_vout->pf_display = Display;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize video thread
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = (vout_sys_t *) p_vout->p_sys;
    int i_index;
    picture_t *p_pic;

    /* Initialize the output structure */
    if( p_vout->render.i_chroma != p_sys->i_chroma )
        p_vout->output.i_chroma = p_vout->fmt_out.i_chroma = p_sys->i_chroma;
    else
       p_vout->output.i_chroma = p_vout->render.i_chroma;

    p_vout->output.pf_setpalette = NULL;
    p_vout->output.i_width = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->output.i_width
                               * VOUT_ASPECT_FACTOR / p_vout->output.i_height;

    p_vout->output.i_rmask = 0xff0000;
    p_vout->output.i_gmask = 0x00ff00;
    p_vout->output.i_bmask = 0x0000ff;

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
        return VLC_EGENERIC;

    vout_AllocatePicture( VLC_OBJECT(p_vout), p_pic, p_vout->output.i_chroma,
                          p_vout->output.i_width, p_vout->output.i_height,
                          p_vout->output.i_aspect );

    if( p_pic->i_planes == 0 )
    {
        return VLC_EGENERIC;
    }

    p_pic->i_status = DESTROYED_PICTURE;
    p_pic->i_type   = DIRECT_PICTURE;

    PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;
    I_OUTPUTPICTURES++;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy video thread
 *****************************************************************************
 * Terminate an output method created by Create
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    int i_index;
    vout_thread_t *p_vout = ( vout_thread_t * )p_this;

    for( i_index = I_OUTPUTPICTURES-1; i_index >= 0; i_index-- )
    {
        free( PP_OUTPUTPICTURE[ i_index ]->p_data );
    }

    /* Destroy structure */
    fclose( p_vout->p_sys->p_fd );

    free( p_vout->p_sys->psz_file );
    free( p_vout->p_sys );
}

/*****************************************************************************
 * Display: displays previously rendered output
 *****************************************************************************
 * This function copies the rendered picture into our circular buffer.
 *****************************************************************************/
static void Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    video_format_t fmt_in;

    memset( &fmt_in, 0, sizeof( fmt_in ) );
    video_format_Copy( &fmt_in, &p_pic->format );
    /* assume square pixels if i_sar_num = 0 */
    if( p_pic->format.i_sar_num == 0 )
        fmt_in.i_sar_num = fmt_in.i_sar_den = 1;

    WriteYUV( p_vout, fmt_in, p_pic );
    video_format_Clean( &fmt_in );
    return;
}

static void End( vout_thread_t *p_vout )
{
    VLC_UNUSED(p_vout);
}

static void WriteYUV( vout_thread_t *p_vout, video_format_t fmt,
                      picture_t *p_pic )
{
    vout_sys_t *p_sys = p_vout->p_sys;

    if( !p_pic || !p_sys ) return;

#if 0
    msg_Dbg( p_vout, "writing %d bytes of fourcc %4s",
             i_bytes, (char *)&fmt.i_chroma );
#endif
    if( !p_sys->b_header )
    {
        const char *p_hdr = "";
        if( p_sys->b_yuv4mpeg2 )
        {
            /* MPlayer compatible header, unfortunately it doesn't tell you
             * the exact fourcc used. */
            p_hdr = "YUV4MPEG2";
        }
        else
        {
            p_hdr = (const char*)&fmt.i_chroma;
        }
        fprintf( p_sys->p_fd, "%4s W%d H%d F%d:%d I%c A%d:%d\n",
                 p_hdr,
                 fmt.i_width, fmt.i_height,
                 fmt.i_frame_rate, fmt.i_frame_rate_base,
                 (p_pic->b_progressive ? 'p' :
                         (p_pic->b_top_field_first ? 't' : 'b')),
                 fmt.i_sar_num, fmt.i_sar_den );
        fflush( p_sys->p_fd );
        p_sys->b_header = true;
    }

    fprintf( p_sys->p_fd, "FRAME\n" );
    if( p_pic->b_progressive )
    {
        size_t i_bytes = (fmt.i_width * fmt.i_height * fmt.i_bits_per_pixel) >> 3;
        size_t i_written = 0;

        i_written = fwrite( p_pic->p_data, 1, i_bytes, p_sys->p_fd );
        if( i_written != i_bytes )
        {
            msg_Warn( p_vout, "only %d of %d bytes written",
                      i_written, i_bytes );
        }
    }
    else
    {
        msg_Warn( p_vout, "only progressive frames are supported, "
                          "use a deinterlace filter" );
    }
    fflush( p_sys->p_fd );
}
