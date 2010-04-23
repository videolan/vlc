/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 */

#include "config.h"

//#define OJI 1

#include "../vlcplugin.h"

#ifndef _NPAPI_H_
#   include "npapi.h"
#endif
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
#include "npupp.h" 
#else
#include "npfunctions.h"
#endif

#include "../vlcshell.h"

//\\// DEFINE
#define NP_EXPORT

//\\// GLOBAL DATA
NPNetscapeFuncs* g_pNavigatorFuncs = 0;

#ifdef OJI
JRIGlobalRef Private_GetJavaClass(void);

//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\.
////\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//.
// Private_GetJavaClass (global function)
//
//	Given a Java class reference (thru NPP_GetJavaClass) inform JRT
//	of this class existence
//
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
#endif /* OJI */

//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\.
////\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//.
//						PLUGIN DLL entry points   
//
// These are the Windows specific DLL entry points. They must be exoprted
//

// we need these to be global since we have to fill one of its field
// with a data (class) which requires knowlwdge of the navigator
// jump-table. This jump table is known at Initialize time (NP_Initialize)
// which is called after NP_GetEntryPoint
static NPPluginFuncs* g_pluginFuncs;

//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\.
////\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//.
// NP_GetEntryPoints
//
//	fills in the func table used by Navigator to call entry points in
//  plugin DLL.  Note that these entry points ensure that DS is loaded
//  by using the NP_LOADDS macro, when compiling for Win16
//
#ifdef __MINGW32__
extern "C" __declspec(dllexport) NPError WINAPI
#else
NPError WINAPI NP_EXPORT
#endif
NP_GetEntryPoints(NPPluginFuncs* pFuncs)
{
    // trap a NULL ptr 
    if(pFuncs == NULL)
        return NPERR_INVALID_FUNCTABLE_ERROR;

    // if the plugin's function table is smaller than the plugin expects,
    // then they are incompatible, and should return an error 

    pFuncs->version       = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
    pFuncs->newp          = NPP_New;
    pFuncs->destroy       = NPP_Destroy;
    pFuncs->setwindow     = NPP_SetWindow;
    pFuncs->newstream     = NPP_NewStream;
    pFuncs->destroystream = NPP_DestroyStream;
    pFuncs->asfile        = NPP_StreamAsFile;
    pFuncs->writeready    = NPP_WriteReady;
    pFuncs->write         = NPP_Write;
    pFuncs->print         = NPP_Print;
    pFuncs->event         = 0;       /// reserved
    pFuncs->getvalue      = NPP_GetValue;
    pFuncs->setvalue      = NPP_SetValue;

    g_pluginFuncs         = pFuncs;

    return NPERR_NO_ERROR;
}

//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\.
////\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//.
// NP_Initialize
//
//	called immediately after the plugin DLL is loaded
//
#ifdef __MINGW32__
extern "C" __declspec(dllexport) NPError WINAPI
#else
NPError WINAPI NP_EXPORT
#endif
NP_Initialize(NPNetscapeFuncs* pFuncs)
{
    // trap a NULL ptr 
    if(pFuncs == NULL)
        return NPERR_INVALID_FUNCTABLE_ERROR;

    g_pNavigatorFuncs = pFuncs; // save it for future reference 

    // if the plugin's major ver level is lower than the Navigator's,
    // then they are incompatible, and should return an error 
    if(HIBYTE(pFuncs->version) > NP_VERSION_MAJOR)
        return NPERR_INCOMPATIBLE_VERSION_ERROR;

    // We have to defer these assignments until g_pNavigatorFuncs is set
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;

    if( navMinorVers >= NPVERS_HAS_NOTIFICATION ) {
        g_pluginFuncs->urlnotify = NPP_URLNotify;
    }
#ifdef OJI	
    if( navMinorVers >= NPVERS_HAS_LIVECONNECT ) {
        g_pluginFuncs->javaClass = Private_GetJavaClass();
    }
#endif
    // NPP_Initialize is a standard (cross-platform) initialize function.
    return NPP_Initialize();
}

