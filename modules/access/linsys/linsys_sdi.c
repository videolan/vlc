/*****************************************************************************
 * linsys_sdi.c: SDI capture for Linear Systems/Computer Modules cards
 *****************************************************************************
 * Copyright (C) 2009-2011 VideoLAN
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <poll.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <vlc_input.h>
#include <vlc_access.h>
#include <vlc_demux.h>

#include <vlc_fs.h>

#include "linsys_sdi.h"

#undef ZVBI_DEBUG
#include <libzvbi.h>

#define SDI_DEVICE        "/dev/sdirx%u"
#define SDI_BUFFERS_FILE  "/sys/class/sdi/sdirx%u/buffers"
#define SDI_BUFSIZE_FILE  "/sys/class/sdi/sdirx%u/bufsize"
#define SDI_MODE_FILE     "/sys/class/sdi/sdirx%u/mode"
#define READ_TIMEOUT      80000
#define RESYNC_TIMEOUT    500000
#define CLOCK_GAP         INT64_C(500000)
#define START_DATE        INT64_C(4294967296)

#define DEMUX_BUFFER_SIZE 1350000
#define MAX_AUDIOS        4
#define SAMPLERATE_TOLERANCE 0.1

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define LINK_TEXT N_("Link #")
#define LINK_LONGTEXT N_( \
    "Allows you to set the desired link of the board for the capture (starting at 0)." )
#define VIDEO_TEXT N_("Video ID")
#define VIDEO_LONGTEXT N_( \
    "Allows you to set the ES ID of the video." )
#define VIDEO_ASPECT_TEXT N_("Aspect ratio")
#define VIDEO_ASPECT_LONGTEXT N_( \
    "Allows you to force the aspect ratio of the video." )
#define AUDIO_TEXT N_("Audio configuration")
#define AUDIO_LONGTEXT N_( \
    "Allows you to set audio configuration (id=group,pair:id=group,pair...)." )
#define TELX_TEXT N_("Teletext configuration")
#define TELX_LONGTEXT N_( \
    "Allows you to set Teletext configuration (id=line1-lineN with both fields)." )
#define TELX_LANG_TEXT N_("Teletext language")
#define TELX_LANG_LONGTEXT N_( \
    "Allows you to set Teletext language (page=lang/type,...)." )

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );
static int  DemuxOpen ( vlc_object_t * );
static void DemuxClose( vlc_object_t * );

vlc_module_begin()
    set_description( N_("SDI Input") )
    set_shortname( N_("SDI") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )

    add_integer( "linsys-sdi-link", 0,
        LINK_TEXT, LINK_LONGTEXT, true )

    add_integer( "linsys-sdi-id-video", 0, VIDEO_TEXT, VIDEO_LONGTEXT, true )
    add_string( "linsys-sdi-aspect-ratio", "", VIDEO_ASPECT_TEXT,
                VIDEO_ASPECT_LONGTEXT, true )
    add_string( "linsys-sdi-audio", "0=1,1", AUDIO_TEXT, AUDIO_LONGTEXT, true )
    add_string( "linsys-sdi-telx", "", TELX_TEXT, TELX_LONGTEXT, true )
    add_string( "linsys-sdi-telx-lang", "", TELX_LANG_TEXT, TELX_LANG_LONGTEXT,
                true )

    set_capability( "access_demux", 0 )
    add_shortcut( "linsys-sdi" )
    set_callbacks( Open, Close )

    add_submodule()
        set_description( N_("SDI Demux") )
        set_capability( "demux", 0 )
        set_callbacks( DemuxOpen, DemuxClose )
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct sdi_audio_t
{
    unsigned int i_group, i_pair;

    /* SDI parser */
    int32_t      i_delay;
    unsigned int i_rate;
    uint8_t      i_block_number;
    int16_t      *p_buffer;
    unsigned int i_left_samples, i_right_samples, i_nb_samples, i_max_samples;

    /* ES stuff */
    int          i_id;
    es_out_id_t  *p_es;
} sdi_audio_t;

enum {
    STATE_NOSYNC,
    STATE_STARTSYNC,
    STATE_ANCSYNC,
    STATE_LINESYNC,
    STATE_ACTIVESYNC,
    STATE_VBLANKSYNC,
    STATE_PICSYNC,
    STATE_SYNC,
};

struct demux_sys_t
{
    /* device reader */
    int              i_fd;
    unsigned int     i_link;
    uint8_t          **pp_buffers;
    unsigned int     i_buffers, i_current_buffer;
    unsigned int     i_buffer_size;

    /* SDI sync */
    int              i_state;
    mtime_t          i_last_state_change;
    unsigned int     i_anc_size, i_active_size, i_picture_size;
    unsigned int     i_line_offset, i_nb_lines;

    /* SDI parser */
    unsigned int     i_line_buffer;
    unsigned int     i_current_line;
    uint8_t          *p_line_buffer;
    block_t          *p_current_picture;
    uint8_t          *p_y, *p_u, *p_v;
    uint8_t          *p_wss_buffer;
    uint8_t          *p_telx_buffer;

    /* picture decoding */
    unsigned int     i_frame_rate, i_frame_rate_base;
    unsigned int     i_width, i_height, i_aspect, i_forced_aspect;
    unsigned int     i_block_size;
    unsigned int     i_telx_line, i_telx_count;
    char             *psz_telx, *psz_telx_lang;
    bool             b_hd, b_vbi;
    vbi_raw_decoder  rd_wss, rd_telx;
    mtime_t          i_next_date;
    int              i_incr;

    /* ES stuff */
    int              i_id_video;
    es_out_id_t      *p_es_video;
    sdi_audio_t      p_audios[MAX_AUDIOS];
    es_out_id_t      *p_es_telx;
};

static int Control( demux_t *, int, va_list );
static int DemuxControl( demux_t *, int, va_list );
static int Demux( demux_t * );
static int DemuxDemux( demux_t * );

static int InitWSS( demux_t *p_demux );
static int InitTelx( demux_t *p_demux );

static int HandleSDBuffer( demux_t *p_demux, uint8_t *p_buffer,
                           unsigned int i_buffer_size );

static int InitCapture( demux_t *p_demux );
static void CloseCapture( demux_t *p_demux );
static int Capture( demux_t *p_demux );

/*****************************************************************************
 * DemuxOpen:
 *****************************************************************************/
static int DemuxOpen( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys;
    char        *psz_parser;

    /* Fill p_demux field */
    p_demux->pf_demux = DemuxDemux;
    p_demux->pf_control = DemuxControl;
    p_demux->p_sys = p_sys = calloc( 1, sizeof( demux_sys_t ) );
    if( unlikely(!p_sys ) )
        return VLC_ENOMEM;

    p_sys->i_state = STATE_NOSYNC;
    p_sys->i_last_state_change = mdate();

    /* SDI AR */
    char *psz_ar = var_InheritString( p_demux, "linsys-sdi-aspect-ratio" );
    if ( psz_ar != NULL )
    {
        psz_parser = strchr( psz_ar, ':' );
        if ( psz_parser )
        {
            *psz_parser++ = '\0';
            p_sys->i_forced_aspect = p_sys->i_aspect =
                 strtol( psz_ar, NULL, 0 ) * VOUT_ASPECT_FACTOR
                 / strtol( psz_parser, NULL, 0 );
        }
        else
            p_sys->i_forced_aspect = 0;
        free( psz_ar );
    }

    /* */
    p_sys->i_id_video = var_InheritInteger( p_demux, "linsys-sdi-id-video" );

    /* Audio ES */
    char *psz_string = psz_parser = var_InheritString( p_demux,
                                                       "linsys-sdi-audio" );
    int i = 0;

    while ( psz_parser != NULL && *psz_parser )
    {
        int i_id, i_group, i_pair;
        char *psz_next = strchr( psz_parser, '=' );
        if ( psz_next != NULL )
        {
            *psz_next = '\0';
            i_id = strtol( psz_parser, NULL, 0 );
            psz_parser = psz_next + 1;
        }
        else
            i_id = 0;

        psz_next = strchr( psz_parser, ':' );
        if ( psz_next != NULL )
        {
            *psz_next = '\0';
            psz_next++;
        }

        if ( sscanf( psz_parser, "%d,%d", &i_group, &i_pair ) == 2 )
        {
            p_sys->p_audios[i].i_group = i_group;
            p_sys->p_audios[i].i_pair = i_pair;
            p_sys->p_audios[i].i_id = i_id;
            i++;
        }
        else
            msg_Warn( p_demux, "malformed audio configuration (%s)",
                      psz_parser );

        psz_parser = psz_next;
    }
    free( psz_string );

    /* Teletext ES */
    p_sys->psz_telx = var_InheritString( p_demux, "linsys-sdi-telx" );

    p_sys->psz_telx_lang = var_InheritString( p_demux, "linsys-sdi-telx-lang" );

    return VLC_SUCCESS;
}

