/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Mozilla/Firefox plugin for VLC
 * Copyright (C) 2009, Jean-Paul Saman <jpsaman@videolan.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stephen Mak <smak@sun.com>
 *
 */

/*
 * npunix.c
 *
 * Netscape Client Plugin API
 * - Wrapper function to interface with the Netscape Navigator
 *
 * dp Suresh <dp@netscape.com>
 *
 *----------------------------------------------------------------------
 * PLUGIN DEVELOPERS:
 *  YOU WILL NOT NEED TO EDIT THIS FILE.
 *----------------------------------------------------------------------
 */

#include "config.h"

#define XP_UNIX 1
#define OJI 1

#include <npapi.h>
#ifdef HAVE_NPFUNCTIONS_H
#include <npfunctions.h>
#else
#include <npupp.h>
#endif

#include "../vlcshell.h"

/*
 * Define PLUGIN_TRACE to have the wrapper functions print
 * messages to stderr whenever they are called.
 */

#ifdef PLUGIN_TRACE
#include <stdio.h>
#define PLUGINDEBUGSTR(msg) fprintf(stderr, "%s\n", msg)
#else
#define PLUGINDEBUGSTR(msg)
#endif

/***********************************************************************
 *
 * Globals
 *
 ***********************************************************************/

static NPNetscapeFuncs   gNetscapeFuncs;    /* Netscape Function table */

/***********************************************************************
 *
 * Wrapper functions : plugin calling Netscape Navigator
 *
 * These functions let the plugin developer just call the APIs
 * as documented and defined in npapi.h, without needing to know
 * about the function table and call macros in npupp.h.
 *
 ***********************************************************************/

void
NPN_Version(int* plugin_major, int* plugin_minor,
         int* netscape_major, int* netscape_minor)
{
    *plugin_major = NP_VERSION_MAJOR;
    *plugin_minor = NP_VERSION_MINOR;

    /* Major version is in high byte */
    *netscape_major = gNetscapeFuncs.version >> 8;
    /* Minor version is in low byte */
    *netscape_minor = gNetscapeFuncs.version & 0xFF;
}

NPError
NPN_GetValue(NPP instance, NPNVariable variable, void *r_value)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    return CallNPN_GetValueProc(gNetscapeFuncs.getvalue,
                    instance, variable, r_value);
#else
    return (*gNetscapeFuncs.getvalue)(instance, variable, r_value);
#endif
}

NPError
NPN_SetValue(NPP instance, NPPVariable variable, void *value)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    return CallNPN_SetValueProc(gNetscapeFuncs.setvalue,
                    instance, variable, value);
#else
    return (*gNetscapeFuncs.setvalue)(instance, variable, value);
#endif
}

NPError
NPN_GetURL(NPP instance, const char* url, const char* window)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    return CallNPN_GetURLProc(gNetscapeFuncs.geturl, instance, url, window);
#else
    return (*gNetscapeFuncs.geturl)(instance, url, window);
#endif
}

NPError
NPN_GetURLNotify(NPP instance, const char* url, const char* window, void* notifyData)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    return CallNPN_GetURLNotifyProc(gNetscapeFuncs.geturlnotify, instance, url, window, notifyData);
#else
    return (*gNetscapeFuncs.geturlnotify)(instance, url, window, notifyData);
#endif
}

NPError
NPN_PostURL(NPP instance, const char* url, const char* window,
         uint32_t len, const char* buf, NPBool file)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    return CallNPN_PostURLProc(gNetscapeFuncs.posturl, instance,
                    url, window, len, buf, file);
#else
    return (*gNetscapeFuncs.posturl)(instance, url, window, len, buf, file);
#endif
}

NPError
NPN_PostURLNotify(NPP instance, const char* url, const char* window, uint32_t len,
                  const char* buf, NPBool file, void* notifyData)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    return CallNPN_PostURLNotifyProc(gNetscapeFuncs.posturlnotify,
            instance, url, window, len, buf, file, notifyData);
#else
    return (*gNetscapeFuncs.posturlnotify)(instance, url, window, len, buf, file, notifyData);

#endif
}