//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\.
////\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//.
// NP_Shutdown
//
//	called immediately before the plugin DLL is unloaded.
//	This function should check for some ref count on the dll to see if it is
//	unloadable or it needs to stay in memory. 
//
#ifdef __MINGW32__
extern "C" __declspec(dllexport) NPError WINAPI
#else
NPError WINAPI NP_EXPORT 
#endif
NP_Shutdown()
{
    NPP_Shutdown();
    g_pNavigatorFuncs = NULL;
    return NPERR_NO_ERROR;
}

char * NP_GetMIMEDescription()
{
  return NPP_GetMIMEDescription();
}

//						END - PLUGIN DLL entry points   
////\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//.
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\.

/*    NAVIGATOR Entry points    */

/* These entry points expect to be called from within the plugin.  The
   noteworthy assumption is that DS has already been set to point to the
   plugin's DLL data segment.  Don't call these functions from outside
   the plugin without ensuring DS is set to the DLLs data segment first,
   typically using the NP_LOADDS macro
 */

/* returns the major/minor version numbers of the Plugin API for the plugin
   and the Navigator
 */
void NPN_Version(int* plugin_major, int* plugin_minor, int* netscape_major, int* netscape_minor)
{
    *plugin_major   = NP_VERSION_MAJOR;
    *plugin_minor   = NP_VERSION_MINOR;
    *netscape_major = HIBYTE(g_pNavigatorFuncs->version);
    *netscape_minor = LOBYTE(g_pNavigatorFuncs->version);
}

NPError NPN_GetValue(NPP instance, NPNVariable variable, void *result)
{
    return g_pNavigatorFuncs->getvalue(instance, variable, result);
}

NPError NPN_SetValue(NPP instance, NPPVariable variable, void *value)
{
    return g_pNavigatorFuncs->setvalue(instance, variable, value);
}

void NPN_InvalidateRect(NPP instance, NPRect *rect)
{
    g_pNavigatorFuncs->invalidaterect(instance, rect);
}

void NPN_InvalidateRegion(NPP instance, NPRegion region)
{
    g_pNavigatorFuncs->invalidateregion(instance, region);
}

void NPN_ForceRedraw(NPP instance)
{
    g_pNavigatorFuncs->forceredraw(instance);
}

NPIdentifier NPN_GetStringIdentifier(const NPUTF8 *name)
{
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;
    if( navMinorVers >= 14 )
    {
        return g_pNavigatorFuncs->getstringidentifier(name);
    }
    return NULL;
}

void NPN_GetStringIdentifiers(const NPUTF8 **names, int32_t nameCount, NPIdentifier *identifiers)
{
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;
    if( navMinorVers >= 14 )
    {
        g_pNavigatorFuncs->getstringidentifiers(names, nameCount, identifiers);
    }
}

NPIdentifier NPN_GetIntIdentifier(int32_t intid)
{
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;
    if( navMinorVers >= 14 )
    {
        return g_pNavigatorFuncs->getintidentifier(intid);
    }
    return NULL;
}

bool NPN_IdentifierIsString(NPIdentifier identifier)
{
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;
    if( navMinorVers >= 14 )
    {
        return g_pNavigatorFuncs->identifierisstring(identifier);
    }
    return false;
}

NPUTF8 *NPN_UTF8FromIdentifier(NPIdentifier identifier)
{
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;
    if( navMinorVers >= 14 )
    {
        return g_pNavigatorFuncs->utf8fromidentifier(identifier);
    }
    return NULL;
}

int32_t NPN_IntFromIdentifier(NPIdentifier identifier)
{
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;
    if( navMinorVers >= 14 )
    {
        return g_pNavigatorFuncs->intfromidentifier(identifier);
    }
    return 0;
}

