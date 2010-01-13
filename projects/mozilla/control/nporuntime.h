/*****************************************************************************
 * runtime.cpp: support for NPRuntime API for Netscape Script-able plugins
 *              FYI: http://www.mozilla.org/projects/plugins/npruntime.html
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id$
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

#ifndef __NPORUNTIME_H__
#define __NPORUNTIME_H__

/*
** support framework for runtime script objects
*/

#include <npapi.h>
#include <npruntime.h>

static void RuntimeNPClassDeallocate(NPObject *npobj);
static void RuntimeNPClassInvalidate(NPObject *npobj);
static bool RuntimeNPClassInvokeDefault(NPObject *npobj,
                                        const NPVariant *args,
                                        uint32_t argCount,
                                        NPVariant *result);

class RuntimeNPObject : public NPObject
{
public:
    // Lazy child object cration helper. Doing this avoids
    // ownership problems with firefox.
    template<class T> void InstantObj( NPObject *&obj );

    /*
    ** utility functions
    */

    static bool isNumberValue(const NPVariant &v)
    {
        return NPVARIANT_IS_INT32(v)
            || NPVARIANT_IS_DOUBLE(v);
    };

    static int numberValue(const NPVariant &v)
    {
        switch( v.type ) {
            case NPVariantType_Int32:
                return NPVARIANT_TO_INT32(v);
            case NPVariantType_Double:
                return(int)NPVARIANT_TO_DOUBLE(v);
            default:
                return 0;
        }
    };

    static char* stringValue(const NPString &v);
    static char* stringValue(const NPVariant &v);

protected:
    void *operator new(size_t n)
    {
        /*
        ** Assume that browser has a smarter memory allocator
        ** than plain old malloc() and use it instead.
        */
        return NPN_MemAlloc(n);
    };

    void operator delete(void *p)
    {
        NPN_MemFree(p);
    };

    bool isValid()
    {
        return _instance != NULL;
    };

    RuntimeNPObject(NPP instance, const NPClass *aClass) :
        _instance(instance)
    {
        _class = const_cast<NPClass *>(aClass);
        referenceCount = 1;
    };
    virtual ~RuntimeNPObject() {};

    enum InvokeResult
    {
        INVOKERESULT_NO_ERROR       = 0,    /* returns no error */
        INVOKERESULT_GENERIC_ERROR  = 1,    /* returns error */
        INVOKERESULT_NO_SUCH_METHOD = 2,    /* throws method does not exist */
        INVOKERESULT_INVALID_ARGS   = 3,    /* throws invalid arguments */
        INVOKERESULT_INVALID_VALUE  = 4,    /* throws invalid value in assignment */
        INVOKERESULT_OUT_OF_MEMORY  = 5,    /* throws out of memory */
    };

    friend void RuntimeNPClassDeallocate(NPObject *npobj);
    friend void RuntimeNPClassInvalidate(NPObject *npobj);
    template <class RuntimeNPObject> friend bool RuntimeNPClassGetProperty(NPObject *npobj, NPIdentifier name, NPVariant *result);
    template <class RuntimeNPObject> friend bool RuntimeNPClassSetProperty(NPObject *npobj, NPIdentifier name, const NPVariant *value);
    template <class RuntimeNPObject> friend bool RuntimeNPClassRemoveProperty(NPObject *npobj, NPIdentifier name);
    template <class RuntimeNPObject> friend bool RuntimeNPClassInvoke(NPObject *npobj, NPIdentifier name,
                                                    const NPVariant *args, uint32_t argCount,
                                                    NPVariant *result);
    friend bool RuntimeNPClassInvokeDefault(NPObject *npobj,
                                            const NPVariant *args,
                                            uint32_t argCount,
                                            NPVariant *result);

    virtual InvokeResult getProperty(int index, NPVariant &result);
    virtual InvokeResult setProperty(int index, const NPVariant &value);
    virtual InvokeResult removeProperty(int index);
    virtual InvokeResult invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant &result);
    virtual InvokeResult invokeDefault(const NPVariant *args, uint32_t argCount, NPVariant &result);

    bool returnInvokeResult(InvokeResult result);

    static InvokeResult invokeResultString(const char *,NPVariant &);

    bool isPluginRunning()
    {
        return (_instance->pdata != NULL);
    }
    template<class T> T *getPrivate()
    {
        return reinterpret_cast<T *>(_instance->pdata);
    }

    NPP _instance;
};

