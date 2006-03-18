/*****************************************************************************
 * subtitle.c: Demux for subtitle text files.
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>

#include <errno.h>
#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif
#include <ctype.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include "vlc_video.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t *p_this );
static void Close( vlc_object_t *p_this );

#define SUB_DELAY_LONGTEXT \
    "Apply a delay to all subtitles (in 1/10s, eg 100 means 10s)."
#define SUB_FPS_LONGTEXT \
    "Override the normal frames per second settings. " \
    "This will only work with MicroDVD and SubRIP (SRT) subtitles."
#define SUB_TYPE_LONGTEXT \
    "Force the subtiles format. Valid values are : \"microdvd\", \"subrip\"," \
    "\"ssa1\", \"ssa2-4\", \"ass\", \"vplayer\" " \
    "\"sami\", \"dvdsubtitle\" and \"auto\" (meaning autodetection, this " \
    "should always work)."
static char *ppsz_sub_type[] =
{
    "auto", "microdvd", "subrip", "subviewer", "ssa1",
    "ssa2-4", "ass", "vplayer", "sami", "dvdsubtitle"
};

vlc_module_begin();
    set_shortname( _("Subtitles"));
    set_description( _("Text subtitles parser") );
    set_capability( "demux2", 0 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    add_float( "sub-fps", 0.0, NULL,
               N_("Frames per second"),
               SUB_FPS_LONGTEXT, VLC_TRUE );
    add_integer( "sub-delay", 0, NULL,
               N_("Subtitles delay"),
               SUB_DELAY_LONGTEXT, VLC_TRUE );
    add_string( "sub-type", "auto", NULL, N_("Subtitles format"),
                SUB_TYPE_LONGTEXT, VLC_TRUE );
        change_string_list( ppsz_sub_type, 0, 0 );
    set_callbacks( Open, Close );

    add_shortcut( "subtitle" );
vlc_module_end();

/*****************************************************************************
 * Prototypes:
 *****************************************************************************/
enum
{
    SUB_TYPE_UNKNOWN = -1,
    SUB_TYPE_MICRODVD,
    SUB_TYPE_SUBRIP,
    SUB_TYPE_SSA1,
    SUB_TYPE_SSA2_4,
    SUB_TYPE_ASS,
    SUB_TYPE_VPLAYER,
    SUB_TYPE_SAMI,
    SUB_TYPE_SUBVIEWER,
    SUB_TYPE_DVDSUBTITLE
};

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
    int64_t i_start;
    int64_t i_stop;

    char    *psz_text;
} subtitle_t;


struct demux_sys_t
{
    int         i_type;
    text_t      txt;
    es_out_id_t *es;

    int64_t     i_next_demux_date;

    int64_t     i_microsecperframe;
    int64_t     i_original_mspf;

    char        *psz_header;
    int         i_subtitle;
    int         i_subtitles;
    subtitle_t  *subtitle;

    int64_t     i_length;
};

static int  ParseMicroDvd   ( demux_t *, subtitle_t * );
static int  ParseSubRip     ( demux_t *, subtitle_t * );
static int  ParseSubViewer  ( demux_t *, subtitle_t * );
static int  ParseSSA        ( demux_t *, subtitle_t * );
static int  ParseVplayer    ( demux_t *, subtitle_t * );
static int  ParseSami       ( demux_t *, subtitle_t * );
static int  ParseDVDSubtitle( demux_t *, subtitle_t * );

static struct
{
    char *psz_type_name;
    int  i_type;
    char *psz_name;
    int  (*pf_read)( demux_t *, subtitle_t* );
} sub_read_subtitle_function [] =
{
    { "microdvd",   SUB_TYPE_MICRODVD,    "MicroDVD",    ParseMicroDvd },
    { "subrip",     SUB_TYPE_SUBRIP,      "SubRIP",      ParseSubRip },
    { "subviewer",  SUB_TYPE_SUBVIEWER,   "SubViewer",   ParseSubViewer },
    { "ssa1",       SUB_TYPE_SSA1,        "SSA-1",       ParseSSA },
    { "ssa2-4",     SUB_TYPE_SSA2_4,      "SSA-2/3/4",   ParseSSA },
    { "ass",        SUB_TYPE_ASS,         "SSA/ASS",     ParseSSA },
    { "vplayer",    SUB_TYPE_VPLAYER,     "VPlayer",     ParseVplayer },
    { "sami",       SUB_TYPE_SAMI,        "SAMI",        ParseSami },
    { "dvdsubtitle",SUB_TYPE_DVDSUBTITLE, "DVDSubtitle", ParseDVDSubtitle },
    { NULL,         SUB_TYPE_UNKNOWN,     "Unknown",     NULL }
};

