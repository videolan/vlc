/*****************************************************************************
 * registry.hpp: Windows Registry Manipulation
 ****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * $Id$
 *
 * Authors: Andre Weber <WeberAndre # gmx - de>
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
#ifndef QVLC_REGISTRY_H
#define QVLC_REGISTRY_H

#include <windows.h>

class QVLCRegistry
{
private:
    HKEY m_RootKey;
    char m_pathBuffer[256];
public:
    QVLCRegistry(HKEY rootKey);
    ~QVLCRegistry(void);

    void WriteRegistryInt( const char *path, const char *valueName, int value);
    void WriteRegistryString( const char *path, const char *valueName, const char *value);
    void WriteRegistryDouble( const char *path, const char *valueName, double value);

    int ReadRegistryInt( const char *path, const char *valueName, int default_value);
    char * ReadRegistryString( const char *path, const char *valueName, const char *default_value);
    double ReadRegistryDouble( const char *path, const char *valueName, double default_value);

    bool RegistryKeyExists( const char *path);
    bool RegistryValueExists( const char *path, const char *valueName);
    int DeleteValue( const char *path, const char *valueName );
    long DeleteKey( const char *path, const char *keyName );
};

#endif
