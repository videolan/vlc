/*****************************************************************************
 * sub.c: subtitle demux for external subtitle files
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
#include <ctype.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include "vlc_video.h"
#include <codecs.h>

#include "sub.h"

#if (!defined( WIN32 ) || defined(__MINGW32__))
#    include <dirent.h>
#endif

#define DVD_VIDEO_LB_LEN 2048

static int  Open ( vlc_object_t *p_this );

static int  sub_open ( subtitle_demux_t *p_sub,
                       input_thread_t  *p_input,
                       char  *psz_name,
                       mtime_t i_microsecperframe );
static int  sub_demux( subtitle_demux_t *p_sub, mtime_t i_maxdate );
static int  sub_seek ( subtitle_demux_t *p_sub, mtime_t i_date );
static void sub_close( subtitle_demux_t *p_sub );

static void sub_fix( subtitle_demux_t *p_sub );

static char *ppsz_sub_type[] = { "auto", "microdvd", "subrip", "subviewer", "ssa1",
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
    add_float( "sub-fps", 0.0, NULL,
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
static int  sub_SubViewer   ( subtitle_demux_t *p_sub, text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe );
static int  sub_SSARead     ( subtitle_demux_t *p_sub, text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe );
static int  sub_Vplayer     ( subtitle_demux_t *p_sub, text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe );
static int  sub_Sami        ( subtitle_demux_t *p_sub, text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe );
static int  sub_VobSubIDX   ( subtitle_demux_t *p_sub, text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe );

static int  DemuxVobSub     ( subtitle_demux_t *, block_t * );

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
    { "subviewer",  SUB_TYPE_SUBVIEWER, "SubViewer",sub_SubViewer },
    { "ssa1",       SUB_TYPE_SSA1,      "SSA-1",    sub_SSARead },
    { "ssa2-4",     SUB_TYPE_SSA2_4,    "SSA-2/3/4",sub_SSARead },
    { "vplayer",    SUB_TYPE_VPLAYER,   "VPlayer",  sub_Vplayer },
    { "sami",       SUB_TYPE_SAMI,      "SAMI",     sub_Sami },
    { "vobsub",     SUB_TYPE_VOBSUB,    "VobSub",   sub_VobSubIDX },
    { NULL,         SUB_TYPE_UNKNOWN,   "Unknown",  NULL }
};

/*****************************************************************************
 * sub_open: Open a subtitle file and add subtitle ES
 *****************************************************************************/