static int Demux( demux_t * );
static int Control( demux_t *, int, va_list );

/*static void Fix( demux_t * );*/

/*****************************************************************************
 * Module initializer
 *****************************************************************************/
static int Open ( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    es_format_t fmt;
    float f_fps;
    char *psz_type;
    int  (*pf_read)( demux_t *, subtitle_t* );
    int i, i_max;

    if( strcmp( p_demux->psz_demux, "subtitle" ) )
    {
        msg_Dbg( p_demux, "subtitle demux discarded" );
        return VLC_EGENERIC;
    }

    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    p_sys->psz_header = NULL;
    p_sys->i_subtitle = 0;
    p_sys->i_subtitles= 0;
    p_sys->subtitle   = NULL;


    /* Get the FPS */
    f_fps = var_CreateGetFloat( p_demux, "sub-fps" );
    if( f_fps >= 1.0 )
    {
        p_sys->i_microsecperframe = (int64_t)( (float)1000000 / f_fps );
    }
    else
    {
        p_sys->i_microsecperframe = 0;
    }

    f_fps = var_CreateGetFloat( p_demux, "sub-original-fps" );
    if( f_fps >= 1.0 )
    {
        p_sys->i_original_mspf = (int64_t)( (float)1000000 / f_fps );
    }
    else
    {
        p_sys->i_original_mspf = 0;
    }

    /* Get or probe the type */
    p_sys->i_type = SUB_TYPE_UNKNOWN;
    psz_type = var_CreateGetString( p_demux, "sub-type" );
    if( *psz_type )
    {
        int i;

        for( i = 0; ; i++ )
        {
            if( sub_read_subtitle_function[i].psz_type_name == NULL )
                break;

            if( !strcmp( sub_read_subtitle_function[i].psz_type_name,
                         psz_type ) )
            {
                p_sys->i_type = sub_read_subtitle_function[i].i_type;
                break;
            }
        }
    }
    free( psz_type );

    /* Probe if unknown type */
    if( p_sys->i_type == SUB_TYPE_UNKNOWN )
    {
        int     i_try;
        char    *s = NULL;

        msg_Dbg( p_demux, "autodetecting subtitle format" );
        for( i_try = 0; i_try < 256; i_try++ )
        {
            int i_dummy;

            if( ( s = stream_ReadLine( p_demux->s ) ) == NULL )
                break;

            if( strcasestr( s, "<SAMI>" ) )
            {
                p_sys->i_type = SUB_TYPE_SAMI;
                break;
            }
            else if( sscanf( s, "{%d}{%d}", &i_dummy, &i_dummy ) == 2 ||
                     sscanf( s, "{%d}{}", &i_dummy ) == 1)
            {
                p_sys->i_type = SUB_TYPE_MICRODVD;
                break;
            }
            else if( sscanf( s,
                             "%d:%d:%d,%d --> %d:%d:%d,%d",
                             &i_dummy,&i_dummy,&i_dummy,&i_dummy,
                             &i_dummy,&i_dummy,&i_dummy,&i_dummy ) == 8 )
            {
                p_sys->i_type = SUB_TYPE_SUBRIP;
                break;
            }
            else if( !strncasecmp( s, "!: This is a Sub Station Alpha v1", 33 ) )
            {
                p_sys->i_type = SUB_TYPE_SSA1;
                break;
            }
            else if( !strncasecmp( s, "ScriptType: v4.00+", 18 ) )
            {
                p_sys->i_type = SUB_TYPE_ASS;
                break;
            }
            else if( !strncasecmp( s, "ScriptType: v4.00", 17 ) )
            {
                p_sys->i_type = SUB_TYPE_SSA2_4;
                break;
            }
            else if( !strncasecmp( s, "Dialogue: Marked", 16  ) )
            {
                p_sys->i_type = SUB_TYPE_SSA2_4;
                break;
            }
            else if( !strncasecmp( s, "Dialogue:", 9  ) )
            {
                p_sys->i_type = SUB_TYPE_ASS;
                break;
            }
            else if( strcasestr( s, "[INFORMATION]" ) )
            {
                p_sys->i_type = SUB_TYPE_SUBVIEWER; /* I hope this will work */
                break;
            }
            else if( sscanf( s, "%d:%d:%d:", &i_dummy, &i_dummy, &i_dummy ) == 3 ||
                     sscanf( s, "%d:%d:%d ", &i_dummy, &i_dummy, &i_dummy ) == 3 )
            {
                p_sys->i_type = SUB_TYPE_VPLAYER;
                break;
            }
            else if( sscanf( s, "{T %d:%d:%d:%d", &i_dummy, &i_dummy,
                             &i_dummy, &i_dummy ) == 4 )
            {
                p_sys->i_type = SUB_TYPE_DVDSUBTITLE;
                break;
            }

            free( s );
            s = NULL;
        }

        if( s ) free( s );

        /* It will nearly always work even for non seekable stream thanks the
         * caching system, and if it fails we loose just a few sub */
        if( stream_Seek( p_demux->s, 0 ) )
        {
            msg_Warn( p_demux, "failed to rewind" );
        }
    }
    if( p_sys->i_type == SUB_TYPE_UNKNOWN )
    {
        msg_Err( p_demux, "failed to recognize subtitle type" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    for( i = 0; ; i++ )
    {
        if( sub_read_subtitle_function[i].i_type == p_sys->i_type )
        {
            msg_Dbg( p_demux, "detected %s format",
                     sub_read_subtitle_function[i].psz_name );
            pf_read = sub_read_subtitle_function[i].pf_read;
            break;
        }
    }

    msg_Dbg( p_demux, "loading all subtitles..." );

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
                if( p_sys->subtitle != NULL )
                    free( p_sys->subtitle );
                TextUnload( &p_sys->txt );
                free( p_sys );
                return VLC_ENOMEM;
            }
        }

        if( pf_read( p_demux, &p_sys->subtitle[p_sys->i_subtitles] ) )
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

    /* *** add subtitle ES *** */
    if( p_sys->i_type == SUB_TYPE_SSA1 ||
             p_sys->i_type == SUB_TYPE_SSA2_4 ||
             p_sys->i_type == SUB_TYPE_ASS )
    {
        es_format_Init( &fmt, SPU_ES, VLC_FOURCC( 's','s','a',' ' ) );
    }
    else
    {
        es_format_Init( &fmt, SPU_ES, VLC_FOURCC( 's','u','b','t' ) );
    }
    if( p_sys->psz_header != NULL )
    {
        fmt.i_extra = strlen( p_sys->psz_header ) + 1;
        fmt.p_extra = strdup( p_sys->psz_header );
    }
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
    int i;

    for( i = 0; i < p_sys->i_subtitles; i++ )
    {
        if( p_sys->subtitle[i].psz_text )
            free( p_sys->subtitle[i].psz_text );
    }
    if( p_sys->subtitle )
        free( p_sys->subtitle );

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

    i_maxdate = p_sys->i_next_demux_date - var_GetTime( p_demux->p_parent, "spu-delay" );;
    if( i_maxdate <= 0 && p_sys->i_subtitle < p_sys->i_subtitles )
    {
        /* Should not happen */
        i_maxdate = p_sys->subtitle[p_sys->i_subtitle].i_start + 1;
    }

    while( p_sys->i_subtitle < p_sys->i_subtitles &&
           p_sys->subtitle[p_sys->i_subtitle].i_start < i_maxdate )
    {
        block_t *p_block;
        int i_len = strlen( p_sys->subtitle[p_sys->i_subtitle].psz_text ) + 1;

        if( i_len <= 1 )
        {
            /* empty subtitle */
            p_sys->i_subtitle++;
            continue;
        }

        if( ( p_block = block_New( p_demux, i_len ) ) == NULL )
        {
            p_sys->i_subtitle++;
            continue;
        }

        if( p_sys->subtitle[p_sys->i_subtitle].i_start < 0 )
        {
            p_sys->i_subtitle++;
            continue;
        }

        p_block->i_pts = p_sys->subtitle[p_sys->i_subtitle].i_start;
        p_block->i_dts = p_block->i_pts;
        if( p_sys->subtitle[p_sys->i_subtitle].i_stop > 0 )
        {
            p_block->i_length =
                p_sys->subtitle[p_sys->i_subtitle].i_stop - p_block->i_pts;
        }

        memcpy( p_block->p_buffer,
                p_sys->subtitle[p_sys->i_subtitle].psz_text, i_len );
        if( p_block->i_pts > 0 )
        {
            es_out_Send( p_demux->out, p_sys->es, p_block );
        }
        else
        {
            block_Release( p_block );
        }
        p_sys->i_subtitle++;
    }

    /* */
    p_sys->i_next_demux_date = 0;

    return 1;
}

