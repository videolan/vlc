/*****************************************************************************
 * subtitle.c: Demux vobsub files.
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan dot org>
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

#include <errno.h>
#include <sys/types.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include "vlc_video.h"

#include "ps.h"

#define MAX_LINE 8192

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t *p_this );
static void Close( vlc_object_t *p_this );

vlc_module_begin();
    set_description( _("Vobsub subtitles demux") );
    set_capability( "demux2", 0 );
    
    set_callbacks( Open, Close );

    add_shortcut( "vobsub" );
vlc_module_end();

/*****************************************************************************
 * Prototypes:
 *****************************************************************************/

typedef struct
{
    int     i_line_count;
    int     i_line;
    char    **line;
} text_t;
static int  TextLoad( text_t *, stream_t *s );
static void TextUnload( text_t * );

typedef struct
{
    mtime_t i_start;
    mtime_t i_stop;

    int     i_vobsub_location;
} subtitle_t;


struct demux_sys_t
{
    int         i_type;
    text_t      txt;
    es_out_id_t *es;

    int64_t     i_next_demux_date;

    int         i_subtitle;
    int         i_subtitles;
    subtitle_t  *subtitle;
    FILE        *p_vobsub_file;

    int64_t     i_length;
};

static int Demux( demux_t * );
static int Control( demux_t *, int, va_list );

static int  ParseVobSubIDX( demux_t *, subtitle_t * );
static int  DemuxVobSub( demux_t *f, block_t *);

/*****************************************************************************
 * Module initializer
 *****************************************************************************/