static int sub_open( subtitle_demux_t *p_sub, input_thread_t  *p_input,
                     char *psz_name, mtime_t i_microsecperframe )
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
    p_sub->p_vobsub_file = 0;
    p_sub->i_original_mspf = i_microsecperframe;
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
    else if( val.f_float == 0 )
    {
        /* No value given */
        i_microsecperframe = 0;
    }
    else if( val.f_float <= 0 )
    {
        /* invalid value, default = 25fps */
        i_microsecperframe = 40000;
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

            if( strcasestr( s, "<SAMI>" ) )
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
            else if( strcasestr( s, "This is a Sub Station Alpha v4 script" ) )
            {
                i_sub_type = SUB_TYPE_SSA2_4; /* I hope this will work */
                break;
            }
            else if( !strncasecmp( s, "Dialogue: Marked", 16  ) )
            {
                i_sub_type = SUB_TYPE_SSA2_4; /* could be wrong */
                break;
            }
            else if( strcasestr( s, "[INFORMATION]" ) )
            {
                i_sub_type = SUB_TYPE_SUBVIEWER; /* I hope this will work */
                break;
            }
            else if( sscanf( s, "%d:%d:%d:", &i_dummy, &i_dummy, &i_dummy ) == 3 ||
                     sscanf( s, "%d:%d:%d ", &i_dummy, &i_dummy, &i_dummy ) == 3 )
            {
                i_sub_type = SUB_TYPE_VPLAYER;
                break;
            }
            else if( strcasestr( s, "# VobSub index file" ) )
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
            if( !( p_sub->subtitle = realloc( p_sub->subtitle,
                                              sizeof(subtitle_t) * i_max ) ) )
            {
                msg_Err( p_sub, "out of memory");
                return VLC_ENOMEM;
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
    vlc_value_t    val;
    mtime_t i_delay;

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

    if( p_sub->i_sub_type != SUB_TYPE_VOBSUB )
    {
        var_Get( p_sub, "sub-delay", &val );
        i_delay = (mtime_t) val.i_int * 100000;
        while( p_sub->i_subtitle < p_sub->i_subtitles &&
               p_sub->subtitle[p_sub->i_subtitle].i_start < i_maxdate - i_delay )
        {
            block_t *p_block;
            int i_len = strlen( p_sub->subtitle[p_sub->i_subtitle].psz_text ) + 1;

            if( i_len <= 1 )
            {
                /* empty subtitle */
                p_sub->i_subtitle++;
                continue;
            }

            if( ( p_block = block_New( p_sub->p_input, i_len ) ) == NULL )
            {
                p_sub->i_subtitle++;
                continue;
            }

            /* XXX we should convert all demuxers to use es_out_Control to set              * pcr and then remove that */
            if( i_delay != 0 )
            {
                p_sub->subtitle[p_sub->i_subtitle].i_start += i_delay;
                p_sub->subtitle[p_sub->i_subtitle].i_stop += i_delay;
            }

            if( p_sub->subtitle[p_sub->i_subtitle].i_start < 0 )
            {
                p_sub->i_subtitle++;
                continue;
            }

            p_block->i_pts = p_sub->subtitle[p_sub->i_subtitle].i_start;
            p_block->i_dts = p_block->i_pts;
            if( p_sub->subtitle[p_sub->i_subtitle].i_stop > 0 )
            {
                p_block->i_length =
                    p_sub->subtitle[p_sub->i_subtitle].i_stop - p_block->i_pts;
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
    }
    else
    {
        while( p_sub->i_subtitle < p_sub->i_subtitles &&
               p_sub->subtitle[p_sub->i_subtitle].i_start < i_maxdate )
        {
            int i_pos = p_sub->subtitle[p_sub->i_subtitle].i_vobsub_location;
            block_t *p_block;
            int i_size = 0;

            /* first compute SPU size */
            if( p_sub->i_subtitle + 1 < p_sub->i_subtitles )
            {
                i_size = p_sub->subtitle[p_sub->i_subtitle+1].i_vobsub_location - i_pos;
            }
            if( i_size <= 0 ) i_size = 65535;   /* Invalid or EOF */

            /* Seek at the right place (could be avoid if sub_seek is fixed to do his job) */
            if( fseek( p_sub->p_vobsub_file, i_pos, SEEK_SET ) )
            {
                msg_Warn( p_sub, "cannot seek at right vobsub location %d", i_pos );
                p_sub->i_subtitle++;
                continue;
            }

            /* allocate a packet */
            if( ( p_block = block_New( p_sub, i_size ) ) == NULL )
            {
                p_sub->i_subtitle++;
                continue;
            }

            /* read data */
            p_block->i_buffer = fread( p_block->p_buffer, 1, i_size, p_sub->p_vobsub_file );
            if( p_block->i_buffer <= 6 )
            {
                block_Release( p_block );
                p_sub->i_subtitle++;
                continue;
            }

            /* pts */
            p_block->i_pts = p_sub->subtitle[p_sub->i_subtitle].i_start;

            /* demux this block */
            DemuxVobSub( p_sub, p_block );

            p_sub->i_subtitle++;
        }
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
#if 0
    /* We do not do this here anymore */
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
#endif
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
    
    p_subtitle->i_start = 0;
    p_subtitle->i_stop  = 0;
    p_subtitle->i_vobsub_location  = 0;
    p_subtitle->psz_text = NULL;

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
    if( i_microsecperframe == 0)
    {
        i_microsecperframe = 40000;
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

    p_subtitle->i_start = 0;
    p_subtitle->i_stop  = 0;
    p_subtitle->i_vobsub_location  = 0;
    p_subtitle->psz_text = NULL;

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
                    /* If framerate is available, use sub-fps */
                    if( i_microsecperframe != 0 && p_sub->i_original_mspf != 0)
                    {
                        p_subtitle->i_start = (mtime_t)i_start *
                                              (mtime_t)p_sub->i_original_mspf /
                                              (mtime_t)i_microsecperframe;
                        p_subtitle->i_stop  = (mtime_t)i_stop  *
                                              (mtime_t)p_sub->i_original_mspf /
                                              (mtime_t)i_microsecperframe;
                    }
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

static int  sub_SubViewer( subtitle_demux_t *p_sub, text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe )
{
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
    mtime_t     i_start;
    mtime_t     i_stop;

    p_subtitle->i_start = 0;
    p_subtitle->i_stop  = 0;
    p_subtitle->i_vobsub_location  = 0;
    p_subtitle->psz_text = NULL;

    for( ;; )
    {
        int h1, m1, s1, d1, h2, m2, s2, d2;
        if( ( s = text_get_line( txt ) ) == NULL )
        {
            return( VLC_EGENERIC );
        }
        if( sscanf( s,
                    "%d:%d:%d.%d,%d:%d:%d.%d",
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
                int i_len, i;
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

                    /* replace [br] by \n */
                    for( i = 0; i < strlen( buffer_text ) - 3; i++ )
                    {
                        if( buffer_text[i] == '[' && buffer_text[i+1] == 'b' &&
                            buffer_text[i+2] == 'r' && buffer_text[i+3] == ']' )
                        {
                            char *temp = buffer_text + i + 1;
                            buffer_text[i] = '\n';
                            memmove( temp, temp+3, strlen( temp-3 ));
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


static int  sub_SSARead( subtitle_demux_t *p_sub, text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe )
{
    char buffer_text[ 10 * MAX_LINE];
    char *s;
    mtime_t     i_start;
    mtime_t     i_stop;

    p_subtitle->i_start = 0;
    p_subtitle->i_stop  = 0;
    p_subtitle->i_vobsub_location  = 0;
    p_subtitle->psz_text = NULL;

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
     *
     */
    char *p;
    char buffer_text[MAX_LINE + 1];
    mtime_t    i_start;
    unsigned int i;
    
    p_subtitle->i_start = 0;
    p_subtitle->i_stop  = 0;
    p_subtitle->i_vobsub_location  = 0;
    p_subtitle->psz_text = NULL;

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
        if( ( p = text_get_line( txt ) ) == NULL )
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

static int  sub_Sami( subtitle_demux_t *p_sub, text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe )
{
    char *p;
    int i_start;

    int  i_text;
    char buffer_text[10*MAX_LINE + 1];

    p_subtitle->i_start = 0;
    p_subtitle->i_stop  = 0;
    p_subtitle->i_vobsub_location  = 0;
    p_subtitle->psz_text = NULL;

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
                else if( strcasestr( p, "Start=" ) )
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

static int  sub_VobSubIDX( subtitle_demux_t *p_sub, text_t *txt, subtitle_t *p_subtitle, mtime_t i_microsecperframe)
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

    p_subtitle->i_start = 0;
    p_subtitle->i_stop  = 0;
    p_subtitle->i_vobsub_location  = 0;
    p_subtitle->psz_text = NULL;

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
    p_subtitle->psz_text = NULL;
    p_subtitle->i_vobsub_location = i_location;
    fprintf( stderr, "time: %x, location: %x\n", i_start, i_location );
    return( 0 );
}

static int  DemuxVobSub( subtitle_demux_t *p_demux, block_t *p_bk )
{
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
        if( p_demux->p_es && i_spu == 0 )
        {
            p_pkt->i_dts = p_pkt->i_pts = p_bk->i_pts;
            p_pkt->i_length = 0;
            es_out_Send( p_demux->p_input->p_es_out, p_demux->p_es, p_pkt );

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
