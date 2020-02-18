/*****************************************************************************
 * vobsub.c: Demux vobsub files.
 *****************************************************************************
 * Copyright (C) 1999-2004 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan dot org>
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

#include <limits.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>

#include "mpeg/pes.h"
#include "mpeg/ps.h"
#include "vobsub.h"
#include "subtitle_helper.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t *p_this );
static void Close( vlc_object_t *p_this );

vlc_module_begin ()
    set_description( N_("Vobsub subtitles parser") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_capability( "demux", 1 )

    set_callbacks( Open, Close )

    add_shortcut( "vobsub", "subtitle" )
vlc_module_end ()

/*****************************************************************************
 * Prototypes:
 *****************************************************************************/

typedef struct
{
    int     i_line_count;
    int     i_line;
    char    **line;
} text_t;

typedef struct
{
    vlc_tick_t i_start;
    int     i_vobsub_location;
} subtitle_t;

typedef struct
{
    es_out_id_t *p_es;
    int         i_track_id;

    int         i_current_subtitle;
    int         i_subtitles;
    subtitle_t  *p_subtitles;

    vlc_tick_t  i_delay;
} vobsub_track_t;

typedef struct
{
    vlc_tick_t     i_next_demux_date;
    vlc_tick_t     i_length;

    text_t         txt;
    stream_t       *p_vobsub_stream;

    /* all tracks */
    int            i_tracks;
    vobsub_track_t *track;

    int            i_original_frame_width;
    int            i_original_frame_height;
    bool           b_palette;
    uint32_t       palette[16];
} demux_sys_t;


static int Demux( demux_t * );
static int Control( demux_t *, int, va_list );

static int  TextLoad( text_t *, stream_t *s );
static void TextUnload( text_t * );
static int ParseVobSubIDX( demux_t * );
static int DemuxVobSub( demux_t *, block_t *);

/*****************************************************************************
 * Module initializer
 *****************************************************************************/
