/*****************************************************************************
 * npolibvlc.h: official Javascript APIs
 *****************************************************************************
 * Copyright (C) 2002-2006 the VideoLAN team
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
#include <vlc/vlc.h>

#include "nporuntime.h"

class LibvlcRootNPObject: public RuntimeNPObject
{
protected:
    friend class RuntimeNPClass<LibvlcRootNPObject>;

    LibvlcRootNPObject(NPP instance, const NPClass *aClass) :
        RuntimeNPObject(instance, aClass),
    audioObj(NULL),
    inputObj(NULL),
    playlistObj(NULL),
    subtitleObj(NULL),
    videoObj(NULL) { }

    virtual ~LibvlcRootNPObject();

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    InvokeResult getProperty(int index, NPVariant &result);

    static const int methodCount;
    static const NPUTF8 * const methodNames[];

    InvokeResult invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result);

private:
    NPObject *audioObj;
    NPObject *inputObj;
    NPObject *playlistObj;
    NPObject *subtitleObj;
    NPObject *videoObj;
};

class LibvlcAudioNPObject: public RuntimeNPObject
{
protected:
    LibvlcAudioNPObject(NPP instance, const NPClass *aClass) :
        RuntimeNPObject(instance, aClass) {};
    virtual ~LibvlcAudioNPObject() {};

    friend class RuntimeNPClass<LibvlcAudioNPObject>;

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    InvokeResult getProperty(int index, NPVariant &result);
    InvokeResult setProperty(int index, const NPVariant &value);

    static const int methodCount;
    static const NPUTF8 * const methodNames[];

    InvokeResult invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result);
};

class LibvlcInputNPObject: public RuntimeNPObject
{
protected:
    friend class RuntimeNPClass<LibvlcInputNPObject>;

    LibvlcInputNPObject(NPP instance, const NPClass *aClass) :
        RuntimeNPObject(instance, aClass) {};

    virtual ~LibvlcInputNPObject() {};

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    InvokeResult getProperty(int index, NPVariant &result);
    InvokeResult setProperty(int index, const NPVariant &value);

    static const int methodCount;
    static const NPUTF8 * const methodNames[];

    InvokeResult invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result);
};

class LibvlcPlaylistItemsNPObject: public RuntimeNPObject
{
protected:
    friend class RuntimeNPClass<LibvlcPlaylistItemsNPObject>;

    LibvlcPlaylistItemsNPObject(NPP instance, const NPClass *aClass) :
        RuntimeNPObject(instance, aClass) {};
    virtual ~LibvlcPlaylistItemsNPObject() {};

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    InvokeResult getProperty(int index, NPVariant &result);

    static const int methodCount;
    static const NPUTF8 * const methodNames[];

    InvokeResult invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result);
};

class LibvlcPlaylistNPObject: public RuntimeNPObject
{
protected:
    friend class RuntimeNPClass<LibvlcPlaylistNPObject>;

    LibvlcPlaylistNPObject(NPP instance, const NPClass *aClass) :
    RuntimeNPObject(instance, aClass),
    playlistItemsObj(NULL) {};
    
    virtual ~LibvlcPlaylistNPObject();

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    InvokeResult getProperty(int index, NPVariant &result);

    static const int methodCount;
    static const NPUTF8 * const methodNames[];

    InvokeResult invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result);

    void parseOptions(const NPString &s, int *i_options, char*** ppsz_options);
    void parseOptions(NPObject *obj, int *i_options, char*** ppsz_options);

private:
    NPObject*  playlistItemsObj;
};

class LibvlcSubtitleNPObject: public RuntimeNPObject
{
protected:
    friend class RuntimeNPClass<LibvlcSubtitleNPObject>;

    LibvlcSubtitleNPObject(NPP instance, const NPClass *aClass) :
        RuntimeNPObject(instance, aClass) {};
    virtual ~LibvlcSubtitleNPObject() {};

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    InvokeResult getProperty(int index, NPVariant &result);
    InvokeResult setProperty(int index, const NPVariant &value);

    static const int methodCount;
    static const NPUTF8 * const methodNames[];

    InvokeResult invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result);
};


class LibvlcVideoNPObject: public RuntimeNPObject
{
protected:
    friend class RuntimeNPClass<LibvlcVideoNPObject>;

    LibvlcVideoNPObject(NPP instance, const NPClass *aClass) :
        RuntimeNPObject(instance, aClass),
        marqueeObj(NULL), logoObj(NULL), deintObj(NULL) { }
    virtual ~LibvlcVideoNPObject();

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    InvokeResult getProperty(int index, NPVariant &result);
    InvokeResult setProperty(int index, const NPVariant &value);

    static const int methodCount;
    static const NPUTF8 * const methodNames[];

    InvokeResult invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result);

private:
    NPObject *marqueeObj;
    NPObject *logoObj;
    NPObject *deintObj;
};

class LibvlcMarqueeNPObject: public RuntimeNPObject
{
protected:
    friend class RuntimeNPClass<LibvlcMarqueeNPObject>;

    LibvlcMarqueeNPObject(NPP instance, const NPClass *aClass) :
        RuntimeNPObject(instance, aClass) {};
    virtual ~LibvlcMarqueeNPObject() {};

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    InvokeResult getProperty(int index, NPVariant &result);
    InvokeResult setProperty(int index, const NPVariant &value);

    static const int methodCount;
    static const NPUTF8 * const methodNames[];

    InvokeResult invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result);
};

class LibvlcLogoNPObject: public RuntimeNPObject
{
protected:
    friend class RuntimeNPClass<LibvlcLogoNPObject>;

    LibvlcLogoNPObject(NPP instance, const NPClass *aClass) :
        RuntimeNPObject(instance, aClass) { }
    virtual ~LibvlcLogoNPObject() { }

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    InvokeResult getProperty(int index, NPVariant &result);
    InvokeResult setProperty(int index, const NPVariant &value);

    static const int methodCount;
    static const NPUTF8 * const methodNames[];

    InvokeResult invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result);
};

class LibvlcDeinterlaceNPObject: public RuntimeNPObject
{
protected:
    friend class RuntimeNPClass<LibvlcDeinterlaceNPObject>;

    LibvlcDeinterlaceNPObject(NPP instance, const NPClass *aClass) :
        RuntimeNPObject(instance, aClass) { }
    virtual ~LibvlcDeinterlaceNPObject() { }

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    InvokeResult getProperty(int index, NPVariant &result);
    InvokeResult setProperty(int index, const NPVariant &value);

    static const int methodCount;
    static const NPUTF8 * const methodNames[];

    InvokeResult invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result);
};

