/*****************************************************************************
 * dummy.c : dummy plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: dummy.c,v 1.2 2002/08/07 21:36:56 massiot Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>

#include "dummy.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CHROMA_TEXT N_("dummy image chroma format")
#define CHROMA_LONGTEXT N_( \
    "Force the dummy video output to create images using a specific chroma " \
    "format instead of trying to improve performances by using the most " \
    "efficient one.")

vlc_module_begin();
    set_description( _("dummy functions module") );
    add_shortcut( "vlc" );
    add_submodule();
        set_capability( "interface", 0 );
        set_callbacks( E_(OpenIntf), NULL );
    add_submodule();
        set_capability( "access", 0 );
        set_callbacks( E_(OpenAccess), NULL );
    add_submodule();
        set_capability( "demux", 0 );
        set_callbacks( E_(OpenDemux), E_(CloseDemux) );
    add_submodule();
        set_capability( "decoder", 0 );
        set_callbacks( E_(OpenDecoder), NULL );
    add_submodule();
        set_capability( "audio output", 1 );
        set_callbacks( E_(OpenAudio), NULL );
    add_submodule();
        set_capability( "video output", 1 );
        set_callbacks( E_(OpenVideo), NULL );
        add_category_hint( N_("Video"), NULL );
        add_string( "dummy-chroma", NULL, NULL, CHROMA_TEXT, CHROMA_LONGTEXT );
vlc_module_end();