NPError
NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    return CallNPN_RequestReadProc(gNetscapeFuncs.requestread,
                    stream, rangeList);
#else
    return (*gNetscapeFuncs.requestread)(stream, rangeList);
#endif
}

NPError
NPN_NewStream(NPP instance, NPMIMEType type, const char *window,
          NPStream** stream_ptr)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    return CallNPN_NewStreamProc(gNetscapeFuncs.newstream, instance,
                    type, window, stream_ptr);
#else
    return (*gNetscapeFuncs.newstream)(instance, type, window, stream_ptr);
#endif
}

int32_t
NPN_Write(NPP instance, NPStream* stream, int32_t len, void* buffer)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    return CallNPN_WriteProc(gNetscapeFuncs.write, instance,
                    stream, len, buffer);
#else
    return (*gNetscapeFuncs.write)(instance, stream, len, buffer);
#endif
}

NPError
NPN_DestroyStream(NPP instance, NPStream* stream, NPError reason)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    return CallNPN_DestroyStreamProc(gNetscapeFuncs.destroystream,
                        instance, stream, reason);
#else
    return (*gNetscapeFuncs.destroystream)(instance, stream, reason);
#endif
}

void
NPN_Status(NPP instance, const char* message)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    CallNPN_StatusProc(gNetscapeFuncs.status, instance, message);
#else
    (*gNetscapeFuncs.status)(instance, message);
#endif
}

const char*
NPN_UserAgent(NPP instance)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    return CallNPN_UserAgentProc(gNetscapeFuncs.uagent, instance);
#else
    return (*gNetscapeFuncs.uagent)(instance);
#endif
}

void *NPN_MemAlloc(uint32_t size)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    return CallNPN_MemAllocProc(gNetscapeFuncs.memalloc, size);
#else
    return (*gNetscapeFuncs.memalloc)(size);
#endif
}

void NPN_MemFree(void* ptr)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    CallNPN_MemFreeProc(gNetscapeFuncs.memfree, ptr);
#else
    (*gNetscapeFuncs.memfree)(ptr);
#endif
}

uint32_t NPN_MemFlush(uint32_t size)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    return CallNPN_MemFlushProc(gNetscapeFuncs.memflush, size);
#else
    return (*gNetscapeFuncs.memflush)(size);
#endif
}

void NPN_ReloadPlugins(NPBool reloadPages)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    CallNPN_ReloadPluginsProc(gNetscapeFuncs.reloadplugins, reloadPages);
#else
    (*gNetscapeFuncs.reloadplugins)(reloadPages);
#endif
}

#ifdef OJI
JRIEnv* NPN_GetJavaEnv()
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    return CallNPN_GetJavaEnvProc(gNetscapeFuncs.getJavaEnv);
#else
    return (*gNetscapeFuncs.getJavaEnv);
#endif
}

jref NPN_GetJavaPeer(NPP instance)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    return CallNPN_GetJavaPeerProc(gNetscapeFuncs.getJavaPeer,
                       instance);
#else
    return (*gNetscapeFuncs.getJavaPeer)(instance);
#endif
}
#endif

void
NPN_InvalidateRect(NPP instance, NPRect *invalidRect)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    CallNPN_InvalidateRectProc(gNetscapeFuncs.invalidaterect, instance,
        invalidRect);
#else
    (*gNetscapeFuncs.invalidaterect)(instance, invalidRect);
#endif
}

void
NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    CallNPN_InvalidateRegionProc(gNetscapeFuncs.invalidateregion, instance,
        invalidRegion);
#else
    (*gNetscapeFuncs.invalidateregion)(instance, invalidRegion);
#endif
}

void
NPN_ForceRedraw(NPP instance)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    CallNPN_ForceRedrawProc(gNetscapeFuncs.forceredraw, instance);
#else
    (*gNetscapeFuncs.forceredraw)(instance);
#endif
}

void NPN_PushPopupsEnabledState(NPP instance, NPBool enabled)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    CallNPN_PushPopupsEnabledStateProc(gNetscapeFuncs.pushpopupsenabledstate,
        instance, enabled);
#else
    (*gNetscapeFuncs.pushpopupsenabledstate)(instance, enabled);