/*****************************************************************************
 * Fix: fix time stamp and order of subtitle
 *****************************************************************************/
#ifdef USE_THIS_UNUSED_PIECE_OF_CODE
static void Fix( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    vlc_bool_t b_done;
    int     i_index;

    /* *** fix order (to be sure...) *** */
    /* We suppose that there are near in order and this durty bubble sort
     * wont take too much time
     */
    do
    {
        b_done = VLC_TRUE;
        for( i_index = 1; i_index < p_sys->i_subtitles; i_index++ )
        {
            if( p_sys->subtitle[i_index].i_start <
                    p_sys->subtitle[i_index - 1].i_start )
            {
                subtitle_t sub_xch;
                memcpy( &sub_xch,
                        p_sys->subtitle + i_index - 1,
                        sizeof( subtitle_t ) );
                memcpy( p_sys->subtitle + i_index - 1,
                        p_sys->subtitle + i_index,
                        sizeof( subtitle_t ) );
                memcpy( p_sys->subtitle + i_index,
                        &sub_xch,
                        sizeof( subtitle_t ) );
                b_done = VLC_FALSE;
            }
        }
    } while( !b_done );
}
#endif

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
            txt->line = realloc( txt->line, i_line_max * sizeof( char * ) );
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

/*****************************************************************************
 * Specific Subtitle function
 *****************************************************************************/
