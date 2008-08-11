/*****************************************************************************
 * xml.h: XML abstraction layer
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Author: Gildas Bazin <gbazin@videolan.org>
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

#ifndef VLC_XML_H
#define VLC_XML_H

/**
 * \file
 * This file defines functions and structures to handle xml tags in vlc
 *
 */

# ifdef __cplusplus
extern "C" {
# endif

struct xml_t
{
    VLC_COMMON_MEMBERS

    /* Module properties */
    module_t  *p_module;
    xml_sys_t *p_sys;

    xml_reader_t * (*pf_reader_create) ( xml_t *, stream_t * );
    void (*pf_reader_delete) ( xml_reader_t * );

    void (*pf_catalog_load) ( xml_t *, const char * );
    void (*pf_catalog_add) ( xml_t *, const char *, const char *,
                               const char * );
};

#define xml_Create( a ) __xml_Create( VLC_OBJECT(a) )
VLC_EXPORT( xml_t *, __xml_Create, ( vlc_object_t * ) );
VLC_EXPORT( void, xml_Delete, ( xml_t * ) );

#define xml_ReaderCreate( a, b ) a->pf_reader_create( a, b )
#define xml_ReaderDelete( a, b ) a->pf_reader_delete( b )
#define xml_CatalogLoad( a, b ) a->pf_catalog_load( a, b )
#define xml_CatalogAdd( a, b, c, d ) a->pf_catalog_add( a, b, c, d )

struct xml_reader_t
{
    xml_t *p_xml;
    xml_reader_sys_t *p_sys;

    int (*pf_read) ( xml_reader_t * );
    int (*pf_node_type) ( xml_reader_t * );
    char * (*pf_name) ( xml_reader_t * );
    char * (*pf_value) ( xml_reader_t * );
    int (*pf_next_attr) ( xml_reader_t * );

    int (*pf_use_dtd) ( xml_reader_t *, bool );
};

#define xml_ReaderRead( a ) a->pf_read( a )
#define xml_ReaderNodeType( a ) a->pf_node_type( a )
#define xml_ReaderName( a ) a->pf_name( a )
#define xml_ReaderValue( a ) a->pf_value( a )
#define xml_ReaderNextAttr( a ) a->pf_next_attr( a )
#define xml_ReaderUseDTD( a, b ) a->pf_use_dtd( a, b )

#define XML_READER_NONE 0
#define XML_READER_STARTELEM 1
#define XML_READER_ENDELEM 2
#define XML_READER_TEXT 3

# ifdef __cplusplus
}
# endif

#endif