static int Open ( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    es_format_t fmt;
    int i_max;

    if( strcmp( p_demux->psz_demux, "vobsub" ) )
    {
        msg_Dbg( p_demux, "vobsub demux discarded" );
        return VLC_EGENERIC;
    }

    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    p_sys->i_subtitle = 0;
    p_sys->i_subtitles= 0;
    p_sys->subtitle   = NULL;
    p_sys->p_vobsub_file = NULL;

    char *s = NULL;
    if( ( s = stream_ReadLine( p_demux->s ) ) != NULL )
    {
        if( !strcasestr( s, "# VobSub index file" ) )
        {
            msg_Err( p_demux, "this doesn't seem to be a vobsub file, bailing" );
            free( s );
            return VLC_EGENERIC;
        }
        free( s );
        s = NULL;

        if( stream_Seek( p_demux->s, 0 ) )
        {
            msg_Warn( p_demux, "failed to rewind" );
        }
    }
    else
    {
        msg_Err( p_demux, "could not read vobsub IDX file" );
        return VLC_EGENERIC;
    }

    /* Load the whole file */
    TextLoad( &p_sys->txt, p_demux->s );

    /* Parse it */
    for( i_max = 0;; )
    {
        if( p_sys->i_subtitles >= i_max )
        {
            i_max += 500;
            if( !( p_sys->subtitle = realloc( p_sys->subtitle,
                                              sizeof(subtitle_t) * i_max ) ) )
            {
                msg_Err( p_demux, "out of memory");
                return VLC_ENOMEM;
            }
        }

        if( ParseVobSubIDX( p_demux, &p_sys->subtitle[p_sys->i_subtitles] ) )
            break;

        p_sys->i_subtitles++;
    }
    /* Unload */
    TextUnload( &p_sys->txt );

    msg_Dbg(p_demux, "loaded %d subtitles", p_sys->i_subtitles );

    /* Fix subtitle (order and time) *** */
    p_sys->i_subtitle = 0;
    p_sys->i_length = 0;
    if( p_sys->i_subtitles > 0 )
    {
        p_sys->i_length = p_sys->subtitle[p_sys->i_subtitles-1].i_stop;
        /* +1 to avoid 0 */
        if( p_sys->i_length <= 0 )
            p_sys->i_length = p_sys->subtitle[p_sys->i_subtitles-1].i_start+1;
    }

    int i_len = strlen( p_demux->psz_path );
    char *psz_vobname = strdup( p_demux->psz_path );

    strcpy( psz_vobname + i_len - 4, ".sub" );

    /* open file */
    if( !( p_sys->p_vobsub_file = fopen( psz_vobname, "rb" ) ) )
    {
        msg_Err( p_demux, "couldn't open .sub Vobsub file: %s",
                 psz_vobname );
    }
    free( psz_vobname );

    es_format_Init( &fmt, SPU_ES, VLC_FOURCC( 's','p','u',' ' ) );

    p_sys->es = es_out_Add( p_demux->out, &fmt );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: Close subtitle demux
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->subtitle )
        free( p_sys->subtitle );

    if( p_sys->p_vobsub_file )
        fclose( p_sys->p_vobsub_file );

    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int64_t *pi64, i64;
    double *pf, f;

    switch( i_query )
    {
        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_length;
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->i_subtitle < p_sys->i_subtitles )
            {
                *pi64 = p_sys->subtitle[p_sys->i_subtitle].i_start;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_SET_TIME:
            i64 = (int64_t)va_arg( args, int64_t );
            p_sys->i_subtitle = 0;
            while( p_sys->i_subtitle < p_sys->i_subtitles &&
                   p_sys->subtitle[p_sys->i_subtitle].i_start < i64 )
            {
                p_sys->i_subtitle++;
            }

            if( p_sys->i_subtitle >= p_sys->i_subtitles )
                return VLC_EGENERIC;
            return VLC_SUCCESS;

        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double * );
            if( p_sys->i_subtitle >= p_sys->i_subtitles )
            {
                *pf = 1.0;
            }
            else if( p_sys->i_subtitles > 0 )
            {
                *pf = (double)p_sys->subtitle[p_sys->i_subtitle].i_start /
                      (double)p_sys->i_length;
            }
            else
            {
                *pf = 0.0;
            }
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            f = (double)va_arg( args, double );
            i64 = f * p_sys->i_length;

            p_sys->i_subtitle = 0;
            while( p_sys->i_subtitle < p_sys->i_subtitles &&
                   p_sys->subtitle[p_sys->i_subtitle].i_start < i64 )
            {
                p_sys->i_subtitle++;
            }
            if( p_sys->i_subtitle >= p_sys->i_subtitles )
                return VLC_EGENERIC;
            return VLC_SUCCESS;

        case DEMUX_SET_NEXT_DEMUX_TIME:
            p_sys->i_next_demux_date = (int64_t)va_arg( args, int64_t );
            return VLC_SUCCESS;

        case DEMUX_GET_FPS:
        case DEMUX_GET_META:
        case DEMUX_GET_TITLE_INFO:
            return VLC_EGENERIC;

        default:
            msg_Err( p_demux, "unknown query in subtitle control" );
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Demux: Send subtitle to decoder
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int64_t i_maxdate;

    if( p_sys->i_subtitle >= p_sys->i_subtitles )
        return 0;

    i_maxdate = p_sys->i_next_demux_date;
    if( i_maxdate <= 0 && p_sys->i_subtitle < p_sys->i_subtitles )
    {
        /* Should not happen */
        i_maxdate = p_sys->subtitle[p_sys->i_subtitle].i_start + 1;
    }

      while( p_sys->i_subtitle < p_sys->i_subtitles &&
           p_sys->subtitle[p_sys->i_subtitle].i_start < i_maxdate )
    {
        int i_pos = p_sys->subtitle[p_sys->i_subtitle].i_vobsub_location;
        block_t *p_block;
        int i_size = 0;

        /* first compute SPU size */
        if( p_sys->i_subtitle + 1 < p_sys->i_subtitles )
        {
            i_size = p_sys->subtitle[p_sys->i_subtitle+1].i_vobsub_location - i_pos;
        }
        if( i_size <= 0 ) i_size = 65535;   /* Invalid or EOF */

        /* Seek at the right place */
        if( fseek( p_sys->p_vobsub_file, i_pos, SEEK_SET ) )
        {
            msg_Warn( p_demux,
                      "cannot seek at right vobsub location %d", i_pos );
            p_sys->i_subtitle++;
            continue;
        }

        /* allocate a packet */
        if( ( p_block = block_New( p_demux, i_size ) ) == NULL )
        {
            p_sys->i_subtitle++;
            continue;
        }

        /* read data */
        p_block->i_buffer = fread( p_block->p_buffer, 1, i_size,
                                   p_sys->p_vobsub_file );
        if( p_block->i_buffer <= 6 )
        {
            block_Release( p_block );
            p_sys->i_subtitle++;
            continue;
        }

        /* pts */
        p_block->i_pts = p_sys->subtitle[p_sys->i_subtitle].i_start;

        /* demux this block */
        DemuxVobSub( p_demux, p_block );

        p_sys->i_subtitle++;
    }

    /* */
    p_sys->i_next_demux_date = 0;

    return 1;
}

