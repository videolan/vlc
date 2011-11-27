/*****************************************************************************
 * xml.c: XML parser wrapper for XML modules
 *****************************************************************************
 * Copyright (C) 2004-2010 VLC authors and VideoLAN
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
#include <vlc_xml.h>
#include <vlc_modules.h>
#include "../libvlc.h"

#undef xml_Create
/*****************************************************************************
 * xml_Create:
 *****************************************************************************
 * Create an instance of an XML parser.
 * Returns NULL on error.
 *****************************************************************************/
xml_t *xml_Create( vlc_object_t *p_this )
{
    xml_t *p_xml;

    p_xml = vlc_custom_create( p_this, sizeof( *p_xml ), "xml" );

    p_xml->p_module = module_need( p_xml, "xml", NULL, false );
    if( !p_xml->p_module )
    {
        vlc_object_release( p_xml );
        msg_Err( p_this, "XML provider not found" );
        return NULL;
    }

    return p_xml;
}

/*****************************************************************************
 * xml_Delete: Deletes an instance of xml_t
 *****************************************************************************/
void xml_Delete( xml_t *p_xml )
{
    module_unneed( p_xml, p_xml->p_module );
    vlc_object_release( p_xml );
}


#undef xml_ReaderCreate
/**
 * Creates an XML reader.
 * @param obj parent VLC object
 * @param stream stream to read XML from
 * @return NULL on error.
 */
xml_reader_t *xml_ReaderCreate(vlc_object_t *obj, stream_t *stream)
{
    xml_reader_t *reader;

    reader = vlc_custom_create(obj, sizeof(*reader), "xml reader");

    reader->p_stream = stream;
    reader->p_module = module_need(reader, "xml reader", NULL, false);
    if (unlikely(reader->p_module == NULL))
    {
        msg_Err(reader, "XML reader not found");
        vlc_object_release(reader);
        return NULL;
    }
    return reader;
}


/**
 * Deletes an XML reader.
 * @param reader XML reader created with xml_ReaderCreate().
 */
void xml_ReaderDelete(xml_reader_t *reader)
{
    if (reader->p_stream)
        module_stop(reader, reader->p_module);
    vlc_object_release(reader);
}


/**
 * Resets an existing XML reader.
 * If you need to parse several XML files, this function is much faster than
 * xml_ReaderCreate() and xml_ReaderDelete() combined.
 * If the stream parameter is NULL, the XML reader will be stopped, but
 * not restarted until the next xml_ReaderReset() call with a non-NULL stream.
 *
 * @param reader XML reader to reinitialize
 * @param stream new stream to read XML data from (or NULL)
 * @return reader on success,
 *         NULL on error (in that case, the reader is destroyed).
 */
xml_reader_t *xml_ReaderReset(xml_reader_t *reader, stream_t *stream)
{
    if (reader->p_stream)
        module_stop(reader, reader->p_module);

    reader->p_stream = stream;
    if ((stream != NULL) && module_start(reader, reader->p_module))
    {
        vlc_object_release(reader);
        return NULL;
    }
    return reader;
}
