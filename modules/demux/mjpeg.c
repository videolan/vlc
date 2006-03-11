/*****************************************************************************
 * mjpeg.c : demuxes mjpeg webcam http streams
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Henry Jen (slowhog) <henryjen@ztune.net>
 *          Derk-Jan Hartman (thedj)
 *          Sigmund Augdal (Dnumgis)
 *          Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc/input.h>

#include <codecs.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define FPS_TEXT N_("Frames per Second")
#define FPS_LONGTEXT N_("This is the desired frame rate when " \
    "playing MJPEG from a file. Use 0 (this is the default value) for a " \
    "live stream (from the camera).")

vlc_module_begin();
    set_shortname( "MJPEG");
    set_description( _("M-JPEG camera demuxer") );
    set_capability( "demux2", 5 );
    set_callbacks( Open, Close );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    add_float( "mjpeg-fps", 0.0, NULL, FPS_TEXT, FPS_LONGTEXT, VLC_FALSE );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int MimeDemux( demux_t * );
static int MjpgDemux( demux_t * );
static int Control( demux_t *, int i_query, va_list args );

struct demux_sys_t
{
    es_format_t     fmt;
    es_out_id_t     *p_es;

    vlc_bool_t      b_still;
    mtime_t         i_still_end;
    mtime_t         i_still_length;

    mtime_t         i_time;
    mtime_t         i_frame_length;
    char            *psz_separator;
    int             i_frame_size_estimate;
    uint8_t         *p_peek;
    int             i_data_peeked;
};

/*****************************************************************************
 * Peek: Helper function to peek data with incremental size.
 * \return VLC_FALSE if peek no more data, VLC_TRUE otherwise.
 *****************************************************************************/
static vlc_bool_t Peek( demux_t *p_demux, vlc_bool_t b_first )
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
    i_data = stream_Peek( p_demux->s, &p_sys->p_peek,
                          p_sys->i_frame_size_estimate );
    if( i_data == p_sys->i_data_peeked )
    {
        msg_Warn( p_demux, "no more data" );
        return VLC_FALSE;
    }
    p_sys->i_data_peeked = i_data;
    if( i_data <= 0 )
    {
        msg_Warn( p_demux, "cannot peek data" );
        return VLC_FALSE;
    }
    return VLC_TRUE;
}

/*****************************************************************************
 * GetLine: Internal function used to dup a line of string from the buffer
 *****************************************************************************/
static char* GetLine( demux_t *p_demux, int *p_pos )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    uint8_t     *p_buf;
    int         i_size;
    int         i;
    char        *p_line;

    while( *p_pos > p_sys->i_data_peeked )
    {
        if( ! Peek( p_demux, VLC_FALSE ) )
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
            if( ! Peek( p_demux, VLC_FALSE ) )
            {
                return NULL;
            }
        }
        p_buf = p_sys->p_peek + *p_pos;
        i_size = p_sys->i_data_peeked - *p_pos;
    }
    *p_pos += ( i + 1 );
    if( i > 0 && '\r' == p_buf[i - 1] )
    {
        i--;
    }
    p_line = malloc( i + 1 );
    if( NULL == p_line )
    {
        msg_Err( p_demux, "out of memory" );
        return NULL;
    }
    strncpy ( p_line, p_buf, i );
    p_line[i] = '\0';
//    msg_Dbg( p_demux, "i = %d, pos = %d, %s", i, *p_pos, p_line );
    return p_line;
}

/*****************************************************************************
 * CheckMimeHeader: Internal function used to verify and skip mime header
 * \param p_header_size Return size of MIME header, 0 if no MIME header
 * detected, minus value if error
 * \return VLC_TRUE if content type is image/jpeg, VLC_FALSE otherwise
 *****************************************************************************/
