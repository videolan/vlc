/*****************************************************************************
 * main.cpp: ActiveX control for VLC
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 *
 * Authors: Damien Fouilleul <Damien.Fouilleul@laposte.net>
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

#include "plugin.h"
#include "utils.h"

#include <stdio.h>

#include <comcat.h>
#include <windows.h>
#include <shlwapi.h>

using namespace std;

#define COMPANY_STR "VideoLAN"
#define PROGRAM_STR "VLCPlugin"
#define DESCRIPTION "VideoLAN VLC ActiveX Plugin"

#define THREADING_MODEL "Apartment"
#define MISC_STATUS     "131473"

#define PROGID_STR COMPANY_STR"."PROGRAM_STR

#define GUID_STRLEN 39

/*
** MingW headers do not declare those
*/
extern const CATID CATID_SafeForInitializing;
extern const CATID CATID_SafeForScripting;

static LONG i_class_ref= 0;
static HINSTANCE h_instance= 0;

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
    HRESULT hr = CLASS_E_CLASSNOTAVAILABLE;

    *ppv = NULL;

    if( (CLSID_VLCPlugin == rclsid)
     || (CLSID_VLCPlugin2 == rclsid) )
    {
        VLCPluginClass *plugin = new VLCPluginClass(&i_class_ref, h_instance, rclsid);
        hr = plugin->QueryInterface(riid, ppv);
        plugin->Release();
    }
    return hr;
};

STDAPI DllCanUnloadNow(VOID)
{
    return (0 == i_class_ref) ? S_OK: S_FALSE;
};

static inline HKEY keyCreate(HKEY parentKey, LPCSTR keyName)
{
    HKEY childKey;
    if( ERROR_SUCCESS == RegCreateKeyExA(parentKey, keyName, 0, NULL,
                REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &childKey, NULL) )
    {
        return childKey;
    }
    return NULL;
};

static inline HKEY keySet(HKEY hKey, LPCSTR valueName, const void *s, size_t len)
{
    if( NULL != hKey )
    {
        RegSetValueExA(hKey, valueName, 0, REG_SZ,
            (const BYTE*)s, len);
    }
    return hKey;
};

static inline HKEY keySetDef(HKEY hKey, const void *s, size_t len)
{
    return keySet(hKey, NULL, s, len);
};

static inline HKEY keySetDef(HKEY hKey, LPCSTR s)
{
    return keySetDef(hKey, s, strlen(s)+1);
};

static inline HKEY keyClose(HKEY hKey)
{
    if( NULL != hKey )
    {
        RegCloseKey(hKey);
    }
    return NULL;
};

static HRESULT UnregisterProgID(REFCLSID rclsid, unsigned int version)
{
    LPCSTR psz_CLSID = CStrFromGUID(rclsid);

    if( NULL == psz_CLSID )
        return E_OUTOFMEMORY;

    char progId[sizeof(PROGID_STR)+16];
    sprintf(progId, "%s.%u", PROGID_STR, version);

    SHDeleteKeyA(HKEY_CLASSES_ROOT, progId);

    HKEY hClsIDKey;
    if( ERROR_SUCCESS == RegOpenKeyExA(HKEY_CLASSES_ROOT, "CLSID", 0, KEY_WRITE, &hClsIDKey) )
    {
        SHDeleteKey(hClsIDKey, psz_CLSID);
        RegCloseKey(hClsIDKey);
    }
    CoTaskMemFree((void *)psz_CLSID);
};

STDAPI DllUnregisterServer(VOID)
{
    // unregister type lib from the registry
    UnRegisterTypeLib(LIBID_AXVLC, 1, 0, LOCALE_NEUTRAL, SYS_WIN32);

    // remove component categories we supports
    ICatRegister *pcr;
    if( SUCCEEDED(CoCreateInstance(CLSID_StdComponentCategoriesMgr, 
            NULL, CLSCTX_INPROC_SERVER, IID_ICatRegister, (void**)&pcr)) ) {
        CATID implCategories[] = {
            CATID_Control,
            CATID_PersistsToPropertyBag,
            CATID_SafeForInitializing,
            CATID_SafeForScripting,
        };

        pcr->UnRegisterClassImplCategories(CLSID_VLCPlugin,
                sizeof(implCategories)/sizeof(CATID), implCategories);
        pcr->UnRegisterClassImplCategories(CLSID_VLCPlugin2,
                sizeof(implCategories)/sizeof(CATID), implCategories);
        pcr->Release();
    }

    SHDeleteKey(HKEY_CLASSES_ROOT, TEXT(PROGID_STR));

    UnregisterProgID(CLSID_VLCPlugin, 1);
    UnregisterProgID(CLSID_VLCPlugin2, 1);

    return S_OK;
};