#define MAX_LINE 8192
static int ParseMicroDvd( demux_t *p_demux, subtitle_t *p_subtitle )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    text_t      *txt = &p_sys->txt;
    /*
     * each line:
     *  {n1}{n2}Line1|Line2|Line3....
     * where n1 and n2 are the video frame number...
     * {n2} can also be {}
     */
    char *s;

    char buffer_text[MAX_LINE + 1];
    int    i_start;
    int    i_stop;
    unsigned int i;

    int i_microsecperframe = 40000; /* default to 25 fps */
    if( p_sys->i_microsecperframe > 0 )
        i_microsecperframe = p_sys->i_microsecperframe;

    p_subtitle->i_start = 0;
    p_subtitle->i_stop  = 0;
    p_subtitle->psz_text = NULL;

    for( ;; )
    {
        if( ( s = TextGetLine( txt ) ) == NULL )
        {
            return( VLC_EGENERIC );
        }
        i_start = 0;
        i_stop  = 0;

        memset( buffer_text, '\0', MAX_LINE );
        if( sscanf( s, "{%d}{}%[^\r\n]", &i_start, buffer_text ) == 2 ||
            sscanf( s, "{%d}{%d}%[^\r\n]", &i_start, &i_stop, buffer_text ) == 3)
        {
            break;
        }
    }
    /* replace | by \n */
    for( i = 0; i < strlen( buffer_text ); i++ )
    {
        if( buffer_text[i] == '|' )
        {
            buffer_text[i] = '\n';
        }
    }

    p_subtitle->i_start = (int64_t)i_start * i_microsecperframe;
    p_subtitle->i_stop  = (int64_t)i_stop  * i_microsecperframe;
    p_subtitle->psz_text = strndup( buffer_text, MAX_LINE );
    return( 0 );
}

