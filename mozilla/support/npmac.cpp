/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//
// npmac.cpp
//
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

#include <Processes.h>
#include <Gestalt.h>
#include <CodeFragments.h>
#include <Timer.h>
#include <Resources.h>
#include <ToolUtils.h>

#define XP_MAC 1
#define NDEBUG 1

//
// A4Stuff.h contains the definition of EnterCodeResource and 
// EnterCodeResource, used for setting up the code resource¹s
// globals for 68K (analagous to the function SetCurrentA5
// defined by the toolbox).
//
// A4Stuff does not exist as of CW 7. Define them to nothing.
//

#if defined(XP_MACOSX) || (defined(__MWERKS__) && (__MWERKS__ >= 0x2400))
	#define EnterCodeResource()
	#define ExitCodeResource()
#else
    #include <A4Stuff.h>
#endif

#include "jri.h"
#include "npapi.h"

//
// The Mixed Mode procInfos defined in npupp.h assume Think C-
// style calling conventions.  These conventions are used by
// Metrowerks with the exception of pointer return types, which
// in Metrowerks 68K are returned in A0, instead of the standard
// D0. Thus, since NPN_MemAlloc and NPN_UserAgent return pointers,
// Mixed Mode will return the values to a 68K plugin in D0, but 
// a 68K plugin compiled by Metrowerks will expect the result in
// A0.  The following pragma forces Metrowerks to use D0 instead.
//
#ifdef __MWERKS__
#ifndef powerc
#pragma pointers_in_D0
#endif
#endif

#include "npupp.h"

#ifdef __MWERKS__
#ifndef powerc
#pragma pointers_in_A0
#endif
#endif

// The following fix for static initializers (which fixes a preious
// incompatibility with some parts of PowerPlant, was submitted by 
// Jan Ulbrich.
#ifdef __MWERKS__
	#ifdef __cplusplus
	extern "C" {
	#endif
		#ifndef powerc
			extern void	__InitCode__(void);
		#else
			extern void __sinit(void);
			#define __InitCode__ __sinit
		#endif
		extern void	__destroy_global_chain(void);
	#ifdef __cplusplus
	}
	#endif // __cplusplus
#endif // __MWERKS__

//
// Define PLUGIN_TRACE to 1 to have the wrapper functions emit
// DebugStr messages whenever they are called.
//
#define PLUGIN_TRACE 0

#if PLUGIN_TRACE
#define PLUGINDEBUGSTR(msg)		::DebugStr(msg)
#else
#define PLUGINDEBUGSTR
#endif






//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//
// Globals
//
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

#if !OPAQUE_TOOLBOX_STRUCTS || !TARGET_API_MAC_CARBON
QDGlobals*		gQDPtr;				// Pointer to Netscape¹s QuickDraw globals
#endif
short			gResFile;			// Refnum of the plugin¹s resource file
NPNetscapeFuncs	gNetscapeFuncs;		// Function table for procs in Netscape called by plugin


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//
// Wrapper functions for all calls from the plugin to Netscape.
// These functions let the plugin developer just call the APIs
// as documented and defined in npapi.h, without needing to know
// about the function table and call macros in npupp.h.
//
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::


void NPN_Version(int* plugin_major, int* plugin_minor, int* netscape_major, int* netscape_minor)
{
	*plugin_major = NP_VERSION_MAJOR;
	*plugin_minor = NP_VERSION_MINOR;
	*netscape_major = gNetscapeFuncs.version >> 8;		// Major version is in high byte
	*netscape_minor = gNetscapeFuncs.version & 0xFF;	// Minor version is in low byte
}

NPError NPN_GetURLNotify(NPP instance, const char* url, const char* window, void* notifyData)
{
	int navMinorVers = gNetscapeFuncs.version & 0xFF;
	NPError err;
	
	if( navMinorVers >= NPVERS_HAS_NOTIFICATION )
	{
		err = CallNPN_GetURLNotifyProc(gNetscapeFuncs.geturlnotify, instance, url, window, notifyData);
	}
	else
	{
		err = NPERR_INCOMPATIBLE_VERSION_ERROR;
	}
	return err;
}

NPError NPN_GetURL(NPP instance, const char* url, const char* window)
{
	return CallNPN_GetURLProc(gNetscapeFuncs.geturl, instance, url, window);
}

