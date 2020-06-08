/*****************************************************************************
 * mjpeg.c : demuxes mjpeg webcam http streams
 *****************************************************************************
 * Copyright (C) 2004 VLC authors and VideoLAN
 *
 * Authors: Henry Jen (slowhog) <henryjen@ztune.net>
 *          Derk-Jan Hartman (thedj)
 *          Sigmund Augdal (Dnumgis)
 *          Laurent Aimar <fenrir@via.ecp.fr>
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
#include "mxpeg_helper.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );

#define FPS_TEXT N_("Frames per Second")
#define FPS_LONGTEXT N_("This is the desired frame rate when " \
    "playing MJPEG from a file. Use 0 (this is the default value) for a " \
    "live stream (from a camera).")

vlc_module_begin ()
    set_shortname( "MJPEG")
    set_description( N_("M-JPEG camera demuxer") )
    set_capability( "demux", 5 )
    set_callback( Open )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    add_float( "mjpeg-fps", 0.0, FPS_TEXT, FPS_LONGTEXT, false )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int MimeDemux( demux_t * );
static int MjpgDemux( demux_t * );
static int Control( demux_t *, int i_query, va_list args );

typedef struct
{
    es_format_t     fmt;
    es_out_id_t     *p_es;

    bool            b_still;
    vlc_tick_t      i_still_end;
    vlc_tick_t      i_time;
    vlc_tick_t      i_frame_length;
    char            *psz_separator;
    int             i_frame_size_estimate;
    const uint8_t   *p_peek;
    int             i_data_peeked;
    int             i_level;
} demux_sys_t;

/*****************************************************************************
 * Peek: Helper function to peek data with incremental size.
 * \return false if peek no more data, true otherwise.
 *****************************************************************************/
static bool Peek( demux_t *p_demux, bool b_first )
{
    int i_data;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( b_first )
    {
        p_sys->i_data_peeked = 0;
    }
    else if( p_sys->i_data_peeked == p_sys->i_frame_size_estimate )
    {
        p_sys->i_frame_size_estimate += 5120;
    }
    i_data = vlc_stream_Peek( p_demux->s, &p_sys->p_peek,
                          p_sys->i_frame_size_estimate );
    if( i_data == p_sys->i_data_peeked )
    {
        msg_Warn( p_demux, "no more data" );
        return false;
    }
    p_sys->i_data_peeked = i_data;
    if( i_data <= 0 )
    {
        msg_Warn( p_demux, "cannot peek data" );
        return false;
    }
    return true;
}

/*****************************************************************************
 * GetLine: Internal function used to dup a line of string from the buffer
 *****************************************************************************/
static char* GetLine( demux_t *p_demux, int *p_pos )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const uint8_t *p_buf;
    int         i_size;
    int         i;
    char        *p_line;

    while( *p_pos >= p_sys->i_data_peeked )
    {
        if( ! Peek( p_demux, false ) )
        {
            return NULL;
        }
    }
    p_buf = p_sys->p_peek + *p_pos;
    i_size = p_sys->i_data_peeked - *p_pos;
    i = 0;
    while( p_buf[i] != '\n' )
    {
        i++;
        if( i == i_size )
        {
            if( ! Peek( p_demux, false ) )
            {
                return NULL;
            }
            p_buf = p_sys->p_peek + *p_pos;
            i_size = p_sys->i_data_peeked - *p_pos;
        }
    }
    *p_pos += i + 1;
    if( i > 0 && p_buf[i - 1] == '\r' )
    {
        i--;
    }
    p_line = vlc_obj_malloc( VLC_OBJECT(p_demux), i + 1 );
    if( unlikely( p_line == NULL ) )
        return NULL;
    strncpy ( p_line, (char*)p_buf, i );
    p_line[i] = '\0';
    return p_line;
}

/*****************************************************************************
 * CheckMimeHeader: Internal function used to verify and skip mime header
 * \param p_header_size Return size of MIME header, 0 if no MIME header
 * detected, minus value if error
 * \return true if content type is image/jpeg, false otherwise
 *****************************************************************************/