#endif
}

void NPN_PopPopupsEnabledState(NPP instance)
{
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    CallNPN_PopPopupsEnabledStateProc(gNetscapeFuncs.poppopupsenabledstate,
        instance);
#else
    (*gNetscapeFuncs.poppopupsenabledstate)(instance);
#endif
}

NPIdentifier NPN_GetStringIdentifier(const NPUTF8 *name)
{
    int minor = gNetscapeFuncs.version & 0xFF;
    if( minor >= 14 )
    {
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
        return CallNPN_GetStringIdentifierProc(
                        gNetscapeFuncs.getstringidentifier, name);
#else
        return (*gNetscapeFuncs.getstringidentifier)(name);
#endif
    }
    return NULL;
}

void NPN_GetStringIdentifiers(const NPUTF8 **names, int32_t nameCount,
                              NPIdentifier *identifiers)
{
    int minor = gNetscapeFuncs.version & 0xFF;
    if( minor >= 14 )
    {
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
        CallNPN_GetStringIdentifiersProc(gNetscapeFuncs.getstringidentifiers,
                                         names, nameCount, identifiers);
#else
        (*gNetscapeFuncs.getstringidentifiers)(names, nameCount, identifiers);
#endif
    }
}

NPIdentifier NPN_GetIntIdentifier(int32_t intid)
{
    int minor = gNetscapeFuncs.version & 0xFF;
    if( minor >= 14 )
    {
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
        return CallNPN_GetIntIdentifierProc(gNetscapeFuncs.getintidentifier, intid);
#else
        return (*gNetscapeFuncs.getintidentifier)(intid);
#endif
    }
    return NULL;
}

bool NPN_IdentifierIsString(NPIdentifier identifier)
{
    int minor = gNetscapeFuncs.version & 0xFF;
    if( minor >= 14 )
    {
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
        return CallNPN_IdentifierIsStringProc(
                        gNetscapeFuncs.identifierisstring,
                        identifier);
#else
        return (*gNetscapeFuncs.identifierisstring)(identifier);
#endif
    }
    return false;
}

NPUTF8 *NPN_UTF8FromIdentifier(NPIdentifier identifier)
{
    int minor = gNetscapeFuncs.version & 0xFF;
    if( minor >= 14 )
    {
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
        return CallNPN_UTF8FromIdentifierProc(
                            gNetscapeFuncs.utf8fromidentifier,
                            identifier);
#else
        return (*gNetscapeFuncs.utf8fromidentifier)(identifier);
#endif
    }
    return NULL;
}

int32_t NPN_IntFromIdentifier(NPIdentifier identifier)
{
    int minor = gNetscapeFuncs.version & 0xFF;
    if( minor >= 14 )
    {
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
        return CallNPN_IntFromIdentifierProc(
                        gNetscapeFuncs.intfromidentifier,
                        identifier);
#else
        return (*gNetscapeFuncs.intfromidentifier)(identifier);
#endif
    }
    return 0;
}

NPObject *NPN_CreateObject(NPP npp, NPClass *aClass)
{
    int minor = gNetscapeFuncs.version & 0xFF;
    if( minor >= 14 )
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
        return CallNPN_CreateObjectProc(gNetscapeFuncs.createobject, npp, aClass);
#else
        return (*gNetscapeFuncs.createobject)(npp, aClass);
#endif
    return NULL;
}

NPObject *NPN_RetainObject(NPObject *obj)
{
    int minor = gNetscapeFuncs.version & 0xFF;
    if( minor >= 14 )
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
        return CallNPN_RetainObjectProc(gNetscapeFuncs.retainobject, obj);
#else
        return (*gNetscapeFuncs.retainobject)(obj);
#endif
    return NULL;
}

void NPN_ReleaseObject(NPObject *obj)
{
    int minor = gNetscapeFuncs.version & 0xFF;
    if( minor >= 14 )
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
        CallNPN_ReleaseObjectProc(gNetscapeFuncs.releaseobject, obj);
#else
        (*gNetscapeFuncs.releaseobject)(obj);
#endif
}

