/*****************************************************************************
 * sub.c
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: sub.c,v 1.12 2003/04/14 03:23:30 fenrir Exp $
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
#include <string.h>
#include <errno.h>
#include <sys/types.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "video.h"

#include "sub.h"


static int  Open ( vlc_object_t *p_this );

static int  sub_open ( subtitle_demux_t *p_sub,
                       input_thread_t  *p_input,
                       char  *psz_name,
                       mtime_t i_microsecperframe );
static int  sub_demux( subtitle_demux_t *p_sub, mtime_t i_maxdate );
static int  sub_seek ( subtitle_demux_t *p_sub, mtime_t i_date );
static void sub_close( subtitle_demux_t *p_sub );

static void sub_fix( subtitle_demux_t *p_sub );

static char *ppsz_sub_type[] = { "microdvd", "subrip", "ssa1", "ssa2-4", "vplayer", "sami", NULL };


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define SUB_FPS_LONGTEXT \
    "Override frames per second. " \
    "It will work only with MicroDVD"
#define SUB_TYPE_LONGTEXT \
    "One from \"microdvd\", \"subrip\", \"ssa1\", \"ssa2-4\", \"vplayer\" \"sami\"" \
    "(nothing for autodetection, it should always work)"

vlc_module_begin();
    set_description( _("text subtitle demux") );
    set_capability( "subtitle demux", 12 );
    add_category_hint( "subtitle", NULL, VLC_TRUE );
        add_file( "sub-file", NULL, NULL,
                  "subtitle file name", "subtitle file name", VLC_TRUE );
        add_float( "sub-fps", 0.0, NULL,
                   "override frames per second",
                   SUB_FPS_LONGTEXT, VLC_TRUE );
        add_integer( "sub-delay", 0, NULL,
                     "delay subtitles (in 1/10s)",
                     "delay subtitles (in 1/10s)", VLC_TRUE );
        add_string_from_list( "sub-type", NULL, ppsz_sub_type, NULL,
                              "subtitle type",
                              SUB_TYPE_LONGTEXT, VLC_TRUE );
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

static int  sub_MicroDvdRead( text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe );
static int  sub_SubRipRead  ( text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe );
static int  sub_SSA1Read    ( text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe );
static int  sub_SSA2_4Read  ( text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe );
static int  sub_Vplayer     ( text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe );
static int  sub_Sami        ( text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe );

static struct
{
    char *psz_type_name;
    int  i_type;
    char *psz_name;
    int  (*pf_read_subtitle)    ( text_t *, subtitle_t*, mtime_t );
} sub_read_subtitle_function [] =
{
    { "microdvd",   SUB_TYPE_MICRODVD,  "MicroDVD", sub_MicroDvdRead },
    { "subrip",     SUB_TYPE_SUBRIP,    "SubRIP",   sub_SubRipRead },
    { "ssa1",       SUB_TYPE_SSA1,      "SSA-1",    sub_SSA1Read },
    { "ssa2-4",     SUB_TYPE_SSA2_4,    "SSA-2/3/4",sub_SSA2_4Read },
    { "vplayer",    SUB_TYPE_VPLAYER,   "VPlayer",  sub_Vplayer },
    { "sami",       SUB_TYPE_SAMI,      "SAMI",     sub_Sami },
    { NULL,         SUB_TYPE_UNKNOWN,   "Unknow",   NULL }
};

/*****************************************************************************
 * sub_open: Open a subtitle file and add subtitle ES
 *****************************************************************************/