static bool CheckMimeHeader( demux_t *p_demux, int *p_header_size )
{
    bool        b_jpeg = false;
    int         i_pos = 0;
    char        *psz_line;
    char        *p_ch;
    demux_sys_t *p_sys = p_demux->p_sys;

    *p_header_size = -1;
    if( !Peek( p_demux, true ) )
    {
        msg_Err( p_demux, "cannot peek" );
        return false;
    }
    if( p_sys->i_data_peeked < 5)
    {
        msg_Err( p_demux, "data shortage" );
        return false;
    }
    if( strncmp( (char *)p_sys->p_peek, "--", 2 ) != 0
        && strncmp( (char *)p_sys->p_peek, "\r\n--", 4 ) != 0 )
    {
        *p_header_size = 0;
        return false;
    }
    else
    {
        i_pos = *p_sys->p_peek == '-' ? 2 : 4;
        psz_line = GetLine( p_demux, &i_pos );
        if( NULL == psz_line )
        {
            msg_Err( p_demux, "no EOL" );
            return false;
        }

        /* Read the separator and remember it if not yet stored */
        if( p_sys->psz_separator == NULL )
        {
            p_sys->psz_separator = psz_line;
            msg_Dbg( p_demux, "Multipart MIME detected, using separator: %s",
                     p_sys->psz_separator );
        }
        else
        {
            if( strcmp( psz_line, p_sys->psz_separator ) )
            {
                msg_Warn( p_demux, "separator %s does not match %s", psz_line,
                          p_sys->psz_separator );
            }
            vlc_obj_free( VLC_OBJECT(p_demux), psz_line );
        }
    }

    psz_line = GetLine( p_demux, &i_pos );
    while( psz_line && *psz_line )
    {
        if( !strncasecmp( psz_line, "Content-Type:", 13 ) )
        {
            p_ch = psz_line + 13;
            while( *p_ch != '\0' && ( *p_ch == ' ' || *p_ch == '\t' ) ) p_ch++;
            if( strncasecmp( p_ch, "image/jpeg", 10 ) )
            {
                msg_Warn( p_demux, "%s, image/jpeg is expected", psz_line );
                b_jpeg = false;
            }
            else
            {
                b_jpeg = true;
            }
        }
        else
        {
            msg_Dbg( p_demux, "discard MIME header: %s", psz_line );
        }
        vlc_obj_free( VLC_OBJECT(p_demux), psz_line );
        psz_line = GetLine( p_demux, &i_pos );
    }

    if( NULL == psz_line )
    {
        msg_Err( p_demux, "no EOL" );
        return false;
    }

    vlc_obj_free( VLC_OBJECT(p_demux), psz_line );

    *p_header_size = i_pos;
    return b_jpeg;
}

static int SendBlock( demux_t *p_demux, int i )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_block;

    if( ( p_block = vlc_stream_Block( p_demux->s, i ) ) == NULL )
    {
        msg_Warn( p_demux, "cannot read data" );
        return VLC_DEMUXER_EOF;
    }

    if( p_sys->i_frame_length != VLC_TICK_INVALID )
    {
        p_block->i_pts = p_sys->i_time;
        p_sys->i_time += p_sys->i_frame_length;
    }
    else
    {
        p_block->i_pts = vlc_tick_now();
    }
    p_block->i_dts = p_block->i_pts;

    es_out_SetPCR( p_demux->out, p_block->i_pts );
    es_out_Send( p_demux->out, p_sys->p_es, p_block );

    if( p_sys->b_still )
        p_sys->i_still_end = vlc_tick_now() + p_sys->i_frame_length;

    return VLC_DEMUXER_SUCCESS;
}

/*****************************************************************************
 * Open: check file and initializes structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    int         i_size;
    bool        b_matched = false;

    if( IsMxpeg( p_demux->s ) && !p_demux->obj.force )
        // let avformat handle this case
        return VLC_EGENERIC;

    demux_sys_t *p_sys = vlc_obj_malloc( p_this, sizeof (*p_sys) );
    if( unlikely(p_sys == NULL) )
        return VLC_ENOMEM;

    p_demux->p_sys      = p_sys;
    p_sys->p_es         = NULL;
    p_sys->i_time       = VLC_TICK_0;
    p_sys->i_level      = 0;

    p_sys->psz_separator = NULL;
    p_sys->i_frame_size_estimate = 15 * 1024;

    char *content_type = stream_ContentType( p_demux->s );
    if ( content_type )
    {
        //FIXME: this is not fully match to RFC
        char* boundary = strstr( content_type, "boundary=" );
        if( boundary )
        {
            boundary += strlen( "boundary=" );
            size_t len = strlen( boundary );
            if( len > 2 && boundary[0] == '"'
                && boundary[len-1] == '"' )
            {
                boundary[len-1] = '\0';
                boundary++;
            }
            p_sys->psz_separator = vlc_obj_strdup( p_this, boundary );
            if( !p_sys->psz_separator )
            {
                free( content_type );
                return VLC_ENOMEM;
            }
        }
        free( content_type );
    }

    b_matched = CheckMimeHeader( p_demux, &i_size);
    if( b_matched )
    {
        p_demux->pf_demux = MimeDemux;
        if( vlc_stream_Read( p_demux->s, NULL, i_size ) < i_size )
            return VLC_EGENERIC;
    }
    else if( i_size == 0 )
    {
        /* 0xffd8 identify a JPEG SOI */
        if( p_sys->p_peek[0] == 0xFF && p_sys->p_peek[1] == 0xD8 )
        {
            msg_Dbg( p_demux, "JPEG SOI marker detected" );
            p_demux->pf_demux = MjpgDemux;
            p_sys->i_level++;
        }
        else
        {
            return VLC_EGENERIC;
        }
    }
    else
    {
        return VLC_EGENERIC;
    }

    /* Frame rate */
    float f_fps = var_InheritFloat( p_demux, "mjpeg-fps" );

    p_sys->i_still_end = VLC_TICK_INVALID;
    if( demux_IsPathExtension( p_demux, ".jpeg" ) ||
        demux_IsPathExtension( p_demux, ".jpg" ) )
    {
        /* Plain JPEG file = single still picture */
        p_sys->b_still = true;
        if( f_fps == 0.f )
            /* Defaults to 1fps */
            f_fps = 1.f;
    }
    else
        p_sys->b_still = false;
    p_sys->i_frame_length = f_fps ? vlc_tick_rate_duration(f_fps) : VLC_TICK_INVALID;

    es_format_Init( &p_sys->fmt, VIDEO_ES, VLC_CODEC_MJPG );

    p_sys->fmt.i_id = 0;
    p_sys->p_es = es_out_Add( p_demux->out, &p_sys->fmt );
    if( unlikely(p_sys->p_es == NULL) )
        return VLC_ENOMEM;

    p_demux->pf_control = Control;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Demux: read packet and send them to decoders
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int MjpgDemux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i;

    if( p_sys->b_still && p_sys->i_still_end != VLC_TICK_INVALID )
    {
        /* Still frame, wait until the pause delay is gone */
        vlc_tick_wait( p_sys->i_still_end );
        p_sys->i_still_end = VLC_TICK_INVALID;
        return VLC_DEMUXER_SUCCESS;
    }

    if( !Peek( p_demux, true ) )
    {
        msg_Warn( p_demux, "cannot peek data" );
        return VLC_DEMUXER_EOF;
    }
    if( p_sys->i_data_peeked < 4 )
    {
        msg_Warn( p_demux, "data shortage" );
        return VLC_DEMUXER_EOF;
    }
    i = 3;
