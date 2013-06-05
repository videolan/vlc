/*****************************************************************************
 * libxml.c: XML parser using libxml2
 *****************************************************************************
 * Copyright (C) 2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <vlc_block.h>
#include <vlc_stream.h>
#include <vlc_xml.h>

#include <libxml/xmlreader.h>
#include <libxml/catalog.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

static int ReaderOpen( vlc_object_t * );
static void ReaderClose( vlc_object_t * );


vlc_module_begin ()
    set_description( N_("XML Parser (using libxml2)") )
    set_capability( "xml", 10 )
    set_callbacks( Open, Close )

#ifdef _WIN32
    cannot_unload_broken_library()
#endif

    add_submodule()
    set_capability( "xml reader", 10 )
    set_callbacks( ReaderOpen, ReaderClose )

vlc_module_end ()

static int ReaderNextNode( xml_reader_t *, const char ** );
static const char *ReaderNextAttr( xml_reader_t *, const char ** );
static int ReaderIsEmptyElement( xml_reader_t *);

static int ReaderUseDTD ( xml_reader_t * );

static void CatalogLoad( xml_t *, const char * );
static void CatalogAdd( xml_t *, const char *, const char *, const char * );
static int StreamRead( void *p_context, char *p_buffer, int i_buffer );

static vlc_mutex_t lock = VLC_STATIC_MUTEX;

/*****************************************************************************
 * Module initialization
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    xml_t *p_xml = (xml_t *)p_this;

    if( !xmlHasFeature( XML_WITH_THREAD ) )
        return VLC_EGENERIC;

    vlc_mutex_lock( &lock );
    xmlInitParser();
    vlc_mutex_unlock( &lock );

    p_xml->pf_catalog_load = CatalogLoad;
    p_xml->pf_catalog_add  = CatalogAdd;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Module deinitialization
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
#ifdef LIBXML_GETS_A_CLUE_ABOUT_REENTRANCY_AND_MEMORY_LEAKS
    vlc_mutex_lock( &lock );
    xmlCleanupParser();
    vlc_mutex_unlock( &lock );
#endif
    VLC_UNUSED(p_this);
    return;
}

/*****************************************************************************
 * Catalogue functions
 *****************************************************************************/
static void CatalogLoad( xml_t *p_xml, const char *psz_filename )
{
    VLC_UNUSED(p_xml);
    if( !psz_filename ) xmlInitializeCatalog();
    else xmlLoadCatalog( psz_filename );
}

static void CatalogAdd( xml_t *p_xml, const char *psz_arg1,
                          const char *psz_arg2, const char *psz_filename )
{
    VLC_UNUSED(p_xml);
    xmlCatalogAdd( (unsigned char*)psz_arg1, (unsigned char*)psz_arg2,
        (unsigned char*)psz_filename );
}

/*****************************************************************************
 * Reader functions
 *****************************************************************************/
static void ReaderErrorHandler( void *p_arg, const char *p_msg,
                                xmlParserSeverities severity,
                                xmlTextReaderLocatorPtr locator)
{
    VLC_UNUSED(severity);
    xml_reader_t *p_reader = (xml_reader_t *)p_arg;
    int line = xmlTextReaderLocatorLineNumber( locator );
    msg_Err( p_reader, "XML parser error (line %d) : %s", line, p_msg );
}

struct xml_reader_sys_t
{
    xmlTextReaderPtr xml;
    char *node;
};

static int ReaderOpen( vlc_object_t *p_this )
{
    if( !xmlHasFeature( XML_WITH_THREAD ) )
        return VLC_EGENERIC;

    xml_reader_t *p_reader = (xml_reader_t *)p_this;
    xml_reader_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    xmlTextReaderPtr p_libxml_reader;

    if( unlikely(!p_sys) )
        return VLC_ENOMEM;

    vlc_mutex_lock( &lock );
    xmlInitParser();
    vlc_mutex_unlock( &lock );

    p_libxml_reader = xmlReaderForIO( StreamRead, NULL, p_reader->p_stream,
                                      NULL, NULL, 0 );
    if( !p_libxml_reader )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }

    /* Set the error handler */
    xmlTextReaderSetErrorHandler( p_libxml_reader,
                                  ReaderErrorHandler, p_reader );

    p_sys->xml = p_libxml_reader;
    p_sys->node = NULL;
    p_reader->p_sys = p_sys;
    p_reader->pf_next_node = ReaderNextNode;
    p_reader->pf_next_attr = ReaderNextAttr;
    p_reader->pf_is_empty = ReaderIsEmptyElement;
    p_reader->pf_use_dtd = ReaderUseDTD;

    return VLC_SUCCESS;
}

