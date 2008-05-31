/*****************************************************************************
 * parser.c :  OSD import module
 *****************************************************************************
 * Copyright (C) 2007 M2X
 * $Id$
 *
 * Authors: Jean-Paul Saman
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>
#include <vlc_config.h>

#include <vlc_keys.h>
#include <vlc_image.h>
#include <vlc_osd.h>
#include <vlc_charset.h>

#include "osd_menu.h"

/***************************************************************************
 * Prototypes
 ***************************************************************************/
int osd_parser_simpleOpen ( vlc_object_t *p_this );
int osd_parser_xmlOpen ( vlc_object_t *p_this );

static void osd_parser_Close( vlc_object_t *p_this );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();

    set_category( CAT_OSD );
    set_subcategory( SUBCAT_OSD_IMPORT );

    add_submodule();
        set_description( N_("OSD configuration importer") );
        add_shortcut( "import-osd" );
        set_capability( "osd parser", 0);
        set_callbacks( osd_parser_simpleOpen, osd_parser_Close );

    add_submodule();
        set_description( N_("XML OSD configuration importer") );
        add_shortcut( "import-osd-xml" );
        set_capability( "osd parser", 0);
        set_callbacks( osd_parser_xmlOpen, osd_parser_Close );

vlc_module_end();

/*****************************************************************************
 * osd_parser_Close: Free all osd menu structure resources
 *****************************************************************************/

void osd_parser_Close ( vlc_object_t *p_this )
{
    osd_menu_t *p_menu = (osd_menu_t *) p_this;
    if( p_menu )
        osd_MenuFree( p_menu );
}