static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys;
    int         i_ret;

    if ( (i_ret = DemuxOpen( p_this )) != VLC_SUCCESS )
        return i_ret;

    /* Fill p_demux field */
    p_demux->pf_demux    = Demux;
    p_demux->pf_control  = Control;
    p_sys = p_demux->p_sys;

    p_sys->i_link = var_InheritInteger( p_demux, "linsys-sdi-link" );

    if( InitCapture( p_demux ) != VLC_SUCCESS )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DemuxClose:
 *****************************************************************************/
static void DemuxClose( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys   = p_demux->p_sys;

    free( p_sys->psz_telx );
    free( p_sys->psz_telx_lang );
    free( p_sys );
}

static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t *)p_this;

    CloseCapture( p_demux );
    DemuxClose( p_this );
}

/*****************************************************************************
 * DemuxDemux:
 *****************************************************************************/
static int DemuxDemux( demux_t *p_demux )
{
    block_t *p_block = stream_Block( p_demux->s, DEMUX_BUFFER_SIZE );
    int i_ret;

    if ( p_block == NULL )
        return 0;

    i_ret = HandleSDBuffer( p_demux, p_block->p_buffer, p_block->i_buffer );
    block_Release( p_block );

    return ( i_ret == VLC_SUCCESS );
}

