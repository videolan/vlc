/*****************************************************************************
 * dlna_common.hpp : DLNA common header
 *****************************************************************************
 * Copyright © 2018 VLC authors and VideoLAN
 *
 * Authors: Shaleen Jain <shaleen@jain.sh>
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

#ifndef DLNA_COMMON_H
#define DLNA_COMMON_H

#include <list>

#include <vlc_common.h>
#include <vlc_fourcc.h>

#include "../renderer_common.hpp"

#define SOUT_CFG_PREFIX "sout-dlna-"

namespace DLNA
{

/* module callbacks */
int OpenSout(vlc_object_t *);
void CloseSout(vlc_object_t *);

}
#endif /* DLNA_COMMON_H */
