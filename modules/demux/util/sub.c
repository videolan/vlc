/*****************************************************************************
 * sub.c
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: sub.c,v 1.4 2003/02/08 19:10:21 massiot Exp $
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

static int  sub_MicroDvdRead( FILE *p_file, subtitle_t *p_subtitle, mtime_t i_microsecperframe );
static int  sub_SubRipRead( FILE *p_file, subtitle_t *p_subtitle, mtime_t i_microsecperframe );
static int  sub_SSA1Read( FILE *p_file, subtitle_t *p_subtitle, mtime_t i_microsecperframe );
static int  sub_SSA2_4Read( FILE *p_file, subtitle_t *p_subtitle, mtime_t i_microsecperframe );

static void sub_fix( subtitle_demux_t *p_sub );

static char *ppsz_sub_type[] = { "microdvd", "subrip", "ssa1", "ssa2-4", NULL };
/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define SUB_FPS_LONGTEXT \
    "Override frames per second. " \
    "It will work only with MicroDVD"
#define SUB_TYPE_LONGTEXT \
    "One from \"microdvd\", \"subrip\", \"ssa1\", \"ssa2-4\" " \
    "(nothing for autodetection, it should always work)"

vlc_module_begin();
    set_description( _("text subtitle demux") );
    set_capability( "subtitle demux", 12 );
    add_category_hint( "subtitle", NULL );
        add_string( "sub-file", NULL, NULL,
                    "subtitle file name", "subtitle file name" );
        add_float( "sub-fps", 0.0, NULL, 
                   "override frames per second",
                   SUB_FPS_LONGTEXT );
        add_integer( "sub-delay", 0, NULL,
                     "delay subtitles (in 1/10s)", 
                     "delay subtitles (in 1/10s)" );
        add_string_from_list( "sub-type", NULL, ppsz_sub_type, NULL,
                              "subtitle type", 
                              SUB_TYPE_LONGTEXT );
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
/*****************************************************************************
 * sub_open: Open a subtitle file and add subtitle ES
 *****************************************************************************/
