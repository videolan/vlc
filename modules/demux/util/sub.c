/*****************************************************************************
 * sub.c
 *****************************************************************************
 * Copyright (C) 1999-2003 VideoLAN
 * $Id: sub.c,v 1.42 2004/01/26 20:02:15 gbazin Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <ctype.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include "vlc_video.h"
#include <codecs.h>

#include "sub.h"

#if (!defined( WIN32 ) || defined(__MINGW32__))
#    include <dirent.h>
#endif

static int  Open ( vlc_object_t *p_this );

static int  sub_open ( subtitle_demux_t *p_sub,
                       input_thread_t  *p_input,
                       char  *psz_name,
                       mtime_t i_microsecperframe,
                       int i_track_id );
static int  sub_demux( subtitle_demux_t *p_sub, mtime_t i_maxdate );
static int  sub_seek ( subtitle_demux_t *p_sub, mtime_t i_date );
static void sub_close( subtitle_demux_t *p_sub );

static void sub_fix( subtitle_demux_t *p_sub );

static char *ppsz_sub_type[] = { "auto", "microdvd", "subrip", "ssa1",
  "ssa2-4", "vplayer", "sami", "vobsub" };

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define SUB_DELAY_LONGTEXT \
    "Delay subtitles (in 1/10s)"
#define SUB_FPS_LONGTEXT \
    "Override frames per second. " \
    "It will only work with MicroDVD subtitles."
#define SUB_TYPE_LONGTEXT \
    "One from \"microdvd\", \"subrip\", \"ssa1\", \"ssa2-4\", \"vplayer\" " \
    "\"sami\" (auto for autodetection, it should always work)."

vlc_module_begin();
    set_description( _("Text subtitles demux") );
    set_capability( "subtitle demux", 12 );
    add_float( "sub-fps", 25.0, NULL,
               N_("Frames per second"),
               SUB_FPS_LONGTEXT, VLC_TRUE );
    add_integer( "sub-delay", 0, NULL,
                 N_("Delay subtitles (in 1/10s)"),
                 SUB_DELAY_LONGTEXT, VLC_TRUE );
    add_string( "sub-type", "auto", NULL, "Subtitles fileformat",
                SUB_TYPE_LONGTEXT, VLC_TRUE );
        change_string_list( ppsz_sub_type, 0, 0 );
    set_callbacks( Open, NULL );
vlc_module_end();

/*****************************************************************************
 * Module initializer
 *****************************************************************************/
static int Open ( vlc_object_t *p_this )
{
    subtitle_demux_t *p_sub = (subtitle_demux_t*)p_this;

    p_sub->pf_open  = sub_open;
    p_sub->pf_demux = sub_demux;
    p_sub->pf_seek  = sub_seek;
    p_sub->pf_close = sub_close;

    /* Initialize the variables */
    var_Create( p_this, "sub-fps", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );
    var_Create( p_this, "sub-delay", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_this, "sub-type", VLC_VAR_STRING | VLC_VAR_DOINHERIT );

    return VLC_SUCCESS;
}
#define MAX_TRY     256
#define MAX_LINE    2048

#define FREE( p ) if( p ) { free( p); (p) = NULL; }

typedef struct
{
    int     i_line_count;
    int     i_line;
    char    **line;
} text_t;

static int  text_load( text_t *txt, char *psz_name )
{
    FILE *f;
    int   i_line_max;

    /* init txt */
    i_line_max          = 100;
    txt->i_line_count   = 0;
    txt->i_line         = 0;
    txt->line           = calloc( i_line_max, sizeof( char * ) );

    /* open file */
    if( !( f = fopen( psz_name, "rb" ) ) )
    {
        return VLC_EGENERIC;
    }

    /* load the complete file */
    for( ;; )
    {
        char buffer[8096];
        char *p;

        if( fgets( buffer, 8096, f ) <= 0)
        {
            break;
        }
        while( ( p = strchr( buffer, '\r' ) ) )
        {
            *p = '\0';
        }
        while( ( p = strchr( buffer, '\n' ) ) )
        {
            *p = '\0';
        }

        txt->line[txt->i_line_count++] = strdup( buffer );

        if( txt->i_line_count >= i_line_max )
        {
            i_line_max += 100;
            txt->line = realloc( txt->line, i_line_max * sizeof( char*) );
        }
    }

    fclose( f );

    if( txt->i_line_count <= 0 )
    {
        FREE( txt->line );
        return( VLC_EGENERIC );
    }

    return( VLC_SUCCESS );
}
static void text_unload( text_t *txt )
{
    int i;

    for( i = 0; i < txt->i_line_count; i++ )
    {
        FREE( txt->line[i] );
    }
    FREE( txt->line );
    txt->i_line       = 0;
    txt->i_line_count = 0;
}