static HRESULT RegisterClassID(HKEY hParent, REFCLSID rclsid, unsigned int version, const char *path, size_t pathLen)
{
    HKEY hClassKey;
    {
        LPCSTR psz_CLSID = CStrFromGUID(rclsid);

        if( NULL == psz_CLSID )
            return E_OUTOFMEMORY;

        hClassKey = keyCreate(hParent, psz_CLSID);
        CoTaskMemFree((void *)psz_CLSID);
    }
    if( NULL != hClassKey )
    {
        // default key value
        keySetDef(hClassKey, DESCRIPTION, sizeof(DESCRIPTION));

        // Control key value
        keyClose(keyCreate(hClassKey, "Control"));

        // Insertable key value
        //keyClose(keyCreate(hClassKey, "Insertable"));

        // ToolboxBitmap32 key value
        {
            char iconPath[pathLen+3];
            memcpy(iconPath, path, pathLen);
            strcpy(iconPath+pathLen, ",1");
            keyClose(keySetDef(keyCreate(hClassKey,
                "ToolboxBitmap32"),
                iconPath, sizeof(iconPath)));
        }

#ifdef BUILD_LOCALSERVER
        // LocalServer32 key value
        keyClose(keySetDef(keyCreate(hClassKey,
            "LocalServer32", path, pathLen+1)));
#else
        // InprocServer32 key value
        {
            HKEY hSubKey = keySetDef(keyCreate(hClassKey,
                "InprocServer32"),
                path, pathLen+1);
            keySet(hSubKey,
                "ThreadingModel",
                THREADING_MODEL, sizeof(THREADING_MODEL));
            keyClose(hSubKey);
        }
#endif

        // MiscStatus key value
        keyClose(keySetDef(keyCreate(hClassKey,
            "MiscStatus\\1"),
            MISC_STATUS, sizeof(MISC_STATUS)));

        // Programmable key value
        keyClose(keyCreate(hClassKey, "Programmable"));

        // ProgID key value
        {
            char progId[sizeof(PROGID_STR)+16];
            sprintf(progId, "%s.%u", PROGID_STR, version);
            keyClose(keySetDef(keyCreate(hClassKey,
                TEXT("ProgID")),
                progId));
        }

        // VersionIndependentProgID key value
        keyClose(keySetDef(keyCreate(hClassKey,
            "VersionIndependentProgID"),
            PROGID_STR, sizeof(PROGID_STR)));

        // Version key value
        {
            char ver[32];
            sprintf(ver, "%u.0", version);
            keyClose(keySetDef(keyCreate(hClassKey,
                "Version"),
                ver));
        }

        // TypeLib key value
        LPCSTR psz_LIBID = CStrFromGUID(LIBID_AXVLC);
        if( NULL != psz_LIBID )
        {
            keyClose(keySetDef(keyCreate(hClassKey,
                    "TypeLib"),
                    psz_LIBID, GUID_STRLEN));
            CoTaskMemFree((void *)psz_LIBID);
        }
        RegCloseKey(hClassKey);
    }
    return S_OK;
}

static HRESULT RegisterProgID(REFCLSID rclsid, unsigned int version)
{
    LPCSTR psz_CLSID = CStrFromGUID(rclsid);

    if( NULL == psz_CLSID )
        return E_OUTOFMEMORY;

    char progId[sizeof(PROGID_STR)+16];
    sprintf(progId, "%s.%u", PROGID_STR, version);

    HKEY hBaseKey = keyCreate(HKEY_CLASSES_ROOT, progId);
    if( NULL != hBaseKey )
    {
        // default key value
        keySetDef(hBaseKey, DESCRIPTION, sizeof(DESCRIPTION));

        keyClose(keySetDef(keyCreate(hBaseKey, "CLSID"),
            psz_CLSID,
            GUID_STRLEN));
 
        //hSubKey = keyClose(keyCreate(hBaseKey, "Insertable"));
 
        RegCloseKey(hBaseKey);
    }
    CoTaskMemFree((void *)psz_CLSID);

    return S_OK;
}

static HRESULT RegisterDefaultProgID(REFCLSID rclsid, unsigned int version)
{
    LPCSTR psz_CLSID = CStrFromGUID(rclsid);

    if( NULL == psz_CLSID )
        return E_OUTOFMEMORY;

    HKEY hBaseKey = keyCreate(HKEY_CLASSES_ROOT, PROGID_STR);
    if( NULL != hBaseKey )
    {
        // default key value
        keySetDef(hBaseKey, DESCRIPTION, sizeof(DESCRIPTION));

        keyClose(keySetDef(keyCreate(hBaseKey, "CLSID"),
            psz_CLSID,
            GUID_STRLEN));
 
        char progId[sizeof(PROGID_STR)+16];
        sprintf(progId, "%s.%u", PROGID_STR, version);

        keyClose(keySetDef(keyCreate(hBaseKey, "CurVer"),
            progId));
    }
    CoTaskMemFree((void *)psz_CLSID);
}

