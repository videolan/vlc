/*****************************************************************************
 * vlc_xml.h: XML abstraction layer
 *****************************************************************************
 * Copyright (C) 2004-2010 VLC authors and VideoLAN
 *
 * Author: Gildas Bazin <gbazin@videolan.org>
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

    void (*pf_catalog_load) ( xml_t *, const char * );
    void (*pf_catalog_add) ( xml_t *, const char *, const char *,
                            const char * );
};

VLC_API xml_t * xml_Create( vlc_object_t * ) VLC_USED;
#define xml_Create( a ) xml_Create( VLC_OBJECT(a) )
VLC_API void xml_Delete( xml_t * );

static inline void xml_CatalogLoad( xml_t *xml, const char *catalog )
{
    xml->pf_catalog_load( xml, catalog );
}

static inline void xml_CatalogAdd( xml_t *xml, const char *type,
                                   const char *orig, const char *value )
{
    xml->pf_catalog_add( xml, type, orig, value );
}


struct xml_reader_t
{
    VLC_COMMON_MEMBERS

    xml_reader_sys_t *p_sys;
    stream_t *p_stream;
    module_t *p_module;

    int (*pf_next_node) ( xml_reader_t *, const char ** );
    const char *(*pf_next_attr) ( xml_reader_t *, const char ** );

    int (*pf_use_dtd) ( xml_reader_t * );
    int (*pf_is_empty) ( xml_reader_t * );
};

VLC_API xml_reader_t * xml_ReaderCreate(vlc_object_t *, stream_t *) VLC_USED;
#define xml_ReaderCreate( a, s ) xml_ReaderCreate(VLC_OBJECT(a), s)
VLC_API void xml_ReaderDelete(xml_reader_t *);
VLC_API xml_reader_t * xml_ReaderReset(xml_reader_t *, stream_t *) VLC_USED;

static inline int xml_ReaderNextNode( xml_reader_t *reader, const char **pval )
{
    return reader->pf_next_node( reader, pval );
}

static inline const char *xml_ReaderNextAttr( xml_reader_t *reader,
                                              const char **pval )
{
  return reader->pf_next_attr( reader, pval );
}

static inline int xml_ReaderUseDTD( xml_reader_t *reader )
{
  return reader->pf_use_dtd( reader );
}

static inline int xml_ReaderIsEmptyElement( xml_reader_t *reader )
{
    if(reader->pf_is_empty == NULL)
        return -2;

    return reader->pf_is_empty( reader );
}

enum {
    XML_READER_NONE=0,
    XML_READER_STARTELEM,
    XML_READER_ENDELEM,
    XML_READER_TEXT,
};

# ifdef __cplusplus
}
# endif

#endif