NPError NPN_PostURLNotify(NPP instance, const char* url, const char* window, uint32 len, const char* buf, NPBool file, void* notifyData)
{
	int navMinorVers = gNetscapeFuncs.version & 0xFF;
	NPError err;
	
	if( navMinorVers >= NPVERS_HAS_NOTIFICATION )
	{
		err = CallNPN_PostURLNotifyProc(gNetscapeFuncs.posturlnotify, instance, url, 
														window, len, buf, file, notifyData);
	}
	else
	{
		err = NPERR_INCOMPATIBLE_VERSION_ERROR;
	}
	return err;
}

NPError NPN_PostURL(NPP instance, const char* url, const char* window, uint32 len, const char* buf, NPBool file)
{
	return CallNPN_PostURLProc(gNetscapeFuncs.posturl, instance, url, window, len, buf, file);
}

NPError NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
	return CallNPN_RequestReadProc(gNetscapeFuncs.requestread, stream, rangeList);
}

NPError NPN_NewStream(NPP instance, NPMIMEType type, const char* window, NPStream** stream)
{
	int navMinorVers = gNetscapeFuncs.version & 0xFF;
	NPError err;
	
	if( navMinorVers >= NPVERS_HAS_STREAMOUTPUT )
	{
		err = CallNPN_NewStreamProc(gNetscapeFuncs.newstream, instance, type, window, stream);
	}
	else
	{
		err = NPERR_INCOMPATIBLE_VERSION_ERROR;
	}
	return err;
}

int32 NPN_Write(NPP instance, NPStream* stream, int32 len, void* buffer)
{
	int navMinorVers = gNetscapeFuncs.version & 0xFF;
	NPError err;
	
	if( navMinorVers >= NPVERS_HAS_STREAMOUTPUT )
	{
		err = CallNPN_WriteProc(gNetscapeFuncs.write, instance, stream, len, buffer);
	}
	else
	{
		err = NPERR_INCOMPATIBLE_VERSION_ERROR;
	}
	return err;
}

NPError	NPN_DestroyStream(NPP instance, NPStream* stream, NPError reason)
{
	int navMinorVers = gNetscapeFuncs.version & 0xFF;
	NPError err;
	
	if( navMinorVers >= NPVERS_HAS_STREAMOUTPUT )
	{
		err = CallNPN_DestroyStreamProc(gNetscapeFuncs.destroystream, instance, stream, reason);
	}
	else
	{
		err = NPERR_INCOMPATIBLE_VERSION_ERROR;
	}
	return err;
}

void NPN_Status(NPP instance, const char* message)
{
	CallNPN_StatusProc(gNetscapeFuncs.status, instance, message);
}

const char* NPN_UserAgent(NPP instance)
{
	return CallNPN_UserAgentProc(gNetscapeFuncs.uagent, instance);
}

#define DEBUG_MEMORY 0

void* NPN_MemAlloc(uint32 size)
{
#if DEBUG_MEMORY
	return (void*) NewPtrClear(size);
#else
	return CallNPN_MemAllocProc(gNetscapeFuncs.memalloc, size);
#endif
}

void NPN_MemFree(void* ptr)
{
#if DEBUG_MEMORY
	DisposePtr(Ptr(ptr));
#else
	CallNPN_MemFreeProc(gNetscapeFuncs.memfree, ptr);
#endif
}

uint32 NPN_MemFlush(uint32 size)
{
	return CallNPN_MemFlushProc(gNetscapeFuncs.memflush, size);
}

void NPN_ReloadPlugins(NPBool reloadPages)
{
	CallNPN_ReloadPluginsProc(gNetscapeFuncs.reloadplugins, reloadPages);
}


JRIEnv* NPN_GetJavaEnv(void)
{
	return CallNPN_GetJavaEnvProc( gNetscapeFuncs.getJavaEnv );
}

jref  NPN_GetJavaPeer(NPP instance)
{
	return CallNPN_GetJavaPeerProc( gNetscapeFuncs.getJavaPeer, instance );
}

NPError NPN_GetValue(NPP instance, NPNVariable variable, void *value)
{
	return CallNPN_GetValueProc( gNetscapeFuncs.getvalue, instance, variable, value);
}

NPError NPN_SetValue(NPP instance, NPPVariable variable, void *value)
{
	return CallNPN_SetValueProc( gNetscapeFuncs.setvalue, instance, variable, value);
}

void NPN_InvalidateRect(NPP instance, NPRect *rect)
{
	CallNPN_InvalidateRectProc( gNetscapeFuncs.invalidaterect, instance, rect);
}

void NPN_InvalidateRegion(NPP instance, NPRegion region)
{
	CallNPN_InvalidateRegionProc( gNetscapeFuncs.invalidateregion, instance, region);
}