static char *text_get_line( text_t *txt )
{
    if( txt->i_line >= txt->i_line_count )
    {
        return( NULL );
    }

    return( txt->line[txt->i_line++] );
}
static void text_previous_line( text_t *txt )
{
    if( txt->i_line > 0 )
    {
        txt->i_line--;
    }
}
static void text_rewind( text_t *txt )
{
    txt->i_line = 0;
}

static int  sub_MicroDvdRead( subtitle_demux_t *p_sub, text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe );
static int  sub_SubRipRead  ( subtitle_demux_t *p_sub, text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe );
static int  sub_SSARead     ( subtitle_demux_t *p_sub, text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe );
static int  sub_Vplayer     ( subtitle_demux_t *p_sub, text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe );
static int  sub_Sami        ( subtitle_demux_t *p_sub, text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe );
static int  sub_VobSub      ( subtitle_demux_t *p_sub, text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe );

static struct
{
    char *psz_type_name;
    int  i_type;
    char *psz_name;
    int  (*pf_read_subtitle)    ( subtitle_demux_t *, text_t *, subtitle_t*, mtime_t );
} sub_read_subtitle_function [] =
{
    { "microdvd",   SUB_TYPE_MICRODVD,  "MicroDVD", sub_MicroDvdRead },
    { "subrip",     SUB_TYPE_SUBRIP,    "SubRIP",   sub_SubRipRead },
    { "ssa1",       SUB_TYPE_SSA1,      "SSA-1",    sub_SSARead },
    { "ssa2-4",     SUB_TYPE_SSA2_4,    "SSA-2/3/4",sub_SSARead },
    { "vplayer",    SUB_TYPE_VPLAYER,   "VPlayer",  sub_Vplayer },
    { "sami",       SUB_TYPE_SAMI,      "SAMI",     sub_Sami },
    { "vobsub",     SUB_TYPE_VOBSUB,    "VobSub",   sub_VobSub },
    { NULL,         SUB_TYPE_UNKNOWN,   "Unknown",  NULL }
};

static char * local_stristr( char *psz_big, char *psz_little)
{
    char *p_pos = psz_big;

    if (!psz_big || !psz_little || !*psz_little) return psz_big;

    while (*p_pos)
    {
        if (toupper(*p_pos) == toupper(*psz_little))
        {
            char * psz_cur1 = p_pos + 1;
            char * psz_cur2 = psz_little + 1;
            while (*psz_cur1 && *psz_cur2 && toupper(*psz_cur1) == toupper(*psz_cur2))
            {
                psz_cur1++;
                psz_cur2++;
            }
            if (!*psz_cur2) return p_pos;
        }
        p_pos++;
    }
    return NULL;
}

/*****************************************************************************
 * sub_open: Open a subtitle file and add subtitle ES
 *****************************************************************************/