static int  sub_open ( subtitle_demux_t *p_sub, 
                       input_thread_t  *p_input,
                       char     *psz_name,
                       mtime_t i_microsecperframe )
{
    FILE *p_file;
    char    *psz_file_type;
    char    buffer[MAX_LINE + 1];
    int     i_try;
    int     i_sub_type;
    int     i_max;
    int (*pf_read_subtitle)( FILE *p_file, subtitle_t *p_subtitle, mtime_t i_microsecperframe ) = NULL;
   
        
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
            return( -1 );
        }
    }

    if(  config_GetFloat( p_sub, "sub-fps" ) >= 1.0 )
    {
        i_microsecperframe = (mtime_t)( (float)1000000 / 
                                        config_GetFloat( p_sub, "sub-fps" ) );
    }
    else if( i_microsecperframe <= 0 )
    {
        i_microsecperframe = 40000; /* default: 25fps */
    }

    /* *** Open the file *** */
    if( !( p_file = fopen( psz_name, "rb" ) ) )
    {
        msg_Err( p_sub, "cannot open `%s' subtitle file", psz_name );

    }
    else
    {
        msg_Dbg( p_sub, "opened `%s'", psz_name );
    }

    psz_file_type = config_GetPsz( p_sub, "sub-type" );
    if( psz_file_type && *psz_file_type)
    {
        if( !strcmp( psz_file_type, "microdvd" ) )
        {
            i_sub_type = SUB_TYPE_MICRODVD;
        }
        else if( !strcmp( psz_file_type, "subrip" ) )
        {
            i_sub_type = SUB_TYPE_SUBRIP;
        }
        else if( !strcmp( psz_file_type, "ssa1" ) )
        {
            i_sub_type = SUB_TYPE_SSA1;
        }
        else if( !strcmp( psz_file_type, "ssa2-4" ) )
        {
            i_sub_type = SUB_TYPE_SSA2_4;
        }
        else
        {
            i_sub_type = SUB_TYPE_UNKNOWN;
        }
    }
    else
    {
        i_sub_type = SUB_TYPE_UNKNOWN;
    }
   
    /* *** Now try to autodetect subtitle format *** */
    if( i_sub_type == SUB_TYPE_UNKNOWN )
    {
        msg_Dbg( p_input, "trying to autodetect file format" );
        for( i_try = 0; i_try < MAX_TRY; i_try++ )
        {
            int i_dummy;
            if( fgets( buffer, MAX_LINE, p_file ) <= 0 )
            {
                break;
            }

            if( sscanf( buffer, "{%d}{%d}", &i_dummy, &i_dummy ) == 2 ||
                sscanf( buffer, "{%d}{}", &i_dummy ) == 1)
            {
                i_sub_type = SUB_TYPE_MICRODVD;
                break;
            }
            else if( sscanf( buffer, 
                             "%d:%d:%d,%d --> %d:%d:%d,%d",
                             &i_dummy,&i_dummy,&i_dummy,&i_dummy, 
                             &i_dummy,&i_dummy,&i_dummy,&i_dummy ) == 8 )
            {
                i_sub_type = SUB_TYPE_SUBRIP;
                break;
            }
            else if( sscanf( buffer, 
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
            }
            else if( !strcmp( buffer,
                              "Dialogue: Marked" ) )
            {
                i_sub_type = SUB_TYPE_SSA2_4; // could be wrong
            }
                              
            
        }
    }

    /* *** Load this file in memory *** */
    switch( i_sub_type )
    {
        case SUB_TYPE_MICRODVD:
            msg_Dbg( p_input, "detected MicroDVD format" );
            pf_read_subtitle = sub_MicroDvdRead;
            break;
        case SUB_TYPE_SUBRIP:
            msg_Dbg( p_input, "detected SubRIP format" );
            pf_read_subtitle = sub_SubRipRead;
            break;
        case SUB_TYPE_SSA1:
            msg_Dbg( p_input, "detected SSAv1 Script format" );
            pf_read_subtitle = sub_SSA1Read;
            break;
        case SUB_TYPE_SSA2_4:
            msg_Dbg( p_input, "detected SSAv2-4 Script format" );
            pf_read_subtitle = sub_SSA2_4Read;
            break;
        default:
            msg_Err( p_sub, "unknown subtitile file" );
            fclose( p_file );
            return( -1 );
    }
    
    if( fseek( p_file, 0L, SEEK_SET ) < 0 )
    {
        msg_Err( p_input, "cannot read file from begining" );
        fclose( p_file );
        return( -1 );
    }
    for( i_max = 0;; )
    {   
        if( p_sub->i_subtitles <= i_max )
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
        if( pf_read_subtitle( p_file, 
                              p_sub->subtitle + p_sub->i_subtitles, 
                              i_microsecperframe ) < 0 )
        {
            break;
        }
        p_sub->i_subtitles++;
    }
    msg_Dbg( p_sub, "loaded %d subtitles", p_sub->i_subtitles );

    
    /* *** Close the file *** */
    fclose( p_file );

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
    return( 0 );
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
        return( 0 );
    }
    else if( !p_sub->p_es->p_decoder_fifo && p_sub->i_previously_selected )
    {
        p_sub->i_previously_selected = 0;
        return( 0 );
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
        if( p_sub->p_es->p_decoder_fifo )
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
static int  sub_MicroDvdRead( FILE *p_file, subtitle_t *p_subtitle, mtime_t i_microsecperframe)
{
    /*
     * each line:
     *  {n1}{n2}Line1|Line2|Line3....
     * where n1 and n2 are the video frame number...
     *
     */
    char buffer[MAX_LINE + 1];
    char buffer_text[MAX_LINE + 1];
    uint32_t    i_start;
    uint32_t    i_stop;
    int         i;
    
    for( ;; )
    {
        if( fgets( buffer, MAX_LINE, p_file ) <= 0) 
        {
            return( -1 );
        }
        i_start = 0;
        i_stop  = 0;
        if( sscanf( buffer, "{%d}{}%[^\r\n]", &i_start, buffer_text ) == 2 ||
            sscanf( buffer, "{%d}{%d}%[^\r\n]", &i_start, &i_stop, buffer_text ) == 3)
        {
            break;
        }
    }
    /* replace | by \n */
    for( i = 0; i < MAX_LINE; i++ )
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

static int  sub_SubRipRead( FILE *p_file, subtitle_t *p_subtitle, mtime_t i_microsecperframe )
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
    char buffer[MAX_LINE + 1];
    char buffer_text[ 10 * MAX_LINE];
    int  i_buffer_text;
    mtime_t     i_start;
    mtime_t     i_stop;

    for( ;; )
    {
        int h1, m1, s1, d1, h2, m2, s2, d2;
        if( fgets( buffer, MAX_LINE, p_file ) <= 0)
        {
            return( -1 );
        }
        if( sscanf( buffer,
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
                if( fgets( buffer, MAX_LINE, p_file ) <= 0) 
                {
                    return( -1 );
                }
                buffer[MAX_LINE] = '\0'; // just in case
                i_len = strlen( buffer );
                if( buffer[0] == '\r' || buffer[0] == '\n' || i_len <= 1 )
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
                                buffer,
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


static int  sub_SSARead( FILE *p_file, subtitle_t *p_subtitle, mtime_t i_microsecperframe, int i_comma_count )
{
    char buffer[MAX_LINE + 1];
    char buffer_text[ 10 * MAX_LINE];
    char *p_buffer_text;
    mtime_t     i_start;
    mtime_t     i_stop;
    int         i_comma;
    int         i_text;
    
    for( ;; )
    {
        int h1, m1, s1, c1, h2, m2, s2, c2;
        int i_dummy;
        if( fgets( buffer, MAX_LINE, p_file ) <= 0) 
        {
            return( -1 );
        }
        if( sscanf( buffer, 
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

static int  sub_SSA1Read( FILE *p_file, subtitle_t *p_subtitle, mtime_t i_microsecperframe )
{
    return( sub_SSARead( p_file, p_subtitle, i_microsecperframe, 8 ) );
}
static int  sub_SSA2_4Read( FILE *p_file, subtitle_t *p_subtitle, mtime_t i_microsecperframe )
{
    return( sub_SSARead( p_file, p_subtitle, i_microsecperframe, 9 ) );
}