static int TextLoad( text_t *txt, stream_t *s )
{
    int   i_line_max;

    /* init txt */
    i_line_max          = 500;
    txt->i_line_count   = 0;
    txt->i_line         = 0;
    txt->line           = calloc( i_line_max, sizeof( char * ) );

    /* load the complete file */
    for( ;; )
    {
        char *psz = stream_ReadLine( s );

        if( psz == NULL )
            break;

        txt->line[txt->i_line_count++] = psz;
        if( txt->i_line_count >= i_line_max )
        {
            i_line_max += 100;
            txt->line = realloc( txt->line, i_line_max * sizeof( char*) );
        }
    }

    if( txt->i_line_count <= 0 )
    {
        free( txt->line );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}
static void TextUnload( text_t *txt )
{
    int i;

    for( i = 0; i < txt->i_line_count; i++ )
    {
        free( txt->line[i] );
    }
    free( txt->line );
    txt->i_line       = 0;
    txt->i_line_count = 0;
}

static char *TextGetLine( text_t *txt )
{
    if( txt->i_line >= txt->i_line_count )
        return( NULL );

    return txt->line[txt->i_line++];
}
static void TextPreviousLine( text_t *txt )
{
    if( txt->i_line > 0 )
        txt->i_line--;
}

static int  ParseVobSubIDX( demux_t *p_demux, subtitle_t *p_subtitle )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    text_t      *txt = &p_sys->txt;

    /*
     * Parse the idx file. Each line:
     * timestamp: hh:mm:ss:mss, filepos: loc
     * hexint is the hex location of the vobsub in the .sub file
     *
     */
    char *p;
    char buffer_text[MAX_LINE + 1];
    unsigned int    i_start, i_location;

    p_subtitle->i_start = 0;
    p_subtitle->i_stop  = 0;
    p_subtitle->i_vobsub_location  = 0;

    for( ;; )
    {
        unsigned int h, m, s, ms, loc;

        if( ( p = TextGetLine( txt ) ) == NULL )
        {
            return( VLC_EGENERIC );
        }
        i_start = 0;

        memset( buffer_text, '\0', MAX_LINE );
        if( sscanf( p, "timestamp: %d:%d:%d:%d, filepos: %x%[^\r\n]",
                    &h, &m, &s, &ms, &loc, buffer_text ) == 5 )
        {
            i_start = ( (mtime_t)h * 3600*1000 +
                        (mtime_t)m * 60*1000 +
                        (mtime_t)s * 1000 +
                        (mtime_t)ms ) * 1000;
            i_location = loc;
            break;
        }
    }
    p_subtitle->i_start = (mtime_t)i_start;
    p_subtitle->i_stop  = 0;
    p_subtitle->i_vobsub_location = i_location;
    fprintf( stderr, "time: %x, location: %x\n", i_start, i_location );
    return( 0 );
}

static int  DemuxVobSub( demux_t *p_demux, block_t *p_bk )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    uint8_t     *p = p_bk->p_buffer;
    uint8_t     *p_end = &p_bk->p_buffer[p_bk->i_buffer];

    while( p < p_end )
    {
        int i_size = ps_pkt_size( p, p_end - p );
        block_t *p_pkt;
        int      i_id;
        int      i_spu;

        if( i_size <= 0 )
        {
            break;
        }
        if( p[0] != 0 || p[1] != 0 || p[2] != 0x01 )
        {
            msg_Warn( p_demux, "invalid PES" );
            break;
        }

        if( p[3] != 0xbd )
        {
            msg_Dbg( p_demux, "we don't need these ps packets (id=0x1%2.2x)", p[3] );
            p += i_size;
            continue;
        }

        /* Create a block */
        p_pkt = block_New( p_demux, i_size );
        memcpy( p_pkt->p_buffer, p, i_size);
        p += i_size;

        i_id = ps_pkt_id( p_pkt );
        if( (i_id&0xffe0) != 0xbd20 ||
            ps_pkt_parse_pes( p_pkt, 1 ) )
        {
            block_Release( p_pkt );
            continue;
        }
        i_spu = i_id&0x1f;
        msg_Dbg( p_demux, "SPU track %d size %d", i_spu, i_size );

        /* FIXME i_spu == determines which of the spu tracks we will show. */
        if( p_sys->es && i_spu == 0 )
        {
            p_pkt->i_dts = p_pkt->i_pts = p_bk->i_pts;
            p_pkt->i_length = 0;
            es_out_Send( p_demux->out, p_sys->es, p_pkt );

            p_bk->i_pts = 0;    /* only first packet has a pts */
        }
        else
        {
            block_Release( p_pkt );
            continue;
        }
    }

    return VLC_SUCCESS;
}
