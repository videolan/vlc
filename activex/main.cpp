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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include "plugin.h"

#include <comcat.h>
#include <windows.h>
#include <shlwapi.h>

using namespace std;

#define COMPANY_STR "VideoLAN"
#define PROGRAM_STR "VLCPlugin"
#define VERSION_MAJOR_STR "1"
#define VERSION_MINOR_STR "0"
#define DESCRIPTION "VideoLAN VLC ActiveX Plugin"

#define THREADING_MODEL "Apartment"
#define MISC_STATUS     "131473"

#define PROGID_STR COMPANY_STR"."PROGRAM_STR
#define VERS_PROGID_STR COMPANY_STR"."PROGRAM_STR"."VERSION_MAJOR_STR
#define VERSION_STR VERSION_MAJOR_STR"."VERSION_MINOR_STR

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

    if( CLSID_VLCPlugin == rclsid )
    {
        VLCPluginClass *plugin = new VLCPluginClass(&i_class_ref, h_instance);
        hr = plugin->QueryInterface(riid, ppv);
        plugin->Release();
    }
    return hr;
};

STDAPI DllCanUnloadNow(VOID)
{
    return (0 == i_class_ref) ? S_OK: S_FALSE;
};

static LPCTSTR TStrFromGUID(REFGUID clsid)
{
    LPOLESTR oleStr;

    if( FAILED(StringFromIID(clsid, &oleStr)) )
        return NULL;

    //check whether TCHAR and OLECHAR are both either ANSI or UNICODE
    if( sizeof(TCHAR) == sizeof(OLECHAR) )
        return (LPCTSTR)oleStr;

    LPTSTR pct_CLSID = NULL;
#ifndef OLE2ANSI
    size_t len = WideCharToMultiByte(CP_ACP, 0, oleStr, -1, NULL, 0, NULL, NULL);
    if( len > 0 )
    {
        pct_CLSID = (char *)CoTaskMemAlloc(len);
        WideCharToMultiByte(CP_ACP, 0, oleStr, -1, pct_CLSID, len, NULL, NULL);
    }
#else
    size_t len = MutiByteToWideChar(CP_ACP, 0, oleStr, -1, NULL, 0);
    if( len > 0 )
    {
        clsidStr = (wchar_t *)CoTaskMemAlloc(len*sizeof(wchar_t));
        WideCharToMultiByte(CP_ACP, 0, oleStr, -1, pct_CLSID, len);
    }
#endif
    CoTaskMemFree(oleStr);
    return pct_CLSID;
};

static HKEY keyCreate(HKEY parentKey, LPCTSTR keyName)
{
    HKEY childKey;
    if( ERROR_SUCCESS == RegCreateKeyEx(parentKey, keyName, 0, NULL,
                REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &childKey, NULL) )
    {
        return childKey;
    }
    return NULL;
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
        pcr->Release();
    }

    SHDeleteKey(HKEY_CLASSES_ROOT, TEXT(VERS_PROGID_STR));
    SHDeleteKey(HKEY_CLASSES_ROOT, TEXT(PROGID_STR));

    LPCTSTR psz_CLSID = TStrFromGUID(CLSID_VLCPlugin);

    if( NULL == psz_CLSID )
        return E_OUTOFMEMORY;

    HKEY hClsIDKey;
    if( ERROR_SUCCESS == RegOpenKeyEx(HKEY_CLASSES_ROOT, TEXT("CLSID"), 0, KEY_WRITE, &hClsIDKey) )
    {
        SHDeleteKey(hClsIDKey, psz_CLSID);
        RegCloseKey(hClsIDKey);
    }
    CoTaskMemFree((void *)psz_CLSID);

    return S_OK;
};