static int  ParseSubRip( demux_t *p_demux, subtitle_t *p_subtitle )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    text_t      *txt = &p_sys->txt;

    /*
     * n
     * h1:m1:s1,d1 --> h2:m2:s2,d2
     * Line1
     * Line2
     * ...
     * [empty line]
     *
     */
    char *s;
    char buffer_text[ 10 * MAX_LINE];
    int  i_buffer_text;
    int64_t     i_start;
    int64_t     i_stop;

    p_subtitle->i_start = 0;
    p_subtitle->i_stop  = 0;
    p_subtitle->psz_text = NULL;

    for( ;; )
    {
        int h1, m1, s1, d1, h2, m2, s2, d2;
        if( ( s = TextGetLine( txt ) ) == NULL )
        {
            return( VLC_EGENERIC );
        }
        if( sscanf( s,
                    "%d:%d:%d,%d --> %d:%d:%d,%d",
                    &h1, &m1, &s1, &d1,
                    &h2, &m2, &s2, &d2 ) == 8 )
        {
            i_start = ( (int64_t)h1 * 3600*1000 +
                        (int64_t)m1 * 60*1000 +
                        (int64_t)s1 * 1000 +
                        (int64_t)d1 ) * 1000;

            i_stop  = ( (int64_t)h2 * 3600*1000 +
                        (int64_t)m2 * 60*1000 +
                        (int64_t)s2 * 1000 +
                        (int64_t)d2 ) * 1000;

            /* Now read text until an empty line */
            for( i_buffer_text = 0;; )
            {
                int i_len;
                if( ( s = TextGetLine( txt ) ) == NULL )
                {
                    return( VLC_EGENERIC );
                }

                i_len = strlen( s );
                if( i_len <= 0 )
                {
                    /* empty line -> end of this subtitle */
                    buffer_text[__MAX( i_buffer_text - 1, 0 )] = '\0';
                    p_subtitle->i_start = i_start;
                    p_subtitle->i_stop = i_stop;
                    p_subtitle->psz_text = strdup( buffer_text );
                    /* If framerate is available, use sub-fps */
                    if( p_sys->i_microsecperframe != 0 &&
                        p_sys->i_original_mspf != 0)
                    {
                        p_subtitle->i_start = (int64_t)i_start *
                                              p_sys->i_microsecperframe/
                                              p_sys->i_original_mspf;
                        p_subtitle->i_stop  = (int64_t)i_stop  *
                                              p_sys->i_microsecperframe /
                                              p_sys->i_original_mspf;
                    }
                    return 0;
                }
                else
                {
                    if( i_buffer_text + i_len + 1 < 10 * MAX_LINE )
                    {
                        memcpy( buffer_text + i_buffer_text,
                                s,
                                i_len );
                        i_buffer_text += i_len;

                        buffer_text[i_buffer_text] = '\n';
                        i_buffer_text++;
                    }
                }
            }
        }
    }
}