bool NPN_Invoke(NPP npp, NPObject* obj, NPIdentifier methodName,
                const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    int minor = gNetscapeFuncs.version & 0xFF;
    if( minor >= 14 )
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
        return CallNPN_InvokeProc(gNetscapeFuncs.invoke, npp, obj, methodName,
                                  args, argCount, result);
#else
        return (*gNetscapeFuncs.invoke)(npp, obj, methodName, args, argCount, result);
#endif
    return false;
}

bool NPN_InvokeDefault(NPP npp, NPObject* obj, const NPVariant *args,
                       uint32_t argCount, NPVariant *result)
{
    int minor = gNetscapeFuncs.version & 0xFF;
    if( minor >= 14 )
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
        return CallNPN_InvokeDefaultProc(gNetscapeFuncs.invokeDefault, npp, obj,
                                         args, argCount, result);
#else
        return (*gNetscapeFuncs.invokeDefault)(npp, obj, args, argCount, result);
#endif
    return false;
}

bool NPN_Evaluate(NPP npp, NPObject* obj, NPString *script,
                  NPVariant *result)
{
    int minor = gNetscapeFuncs.version & 0xFF;
    if( minor >= 14 )
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
        return CallNPN_EvaluateProc(gNetscapeFuncs.evaluate, npp, obj,
                                    script, result);
#else
        return (*gNetscapeFuncs.evaluate)(npp, obj, script, result);
#endif
    return false;
}

bool NPN_GetProperty(NPP npp, NPObject* obj, NPIdentifier propertyName,
                     NPVariant *result)
{
    int minor = gNetscapeFuncs.version & 0xFF;
    if( minor >= 14 )
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
        return CallNPN_GetPropertyProc(gNetscapeFuncs.getproperty, npp, obj,
                                       propertyName, result);
#else
        return (*gNetscapeFuncs.getproperty)(npp, obj, propertyName, result);
#endif
    return false;
}

bool NPN_SetProperty(NPP npp, NPObject* obj, NPIdentifier propertyName,
                     const NPVariant *value)
{
    int minor = gNetscapeFuncs.version & 0xFF;
    if( minor >= 14 )
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
        return CallNPN_SetPropertyProc(gNetscapeFuncs.setproperty, npp, obj,
                                       propertyName, value);
#else
        return (*gNetscapeFuncs.setproperty)(npp, obj, propertyName, value);
#endif
    return false;
}

bool NPN_RemoveProperty(NPP npp, NPObject* obj, NPIdentifier propertyName)
{
    int minor = gNetscapeFuncs.version & 0xFF;
    if( minor >= 14 )
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
        return CallNPN_RemovePropertyProc(gNetscapeFuncs.removeproperty, npp, obj,
                                          propertyName);
#else
        return (*gNetscapeFuncs.removeproperty)(npp, obj, propertyName);
#endif
    return false;
}

bool NPN_HasProperty(NPP npp, NPObject* obj, NPIdentifier propertyName)
{
    int minor = gNetscapeFuncs.version & 0xFF;
    if( minor >= 14 )
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
        return CallNPN_HasPropertyProc(gNetscapeFuncs.hasproperty, npp, obj,
                                       propertyName);
#else
        return (*gNetscapeFuncs.hasproperty)(npp, obj, propertyName);
#endif
    return false;
}

bool NPN_HasMethod(NPP npp, NPObject* obj, NPIdentifier methodName)
{
    int minor = gNetscapeFuncs.version & 0xFF;
    if( minor >= 14 )
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
        return CallNPN_HasMethodProc(gNetscapeFuncs.hasmethod, npp,
                                     obj, methodName);
#else
        return (*gNetscapeFuncs.hasmethod)(npp, obj, methodName);
#endif
    return false;
}

void NPN_ReleaseVariantValue(NPVariant *variant)
{
    int minor = gNetscapeFuncs.version & 0xFF;
    if( minor >= 14 )
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
        CallNPN_ReleaseVariantValueProc(gNetscapeFuncs.releasevariantvalue, variant);
#else
        (*gNetscapeFuncs.releasevariantvalue)(variant);
#endif
}