template<class T> class RuntimeNPClass : public NPClass
{
public:
    static NPClass *getClass()
    {
        static NPClass *singleton = new RuntimeNPClass<T>;
        return singleton;
    }

protected:
    RuntimeNPClass();
    virtual ~RuntimeNPClass();

    template <class RuntimeNPObject> friend NPObject *RuntimeNPClassAllocate(NPP instance, NPClass *aClass);
    template <class RuntimeNPObject> friend bool RuntimeNPClassHasMethod(NPObject *npobj, NPIdentifier name);
    template <class RuntimeNPObject> friend bool RuntimeNPClassHasProperty(NPObject *npobj, NPIdentifier name);
    template <class RuntimeNPObject> friend bool RuntimeNPClassGetProperty(NPObject *npobj, NPIdentifier name, NPVariant *result);
    template <class RuntimeNPObject> friend bool RuntimeNPClassSetProperty(NPObject *npobj, NPIdentifier name, const NPVariant *value);
    template <class RuntimeNPObject> friend bool RuntimeNPClassRemoveProperty(NPObject *npobj, NPIdentifier name);
    template <class RuntimeNPObject> friend bool RuntimeNPClassInvoke(NPObject *npobj, NPIdentifier name,
                                                                      const NPVariant *args, uint32_t argCount,
                                                                      NPVariant *result);

    RuntimeNPObject *create(NPP instance) const;

    int indexOfMethod(NPIdentifier name) const;
    int indexOfProperty(NPIdentifier name) const;

private:
    NPIdentifier *propertyIdentifiers;
    NPIdentifier *methodIdentifiers;
};

template<class T>
inline void RuntimeNPObject::InstantObj( NPObject *&obj )
{
    if( !obj )
        obj = NPN_CreateObject(_instance, RuntimeNPClass<T>::getClass());
}

template<class T>
static NPObject *RuntimeNPClassAllocate(NPP instance, NPClass *aClass)
{
    const RuntimeNPClass<T> *vClass = static_cast<RuntimeNPClass<T> *>(aClass);
    return vClass->create(instance);
}

static void RuntimeNPClassDeallocate(NPObject *npobj)
{
    RuntimeNPObject *vObj = static_cast<RuntimeNPObject *>(npobj);
    vObj->_class = NULL;
    delete vObj;
}

static void RuntimeNPClassInvalidate(NPObject *npobj)
{
    RuntimeNPObject *vObj = static_cast<RuntimeNPObject *>(npobj);
    vObj->_instance = NULL;
}

template<class T>
static bool RuntimeNPClassHasMethod(NPObject *npobj, NPIdentifier name)
{
    const RuntimeNPClass<T> *vClass = static_cast<RuntimeNPClass<T> *>(npobj->_class);
    return vClass->indexOfMethod(name) != -1;
}

template<class T>
static bool RuntimeNPClassHasProperty(NPObject *npobj, NPIdentifier name)
{
    const RuntimeNPClass<T> *vClass = static_cast<RuntimeNPClass<T> *>(npobj->_class);
    return vClass->indexOfProperty(name) != -1;
}

template<class T>
static bool RuntimeNPClassGetProperty(NPObject *npobj, NPIdentifier name, NPVariant *result)
{
    RuntimeNPObject *vObj = static_cast<RuntimeNPObject *>(npobj);
    if( vObj->isValid() )
    {
        const RuntimeNPClass<T> *vClass = static_cast<RuntimeNPClass<T> *>(npobj->_class);
        int index = vClass->indexOfProperty(name);
        if( index != -1 )
        {
            return vObj->returnInvokeResult(vObj->getProperty(index, *result));
        }
    }
    return false;
}

template<class T>
static bool RuntimeNPClassSetProperty(NPObject *npobj, NPIdentifier name, const NPVariant *value)
{
    RuntimeNPObject *vObj = static_cast<RuntimeNPObject *>(npobj);
    if( vObj->isValid() )
    {
        const RuntimeNPClass<T> *vClass = static_cast<RuntimeNPClass<T> *>(npobj->_class);
        int index = vClass->indexOfProperty(name);
        if( index != -1 )
        {
            return vObj->returnInvokeResult(vObj->setProperty(index, *value));
        }
    }
    return false;
}

