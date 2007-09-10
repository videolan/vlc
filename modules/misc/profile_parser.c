/*****************************************************************************
 * profile_parser.c : VLC Streaming Profile parser
 *****************************************************************************
 * Copyright (C) 2003-2006 the VideoLAN team
 * $Id: rtsp.c 16204 2006-08-03 16:58:10Z zorglub $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <vlc/vlc.h>
#include <vlc_stream.h>
#include <vlc_streaming.h>
#include "vlc_xml.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );

vlc_module_begin();
    set_capability( "profile parser", 1 );
    set_callbacks( Open, NULL );
vlc_module_end();

static int Open( vlc_object_t *p_this )
{
    profile_parser_t *p_parser = (profile_parser_t *)p_this->p_private;
    stream_t *p_stream = stream_UrlNew( p_this, p_parser->psz_profile );
    xml_t *p_xml;
    xml_reader_t *p_reader;
    int i_ret;
    char *psz_elname = NULL;

    /* Open the profile and get a XML reader from it */
    if( !p_stream )
    {
        msg_Err( p_this, "failed to parse profile %s", p_parser->psz_profile );
        return VLC_EGENERIC;
    }
    p_xml = xml_Create( p_this );
    if( !p_xml ) return VLC_EGENERIC;
    p_reader = xml_ReaderCreate( p_xml, p_stream );

    if( xml_ReaderRead( p_reader ) != 1 ||
        xml_ReaderNodeType( p_reader ) != XML_READER_STARTELEM )
    {
        msg_Err( p_this, "invalid file (invalid root)" );
        return VLC_EGENERIC;
    }

    /* Here goes the real parsing */

    while( (i_ret = xml_ReaderRead( p_reader ) ) == 1 )
    {
        int i_type = xml_ReaderNodeType( p_reader );
        switch( i_type )
        {
        case -1:
            /* ERROR : Bail out */
            return -1;

        case XML_READER_STARTELEM:
            FREE( psz_elname );
            psz_elname = xml_ReaderName( p_reader );
            if( !psz_elname ) return VLC_EGENERIC;
            printf( "<%s", psz_elname );
            break;

        case XML_READER_TEXT:
            break;
        case XML_READER_ENDELEM:
            FREE( psz_elname );
            psz_elname = xml_ReaderName( p_reader );
            if( !psz_elname ) return VLC_EGENERIC;
            printf( ">" );
            break;
        }
    }

    if( i_ret != 0 )
    {
        msg_Err( p_this, "parse error" );
        return VLC_EGENERIC;
    }

    if( p_reader ) xml_ReaderDelete( p_xml, p_reader );
    if( p_xml ) xml_Delete( p_xml );

    return VLC_SUCCESS;
}
