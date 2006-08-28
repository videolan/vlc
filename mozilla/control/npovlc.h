/*****************************************************************************
 * vlc.h: a VLC plugin for Mozilla
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id: vlcruntime.h 14466 2006-02-22 23:34:54Z dionoea $
 *
 * Authors: Damien Fouilleul <damien.fouilleul@laposte.net>
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

/*
** defined runtime script objects
*/

#include "nporuntime.h"

class VlcNPObject: public RuntimeNPObject
{
public:
    VlcNPObject(NPP instance, const NPClass *aClass) :
        RuntimeNPObject(instance, aClass) {};
    virtual ~VlcNPObject() {};

protected:
    friend class RuntimeNPClass<VlcNPObject>;

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    static const int methodCount;
    static const NPUTF8 * const methodNames[];

    virtual InvokeResult invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result);
};