STDAPI DllRegisterServer(VOID)
{
    DllUnregisterServer();

    char DllPath[MAX_PATH];
    DWORD DllPathLen=GetModuleFileNameA(h_instance, DllPath, sizeof(DllPath)) ;
	if( 0 == DllPathLen )
        return E_UNEXPECTED;

    HKEY hBaseKey;

    if( ERROR_SUCCESS != RegOpenKeyExA(HKEY_CLASSES_ROOT, "CLSID", 0, KEY_CREATE_SUB_KEY, &hBaseKey) )
        return SELFREG_E_CLASS;
    
    RegisterClassID(hBaseKey, CLSID_VLCPlugin, 1, DllPath, DllPathLen);
    RegisterClassID(hBaseKey, CLSID_VLCPlugin2, 2, DllPath, DllPathLen);

    RegCloseKey(hBaseKey);

    RegisterProgID(CLSID_VLCPlugin, 1);
    RegisterProgID(CLSID_VLCPlugin2, 2);

    /* default control */
    RegisterDefaultProgID(CLSID_VLCPlugin2, 2);

    // indicate which component categories we support
    ICatRegister *pcr;
    if( SUCCEEDED(CoCreateInstance(CLSID_StdComponentCategoriesMgr, 
            NULL, CLSCTX_INPROC_SERVER, IID_ICatRegister, (void**)&pcr)) ) {
        CATID implCategories[] = {
            CATID_Control,
            CATID_PersistsToPropertyBag,
            CATID_SafeForInitializing,
            CATID_SafeForScripting,
        };

        pcr->RegisterClassImplCategories(CLSID_VLCPlugin,
                sizeof(implCategories)/sizeof(CATID), implCategories);
        pcr->RegisterClassImplCategories(CLSID_VLCPlugin2,
                sizeof(implCategories)/sizeof(CATID), implCategories);
        pcr->Release();
    }

    // register type lib into the registry
    ITypeLib *typeLib;

#ifdef BUILD_LOCALSERVER
    // replace .exe by .tlb
    strcpy(DllPath+DllPathLen-4, ".tlb");
#endif
    
#ifndef OLE2ANSI
    size_t typeLibPathLen = MultiByteToWideChar(CP_ACP, 0, DllPath, -1, NULL, 0);
    if( typeLibPathLen > 0 )
    {
        LPOLESTR typeLibPath = (LPOLESTR)CoTaskMemAlloc(typeLibPathLen*sizeof(wchar_t));
        MultiByteToWideChar(CP_ACP, 0, DllPath, DllPathLen, typeLibPath, typeLibPathLen);
        if( FAILED(LoadTypeLibEx(typeLibPath, REGKIND_REGISTER, &typeLib)) )
#ifndef BUILD_LOCALSERVER
            return SELFREG_E_TYPELIB;
        typeLib->Release();
#endif
        CoTaskMemFree((void *)typeLibPath);
    }
#else
    if( FAILED(LoadTypeLibEx((LPOLESTR)DllPath, REGKIND_REGISTER, &typeLib)) )
        return SELFREG_E_TYPELIB;
    typeLib->Release();
#endif

    return S_OK;
};

#ifdef BUILD_LOCALSERVER

/*
** easier to debug an application than a DLL on cygwin GDB :)
*/
#include <iostream>

STDAPI_(int) WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw)
{
    MSG msg;

    if( FAILED(OleInitialize(NULL)) )
    {
        cerr << "cannot initialize OLE" << endl;
        return 1;
    }

    h_instance = hInst;

    if( FAILED(DllRegisterServer()) )
    {
        cerr << "cannot register Local Server" << endl;
        return 1;
    }

    IUnknown *classProc = NULL;

    if( FAILED(DllGetClassObject(CLSID_VLCPlugin, IID_IUnknown, (LPVOID *)&classProc)) )
        return 0;
 
    DWORD dwRegisterClassObject;

    if( FAILED(CoRegisterClassObject(CLSID_VLCPlugin, classProc,
        CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &dwRegisterClassObject)) )
        return 0;

    DWORD dwRegisterActiveObject;

    if( FAILED(RegisterActiveObject(classProc, CLSID_VLCPlugin,
                    ACTIVEOBJECT_WEAK, &dwRegisterActiveObject)) )
        return 0;

    classProc->Release();

    /*
    * Polling messages from event queue
    */
    while( S_FALSE == DllCanUnloadNow() )
    {
        while( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
        {
            if( msg.message == WM_QUIT )
                break;  // Leave the PeekMessage while() loop

            /*if(TranslateAccelerator(ghwndApp, ghAccel, &msg))
                continue;*/

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if(msg.message == WM_QUIT)
            break;  // Leave the for() loop

        WaitMessage();
    }

    if( SUCCEEDED(RevokeActiveObject(dwRegisterActiveObject, NULL)) )
        CoRevokeClassObject(dwRegisterClassObject);

    // Reached on WM_QUIT message
    CoUninitialize();
    return ((int) msg.wParam);
};

#else

STDAPI_(BOOL) DllMain(HANDLE hModule, DWORD fdwReason, LPVOID lpReserved )
{
    switch( fdwReason )
    {
        case DLL_PROCESS_ATTACH:
            h_instance = (HINSTANCE)hModule;
            break;

        default:
            break;
    }
    return TRUE;
};

#endif