static vlc_bool_t CheckMimeHeader( demux_t *p_demux, int *p_header_size )
{
    vlc_bool_t  b_jpeg = VLC_FALSE;
    int         i_pos;
    char        *psz_line;
    char        *p_ch;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( !Peek( p_demux, VLC_TRUE ) )
    {
        msg_Err( p_demux, "cannot peek" );
        *p_header_size = -1;
        return VLC_FALSE;
    }
    if( p_sys->i_data_peeked < 3)
    {
        msg_Err( p_demux, "data shortage" );
        *p_header_size = -2;
        return VLC_FALSE;
    }
    if( strncmp( (char *)p_sys->p_peek, "--", 2 ) )
    {
        *p_header_size = 0;
        return VLC_FALSE;
    }
    i_pos = 2;
    psz_line = GetLine( p_demux, &i_pos );
    if( NULL == psz_line )
    {
        msg_Err( p_demux, "no EOL" );
        *p_header_size = -3;
        return VLC_FALSE;
    }
    if( NULL == p_sys->psz_separator )
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
        free( psz_line );
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
                b_jpeg = VLC_FALSE;
            }
            else
            {
                b_jpeg = VLC_TRUE;
            }
        }
        else
        {
            msg_Dbg( p_demux, "Discard MIME header: %s", psz_line );
        }
        free( psz_line );
        psz_line = GetLine( p_demux, &i_pos );
    }

    if( NULL == psz_line )
    {
        msg_Err( p_demux, "no EOL" );
        *p_header_size = -3;
        return VLC_FALSE;
    }

    free( psz_line );

    *p_header_size = i_pos;
    return b_jpeg;
}

static int SendBlock( demux_t *p_demux, int i )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_block;

    if( ( p_block = stream_Block( p_demux->s, i ) ) == NULL )
    {
        msg_Warn( p_demux, "cannot read data" );
        return 0;
    }

    if( !p_sys->i_frame_length || !p_sys->i_time )
    {
        p_sys->i_time = p_block->i_dts = p_block->i_pts = mdate();
    }
    else
    {
        p_block->i_dts = p_block->i_pts = p_sys->i_time;
        p_sys->i_time += p_sys->i_frame_length;
    }

    /* set PCR */
    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block->i_pts );
    es_out_Send( p_demux->out, p_sys->p_es, p_block );

    if( p_sys->b_still )
    {
        p_sys->i_still_end = mdate() + p_sys->i_still_length;
    }

    return 1;
}

/*****************************************************************************
 * Open: check file and initializes structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    int         i_size;
    int         b_matched = VLC_FALSE;
    vlc_value_t val;
    char *psz_ext;

    p_demux->pf_control = Control;
    p_demux->p_sys      = p_sys = malloc( sizeof( demux_sys_t ) );
    p_sys->p_es         = NULL;
    p_sys->i_time       = 0;

    p_sys->psz_separator = NULL;
    p_sys->i_frame_size_estimate = 15 * 1024;

    b_matched = CheckMimeHeader( p_demux, &i_size);
    if( b_matched )
    {
        p_demux->pf_demux = MimeDemux;
        stream_Read( p_demux->s, NULL, i_size );
    }
    else if( 0 == i_size )
    {
        /* 0xffd8 identify a JPEG SOI */
        if( p_sys->p_peek[0] == 0xFF && p_sys->p_peek[1] == 0xD8 )
        {
            msg_Dbg( p_demux, "JPEG SOI marker detected" );
            p_demux->pf_demux = MjpgDemux;
        }
        else
        {
            goto error;
        }
    }
    else
    {
        goto error;
    }


    var_Create( p_demux, "mjpeg-fps", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "mjpeg-fps", &val );
    p_sys->i_frame_length = 0;

    /* Check for jpeg file extension */
    p_sys->b_still = VLC_FALSE;
    p_sys->i_still_end = 0;
    psz_ext = strrchr( p_demux->psz_path, '.' );
    if( psz_ext && ( !strcasecmp( psz_ext, ".jpeg" ) ||
                     !strcasecmp( psz_ext, ".jpg" ) ) )
    {
        p_sys->b_still = VLC_TRUE;
        if( val.f_float)
        {
            p_sys->i_still_length =1000000.0 / val.f_float;
        }
        else
        {
            /* Defaults to 1fps */
            p_sys->i_still_length = 1000000;
        }
    }
    else if ( val.f_float )
    {
        p_sys->i_frame_length = 1000000.0 / val.f_float;
    }

    es_format_Init( &p_sys->fmt, VIDEO_ES, 0 );
    p_sys->fmt.i_codec = VLC_FOURCC('m','j','p','g');

    p_sys->p_es = es_out_Add( p_demux->out, &p_sys->fmt );
    return VLC_SUCCESS;

