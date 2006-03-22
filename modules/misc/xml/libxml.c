/*****************************************************************************
 * libxml.c: XML parser using libxml2
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#include <stdlib.h>
#include <vlc/vlc.h>

#include "vlc_block.h"
#include "vlc_stream.h"
#include "vlc_xml.h"

#include <libxml/xmlreader.h>
#include <libxml/catalog.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("XML Parser (using libxml2)") );
    set_capability( "xml", 10 );
    set_callbacks( Open, Close );
vlc_module_end();

struct xml_reader_sys_t
{
    /* libxml2 reader context */
    xmlTextReaderPtr p_reader;
};

static xml_reader_t *ReaderCreate( xml_t *, stream_t * );
static void ReaderDelete( xml_reader_t * );
static int ReaderRead( xml_reader_t * );
static int ReaderNodeType( xml_reader_t * );
static char *ReaderName( xml_reader_t * );
static char *ReaderValue( xml_reader_t * );
static int ReaderNextAttr( xml_reader_t * );

static int ReaderUseDTD ( xml_reader_t *, vlc_bool_t );

static void CatalogLoad( xml_t *, const char * );
static void CatalogAdd( xml_t *, const char *, const char *, const char * );
static int StreamRead( void *p_context, char *p_buffer, int i_buffer );

/*****************************************************************************
 * Module initialization
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    xml_t *p_xml = (xml_t *)p_this;

    p_xml->pf_reader_create = ReaderCreate;
    p_xml->pf_reader_delete = ReaderDelete;

    p_xml->pf_catalog_load = CatalogLoad;
    p_xml->pf_catalog_add  = CatalogAdd;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Module deinitialization
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    return;
}

/*****************************************************************************
 * Catalogue functions
 *****************************************************************************/
static void CatalogLoad( xml_t *p_xml, const char *psz_filename )
{
    if( !psz_filename ) xmlInitializeCatalog();
    else xmlLoadCatalog( psz_filename );
}

static void CatalogAdd( xml_t *p_xml, const char *psz_arg1,
                          const char *psz_arg2, const char *psz_filename )
{
    xmlCatalogAdd( psz_arg1, psz_arg2, psz_filename );
}

/*****************************************************************************
 * Reader functions
 *****************************************************************************/
static void ReaderErrorHandler( void *p_arg, const char *p_msg,
                                xmlParserSeverities severity,
                                xmlTextReaderLocatorPtr locator)
{
    xml_reader_t *p_reader = (xml_reader_t *)p_arg;
    int line = xmlTextReaderLocatorLineNumber( locator );
    msg_Err( p_reader->p_xml, "XML parser error (line %d) : %s", line, p_msg );
}

static xml_reader_t *ReaderCreate( xml_t *p_xml, stream_t *p_stream )
{
    xml_reader_t *p_reader;
    xml_reader_sys_t *p_sys;
    xmlTextReaderPtr p_libxml_reader;
    xmlParserInputBufferPtr p_read_context;

    p_read_context = malloc( sizeof( xmlParserInputBuffer ) );

    p_libxml_reader = xmlReaderForIO( StreamRead, NULL, p_stream,
                                      NULL, NULL, 0 );
    if( !p_libxml_reader )
    {
        msg_Err( p_xml, "failed to create xml parser" );
        return 0;
    }

    p_reader = malloc( sizeof(xml_reader_t) );
    p_reader->p_sys = p_sys = malloc( sizeof(xml_reader_sys_t) );
    p_reader->p_sys->p_reader = p_libxml_reader;
    p_reader->p_xml = p_xml;

    /* Set the error handler */
    xmlTextReaderSetErrorHandler( p_libxml_reader,
                                  ReaderErrorHandler, p_reader );


    p_reader->pf_read = ReaderRead;
    p_reader->pf_node_type = ReaderNodeType;
    p_reader->pf_name = ReaderName;
    p_reader->pf_value = ReaderValue;
    p_reader->pf_next_attr = ReaderNextAttr;
    p_reader->pf_use_dtd = ReaderUseDTD;

    return p_reader;
}

static void ReaderDelete( xml_reader_t *p_reader )
{
    xmlFreeTextReader( p_reader->p_sys->p_reader );
    free( p_reader->p_sys );
    free( p_reader );
}

static int ReaderUseDTD ( xml_reader_t *p_reader, vlc_bool_t b_use )
{
    /* Activate DTD validation */
    xmlTextReaderSetParserProp( p_reader->p_sys->p_reader,
                                XML_PARSER_DEFAULTATTRS, b_use );
    xmlTextReaderSetParserProp( p_reader->p_sys->p_reader,
                                XML_PARSER_VALIDATE, b_use );

    return VLC_SUCCESS;
}

static int ReaderRead( xml_reader_t *p_reader )
{
    int i_ret = xmlTextReaderRead( p_reader->p_sys->p_reader );

#if 0
    switch( i_ret )
    {
    default:
    }
#endif

    return i_ret;
}

static int ReaderNodeType( xml_reader_t *p_reader )
{
    int i_ret = xmlTextReaderNodeType( p_reader->p_sys->p_reader );

    switch( i_ret )
    {
    case XML_READER_TYPE_ELEMENT:
        i_ret = XML_READER_STARTELEM;
        break;
    case XML_READER_TYPE_END_ELEMENT:
        i_ret = XML_READER_ENDELEM;
        break;
    case XML_READER_TYPE_CDATA:
    case XML_READER_TYPE_TEXT:
        i_ret = XML_READER_TEXT;
        break;
    case -1:
        i_ret = -1;
        break;
    default:
        i_ret = XML_READER_NONE;
        break;
    }

    return i_ret;
}

static char *ReaderName( xml_reader_t *p_reader )
{
    const xmlChar *psz_name =
        xmlTextReaderConstName( p_reader->p_sys->p_reader );

    if( psz_name ) return strdup( psz_name );
    else return 0;
}

static char *ReaderValue( xml_reader_t *p_reader )
{
    const xmlChar *psz_value =
        xmlTextReaderConstValue( p_reader->p_sys->p_reader );

    if( psz_value ) return strdup( psz_value );
    else return 0;
}

static int ReaderNextAttr( xml_reader_t *p_reader )
{
    return ( xmlTextReaderMoveToNextAttribute( p_reader->p_sys->p_reader )
             == 1 ) ? VLC_SUCCESS : VLC_EGENERIC;
}

static int StreamRead( void *p_context, char *p_buffer, int i_buffer )
{
    stream_t *s = (stream_t*)p_context;
    return stream_Read( s, p_buffer, i_buffer );    
}