static int Open ( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    char *psz_vobname, *s;
    int i_len;
    uint64_t i_read_offset = 0;

    if( ( s = peek_Readline( p_demux->s, &i_read_offset ) ) != NULL )
    {
        if( !strcasestr( s, "# VobSub index file" ) )
        {
            msg_Dbg( p_demux, "this doesn't seem to be a vobsub file" );
            free( s );
            return VLC_EGENERIC;
        }
        free( s );
    }
    else
    {
        msg_Dbg( p_demux, "could not read vobsub IDX file" );
        return VLC_EGENERIC;
    }

    /* */
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    if( unlikely( !p_sys ) )
        return VLC_ENOMEM;

    p_sys->p_vobsub_stream = NULL;
    p_sys->i_tracks = 0;
    p_sys->track = malloc( sizeof( vobsub_track_t ) );
    if( unlikely( !p_sys->track ) )
        goto error;
    p_sys->i_original_frame_width = -1;
    p_sys->i_original_frame_height = -1;
    p_sys->b_palette = false;
    memset( p_sys->palette, 0, 16 * sizeof( uint32_t ) );

    /* Load the whole file */
    TextLoad( &p_sys->txt, p_demux->s );

    /* Parse it */
    ParseVobSubIDX( p_demux );

    /* Unload */
    TextUnload( &p_sys->txt );

    /* Find the total length of the vobsubs */
    p_sys->i_length = 0;
    for( int i = 0; i < p_sys->i_tracks; i++ )
    {
        if( !p_sys->track[i].i_subtitles )
            continue;
        if( p_sys->track[i].p_subtitles[p_sys->track[i].i_subtitles-1].i_start > p_sys->i_length )
            p_sys->i_length = p_sys->track[i].p_subtitles[p_sys->track[i].i_subtitles-1].i_start;
    }

    if ( p_sys->i_length != 0)
        p_sys->i_length += VLC_TICK_FROM_SEC( 1 );

    psz_vobname = strdup( p_demux->psz_url );
    if( psz_vobname == NULL )
        goto error;

    i_len = strlen( psz_vobname );
    if( i_len >= 4 ) memcpy( psz_vobname + i_len - 4, ".sub", 4 );

    /* open file */
    p_sys->p_vobsub_stream = vlc_stream_NewURL( p_demux, psz_vobname );
    if( p_sys->p_vobsub_stream == NULL )
    {
        msg_Err( p_demux, "couldn't open .sub Vobsub file: %s",
                 psz_vobname );
        free( psz_vobname );
        goto error;
    }
    free( psz_vobname );

    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    return VLC_SUCCESS;

error:
    /* Clean all subs from all tracks */
    for( int i = 0; i < p_sys->i_tracks; i++ )
        free( p_sys->track[i].p_subtitles );
    free( p_sys->track );
    free( p_sys );

    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: Close subtitle demux
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->p_vobsub_stream )
        vlc_stream_Delete( p_sys->p_vobsub_stream );

    /* Clean all subs from all tracks */
    for( int i = 0; i < p_sys->i_tracks; i++ )
        free( p_sys->track[i].p_subtitles );
    free( p_sys->track );
    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    vlc_tick_t i64;
    int i;
    double *pf, f;

    switch( i_query )
    {
        case DEMUX_CAN_SEEK:
            *va_arg( args, bool * ) = true;
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
            *va_arg( args, vlc_tick_t * ) = p_sys->i_length;
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            for( i = 0; i < p_sys->i_tracks; i++ )
            {
                bool b_selected;
                /* Check the ES is selected */
                es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE,
                                p_sys->track[i].p_es, &b_selected );
                if( b_selected ) break;
            }
            if( i < p_sys->i_tracks && p_sys->track[i].i_current_subtitle < p_sys->track[i].i_subtitles )
            {
                *va_arg( args, vlc_tick_t * ) = p_sys->track[i].p_subtitles[p_sys->track[i].i_current_subtitle].i_start;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_SET_TIME:
            i64 = va_arg( args, vlc_tick_t );
            for( i = 0; i < p_sys->i_tracks; i++ )
            {
                p_sys->track[i].i_current_subtitle = 0;
                while( p_sys->track[i].i_current_subtitle < p_sys->track[i].i_subtitles &&
                       p_sys->track[i].p_subtitles[p_sys->track[i].i_current_subtitle].i_start < i64 )
                {
                    p_sys->track[i].i_current_subtitle++;
                }

                if( p_sys->track[i].i_current_subtitle >= p_sys->track[i].i_subtitles )
                    return VLC_EGENERIC;
            }
            return VLC_SUCCESS;

        case DEMUX_GET_POSITION:
            pf = va_arg( args, double * );
            for( i = 0; i < p_sys->i_tracks; i++ )
            {
                bool b_selected;
                /* Check the ES is selected */
                es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE,
                                p_sys->track[i].p_es, &b_selected );
                if( b_selected ) break;
            }
            if (i >= p_sys->i_tracks) {
                /* No selected ES found */
                return VLC_EGENERIC;
            }
            if( p_sys->track[i].i_current_subtitle >= p_sys->track[i].i_subtitles )
            {
                *pf = 1.0;
            }
            else if( p_sys->track[i].i_subtitles > 0 )
            {
                *pf = (double)p_sys->track[i].p_subtitles[p_sys->track[i].i_current_subtitle].i_start /
                      (double)p_sys->i_length;
            }
            else
            {
                *pf = 0.0;
            }
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            f = va_arg( args, double );
            i64 = (vlc_tick_t) f * p_sys->i_length;
            return demux_Control(p_demux, DEMUX_SET_TIME, i64, false);

        case DEMUX_SET_NEXT_DEMUX_TIME:
            p_sys->i_next_demux_date = va_arg( args, vlc_tick_t );
            return VLC_SUCCESS;

        case DEMUX_CAN_PAUSE:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
            return demux_vaControlHelper( p_demux->s, 0, -1, 0, 1, i_query, args );

        case DEMUX_GET_PTS_DELAY:
        case DEMUX_GET_FPS:
        case DEMUX_GET_META:
        case DEMUX_GET_TITLE_INFO:
        case DEMUX_HAS_UNSUPPORTED_META:
        case DEMUX_GET_ATTACHMENTS:
        case DEMUX_CAN_RECORD:
            return VLC_EGENERIC;

        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Demux: Send subtitle to decoder
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    vlc_tick_t i_maxdate;

    for( int i = 0; i < p_sys->i_tracks; i++ )
    {
        vobsub_track_t *tk = &p_sys->track[i];

        if( tk->i_current_subtitle >= tk->i_subtitles )
            continue;

        i_maxdate = p_sys->i_next_demux_date;
        if( i_maxdate <= 0 && tk->i_current_subtitle < tk->i_subtitles )
        {
            /* Should not happen */
            i_maxdate = tk->p_subtitles[tk->i_current_subtitle].i_start + 1;
        }

        while( tk->i_current_subtitle < tk->i_subtitles &&
               tk->p_subtitles[tk->i_current_subtitle].i_start < i_maxdate )
        {
            int i_pos = tk->p_subtitles[tk->i_current_subtitle].i_vobsub_location;
            block_t *p_block;
            int i_size = 0;

            /* first compute SPU size */
            if( tk->i_current_subtitle + 1 < tk->i_subtitles )
            {
                i_size = tk->p_subtitles[tk->i_current_subtitle+1].i_vobsub_location - i_pos;
            }
            if( i_size <= 0 ) i_size = 65535;   /* Invalid or EOF */

            /* Seek at the right place */
            if( vlc_stream_Seek( p_sys->p_vobsub_stream, i_pos ) )
            {
                msg_Warn( p_demux,
                          "cannot seek in the VobSub to the correct time %d", i_pos );
                tk->i_current_subtitle++;
                continue;
            }

            /* read data */
            p_block = vlc_stream_Block( p_sys->p_vobsub_stream, i_size );
            if( p_block )
            {
                if( p_block->i_buffer > 6 )
                {
                    /* pts */
                    p_block->i_pts = VLC_TICK_0 + tk->p_subtitles[tk->i_current_subtitle].i_start;

                    /* demux this block */
                    DemuxVobSub( p_demux, p_block );
                }
                block_Release( p_block );
            }

            tk->i_current_subtitle++;
        }
    }

    /* */
    p_sys->i_next_demux_date = 0;

    return 1;
}

