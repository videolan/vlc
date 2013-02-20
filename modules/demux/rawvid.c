/*****************************************************************************
 * rawvid.c : raw video input module for vlc
 *****************************************************************************
 * Copyright (C) 2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Antoine Cellerier <dionoea at videolan d.t org>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define FPS_TEXT N_("Frames per Second")
#define FPS_LONGTEXT N_("This is the desired frame rate when " \
    "playing raw video streams. In the form 30000/1001 or 29.97")

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
    "Aspect ratio (4:3, 16:9). Default assumes square pixels." )

vlc_module_begin ()
    set_shortname( "Raw Video" )
    set_description( N_("Raw video demuxer") )
    set_capability( "demux", 10 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_callbacks( Open, Close )
    add_shortcut( "rawvideo" )
    add_string( "rawvid-fps", NULL, FPS_TEXT, FPS_LONGTEXT, false )
    add_integer( "rawvid-width", 0, WIDTH_TEXT, WIDTH_LONGTEXT, 0 )
    add_integer( "rawvid-height", 0, HEIGHT_TEXT, HEIGHT_LONGTEXT, 0 )
    add_string( "rawvid-chroma", NULL, CHROMA_TEXT, CHROMA_LONGTEXT,
                true )
    add_string( "rawvid-aspect-ratio", NULL,
                ASPECT_RATIO_TEXT, ASPECT_RATIO_LONGTEXT, true )
vlc_module_end ()

/*****************************************************************************
 * Definitions of structures used by this plugin
 *****************************************************************************/
struct demux_sys_t
{
    int    frame_size;

    es_out_id_t *p_es_video;
    es_format_t  fmt_video;

    date_t pcr;

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
    unsigned u_fps_num;
    unsigned u_fps_den;
    unsigned u_ar_num;
    unsigned u_ar_den;
    vlc_fourcc_t i_chroma;
};

static const struct preset_t p_presets[] =
{
    { "sqcif", 128, 96, 30000, 1001, 4,3, VLC_CODEC_YV12 },
    { "qcif", 176, 144, 30000, 1001, 4,3, VLC_CODEC_YV12 },
    { "cif", 352, 288, 30000, 1001, 4,3, VLC_CODEC_YV12 },
    { "4cif", 704, 576, 30000, 1001, 4,3, VLC_CODEC_YV12 },
    { "16cif", 1408, 1152, 30000, 1001, 4,3, VLC_CODEC_YV12 },
    { "yuv", 176, 144, 25, 1, 4,3, VLC_CODEC_YV12 },
    { NULL, 0, 0, 0, 0, 0,0, 0 }
};

/*****************************************************************************
 * Open: initializes raw DV demux structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    int i_width=-1, i_height=-1;
    unsigned u_fps_num=0, u_fps_den=1;
    vlc_fourcc_t i_chroma = 0;
    unsigned int i_sar_num = 0;
    unsigned int i_sar_den = 0;
    const struct preset_t *p_preset = NULL;
    const uint8_t *p_peek;
    bool b_y4m = false;

    if( stream_Peek( p_demux->s, &p_peek, 9 ) == 9 )
    {
        /* http://wiki.multimedia.cx/index.php?title=YUV4MPEG2 */
        if( !strncmp( (char *)p_peek, "YUV4MPEG2", 9 ) )
        {
            b_y4m = true;
            goto valid;
        }
    }

    if( !p_demux->b_force )
    {
        /* guess preset based on file extension */
        if( !p_demux->psz_file )
            return VLC_EGENERIC;

        const char *psz_ext = strrchr( p_demux->psz_file, '.' );
        if( !psz_ext )
            return VLC_EGENERIC;
        psz_ext++;

        for( unsigned i = 0; p_presets[i].psz_ext ; i++ )
        {
            if( !strcasecmp( psz_ext, p_presets[i].psz_ext ) )
            {
                p_preset = &p_presets[i];
                goto valid;
            }
        }
        return VLC_EGENERIC;
    }
