/*****************************************************************************
 * vlcruntime.h: a VLC plugin for Mozilla
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
** support framework for runtime script objects
*/

class VlcRuntimeObject : public NPObject
{
public:
    VlcRuntimeObject(NPP instance, const NPClass *aClass) :
        _instance(instance)
    {
        _class = const_cast<NPClass *>(aClass);
        referenceCount = 1;
    };
    virtual ~VlcRuntimeObject() {};

    virtual bool getProperty(int index, NPVariant *result) = 0;
    virtual bool setProperty(int index, const NPVariant *value) = 0;
    virtual bool removeProperty(int index) = 0;
    virtual bool invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant *result) = 0;
    virtual bool invokeDefault(const NPVariant *args, uint32_t argCount, NPVariant *result) = 0;
    NPP _instance;
};

template<class T> class VlcRuntimeClass : public NPClass
{
public:
    VlcRuntimeClass();
    virtual ~VlcRuntimeClass();

    VlcRuntimeObject *create(NPP instance) const;

    int indexOfMethod(NPIdentifier name) const;
    int indexOfProperty(NPIdentifier name) const;

private:
    NPIdentifier *propertyIdentifiers;
    NPIdentifier *methodIdentifiers;
};

template<class T>
static NPObject *vlcRuntimeClassAllocate(NPP instance, NPClass *aClass)
{
    const VlcRuntimeClass<T> *vClass = static_cast<VlcRuntimeClass<T> *>(aClass);
    return (NPObject *)vClass->create(instance);
}

template<class T>
static void vlcRuntimeClassDeallocate(NPObject *npobj)
{
    VlcRuntimeObject *vObj = static_cast<VlcRuntimeObject *>(npobj);
    delete vObj;
}

template<class T>
static void vlcRuntimeClassInvalidate(NPObject *npobj)
{
    VlcRuntimeObject *vObj = static_cast<VlcRuntimeObject *>(npobj);
    vObj->_instance = NULL;
}

template<class T>
bool vlcRuntimeClassHasMethod(NPObject *npobj, NPIdentifier name)
{
    const VlcRuntimeClass<T> *vClass = static_cast<VlcRuntimeClass<T> *>(npobj->_class);
    return vClass->indexOfMethod(name) != -1;
}

template<class T>
bool vlcRuntimeClassHasProperty(NPObject *npobj, NPIdentifier name)
{
    const VlcRuntimeClass<T> *vClass = static_cast<VlcRuntimeClass<T> *>(npobj->_class);
    return vClass->indexOfProperty(name) != -1;
}

template<class T>
bool vlcRuntimeClassGetProperty(NPObject *npobj, NPIdentifier name, NPVariant *result)
{
    const VlcRuntimeClass<T> *vClass = static_cast<VlcRuntimeClass<T> *>(npobj->_class);
    int index = vClass->indexOfProperty(name);
    if( index != -1 )
    {
        VlcRuntimeObject *vObj = static_cast<VlcRuntimeObject *>(npobj);
        return vObj->getProperty(index, result);
    }
    return false;
}

template<class T>
bool vlcRuntimeClassSetProperty(NPObject *npobj, NPIdentifier name, const NPVariant *value)
{
    const VlcRuntimeClass<T> *vClass = static_cast<VlcRuntimeClass<T> *>(npobj->_class);
    int index = vClass->indexOfProperty(name);
    if( index != -1 )
    {
        VlcRuntimeObject *vObj = static_cast<VlcRuntimeObject *>(npobj);
        return vObj->setProperty(index, value);
    }
    return false;
}

template<class T>
bool vlcRuntimeClassRemoveProperty(NPObject *npobj, NPIdentifier name)
{
    const VlcRuntimeClass<T> *vClass = static_cast<VlcRuntimeClass<T> *>(npobj->_class);
    int index = vClass->indexOfProperty(name);
    if( index != -1 )
    {
        VlcRuntimeObject *vObj = static_cast<VlcRuntimeObject *>(npobj);
        return vObj->removeProperty(index);
    }
    return false;
}

template<class T>
static bool vlcRuntimeClassInvoke(NPObject *npobj, NPIdentifier name,
                                    const NPVariant *args, uint32_t argCount,
                                    NPVariant *result)
{
    const VlcRuntimeClass<T> *vClass = static_cast<VlcRuntimeClass<T> *>(npobj->_class);
    int index = vClass->indexOfMethod(name);
    if( index != -1 )
    {
        VlcRuntimeObject *vObj = static_cast<VlcRuntimeObject *>(npobj);
        return vObj->invoke(index, args, argCount, result);
    }
    return false;
}

template<class T>
static bool vlcRuntimeClassInvokeDefault(NPObject *npobj,
                                           const NPVariant *args,
                                           uint32_t argCount,
                                           NPVariant *result)
{
    VlcRuntimeObject *vObj = static_cast<VlcRuntimeObject *>(npobj);
    return vObj->invokeDefault(args, argCount, result);
}

template<class T>
VlcRuntimeClass<T>::VlcRuntimeClass()
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
    allocate       = vlcRuntimeClassAllocate<T>;
    deallocate     = vlcRuntimeClassDeallocate<T>;
    invalidate     = vlcRuntimeClassInvalidate<T>;
    hasMethod      = vlcRuntimeClassHasMethod<T>;
    invoke         = vlcRuntimeClassInvoke<T>;
    invokeDefault  = vlcRuntimeClassInvokeDefault<T>;
    hasProperty    = vlcRuntimeClassHasProperty<T>;
    getProperty    = vlcRuntimeClassGetProperty<T>;
    setProperty    = vlcRuntimeClassSetProperty<T>;
    removeProperty = vlcRuntimeClassRemoveProperty<T>;
}

template<class T>
VlcRuntimeClass<T>::~VlcRuntimeClass()
{
    delete propertyIdentifiers;
    delete methodIdentifiers;
}

template<class T>
VlcRuntimeObject *VlcRuntimeClass<T>::create(NPP instance) const
{
    return new T(instance, this);
}

template<class T>
int VlcRuntimeClass<T>::indexOfMethod(NPIdentifier name) const
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
int VlcRuntimeClass<T>::indexOfProperty(NPIdentifier name) const
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

/*
** defined runtime script objects
*/

class VlcRuntimeRootObject: public VlcRuntimeObject
{
public:
    VlcRuntimeRootObject(NPP instance, const NPClass *aClass) :
        VlcRuntimeObject(instance, aClass) {};
    virtual ~VlcRuntimeRootObject() {};

    static const int propertyCount;
    static const NPUTF8 * const propertyNames[];

    static const int methodCount;
    static const NPUTF8 * const methodNames[];

    virtual bool getProperty(int index, NPVariant *result);
    virtual bool setProperty(int index, const NPVariant *value);
    virtual bool removeProperty(int index);
    virtual bool invoke(int index, const NPVariant *args, uint32_t argCount, NPVariant *result);
    virtual bool invokeDefault(const NPVariant *args, uint32_t argCount, NPVariant *result);
};

