/*****************************************************************************
 * xml.c - The OSD Menu XML parser code.
 *****************************************************************************
 * Copyright (C) 2005-2007 M2X
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
#include <vlc_vout.h>
#include <vlc_config.h>

#include <vlc_keys.h>
#include <vlc_image.h>
#include <vlc_osd.h>
#include <vlc_charset.h>

#include "osd_menu.h"

int osd_parser_xmlOpen ( vlc_object_t *p_this );

/****************************************************************************
 * Local structures
 ****************************************************************************/

int osd_parser_xmlOpen( vlc_object_t *p_this )
{
    VLC_UNUSED(p_this);
    return VLC_SUCCESS;
}

