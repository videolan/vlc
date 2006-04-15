//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//
// npmac.cpp
//
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

#include <string.h>

#include <Processes.h>
#include <Gestalt.h>
#include <CodeFragments.h>
#include <Timer.h>
#include <Resources.h>
#include <ToolUtils.h>

#define XP_MACOSX 1
#undef TARGET_RT_MAC_CFM

//
// A4Stuff.h contains the definition of EnterCodeResource and 
// EnterCodeResource, used for setting up the code resource’s
// globals for 68K (analagous to the function SetCurrentA5
// defined by the toolbox).
//
// A4Stuff does not exist as of CW 7. Define them to nothing.
//

#if (defined(__MWERKS__) && (__MWERKS__ >= 0x2400)) || defined(__GNUC__)
    #define EnterCodeResource()
    #define ExitCodeResource()
#else
    #include <A4Stuff.h>
#endif

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

#ifdef XP_UNIX
#undef XP_UNIX
#endif

#include "npupp.h"

#ifdef __MWERKS__
#ifndef powerc
#pragma pointers_in_A0
#endif
#endif

// The following fix for static initializers (which fixes a previous
// incompatibility with some parts of PowerPlant, was submitted by 
// Jan Ulbrich.
#ifdef __MWERKS__
    #ifdef __cplusplus
    extern "C" {
    #endif
        #ifndef powerc
            extern void __InitCode__(void);
        #else
            extern void __sinit(void);
            #define __InitCode__ __sinit
        #endif
        extern void __destroy_global_chain(void);
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
#define PLUGINDEBUGSTR(msg)     ::DebugStr(msg)
#else
#define PLUGINDEBUGSTR(msg) {}
#endif


#ifdef XP_MACOSX && !TARGET_RT_MAC_CFM

// glue for mapping outgoing Macho function pointers to TVectors
struct TFPtoTVGlue{
    void* glue[2];
};

static struct {
    TFPtoTVGlue     newp;
    TFPtoTVGlue     destroy;
    TFPtoTVGlue     setwindow;
    TFPtoTVGlue     newstream;
    TFPtoTVGlue     destroystream;
    TFPtoTVGlue     asfile;
    TFPtoTVGlue     writeready;
    TFPtoTVGlue     write;
    TFPtoTVGlue     print;
    TFPtoTVGlue     event;
    TFPtoTVGlue     urlnotify;
    TFPtoTVGlue     getvalue;
    TFPtoTVGlue     setvalue;

    TFPtoTVGlue     shutdown;
} gPluginFuncsGlueTable;

static inline void* SetupFPtoTVGlue(TFPtoTVGlue* functionGlue, void* fp)
{
    functionGlue->glue[0] = fp;
    functionGlue->glue[1] = 0;
    return functionGlue;
}

#define PLUGIN_TO_HOST_GLUE(name, fp) (SetupFPtoTVGlue(&gPluginFuncsGlueTable.name, (void*)fp))

// glue for mapping netscape TVectors to Macho function pointers
struct TTVtoFPGlue {
    uint32 glue[6];
};

static struct {
    TTVtoFPGlue             geturl;
    TTVtoFPGlue             posturl;
    TTVtoFPGlue             requestread;
    TTVtoFPGlue             newstream;
    TTVtoFPGlue             write;
    TTVtoFPGlue             destroystream;
    TTVtoFPGlue             status;
    TTVtoFPGlue             uagent;
    TTVtoFPGlue             memalloc;
    TTVtoFPGlue             memfree;
    TTVtoFPGlue             memflush;
    TTVtoFPGlue             reloadplugins;
    TTVtoFPGlue             getJavaEnv;
    TTVtoFPGlue             getJavaPeer;
    TTVtoFPGlue             geturlnotify;
    TTVtoFPGlue             posturlnotify;
    TTVtoFPGlue             getvalue;
    TTVtoFPGlue             setvalue;
    TTVtoFPGlue             invalidaterect;
    TTVtoFPGlue             invalidateregion;
    TTVtoFPGlue             forceredraw;
    // NPRuntime support
    TTVtoFPGlue             getstringidentifier;
    TTVtoFPGlue             getstringidentifiers;
    TTVtoFPGlue             getintidentifier;
    TTVtoFPGlue             identifierisstring;
    TTVtoFPGlue             utf8fromidentifier;
    TTVtoFPGlue             intfromidentifier;
    TTVtoFPGlue             createobject;
    TTVtoFPGlue             retainobject;
    TTVtoFPGlue             releaseobject;
    TTVtoFPGlue             invoke;
    TTVtoFPGlue             invokeDefault;
    TTVtoFPGlue             evaluate;
    TTVtoFPGlue             getproperty;
    TTVtoFPGlue             setproperty;
    TTVtoFPGlue             removeproperty;
    TTVtoFPGlue             hasproperty;
    TTVtoFPGlue             hasmethod;
    TTVtoFPGlue             releasevariantvalue;
    TTVtoFPGlue             setexception;
} gNetscapeFuncsGlueTable;

static void* SetupTVtoFPGlue(TTVtoFPGlue* functionGlue, void* tvp)
{
    static const TTVtoFPGlue glueTemplate = { 0x3D800000, 0x618C0000, 0x800C0000, 0x804C0004, 0x7C0903A6, 0x4E800420 };

    memcpy(functionGlue, &glueTemplate, sizeof(TTVtoFPGlue));
    functionGlue->glue[0] |= ((UInt32)tvp >> 16);
    functionGlue->glue[1] |= ((UInt32)tvp & 0xFFFF);
    ::MakeDataExecutable(functionGlue, sizeof(TTVtoFPGlue));
    return functionGlue;
}

#define HOST_TO_PLUGIN_GLUE(name, fp) (SetupTVtoFPGlue(&gNetscapeFuncsGlueTable.name, (void*)fp))

#else

#define PLUGIN_TO_HOST_GLUE(name, fp) (fp)
#define HOST_TO_PLUGIN_GLUE(name, fp) (fp)

#endif /* XP_MACOSX */


#pragma mark -


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//
// Globals
//
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

#if !TARGET_API_MAC_CARBON
QDGlobals*      gQDPtr;             // Pointer to Netscape’s QuickDraw globals
#endif
short           gResFile;           // Refnum of the plugin’s resource file
NPNetscapeFuncs    gNetscapeFuncs;      // Function table for procs in Netscape called by plugin

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
    *netscape_major = gNetscapeFuncs.version >> 8;      // Major version is in high byte
    *netscape_minor = gNetscapeFuncs.version & 0xFF;    // Minor version is in low byte
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

NPError    NPN_DestroyStream(NPP instance, NPStream* stream, NPError reason)
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

void* NPN_MemAlloc(uint32 size)
{
    return CallNPN_MemAllocProc(gNetscapeFuncs.memalloc, size);
}

void NPN_MemFree(void* ptr)
{
    CallNPN_MemFreeProc(gNetscapeFuncs.memfree, ptr);
}

uint32 NPN_MemFlush(uint32 size)
{
    return CallNPN_MemFlushProc(gNetscapeFuncs.memflush, size);
}

void NPN_ReloadPlugins(NPBool reloadPages)
{
    CallNPN_ReloadPluginsProc(gNetscapeFuncs.reloadplugins, reloadPages);
}

#ifdef OJI
JRIEnv* NPN_GetJavaEnv(void)
{
    return CallNPN_GetJavaEnvProc( gNetscapeFuncs.getJavaEnv );
}

jobject  NPN_GetJavaPeer(NPP instance)
{
    return CallNPN_GetJavaPeerProc( gNetscapeFuncs.getJavaPeer, instance );
}
#endif

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

NPIdentifier NPN_GetStringIdentifier(const NPUTF8 *name)
{
    int navMinorVers = gNetscapeFuncs.version & 0xFF;
    if( navMinorVers >= 14 )
    {   
	return CallNPN_GetStringIdentifierProc( gNetscapeFuncs.getstringidentifier, name);
    }
    return NULL;
}

void NPN_GetStringIdentifiers(const NPUTF8 **names, int32_t nameCount, NPIdentifier *identifiers)
{
    int navMinorVers = gNetscapeFuncs.version & 0xFF;
    if( navMinorVers >= 14 )
    {   
	CallNPN_GetStringIdentifiersProc( gNetscapeFuncs.getstringidentifiers, names, nameCount, identifiers);
    }
}

NPIdentifier NPN_GetIntIdentifier(int32_t intid)
{
    int navMinorVers = gNetscapeFuncs.version & 0xFF;
    if( navMinorVers >= 14 )
    {   
	return CallNPN_GetIntIdentifierProc( gNetscapeFuncs.getintidentifier, intid);
    }
    return NULL;
}

bool NPN_IdentifierIsString(NPIdentifier identifier)
{
    int navMinorVers = gNetscapeFuncs.version & 0xFF;
    if( navMinorVers >= 14 )
    {   
	return CallNPN_IdentifierIsStringProc( gNetscapeFuncs.identifierisstring, identifier);
    }
    return false;
}

NPUTF8 *NPN_UTF8FromIdentifier(NPIdentifier identifier)
{
    int navMinorVers = gNetscapeFuncs.version & 0xFF;
    if( navMinorVers >= 14 )
    {   
	return CallNPN_UTF8FromIdentifierProc( gNetscapeFuncs.utf8fromidentifier, identifier);
    }
    return NULL;
}

int32_t NPN_IntFromIdentifier(NPIdentifier identifier)
{
    int navMinorVers = gNetscapeFuncs.version & 0xFF;
    if( navMinorVers >= 14 )
    {   
	return CallNPN_IntFromIdentifierProc( gNetscapeFuncs.intfromidentifier, identifier);
    }
    return 0;
}

NPObject *NPN_CreateObject(NPP instance, NPClass *aClass)
{
    int navMinorVers = gNetscapeFuncs.version & 0xFF;
    if( navMinorVers >= 14 )
    {   
	return CallNPN_CreateObjectProc( gNetscapeFuncs.createobject, instance, aClass);
    }
    return NULL;
}

NPObject *NPN_RetainObject(NPObject *npobj)
{
    int navMinorVers = gNetscapeFuncs.version & 0xFF;
    if( navMinorVers >= 14 )
    {   
	return CallNPN_RetainObjectProc( gNetscapeFuncs.retainobject, npobj);
    }
    return NULL;
}

void NPN_ReleaseObject(NPObject *npobj)
{
    int navMinorVers = gNetscapeFuncs.version & 0xFF;
    if( navMinorVers >= 14 )
    {   
	CallNPN_ReleaseObjectProc( gNetscapeFuncs.releaseobject, npobj);
    }
}

bool NPN_Invoke(NPP instance, NPObject *npobj, NPIdentifier methodName, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    int navMinorVers = gNetscapeFuncs.version & 0xFF;
    if( navMinorVers >= 14 )
    {   
	return CallNPN_InvokeProc( gNetscapeFuncs.invoke, instance, npobj, methodName, args, argCount, result);
    }
    return false;
}

bool NPN_InvokeDefault(NPP instance, NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    int navMinorVers = gNetscapeFuncs.version & 0xFF;
    if( navMinorVers >= 14 )
    {   
	return CallNPN_InvokeDefaultProc( gNetscapeFuncs.invokeDefault, instance, npobj, args, argCount, result);
    }
    return false;
}

bool NPN_Evaluate(NPP instance, NPObject *npobj, NPString *script, NPVariant *result)
{
    int navMinorVers = gNetscapeFuncs.version & 0xFF;
    if( navMinorVers >= 14 )
    {   
	return CallNPN_EvaluateProc( gNetscapeFuncs.evaluate, instance, npobj, script, result);
    }
    return false;
}

bool NPN_GetProperty(NPP instance, NPObject *npobj, NPIdentifier propertyName, NPVariant *result)
{
    int navMinorVers = gNetscapeFuncs.version & 0xFF;
    if( navMinorVers >= 14 )
    {   
	return CallNPN_GetPropertyProc( gNetscapeFuncs.getproperty, instance, npobj, propertyName, result);
    }
    return false;
}

bool NPN_SetProperty(NPP instance, NPObject *npobj, NPIdentifier propertyName, const NPVariant *value)
{
    int navMinorVers = gNetscapeFuncs.version & 0xFF;
    if( navMinorVers >= 14 )
    {   
	return CallNPN_SetPropertyProc( gNetscapeFuncs.setproperty, instance, npobj, propertyName, value);
    }
    return false;
}

bool NPN_RemoveProperty(NPP instance, NPObject *npobj, NPIdentifier propertyName)
{
    int navMinorVers = gNetscapeFuncs.version & 0xFF;
    if( navMinorVers >= 14 )
    {   
	return CallNPN_RemovePropertyProc( gNetscapeFuncs.removeproperty, instance, npobj, propertyName);
    }
    return false;
}

bool NPN_HasProperty(NPP instance, NPObject *npobj, NPIdentifier propertyName)
{
    int navMinorVers = gNetscapeFuncs.version & 0xFF;
    if( navMinorVers >= 14 )
    {   
	return CallNPN_HasPropertyProc( gNetscapeFuncs.hasproperty, instance, npobj, propertyName);
    }
    return false;
}

bool NPN_HasMethod(NPP instance, NPObject *npobj, NPIdentifier methodName)
{
    int navMinorVers = gNetscapeFuncs.version & 0xFF;
    if( navMinorVers >= 14 )
    {   
	return CallNPN_HasMethodProc( gNetscapeFuncs.hasmethod, instance, npobj, methodName);
    }
    return false;
}

void NPN_ReleaseVariantValue(NPVariant *variant)
{
    int navMinorVers = gNetscapeFuncs.version & 0xFF;
    if( navMinorVers >= 14 )
    {   
	CallNPN_ReleaseVariantValueProc( gNetscapeFuncs.releasevariantvalue, variant);
    }
}

void NPN_SetException(NPObject *npobj, const NPUTF8 *message)
{
    int navMinorVers = gNetscapeFuncs.version & 0xFF;
    if( navMinorVers >= 14 )
    {   
	CallNPN_SetExceptionProc( gNetscapeFuncs.setexception, npobj, message);
    }
}

#pragma mark -

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//
// Wrapper functions for all calls from Netscape to the plugin.
// These functions let the plugin developer just create the APIs
// as documented and defined in npapi.h, without needing to 
// install those functions in the function table or worry about
// setting up globals for 68K plugins.
//
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

NPError     Private_Initialize(void);
void        Private_Shutdown(void);
NPError     Private_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved);
NPError     Private_Destroy(NPP instance, NPSavedData** save);
NPError     Private_SetWindow(NPP instance, NPWindow* window);
NPError     Private_GetValue( NPP instance, NPPVariable variable, void *value );
NPError     Private_SetValue( NPP instance, NPPVariable variable, void *value );
NPError     Private_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype);
NPError     Private_DestroyStream(NPP instance, NPStream* stream, NPError reason);
int32       Private_WriteReady(NPP instance, NPStream* stream);
int32       Private_Write(NPP instance, NPStream* stream, int32 offset, int32 len, void* buffer);
void        Private_StreamAsFile(NPP instance, NPStream* stream, const char* fname);
void        Private_Print(NPP instance, NPPrint* platformPrint);
int16       Private_HandleEvent(NPP instance, void* event);
void        Private_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData);
jobject     Private_GetJavaClass(void);


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