void NPN_ForceRedraw(NPP instance)
{
	CallNPN_ForceRedrawProc( gNetscapeFuncs.forceredraw, instance);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//
// Wrapper functions for all calls from Netscape to the plugin.
// These functions let the plugin developer just create the APIs
// as documented and defined in npapi.h, without needing to 
// install those functions in the function table or worry about
// setting up globals for 68K plugins.
//
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

NPError 	Private_Initialize(void);
void 		Private_Shutdown(void);
NPError		Private_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved);
NPError 	Private_Destroy(NPP instance, NPSavedData** save);
NPError		Private_SetWindow(NPP instance, NPWindow* window);
NPError		Private_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype);
NPError		Private_DestroyStream(NPP instance, NPStream* stream, NPError reason);
int32		Private_WriteReady(NPP instance, NPStream* stream);
int32		Private_Write(NPP instance, NPStream* stream, int32 offset, int32 len, void* buffer);
void		Private_StreamAsFile(NPP instance, NPStream* stream, const char* fname);
void		Private_Print(NPP instance, NPPrint* platformPrint);
int16 		Private_HandleEvent(NPP instance, void* event);
void        Private_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData);
jref		Private_GetJavaClass(void);


NPError Private_Initialize(void)
{
	NPError err;
	EnterCodeResource();
	PLUGINDEBUGSTR("\pInitialize;g;");
	err = NPP_Initialize();
	ExitCodeResource();
	return err;
}

void Private_Shutdown(void)
{
	EnterCodeResource();
	PLUGINDEBUGSTR("\pShutdown;g;");
	NPP_Shutdown();

#ifndef XP_MACOSX
	__destroy_global_chain();
#endif

	ExitCodeResource();
}


NPError	Private_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved)
{
	EnterCodeResource();
	NPError ret = NPP_New(pluginType, instance, mode, argc, argn, argv, saved);
	PLUGINDEBUGSTR("\pNew;g;");
	ExitCodeResource();
	return ret;	
}

NPError Private_Destroy(NPP instance, NPSavedData** save)
{
	NPError err;
	EnterCodeResource();
	PLUGINDEBUGSTR("\pDestroy;g;");
	err = NPP_Destroy(instance, save);
	ExitCodeResource();
	return err;
}

NPError Private_SetWindow(NPP instance, NPWindow* window)
{
	NPError err;
	EnterCodeResource();
	PLUGINDEBUGSTR("\pSetWindow;g;");
	err = NPP_SetWindow(instance, window);
	ExitCodeResource();
	return err;
}

NPError Private_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype)
{
	NPError err;
	EnterCodeResource();
	PLUGINDEBUGSTR("\pNewStream;g;");
	err = NPP_NewStream(instance, type, stream, seekable, stype);
	ExitCodeResource();
	return err;
}

int32 Private_WriteReady(NPP instance, NPStream* stream)
{
	int32 result;
	EnterCodeResource();
	PLUGINDEBUGSTR("\pWriteReady;g;");
	result = NPP_WriteReady(instance, stream);
	ExitCodeResource();
	return result;
}

int32 Private_Write(NPP instance, NPStream* stream, int32 offset, int32 len, void* buffer)
{
	int32 result;
	EnterCodeResource();
	PLUGINDEBUGSTR("\pWrite;g;");
	result = NPP_Write(instance, stream, offset, len, buffer);
	ExitCodeResource();
	return result;
}

void Private_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
	EnterCodeResource();
	PLUGINDEBUGSTR("\pStreamAsFile;g;");
	NPP_StreamAsFile(instance, stream, fname);
	ExitCodeResource();
}


NPError Private_DestroyStream(NPP instance, NPStream* stream, NPError reason)
{
	NPError err;
	EnterCodeResource();
	PLUGINDEBUGSTR("\pDestroyStream;g;");
	err = NPP_DestroyStream(instance, stream, reason);
	ExitCodeResource();
	return err;
}

int16 Private_HandleEvent(NPP instance, void* event)
{
	int16 result;
	EnterCodeResource();
	PLUGINDEBUGSTR("\pHandleEvent;g;");
	result = NPP_HandleEvent(instance, event);
	ExitCodeResource();
	return result;
}

void Private_Print(NPP instance, NPPrint* platformPrint)
{
	EnterCodeResource();
	PLUGINDEBUGSTR("\pPrint;g;");
	NPP_Print(instance, platformPrint);
	ExitCodeResource();
}

void Private_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData)
{
	EnterCodeResource();
	PLUGINDEBUGSTR("\pURLNotify;g;");
	NPP_URLNotify(instance, url, reason, notifyData);
	ExitCodeResource();
}