static int Demux( demux_t *p_demux )
{
    return ( Capture( p_demux ) == VLC_SUCCESS );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int DemuxControl( demux_t *p_demux, int i_query, va_list args )
{
    return demux_vaControlHelper( p_demux->s, -1, -1, 270000000, 1, i_query,
                                   args );
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    bool *pb;
    int64_t *pi64;

    switch( i_query )
    {
        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_CONTROL_PACE:
            /* TODO */
            pb = (bool*)va_arg( args, bool * );
            *pb = false;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = INT64_C(1000)
                  * var_InheritInteger( p_demux, "live-caching" );
            return VLC_SUCCESS;

        /* TODO implement others */
        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Video, audio & VBI decoding
 *****************************************************************************/
#define WSS_LINE        23

struct block_extension_t
{
    bool            b_progressive;          /**< is it a progressive frame ? */
    bool            b_top_field_first;             /**< which field is first */
    unsigned int    i_nb_fields;                  /**< # of displayed fields */
    unsigned int    i_aspect;                     /**< aspect ratio of frame */
};

static int NewFrame( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    p_sys->p_current_picture = block_Alloc( p_sys->i_block_size );
    if( unlikely( !p_sys->p_current_picture ) )
        return VLC_ENOMEM;
    p_sys->p_y = p_sys->p_current_picture->p_buffer;
    p_sys->p_u = p_sys->p_y + p_sys->i_width * p_sys->i_height;
    p_sys->p_v = p_sys->p_u + p_sys->i_width * p_sys->i_height / 4;

    for ( int i = 0; i < MAX_AUDIOS; i++ )
    {
        sdi_audio_t *p_audio = &p_sys->p_audios[i];
        p_audio->i_left_samples = p_audio->i_right_samples = 0;
    }
    return VLC_SUCCESS;
}

static int StartDecode( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    es_format_t fmt;
    char *psz_parser;

    p_sys->i_next_date = START_DATE;
    p_sys->i_incr = 1000000 * p_sys->i_frame_rate_base / p_sys->i_frame_rate;
    p_sys->i_block_size = p_sys->i_width * p_sys->i_height * 3 / 2
                           + sizeof(struct block_extension_t);
    if( NewFrame( p_demux ) != VLC_SUCCESS )
        return VLC_ENOMEM;

    /* Video ES */
    es_format_Init( &fmt, VIDEO_ES, VLC_CODEC_I420 );
    fmt.i_id                    = p_sys->i_id_video;
    fmt.video.i_frame_rate      = p_sys->i_frame_rate;
    fmt.video.i_frame_rate_base = p_sys->i_frame_rate_base;
    fmt.video.i_width           = p_sys->i_width;
    fmt.video.i_height          = p_sys->i_height;
    int i_aspect = p_sys->i_forced_aspect ? p_sys->i_forced_aspect
                                          : p_sys->i_aspect;
    fmt.video.i_sar_num = i_aspect * fmt.video.i_height
                           / fmt.video.i_width;
    fmt.video.i_sar_den = VOUT_ASPECT_FACTOR;
    p_sys->p_es_video   = es_out_Add( p_demux->out, &fmt );

    if ( p_sys->b_vbi && InitWSS( p_demux ) != VLC_SUCCESS )
        p_sys->b_vbi = 0;

    /* Teletext ES */
    psz_parser = p_sys->psz_telx;
    if ( psz_parser != NULL && *psz_parser )
    {
        if ( !p_sys->b_vbi )
        {
            msg_Warn( p_demux, "VBI is unsupported on this input stream" );
        }
        else
        {
            int i_id;
            char *psz_next = strchr( psz_parser, '=' );
            if ( psz_next != NULL )
            {
                *psz_next = '\0';
                i_id = strtol( psz_parser, NULL, 0 );
                psz_parser = psz_next + 1;
            }
            else
                i_id = 0;

            psz_next = strchr( psz_parser, '-' );
            if ( psz_next != NULL )
                *psz_next++ = '\0';

            p_sys->i_telx_line = strtol( psz_parser, NULL, 0 ) - 1;
            if ( psz_next != NULL )
                p_sys->i_telx_count = strtol( psz_next, NULL, 0 )
                                       - p_sys->i_telx_line - 1 + 1;
            else
                p_sys->i_telx_count = 1;

            if ( InitTelx( p_demux ) == VLC_SUCCESS )
            {
                int i_dr_size = 0;
                uint8_t *p_dr = NULL;

                msg_Dbg( p_demux, "capturing VBI lines %d-%d and %d-%d",
                         p_sys->i_telx_line + 1,
                         p_sys->i_telx_line + 1 + p_sys->i_telx_count - 1,
                         p_sys->i_telx_line + 1 + 313,
                         p_sys->i_telx_line + 1 + 313
                                                + p_sys->i_telx_count - 1 );

                es_format_Init( &fmt, SPU_ES, VLC_CODEC_TELETEXT );
                fmt.i_id = i_id;

                /* Teletext language & type */
                psz_parser = p_sys->psz_telx_lang;

                while ( (psz_next = strchr( psz_parser, '=' )) != NULL )
                {
                    int i_page;
                    *psz_next++ = '\0';
                    if ( !psz_next[0] || !psz_next[1] || !psz_next[2] )
                        break;
                    i_page = strtol( psz_parser, NULL, 0 );
                    i_dr_size += 5;
                    p_dr = realloc( p_dr, i_dr_size );
                    p_dr[i_dr_size - 5] = *psz_next++;
                    p_dr[i_dr_size - 4] = *psz_next++;
                    p_dr[i_dr_size - 3] = *psz_next++;
                    if ( *psz_next == '/' )
                    {
                        psz_next++;
                        p_dr[i_dr_size - 2] = strtol( psz_next, &psz_next, 0 )
                                               << 3;
                    }
                    else  /* subtitle for hearing impaired */
                        p_dr[i_dr_size - 2] = 0x5 << 3;
                    p_dr[i_dr_size - 2] |= (i_page / 100) & 0x7;
                    p_dr[i_dr_size - 1] = i_page % 100;

                    if ( *psz_next == ',' )
                        psz_next++;
                    psz_parser = psz_next;
                }

                fmt.i_extra = i_dr_size;
                fmt.p_extra = p_dr;
                p_sys->p_es_telx = es_out_Add( p_demux->out, &fmt );
            }
            else
                p_sys->i_telx_count = 0;
        }
    }
    return VLC_SUCCESS;
}

static void StopDecode( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if ( p_sys->i_state != STATE_SYNC )
        return;

    free( p_sys->p_line_buffer );

    block_Release( p_sys->p_current_picture );
    p_sys->p_current_picture = NULL;
    es_out_Del( p_demux->out, p_sys->p_es_video );

    if ( p_sys->b_vbi )
    {
        free( p_sys->p_wss_buffer );
        p_sys->p_wss_buffer = NULL;
        vbi_raw_decoder_destroy( &p_sys->rd_wss );

        if ( p_sys->p_es_telx )
        {
            es_out_Del( p_demux->out, p_sys->p_es_telx );
            free( p_sys->p_telx_buffer );
            p_sys->p_telx_buffer = NULL;
            vbi_raw_decoder_destroy( &p_sys->rd_telx );
        }
    }

    for ( int i = 0; i < MAX_AUDIOS; i++ )
    {
        sdi_audio_t *p_audio = &p_sys->p_audios[i];
        if ( p_audio->i_group && p_audio->p_es != NULL )
        {
            es_out_Del( p_demux->out, p_audio->p_es );
            p_audio->p_es = NULL;
            free( p_audio->p_buffer );
            p_audio->p_buffer = NULL;
        }
    }
}

static void InitVideo( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_total_width = (p_sys->i_anc_size + p_sys->i_active_size) * 4 / 5;
    p_sys->i_width = (p_sys->i_active_size - 5) * 4 / 10;
    if ( p_sys->i_nb_lines == 625 )
    {
        /* PAL */
        p_sys->i_frame_rate      = 25;
        p_sys->i_frame_rate_base = 1;
        p_sys->i_height          = 576;
        p_sys->i_aspect          = 4 * VOUT_ASPECT_FACTOR / 3;
        p_sys->b_hd              = false;
    }
    else if ( p_sys->i_nb_lines == 525 )
    {
        /* NTSC */
        p_sys->i_frame_rate      = 30000;
        p_sys->i_frame_rate_base = 1001;
        p_sys->i_height          = 480;
        p_sys->i_aspect          = 4 * VOUT_ASPECT_FACTOR / 3;
        p_sys->b_hd              = false;
    }
    else if ( p_sys->i_nb_lines == 1125 && i_total_width == 2640 )
    {
        /* 1080i50 or 1080p25 */
        p_sys->i_frame_rate      = 25;
        p_sys->i_frame_rate_base = 1;
        p_sys->i_height          = 1080;
        p_sys->i_aspect          = 16 * VOUT_ASPECT_FACTOR / 9;
        p_sys->b_hd              = true;
    }
    else if ( p_sys->i_nb_lines == 1125 && i_total_width == 2200 )
    {
        /* 1080i60 or 1080p30 */
        p_sys->i_frame_rate      = 30000;
        p_sys->i_frame_rate_base = 1001;
        p_sys->i_height          = 1080;
        p_sys->i_aspect          = 16 * VOUT_ASPECT_FACTOR / 9;
        p_sys->b_hd              = true;
    }
    else if ( p_sys->i_nb_lines == 750 && i_total_width == 1980 )
    {
        /* 720p50 */
        p_sys->i_frame_rate      = 50;
        p_sys->i_frame_rate_base = 1;
        p_sys->i_height          = 720;
        p_sys->i_aspect          = 16 * VOUT_ASPECT_FACTOR / 9;
        p_sys->b_hd              = true;
    }
    else if ( p_sys->i_nb_lines == 750 && i_total_width == 1650 )
    {
        /* 720p60 */
        p_sys->i_frame_rate      = 60000;
        p_sys->i_frame_rate_base = 1001;
        p_sys->i_height          = 720;
        p_sys->i_aspect          = 16 * VOUT_ASPECT_FACTOR / 9;
        p_sys->b_hd              = true;
    }
    else
    {
        msg_Warn( p_demux, "unable to determine video type" );
        /* Put sensitive defaults */
        p_sys->i_frame_rate      = 25;
        p_sys->i_frame_rate_base = 1;
        p_sys->i_height          = p_sys->i_nb_lines;
        p_sys->i_aspect          = 16 * VOUT_ASPECT_FACTOR / 9;
        p_sys->b_hd              = true;
    }
    p_sys->b_vbi = !p_sys->b_hd;
}

static void DecodeVideo( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    struct block_extension_t ext;

    /* FIXME: progressive formats ? */
    ext.b_progressive     = false;
    ext.i_nb_fields       = 2;
    ext.b_top_field_first = true;
    ext.i_aspect = p_sys->i_forced_aspect ? p_sys->i_forced_aspect :
                   p_sys->i_aspect;

    memcpy( &p_sys->p_current_picture->p_buffer[p_sys->i_block_size
                                     - sizeof(struct block_extension_t)],
            &ext, sizeof(struct block_extension_t) );

    p_sys->p_current_picture->i_dts = p_sys->p_current_picture->i_pts
        = p_sys->i_next_date;
    es_out_Send( p_demux->out, p_sys->p_es_video, p_sys->p_current_picture );
}

static int InitWSS( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    vbi_raw_decoder_init( &p_sys->rd_wss );

    p_sys->rd_wss.scanning        = 625;
    p_sys->rd_wss.sampling_format = VBI_PIXFMT_UYVY;
    p_sys->rd_wss.sampling_rate   = 13.5e6;
    p_sys->rd_wss.bytes_per_line  = 720 * 2;
    p_sys->rd_wss.offset          = 9.5e-6 * 13.5e6;

    p_sys->rd_wss.start[0] = 23;
    p_sys->rd_wss.count[0] = 1;
    p_sys->rd_wss.start[1] = 0;
    p_sys->rd_wss.count[1] = 0;

    p_sys->rd_wss.interlaced = FALSE;
    p_sys->rd_wss.synchronous = TRUE;

    if ( vbi_raw_decoder_add_services( &p_sys->rd_wss,
                                       VBI_SLICED_WSS_625,
                                       /* strict */ 2 ) == 0 )
    {
        msg_Warn( p_demux, "cannot initialize zvbi for WSS" );
        vbi_raw_decoder_destroy ( &p_sys->rd_telx );
        return VLC_EGENERIC;
    }

    p_sys->p_wss_buffer = malloc( p_sys->i_width * 2 );
    if( !p_sys->p_wss_buffer )
    {
        vbi_raw_decoder_destroy ( &p_sys->rd_telx );
        return VLC_ENOMEM;
    }
    return VLC_SUCCESS;
}

static void DecodeWSS( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    vbi_sliced p_sliced[1];

    if ( vbi_raw_decode( &p_sys->rd_wss, p_sys->p_wss_buffer, p_sliced ) == 0 )
    {
        p_sys->i_aspect = 4 * VOUT_ASPECT_FACTOR / 3;
    }
    else
    {
        unsigned int i_old_aspect = p_sys->i_aspect;
        uint8_t *p = p_sliced[0].data;
        int i_aspect, i_parity;

        i_aspect = p[0] & 15;
        i_parity = i_aspect;
        i_parity ^= i_parity >> 2;
        i_parity ^= i_parity >> 1;
        i_aspect &= 7;

        if ( !(i_parity & 1) )
            msg_Warn( p_demux, "WSS parity error" );
        else if ( i_aspect == 7 )
            p_sys->i_aspect = 16 * VOUT_ASPECT_FACTOR / 9;
        else
            p_sys->i_aspect = 4 * VOUT_ASPECT_FACTOR / 3;

        if ( p_sys->i_aspect != i_old_aspect )
            msg_Dbg( p_demux, "new WSS information (ra=%x md=%x cod=%x hlp=%x rvd=%x sub=%x pos=%x srd=%x c=%x cp=%x)",
                     i_aspect, (p[0] & 0x10) >> 4, (p[0] & 0x20) >> 5,
                     (p[0] & 0x40) >> 6, (p[0] & 0x80) >> 7, p[1] & 0x01,
                     (p[1] >> 1) & 3, (p[1] & 0x08) >> 3, (p[1] & 0x10) >> 4,
                     (p[1] & 0x20) >> 5 );
    }
}

static int InitTelx( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    vbi_raw_decoder_init( &p_sys->rd_telx );

    p_sys->rd_telx.scanning        = 625;
    p_sys->rd_telx.sampling_format = VBI_PIXFMT_UYVY;
    p_sys->rd_telx.sampling_rate   = 13.5e6;
    p_sys->rd_telx.bytes_per_line  = 720 * 2;
    p_sys->rd_telx.offset          = 9.5e-6 * 13.5e6;

    p_sys->rd_telx.start[0] = p_sys->i_telx_line + 1;
    p_sys->rd_telx.count[0] = p_sys->i_telx_count;
    p_sys->rd_telx.start[1] = p_sys->i_telx_line + 1 + 313;
    p_sys->rd_telx.count[1] = p_sys->i_telx_count;

    p_sys->rd_telx.interlaced = FALSE;
    p_sys->rd_telx.synchronous = TRUE;

    if ( vbi_raw_decoder_add_services( &p_sys->rd_telx, VBI_SLICED_TELETEXT_B,
                                       0 ) == 0 )
    {
        msg_Warn( p_demux, "cannot initialize zvbi for Teletext" );
        vbi_raw_decoder_destroy ( &p_sys->rd_telx );
        return VLC_EGENERIC;
    }

    p_sys->p_telx_buffer = malloc( p_sys->i_telx_count * p_sys->i_width * 4 );
    if( !p_sys->p_telx_buffer )
    {
        vbi_raw_decoder_destroy ( &p_sys->rd_telx );
        return VLC_ENOMEM;
    }
    return VLC_SUCCESS;
}

static int DecodeTelx( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    vbi_sliced p_sliced[p_sys->i_telx_count * 2];
    int i_nb_slices = vbi_raw_decode( &p_sys->rd_telx, p_sys->p_telx_buffer,
                                      p_sliced );

    if ( i_nb_slices )
    {
        /* 3, 7, 11, 15, etc. */
        int i_nb_slices_rounded = 3 + (i_nb_slices / 4) * 4;
        int i;
        uint8_t *p;
        block_t *p_block = block_Alloc( 1 + i_nb_slices_rounded * 46 );
        if( unlikely( !p_block ) )
            return VLC_ENOMEM;
        p_block->p_buffer[0] = 0x10; /* FIXME ? data_identifier */
        p = p_block->p_buffer + 1;

        for ( i = 0; i < i_nb_slices; i++ )
        {
            int i_line = p_sliced[i].line;
            p[0] = 0x3; /* FIXME data_unit_id == subtitles */
            p[1] = 0x2c; /* data_unit_length */
            /* reserved | field_parity (kind of inverted) | line */
            p[2] = 0xc0 | (i_line > 313 ? 0 : 0x20) | (i_line % 313);
            p[3] = 0xe4; /* framing code */
            for ( int j = 0; j < 42; j++ )
                p[4 + j] = vbi_rev8( p_sliced[i].data[j] );
            p += 46;
        }

        /* Let's stuff */
        for ( ; i < i_nb_slices_rounded; i++ )
        {
            p[0] = 0xff;
            p[1] = 0x2c;
            memset( p + 2, 0xff, 44 );
            p += 46;
        }

        p_block->i_dts = p_block->i_pts = p_sys->i_next_date;
        es_out_Send( p_demux->out, p_sys->p_es_telx, p_block );
    }
    return VLC_SUCCESS;
}

static int InitAudio( demux_t *p_demux, sdi_audio_t *p_audio )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    es_format_t fmt;

    msg_Dbg( p_demux, "starting audio %u/%u rate:%u delay:%d", p_audio->i_group,
             p_audio->i_pair, p_audio->i_rate, p_audio->i_delay );

    es_format_Init( &fmt, AUDIO_ES, VLC_CODEC_S16L );
    fmt.i_id = p_audio->i_id;
    fmt.audio.i_channels          = 2;
    fmt.audio.i_original_channels =
    fmt.audio.i_physical_channels = AOUT_CHANS_STEREO;
    fmt.audio.i_rate              = p_audio->i_rate;
    fmt.audio.i_bitspersample     = 16;
    fmt.audio.i_blockalign        = fmt.audio.i_channels *
                                    fmt.audio.i_bitspersample / 8;
    fmt.i_bitrate                 = fmt.audio.i_channels * fmt.audio.i_rate *
                                    fmt.audio.i_bitspersample;
    p_audio->p_es                 = es_out_Add( p_demux->out, &fmt );

    p_audio->i_nb_samples         = p_audio->i_rate * p_sys->i_frame_rate_base
                                    / p_sys->i_frame_rate;
    p_audio->i_max_samples        = (float)p_audio->i_nb_samples *
                                    (1. + SAMPLERATE_TOLERANCE);

    p_audio->p_buffer             = malloc( p_audio->i_max_samples * sizeof(int16_t) * 2 );
    p_audio->i_left_samples       = p_audio->i_right_samples = 0;
    p_audio->i_block_number       = 0;

    if( unlikely( !p_audio->p_buffer ) )
        return VLC_ENOMEM;
    return VLC_SUCCESS;
}

/* Fast and efficient linear resampling routine */
static void ResampleAudio( int16_t *p_out, int16_t *p_in,
                           unsigned int i_out, unsigned int i_in )
{
    unsigned int i_remainder = 0;
    float f_last_sample = (float)*p_in / 32768.0;

    *p_out = *p_in;
    p_out += 2;
    p_in += 2;

    for ( unsigned int i = 1; i < i_in; i++ )
    {
        float f_in = (float)*p_in / 32768.0;
        while ( i_remainder < i_out )
        {
            float f_out = f_last_sample;
            f_out += (f_in - f_last_sample) * i_remainder / i_out;
            if ( f_out >= 1.0 ) *p_out = 32767;
            else if ( f_out < -1.0 ) *p_out = -32768;
            else *p_out = f_out * 32768.0;
            p_out += 2;
            i_remainder += i_in;
        }

        f_last_sample = f_in;
        p_in += 2;
        i_remainder -= i_out;
    }
}

static int DecodeAudio( demux_t *p_demux, sdi_audio_t *p_audio )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_block;
    int16_t *p_output;

    if ( p_audio->p_buffer == NULL )
        return VLC_EGENERIC;
    if ( !p_audio->i_left_samples && !p_audio->i_right_samples )
    {
        msg_Warn( p_demux, "no audio %u/%u", p_audio->i_group,
                  p_audio->i_pair );
        return VLC_EGENERIC;
    }
    if ( p_audio->i_left_samples <
            (float)p_audio->i_nb_samples * (1. - SAMPLERATE_TOLERANCE) ||
        p_audio->i_left_samples >
            (float)p_audio->i_nb_samples * (1. + SAMPLERATE_TOLERANCE) )
    {
        msg_Warn( p_demux,
            "left samplerate out of tolerance for audio %u/%u (%u vs. %u)",
            p_audio->i_group, p_audio->i_pair,
            p_audio->i_left_samples, p_audio->i_nb_samples );
        return VLC_EGENERIC;
    }
    if ( p_audio->i_right_samples <
            (float)p_audio->i_nb_samples * (1. - SAMPLERATE_TOLERANCE) ||
        p_audio->i_right_samples >
            (float)p_audio->i_nb_samples * (1. + SAMPLERATE_TOLERANCE) )
    {
        msg_Warn( p_demux,
            "right samplerate out of tolerance for audio %u/%u (%u vs. %u)",
            p_audio->i_group, p_audio->i_pair,
            p_audio->i_right_samples, p_audio->i_nb_samples );
        return VLC_EGENERIC;
    }

    p_block = block_Alloc( p_audio->i_nb_samples * sizeof(int16_t) * 2 );
    if( unlikely( !p_block ) )
        return VLC_ENOMEM;
    p_block->i_dts = p_block->i_pts = p_sys->i_next_date
        + (mtime_t)p_audio->i_delay * INT64_C(1000000) / p_audio->i_rate;
    p_output = (int16_t *)p_block->p_buffer;

    if ( p_audio->i_left_samples == p_audio->i_nb_samples &&
         p_audio->i_right_samples == p_audio->i_nb_samples )
        memcpy( p_output, p_audio->p_buffer,
                    p_audio->i_nb_samples * sizeof(int16_t) * 2 );
    else
    {
        ResampleAudio( p_output, p_audio->p_buffer,
                       p_audio->i_nb_samples, p_audio->i_left_samples );

        ResampleAudio( p_output + 1, p_audio->p_buffer + 1,
                       p_audio->i_nb_samples, p_audio->i_right_samples );
    }

    es_out_Send( p_demux->out, p_audio->p_es, p_block );
    return VLC_SUCCESS;
}

static int DecodeFrame( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if ( p_sys->b_vbi )
    {
        DecodeWSS( p_demux );

        if ( p_sys->i_height == 576 )
        {
            /* For PAL, erase first half of line 23, last half of line 623,
             * and line 624 ; no need to erase chrominance */
            memset( p_sys->p_y, 0, p_sys->i_width / 2 );
            memset( p_sys->p_y + p_sys->i_width * 574 + p_sys->i_width / 2,
                        0, p_sys->i_width * 3 / 2 );
        }
    }

    if ( p_sys->i_telx_count )
        if ( DecodeTelx( p_demux ) != VLC_SUCCESS )
            return VLC_ENOMEM;

    for ( int i = 0; i < MAX_AUDIOS; i++ )
    {
        if ( p_sys->p_audios[i].i_group && p_sys->p_audios[i].p_es != NULL )
            if( DecodeAudio( p_demux, &p_sys->p_audios[i] ) != VLC_SUCCESS )
                return VLC_EGENERIC;
    }

    DecodeVideo( p_demux );

    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->i_next_date );
    p_sys->i_next_date += p_sys->i_incr;

    if( NewFrame( p_demux ) != VLC_SUCCESS )
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SDI syntax parsing stuff
 *****************************************************************************/
#define FIELD_1_VBLANK_EAV  0xB6
#define FIELD_1_VBLANK_SAV  0xAB
#define FIELD_1_ACTIVE_EAV  0x9D
#define FIELD_1_ACTIVE_SAV  0x80
#define FIELD_2_VBLANK_EAV  0xF1
#define FIELD_2_VBLANK_SAV  0xEC
#define FIELD_2_ACTIVE_EAV  0xDA
#define FIELD_2_ACTIVE_SAV  0xC7

static const uint8_t *FindReferenceCode( uint8_t i_code,
                                         const uint8_t *p_parser,
                                         const uint8_t *p_end )
{
    while ( p_parser <= p_end - 5 )
    {
        if ( p_parser[0] == 0xff && p_parser[1] == 0x3 && p_parser[2] == 0x0
              && p_parser[3] == 0x0 && p_parser[4] == i_code )
            return p_parser;
        p_parser += 5;
    }

    return NULL;
}

static const uint8_t *CountReference( unsigned int *pi_count, uint8_t i_code,
                                      const uint8_t *p_parser,
                                      const uint8_t *p_end )
{
    const uint8_t *p_tmp = FindReferenceCode( i_code, p_parser, p_end );
    if ( p_tmp == NULL )
    {
        *pi_count += p_end - p_parser;
        return NULL;
    }
    *pi_count += p_tmp - p_parser;
    return p_tmp;
}

static const uint8_t *GetLine( demux_t *p_demux, const uint8_t **pp_parser,
                               const uint8_t *p_end )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned int i_total_size = p_sys->i_anc_size + p_sys->i_active_size;
    const uint8_t *p_tmp;

    if ( p_sys->i_line_buffer )
    {
        unsigned int i_remaining = i_total_size - p_sys->i_line_buffer;
        memcpy( p_sys->p_line_buffer + p_sys->i_line_buffer,
                                   *pp_parser, i_remaining );
        *pp_parser += i_remaining;
        p_sys->i_line_buffer = 0;

        return p_sys->p_line_buffer;
    }

    if ( p_end - *pp_parser < (int)i_total_size )
    {
        memcpy( p_sys->p_line_buffer, *pp_parser,
                                   p_end - *pp_parser );
        p_sys->i_line_buffer = p_end - *pp_parser;
        return NULL;
    }

    p_tmp = *pp_parser;
    *pp_parser += i_total_size;
    return p_tmp;
}

