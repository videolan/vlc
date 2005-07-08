/*****************************************************************************
 * info.h : VCD information routine headers
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN (Centrale Réseaux) and its contributors
 * $Id: info.h 8606 2004-08-31 18:32:54Z rocky $
 *
 * Authors: Rocky Bernstein <rocky@panix.com>
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

#ifndef VCD_INFO_H
#define VCD_INFO_H

#include "vcdplayer.h"

/*
 Sets VCD meta information and navigation/playlist entries. 
 */
void VCDMetaInfo( access_t *p_access, /*const*/ char *psz_mrl );


char * VCDFormatStr(const access_t *p_access, vcdplayer_t *p_vcdplayer,
		    const char format_str[], const char *mrl,
		    const vcdinfo_itemid_t *itemid);

void VCDUpdateTitle( access_t *p_access );

#endif /* VCD_INFO_H */