void NPN_SetException(NPObject* obj, const NPUTF8 *message)
{
    int minor = gNetscapeFuncs.version & 0xFF;
    if( minor >= 14 )
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
        CallNPN_SetExceptionProc(gNetscapeFuncs.setexception, obj, message);
#else
        (*gNetscapeFuncs.setexception)(obj, message);
#endif
}

/***********************************************************************
 *
 * Wrapper functions : Netscape Navigator -> plugin
 *
 * These functions let the plugin developer just create the APIs
 * as documented and defined in npapi.h, without needing to 
 * install those functions in the function table or worry about
 * setting up globals for 68K plugins.
 *
 ***********************************************************************/

/* Function prototypes */
NPError Private_New(NPMIMEType pluginType, NPP instance, uint16_t mode,
        int16_t argc, char* argn[], char* argv[], NPSavedData* saved);
NPError Private_Destroy(NPP instance, NPSavedData** save);
NPError Private_SetWindow(NPP instance, NPWindow* window);
NPError Private_NewStream(NPP instance, NPMIMEType type, NPStream* stream,
                          NPBool seekable, uint16_t* stype);
int32_t Private_WriteReady(NPP instance, NPStream* stream);
int32_t Private_Write(NPP instance, NPStream* stream, int32_t offset,
                    int32_t len, void* buffer);
void Private_StreamAsFile(NPP instance, NPStream* stream, const char* fname);
NPError Private_DestroyStream(NPP instance, NPStream* stream, NPError reason);
void Private_URLNotify(NPP instance, const char* url,
                       NPReason reason, void* notifyData);
void Private_Print(NPP instance, NPPrint* platformPrint);
NPError Private_GetValue(NPP instance, NPPVariable variable, void *r_value);
NPError Private_SetValue(NPP instance, NPPVariable variable, void *r_value);
#ifdef OJI
JRIGlobalRef Private_GetJavaClass(void);
#endif

/* function implementations */
NPError
Private_New(NPMIMEType pluginType, NPP instance, uint16_t mode,
        int16_t argc, char* argn[], char* argv[], NPSavedData* saved)
{
    NPError ret;
    PLUGINDEBUGSTR("New");
    ret = NPP_New(pluginType, instance, mode, argc, argn, argv, saved);
    return ret; 
}

NPError
Private_Destroy(NPP instance, NPSavedData** save)
{
    PLUGINDEBUGSTR("Destroy");
    return NPP_Destroy(instance, save);
}

NPError
Private_SetWindow(NPP instance, NPWindow* window)
{
    NPError err;
    PLUGINDEBUGSTR("SetWindow");
    err = NPP_SetWindow(instance, window);
    return err;
}

NPError
Private_NewStream(NPP instance, NPMIMEType type, NPStream* stream,
            NPBool seekable, uint16_t* stype)
{
    NPError err;
    PLUGINDEBUGSTR("NewStream");
    err = NPP_NewStream(instance, type, stream, seekable, stype);
    return err;
}

int32_t
Private_WriteReady(NPP instance, NPStream* stream)
{
    unsigned int result;
    PLUGINDEBUGSTR("WriteReady");
    result = NPP_WriteReady(instance, stream);
    return result;
}

int32_t
Private_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len,
        void* buffer)
{
    unsigned int result;
    PLUGINDEBUGSTR("Write");
    result = NPP_Write(instance, stream, offset, len, buffer);
    return result;
}

void
Private_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
    PLUGINDEBUGSTR("StreamAsFile");
    NPP_StreamAsFile(instance, stream, fname);
}


NPError
Private_DestroyStream(NPP instance, NPStream* stream, NPError reason)
{
    NPError err;
    PLUGINDEBUGSTR("DestroyStream");
    err = NPP_DestroyStream(instance, stream, reason);
    return err;
}

void
Private_URLNotify(NPP instance, const char* url,
                NPReason reason, void* notifyData)
{
    PLUGINDEBUGSTR("URLNotify");
    NPP_URLNotify(instance, url, reason, notifyData);
}

void
Private_Print(NPP instance, NPPrint* platformPrint)
{
    PLUGINDEBUGSTR("Print");
    NPP_Print(instance, platformPrint);
}

