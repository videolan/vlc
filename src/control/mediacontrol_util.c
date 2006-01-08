/*****************************************************************************
 * util.c: Utility functions and exceptions management
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Olivier Aubert <olivier.aubert@liris.univ-lyon1.fr>
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

#include <mediacontrol_internal.h>
#include <vlc/mediacontrol.h>

#include <vlc/intf.h>
#include <vlc/vout.h>
#include <vlc/aout.h>
#include <vlc_demux.h>

#include <vlc_osd.h>

#define HAS_SNAPSHOT 1

#ifdef HAS_SNAPSHOT
#include <snapshot.h>
#endif

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <errno.h>                                                 /* ENOMEM */
#include <stdio.h>
#include <ctype.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#    include <sys/types.h>
#endif

#define RAISE( c, m )  exception->code = c; \
                       exception->message = strdup(m);

vlc_int64_t mediacontrol_unit_convert( input_thread_t *p_input,
                                       mediacontrol_PositionKey from,
                                       mediacontrol_PositionKey to,
                                       vlc_int64_t value )
{
    if( to == from )
        return value;

    /* For all conversions, we need data from p_input */
    if( !p_input )
        return 0;

    switch( from )
    {
    case mediacontrol_MediaTime:
        if( to == mediacontrol_ByteCount )
        {
            /* FIXME */
            /* vlc < 0.8 API */
            /* return value * 50 * p_input->stream.i_mux_rate / 1000; */
            return 0;
        }
        if( to == mediacontrol_SampleCount )
        {
            double f_fps;

            if( demux2_Control( p_input->input.p_demux, DEMUX_GET_FPS, &f_fps ) || f_fps < 0.1 )
                return 0;
            else
                return( value * f_fps / 1000.0 );
        }
        /* Cannot happen */
        /* See http://catb.org/~esr/jargon/html/entry/can't-happen.html */
        break;

    case mediacontrol_SampleCount:
    {
        double f_fps;

	if( demux2_Control( p_input->input.p_demux, DEMUX_GET_FPS, &f_fps ) || f_fps < 0.1 )
            return 0;

        if( to == mediacontrol_ByteCount )
        {
            /* FIXME */
            /* vlc < 0.8 API */
/*             return ( vlc_int64_t )( value * 50 * p_input->stream.i_mux_rate / f_fps ); */
            return 0;
        }

        if( to == mediacontrol_MediaTime )
            return( vlc_int64_t )( value * 1000.0 / ( double )f_fps );

        /* Cannot happen */
        break;
    }
    case mediacontrol_ByteCount:
        /* FIXME */
        return 0;
/* vlc < 0.8 API: */

//         if( p_input->stream.i_mux_rate == 0 )
//             return 0;
// 
//         /* Convert an offset into milliseconds. Taken from input_ext-intf.c.
//            The 50 hardcoded constant comes from the definition of i_mux_rate :
//            i_mux_rate : the rate we read the stream (in units of 50 bytes/s) ;
//            0 if undef */
//         if( to == mediacontrol_MediaTime )
//             return ( vlc_int64_t )( 1000 * value / 50 / p_input->stream.i_mux_rate );
// 
//         if( to == mediacontrol_SampleCount )
//         {
//             double f_fps;
//             if( demux2_Control( p_input->input.p_demux, DEMUX_GET_FPS, &f_fps ) || f_fps < 0.1 )
//                 return 0;
//             else
//                 return ( vlc_int64_t )( value * f_fps / 50 / p_input->stream.i_mux_rate );
//         }
        /* Cannot happen */
        break;
    }
    /* Cannot happen */
    return 0;
}

/* Converts a mediacontrol_Position into a time in microseconds in
   movie clock time */
vlc_int64_t
mediacontrol_position2microsecond( input_thread_t* p_input, const mediacontrol_Position * pos )
{
    switch( pos->origin )
    {
    case mediacontrol_AbsolutePosition:
        return ( 1000 * mediacontrol_unit_convert( p_input,
                                                   pos->key, /* from */
                                                   mediacontrol_MediaTime,  /* to */
                                                   pos->value ) );
        break;
    case mediacontrol_RelativePosition:
    {
        vlc_int64_t l_pos;
        vlc_value_t val;

        val.i_time = 0;
        if( p_input )
        {
            var_Get( p_input, "time", &val );
        }

        l_pos = 1000 * mediacontrol_unit_convert( p_input,
                                                  pos->key,
                                                  mediacontrol_MediaTime,
                                                  pos->value );
        return val.i_time + l_pos;
        break;
    }
    case mediacontrol_ModuloPosition:
    {
        vlc_int64_t l_pos;
        vlc_value_t val;

        val.i_time = 0;
        if( p_input )
        {
            var_Get( p_input, "length", &val );
        }

        if( val.i_time > 0)
        {
            l_pos = ( 1000 * mediacontrol_unit_convert( p_input,
                                                        pos->key,
                                                        mediacontrol_MediaTime,
                                                        pos->value ) );
        }
        else
            l_pos = 0;

        return l_pos % val.i_time;
        break;
    }
    }
    return 0;
}

mediacontrol_RGBPicture*
mediacontrol_RGBPicture__alloc( int datasize )
{
    mediacontrol_RGBPicture* pic;

    pic = ( mediacontrol_RGBPicture * )malloc( sizeof( mediacontrol_RGBPicture ) );
    if( ! pic )
        return NULL;

    pic->size = datasize;
    pic->data = ( char* )malloc( datasize );
    return pic;
}

void
mediacontrol_RGBPicture__free( mediacontrol_RGBPicture* pic )
{
    if( pic )
        free( pic->data );
    free( pic );
}

mediacontrol_PlaylistSeq*
mediacontrol_PlaylistSeq__alloc( int size )
{
    mediacontrol_PlaylistSeq* ps;

    ps =( mediacontrol_PlaylistSeq* )malloc( sizeof( mediacontrol_PlaylistSeq ) );
    if( ! ps )
        return NULL;

    ps->size = size;
    ps->data = ( char** )malloc( size * sizeof( char* ) );
    return ps;
}

void
mediacontrol_PlaylistSeq__free( mediacontrol_PlaylistSeq* ps )
{
    if( ps )
    {
        int i;
        for( i = 0 ; i < ps->size ; i++ )
            free( ps->data[i] );
    }
    free( ps->data );
    free( ps );
}

mediacontrol_Exception*
mediacontrol_exception_init( mediacontrol_Exception *exception )
{
    if( exception == NULL )
    {
        exception = ( mediacontrol_Exception* )malloc( sizeof( mediacontrol_Exception ) );
    }

    exception->code = 0;
    exception->message = NULL;
    return exception;
}

void
mediacontrol_exception_free( mediacontrol_Exception *exception )
{
    if( ! exception )
        return;

    free( exception->message );
    free( exception );
}

mediacontrol_RGBPicture*
_mediacontrol_createRGBPicture( int i_width, int i_height, long i_chroma, vlc_int64_t l_date,
                                char* p_data, int i_datasize )
{
    mediacontrol_RGBPicture *retval;

    retval = mediacontrol_RGBPicture__alloc( i_datasize );
    if( retval )
    {
        retval->width  = i_width;
        retval->height = i_height;
        retval->type   = i_chroma;
        retval->date   = l_date;
        retval->size   = i_datasize;
        memcpy( retval->data, p_data, i_datasize );
    }
    return retval;
}