static int  ParseSubViewer( demux_t *p_demux, subtitle_t *p_subtitle )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    text_t      *txt = &p_sys->txt;

    /*
     * h1:m1:s1.d1,h2:m2:s2.d2
     * Line1[br]Line2
     * Line3
     * ...
     * [empty line]
     * ( works with subviewer and subviewer v2 )
     */
    char *s;
    char buffer_text[ 10 * MAX_LINE];
    int  i_buffer_text;
    int64_t     i_start;
    int64_t     i_stop;

    p_subtitle->i_start = 0;
    p_subtitle->i_stop  = 0;
    p_subtitle->psz_text = NULL;

    for( ;; )
    {
        int h1, m1, s1, d1, h2, m2, s2, d2;
        if( ( s = TextGetLine( txt ) ) == NULL )
        {
            return( VLC_EGENERIC );
        }
        if( sscanf( s,
                    "%d:%d:%d.%d,%d:%d:%d.%d",
                    &h1, &m1, &s1, &d1,
                    &h2, &m2, &s2, &d2 ) == 8 )
        {
            i_start = ( (int64_t)h1 * 3600*1000 +
                        (int64_t)m1 * 60*1000 +
                        (int64_t)s1 * 1000 +
                        (int64_t)d1 ) * 1000;

            i_stop  = ( (int64_t)h2 * 3600*1000 +
                        (int64_t)m2 * 60*1000 +
                        (int64_t)s2 * 1000 +
                        (int64_t)d2 ) * 1000;

            /* Now read text until an empty line */
            for( i_buffer_text = 0;; )
            {
                int i_len, i;
                if( ( s = TextGetLine( txt ) ) == NULL )
                {
                    return( VLC_EGENERIC );
                }

                i_len = strlen( s );
                if( i_len <= 0 )
                {
                    /* empty line -> end of this subtitle */
                    buffer_text[__MAX( i_buffer_text - 1, 0 )] = '\0';
                    p_subtitle->i_start = i_start;
                    p_subtitle->i_stop = i_stop;

                    /* replace [br] by \n */
                    for( i = 0; i < i_buffer_text - 3; i++ )
                    {
                        if( buffer_text[i] == '[' && buffer_text[i+1] == 'b' &&
                            buffer_text[i+2] == 'r' && buffer_text[i+3] == ']' )
                        {
                            char *temp = buffer_text + i + 1;
                            buffer_text[i] = '\n';
                            memmove( temp, temp+3, strlen( temp ) -3 );
                            temp[strlen( temp )-3] = '\0';
                        }
                    }
                    p_subtitle->psz_text = strdup( buffer_text );
                    return( 0 );
                }
                else
                {
                    if( i_buffer_text + i_len + 1 < 10 * MAX_LINE )
                    {
                        memcpy( buffer_text + i_buffer_text,
                                s,
                                i_len );
                        i_buffer_text += i_len;

                        buffer_text[i_buffer_text] = '\n';
                        i_buffer_text++;
                    }
                }
            }
        }
    }
}


static int  ParseSSA( demux_t *p_demux, subtitle_t *p_subtitle )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    text_t      *txt = &p_sys->txt;

    char buffer_text[ 10 * MAX_LINE];
    char buffer_text2[ 10 * MAX_LINE];
    char *s;
    int64_t     i_start;
    int64_t     i_stop;

    p_subtitle->i_start = 0;
    p_subtitle->i_stop  = 0;
    p_subtitle->psz_text = NULL;

    for( ;; )
    {
        int h1, m1, s1, c1, h2, m2, s2, c2;

        if( ( s = TextGetLine( txt ) ) == NULL )
        {
            return( VLC_EGENERIC );
        }
        p_subtitle->psz_text = malloc( strlen( s ) );

        /* We expect (SSA2-4):
         * Format: Marked, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
         * Dialogue: Marked=0,0:02:40.65,0:02:41.79,Wolf main,Cher,0000,0000,0000,,Et les enregistrements de ses ondes delta ?
         *
         * SSA-1 is similar but only has 8 commas up untill the subtitle text. Probably the Effect field is no present, but not 100 % sure.
         */

        /* For ASS:
         * Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
         * Dialogue: Layer#,0:02:40.65,0:02:41.79,Wolf main,Cher,0000,0000,0000,,Et les enregistrements de ses ondes delta ?
         */
        if( sscanf( s,
                    "Dialogue: %[^,],%d:%d:%d.%d,%d:%d:%d.%d,%[^\r\n]",
                    buffer_text2,
                    &h1, &m1, &s1, &c1,
                    &h2, &m2, &s2, &c2,
                    buffer_text ) == 10 )
        {
            i_start = ( (int64_t)h1 * 3600*1000 +
                        (int64_t)m1 * 60*1000 +
                        (int64_t)s1 * 1000 +
                        (int64_t)c1 * 10 ) * 1000;

            i_stop  = ( (int64_t)h2 * 3600*1000 +
                        (int64_t)m2 * 60*1000 +
                        (int64_t)s2 * 1000 +
                        (int64_t)c2 * 10 ) * 1000;

            /* The dec expects: ReadOrder, Layer, Style, Name, MarginL, MarginR, MarginV, Effect, Text */
            /* (Layer comes from ASS specs ... it's empty for SSA.) */
            if( p_sys->i_type == SUB_TYPE_SSA1 )
            {
                sprintf( p_subtitle->psz_text,
                         ",%s", strdup( buffer_text) ); /* SSA1 has only 8 commas before the text starts, not 9 */
            }
            else
            {
                sprintf( p_subtitle->psz_text,
                         ",,%s", strdup( buffer_text) ); /* ReadOrder, Layer, %s(rest of fields) */
            }
            p_subtitle->i_start = i_start;
            p_subtitle->i_stop = i_stop;
            return 0;
        }
        else
        {
            /* All the other stuff we add to the header field */
            if( p_sys->psz_header != NULL )
            {
                if( !( p_sys->psz_header = realloc( p_sys->psz_header,
                          strlen( p_sys->psz_header ) + 1 + strlen( s ) + 2 ) ) )
                {
                    msg_Err( p_demux, "out of memory");
                    return VLC_ENOMEM;
                }
                p_sys->psz_header = strcat( p_sys->psz_header,  s );
                p_sys->psz_header = strcat( p_sys->psz_header, "\n" );
            }
            else
            {
                if( !( p_sys->psz_header = malloc( strlen( s ) + 2 ) ) )
                {
                    msg_Err( p_demux, "out of memory");
                    return VLC_ENOMEM;
                }
                p_sys->psz_header = s;
                p_sys->psz_header = strcat( p_sys->psz_header, "\n" );
            }
        }
    }
}