NPError
Private_GetValue(NPP instance, NPPVariable variable, void *r_value)
{
    PLUGINDEBUGSTR("GetValue");
    return NPP_GetValue(instance, variable, r_value);
}

NPError
Private_SetValue(NPP instance, NPPVariable variable, void *r_value)
{
    PLUGINDEBUGSTR("SetValue");
    return NPP_SetValue(instance, variable, r_value);
}

#ifdef OJI
JRIGlobalRef
Private_GetJavaClass(void)
{
    jref clazz = NPP_GetJavaClass();
    if (clazz) {
    JRIEnv* env = NPN_GetJavaEnv();
    return JRI_NewGlobalRef(env, clazz);
    }
    return NULL;
}
#endif

/*********************************************************************** 
 *
 * These functions are located automagically by netscape.
 *
 ***********************************************************************/

/*
 * NP_GetMIMEDescription
 *  - Netscape needs to know about this symbol
 *  - Netscape uses the return value to identify when an object instance
 *    of this plugin should be created.
 */
char *
NP_GetMIMEDescription(void)
{
    return NPP_GetMIMEDescription();
}

/*
 * NP_GetValue [optional]
 *  - Netscape needs to know about this symbol.
 *  - Interfaces with plugin to get values for predefined variables
 *    that the navigator needs.
 */
NPError
NP_GetValue(void* future, NPPVariable variable, void *value)
{
    return NPP_GetValue(future, variable, value);
}

/*
 * NP_Initialize
 *  - Netscape needs to know about this symbol.
 *  - It calls this function after looking up its symbol before it
 *    is about to create the first ever object of this kind.
 *
 * PARAMETERS
 *    nsTable   - The netscape function table. If developers just use these
 *        wrappers, they don't need to worry about all these function
 *        tables.
 * RETURN
 *    pluginFuncs
 *      - This functions needs to fill the plugin function table
 *        pluginFuncs and return it. Netscape Navigator plugin
 *        library will use this function table to call the plugin.
 *
 */