#define U   (uint16_t)((p_line[0]) | ((p_line[1] & 0x3) << 8))
#define Y1  (uint16_t)((p_line[1] >> 2) | ((p_line[2] & 0xf) << 6))
#define V   (uint16_t)((p_line[2] >> 4) | ((p_line[3] & 0x3f) << 4))
#define Y2  (uint16_t)((p_line[3] >> 6) | (p_line[4] << 2))

static void UnpackVBI( const uint8_t *p_line, unsigned int i_size,
                       uint8_t *p_dest )
{
    const uint8_t *p_end = p_line + i_size;

    while ( p_line < p_end )
    {
        *p_dest++ = (U + 2) / 4;
        *p_dest++ = (Y1 + 2) / 4;
        *p_dest++ = (V + 2) / 4;
        *p_dest++ = (Y2 + 2) / 4;
        p_line += 5;
    }
}

/* For lines 0 [4] or 1 [4] */
static void Unpack01( const uint8_t *p_line, unsigned int i_size,
                      uint8_t *p_y, uint8_t *p_u, uint8_t *p_v )
{
    const uint8_t *p_end = p_line + i_size;

    while ( p_line < p_end )
    {
        *p_u++ = (U + 2) / 4;
        *p_y++ = (Y1 + 2) / 4;
        *p_v++ = (V + 2) / 4;
        *p_y++ = (Y2 + 2) / 4;
        p_line += 5;
    }
}

