/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _NPAPI_H_
#include "npapi.h"
#endif
#ifndef _NPUPP_H_
#include "npupp.h"
#endif

#include "nsDefaultPlugin.h"

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
NPError WINAPI NP_EXPORT
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

	g_pluginFuncs		  = pFuncs;

    return NPERR_NO_ERROR;
}

//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\.
////\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//.
// NP_Initialize
//
//	called immediately after the plugin DLL is loaded
//
NPError WINAPI NP_EXPORT 
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
NPError WINAPI NP_EXPORT 
NP_Shutdown()
{
    NPP_Shutdown();
    g_pNavigatorFuncs = NULL;
    return NPERR_NO_ERROR;
}

char * NP_GetMIMEDescription()
{
  static char mimetype[] = NS_PLUGIN_DEFAULT_MIME_DESCRIPTION;
  return mimetype;
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

NPError NPN_PostURLNotify(NPP instance, const char* url, const char* window, uint32 len, const char* buf, NPBool file, void* notifyData)
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


NPError NPN_PostURL(NPP instance, const char* url, const char* window, uint32 len, const char* buf, NPBool file)
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
   by Netscape in the current window.
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
int32 NPN_Write(NPP instance, NPStream *stream,
                int32 len, void *buffer)
{
	int navMinorVersion = g_pNavigatorFuncs->version & 0xFF;
	int32 result;

	if( navMinorVersion >= NPVERS_HAS_STREAMOUTPUT ) {
		result = g_pNavigatorFuncs->write(instance, stream, len, buffer);
	}
	else {
		result = -1;
	}
	return result;
}

/* Closes a stream object.  
reason indicates why the stream was closed.
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
   saved instance data may be freed by Navigator when exiting.
*/


void* NPN_MemAlloc(uint32 size)
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

