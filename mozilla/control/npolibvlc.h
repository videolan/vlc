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
#include <vlc/libvlc.h>

#include "nporuntime.h"

class LibvlcRootNPObject: public RuntimeNPObject
{
protected:
    friend class RuntimeNPClass<LibvlcRootNPObject>;

    LibvlcRootNPObject(NPP instance, const NPClass *aClass);
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
    NPObject *logObj;
    NPObject *playlistObj;
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
};

class LibvlcMessageNPObject: public RuntimeNPObject
{
public:
    void setMessage(struct libvlc_log_message_t &msg)
    {
        _msg = msg;
    };

protected:
    friend class RuntimeNPClass<LibvlcMessageNPObject>;

    LibvlcMessageNPObject(NPP instance, const NPClass *aClass) :
        RuntimeNPObject(instance, aClass) {};

    virtual ~LibvlcMessageNPObject() {};

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    InvokeResult getProperty(int index, NPVariant &result);

    static const int methodCount;
    static const NPUTF8 * const methodNames[];

private:
    struct libvlc_log_message_t _msg;
};

class LibvlcLogNPObject;

class LibvlcMessageIteratorNPObject: public RuntimeNPObject
{
protected:
    friend class RuntimeNPClass<LibvlcMessageIteratorNPObject>;

    LibvlcMessageIteratorNPObject(NPP instance, const NPClass *aClass);
    virtual ~LibvlcMessageIteratorNPObject();

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    InvokeResult getProperty(int index, NPVariant &result);

    static const int methodCount;
    static const NPUTF8 * const methodNames[];

    InvokeResult invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result);

private:
    libvlc_log_iterator_t*  _p_iter;
};

class LibvlcMessagesNPObject: public RuntimeNPObject
{
protected:
    friend class RuntimeNPClass<LibvlcMessagesNPObject>;

    LibvlcMessagesNPObject(NPP instance, const NPClass *aClass) :
        RuntimeNPObject(instance, aClass) {};

    virtual ~LibvlcMessagesNPObject() {};

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    InvokeResult getProperty(int index, NPVariant &result);

    static const int methodCount;
    static const NPUTF8 * const methodNames[];

    InvokeResult invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result);
};

class LibvlcLogNPObject: public RuntimeNPObject
{
protected:
    friend class RuntimeNPClass<LibvlcLogNPObject>;

    LibvlcLogNPObject(NPP instance, const NPClass *aClass);
    virtual ~LibvlcLogNPObject();

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    InvokeResult getProperty(int index, NPVariant &result);
    InvokeResult setProperty(int index, const NPVariant &value);

    static const int methodCount;
    static const NPUTF8 * const methodNames[];

private:
    NPObject* _p_vlcmessages;
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

    LibvlcPlaylistNPObject(NPP instance, const NPClass *aClass);
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
    NPObject*  _p_vlcplaylistitems;
};

class LibvlcVideoNPObject: public RuntimeNPObject
{
protected:
    friend class RuntimeNPClass<LibvlcVideoNPObject>;

    LibvlcVideoNPObject(NPP instance, const NPClass *aClass) :
        RuntimeNPObject(instance, aClass) {};
    virtual ~LibvlcVideoNPObject() {};

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    InvokeResult getProperty(int index, NPVariant &result);
    InvokeResult setProperty(int index, const NPVariant &value);

    static const int methodCount;
    static const NPUTF8 * const methodNames[];

    InvokeResult invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result);
};