NPObject *NPN_CreateObject(NPP instance, NPClass *aClass)
{
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;
    if( navMinorVers >= 14 )
    {
        return g_pNavigatorFuncs->createobject(instance, aClass);
    }
    return NULL;
}

NPObject *NPN_RetainObject(NPObject *npobj)
{
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;
    if( navMinorVers >= 14 )
    {
        return g_pNavigatorFuncs->retainobject(npobj);
    }
    return NULL;
}

void NPN_ReleaseObject(NPObject *npobj)
{
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;
    if( navMinorVers >= 14 )
    {
        g_pNavigatorFuncs->releaseobject(npobj);
    }
}

bool NPN_Invoke(NPP instance, NPObject *npobj, NPIdentifier methodName, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;
    if( navMinorVers >= 14 )
    {
        return g_pNavigatorFuncs->invoke(instance, npobj, methodName, args, argCount, result);
    }
    return false;
}

bool NPN_InvokeDefault(NPP instance, NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;
    if( navMinorVers >= 14 )
    {
        return g_pNavigatorFuncs->invokeDefault(instance, npobj, args, argCount, result);
    }
    return false;
}

bool NPN_Evaluate(NPP instance, NPObject *npobj, NPString *script, NPVariant *result)
{
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;
    if( navMinorVers >= 14 )
    {
        return g_pNavigatorFuncs->evaluate(instance, npobj, script, result);
    }
    return false;
}

bool NPN_GetProperty(NPP instance, NPObject *npobj, NPIdentifier propertyName, NPVariant *result)
{
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;
    if( navMinorVers >= 14 )
    {
        return g_pNavigatorFuncs->getproperty(instance, npobj, propertyName, result);
    }
    return false;
}

bool NPN_SetProperty(NPP instance, NPObject *npobj, NPIdentifier propertyName, const NPVariant *value)
{
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;
    if( navMinorVers >= 14 )
    {
        return g_pNavigatorFuncs->setproperty(instance, npobj, propertyName, value);
    }
    return false;
}

bool NPN_RemoveProperty(NPP instance, NPObject *npobj, NPIdentifier propertyName)
{
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;
    if( navMinorVers >= 14 )
    {
        return g_pNavigatorFuncs->removeproperty(instance, npobj, propertyName);
    }
    return false;
}

bool NPN_HasProperty(NPP instance, NPObject *npobj, NPIdentifier propertyName)
{
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;
    if( navMinorVers >= 14 )
    {
        return g_pNavigatorFuncs->hasproperty(instance, npobj, propertyName);
    }
    return false;
}

bool NPN_HasMethod(NPP instance, NPObject *npobj, NPIdentifier methodName)
{
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;
    if( navMinorVers >= 14 )
    {
        return g_pNavigatorFuncs->hasmethod(instance, npobj, methodName);
    }
    return false;
}

void NPN_ReleaseVariantValue(NPVariant *variant)
{
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;
    if( navMinorVers >= 14 )
    {
        g_pNavigatorFuncs->releasevariantvalue(variant);
    }
}

void NPN_SetException(NPObject *npobj, const NPUTF8 *message)
{
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;
    if( navMinorVers >= 14 )
    {
        g_pNavigatorFuncs->setexception(npobj, message);
    }
}

/* causes the specified URL to be fetched and streamed in
 */
NPError NPN_GetURLNotify(NPP instance, const char *url, const char *target, void* notifyData)

{
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;
    NPError err;
    if( navMinorVers >= NPVERS_HAS_NOTIFICATION ) {
        err = g_pNavigatorFuncs->geturlnotify(instance, url, target, notifyData);
    }
    else {
        err = NPERR_INCOMPATIBLE_VERSION_ERROR;
    }
    return err;
}

NPError NPN_GetURL(NPP instance, const char *url, const char *target)
{
    return g_pNavigatorFuncs->geturl(instance, url, target);
}