static int TextLoad( text_t *txt, stream_t *s )
{
    char **lines = NULL;
    size_t n = 0;

    /* load the complete file */
    for( ;; )
    {
        char *psz = vlc_stream_ReadLine( s );
        char **ppsz_new;

        if( psz == NULL || (n >= INT_MAX/sizeof(char *)) )
        {
            free( psz );
            break;
        }

        ppsz_new = realloc( lines, (n + 1) * sizeof (char *) );
        if( ppsz_new == NULL )
        {
            free( psz );
            break;
        }
        lines = ppsz_new;
        lines[n++] = psz;
    }

    txt->i_line_count = n;
    txt->i_line       = 0;
    txt->line         = lines;

    return VLC_SUCCESS;
}

static void TextUnload( text_t *txt )
{
    for( int i = 0; i < txt->i_line_count; i++ )
        free( txt->line[i] );
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

static int ParseVobSubIDX( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    text_t      *txt = &p_sys->txt;
    char        *line;

    for( ;; )
    {
        if( ( line = TextGetLine( txt ) ) == NULL )
        {
            return( VLC_EGENERIC );
        }

        if( *line == 0 || *line == '\r' || *line == '\n' || *line == '#' )
        {
            continue;
        }
        else if( !strncmp( "size:", line, 5 ) )
        {
            /* Store the original size of the video */
            if( vobsub_size_parse( line, &p_sys->i_original_frame_width,
                                   &p_sys->i_original_frame_height ) == VLC_SUCCESS )
            {
                msg_Dbg( p_demux, "original frame size: %dx%d", p_sys->i_original_frame_width, p_sys->i_original_frame_height );
            }
            else
            {
                msg_Warn( p_demux, "reading original frame size failed" );
            }
        }
        else if( !strncmp( "palette:", line, 8 ) )
        {
            if( vobsub_palette_parse( line, p_sys->palette ) == VLC_SUCCESS )
            {
                p_sys->b_palette = true;
                msg_Dbg( p_demux, "vobsub palette read" );
            }
            else
            {
                msg_Warn( p_demux, "reading original palette failed" );
            }
        }
        else if( !strncmp( "id:", line, 3 ) )
        {
            char language[33]; /* Usually 2 or 3 letters, sometimes more.
                                  Spec (or lack of) doesn't define any limit */
            int i_track_id;
            es_format_t fmt;

            /* Lets start a new track */
            if( sscanf( line, "id: %32[^ ,], index: %d",
                        language, &i_track_id ) != 2 )
            {
                if( sscanf( line, "id: , index: %d", &i_track_id ) != 1 )
                {
                    msg_Warn( p_demux, "reading new track failed" );
                    continue;
                }
                language[0] = '\0';
            }

            vobsub_track_t *p_realloc = vlc_reallocarray( p_sys->track,
                                                          p_sys->i_tracks + 1,
                                                          sizeof(*p_realloc) );
            if( !p_realloc )
                return VLC_EGENERIC;
            p_sys->track = p_realloc;
            p_sys->i_tracks++;

            /* Init the track */
            vobsub_track_t *current_tk = &p_sys->track[p_sys->i_tracks - 1];
            memset( current_tk, 0, sizeof( vobsub_track_t ) );
            current_tk->i_current_subtitle = 0;
            current_tk->i_subtitles = 0;
            current_tk->p_subtitles = NULL;
            current_tk->i_track_id = i_track_id;
            current_tk->i_delay = 0;

            es_format_Init( &fmt, SPU_ES, VLC_CODEC_SPU );
            fmt.subs.spu.i_original_frame_width = p_sys->i_original_frame_width;
            fmt.subs.spu.i_original_frame_height = p_sys->i_original_frame_height;
            fmt.psz_language = language;
            if( p_sys->b_palette )
            {
                fmt.subs.spu.palette[0] = SPU_PALETTE_DEFINED;
                memcpy( &fmt.subs.spu.palette[1], p_sys->palette, 16 * sizeof( uint32_t ) );
            }

            fmt.i_id = i_track_id;
            current_tk->p_es = es_out_Add( p_demux->out, &fmt );
            msg_Dbg( p_demux, "New vobsub track detected: %i [%s]", i_track_id, language );
        }
        else if( !strncmp( line, "timestamp:", 10 ) )
        {
            /*
             * timestamp: [sign]hh:mm:ss:mss, filepos: loc
             * loc is the hex location of the spu in the .sub file
             */
            int h, m, s, ms;
            int64_t loc = 0;
            vlc_tick_t i_start;
            int64_t i_location = 0;

            if( p_sys->i_tracks > 0 &&
                sscanf( line, "timestamp: %02d:%02d:%02d:%03d, filepos: %"SCNx64,
                        &h, &m, &s, &ms, &loc ) == 5  )
            {
                vobsub_track_t *current_tk = &p_sys->track[p_sys->i_tracks - 1];
                subtitle_t *current_sub;

                i_start = vlc_tick_from_sec( h * 3600 + m * 60 + s ) + VLC_TICK_FROM_MS( ms );
                i_location = loc;

                subtitle_t *p_realloc = vlc_reallocarray( current_tk->p_subtitles,
                                                          current_tk->i_subtitles + 1,
                                                          sizeof(*p_realloc) );
                if( p_realloc )
                {
                    current_tk->p_subtitles = p_realloc;
                    current_sub = &p_realloc[current_tk->i_subtitles++];
                    current_sub->i_start = i_start;
                    current_sub->i_start += current_tk->i_delay;
                    current_sub->i_vobsub_location = i_location;
                }
            }
            else
            {
                msg_Warn( p_demux, "reading timestamp failed" );
            }
        }
        else if( !strncasecmp( line, "delay:", 6 ) )
        {
            /*
             * delay: [sign]hh:mm:ss:mss
             */
            int h, m, s, ms, count = 0;
            int i_sign = 1;
            vlc_tick_t i_gap = 0;

            if( p_sys->i_tracks > 0 &&
                sscanf( line, "%*celay: %02d%n:%02d:%02d:%03d",
                        &h, &count, &m, &s, &ms ) >= 4 )
            {
                vobsub_track_t *current_tk = &p_sys->track[p_sys->i_tracks - 1];
                if( line[count-3] == '-' )
                {
                    i_sign = -1;
                    h = -h;
                }
                i_gap = vlc_tick_from_sec( h * 3600 + m * 60 + s ) + VLC_TICK_FROM_MS( ms );

                current_tk->i_delay += i_gap * i_sign;
                msg_Dbg( p_demux, "sign: %+d gap: %+"PRId64" global delay: %+"PRId64"",
                         i_sign, i_gap, current_tk->i_delay );
            }
            else
            {
                msg_Warn( p_demux, "reading delay failed" );
            }
        }
    }
    return( 0 );
}

static int DemuxVobSub( demux_t *p_demux, block_t *p_bk )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    uint8_t     *p = p_bk->p_buffer;
    uint8_t     *p_end = &p_bk->p_buffer[p_bk->i_buffer];
    int i;

    while( p + 6 < p_end )
    {
        int i_size = ps_pkt_size( p, p_end - p );
        block_t *p_pkt;
        int      i_id = 0;
        int      i_spu;

        if( i_size <= 0 )
            break;

        if( i_size > p_end - p )
        {
            msg_Warn( p_demux, "broken PES size" );
            break;
        }

        if( p[0] != 0 || p[1] != 0 || p[2] != 0x01 )
        {
            msg_Warn( p_demux, "invalid PES" );
            break;
        }

        i_id = ps_pkt_id( p, i_size );
        if( (i_id&0xffe0) != 0xbd20 )
        {
            /* msg_Dbg( p_demux, "we don't need these ps packets (id=0x1%2.2x)", p[3] ); */
            p += i_size;
            continue;
        }

        /* Create a block */
        p_pkt = block_Alloc( i_size );
        if( unlikely(p_pkt == NULL) )
            break;
        memcpy( p_pkt->p_buffer, p, i_size);
        p += i_size;

        if( ps_pkt_parse_pes( VLC_OBJECT(p_demux), p_pkt, 1 ) )
        {
            block_Release( p_pkt );
            continue;
        }
        i_spu = i_id&0x1f;
        /* msg_Dbg( p_demux, "SPU track %d size %d", i_spu, i_size ); */

        for( i = 0; i < p_sys->i_tracks; i++ )
        {
            vobsub_track_t *p_tk = &p_sys->track[i];

            p_pkt->i_dts = p_pkt->i_pts = p_bk->i_pts;
            p_pkt->i_length = 0;

            if( p_tk->p_es && p_tk->i_track_id == i_spu )
            {
                es_out_Send( p_demux->out, p_tk->p_es, p_pkt );
                p_bk->i_pts = VLC_TICK_INVALID;     /*only first packet has a pts */
                break;
            }
        }
        if( i >= p_sys->i_tracks )
        {
            block_Release( p_pkt );
        }
    }

    return VLC_SUCCESS;
}