jref Private_GetJavaClass(void)
{
	EnterCodeResource();
	PLUGINDEBUGSTR("\pGetJavaClass;g;");

    jref clazz = NPP_GetJavaClass();
    ExitCodeResource();
    if (clazz)
    {
		JRIEnv* env = NPN_GetJavaEnv();
		return (jref)JRI_NewGlobalRef(env, clazz);
    }
    return NULL;
}


void SetUpQD(void);

void SetUpQD(void)
{
#if !OPAQUE_TOOLBOX_STRUCTS || !TARGET_API_MAC_CARBON
	ProcessSerialNumber PSN;
	FSSpec				myFSSpec;
	Str63				name;
	ProcessInfoRec		infoRec;
	OSErr				result = noErr;
	CFragConnectionID 	connID;
	Str255 				errName;
	
	//
	// Memorize the plugin¹s resource file 
	// refnum for later use.
	//
	gResFile = CurResFile();
	
	//
	// Ask the system if CFM is available.
	//
	long response;
	OSErr err = Gestalt(gestaltCFMAttr, &response);
	Boolean hasCFM = BitTst(&response, 31-gestaltCFMPresent);
			
	if (hasCFM)
	{
		//
		// GetProcessInformation takes a process serial number and 
		// will give us back the name and FSSpec of the application.
		// See the Process Manager in IM.
		//
		infoRec.processInfoLength = sizeof(ProcessInfoRec);
		infoRec.processName = name;
		infoRec.processAppSpec = &myFSSpec;
		
		PSN.highLongOfPSN = 0;
		PSN.lowLongOfPSN = kCurrentProcess;
		
		result = GetProcessInformation(&PSN, &infoRec);
		if (result != noErr)
			PLUGINDEBUGSTR("\pFailed in GetProcessInformation");
		}
	else
		//
		// If no CFM installed, assume it must be a 68K app.
		//
		result = -1;		
		
	if (result == noErr)
	{
		//
		// Now that we know the app name and FSSpec, we can call GetDiskFragment
		// to get a connID to use in a subsequent call to FindSymbol (it will also
		// return the address of ³main² in app, which we ignore).  If GetDiskFragment 
		// returns an error, we assume the app must be 68K.
		//
		Ptr mainAddr; 	
		result =  GetDiskFragment(infoRec.processAppSpec, 0L, 0L, infoRec.processName,
								  kReferenceCFrag, &connID, (Ptr*)&mainAddr, errName);
	}

	if (result == noErr) 
	{
		//
		// The app is a PPC code fragment, so call FindSymbol
		// to get the exported ³qd² symbol so we can access its
		// QuickDraw globals.
		//
		CFragSymbolClass symClass;
		result = FindSymbol(connID, "\pqd", (Ptr*)&gQDPtr, &symClass);
		if (result != noErr)
			PLUGINDEBUGSTR("\pFailed in FindSymbol qd");
	}
	else
	{
		//
		// The app is 68K, so use its A5 to compute the address
		// of its QuickDraw globals.
		//
		gQDPtr = (QDGlobals*)(*((long*)SetCurrentA5()) - (sizeof(QDGlobals) - sizeof(GrafPtr)));
	}
#endif
}



#if TARGET_RT_MAC_CFM && !TARGET_API_MAC_CARBON
NPError
#else
int
#endif
main(NPNetscapeFuncs* nsTable, NPPluginFuncs* pluginFuncs, NPP_ShutdownUPP* unloadUpp);

#pragma export on

#if TARGET_RT_MAC_CFM && !TARGET_API_MAC_CARBON
RoutineDescriptor mainRD = BUILD_ROUTINE_DESCRIPTOR(uppNPP_MainEntryProcInfo, main);
#endif

#pragma export off
 