/* For lines 2 [4] */
static void Unpack2( const uint8_t *p_line, unsigned int i_size,
                     uint8_t *p_y, uint8_t *p_u, uint8_t *p_v )
{
    const uint8_t *p_end = p_line + i_size;

    while ( p_line < p_end )
    {
        uint16_t tmp;
        tmp = 3 * *p_u;
        tmp += (U + 2) / 4;
        *p_u++ = tmp / 4;
        *p_y++ = (Y1 + 2) / 4;
        tmp = 3 * *p_v;
        tmp += (V + 2) / 4;
        *p_v++ = tmp / 4;
        *p_y++ = (Y2 + 2) / 4;
        p_line += 5;
    }
}

/* For lines 3 [4] */
static void Unpack3( const uint8_t *p_line, unsigned int i_size,
                     uint8_t *p_y, uint8_t *p_u, uint8_t *p_v )
{
    const uint8_t *p_end = p_line + i_size;

    while ( p_line < p_end )
    {
        uint16_t tmp;
        tmp = *p_u;
        tmp += 3 * (U + 2) / 4;
        *p_u++ = tmp / 4;
        *p_y++ = (Y1 + 2) / 4;
        tmp = *p_v;
        tmp += 3 * (V + 2) / 4;
        *p_v++ = tmp / 4;
        *p_y++ = (Y2 + 2) / 4;
        p_line += 5;
    }
}

#undef U
#undef Y1
#undef V
#undef Y2

#define A0  (uint16_t)((p_anc[0]) | ((p_anc[1] & 0x3) << 8))
#define A1  (uint16_t)((p_anc[1] >> 2) | ((p_anc[2] & 0xf) << 6))
#define A2  (uint16_t)((p_anc[2] >> 4) | ((p_anc[3] & 0x3f) << 4))
#define A3  (uint16_t)((p_anc[3] >> 6) | (p_anc[4] << 2))

static void UnpackAnc( const uint8_t *p_anc, unsigned int i_size,
                       uint16_t *p_dest )
{
    const uint8_t *p_end = p_anc + i_size;

    while ( p_anc <= p_end - 5 )
    {
        *p_dest++ = A0;
        *p_dest++ = A1;
        *p_dest++ = A2;
        *p_dest++ = A3;
        p_anc += 5;
    }
}

#undef A0
#undef A1
#undef A2
#undef A3

static int HasAncillary( const uint8_t *p_anc )
{
    return ( (p_anc[0] == 0x0 && p_anc[1] == 0xfc && p_anc[2] == 0xff
               && (p_anc[3] & 0x3f) == 0x3f) );
}

