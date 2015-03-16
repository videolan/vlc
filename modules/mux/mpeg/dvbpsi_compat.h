/*****************************************************************************
 * dvbpsi_compat.h: Compatibility headerfile
 *****************************************************************************
 * Copyright (C) 2013 VideoLAN Association
 *
 * Authors: Jean-Paul Saman <jpsaman@videolan.org>
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

#ifndef DVBPSI_COMPAT_H
#define DVBPSI_COMPAT_H

static inline void dvbpsi_messages(dvbpsi_t *p_dvbpsi, const dvbpsi_msg_level_t level, const char* msg)
{
    vlc_object_t *obj = (vlc_object_t *)p_dvbpsi->p_sys;

    /* See dvbpsi.h for the definition of these log levels.*/
    switch(level)
    {
        case DVBPSI_MSG_ERROR:
        {
#if DVBPSI_VERSION_INT <= ((1 << 16) + (2 << 8))
            if( strncmp( msg, "libdvbpsi (PMT decoder): ", 25 ) ||
                ( strncmp( &msg[25], "invalid section", 15 ) &&
                  strncmp( &msg[25], "'program_number' don't match", 28 ) ) )
#endif
            msg_Err( obj, "%s", msg ); break;
        }
        case DVBPSI_MSG_WARN:  msg_Warn( obj, "%s", msg ); break;
        case DVBPSI_MSG_NONE:
        case DVBPSI_MSG_DEBUG:
#ifdef DVBPSI_DEBUG
            msg_Dbg( obj, "%s", msg );
#endif
            break;
    }
}

#endif