#if TARGET_RT_MAC_CFM && !TARGET_API_MAC_CARBON
NPError
#else
int
#endif
main(NPNetscapeFuncs* nsTable, NPPluginFuncs* pluginFuncs, NPP_ShutdownUPP* unloadUpp)
{
	EnterCodeResource();
	PLUGINDEBUGSTR("\pmain");

	NPError err = NPERR_NO_ERROR;
	
	//
	// Ensure that everything Netscape passed us is valid!
	//
	if ((nsTable == NULL) || (pluginFuncs == NULL) || (unloadUpp == NULL))
		err = NPERR_INVALID_FUNCTABLE_ERROR;
	
	//
	// Check the ³major² version passed in Netscape¹s function table.
	// We won¹t load if the major version is newer than what we expect.
	// Also check that the function tables passed in are big enough for
	// all the functions we need (they could be bigger, if Netscape added
	// new APIs, but that¹s OK with us -- we¹ll just ignore them).
	//
	if (err == NPERR_NO_ERROR)
	{
		if ((nsTable->version >> 8) > NP_VERSION_MAJOR)		// Major version is in high byte
			err = NPERR_INCOMPATIBLE_VERSION_ERROR;
//		if (nsTable->size < sizeof(NPNetscapeFuncs))
//			err = NPERR_INVALID_FUNCTABLE_ERROR;
//		if (pluginFuncs->size < sizeof(NPPluginFuncs))		
//			err = NPERR_INVALID_FUNCTABLE_ERROR;
	}
		
	
	if (err == NPERR_NO_ERROR)
	{
		//
		// Copy all the fields of Netscape¹s function table into our
		// copy so we can call back into Netscape later.  Note that
		// we need to copy the fields one by one, rather than assigning
		// the whole structure, because the Netscape function table
		// could actually be bigger than what we expect.
		//
		
		int navMinorVers = nsTable->version & 0xFF;

		gNetscapeFuncs.version = nsTable->version;
		gNetscapeFuncs.size = nsTable->size;
		gNetscapeFuncs.posturl = nsTable->posturl;
		gNetscapeFuncs.geturl = nsTable->geturl;
		gNetscapeFuncs.requestread = nsTable->requestread;
		gNetscapeFuncs.newstream = nsTable->newstream;
		gNetscapeFuncs.write = nsTable->write;
		gNetscapeFuncs.destroystream = nsTable->destroystream;
		gNetscapeFuncs.status = nsTable->status;
		gNetscapeFuncs.uagent = nsTable->uagent;
		gNetscapeFuncs.memalloc = nsTable->memalloc;
		gNetscapeFuncs.memfree = nsTable->memfree;
		gNetscapeFuncs.memflush = nsTable->memflush;
		gNetscapeFuncs.reloadplugins = nsTable->reloadplugins;
		if( navMinorVers >= NPVERS_HAS_LIVECONNECT )
		{
			gNetscapeFuncs.getJavaEnv = nsTable->getJavaEnv;
			gNetscapeFuncs.getJavaPeer = nsTable->getJavaPeer;
		}
		if( navMinorVers >= NPVERS_HAS_NOTIFICATION )
		{	
			gNetscapeFuncs.geturlnotify = nsTable->geturlnotify;
			gNetscapeFuncs.posturlnotify = nsTable->posturlnotify;
		}
		gNetscapeFuncs.getvalue = nsTable->getvalue;
		gNetscapeFuncs.setvalue = nsTable->setvalue;
		gNetscapeFuncs.invalidaterect = nsTable->invalidaterect;
		gNetscapeFuncs.invalidateregion = nsTable->invalidateregion;
		gNetscapeFuncs.forceredraw = nsTable->forceredraw;

		// defer static constructors until the global functions are initialized.
#ifndef XP_MACOSX
		__InitCode__();
#endif
		
		//
		// Set up the plugin function table that Netscape will use to
		// call us.  Netscape needs to know about our version and size
		// and have a UniversalProcPointer for every function we implement.
		//
		pluginFuncs->version = (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;
		pluginFuncs->size = sizeof(NPPluginFuncs);
		pluginFuncs->newp = NewNPP_NewProc(Private_New);
		pluginFuncs->destroy = NewNPP_DestroyProc(Private_Destroy);
		pluginFuncs->setwindow = NewNPP_SetWindowProc(Private_SetWindow);
		pluginFuncs->newstream = NewNPP_NewStreamProc(Private_NewStream);
		pluginFuncs->destroystream = NewNPP_DestroyStreamProc(Private_DestroyStream);
		pluginFuncs->asfile = NewNPP_StreamAsFileProc(Private_StreamAsFile);
		pluginFuncs->writeready = NewNPP_WriteReadyProc(Private_WriteReady);
		pluginFuncs->write = NewNPP_WriteProc(Private_Write);
		pluginFuncs->print = NewNPP_PrintProc(Private_Print);
		pluginFuncs->event = NewNPP_HandleEventProc(Private_HandleEvent);	
		if( navMinorVers >= NPVERS_HAS_NOTIFICATION )
		{	
			pluginFuncs->urlnotify = NewNPP_URLNotifyProc(Private_URLNotify);			
		}
		if( navMinorVers >= NPVERS_HAS_LIVECONNECT )
		{
			pluginFuncs->javaClass	= (JRIGlobalRef) Private_GetJavaClass();
		}
		*unloadUpp = NewNPP_ShutdownProc(Private_Shutdown);
		SetUpQD();
		err = Private_Initialize();
	}
	
	ExitCodeResource();
	return err;
}