static void HandleAudioData( demux_t *p_demux, const uint16_t *p_anc,
                             uint8_t i_data_count, uint8_t i_group,
                             uint8_t i_block_number )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if ( i_data_count % 3 )
    {
        msg_Warn( p_demux, "malformed audio data for group %u", i_group );
        return;
    }

    for ( int i = 0; i < MAX_AUDIOS; i++ )
    {
        sdi_audio_t *p_audio = &p_sys->p_audios[i];
        if ( p_audio->i_group == i_group )
        {
            const uint16_t *x = p_anc;

            /* SMPTE 272M says that when parsing a frame, if an audio config
             * structure is present we will encounter it first. Otherwise
             * it is assumed to be 48 kHz. */
            if ( p_audio->p_es == NULL )
            {
                p_audio->i_rate = 48000;
                p_audio->i_delay = 0;
                if( InitAudio( p_demux, p_audio ) != VLC_SUCCESS )
                    return;
            }

            if ( i_block_number )
            {
                if ( p_audio->i_block_number + 1 != i_block_number )
                    msg_Warn( p_demux,
                              "audio data block discontinuity (%"PRIu8"->%"PRIu8") for group %"PRIu8,
                              p_audio->i_block_number, i_block_number,
                              i_group );
                if ( i_block_number == 0xff )
                    p_audio->i_block_number = 0;
                else
                    p_audio->i_block_number = i_block_number;
            }

            while ( x < p_anc + i_data_count )
            {
                if ( ((*x & 0x4) && p_audio->i_pair == 2)
                      || (!(*x & 0x4) && p_audio->i_pair == 1) )
                {
                    uint32_t i_tmp = (uint32_t)((x[0] & 0x1f1) >> 3)
                                                  | ((x[1] & 0x1ff) << 6)
                                                  | ((x[2] & 0x1f) << 15);
                    int32_t i_sample;
                    if ( x[2] & 0x10 )
                        i_sample = i_tmp | 0xfff00000;
                    else
                        i_sample = i_tmp;

                    if ( x[0] & 0x2 )
                    {
                        if ( p_audio->i_right_samples < p_audio->i_max_samples )
                            p_audio->p_buffer[2 * p_audio->i_right_samples
                                               + 1] = (i_sample + 8) / 16;
                        p_audio->i_right_samples++;
                    }
                    else
                    {
                        if ( p_audio->i_left_samples < p_audio->i_max_samples )
                            p_audio->p_buffer[2 * p_audio->i_left_samples]
                                = (i_sample + 8) / 16;
                        p_audio->i_left_samples++;
                    }
                }
                x += 3;
            }
        }
    }
}

static void HandleAudioConfig( demux_t *p_demux, const uint16_t *p_anc,
                               uint8_t i_data_count, uint8_t i_group )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if ( i_data_count != 18 )
    {
        msg_Warn( p_demux, "malformed audio config for group %u", i_group );
        return;
    }

    for ( int i = 0; i < MAX_AUDIOS; i++ )
    {
        sdi_audio_t *p_audio = &p_sys->p_audios[i];
        if ( p_audio->i_group == i_group && p_audio->p_es == NULL )
        {
            unsigned int i_rate;

            if ( p_audio->i_pair == 2 )
            {
                i_rate = (p_anc[2] & 0xe0) >> 5;
                if ( p_anc[7] & 0x1 )
                {
                    uint32_t i_tmp = ((p_anc[7] & 0x1fe) >> 1)
                                       | ((p_anc[8] & 0x1ff) << 8)
                                       | ((p_anc[9] & 0x1ff) << 17);
                    if ( p_anc[9] & 0x80 )
                        p_audio->i_delay = i_tmp | 0xfc000000;
                    else
                        p_audio->i_delay = i_tmp;
                }
                if ( p_anc[13] & 0x1 )
                    msg_Warn( p_demux, "asymetric audio is not supported" );
            }
            else
            {
                i_rate = (p_anc[2] & 0xe) >> 1;
                if ( p_anc[4] & 0x1 )
                {
                    uint32_t i_tmp = ((p_anc[4] & 0x1fe) >> 1)
                                       | ((p_anc[5] & 0x1ff) << 8)
                                       | ((p_anc[6] & 0x1ff) << 17);
                    if ( p_anc[6] & 0x80 )
                        p_audio->i_delay = i_tmp | 0xfc000000;
                    else
                        p_audio->i_delay = i_tmp;
                }
                if ( p_anc[10] & 0x1 )
                    msg_Warn( p_demux, "asymetric audio is not supported" );
            }

            switch ( i_rate )
            {
            case 0: p_audio->i_rate = 48000; break;
            case 1: p_audio->i_rate = 44100; break;
            case 2: p_audio->i_rate = 32000; break;
            default:
                msg_Warn( p_demux, "unknown rate for audio %u/%u (%u)",
                          i_group, p_sys->p_audios[i].i_pair, i_rate );
                continue;
            }

            if( InitAudio( p_demux, p_audio ) != VLC_SUCCESS )
                return;
        }
    }
}

/*
 * Ancillary packet structure:
 *  byte 0: Ancillary Data Flag (0)
 *  byte 1: Ancillary Data Flag (0x3ff)
 *  byte 2: Ancillary Data Flag (0x3ff)
 *  byte 3: Data ID (2 high order bits = parity)
 *  byte 4: Data Block Number 1-255 or 0=unknown (if DID < 0x80)
 *       or Secondary Data ID (if DID >= 0x80)
 *  byte 5: Data Count (10 bits)
 *  byte 6+DC: Checksum
 */
static void HandleAncillary( demux_t *p_demux, const uint16_t *p_anc,
                             unsigned int i_size )
{
    uint8_t i_data_count;

    if ( i_size < 7
          || p_anc[0] != 0x0 || p_anc[1] != 0x3ff || p_anc[2] != 0x3ff )
        return;

    i_data_count = p_anc[5] & 0xff;
    if ( i_size - 6 < i_data_count )
    {
        msg_Warn( p_demux, "malformed ancillary packet (size %u > %u)",
                  i_data_count, i_size - 6 );
        return;
    }

    switch ( p_anc[3] ) /* Data ID */
    {
    case 0x2ff:
        HandleAudioData( p_demux, p_anc + 6, i_data_count, 1, p_anc[4] & 0xff );
        break;
    case 0x1fd:
        HandleAudioData( p_demux, p_anc + 6, i_data_count, 2, p_anc[4] & 0xff );
        break;
    case 0x1fb:
        HandleAudioData( p_demux, p_anc + 6, i_data_count, 3, p_anc[4] & 0xff );
        break;
    case 0x2f9:
        HandleAudioData( p_demux, p_anc + 6, i_data_count, 4, p_anc[4] & 0xff );
        break;

    case 0x1ef:
        HandleAudioConfig( p_demux, p_anc + 6, i_data_count, 1 );
        break;
    case 0x2ee:
        HandleAudioConfig( p_demux, p_anc + 6, i_data_count, 2 );
        break;
    case 0x2ed:
        HandleAudioConfig( p_demux, p_anc + 6, i_data_count, 3 );
        break;
    case 0x1ec:
        HandleAudioConfig( p_demux, p_anc + 6, i_data_count, 4 );
        break;

    /* Extended data packets, same order */
    case 0x1fe:
    case 0x2fc:
    case 0x2fa:
    case 0x1f8:

    default:
        break;

    case 0x88: /* non-conforming ANC packet */
        p_anc += 7;
        i_size -= 7;
        while ( i_size >= 7 && (p_anc[0] != 0x0 || p_anc[1] != 0x3ff
                                 || p_anc[2] != 0x3ff) )
        {
            p_anc++;
            i_size--;
        }
        if ( i_size >= 7 )
            HandleAncillary( p_demux, p_anc, i_size );
        return;
    }

    return HandleAncillary( p_demux, p_anc + i_data_count + 7,
                            i_size - i_data_count - 7 );

}

