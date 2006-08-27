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

class LibvlcRootNPObject: public RuntimeNPObject
{
public:
    LibvlcRootNPObject(NPP instance, const NPClass *aClass);
    virtual ~LibvlcRootNPObject();

protected:
    friend class RuntimeNPClass<LibvlcRootNPObject>;

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    InvokeResult getProperty(int index, NPVariant *result);

    static const int methodCount;
    static const NPUTF8 * const methodNames[];

    NPObject *audioObj;
    NPObject *inputObj;
    NPObject *playlistObj;
    NPObject *videoObj;
};

class LibvlcAudioNPObject: public RuntimeNPObject
{
public:
    LibvlcAudioNPObject(NPP instance, const NPClass *aClass) :
        RuntimeNPObject(instance, aClass) {};
    virtual ~LibvlcAudioNPObject() {};

protected:
    friend class RuntimeNPClass<LibvlcAudioNPObject>;

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    InvokeResult getProperty(int index, NPVariant *result);
    InvokeResult setProperty(int index, const NPVariant *value);

    static const int methodCount;
    static const NPUTF8 * const methodNames[];

    InvokeResult invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant *result);
};

class LibvlcInputNPObject: public RuntimeNPObject
{
public:
    LibvlcInputNPObject(NPP instance, const NPClass *aClass) :
        RuntimeNPObject(instance, aClass) {};
	
    virtual ~LibvlcInputNPObject() {};

protected:
    friend class RuntimeNPClass<LibvlcInputNPObject>;

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    InvokeResult getProperty(int index, NPVariant *result);
    InvokeResult setProperty(int index, const NPVariant *value);

    static const int methodCount;
    static const NPUTF8 * const methodNames[];
};

class LibvlcPlaylistNPObject: public RuntimeNPObject
{
public:
    LibvlcPlaylistNPObject(NPP instance, const NPClass *aClass) :
        RuntimeNPObject(instance, aClass) {};
    virtual ~LibvlcPlaylistNPObject() {};

protected:
    friend class RuntimeNPClass<LibvlcPlaylistNPObject>;

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    InvokeResult getProperty(int index, NPVariant *result);

    static const int methodCount;
    static const NPUTF8 * const methodNames[];

    InvokeResult invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant *result);
};

class LibvlcVideoNPObject: public RuntimeNPObject
{
public:
    LibvlcVideoNPObject(NPP instance, const NPClass *aClass) :
        RuntimeNPObject(instance, aClass) {};
    virtual ~LibvlcVideoNPObject() {};

protected:
    friend class RuntimeNPClass<LibvlcVideoNPObject>;

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    InvokeResult getProperty(int index, NPVariant *result);
    InvokeResult setProperty(int index, const NPVariant *value);

    static const int methodCount;
    static const NPUTF8 * const methodNames[];

    InvokeResult invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant *result);

};

