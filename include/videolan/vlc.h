/*****************************************************************************
 * vlc.h: global header for vlc
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: vlc.h,v 1.4 2002/02/24 20:51:09 gbazin Exp $
 *
 * Authors: Samuel Hocevar <sam@via.ecp.fr>
 *          Vincent Seguin <seguin@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
 * Required vlc headers
 *****************************************************************************/
#include "defs.h"
#include "config.h"
#include "int_types.h"

#if defined( PLUGIN ) || defined( BUILTIN )
#   include "modules_inner.h"
#endif

#include "common.h"

#ifdef SYS_BEOS
#   include "beos_specific.h"
#endif
#ifdef SYS_DARWIN
#   include "darwin_specific.h"
#endif
#ifdef WIN32
#   include "win32_specific.h"
#endif

#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"
#include "modules.h"

#include "main.h"
#include "configuration.h"