static void ReaderClose( vlc_object_t *p_this )
{
    xml_reader_t *p_reader = (xml_reader_t *)p_this;
    xml_reader_sys_t *p_sys = p_reader->p_sys;

    xmlFreeTextReader( p_sys->xml );
#ifdef LIBXML_GETS_A_CLUE_ABOUT_REENTRANCY_AND_MEMORY_LEAKS
    vlc_mutex_lock( &lock );
    xmlCleanupParser();
    vlc_mutex_unlock( &lock );
#endif
    free( p_sys->node );
    free( p_sys );
}

static int ReaderUseDTD ( xml_reader_t *p_reader )
{
    /* Activate DTD validation */
    xmlTextReaderSetParserProp( p_reader->p_sys->xml,
                                XML_PARSER_DEFAULTATTRS, true );
    xmlTextReaderSetParserProp( p_reader->p_sys->xml,
                                XML_PARSER_VALIDATE, true );

    return VLC_SUCCESS;
}

static int ReaderNextNode( xml_reader_t *p_reader, const char **pval )
{
    xml_reader_sys_t *p_sys = p_reader->p_sys;
    const xmlChar *node;
    int ret;

    free( p_sys->node );
    p_sys->node = NULL;

skip:
    switch( xmlTextReaderRead( p_sys->xml ) )
    {
        case 0: /* EOF */
            return 0;
        case -1: /* error */
            return -1;
    }

    switch( xmlTextReaderNodeType( p_sys->xml ) )
    {
        case XML_READER_TYPE_ELEMENT:
            node = xmlTextReaderConstName( p_sys->xml );
            ret = XML_READER_STARTELEM;
            break;

        case XML_READER_TYPE_END_ELEMENT:
            node = xmlTextReaderConstName( p_sys->xml );
            ret = XML_READER_ENDELEM;
            break;

        case XML_READER_TYPE_CDATA:
        case XML_READER_TYPE_TEXT:
            node = xmlTextReaderConstValue( p_sys->xml );
            ret = XML_READER_TEXT;
            break;

        case -1:
            return -1;

        default:
            goto skip;
    }

    if( unlikely(node == NULL) )
        return -1;

    p_sys->node = strdup( (const char *)node );
    if( pval != NULL )
        *pval = p_sys->node;
    return likely(p_sys->node != NULL) ? ret : -1;
}

#if 0
static char *ReaderValue( xml_reader_t *p_reader )
{
    const xmlChar *psz_value =
        xmlTextReaderConstValue( p_reader->p_sys->xml );

    return psz_value ? strdup( (const char *)psz_value ) : NULL;
}
#endif

static const char *ReaderNextAttr( xml_reader_t *p_reader, const char **pval )
{
    xmlTextReaderPtr xml = p_reader->p_sys->xml;
    const xmlChar *name, *value;

    if( xmlTextReaderMoveToNextAttribute( xml ) != 1
     || (name = xmlTextReaderConstName( xml )) == NULL
     || (value = xmlTextReaderConstValue( xml )) == NULL )
        return NULL;

    *pval = (const char *)value;
    return (const char *)name;
}

static int StreamRead( void *p_context, char *p_buffer, int i_buffer )
{
    stream_t *s = (stream_t*)p_context;
    return stream_Read( s, p_buffer, i_buffer );
}

static int ReaderIsEmptyElement( xml_reader_t *p_reader )
{
    return xmlTextReaderIsEmptyElement( p_reader->p_sys->xml );
}