valid:
    p_demux->p_sys      = p_sys = malloc( sizeof( demux_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->b_y4m = b_y4m;

    /* guess the parameters based on the preset */
    if( p_preset )
    {
        i_width = p_preset->i_width;
        i_height = p_preset->i_height;
        u_fps_num = p_preset->u_fps_num;
        u_fps_den = p_preset->u_fps_den;
        i_sar_num = p_preset->u_ar_num * p_preset->i_height;
        i_sar_den = p_preset->u_ar_den * p_preset->i_width;
        i_chroma = p_preset->i_chroma;
    }

    /* override presets if yuv4mpeg2 */
    if( b_y4m )
    {
        char *psz = stream_ReadLine( p_demux->s );
        char *psz_buf;
        int a = 1;
        int b = 1;

        /* The string will start with "YUV4MPEG2" */
        assert( strlen(psz) >= 9 );

        /* NB, it is not possible to handle interlaced here, since the
         * interlaced picture flags are in picture_t not block_t */

#define READ_FRAC( key, num, den ) do { \
        psz_buf = strstr( psz+9, key );\
        if( psz_buf )\
        {\
            char *end = strchr( psz_buf+1, ' ' );\
            char *sep;\
            if( end ) *end = '\0';\
            sep = strchr( psz_buf+1, ':' );\
            if( sep )\
            {\
                *sep = '\0';\
                den = atoi( sep+1 );\
            }\
            else\
            {\
                den = 1;\
            }\
            num = atoi( psz_buf+2 );\
            if( sep ) *sep = ':';\
            if( end ) *end = ' ';\
        } } while(0)
        READ_FRAC( " W", i_width, a );
        READ_FRAC( " H", i_height, a );
        READ_FRAC( " F", u_fps_num, u_fps_den );
        READ_FRAC( " A", a, b );
#undef READ_FRAC
        if( b != 0 )
        {
            i_sar_num = a;
            i_sar_den = b;
        }

        psz_buf = strstr( psz+9, " C" );
        if( psz_buf )
        {
            static const struct { const char *psz_name; vlc_fourcc_t i_fcc; } formats[] =
            {
                { "420jpeg",    VLC_CODEC_I420 },
                { "420paldv",   VLC_CODEC_I420 },
                { "420",        VLC_CODEC_I420 },
                { "422",        VLC_CODEC_I422 },
                { "444",        VLC_CODEC_I444 },
                { "mono",       VLC_CODEC_GREY },
                { NULL, 0 }
            };
            bool b_found = false;
            char *psz_end = strchr( psz_buf+1, ' ' );
            if( psz_end )
                *psz_end = '\0';
            psz_buf += 2;

            for( int i = 0; formats[i].psz_name != NULL; i++ )
            {
                if( !strncmp( psz_buf, formats[i].psz_name, strlen(formats[i].psz_name) ) )
                {
                    i_chroma = formats[i].i_fcc;
                    b_found = true;
                    break;
                }
            }
            if( !b_found )
                msg_Warn( p_demux, "Unknown YUV4MPEG2 chroma type \"%s\"", psz_buf );
            if( psz_end )
                *psz_end = ' ';
        }

        free( psz );
    }

    /* allow the user to override anything guessed from the input */
    int i_tmp;
    i_tmp = var_CreateGetInteger( p_demux, "rawvid-width" );
    if( i_tmp ) i_width = i_tmp;

    i_tmp = var_CreateGetInteger( p_demux, "rawvid-height" );
    if( i_tmp ) i_height = i_tmp;

    char *psz_tmp;
    psz_tmp = var_CreateGetNonEmptyString( p_demux, "rawvid-chroma" );
    if( psz_tmp )
    {
        if( strlen( psz_tmp ) != 4 )
        {
            msg_Err( p_demux, "Invalid fourcc format/chroma specification %s"
                     " expecting four characters eg, UYVY", psz_tmp );
            free( psz_tmp );
            goto error;
        }
        memcpy( &i_chroma, psz_tmp, 4 );
        msg_Dbg( p_demux, "Forcing chroma to 0x%.8x (%4.4s)", i_chroma, (char*)&i_chroma );
        free( psz_tmp );
    }

    psz_tmp = var_CreateGetNonEmptyString( p_demux, "rawvid-fps" );
    if( psz_tmp )
    {
        char *p_ptr;
        /* fps can either be n/d or q.f
         * for accuracy, avoid representation in float */
        u_fps_num = strtol( psz_tmp, &p_ptr, 10 );
        if( *p_ptr == '/' )
        {
            p_ptr++;
            u_fps_den = strtol( p_ptr, NULL, 10 );
        }
        else if( *p_ptr == '.' )
        {
            char *p_end;
            p_ptr++;
            int i_frac =  strtol( p_ptr, &p_end, 10 );
            u_fps_den = (p_end - p_ptr) * 10;
            if( !u_fps_den )
                u_fps_den = 1;
            u_fps_num = u_fps_num * u_fps_den + i_frac;
        }
        else if( *p_ptr == '\0')
        {
            u_fps_den = 1;
        }
        free( psz_tmp );
    }

    psz_tmp = var_CreateGetNonEmptyString( p_demux, "rawvid-aspect-ratio" );
    if( psz_tmp )
    {
        char *psz_denominator = strchr( psz_tmp, ':' );
        if( psz_denominator )
        {
            *psz_denominator++ = '\0';
            i_sar_num = atoi( psz_tmp )         * i_height;
            i_sar_den = atoi( psz_denominator ) * i_width;
        }
        free( psz_tmp );
    }

    /* moan about anything wrong */
    if( i_width <= 0 || i_height <= 0 )
    {
        msg_Err( p_demux, "width and height must be strictly positive." );
        goto error;
    }

    if( !u_fps_num || !u_fps_den )
    {
        msg_Err( p_demux, "invalid or no framerate specified." );
        goto error;
    }

    if( i_chroma == 0 )
    {
        msg_Err( p_demux, "invalid or no chroma specified." );
        goto error;
    }

    /* fixup anything missing with sensible assumptions */
    if( i_sar_num <= 0 || i_sar_den <= 0 )
    {
        /* assume 1:1 sar */
        i_sar_num = 1;
        i_sar_den = 1;
    }

    es_format_Init( &p_sys->fmt_video, VIDEO_ES, i_chroma );
    video_format_Setup( &p_sys->fmt_video.video,
                        i_chroma, i_width, i_height,
                        i_sar_num, i_sar_den );

    vlc_ureduce( &p_sys->fmt_video.video.i_frame_rate,
                 &p_sys->fmt_video.video.i_frame_rate_base,
                 u_fps_num, u_fps_den, 0);
    date_Init( &p_sys->pcr, p_sys->fmt_video.video.i_frame_rate,
               p_sys->fmt_video.video.i_frame_rate_base );
    date_Set( &p_sys->pcr, 0 );

    if( !p_sys->fmt_video.video.i_bits_per_pixel )
    {
        msg_Err( p_demux, "Unsupported chroma 0x%.8x (%4.4s)", i_chroma,
                 (char*)&i_chroma );
        goto error;
    }
    p_sys->frame_size = i_width * i_height
                        * p_sys->fmt_video.video.i_bits_per_pixel / 8;
    p_sys->p_es_video = es_out_Add( p_demux->out, &p_sys->fmt_video );

    p_demux->pf_demux   = Demux;
    p_demux->pf_control = Control;
    return VLC_SUCCESS;

error:
    stream_Seek( p_demux->s, 0 ); // Workaround, but y4m uses stream_ReadLines
    free( p_sys );
    return VLC_EGENERIC;
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
    mtime_t i_pcr = date_Get( &p_sys->pcr );

    /* Call the pace control */
    es_out_Control( p_demux->out, ES_OUT_SET_PCR, VLC_TS_0 + i_pcr );

    if( p_sys->b_y4m )
    {
        /* Skip the frame header */
        /* Skip "FRAME" */
        if( stream_Read( p_demux->s, NULL, 5 ) < 5 )
            return 0;
        /* Find \n */
        for( ;; )
        {
            uint8_t b;
            if( stream_Read( p_demux->s, &b, 1 ) < 1 )
                return 0;
            if( b == 0x0a )
                break;
        }
    }

    if( ( p_block = stream_Block( p_demux->s, p_sys->frame_size ) ) == NULL )
    {
        /* EOF */
        return 0;
    }

    p_block->i_dts = p_block->i_pts = VLC_TS_0 + i_pcr;
    es_out_Send( p_demux->out, p_sys->p_es_video, p_block );

    date_Increment( &p_sys->pcr, 1 );

    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys  = p_demux->p_sys;

     /* (2**31)-1 is insufficient to store 1080p50 4:4:4. */
    const int64_t i_bps = 8LL * p_sys->frame_size * p_sys->pcr.i_divider_num /
                                                    p_sys->pcr.i_divider_den;

    /* XXX: DEMUX_SET_TIME is precise here */
    return demux_vaControlHelper( p_demux->s, 0, -1, i_bps,
                                   p_sys->frame_size, i_query, args );
}