#ifdef __MWERKS__
    __destroy_global_chain();
#endif

    ExitCodeResource();
}


NPError    Private_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved)
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

NPError Private_GetValue( NPP instance, NPPVariable variable, void *value )
{
    NPError err;
    EnterCodeResource();
    PLUGINDEBUGSTR("\pGetValue;g;");
    err = NPP_GetValue( instance, variable, value);
    ExitCodeResource();
    return err;
}

NPError Private_SetValue( NPP instance, NPNVariable variable, void *value )
{
    NPError err;
    EnterCodeResource();
    PLUGINDEBUGSTR("\pSetValue;g;");
    err = NPERR_NO_ERROR; //NPP_SetValue( instance, variable, value);
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

#ifdef OJI
jobject Private_GetJavaClass(void)
{
    EnterCodeResource();
    PLUGINDEBUGSTR("\pGetJavaClass;g;");

    jobject clazz = NPP_GetJavaClass();
    ExitCodeResource();
    if (clazz)
    {
        JRIEnv* env = NPN_GetJavaEnv();
        return (jobject)JRI_NewGlobalRef(env, clazz);
    }
    return NULL;
}
#endif

void SetUpQD(void);
void SetUpQD(void)
{
#if !TARGET_API_MAC_CARBON
    ProcessSerialNumber PSN;
    FSSpec              myFSSpec;
    Str63               name;
    ProcessInfoRec      infoRec;
    OSErr               result = noErr;
    CFragConnectionID   connID;
    Str255              errName;
#endif    

    //
    // Memorize the plugin’s resource file 
    // refnum for later use.
    //
    gResFile = CurResFile();
    
#if !TARGET_API_MAC_CARBON
    //
    // Ask the system if CFM is available.
    //
    long response;
    OSErr err = Gestalt(gestaltCFMAttr, &response);
    Boolean hasCFM = BitTst(&response, 31-gestaltCFMPresent);

    ProcessInfoRec infoRec;
    if (hasCFM)
    {
        //
        // GetProcessInformation takes a process serial number and 
        // will give us back the name and FSSpec of the application.
        // See the Process Manager in IM.
        //
        Str63 name;
        FSSpec myFSSpec;
        infoRec.processInfoLength = sizeof(ProcessInfoRec);
        infoRec.processName = name;
        infoRec.processAppSpec = &myFSSpec;
        
        ProcessSerialNumber PSN;
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
        
    CFragConnectionID connID;
    if (result == noErr)
    {
        //
        // Now that we know the app name and FSSpec, we can call GetDiskFragment
        // to get a connID to use in a subsequent call to FindSymbol (it will also
        // return the address of “main” in app, which we ignore).  If GetDiskFragment 
        // returns an error, we assume the app must be 68K.
        //
        Ptr mainAddr;   
        Str255 errName;
        result =  GetDiskFragment(infoRec.processAppSpec, 0L, 0L, infoRec.processName,
                                  kLoadCFrag, &connID, (Ptr*)&mainAddr, errName);
    }

    if (result == noErr) 
    {
        //
        // The app is a PPC code fragment, so call FindSymbol
        // to get the exported “qd” symbol so we can access its
        // QuickDraw globals.
        //
        CFragSymbolClass symClass;
        result = FindSymbol(connID, "\pqd", (Ptr*)&gQDPtr, &symClass);
        if (result != noErr) {  // this fails if we are in NS 6
            gQDPtr = &qd;       // so we default to the standard QD globals
        }
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


#ifdef __GNUC__
// gcc requires that main have an 'int' return type
int main(NPNetscapeFuncs* nsTable, NPPluginFuncs* pluginFuncs, NPP_ShutdownUPP* unloadUpp);
#else
NPError main(NPNetscapeFuncs* nsTable, NPPluginFuncs* pluginFuncs, NPP_ShutdownUPP* unloadUpp);
#endif

#if !TARGET_API_MAC_CARBON
#pragma export on

#if TARGET_RT_MAC_CFM

RoutineDescriptor mainRD = BUILD_ROUTINE_DESCRIPTOR(uppNPP_MainEntryProcInfo, main);

#endif

#pragma export off
#endif /* !TARGET_API_MAC_CARBON */

#ifdef __GNUC__
DEFINE_API_C(int) main(NPNetscapeFuncs* nsTable, NPPluginFuncs* pluginFuncs, NPP_ShutdownUPP* unloadUpp)
#else
DEFINE_API_C(NPError) main(NPNetscapeFuncs* nsTable, NPPluginFuncs* pluginFuncs, NPP_ShutdownUPP* unloadUpp)
#endif
{
    EnterCodeResource();
    PLUGINDEBUGSTR("\pmain");

#ifdef __MWERKS__
    __InitCode__();
#endif

    NPError err = NPERR_NO_ERROR;
    
    //
    // Ensure that everything Netscape passed us is valid!
    //
    if ((nsTable == NULL) || (pluginFuncs == NULL) || (unloadUpp == NULL))
        err = NPERR_INVALID_FUNCTABLE_ERROR;
    
    //
    // Check the “major” version passed in Netscape’s function table.
    // We won’t load if the major version is newer than what we expect.
    // Also check that the function tables passed in are big enough for
    // all the functions we need (they could be bigger, if Netscape added
    // new APIs, but that’s OK with us -- we’ll just ignore them).
    //
    if (err == NPERR_NO_ERROR)
    {
        if ((nsTable->version >> 8) > NP_VERSION_MAJOR)     // Major version is in high byte
            err = NPERR_INCOMPATIBLE_VERSION_ERROR;
    }
        
    
    if (err == NPERR_NO_ERROR)
    {
        //
        // Copy all the fields of Netscape’s function table into our
        // copy so we can call back into Netscape later.  Note that
        // we need to copy the fields one by one, rather than assigning
        // the whole structure, because the Netscape function table
        // could actually be bigger than what we expect.
        //
        
        int navMinorVers = nsTable->version & 0xFF;

        gNetscapeFuncs.version          = nsTable->version;
        gNetscapeFuncs.size             = nsTable->size;
        gNetscapeFuncs.posturl          = (NPN_PostURLUPP)HOST_TO_PLUGIN_GLUE(posturl, nsTable->posturl);
        gNetscapeFuncs.geturl           = (NPN_GetURLUPP)HOST_TO_PLUGIN_GLUE(geturl, nsTable->geturl);
        gNetscapeFuncs.requestread      = (NPN_RequestReadUPP)HOST_TO_PLUGIN_GLUE(requestread, nsTable->requestread);
        gNetscapeFuncs.newstream        = (NPN_NewStreamUPP)HOST_TO_PLUGIN_GLUE(newstream, nsTable->newstream);
        gNetscapeFuncs.write            = (NPN_WriteUPP)HOST_TO_PLUGIN_GLUE(write, nsTable->write);
        gNetscapeFuncs.destroystream    = (NPN_DestroyStreamUPP)HOST_TO_PLUGIN_GLUE(destroystream, nsTable->destroystream);
        gNetscapeFuncs.status           = (NPN_StatusUPP)HOST_TO_PLUGIN_GLUE(status, nsTable->status);
        gNetscapeFuncs.uagent           = (NPN_UserAgentUPP)HOST_TO_PLUGIN_GLUE(uagent, nsTable->uagent);
        gNetscapeFuncs.memalloc         = (NPN_MemAllocUPP)HOST_TO_PLUGIN_GLUE(memalloc, nsTable->memalloc);
        gNetscapeFuncs.memfree          = (NPN_MemFreeUPP)HOST_TO_PLUGIN_GLUE(memfree, nsTable->memfree);
        gNetscapeFuncs.memflush         = (NPN_MemFlushUPP)HOST_TO_PLUGIN_GLUE(memflush, nsTable->memflush);
        gNetscapeFuncs.reloadplugins    = (NPN_ReloadPluginsUPP)HOST_TO_PLUGIN_GLUE(reloadplugins, nsTable->reloadplugins);
        if( navMinorVers >= NPVERS_HAS_LIVECONNECT )
        {
            gNetscapeFuncs.getJavaEnv   = (NPN_GetJavaEnvUPP)HOST_TO_PLUGIN_GLUE(getJavaEnv, nsTable->getJavaEnv);
            gNetscapeFuncs.getJavaPeer  = (NPN_GetJavaPeerUPP)HOST_TO_PLUGIN_GLUE(getJavaPeer, nsTable->getJavaPeer);
        }
        if( navMinorVers >= NPVERS_HAS_NOTIFICATION )
        {   
            gNetscapeFuncs.geturlnotify = (NPN_GetURLNotifyUPP)HOST_TO_PLUGIN_GLUE(geturlnotify, nsTable->geturlnotify);
            gNetscapeFuncs.posturlnotify    = (NPN_PostURLNotifyUPP)HOST_TO_PLUGIN_GLUE(posturlnotify, nsTable->posturlnotify);
        }
        gNetscapeFuncs.getvalue         = (NPN_GetValueUPP)HOST_TO_PLUGIN_GLUE(getvalue, nsTable->getvalue);
        gNetscapeFuncs.setvalue         = (NPN_SetValueUPP)HOST_TO_PLUGIN_GLUE(setvalue, nsTable->setvalue);
        gNetscapeFuncs.invalidaterect   = (NPN_InvalidateRectUPP)HOST_TO_PLUGIN_GLUE(invalidaterect, nsTable->invalidaterect);
        gNetscapeFuncs.invalidateregion = (NPN_InvalidateRegionUPP)HOST_TO_PLUGIN_GLUE(invalidateregion, nsTable->invalidateregion);
        gNetscapeFuncs.forceredraw      = (NPN_ForceRedrawUPP)HOST_TO_PLUGIN_GLUE(forceredraw, nsTable->forceredraw);
        if( navMinorVers >= 14 )
        {   
	    // NPRuntime support 
	    gNetscapeFuncs.getstringidentifier  = (NPN_GetStringIdentifierUPP)HOST_TO_PLUGIN_GLUE(getstringidentifier, nsTable->getstringidentifier);
	    gNetscapeFuncs.getstringidentifiers = (NPN_GetStringIdentifiersUPP)HOST_TO_PLUGIN_GLUE(getstringidentifiers, nsTable->getstringidentifiers);
	    gNetscapeFuncs.getintidentifier     = (NPN_GetIntIdentifierUPP)HOST_TO_PLUGIN_GLUE(getintidentifier, nsTable->getintidentifier);
	    gNetscapeFuncs.identifierisstring   = (NPN_IdentifierIsStringUPP)HOST_TO_PLUGIN_GLUE(identifierisstring, nsTable->identifierisstring);
	    gNetscapeFuncs.utf8fromidentifier   = (NPN_UTF8FromIdentifierUPP)HOST_TO_PLUGIN_GLUE(utf8fromidentifier, nsTable->utf8fromidentifier);
	    gNetscapeFuncs.intfromidentifier    = (NPN_IntFromIdentifierUPP)HOST_TO_PLUGIN_GLUE(intfromidentifier, nsTable->intfromidentifier);
	    gNetscapeFuncs.createobject         = (NPN_CreateObjectUPP)HOST_TO_PLUGIN_GLUE(createobject, nsTable->createobject);
	    gNetscapeFuncs.retainobject         = (NPN_RetainObjectUPP)HOST_TO_PLUGIN_GLUE(retainobject, nsTable->retainobject);
	    gNetscapeFuncs.releaseobject        = (NPN_ReleaseObjectUPP)HOST_TO_PLUGIN_GLUE(releaseobject, nsTable->releaseobject);
	    gNetscapeFuncs.invoke               = (NPN_InvokeUPP)HOST_TO_PLUGIN_GLUE(invoke, nsTable->invoke);
	    gNetscapeFuncs.invokeDefault        = (NPN_InvokeDefaultUPP)HOST_TO_PLUGIN_GLUE(invokeDefault, nsTable->invokeDefault);
	    gNetscapeFuncs.evaluate             = (NPN_EvaluateUPP)HOST_TO_PLUGIN_GLUE(evaluate, nsTable->evaluate);
	    gNetscapeFuncs.getproperty          = (NPN_GetPropertyUPP)HOST_TO_PLUGIN_GLUE(getproperty, nsTable->getproperty);
	    gNetscapeFuncs.setproperty          = (NPN_SetPropertyUPP)HOST_TO_PLUGIN_GLUE(setproperty, nsTable->setproperty);
	    gNetscapeFuncs.removeproperty       = (NPN_RemovePropertyUPP)HOST_TO_PLUGIN_GLUE(removeproperty, nsTable->removeproperty);
	    gNetscapeFuncs.hasproperty          = (NPN_HasPropertyUPP)HOST_TO_PLUGIN_GLUE(hasproperty, nsTable->hasproperty);
	    gNetscapeFuncs.hasmethod            = (NPN_HasMethodUPP)HOST_TO_PLUGIN_GLUE(hasmethod, nsTable->hasmethod);
	    gNetscapeFuncs.releasevariantvalue  = (NPN_ReleaseVariantValueUPP)HOST_TO_PLUGIN_GLUE(releasevariantvalue, nsTable->releasevariantvalue);
	    gNetscapeFuncs.setexception         = (NPN_SetExceptionUPP)HOST_TO_PLUGIN_GLUE(setexception, nsTable->setexception);
	}

        //
        // Set up the plugin function table that Netscape will use to
        // call us.  Netscape needs to know about our version and size
        // and have a UniversalProcPointer for every function we implement.
        //
        pluginFuncs->version        = (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;
        pluginFuncs->size           = sizeof(NPPluginFuncs);
        pluginFuncs->newp           = NewNPP_NewProc(PLUGIN_TO_HOST_GLUE(newp, Private_New));
        pluginFuncs->destroy        = NewNPP_DestroyProc(PLUGIN_TO_HOST_GLUE(destroy, Private_Destroy));
        pluginFuncs->setwindow      = NewNPP_SetWindowProc(PLUGIN_TO_HOST_GLUE(setwindow, Private_SetWindow));
        pluginFuncs->newstream      = NewNPP_NewStreamProc(PLUGIN_TO_HOST_GLUE(newstream, Private_NewStream));
        pluginFuncs->destroystream  = NewNPP_DestroyStreamProc(PLUGIN_TO_HOST_GLUE(destroystream, Private_DestroyStream));
        pluginFuncs->asfile         = NewNPP_StreamAsFileProc(PLUGIN_TO_HOST_GLUE(asfile, Private_StreamAsFile));
        pluginFuncs->writeready     = NewNPP_WriteReadyProc(PLUGIN_TO_HOST_GLUE(writeready, Private_WriteReady));
        pluginFuncs->write          = NewNPP_WriteProc(PLUGIN_TO_HOST_GLUE(write, Private_Write));
        pluginFuncs->print          = NewNPP_PrintProc(PLUGIN_TO_HOST_GLUE(print, Private_Print));
        pluginFuncs->event          = NewNPP_HandleEventProc(PLUGIN_TO_HOST_GLUE(event, Private_HandleEvent));  
        pluginFuncs->getvalue       = NewNPP_GetValueProc(PLUGIN_TO_HOST_GLUE(getvalue, Private_GetValue)); 
        if( navMinorVers >= NPVERS_HAS_NOTIFICATION )
        {   
            pluginFuncs->urlnotify = NewNPP_URLNotifyProc(PLUGIN_TO_HOST_GLUE(urlnotify, Private_URLNotify));           
        }
#ifdef OJI
        if( navMinorVers >= NPVERS_HAS_LIVECONNECT )
        {
            pluginFuncs->javaClass  = (JRIGlobalRef) Private_GetJavaClass();
        }
#else
        pluginFuncs->javaClass = NULL;
#endif
        *unloadUpp = NewNPP_ShutdownProc(PLUGIN_TO_HOST_GLUE(shutdown, Private_Shutdown));

        SetUpQD();
        err = Private_Initialize();
    }
    
    ExitCodeResource();
    return err;
}

#ifdef __MACH__

/*
** netscape plugins functions when building Mach-O binary
*/

extern "C" {
    NPError NP_Initialize(NPNetscapeFuncs* nsTable);
    NPError NP_GetEntryPoints(NPPluginFuncs* pluginFuncs);
    NPError NP_Shutdown(void);
}

/*
** netscape plugins functions when using Mach-O binary
*/

NPError NP_Initialize(NPNetscapeFuncs* nsTable)
{
    PLUGINDEBUGSTR("\pNP_Initialize");
    
    /* validate input parameters */

    if( NULL == nsTable  )
        return NPERR_INVALID_FUNCTABLE_ERROR;
    
    /*
     * Check the major version passed in Netscape's function table.
     * We won't load if the major version is newer than what we expect.
     * Also check that the function tables passed in are big enough for
     * all the functions we need (they could be bigger, if Netscape added
     * new APIs, but that's OK with us -- we'll just ignore them).
     *
     */

    if ((nsTable->version >> 8) > NP_VERSION_MAJOR)
        return NPERR_INCOMPATIBLE_VERSION_ERROR;

    if (nsTable->size < sizeof(NPNetscapeFuncs))
        return NPERR_INVALID_FUNCTABLE_ERROR;
        
    
    int navMinorVers = nsTable->version & 0xFF;

    /*
     * Copy all the fields of Netscape function table into our
     * copy so we can call back into Netscape later.  Note that
     * we need to copy the fields one by one, rather than assigning
     * the whole structure, because the Netscape function table
     * could actually be bigger than what we expect.
     */
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
    if( navMinorVers >= NPVERS_HAS_LIVECONNECT )
    {
        gNetscapeFuncs.getJavaEnv   = nsTable->getJavaEnv;
        gNetscapeFuncs.getJavaPeer  = nsTable->getJavaPeer;
    }
    if( navMinorVers >= NPVERS_HAS_NOTIFICATION )
    {    
        gNetscapeFuncs.geturlnotify     = nsTable->geturlnotify;
        gNetscapeFuncs.posturlnotify    = nsTable->posturlnotify;
    }

    gNetscapeFuncs.getvalue         = nsTable->getvalue;
    gNetscapeFuncs.setvalue         = nsTable->setvalue;
    gNetscapeFuncs.invalidaterect   = nsTable->invalidaterect;
    gNetscapeFuncs.invalidateregion = nsTable->invalidateregion;
    gNetscapeFuncs.forceredraw      = nsTable->forceredraw;
    if( navMinorVers >= 14 )
    {   
	// NPRuntime support 
	gNetscapeFuncs.getstringidentifier  = nsTable->getstringidentifier;
	gNetscapeFuncs.getstringidentifiers = nsTable->getstringidentifiers;
	gNetscapeFuncs.getintidentifier     = nsTable->getintidentifier;
	gNetscapeFuncs.identifierisstring   = nsTable->identifierisstring;
	gNetscapeFuncs.utf8fromidentifier   = nsTable->utf8fromidentifier;
	gNetscapeFuncs.intfromidentifier    = nsTable->intfromidentifier;
	gNetscapeFuncs.createobject         = nsTable->createobject;
	gNetscapeFuncs.retainobject         = nsTable->retainobject;
	gNetscapeFuncs.releaseobject        = nsTable->releaseobject;
	gNetscapeFuncs.invoke               = nsTable->invoke;
	gNetscapeFuncs.invokeDefault        = nsTable->invokeDefault;
	gNetscapeFuncs.evaluate             = nsTable->evaluate;
	gNetscapeFuncs.getproperty          = nsTable->getproperty;
	gNetscapeFuncs.setproperty          = nsTable->setproperty;
	gNetscapeFuncs.removeproperty       = nsTable->removeproperty;
	gNetscapeFuncs.hasproperty          = nsTable->hasproperty;
	gNetscapeFuncs.hasmethod            = nsTable->hasmethod;
	gNetscapeFuncs.releasevariantvalue  = nsTable->releasevariantvalue;
	gNetscapeFuncs.setexception         = nsTable->setexception;
    }
    return NPP_Initialize();
}

NPError NP_GetEntryPoints(NPPluginFuncs* pluginFuncs)
{
    int navMinorVers = gNetscapeFuncs.version & 0xFF;

    PLUGINDEBUGSTR("\pNP_GetEntryPoints");

    if( pluginFuncs == NULL )
        return NPERR_INVALID_FUNCTABLE_ERROR;

    /*if (pluginFuncs->size < sizeof(NPPluginFuncs)) 
    return NPERR_INVALID_FUNCTABLE_ERROR;*/

    /*
     * Set up the plugin function table that Netscape will use to
     * call us.  Netscape needs to know about our version and size
     * and have a UniversalProcPointer for every function we
     * implement.
     */

    pluginFuncs->version    = (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;
    pluginFuncs->size       = sizeof(NPPluginFuncs);
    pluginFuncs->newp       = Private_New;
    pluginFuncs->destroy    = NewNPP_DestroyProc(Private_Destroy);
    pluginFuncs->setwindow  = NewNPP_SetWindowProc(Private_SetWindow);
    pluginFuncs->newstream  = NewNPP_NewStreamProc(Private_NewStream);
    pluginFuncs->destroystream = NewNPP_DestroyStreamProc(Private_DestroyStream);
    pluginFuncs->asfile     = NewNPP_StreamAsFileProc(Private_StreamAsFile);
    pluginFuncs->writeready = NewNPP_WriteReadyProc(Private_WriteReady);
    pluginFuncs->write      = NewNPP_WriteProc(Private_Write);
    pluginFuncs->print      = NewNPP_PrintProc(Private_Print);
    pluginFuncs->event      = NewNPP_HandleEventProc(Private_HandleEvent);
    pluginFuncs->getvalue   = NewNPP_GetValueProc(Private_GetValue);
    pluginFuncs->setvalue   = NewNPP_SetValueProc(Private_SetValue);
    if( navMinorVers >= NPVERS_HAS_NOTIFICATION )
    {    
        pluginFuncs->urlnotify = Private_URLNotify;         
    }
#ifdef OJI
    if( navMinorVers >= NPVERS_HAS_LIVECONNECT )
    {
        pluginFuncs->javaClass  = (JRIGlobalRef) Private_GetJavaClass();
    }
#else
    pluginFuncs->javaClass = NULL;
#endif

    return NPERR_NO_ERROR;
}

NPError NP_Shutdown(void)
{
    PLUGINDEBUGSTR("\pNP_Shutdown");
    NPP_Shutdown();
    return NPERR_NO_ERROR;
}

#endif