static int sub_open( subtitle_demux_t *p_sub, input_thread_t  *p_input,
                     char *psz_name, mtime_t i_microsecperframe,
                     int i_track_id )
{
    text_t  txt;
    vlc_value_t val;
    es_format_t fmt;
    int i, i_sub_type, i_max;
    int (*pf_read_subtitle)( subtitle_demux_t *, text_t *, subtitle_t *,
                             mtime_t ) = NULL;

    p_sub->i_sub_type = SUB_TYPE_UNKNOWN;
    p_sub->p_es = NULL;
    p_sub->i_subtitles = 0;
    p_sub->subtitle = NULL;
    p_sub->p_input = p_input;

    if( !psz_name || !*psz_name )
    {
        msg_Err( p_sub, "no subtitle file specified" );
        return VLC_EGENERIC;
    }

    /* *** load the file *** */
    if( text_load( &txt, psz_name ) )
    {
        msg_Err( p_sub, "cannot open `%s' subtitle file", psz_name );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_sub, "opened `%s'", psz_name );

    var_Get( p_sub, "sub-fps", &val );
    if( val.i_int >= 1.0 )
    {
        i_microsecperframe = (mtime_t)( (float)1000000 / val.f_float );
    }
    else if( val.f_float <= 0 )
    {
        i_microsecperframe = 40000; /* default: 25fps */
    }

    var_Get( p_sub, "sub-type", &val);
    if( val.psz_string && *val.psz_string )
    {
        int i;

        for( i = 0; ; i++ )
        {
            if( sub_read_subtitle_function[i].psz_type_name == NULL )
            {
                i_sub_type = SUB_TYPE_UNKNOWN;
                break;
            }
            if( !strcmp( sub_read_subtitle_function[i].psz_type_name,
                         val.psz_string ) )
            {
                i_sub_type = sub_read_subtitle_function[i].i_type;
                break;
            }
        }
    }
    else
    {
        i_sub_type = SUB_TYPE_UNKNOWN;
    }
    FREE( val.psz_string );

    /* *** Now try to autodetect subtitle format *** */
    if( i_sub_type == SUB_TYPE_UNKNOWN )
    {
        int     i_try;
        char    *s;

        msg_Dbg( p_input, "trying to autodetect file format" );
        for( i_try = 0; i_try < MAX_TRY; i_try++ )
        {
            int i_dummy;

            if( ( s = text_get_line( &txt ) ) == NULL )
            {
                break;
            }

            if( local_stristr( s, "<SAMI>" ) )
            {
                i_sub_type = SUB_TYPE_SAMI;
                break;
            }
            else if( sscanf( s, "{%d}{%d}", &i_dummy, &i_dummy ) == 2 ||
                     sscanf( s, "{%d}{}", &i_dummy ) == 1)
            {
                i_sub_type = SUB_TYPE_MICRODVD;
                break;
            }
            else if( sscanf( s,
                             "%d:%d:%d,%d --> %d:%d:%d,%d",
                             &i_dummy,&i_dummy,&i_dummy,&i_dummy,
                             &i_dummy,&i_dummy,&i_dummy,&i_dummy ) == 8 )
            {
                i_sub_type = SUB_TYPE_SUBRIP;
                break;
            }
            else if( sscanf( s,
                             "!: This is a Sub Station Alpha v%d.x script.",
                             &i_dummy ) == 1)
            {
                if( i_dummy <= 1 )
                {
                    i_sub_type = SUB_TYPE_SSA1;
                }
                else
                {
                    i_sub_type = SUB_TYPE_SSA2_4; /* I hope this will work */
                }
                break;
            }
            else if( local_stristr( s, "This is a Sub Station Alpha v4 script" ) )
            {
                i_sub_type = SUB_TYPE_SSA2_4; /* I hope this will work */
                break;
            }
            else if( !strncasecmp( s, "Dialogue: Marked", 16  ) )
            {
                i_sub_type = SUB_TYPE_SSA2_4; /* could be wrong */
                break;
            }
            else if( sscanf( s, "%d:%d:%d:", &i_dummy, &i_dummy, &i_dummy ) == 3 ||
                     sscanf( s, "%d:%d:%d ", &i_dummy, &i_dummy, &i_dummy ) == 3 )
            {
                i_sub_type = SUB_TYPE_VPLAYER;
                break;
            }
            else if( local_stristr( s, "# VobSub index file" ) )
            {
                i_sub_type = SUB_TYPE_VOBSUB;
                break;
            }
        }

        text_rewind( &txt );
    }

    /* *** Load this file in memory *** */
    for( i = 0; ; i++ )
    {
        if( sub_read_subtitle_function[i].i_type == SUB_TYPE_UNKNOWN )
        {
            msg_Dbg( p_input, "unknown subtitle file" );
            text_unload( &txt );
            return VLC_EGENERIC;
        }

        if( sub_read_subtitle_function[i].i_type == i_sub_type )
        {
            msg_Dbg( p_input, "detected %s format",
                    sub_read_subtitle_function[i].psz_name );
            p_sub->i_sub_type = i_sub_type;
            pf_read_subtitle = sub_read_subtitle_function[i].pf_read_subtitle;
            break;
        }
    }

    for( i_max = 0;; )
    {
        if( p_sub->i_subtitles >= i_max )
        {
            i_max += 128;
            if( p_sub->subtitle )
            {
                if( !( p_sub->subtitle = realloc( p_sub->subtitle,
                                           sizeof( subtitle_t ) * i_max ) ) )
                {
                    msg_Err( p_sub, "out of memory");
                    return VLC_ENOMEM;
                }
            }
            else
            {
                if( !(  p_sub->subtitle = malloc( sizeof( subtitle_t ) * i_max ) ) )
                {
                    msg_Err( p_sub, "out of memory");
                    return VLC_ENOMEM;
                }
            }
        }
        if( pf_read_subtitle( p_sub, &txt,
                              p_sub->subtitle + p_sub->i_subtitles,
                              i_microsecperframe ) < 0 )
        {
            break;
        }
        p_sub->i_subtitles++;
    }
    msg_Dbg( p_sub, "loaded %d subtitles", p_sub->i_subtitles );

    /* *** Close the file *** */
    text_unload( &txt );

    /* *** fix subtitle (order and time) *** */
    p_sub->i_subtitle = 0;  /* will be modified by sub_fix */
    if( p_sub->i_sub_type != SUB_TYPE_VOBSUB )
    {
        sub_fix( p_sub );
    }

    /* *** add subtitle ES *** */
    if( p_sub->i_sub_type == SUB_TYPE_VOBSUB )
    {
        int i_len = strlen( psz_name );
        char *psz_vobname = strdup(psz_name);

        strcpy( psz_vobname + i_len - 4, ".sub" );

        /* open file */
        if( !( p_sub->p_vobsub_file = fopen( psz_vobname, "rb" ) ) )
        {
            msg_Err( p_sub, "couldn't open .sub Vobsub file: %s", psz_vobname );
        }
        free( psz_vobname );

        es_format_Init( &fmt, SPU_ES, VLC_FOURCC( 's','p','u',' ' ) );
    }
    else if( p_sub->i_sub_type == SUB_TYPE_SSA1 ||
             p_sub->i_sub_type == SUB_TYPE_SSA2_4 )
    {
        es_format_Init( &fmt, SPU_ES, VLC_FOURCC( 's','s','a',' ' ) );
    }
    else
    {
        es_format_Init( &fmt, SPU_ES, VLC_FOURCC( 's','u','b','t' ) );
    }
    if( p_sub->psz_header != NULL )
    {
        fmt.i_extra = strlen( p_sub->psz_header ) + 1;
        fmt.p_extra = strdup( p_sub->psz_header );
    }
    p_sub->p_es = es_out_Add( p_input->p_es_out, &fmt );
    p_sub->i_previously_selected = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * sub_demux: Send subtitle to decoder until i_maxdate
 *****************************************************************************/
static int  sub_demux( subtitle_demux_t *p_sub, mtime_t i_maxdate )
{
    input_thread_t *p_input = p_sub->p_input;
    vlc_bool_t     b;

    es_out_Control( p_input->p_es_out, ES_OUT_GET_ES_STATE, p_sub->p_es, &b );
    if( b && !p_sub->i_previously_selected )
    {
        p_sub->i_previously_selected = 1;
        p_sub->pf_seek( p_sub, i_maxdate );
        return VLC_SUCCESS;
    }
    else if( !b && p_sub->i_previously_selected )
    {
        p_sub->i_previously_selected = 0;
        return VLC_SUCCESS;
    }

    while( p_sub->i_subtitle < p_sub->i_subtitles &&
           p_sub->subtitle[p_sub->i_subtitle].i_start < i_maxdate )
    {
        block_t *p_block;
        int i_len;

        i_len = strlen( p_sub->subtitle[p_sub->i_subtitle].psz_text ) + 1;

        if( i_len <= 1 )
        {
            /* empty subtitle */
            p_sub->i_subtitle++;
            continue;
        }

        if( !( p_block = block_New( p_sub->p_input, i_len ) ) )
        {
            p_sub->i_subtitle++;
            continue;
        }

        p_block->i_pts =
            input_ClockGetTS( p_sub->p_input,
                              p_sub->p_input->stream.p_selected_program,
                              p_sub->subtitle[p_sub->i_subtitle].i_start*9/100);
        if( p_sub->subtitle[p_sub->i_subtitle].i_stop > 0 )
        {
            /* FIXME kludge ...
             * i_dts means end of display...
             */
            p_block->i_dts =
                input_ClockGetTS( p_sub->p_input,
                              p_sub->p_input->stream.p_selected_program,
                              p_sub->subtitle[p_sub->i_subtitle].i_stop *9/100);
        }
        else
        {
            p_block->i_dts = 0;
        }

        memcpy( p_block->p_buffer,
                p_sub->subtitle[p_sub->i_subtitle].psz_text, i_len );

        if( p_block->i_pts > 0 )
        {
            es_out_Send( p_input->p_es_out, p_sub->p_es, p_block );
        }
        else
        {
            block_Release( p_block );
        }

        p_sub->i_subtitle++;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * sub_seek: Seek to i_date
 *****************************************************************************/
static int  sub_seek ( subtitle_demux_t *p_sub, mtime_t i_date )
{
    /* should be fast enough... */
    p_sub->i_subtitle = 0;
    while( p_sub->i_subtitle < p_sub->i_subtitles &&
           p_sub->subtitle[p_sub->i_subtitle].i_start < i_date )
    {
        p_sub->i_subtitle++;
    }
    return( 0 );
}

/*****************************************************************************
 * sub_close: Close subtitle demux
 *****************************************************************************/
static void sub_close( subtitle_demux_t *p_sub )
{
    if( p_sub->subtitle )
    {
        int i;
        for( i = 0; i < p_sub->i_subtitles; i++ )
        {
            if( p_sub->subtitle[i].psz_text )
            {
                free( p_sub->subtitle[i].psz_text );
            }
        }
        free( p_sub->subtitle );
    }
    if( p_sub->p_vobsub_file )
    {
        fclose( p_sub->p_vobsub_file );
    }
}

/*****************************************************************************
 * sub_fix: fix time stamp and order of subtitle
 *****************************************************************************/
static void  sub_fix( subtitle_demux_t *p_sub )
{
    int     i;
    mtime_t i_delay;
    int     i_index;
    int     i_done;
    vlc_value_t val;

    /* *** fix order (to be sure...) *** */
    /* We suppose that there are near in order and this durty bubble sort
     * wont take too much time
     */
    do
    {
        i_done = 1;
        for( i_index = 1; i_index < p_sub->i_subtitles; i_index++ )
        {
            if( p_sub->subtitle[i_index].i_start <
                    p_sub->subtitle[i_index - 1].i_start )
            {
                subtitle_t sub_xch;
                memcpy( &sub_xch,
                        p_sub->subtitle + i_index - 1,
                        sizeof( subtitle_t ) );
                memcpy( p_sub->subtitle + i_index - 1,
                        p_sub->subtitle + i_index,
                        sizeof( subtitle_t ) );
                memcpy( p_sub->subtitle + i_index,
                        &sub_xch,
                        sizeof( subtitle_t ) );
                i_done = 0;
            }
        }
    } while( !i_done );

    /* *** and at the end add delay *** */
    var_Get( p_sub, "sub-delay", &val );
    i_delay = (mtime_t) val.i_int * 100000;
    if( i_delay != 0 )
    {
        for( i = 0; i < p_sub->i_subtitles; i++ )
        {
            p_sub->subtitle[i].i_start += i_delay;
            p_sub->subtitle[i].i_stop += i_delay;
            if( p_sub->subtitle[i].i_start < 0 )
            {
                p_sub->i_subtitle = i + 1;
            }
        }
    }
}



/*****************************************************************************
 * Specific Subtitle function
 *****************************************************************************/
static int  sub_MicroDvdRead( subtitle_demux_t *p_sub, text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe)
{
    /*
     * each line:
     *  {n1}{n2}Line1|Line2|Line3....
     * where n1 and n2 are the video frame number...
     *
     */
    char *s;

    char buffer_text[MAX_LINE + 1];
    unsigned int    i_start;
    unsigned int    i_stop;
    unsigned int i;

    for( ;; )
    {
        if( ( s = text_get_line( txt ) ) == NULL )
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
    p_subtitle->i_start = (mtime_t)i_start * (mtime_t)i_microsecperframe;
    p_subtitle->i_stop  = (mtime_t)i_stop  * (mtime_t)i_microsecperframe;
    p_subtitle->psz_text = strndup( buffer_text, MAX_LINE );
    return( 0 );
}

static int  sub_SubRipRead( subtitle_demux_t *p_sub, text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe )
{
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
    mtime_t     i_start;
    mtime_t     i_stop;

    for( ;; )
    {
        int h1, m1, s1, d1, h2, m2, s2, d2;
        if( ( s = text_get_line( txt ) ) == NULL )
        {
            return( VLC_EGENERIC );
        }
        if( sscanf( s,
                    "%d:%d:%d,%d --> %d:%d:%d,%d",
                    &h1, &m1, &s1, &d1,
                    &h2, &m2, &s2, &d2 ) == 8 )
        {
            i_start = ( (mtime_t)h1 * 3600*1000 +
                        (mtime_t)m1 * 60*1000 +
                        (mtime_t)s1 * 1000 +
                        (mtime_t)d1 ) * 1000;

            i_stop  = ( (mtime_t)h2 * 3600*1000 +
                        (mtime_t)m2 * 60*1000 +
                        (mtime_t)s2 * 1000 +
                        (mtime_t)d2 ) * 1000;

            /* Now read text until an empty line */
            for( i_buffer_text = 0;; )
            {
                int i_len;
                if( ( s = text_get_line( txt ) ) == NULL )
                {
                    return( VLC_EGENERIC );
                }

                i_len = strlen( s );
                if( i_len <= 1 )
                {
                    /* empty line -> end of this subtitle */
                    buffer_text[__MAX( i_buffer_text - 1, 0 )] = '\0';
                    p_subtitle->i_start = i_start;
                    p_subtitle->i_stop = i_stop;
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


static int  sub_SSARead( subtitle_demux_t *p_sub, text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe )
{
    char buffer_text[ 10 * MAX_LINE];
    char *s;
    mtime_t     i_start;
    mtime_t     i_stop;

    for( ;; )
    {
        int h1, m1, s1, c1, h2, m2, s2, c2;
        int i_dummy;

        if( ( s = text_get_line( txt ) ) == NULL )
        {
            return( VLC_EGENERIC );
        }
        p_subtitle->psz_text = malloc( strlen( s ) );

        if( sscanf( s,
                    "Dialogue: Marked=%d,%d:%d:%d.%d,%d:%d:%d.%d%[^\r\n]",
                    &i_dummy,
                    &h1, &m1, &s1, &c1,
                    &h2, &m2, &s2, &c2,
                    buffer_text ) == 10 )
        {
            i_start = ( (mtime_t)h1 * 3600*1000 +
                        (mtime_t)m1 * 60*1000 +
                        (mtime_t)s1 * 1000 +
                        (mtime_t)c1 * 10 ) * 1000;

            i_stop  = ( (mtime_t)h2 * 3600*1000 +
                        (mtime_t)m2 * 60*1000 +
                        (mtime_t)s2 * 1000 +
                        (mtime_t)c2 * 10 ) * 1000;

            /* The dec expects: ReadOrder, Layer, Style, Name, MarginL, MarginR, MarginV, Effect, Text */
            if( p_sub->i_sub_type == SUB_TYPE_SSA1 )
            {
                sprintf( p_subtitle->psz_text, ",%d%s", i_dummy, strdup( buffer_text) );
            }
            else
            {
                sprintf( p_subtitle->psz_text, ",%d,%s", i_dummy, strdup( buffer_text) );
            }
            p_subtitle->i_start = i_start;
            p_subtitle->i_stop = i_stop;
            return( 0 );
        }
        else
        {
            /* All the other stuff we add to the header field */
            if( p_sub->psz_header != NULL )
            {
                if( !( p_sub->psz_header = realloc( p_sub->psz_header,
                          strlen( p_sub->psz_header ) + strlen( s ) + 2 ) ) )
                {
                    msg_Err( p_sub, "out of memory");
                    return VLC_ENOMEM;
                }
                p_sub->psz_header = strcat( p_sub->psz_header, strdup( s ) );
                p_sub->psz_header = strcat( p_sub->psz_header, "\n" );
            }
            else
            {
                if( !( p_sub->psz_header = malloc( strlen( s ) + 2 ) ) )
                {
                    msg_Err( p_sub, "out of memory");
                    return VLC_ENOMEM;
                }
                p_sub->psz_header = strdup( s );
                p_sub->psz_header = strcat( p_sub->psz_header, "\n" );
            }
        }
    }
}

static int  sub_Vplayer( subtitle_demux_t *p_sub, text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe)
{
    /*
     * each line:
     *  h:m:s:Line1|Line2|Line3....
     *  or
     *  h:m:s Line1|Line2|Line3....
     * where n1 and n2 are the video frame number...
     *
     */
    char *p;
    char buffer_text[MAX_LINE + 1];
    mtime_t    i_start;
    unsigned int i;

    for( ;; )
    {
        int h, m, s;
        char c;

        if( ( p = text_get_line( txt ) ) == NULL )
        {
            return( VLC_EGENERIC );
        }

        i_start = 0;

        memset( buffer_text, '\0', MAX_LINE );
        if( sscanf( p, "%d:%d:%d%[ :]%[^\r\n]", &h, &m, &s, &c, buffer_text ) == 5 )
        {
            i_start = ( (mtime_t)h * 3600*1000 +
                        (mtime_t)m * 60*1000 +
                        (mtime_t)s * 1000 ) * 1000;
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

static char *sub_SamiSearch( text_t *txt, char *psz_start, char *psz_str )
{
    if( psz_start )
    {
        if( local_stristr( psz_start, psz_str ) )
        {
            char *s = local_stristr( psz_start, psz_str );

            s += strlen( psz_str );

            return( s );
        }
    }
    for( ;; )
    {
        char *p;
        if( ( p = text_get_line( txt ) ) == NULL )
        {
            return NULL;
        }
        if( local_stristr( p, psz_str ) )
        {
            char *s = local_stristr( p, psz_str );

            s += strlen( psz_str );

            return(  s);
        }
    }
}

static int  sub_Sami( subtitle_demux_t *p_sub, text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe )
{
    char *p;
    int i_start;

    int  i_text;
    char buffer_text[10*MAX_LINE + 1];
#define ADDC( c ) \
    if( i_text < 10*MAX_LINE )      \
    {                               \
        buffer_text[i_text++] = c;  \
        buffer_text[i_text] = '\0'; \
    }

    /* search "Start=" */
    if( !( p = sub_SamiSearch( txt, NULL, "Start=" ) ) )
    {
        return VLC_EGENERIC;
    }

    /* get start value */
    i_start = strtol( p, &p, 0 );

    /* search <P */
    if( !( p = sub_SamiSearch( txt, p, "<P" ) ) )
    {
        return VLC_EGENERIC;
    }
    /* search > */
    if( !( p = sub_SamiSearch( txt, p, ">" ) ) )
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
                else if( local_stristr( p, "Start=" ) )
                {
                    text_previous_line( txt );
                    break;
                }
                p = sub_SamiSearch( txt, p, ">" );
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
            p = text_get_line( txt );
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

static int  sub_VobSub( subtitle_demux_t *p_sub, text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe)
{
    /*
     * Parse the idx file. Each line:
     * timestamp: hh:mm:ss:mss, filepos: loc
     * hexint is the hex location of the vobsub in the .sub file
     *
     */
    char *p;

    char buffer_text[MAX_LINE + 1];
    unsigned int    i_start, i_location;

    for( ;; )
    {
        unsigned int h, m, s, ms, loc;

        if( ( p = text_get_line( txt ) ) == NULL )
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
/*

#define mpeg_read(buf, num) _mpeg_read(srcbuf, size, srcpos, buf, num)
static unsigned int _mpeg_read(unsigned char *srcbufunsigned int, unsigned int size,
                           unsigned int &srcpos, unsigned char *dstbuf,
                           unsigned int num) {
  unsigned int real_num;

  if ((srcpos + num) >= size)
    real_num = size - srcpos;
  else
    real_num = num;
  memcpy(dstbuf, &srcbuf[srcpos], real_num);
  srcpos += real_num;

  return real_num;
}

#define mpeg_getc() _mpeg_getch(srcbuf, size, srcpos)
static int _mpeg_getch(unsigned char *srcbuf, unsigned int size,
                       unsigned int &srcpos) {
  unsigned char c;

  if (mpeg_read(&c, 1) != 1)
    return -1;
  return (int)c;
}

#define mpeg_seek(b, w) _mpeg_seek(size, srcpos, b, w)
static int _mpeg_seek(unsigned int size, unsigned int &srcpos, unsigned int num,
                      int whence) {
  unsigned int new_pos;

  if (whence == SEEK_SET)
    new_pos = num;
  else if (whence == SEEK_CUR)
    new_pos = srcpos + num;
  else
    abort();

  if (new_pos >= size) {
    srcpos = size;
    return 1;
  }

  srcpos = new_pos;
  return 0;
}

#define mpeg_tell() srcpos
*/
/*
static int mpeg_run(demuxer_t *demuxer, unsigned char *srcbuf, unsigned int size) {
  unsigned int len, idx, version, srcpos, packet_size;
  int c, aid;
  float pts;*/
  /* Goto start of a packet, it starts with 0x000001?? */
  /*const unsigned char wanted[] = { 0, 0, 1 };
  unsigned char buf[5];
  demux_packet_t *dp;
  demux_stream_t *ds;
  mkv_demuxer_t *mkv_d;

  mkv_d = (mkv_demuxer_t *)demuxer->priv;
  ds = demuxer->sub;

  srcpos = 0;
  packet_size = 0;
  while (1) {
    if (mpeg_read(buf, 4) != 4)
      return -1;
    while (memcmp(buf, wanted, sizeof(wanted)) != 0) {
      c = mpeg_getc();
      if (c < 0)
        return -1;
      memmove(buf, buf + 1, 3);
      buf[3] = c;
    }
    switch (buf[3]) {
      case 0xb9:	*/		/* System End Code */
   /*     return 0;
        break;

      case 0xba:                    */  /* Packet start code */
 /*       c = mpeg_getc();
        if (c < 0)
          return -1;
        if ((c & 0xc0) == 0x40)
          version = 4;
        else if ((c & 0xf0) == 0x20)
          version = 2;
        else {
          mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] VobSub: Unsupported MPEG "
                 "version: 0x%02x\n", c);
          return -1;
        }

        if (version == 4) {
          if (mpeg_seek(9, SEEK_CUR))
            return -1;
        } else if (version == 2) {
          if (mpeg_seek(7, SEEK_CUR))
            return -1;
        } else
          abort();
        break;

      case 0xbd:	*/		/* packet */
/*        if (mpeg_read(buf, 2) != 2)
          return -1;
        len = buf[0] << 8 | buf[1];
        idx = mpeg_tell();
        c = mpeg_getc();
        if (c < 0)
          return -1;
        if ((c & 0xC0) == 0x40) {*/ /* skip STD scale & size */
/*          if (mpeg_getc() < 0)
            return -1;
          c = mpeg_getc();
          if (c < 0)
            return -1;
        }
        if ((c & 0xf0) == 0x20) { */ /* System-1 stream timestamp */
          /* Do we need this? */
/*          abort();
        } else if ((c & 0xf0) == 0x30) {
  */        /* Do we need this? */
/*          abort();
        } else if ((c & 0xc0) == 0x80) {*/ /* System-2 (.VOB) stream */
/*          unsigned int pts_flags, hdrlen, dataidx;
          c = mpeg_getc();
          if (c < 0)
            return -1;
          pts_flags = c;
          c = mpeg_getc();
          if (c < 0)
            return -1;
          hdrlen = c;
          dataidx = mpeg_tell() + hdrlen;
          if (dataidx > idx + len) {
            mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] VobSub: Invalid header "
                   "length: %d (total length: %d, idx: %d, dataidx: %d)\n",
                   hdrlen, len, idx, dataidx);
            return -1;
          }
          if ((pts_flags & 0xc0) == 0x80) {
            if (mpeg_read(buf, 5) != 5)
              return -1;
            if (!(((buf[0] & 0xf0) == 0x20) && (buf[0] & 1) && (buf[2] & 1) &&
                  (buf[4] & 1))) {
              mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] VobSub PTS error: 0x%02x "
                     "%02x%02x %02x%02x \n",
                     buf[0], buf[1], buf[2], buf[3], buf[4]);
              pts = 0;
            } else
              pts = ((buf[0] & 0x0e) << 29 | buf[1] << 22 |
                     (buf[2] & 0xfe) << 14 | buf[3] << 7 | (buf[4] >> 1));
          } else*/ /* if ((pts_flags & 0xc0) == 0xc0) */// {
            /* what's this? */
            /* abort(); */
    /*      }
          mpeg_seek(dataidx, SEEK_SET);
          aid = mpeg_getc();
          if (aid < 0) {
            mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] VobSub: Bogus aid %d\n", aid);
            return -1;
          }
          packet_size = len - ((unsigned int)mpeg_tell() - idx);
          
          dp = new_demux_packet(packet_size);
          dp->flags = 1;
          dp->pts = mkv_d->last_pts;
          if (mpeg_read(dp->buffer, packet_size) != packet_size) {
            mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] VobSub: mpeg_read failure");
            packet_size = 0;
            return -1;
          }
          ds_add_packet(ds, dp);
          idx = len;
        }
        break;

      case 0xbe:		*/	/* Padding */
 /*       if (mpeg_read(buf, 2) != 2)
          return -1;
        len = buf[0] << 8 | buf[1];
        if ((len > 0) && mpeg_seek(len, SEEK_CUR))
          return -1;
        break;

      default:
        if ((0xc0 <= buf[3]) && (buf[3] < 0xf0)) {
  */        /* MPEG audio or video */
  /*        if (mpeg_read(buf, 2) != 2)
            return -1;
          len = (buf[0] << 8) | buf[1];
          if ((len > 0) && mpeg_seek(len, SEEK_CUR))
            return -1;

        }
        else {
          mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] VobSub: unknown header "
                 "0x%02X%02X%02X%02X\n", buf[0], buf[1], buf[2], buf[3]);
          return -1;
        }
    }
  }
  return 0;
}

*/