static int HandleSDBuffer( demux_t *p_demux, uint8_t *p_buffer,
                           unsigned int i_buffer_size )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const uint8_t *p_parser = p_buffer;
    const uint8_t *p_end = p_parser + i_buffer_size;
    const uint8_t *p_line;

    if ( p_sys->i_state != STATE_SYNC
          && p_sys->i_last_state_change < mdate() - RESYNC_TIMEOUT )
    {
        p_sys->i_state = STATE_NOSYNC;
        p_sys->i_last_state_change = mdate();
        return VLC_EGENERIC;
    }

    switch ( p_sys->i_state )
    {
    case STATE_NOSYNC:
    default:
        p_parser = FindReferenceCode( FIELD_2_VBLANK_SAV, p_parser, p_end );
        if ( p_parser == NULL )
            break;
        p_sys->i_state = STATE_STARTSYNC;
        p_sys->i_last_state_change = mdate();

    case STATE_STARTSYNC:
        p_parser = FindReferenceCode( FIELD_1_VBLANK_EAV, p_parser, p_end );
        if ( p_parser == NULL )
            break;
        p_sys->i_anc_size = 0;
        p_sys->i_state = STATE_ANCSYNC;
        p_sys->i_last_state_change = mdate();

    case STATE_ANCSYNC:
        p_parser = CountReference( &p_sys->i_anc_size,
                                   FIELD_1_VBLANK_SAV, p_parser, p_end );
        if ( p_parser == NULL )
            break;
        p_sys->i_active_size = 0;
        p_sys->i_state = STATE_LINESYNC;
        p_sys->i_last_state_change = mdate();

    case STATE_LINESYNC:
        p_parser = CountReference( &p_sys->i_active_size,
                                   FIELD_1_VBLANK_EAV, p_parser, p_end );
        if ( p_parser == NULL )
            break;
        p_sys->i_picture_size = p_sys->i_anc_size + p_sys->i_active_size;
        p_sys->i_state = STATE_ACTIVESYNC;
        p_sys->i_last_state_change = mdate();

    case STATE_ACTIVESYNC:
        p_parser = CountReference( &p_sys->i_picture_size,
                                   FIELD_1_ACTIVE_EAV, p_parser, p_end );
        if ( p_parser == NULL )
            break;
        p_sys->i_line_offset = p_sys->i_picture_size
                             / (p_sys->i_anc_size + p_sys->i_active_size);
        p_sys->i_state = STATE_VBLANKSYNC;
        p_sys->i_last_state_change = mdate();

    case STATE_VBLANKSYNC:
        p_parser = CountReference( &p_sys->i_picture_size,
                                   FIELD_2_ACTIVE_EAV, p_parser, p_end );
        if ( p_parser == NULL )
            break;
        p_sys->i_state = STATE_PICSYNC;
        p_sys->i_last_state_change = mdate();

    case STATE_PICSYNC:
        p_parser = CountReference( &p_sys->i_picture_size,
                                   FIELD_1_VBLANK_EAV, p_parser, p_end );
        if ( p_parser == NULL )
            break;

        if ( p_sys->i_picture_size
              % (p_sys->i_anc_size + p_sys->i_active_size) )
        {
            msg_Warn( p_demux, "wrong picture size (anc=%d active=%d total=%d offset=%d), syncing",
                 p_sys->i_anc_size, p_sys->i_active_size,
                 p_sys->i_picture_size, p_sys->i_line_offset + 1 );
            p_sys->i_state = STATE_NOSYNC;
            p_sys->i_last_state_change = mdate();
            break;
        }

        p_sys->i_nb_lines = p_sys->i_picture_size
                             / (p_sys->i_anc_size + p_sys->i_active_size);
        InitVideo( p_demux );
        msg_Dbg( p_demux,
                 "acquired sync, anc=%d active=%d lines=%d offset=%d",
                 p_sys->i_anc_size, p_sys->i_active_size,
                 p_sys->i_nb_lines, p_sys->i_line_offset + 1 );
        p_sys->i_state = STATE_SYNC;
        if( StartDecode( p_demux ) != VLC_SUCCESS )
        {
            StopDecode( p_demux );
            return VLC_ENOMEM;
        }
        p_sys->i_current_line = 0;
        p_sys->p_line_buffer = malloc( p_sys->i_anc_size
                                        + p_sys->i_active_size );
        if( !p_sys->p_line_buffer )
        {
            StopDecode( p_demux );
            return VLC_ENOMEM;
        }
        p_sys->i_line_buffer = 0;

    case STATE_SYNC:
        while ( (p_line = GetLine( p_demux, &p_parser, p_end )) != NULL )
        {
            bool b_field = p_sys->b_hd ? false :
                (p_sys->i_current_line >= p_sys->i_nb_lines / 2);
            unsigned int i_field_height = p_sys->b_hd ? p_sys->i_height :
                p_sys->i_height / 2;
            unsigned int i_field_line = b_field ?
                p_sys->i_current_line - (p_sys->i_nb_lines + 1) / 2 :
                p_sys->i_current_line;
            bool b_vbi = i_field_line < p_sys->i_line_offset ||
                i_field_line >= p_sys->i_line_offset + i_field_height;
            unsigned int anc = p_sys->i_anc_size;

            if ( p_line[0] != 0xff || p_line[1] != 0x3
                  || p_line[2] != 0x0 || p_line[3] != 0x0
                  || p_line[anc+0] != 0xff || p_line[anc+1] != 0x3
                  || p_line[anc+2] != 0x0 || p_line[anc+3] != 0x0
                  || (!b_field && b_vbi &&
                      (p_line[4] != FIELD_1_VBLANK_EAV ||
                       p_line[anc+4] != FIELD_1_VBLANK_SAV))
                  || (!b_field && !b_vbi &&
                      (p_line[4] != FIELD_1_ACTIVE_EAV ||
                       p_line[anc+4] != FIELD_1_ACTIVE_SAV))
                  || (b_field && b_vbi &&
                      (p_line[4] != FIELD_2_VBLANK_EAV ||
                       p_line[anc+4] != FIELD_2_VBLANK_SAV))
                  || (b_field && !b_vbi &&
                      (p_line[4] != FIELD_2_ACTIVE_EAV ||
                       p_line[anc+4] != FIELD_2_ACTIVE_SAV)) )
            {
                msg_Warn( p_demux, "lost sync line:%u SAV:%x EAV:%x",
                          p_sys->i_current_line + 1, p_line[4], p_line[anc+4] );
                StopDecode( p_demux );
                p_sys->i_state = STATE_NOSYNC;
                p_sys->i_last_state_change = mdate();
                break;
            }

            if ( HasAncillary( p_line + 5 ) )
            {
                /* HANC */
                unsigned int i_anc_words = (p_sys->i_anc_size - 5) * 4 / 5;
                uint16_t p_anc[i_anc_words];
                UnpackAnc( p_line + 5, p_sys->i_anc_size - 5, p_anc );
                HandleAncillary( p_demux, p_anc, i_anc_words );
            }

            if ( !b_vbi )
            {
                unsigned int i_active_field_line = i_field_line
                                                    - p_sys->i_line_offset;
                unsigned int i_active_line = b_field
                                              + i_active_field_line * 2;
                if ( !(i_active_field_line % 2) && !b_field )
                    Unpack01( p_line + anc + 5, p_sys->i_active_size - 5,
                              p_sys->p_y + p_sys->i_width * i_active_line,
                              p_sys->p_u + (p_sys->i_width / 2)
                               * (i_active_line / 2),
                              p_sys->p_v + (p_sys->i_width / 2)
                               * (i_active_line / 2) );
                else if ( !(i_active_field_line % 2) )
                    Unpack01( p_line + anc + 5, p_sys->i_active_size - 5,
                              p_sys->p_y + p_sys->i_width * i_active_line,
                              p_sys->p_u + (p_sys->i_width / 2)
                               * (i_active_line / 2 + 1),
                              p_sys->p_v + (p_sys->i_width / 2)
                               * (i_active_line / 2 + 1) );
                else if ( !b_field )
                    Unpack2( p_line + anc + 5, p_sys->i_active_size - 5,
                             p_sys->p_y + p_sys->i_width * i_active_line,
                             p_sys->p_u + (p_sys->i_width / 2)
                              * (i_active_line / 2 - 1),
                             p_sys->p_v + (p_sys->i_width / 2)
                              * (i_active_line / 2 - 1) );
                else
                    Unpack3( p_line + anc + 5, p_sys->i_active_size - 5,
                             p_sys->p_y + p_sys->i_width * i_active_line,
                             p_sys->p_u + (p_sys->i_width / 2)
                              * (i_active_line / 2),
                             p_sys->p_v + (p_sys->i_width / 2)
                              * (i_active_line / 2) );

                if ( p_sys->b_vbi && p_sys->i_height == 576
                      && p_sys->i_current_line == p_sys->i_line_offset )
                {
                    /* Line 23 is half VBI, half active */
                    UnpackVBI( p_line + anc + 5, p_sys->i_active_size - 5,
                               p_sys->p_wss_buffer );
                }
            }
            else if ( p_sys->b_vbi && p_sys->i_telx_count &&
                      i_field_line >= p_sys->i_telx_line &&
                      i_field_line < p_sys->i_telx_line
                                      + p_sys->i_telx_count )
            {
                UnpackVBI( p_line + anc + 5, p_sys->i_active_size - 5,
                    &p_sys->p_telx_buffer[(i_field_line
                        - p_sys->i_telx_line + b_field * p_sys->i_telx_count)
                        * p_sys->i_width * 2] );
            }
            else if ( b_vbi && HasAncillary( p_line + anc + 5 ) )
            {
                /* VANC */
                unsigned int i_anc_words = (p_sys->i_active_size - 5) * 4 / 5;
                uint16_t p_anc[i_anc_words];
                UnpackAnc( p_line + 5, p_sys->i_active_size - 5,
                           p_anc );
                HandleAncillary( p_demux, p_anc, i_anc_words );
            }

            p_sys->i_current_line++;
            if ( p_sys->i_current_line == p_sys->i_nb_lines )
            {
                p_sys->i_current_line %= p_sys->i_nb_lines;
                if( DecodeFrame( p_demux ) != VLC_SUCCESS )
                    return VLC_EGENERIC;
            }
        }
        break;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Low-level device stuff
 *****************************************************************************/
#define MAXLEN 256

static int ReadULSysfs( const char *psz_fmt, unsigned int i_link )
{
    char psz_file[MAXLEN], psz_data[MAXLEN];
    char *psz_tmp;
    int i_fd;
    ssize_t i_ret;
    unsigned int i_data;

    snprintf( psz_file, sizeof(psz_file) - 1, psz_fmt, i_link );

    if ( (i_fd = vlc_open( psz_file, O_RDONLY )) < 0 )
        return i_fd;

    i_ret = read( i_fd, psz_data, sizeof(psz_data) );
    close( i_fd );

    if ( i_ret < 0 )
        return i_ret;

    i_data = strtoul( psz_data, &psz_tmp, 0 );
    if ( *psz_tmp != '\n' )
        return -1;

    return i_data;
}

static ssize_t WriteULSysfs( const char *psz_fmt, unsigned int i_link,
                             unsigned int i_buf )
{
    char psz_file[MAXLEN], psz_data[MAXLEN];
    int i_fd;
    ssize_t i_ret;

    snprintf( psz_file, sizeof(psz_file) -1, psz_fmt, i_link );

    snprintf( psz_data, sizeof(psz_data) -1, "%u\n", i_buf );

    if ( (i_fd = vlc_open( psz_file, O_WRONLY )) < 0 )
        return i_fd;

    i_ret = write( i_fd, psz_data, strlen(psz_data) + 1 );
    close( i_fd );
    return i_ret;
}

static int InitCapture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const int i_page_size = getpagesize();
    unsigned int i_bufmemsize;
    int i_ret;
    char psz_dev[MAXLEN];

    /* 10-bit mode or nothing */
    if ( WriteULSysfs( SDI_MODE_FILE, p_sys->i_link, SDI_CTL_MODE_10BIT ) < 0 )
    {
        msg_Err( p_demux, "couldn't write file " SDI_MODE_FILE, p_sys->i_link );
        return VLC_EGENERIC;
    }

    if ( (i_ret = ReadULSysfs( SDI_BUFFERS_FILE, p_sys->i_link )) < 0 )
    {
        msg_Err( p_demux, "couldn't read file " SDI_BUFFERS_FILE,
                 p_sys->i_link );
        return VLC_EGENERIC;
    }
    p_sys->i_buffers = i_ret;
    p_sys->i_current_buffer = 0;

    if ( (i_ret = ReadULSysfs( SDI_BUFSIZE_FILE, p_sys->i_link )) < 0 )
    {
        msg_Err( p_demux, "couldn't read file " SDI_BUFSIZE_FILE,
                 p_sys->i_link );
        return VLC_EGENERIC;
    }
    p_sys->i_buffer_size = i_ret;
    if ( p_sys->i_buffer_size % 20 )
    {
        msg_Err( p_demux, "buffer size must be a multiple of 20" );
        return VLC_EGENERIC;
    }

    snprintf( psz_dev, sizeof(psz_dev) - 1, SDI_DEVICE, p_sys->i_link );
    if ( (p_sys->i_fd = vlc_open( psz_dev, O_RDONLY ) ) < 0 )
    {
        msg_Err( p_demux, "couldn't open device %s", psz_dev );
        return VLC_EGENERIC;
    }

    i_bufmemsize = ((p_sys->i_buffer_size + i_page_size - 1) / i_page_size)
                     * i_page_size;
    p_sys->pp_buffers = malloc( p_sys->i_buffers * sizeof(uint8_t *) );
    if( !p_sys->pp_buffers )
        return VLC_ENOMEM;

    for ( unsigned int i = 0; i < p_sys->i_buffers; i++ )
    {
        if ( (p_sys->pp_buffers[i] = mmap( NULL, p_sys->i_buffer_size,
                                           PROT_READ, MAP_SHARED, p_sys->i_fd,
                                           i * i_bufmemsize )) == MAP_FAILED )
        {
            msg_Err( p_demux, "couldn't mmap(%d): %m", i );
            free( p_sys->pp_buffers );
            return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}

static void CloseCapture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    StopDecode( p_demux );
    for ( unsigned int i = 0; i < p_sys->i_buffers; i++ )
        munmap( p_sys->pp_buffers[i], p_sys->i_buffer_size );
    close( p_sys->i_fd );
    free( p_sys->pp_buffers );
}

static int Capture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    struct pollfd pfd;

    pfd.fd = p_sys->i_fd;
    pfd.events = POLLIN | POLLPRI;

    if ( poll( &pfd, 1, READ_TIMEOUT ) < 0 )
    {
        msg_Warn( p_demux, "couldn't poll(): %m" );
        return VLC_EGENERIC;
    }

    if ( pfd.revents & POLLPRI )
    {
        unsigned int i_val;

        if ( ioctl( p_sys->i_fd, SDI_IOC_RXGETEVENTS, &i_val ) < 0 )
            msg_Warn( p_demux, "couldn't SDI_IOC_RXGETEVENTS %m" );
        else
        {
            if ( i_val & SDI_EVENT_RX_BUFFER )
                msg_Warn( p_demux, "driver receive buffer queue overrun" );
            if ( i_val & SDI_EVENT_RX_FIFO )
                msg_Warn( p_demux, "onboard receive FIFO overrun");
            if ( i_val & SDI_EVENT_RX_CARRIER )
                msg_Warn( p_demux, "carrier status change");
        }

        p_sys->i_next_date += CLOCK_GAP;
    }

    if ( pfd.revents & POLLIN )
    {
        int i_ret;

        if ( ioctl( p_sys->i_fd, SDI_IOC_DQBUF, p_sys->i_current_buffer ) < 0 )
        {
            msg_Warn( p_demux, "couldn't SDI_IOC_DQBUF %m" );
            return VLC_EGENERIC;
        }

        i_ret = HandleSDBuffer( p_demux,
                                p_sys->pp_buffers[p_sys->i_current_buffer],
                                p_sys->i_buffer_size );

        if ( ioctl( p_sys->i_fd, SDI_IOC_QBUF, p_sys->i_current_buffer ) < 0 )
        {
            msg_Warn( p_demux, "couldn't SDI_IOC_QBUF %m" );
            return VLC_EGENERIC;
        }

        if ( i_ret == VLC_SUCCESS )
        {
            p_sys->i_current_buffer++;
            p_sys->i_current_buffer %= p_sys->i_buffers;
        }
        else
        {
            /* Reference codes do not start on a multiple of 5. This sometimes
             * happen. We really don't want to allow this. */
            msg_Warn( p_demux, "resetting board" );
            CloseCapture( p_demux );
            InitCapture( p_demux );
        }
    }

    return VLC_SUCCESS;
}