error:
    free( p_sys );
    return VLC_EGENERIC;
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

    if( p_sys->b_still && p_sys->i_still_end && p_sys->i_still_end < mdate() )
    {
        /* Still frame, wait until the pause delay is gone */
        p_sys->i_still_end = 0;
    }
    else if( p_sys->b_still && p_sys->i_still_end )
    {
        msleep( 400 );
        return 1;
    }

    if( !Peek( p_demux, VLC_TRUE ) )
    {
        msg_Warn( p_demux, "cannot peek data" );
        return 0;
    }
    if( p_sys->i_data_peeked < 4 )
    {
        msg_Warn( p_demux, "data shortage" );
        return 0;
    }
    i = 3;
    while( !( 0xFF == p_sys->p_peek[i-1] && 0xD9 == p_sys->p_peek[i] ) )
    {
        i++;
        if( i >= p_sys->i_data_peeked )
        {
            msg_Dbg( p_demux, "Did not find JPEG EOI in %d bytes",
                     p_sys->i_data_peeked );
            if( !Peek( p_demux, VLC_FALSE ) )
            {
                msg_Warn( p_demux, "No more data is available at the moment" );
                return 0;
            }
        }
    }
    i++;

    msg_Dbg( p_demux, "JPEG EOI detected at %d", i );
    return SendBlock( p_demux, i );
}

static int MimeDemux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int         i_size, i;
    vlc_bool_t  b_match;
    vlc_bool_t  b_done;

    b_match = CheckMimeHeader( p_demux, &i_size );
    if( i_size > 0 )
    {
        stream_Read( p_demux->s, NULL, i_size );
    }
    else if( i_size < 0 )
    {
        return 0;
    }
    else
    {
        // No MIME header, assume OK
        b_match = VLC_TRUE;
    }

    if( !Peek( p_demux, VLC_TRUE ) )
    {
        msg_Warn( p_demux, "cannot peek data" );
        return 0;
    }
    i = 0;
    i_size = strlen( p_sys->psz_separator ) + 2;
    if( p_sys->i_data_peeked < i_size )
    {
        msg_Warn( p_demux, "data shortage" );
        return 0;
    }
    b_done = VLC_FALSE;
    while( !b_done )
    {
        while( !( p_sys->p_peek[i] == '-' && p_sys->p_peek[i+1] == '-' ) )
        {
            i++;
            i_size++;
            if( i_size >= p_sys->i_data_peeked )
            {
                msg_Dbg( p_demux, "MIME boundary not found in %d bytes of "
                         "data", p_sys->i_data_peeked );

                if( !Peek( p_demux, VLC_FALSE ) )
                {
                    msg_Warn( p_demux, "No more data is available at the "
                              "moment" );
                    return 0;
                }
            }
        }
        if( !strncmp( p_sys->psz_separator, (char *)(p_sys->p_peek + i + 2),
                      strlen( p_sys->psz_separator ) ) )
        {
            b_done = VLC_TRUE;
        }
        else
        {
            i++;
            i_size++;
        }
    }

    if( !b_match )
    {
        msg_Err( p_demux, "Discard non-JPEG part" );
        stream_Read( p_demux->s, NULL, i );
        return 0;
    }

    return SendBlock( p_demux, i );
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close ( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys  = p_demux->p_sys;

    if( p_sys->psz_separator )
    {
        free( p_sys->psz_separator );
    }
    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return demux2_vaControlHelper( p_demux->s, 0, 0, 0, 0, i_query, args );
}