template<class T>
static bool RuntimeNPClassRemoveProperty(NPObject *npobj, NPIdentifier name)
{
    RuntimeNPObject *vObj = static_cast<RuntimeNPObject *>(npobj);
    if( vObj->isValid() )
    {
        const RuntimeNPClass<T> *vClass = static_cast<RuntimeNPClass<T> *>(npobj->_class);
        int index = vClass->indexOfProperty(name);
        if( index != -1 )
        {
            return vObj->returnInvokeResult(vObj->removeProperty(index));
        }
    }
    return false;
}

template<class T>
static bool RuntimeNPClassInvoke(NPObject *npobj, NPIdentifier name,
                                    const NPVariant *args, uint32_t argCount,
                                    NPVariant *result)
{
    RuntimeNPObject *vObj = static_cast<RuntimeNPObject *>(npobj);
    if( vObj->isValid() )
    {
        const RuntimeNPClass<T> *vClass = static_cast<RuntimeNPClass<T> *>(npobj->_class);
        int index = vClass->indexOfMethod(name);
        if( index != -1 )
        {
            return vObj->returnInvokeResult(vObj->invoke(index, args, argCount, *result));

        }
    }
    return false;
}

static bool RuntimeNPClassInvokeDefault(NPObject *npobj,
                                           const NPVariant *args,
                                           uint32_t argCount,
                                           NPVariant *result)
{
    RuntimeNPObject *vObj = static_cast<RuntimeNPObject *>(npobj);
    if( vObj->isValid() )
    {
        return vObj->returnInvokeResult(vObj->invokeDefault(args, argCount, *result));
    }
    return false;
}

template<class T>
RuntimeNPClass<T>::RuntimeNPClass()
{
    // retreive property identifiers from names
    if( T::propertyCount > 0 )
    {
        propertyIdentifiers = new NPIdentifier[T::propertyCount];
        if( propertyIdentifiers )
            NPN_GetStringIdentifiers(const_cast<const NPUTF8**>(T::propertyNames),
                T::propertyCount, propertyIdentifiers);
    }

    // retreive method identifiers from names
    if( T::methodCount > 0 )
    {
        methodIdentifiers = new NPIdentifier[T::methodCount];
        if( methodIdentifiers )
            NPN_GetStringIdentifiers(const_cast<const NPUTF8**>(T::methodNames),
                T::methodCount, methodIdentifiers);
    }

    // fill in NPClass structure
    structVersion  = NP_CLASS_STRUCT_VERSION;
    allocate       = &RuntimeNPClassAllocate<T>;
    deallocate     = &RuntimeNPClassDeallocate;
    invalidate     = &RuntimeNPClassInvalidate;
    hasMethod      = &RuntimeNPClassHasMethod<T>;
    invoke         = &RuntimeNPClassInvoke<T>;
    invokeDefault  = &RuntimeNPClassInvokeDefault;
    hasProperty    = &RuntimeNPClassHasProperty<T>;
    getProperty    = &RuntimeNPClassGetProperty<T>;
    setProperty    = &RuntimeNPClassSetProperty<T>;
    removeProperty = &RuntimeNPClassRemoveProperty<T>;
}

template<class T>
RuntimeNPClass<T>::~RuntimeNPClass()
{
    delete[] propertyIdentifiers;
    delete[] methodIdentifiers;
}

template<class T>
RuntimeNPObject *RuntimeNPClass<T>::create(NPP instance) const
{
    return new T(instance, this);
}

template<class T>
int RuntimeNPClass<T>::indexOfMethod(NPIdentifier name) const
{
    if( methodIdentifiers )
    {
        for(int c=0; c< T::methodCount; ++c )
        {
            if( name == methodIdentifiers[c] )
                return c;
        }
    }
    return -1;
}

template<class T>
int RuntimeNPClass<T>::indexOfProperty(NPIdentifier name) const
{
    if( propertyIdentifiers )
    {
        for(int c=0; c< T::propertyCount; ++c )
        {
            if( name == propertyIdentifiers[c] )
                return c;
        }
    }
    return -1;
}

#endif
