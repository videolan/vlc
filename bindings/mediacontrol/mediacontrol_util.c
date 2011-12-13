/*****************************************************************************
 * mediacontrol_util.c: Utility functions and exceptions management
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "mediacontrol_internal.h"
#include <vlc/mediacontrol.h>

#include <vlc_common.h>
#include <vlc_vout.h>
#include <vlc_osd.h>

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <stdio.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#include <sys/types.h>

libvlc_time_t private_mediacontrol_unit_convert( libvlc_media_player_t *p_media_player,
                                                 mediacontrol_PositionKey from,
                                                 mediacontrol_PositionKey to,
                                                 int64_t value )
{
    if( to == from )
        return value;

    if( !p_media_player )
        return 0;

    switch( from )
    {
    case mediacontrol_MediaTime:
        if( to == mediacontrol_ByteCount )
        {
            /* FIXME Unsupported */
            /* vlc < 0.8 API */
            /* return value * 50 * p_input->stream.i_mux_rate / 1000; */
            return 0;
        }
        if( to == mediacontrol_SampleCount )
        {
            double f_fps;

            f_fps = libvlc_media_player_get_rate( p_media_player );
            if( f_fps < 0 )
                return 0;
            else
                return( value * f_fps / 1000.0 );
        }
        /* Cannot happen */
        /* See http://catb.org/~esr/jargon/html/entry/can-t-happen.html */
        break;

    case mediacontrol_SampleCount:
    {
        double f_fps;

        f_fps = libvlc_media_player_get_rate( p_media_player );
        if( f_fps < 0 )
            return 0;

        if( to == mediacontrol_ByteCount )
        {
            /* FIXME */
            /* vlc < 0.8 API */
/*             return ( int64_t )( value * 50 * p_input->stream.i_mux_rate / f_fps ); */
            return 0;
        }

        if( to == mediacontrol_MediaTime )
            return( int64_t )( value * 1000.0 / ( double )f_fps );

        /* Cannot happen */
        break;
    }
    case mediacontrol_ByteCount:
        /* FIXME */
        return 0;
    }
    /* Cannot happen */
    return 0;
}

/* Converts a mediacontrol_Position into a time in microseconds in
   movie clock time */
libvlc_time_t
private_mediacontrol_position2microsecond( libvlc_media_player_t * p_media_player,
                                           const mediacontrol_Position * pos )
{
    switch( pos->origin )
    {
    case mediacontrol_AbsolutePosition:
        return ( 1000 * private_mediacontrol_unit_convert( p_media_player,
                                                   pos->key, /* from */
                                                   mediacontrol_MediaTime,  /* to */
                                                   pos->value ) );
        break;
    case mediacontrol_RelativePosition:
    {
        libvlc_time_t l_time = 0;
        libvlc_time_t l_pos = 0;

        l_time = libvlc_media_player_get_time( p_media_player );
        /* Ignore exception, we will assume a 0 time value */

        l_pos = private_mediacontrol_unit_convert( p_media_player,
                                                   pos->key,
                                                   mediacontrol_MediaTime,
                                                   pos->value );
        return 1000 * ( l_time + l_pos );
        break;
    }
    case mediacontrol_ModuloPosition:
    {
        libvlc_time_t l_time = 0;
        libvlc_time_t l_length = 0;
        libvlc_time_t l_pos = 0;

        l_length = libvlc_media_player_get_length( p_media_player );
        if( l_length <= 0 )
            return 0;

        l_time = libvlc_media_player_get_time( p_media_player );
        /* Ignore exception, we will assume a 0 time value */

        l_pos = private_mediacontrol_unit_convert( p_media_player,
                                                   pos->key,
                                                   mediacontrol_MediaTime,
                                                   pos->value );

        return 1000 * ( ( l_time + l_pos ) % l_length );
        break;
    }
    }
    return 0;
}

void
mediacontrol_RGBPicture__free( mediacontrol_RGBPicture* pic )
{
    if( pic )
    {
        free( pic->data );
        free( pic );
    }
}

void
mediacontrol_StreamInformation__free( mediacontrol_StreamInformation* p_si )
{
  if( p_si )
  {
      free( p_si->url );
      free( p_si );
  }
}


mediacontrol_Exception*
mediacontrol_exception_create( void )
{
    mediacontrol_Exception* exception;

    exception = ( mediacontrol_Exception* )malloc( sizeof( mediacontrol_Exception ) );
    mediacontrol_exception_init( exception );
    return exception;
}

void
mediacontrol_exception_init( mediacontrol_Exception *exception )
{
    if( exception )
    {
        exception->code = 0;
        exception->message = NULL;
    }
}

void
mediacontrol_exception_cleanup( mediacontrol_Exception *exception )
{
    if( exception )
        free( exception->message );
}

void
mediacontrol_exception_free( mediacontrol_Exception *exception )
{
    mediacontrol_exception_cleanup( exception );
    free( exception );
}

/**
 * Allocates and initializes a mediacontrol_RGBPicture object.
 *
 * @param i_width: picture width
 * @param i_height: picture width
 * @param i_chroma: picture chroma
 * @param l_date: picture timestamp
 * @param p_data: pointer to the data. The data will be directly used, not copied.
 * @param i_datasize: data size in bytes
 *
 * @return the new object, or NULL on error.
 */
mediacontrol_RGBPicture*
private_mediacontrol_createRGBPicture( int i_width, int i_height, long i_chroma, int64_t l_date,
                                       char* p_data, int i_datasize )
{
    mediacontrol_RGBPicture *retval;

    retval = ( mediacontrol_RGBPicture * )malloc( sizeof( mediacontrol_RGBPicture ) );
    if( retval )
    {
        retval->width  = i_width;
        retval->height = i_height;
        retval->type   = i_chroma;
        retval->date   = l_date;
        retval->size   = i_datasize;
        retval->data   = p_data;
    }
    return retval;
}