static int  sub_open ( subtitle_demux_t *p_sub,
                       input_thread_t  *p_input,
                       char     *psz_name,
                       mtime_t i_microsecperframe )
{
    text_t  txt;

    int     i;
    char    *psz_file_type;
    int     i_sub_type;
    int     i_max;
    int (*pf_read_subtitle)( text_t *, subtitle_t *, mtime_t ) = NULL;

    p_sub->i_sub_type = SUB_TYPE_UNKNOWN;
    p_sub->p_es = NULL;
    p_sub->i_subtitles = 0;
    p_sub->subtitle = NULL;
    p_sub->p_input = p_input;

    if( !psz_name || !*psz_name)
    {
        psz_name = config_GetPsz( p_sub, "sub-file" );
        if( !psz_name || !*psz_name )
        {
            return VLC_EGENERIC;
        }
    }
    else
    {
        psz_name = strdup( psz_name );
    }

    /* *** load the file *** */
    if( text_load( &txt, psz_name ) )
    {
        msg_Err( p_sub, "cannot open `%s' subtitle file", psz_name );
        free( psz_name );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_sub, "opened `%s'", psz_name );
    free( psz_name );


    if(  config_GetFloat( p_sub, "sub-fps" ) >= 1.0 )
    {
        i_microsecperframe = (mtime_t)( (float)1000000 /
                                        config_GetFloat( p_sub, "sub-fps" ) );
    }
    else if( i_microsecperframe <= 0 )
    {
        i_microsecperframe = 40000; /* default: 25fps */
    }

    psz_file_type = config_GetPsz( p_sub, "sub-type" );
    if( psz_file_type && *psz_file_type)
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
                         psz_file_type ) )
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
    FREE( psz_file_type );

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

            if( strstr( s, "<SAMI>" ) )
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
                    i_sub_type = SUB_TYPE_SSA2_4; // I hop this will work
                }
                break;
            }
            else if( strstr( s, "This is a Sub Station Alpha v4 script" ) )
            {
                i_sub_type = SUB_TYPE_SSA2_4; // I hop this will work
            }
            else if( !strncmp( s, "Dialogue: Marked", 16  ) )
            {
                i_sub_type = SUB_TYPE_SSA2_4; // could be wrong
                break;
            }
            else if( sscanf( s, "%d:%d:%d:", &i_dummy, &i_dummy, &i_dummy ) == 3 ||
                     sscanf( s, "%d:%d:%d ", &i_dummy, &i_dummy, &i_dummy ) == 3 )
            {
                i_sub_type = SUB_TYPE_VPLAYER;
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
            msg_Dbg( p_input, "unknown subtitile file" );
            text_unload( &txt );
            return VLC_EGENERIC;
        }

        if( sub_read_subtitle_function[i].i_type == i_sub_type )
        {
            msg_Dbg( p_input,
                    "detected %s format",
                    sub_read_subtitle_function[i].psz_name );
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
                p_sub->subtitle = realloc( p_sub->subtitle,
                                           sizeof( subtitle_t ) * i_max );
            }
            else
            {
                p_sub->subtitle = malloc( sizeof( subtitle_t ) * i_max );
            }
        }
        if( pf_read_subtitle( &txt,
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
    p_sub->i_subtitle = 0;  // will be modified by sub_fix
    sub_fix( p_sub );

    /* *** add subtitle ES *** */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_sub->p_es = input_AddES( p_input,
                               p_input->stream.p_selected_program,
                               0xff,    // FIXME
                               0 );
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    p_sub->p_es->i_stream_id = 0xff;    // FIXME
    p_sub->p_es->i_fourcc    = VLC_FOURCC( 's','u','b','t' );
    p_sub->p_es->i_cat       = SPU_ES;

    p_sub->i_previously_selected = 0;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * sub_demux: Send subtitle to decoder until i_maxdate
 *****************************************************************************/
static int  sub_demux( subtitle_demux_t *p_sub, mtime_t i_maxdate )
{

    if( p_sub->p_es->p_decoder_fifo && !p_sub->i_previously_selected )
    {
        p_sub->i_previously_selected = 1;
        p_sub->pf_seek( p_sub, i_maxdate );
        return VLC_SUCCESS;
    }
    else if( !p_sub->p_es->p_decoder_fifo && p_sub->i_previously_selected )
    {
        p_sub->i_previously_selected = 0;
        return VLC_SUCCESS;
    }

    while( p_sub->i_subtitle < p_sub->i_subtitles &&
           p_sub->subtitle[p_sub->i_subtitle].i_start < i_maxdate )
    {
        pes_packet_t    *p_pes;
        data_packet_t   *p_data;

        int i_len;

        i_len = strlen( p_sub->subtitle[p_sub->i_subtitle].psz_text ) + 1;

        if( i_len <= 1 )
        {
            /* empty subtitle */
            p_sub->i_subtitle++;
            continue;
        }
        if( !( p_pes = input_NewPES( p_sub->p_input->p_method_data ) ) )
        {
            p_sub->i_subtitle++;
            continue;
        }

        if( !( p_data = input_NewPacket( p_sub->p_input->p_method_data,
                                         i_len ) ) )
        {
            input_DeletePES( p_sub->p_input->p_method_data, p_pes );
            p_sub->i_subtitle++;
            continue;
        }
        p_data->p_payload_end = p_data->p_payload_start + i_len;

        p_pes->i_pts =
            input_ClockGetTS( p_sub->p_input,
                              p_sub->p_input->stream.p_selected_program,
                              p_sub->subtitle[p_sub->i_subtitle].i_start*9/100);
        if( p_sub->subtitle[p_sub->i_subtitle].i_stop > 0 )
        {
            /* FIXME kludge ...
             * i_dts means end of display...
             */
            p_pes->i_dts =
                input_ClockGetTS( p_sub->p_input,
                              p_sub->p_input->stream.p_selected_program,
                              p_sub->subtitle[p_sub->i_subtitle].i_stop *9/100);
        }
        else
        {
            p_pes->i_dts = 0;
        }
        p_pes->i_nb_data = 1;
        p_pes->p_first =
            p_pes->p_last = p_data;
        p_pes->i_pes_size = i_len;

        memcpy( p_data->p_payload_start,
                p_sub->subtitle[p_sub->i_subtitle].psz_text,
                i_len );
        if( p_sub->p_es->p_decoder_fifo && p_pes->i_pts > 0 )
        {

            input_DecodePES( p_sub->p_es->p_decoder_fifo, p_pes );
        }
        else
        {
            input_DeletePES( p_sub->p_input->p_method_data, p_pes );
        }

        p_sub->i_subtitle++;
    }
    return( 0 );
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
}
/*****************************************************************************
 *
 * sub_fix: fix time stamp and order of subtitle
 *****************************************************************************/
static void  sub_fix( subtitle_demux_t *p_sub )
{
    int     i;
    mtime_t i_delay;
    int     i_index;
    int     i_done;

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
    i_delay = (mtime_t)config_GetInt( p_sub, "sub-delay" ) * 100000;
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
static int  sub_MicroDvdRead( text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe)
{
    /*
     * each line:
     *  {n1}{n2}Line1|Line2|Line3....
     * where n1 and n2 are the video frame number...
     *
     */
    char *s;

    char buffer_text[MAX_LINE + 1];
    uint32_t    i_start;
    uint32_t    i_stop;
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

static int  sub_SubRipRead( text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe )
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
                    // empty line -> end of this subtitle
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


static int  sub_SSARead( text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe, int i_comma_count )
{
    char buffer_text[ 10 * MAX_LINE];
    char *s;
    char *p_buffer_text;
    mtime_t     i_start;
    mtime_t     i_stop;
    int         i_comma;
    int         i_text;

    for( ;; )
    {
        int h1, m1, s1, c1, h2, m2, s2, c2;
        int i_dummy;

        if( ( s = text_get_line( txt ) ) == NULL )
        {
            return( VLC_EGENERIC );
        }
        if( sscanf( s,
                    "Dialogue: Marked=%d,%d:%d:%d.%d,%d:%d:%d.%d,%[^\r\n]",
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

            p_buffer_text = buffer_text;
            i_comma = 3;
            while( i_comma < i_comma_count &&
                   *p_buffer_text != '\0' )
            {
                if( *p_buffer_text == ',' )
                {
                    i_comma++;
                }
                p_buffer_text++;
            }
            p_subtitle->psz_text = malloc( strlen( p_buffer_text ) + 1);
            i_text = 0;
            while( *p_buffer_text )
            {
                if( *p_buffer_text == '\\' && ( *p_buffer_text =='n' || *p_buffer_text =='N' ) )
                {
                    p_subtitle->psz_text[i_text] = '\n';
                    i_text++;
                    p_buffer_text += 2;
                }
                else if( *p_buffer_text == '{' && *p_buffer_text == '\\')
                {
                    while( *p_buffer_text && *p_buffer_text != '}' )
                    {
                        p_buffer_text++;
                    }
                }
                else
                {
                    p_subtitle->psz_text[i_text] = *p_buffer_text;
                    i_text++;
                    p_buffer_text++;
                }
            }
            p_subtitle->psz_text[i_text] = '\0';
            p_subtitle->i_start = i_start;
            p_subtitle->i_stop = i_stop;
            return( 0 );
        }
    }
}

static int  sub_SSA1Read( text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe )
{
    return( sub_SSARead( txt, p_subtitle, i_microsecperframe, 8 ) );
}
static int  sub_SSA2_4Read( text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe )
{
    return( sub_SSARead( txt, p_subtitle, i_microsecperframe, 9 ) );
}

static int  sub_Vplayer( text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe)
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
        if( strstr( psz_start, psz_str ) )
        {
            char *s = strstr( psz_start, psz_str );

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
        if( strstr( p, psz_str ) )
        {
            char *s = strstr( p, psz_str );

            s += strlen( psz_str );

            return(  s);
        }
    }
}

static int  sub_Sami( text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe )
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
                if( !strncmp( p, "<br", 3 ) || !strncmp( p, "<BR", 3 ) )
                {
                    ADDC( '\n' );
                }
                else if( strstr( p, "Start=" ) )
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