STDAPI DllRegisterServer(VOID)
{
    DllUnregisterServer();

    char DllPath[MAX_PATH];
    DWORD DllPathLen= GetModuleFileName(h_instance, DllPath, sizeof(DllPath)) ;
	if( 0 == DllPathLen )
        return E_UNEXPECTED;

    LPCTSTR psz_CLSID = TStrFromGUID(CLSID_VLCPlugin);

    if( NULL == psz_CLSID )
        return E_OUTOFMEMORY;

    HKEY hBaseKey;

    if( ERROR_SUCCESS != RegOpenKeyEx(HKEY_CLASSES_ROOT, TEXT("CLSID"), 0, KEY_CREATE_SUB_KEY, &hBaseKey) )
        return SELFREG_E_CLASS;

    HKEY hClassKey = keyCreate(hBaseKey, psz_CLSID);
    if( NULL != hClassKey )
    {
        HKEY hSubKey;

        // default key value
        RegSetValueEx(hClassKey, NULL, 0, REG_SZ,
                (const BYTE*)DESCRIPTION, sizeof(DESCRIPTION));

        // Control key value
        hSubKey = keyCreate(hClassKey, TEXT("Control"));
        RegCloseKey(hSubKey);

        // ToolboxBitmap32 key value
        hSubKey = keyCreate(hClassKey, TEXT("ToolboxBitmap32"));
        strcpy(DllPath+DllPathLen, ",1");
        RegSetValueEx(hSubKey, NULL, 0, REG_SZ,
                (const BYTE*)DllPath, DllPathLen+2);
        DllPath[DllPathLen] = '\0';
        RegCloseKey(hSubKey);

#ifdef BUILD_LOCALSERVER
        // LocalServer32 key value
        hSubKey = keyCreate(hClassKey, TEXT("LocalServer32"));
        RegSetValueEx(hSubKey, NULL, 0, REG_SZ,
                (const BYTE*)DllPath, DllPathLen);
        RegCloseKey(hSubKey);
#else
        // InprocServer32 key value
        hSubKey = keyCreate(hClassKey, TEXT("InprocServer32"));
        RegSetValueEx(hSubKey, NULL, 0, REG_SZ,
                (const BYTE*)DllPath, DllPathLen);
        RegSetValueEx(hSubKey, TEXT("ThreadingModel"), 0, REG_SZ,
                (const BYTE*)THREADING_MODEL, sizeof(THREADING_MODEL));
        RegCloseKey(hSubKey);
#endif

        // MiscStatus key value
        hSubKey = keyCreate(hClassKey, TEXT("MiscStatus\\1"));
        RegSetValueEx(hSubKey, NULL, 0, REG_SZ, (const BYTE*)MISC_STATUS, sizeof(MISC_STATUS));
        RegCloseKey(hSubKey);

        // Programmable key value
        hSubKey = keyCreate(hClassKey, TEXT("Programmable"));
        RegCloseKey(hSubKey);

        // ProgID key value
        hSubKey = keyCreate(hClassKey, TEXT("ProgID"));
        RegSetValueEx(hSubKey, NULL, 0, REG_SZ, 
                (const BYTE*)VERS_PROGID_STR, sizeof(VERS_PROGID_STR));
        RegCloseKey(hSubKey);

        // VersionIndependentProgID key value
        hSubKey = keyCreate(hClassKey, TEXT("VersionIndependentProgID"));
        RegSetValueEx(hSubKey, NULL, 0, REG_SZ, 
                (const BYTE*)PROGID_STR, sizeof(PROGID_STR));
        RegCloseKey(hSubKey);

        // Version key value
        hSubKey = keyCreate(hClassKey, TEXT("Version"));
        RegSetValueEx(hSubKey, NULL, 0, REG_SZ,
                (const BYTE*)VERSION_STR, sizeof(VERSION_STR));
        RegCloseKey(hSubKey);

        // TypeLib key value
        LPCTSTR psz_LIBID = TStrFromGUID(LIBID_AXVLC);
        if( NULL != psz_LIBID )
        {
            hSubKey = keyCreate(hClassKey, TEXT("TypeLib"));
            RegSetValueEx(hSubKey, NULL, 0, REG_SZ,
                    (const BYTE*)psz_LIBID, sizeof(TCHAR)*GUID_STRLEN);
            RegCloseKey(hSubKey);
        }
        RegCloseKey(hClassKey);
    }
    RegCloseKey(hBaseKey);

    hBaseKey = keyCreate(HKEY_CLASSES_ROOT, TEXT(PROGID_STR));
    if( NULL != hBaseKey )
    {
        // default key value
        RegSetValueEx(hBaseKey, NULL, 0, REG_SZ,
                (const BYTE*)DESCRIPTION, sizeof(DESCRIPTION));

        HKEY hSubKey = keyCreate(hBaseKey, TEXT("CLSID"));
        if( NULL != hSubKey )
        {
            // default key value
            RegSetValueEx(hSubKey, NULL, 0, REG_SZ,
                    (const BYTE*)psz_CLSID, sizeof(TCHAR)*GUID_STRLEN);

            RegCloseKey(hSubKey);
        }
        hSubKey = keyCreate(hBaseKey, TEXT("CurVer"));
        if( NULL != hSubKey )
        {
            // default key value
            RegSetValueEx(hSubKey, NULL, 0, REG_SZ,
                    (const BYTE*)VERS_PROGID_STR, sizeof(VERS_PROGID_STR));

            RegCloseKey(hSubKey);
        }
        RegCloseKey(hBaseKey);
    }

    hBaseKey = keyCreate(HKEY_CLASSES_ROOT, TEXT(VERS_PROGID_STR));
    if( NULL != hBaseKey )
    {
        // default key value
        RegSetValueEx(hBaseKey, NULL, 0, REG_SZ,
                (const BYTE*)DESCRIPTION, sizeof(DESCRIPTION));

        HKEY hSubKey = keyCreate(hBaseKey, TEXT("CLSID"));
        if( NULL != hSubKey )
        {
            // default key value
            RegSetValueEx(hSubKey, NULL, 0, REG_SZ,
                    (const BYTE*)psz_CLSID, sizeof(TCHAR)*GUID_STRLEN);

            RegCloseKey(hSubKey);
        }
        RegCloseKey(hBaseKey);
    }

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

    CoTaskMemFree((void *)psz_CLSID);

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