NPError NPN_PostURLNotify(NPP instance, const char* url, const char* window, uint32_t len, const char* buf, NPBool file, void* notifyData)
{
    int navMinorVers = g_pNavigatorFuncs->version & 0xFF;
    NPError err;
    if( navMinorVers >= NPVERS_HAS_NOTIFICATION ) {
        err = g_pNavigatorFuncs->posturlnotify(instance, url, window, len, buf, file, notifyData);
    }
    else {
        err = NPERR_INCOMPATIBLE_VERSION_ERROR;
    }
    return err;
}


NPError NPN_PostURL(NPP instance, const char* url, const char* window, uint32_t len, const char* buf, NPBool file)
{
    return g_pNavigatorFuncs->posturl(instance, url, window, len, buf, file);
}

/* Requests that a number of bytes be provided on a stream.  Typically
   this would be used if a stream was in "pull" mode.  An optional
   position can be provided for streams which are seekable.
 */
NPError NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
    return g_pNavigatorFuncs->requestread(stream, rangeList);
}

/* Creates a new stream of data from the plug-in to be interpreted
 * by Netscape in the current window.
 */
NPError NPN_NewStream(NPP instance, NPMIMEType type, 
                      const char* target, NPStream** stream)
{
    int navMinorVersion = g_pNavigatorFuncs->version & 0xFF;
    NPError err;

    if( navMinorVersion >= NPVERS_HAS_STREAMOUTPUT ) {
        err = g_pNavigatorFuncs->newstream(instance, type, target, stream);
    }
    else {
        err = NPERR_INCOMPATIBLE_VERSION_ERROR;
    }
    return err;
}

/* Provides len bytes of data.
 */
int32_t NPN_Write(NPP instance, NPStream *stream,
                int32_t len, void *buffer)
{
    int navMinorVersion = g_pNavigatorFuncs->version & 0xFF;
    int32_t result;

    if( navMinorVersion >= NPVERS_HAS_STREAMOUTPUT ) {
        result = g_pNavigatorFuncs->write(instance, stream, len, buffer);
    }
    else {
        result = -1;
    }
    return result;
}

/* Closes a stream object.
 * reason indicates why the stream was closed.
 */
NPError NPN_DestroyStream(NPP instance, NPStream* stream, NPError reason)
{
    int navMinorVersion = g_pNavigatorFuncs->version & 0xFF;
    NPError err;

    if( navMinorVersion >= NPVERS_HAS_STREAMOUTPUT ) {
        err = g_pNavigatorFuncs->destroystream(instance, stream, reason);
    }
    else {
        err = NPERR_INCOMPATIBLE_VERSION_ERROR;
    }
    return err;
}

/* Provides a text status message in the Netscape client user interface
 */
void NPN_Status(NPP instance, const char *message)
{
    g_pNavigatorFuncs->status(instance, message);
}

/* returns the user agent string of Navigator, which contains version info
 */
const char* NPN_UserAgent(NPP instance)
{
    return g_pNavigatorFuncs->uagent(instance);
}

/* allocates memory from the Navigator's memory space.  Necessary so that
 * saved instance data may be freed by Navigator when exiting.
 */
void *NPN_MemAlloc(uint32_t size)
{
    return g_pNavigatorFuncs->memalloc(size);
}

/* reciprocal of MemAlloc() above
 */
void NPN_MemFree(void* ptr)
{
    g_pNavigatorFuncs->memfree(ptr);
}

#ifdef OJI
/* private function to Netscape.  do not use!
 */
void NPN_ReloadPlugins(NPBool reloadPages)
{
    g_pNavigatorFuncs->reloadplugins(reloadPages);
}

JRIEnv* NPN_GetJavaEnv(void)
{
    return g_pNavigatorFuncs->getJavaEnv();
}

jref NPN_GetJavaPeer(NPP instance)
{
    return g_pNavigatorFuncs->getJavaPeer(instance);
}
#endif