NPError
NP_Initialize(NPNetscapeFuncs* nsTable, NPPluginFuncs* pluginFuncs)
{
    NPError err = NPERR_NO_ERROR;

    PLUGINDEBUGSTR("NP_Initialize");

    /* validate input parameters */
    if ((nsTable == NULL) || (pluginFuncs == NULL))
        err = NPERR_INVALID_FUNCTABLE_ERROR;

    /*
     * Check the major version passed in Netscape's function table.
     * We won't load if the major version is newer than what we expect.
     * Also check that the function tables passed in are big enough for
     * all the functions we need (they could be bigger, if Netscape added
     * new APIs, but that's OK with us -- we'll just ignore them).
     *
     */
    if (err == NPERR_NO_ERROR) {
        if ((nsTable->version >> 8) > NP_VERSION_MAJOR)
            err = NPERR_INCOMPATIBLE_VERSION_ERROR;
        if (nsTable->size < ((char *)&nsTable->posturlnotify - (char *)nsTable))
            err = NPERR_INVALID_FUNCTABLE_ERROR;
        if (pluginFuncs->size < sizeof(NPPluginFuncs))
            err = NPERR_INVALID_FUNCTABLE_ERROR;
    }

    if (err == NPERR_NO_ERROR)
    {
        /*
         * Copy all the fields of Netscape function table into our
         * copy so we can call back into Netscape later.  Note that
         * we need to copy the fields one by one, rather than assigning
         * the whole structure, because the Netscape function table
         * could actually be bigger than what we expect.
         */
        int minor = nsTable->version & 0xFF;

        gNetscapeFuncs.version       = nsTable->version;
        gNetscapeFuncs.size          = nsTable->size;
        gNetscapeFuncs.posturl       = nsTable->posturl;
        gNetscapeFuncs.geturl        = nsTable->geturl;
        gNetscapeFuncs.requestread   = nsTable->requestread;
        gNetscapeFuncs.newstream     = nsTable->newstream;
        gNetscapeFuncs.write         = nsTable->write;
        gNetscapeFuncs.destroystream = nsTable->destroystream;
        gNetscapeFuncs.status        = nsTable->status;
        gNetscapeFuncs.uagent        = nsTable->uagent;
        gNetscapeFuncs.memalloc      = nsTable->memalloc;
        gNetscapeFuncs.memfree       = nsTable->memfree;
        gNetscapeFuncs.memflush      = nsTable->memflush;
        gNetscapeFuncs.reloadplugins = nsTable->reloadplugins;
#ifdef OJI
        if( minor >= NPVERS_HAS_LIVECONNECT )
        {
            gNetscapeFuncs.getJavaEnv    = nsTable->getJavaEnv;
            gNetscapeFuncs.getJavaPeer   = nsTable->getJavaPeer;
        }
#endif
        gNetscapeFuncs.getvalue      = nsTable->getvalue;
        gNetscapeFuncs.setvalue      = nsTable->setvalue;

        if( minor >= NPVERS_HAS_NOTIFICATION )
        {
            gNetscapeFuncs.geturlnotify  = nsTable->geturlnotify;
            gNetscapeFuncs.posturlnotify = nsTable->posturlnotify;
        }

        if (nsTable->size >= ((char *)&nsTable->setexception - (char *)nsTable))
        {
            gNetscapeFuncs.invalidaterect = nsTable->invalidaterect;
            gNetscapeFuncs.invalidateregion = nsTable->invalidateregion;
            gNetscapeFuncs.forceredraw = nsTable->forceredraw;
            /* npruntime support */
            if (minor >= 14)
            {
                gNetscapeFuncs.getstringidentifier = nsTable->getstringidentifier;
                gNetscapeFuncs.getstringidentifiers = nsTable->getstringidentifiers;
                gNetscapeFuncs.getintidentifier = nsTable->getintidentifier;
                gNetscapeFuncs.identifierisstring = nsTable->identifierisstring;
                gNetscapeFuncs.utf8fromidentifier = nsTable->utf8fromidentifier;
                gNetscapeFuncs.intfromidentifier = nsTable->intfromidentifier;
                gNetscapeFuncs.createobject = nsTable->createobject;
                gNetscapeFuncs.retainobject = nsTable->retainobject;
                gNetscapeFuncs.releaseobject = nsTable->releaseobject;
                gNetscapeFuncs.invoke = nsTable->invoke;
                gNetscapeFuncs.invokeDefault = nsTable->invokeDefault;
                gNetscapeFuncs.evaluate = nsTable->evaluate;
                gNetscapeFuncs.getproperty = nsTable->getproperty;
                gNetscapeFuncs.setproperty = nsTable->setproperty;
                gNetscapeFuncs.removeproperty = nsTable->removeproperty;
                gNetscapeFuncs.hasproperty = nsTable->hasproperty;
                gNetscapeFuncs.hasmethod = nsTable->hasmethod;
                gNetscapeFuncs.releasevariantvalue = nsTable->releasevariantvalue;
                gNetscapeFuncs.setexception = nsTable->setexception;
            }
        }
        else
        {
            gNetscapeFuncs.invalidaterect = NULL;
            gNetscapeFuncs.invalidateregion = NULL;
            gNetscapeFuncs.forceredraw = NULL;
            gNetscapeFuncs.getstringidentifier = NULL;
            gNetscapeFuncs.getstringidentifiers = NULL;
            gNetscapeFuncs.getintidentifier = NULL;
            gNetscapeFuncs.identifierisstring = NULL;
            gNetscapeFuncs.utf8fromidentifier = NULL;
            gNetscapeFuncs.intfromidentifier = NULL;
            gNetscapeFuncs.createobject = NULL;
            gNetscapeFuncs.retainobject = NULL;
            gNetscapeFuncs.releaseobject = NULL;
            gNetscapeFuncs.invoke = NULL;
            gNetscapeFuncs.invokeDefault = NULL;
            gNetscapeFuncs.evaluate = NULL;
            gNetscapeFuncs.getproperty = NULL;
            gNetscapeFuncs.setproperty = NULL;
            gNetscapeFuncs.removeproperty = NULL;
            gNetscapeFuncs.hasproperty = NULL;
            gNetscapeFuncs.releasevariantvalue = NULL;
            gNetscapeFuncs.setexception = NULL;
        }
        if (nsTable->size >=
            ((char *)&nsTable->poppopupsenabledstate - (char *)nsTable))
        {
            gNetscapeFuncs.pushpopupsenabledstate = nsTable->pushpopupsenabledstate;
            gNetscapeFuncs.poppopupsenabledstate  = nsTable->poppopupsenabledstate;
        }
        else
        {
            gNetscapeFuncs.pushpopupsenabledstate = NULL;
            gNetscapeFuncs.poppopupsenabledstate  = NULL;
        }

        /*
         * Set up the plugin function table that Netscape will use to
         * call us.  Netscape needs to know about our version and size
         * and have a UniversalProcPointer for every function we
         * implement.
         */
        pluginFuncs->version    = (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;
        pluginFuncs->size       = sizeof(NPPluginFuncs);
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
        pluginFuncs->newp       = NewNPP_NewProc(Private_New);
        pluginFuncs->destroy    = NewNPP_DestroyProc(Private_Destroy);
        pluginFuncs->setwindow  = NewNPP_SetWindowProc(Private_SetWindow);
        pluginFuncs->newstream  = NewNPP_NewStreamProc(Private_NewStream);
        pluginFuncs->destroystream = NewNPP_DestroyStreamProc(Private_DestroyStream);
        pluginFuncs->asfile     = NewNPP_StreamAsFileProc(Private_StreamAsFile);
        pluginFuncs->writeready = NewNPP_WriteReadyProc(Private_WriteReady);
        pluginFuncs->write      = NewNPP_WriteProc(Private_Write);
        pluginFuncs->print      = NewNPP_PrintProc(Private_Print);
        pluginFuncs->getvalue   = NewNPP_GetValueProc(Private_GetValue);
        pluginFuncs->setvalue   = NewNPP_SetValueProc(Private_SetValue);
#else
        pluginFuncs->newp       = (NPP_NewProcPtr)(Private_New);
        pluginFuncs->destroy    = (NPP_DestroyProcPtr)(Private_Destroy);
        pluginFuncs->setwindow  = (NPP_SetWindowProcPtr)(Private_SetWindow);
        pluginFuncs->newstream  = (NPP_NewStreamProcPtr)(Private_NewStream);
        pluginFuncs->destroystream = (NPP_DestroyStreamProcPtr)(Private_DestroyStream);
        pluginFuncs->asfile     = (NPP_StreamAsFileProcPtr)(Private_StreamAsFile);
        pluginFuncs->writeready = (NPP_WriteReadyProcPtr)(Private_WriteReady);
        pluginFuncs->write      = (NPP_WriteProcPtr)(Private_Write);
        pluginFuncs->print      = (NPP_PrintProcPtr)(Private_Print);
        pluginFuncs->getvalue   = (NPP_GetValueProcPtr)(Private_GetValue);
        pluginFuncs->setvalue   = (NPP_SetValueProcPtr)(Private_SetValue);
#endif
        pluginFuncs->event      = NULL;
        if( minor >= NPVERS_HAS_NOTIFICATION )
        {
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
            pluginFuncs->urlnotify = NewNPP_URLNotifyProc(Private_URLNotify);
#else
            pluginFuncs->urlnotify = (NPP_URLNotifyProcPtr)(Private_URLNotify);
#endif
        }
#ifdef OJI
        if( minor >= NPVERS_HAS_LIVECONNECT )
            pluginFuncs->javaClass  = Private_GetJavaClass();
        else
            pluginFuncs->javaClass = NULL;
#else
        pluginFuncs->javaClass = NULL;
#endif

        err = NPP_Initialize();
    }

    return err;
}

/*
 * NP_Shutdown [optional]
 *  - Netscape needs to know about this symbol.
 *  - It calls this function after looking up its symbol after
 *    the last object of this kind has been destroyed.
 *
 */
NPError
NP_Shutdown(void)
{
    PLUGINDEBUGSTR("NP_Shutdown");
    NPP_Shutdown();
    return NPERR_NO_ERROR;
}
