/*****************************************************************************
 * xmlreading.h : Videolan.org's Addons xml readers helper
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
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

#define BINDNODE(name,target,type)\
    if ( ! strcmp(p_node, name) )\
    {\
        if ( type == TYPE_STRING ) \
            data_pointer.u_data.ppsz = (char**)&target;\
        else if ( type == TYPE_LONG ) \
            data_pointer.u_data.pl = (long*)&target;\
        else \
            data_pointer.u_data.pi = (int*)&target;\
        data_pointer.e_type = type;\
    } else

struct
{
    union
    {
        char ** ppsz;
        int * pi;
        long * pl;
    } u_data;
    enum
    {
        TYPE_NONE, TYPE_STRING, TYPE_INTEGER, TYPE_LONG
    } e_type;
} data_pointer = { {NULL}, TYPE_NONE };


static inline int ReadType( const char *value )
{
    if ( !strcmp( value, "playlist" ) )
        return ADDON_PLAYLIST_PARSER;
    else if ( !strcmp( value, "skin" ) )
        return ADDON_SKIN2;
    else if ( !strcmp( value, "discovery" ) )
        return ADDON_SERVICE_DISCOVERY;
    else if ( !strcmp( value, "extension" ) )
        return ADDON_EXTENSION;
    else if ( !strcmp( value, "interface" ) )
        return ADDON_INTERFACE;
    else if ( !strcmp( value, "meta" ) )
        return ADDON_META;
    else
        return ADDON_UNKNOWN;
}

static inline const char * getTypePsz( int i_type )
{
    switch( i_type )
    {
    case ADDON_PLAYLIST_PARSER:
        return "playlist";
    case ADDON_SKIN2:
        return "skin";
    case ADDON_SERVICE_DISCOVERY:
        return "discovery";
    case ADDON_EXTENSION:
        return "extension";
    case ADDON_INTERFACE:
        return "interface";
    case ADDON_META:
        return "meta";
    case ADDON_UNKNOWN:
    default:
        return "unknown";
    }
}