FIND_NEXT_EOI:
    while( !( p_sys->p_peek[i-1] == 0xFF && p_sys->p_peek[i] == 0xD9 ) )
    {
        if( p_sys->p_peek[i-1] == 0xFF && p_sys->p_peek[i] == 0xD9  )
        {
            p_sys->i_level++;
            msg_Dbg( p_demux, "we found another JPEG SOI at %d", i );
        }
        i++;
        if( i >= p_sys->i_data_peeked )
        {
            msg_Dbg( p_demux, "did not find JPEG EOI in %d bytes",
                     p_sys->i_data_peeked );
            if( !Peek( p_demux, false ) )
            {
                msg_Warn( p_demux, "no more data is available at the moment" );
                return VLC_DEMUXER_EOF;
            }
        }
    }
    i++;

    msg_Dbg( p_demux, "JPEG EOI detected at %d", i );
    p_sys->i_level--;

    if( p_sys->i_level > 0 )
        goto FIND_NEXT_EOI;
    return SendBlock( p_demux, i );
}

static int MimeDemux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int         i_size, i;

    bool  b_match = CheckMimeHeader( p_demux, &i_size );

    if( i_size > 0 )
    {
        if( vlc_stream_Read( p_demux->s, NULL, i_size ) != i_size )
            return VLC_DEMUXER_EOF;
    }
    else if( i_size < 0 )
    {
        return VLC_DEMUXER_EOF;
    }
    else
    {
        // No MIME header, assume OK
        b_match = true;
    }

    if( !Peek( p_demux, true ) )
    {
        msg_Warn( p_demux, "cannot peek data" );
        return VLC_DEMUXER_EOF;
    }

    i = 0;
    i_size = strlen( p_sys->psz_separator ) + 2;
    if( p_sys->i_data_peeked < i_size )
    {
        msg_Warn( p_demux, "data shortage" );
        return VLC_DEMUXER_EOF;
    }

    for( ;; )
    {
        while( !( p_sys->p_peek[i] == '-' && p_sys->p_peek[i+1] == '-' ) )
        {
            i++;
            i_size++;
            if( i_size >= p_sys->i_data_peeked )
            {
                msg_Dbg( p_demux, "MIME boundary not found in %d bytes of "
                         "data", p_sys->i_data_peeked );

                if( !Peek( p_demux, false ) )
                {
                    msg_Warn( p_demux, "no more data is available at the "
                              "moment" );
                    return VLC_DEMUXER_EOF;
                }
            }
        }

        /* Handle old and new style of separators */
        if (!strncmp(p_sys->psz_separator, (char *)(p_sys->p_peek + i + 2),
                     strlen( p_sys->psz_separator ))
         || ((strlen(p_sys->psz_separator) > 4)
          && !strncmp(p_sys->psz_separator, "--", 2)
          && !strncmp(p_sys->psz_separator, (char *)(p_sys->p_peek + i),
                      strlen( p_sys->psz_separator))))
        {
            break;
        }

        i++;
        i_size++;
    }

    if( !b_match )
    {
        msg_Err( p_demux, "discard non-JPEG part" );
        return VLC_DEMUXER_EOF;
    }

    return SendBlock( p_demux, i );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return demux_vaControlHelper( p_demux->s, 0, 0, 0, 0, i_query, args );
}