static int  ParseVplayer( demux_t *p_demux, subtitle_t *p_subtitle )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    text_t      *txt = &p_sys->txt;

    /*
     * each line:
     *  h:m:s:Line1|Line2|Line3....
     *  or
     *  h:m:s Line1|Line2|Line3....
     *
     */
    char *p;
    char buffer_text[MAX_LINE + 1];
    int64_t    i_start;
    unsigned int i;

    p_subtitle->i_start = 0;
    p_subtitle->i_stop  = 0;
    p_subtitle->psz_text = NULL;

    for( ;; )
    {
        int h, m, s;
        char c;

        if( ( p = TextGetLine( txt ) ) == NULL )
        {
            return( VLC_EGENERIC );
        }

        i_start = 0;

        memset( buffer_text, '\0', MAX_LINE );
        if( sscanf( p, "%d:%d:%d%[ :]%[^\r\n]", &h, &m, &s, &c, buffer_text ) == 5 )
        {
            i_start = ( (int64_t)h * 3600*1000 +
                        (int64_t)m * 60*1000 +
                        (int64_t)s * 1000 ) * 1000;
            break;
        }
    }

    /* replace | by \n */
    for( i = 0; i < strlen( buffer_text ); i++ )
    {
        if( buffer_text[i] == '|' )
        {
            buffer_text[i] = '\n';
        }
    }
    p_subtitle->i_start = i_start;

    p_subtitle->i_stop  = 0;
    p_subtitle->psz_text = strndup( buffer_text, MAX_LINE );
    return( 0 );
}

static char *ParseSamiSearch( text_t *txt, char *psz_start, char *psz_str )
{
    if( psz_start )
    {
        if( strcasestr( psz_start, psz_str ) )
        {
            char *s = strcasestr( psz_start, psz_str );

            s += strlen( psz_str );

            return( s );
        }
    }
    for( ;; )
    {
        char *p;
        if( ( p = TextGetLine( txt ) ) == NULL )
        {
            return NULL;
        }
        if( strcasestr( p, psz_str ) )
        {
            char *s = strcasestr( p, psz_str );

            s += strlen( psz_str );

            return(  s);
        }
    }
}

static int  ParseSami( demux_t *p_demux, subtitle_t *p_subtitle )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    text_t      *txt = &p_sys->txt;

    char *p;
    int64_t i_start;

    int  i_text;
    char buffer_text[10*MAX_LINE + 1];

    p_subtitle->i_start = 0;
    p_subtitle->i_stop  = 0;
    p_subtitle->psz_text = NULL;

