/*****************************************************************************
 * xml.c: XML parser wrapper for XML modules
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "vlc_xml.h"
#include "../libvlc.h"

/*****************************************************************************
 * xml_Create:
 *****************************************************************************
 * Create an instance of an XML parser.
 * Returns NULL on error.
 *****************************************************************************/
xml_t *__xml_Create( vlc_object_t *p_this )
{
    xml_t *p_xml;

    p_xml = vlc_custom_create( p_this, sizeof( *p_xml ), VLC_OBJECT_GENERIC,
                               "xml" );
    vlc_object_attach( p_xml, p_this );

    p_xml->p_module = module_Need( p_xml, "xml", 0, 0 );
    if( !p_xml->p_module )
    {
        vlc_object_detach( p_xml );
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
    module_Unneed( p_xml, p_xml->p_module );
    vlc_object_detach( p_xml );
    vlc_object_release( p_xml );
}