#define ADDC( c ) \
    if( i_text < 10*MAX_LINE )      \
    {                               \
        buffer_text[i_text++] = c;  \
        buffer_text[i_text] = '\0'; \
    }

    /* search "Start=" */
    if( !( p = ParseSamiSearch( txt, NULL, "Start=" ) ) )
    {
        return VLC_EGENERIC;
    }

    /* get start value */
    i_start = strtol( p, &p, 0 );

    /* search <P */
    if( !( p = ParseSamiSearch( txt, p, "<P" ) ) )
    {
        return VLC_EGENERIC;
    }
    /* search > */
    if( !( p = ParseSamiSearch( txt, p, ">" ) ) )
    {
        return VLC_EGENERIC;
    }

    i_text = 0;
    buffer_text[0] = '\0';
    /* now get all txt until  a "Start=" line */
    for( ;; )
    {
        if( *p )
        {
            if( *p == '<' )
            {
                if( !strncasecmp( p, "<br", 3 ) )
                {
                    ADDC( '\n' );
                }
                else if( strcasestr( p, "Start=" ) )
                {
                    TextPreviousLine( txt );
                    break;
                }
                p = ParseSamiSearch( txt, p, ">" );
            }
            else if( !strncmp( p, "&nbsp;", 6 ) )
            {
                ADDC( ' ' );
                p += 6;
            }
            else if( *p == '\t' )
            {
                ADDC( ' ' );
                p++;
            }
            else
            {
                ADDC( *p );
                p++;
            }
        }
        else
        {
            p = TextGetLine( txt );
        }

        if( p == NULL )
        {
            break;
        }
    }

    p_subtitle->i_start = i_start * 1000;
    p_subtitle->i_stop  = 0;
    p_subtitle->psz_text = strndup( buffer_text, 10*MAX_LINE );

    return( VLC_SUCCESS );
#undef ADDC
}

static int ParseDVDSubtitle( demux_t *p_demux, subtitle_t *p_subtitle )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    text_t      *txt = &p_sys->txt;

    /*
     * {T h1:m1:s1:c1
     * Line1
     * Line2
     * ...
     * }
     *
     */
    char *s;
    char buffer_text[ 10 * MAX_LINE];
    int  i_buffer_text;
    int64_t     i_start;

    p_subtitle->i_start = 0;
    p_subtitle->i_stop  = 0;
    p_subtitle->psz_text = NULL;

    for( ;; )
    {
        int h1, m1, s1, c1;
        if( ( s = TextGetLine( txt ) ) == NULL )
        {
            return( VLC_EGENERIC );
        }
        if( sscanf( s,
                    "{T %d:%d:%d:%d",
                    &h1, &m1, &s1, &c1 ) == 4 )
        {
            i_start = ( (int64_t)h1 * 3600*1000 +
                        (int64_t)m1 * 60*1000 +
                        (int64_t)s1 * 1000 +
                        (int64_t)c1 * 10) * 1000;

            /* Now read text until a line containing "}" */
            for( i_buffer_text = 0;; )
            {
                int i_len;
                if( ( s = TextGetLine( txt ) ) == NULL )
                {
                    return( VLC_EGENERIC );
                }

                i_len = strlen( s );
                if( i_len == 1 && s[0] == '}' )
                {
                    /* "}" -> end of this subtitle */
                    buffer_text[__MAX( i_buffer_text - 1, 0 )] = '\0';
                    p_subtitle->i_start = i_start;
                    p_subtitle->i_stop  = 0;
                    p_subtitle->psz_text = strdup( buffer_text );
                    return 0;
                }
                else
                {
                    if( i_buffer_text + i_len + 1 < 10 * MAX_LINE )
                    {
                        memcpy( buffer_text + i_buffer_text,
                                s,
                                i_len );
                        i_buffer_text += i_len;

                        buffer_text[i_buffer_text] = '\n';
                        i_buffer_text++;
                    }
                }
            }
        }
    }
}

