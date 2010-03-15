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

#include <tchar.h>
#include <guiddef.h>

using namespace std;

#define COMPANY_STR "VideoLAN"
#define PROGRAM_STR "VLCPlugin"
#define DESCRIPTION "VideoLAN VLC ActiveX Plugin"

#define THREADING_MODEL "Apartment"
#define MISC_STATUS     "131473"

#define PROGID_STR COMPANY_STR"."PROGRAM_STR

#define GUID_STRLEN 39

/*
** MingW headers & libs do not declare those
*/
static DEFINE_GUID(_CATID_InternetAware, \
	0x0DE86A58, 0x2BAA, 0x11CF, 0xA2, 0x29, 0x00,0xAA,0x00,0x3D,0x73,0x52);
static DEFINE_GUID(_CATID_SafeForInitializing, \
	0x7DD95802, 0x9882, 0x11CF, 0x9F, 0xA9, 0x00,0xAA,0x00,0x6C,0x42,0xC4);
static DEFINE_GUID(_CATID_SafeForScripting, \
	0x7DD95801, 0x9882, 0x11CF, 0x9F, 0xA9, 0x00,0xAA,0x00,0x6C,0x42,0xC4);

static LONG i_class_ref= 0;
static HINSTANCE h_instance= 0;

HMODULE DllGetModule()
{
    return h_instance;
};

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
    HRESULT hr = CLASS_E_CLASSNOTAVAILABLE;

    *ppv = NULL;

    if( (CLSID_VLCPlugin == rclsid) || (CLSID_VLCPlugin2 == rclsid) )
    {
        VLCPluginClass *plugin =
            new VLCPluginClass(&i_class_ref, h_instance, rclsid);
        hr = plugin->QueryInterface(riid, ppv);
        plugin->Release();
    }
    return hr;
};

STDAPI DllCanUnloadNow(VOID)
{
    return (0 == i_class_ref) ? S_OK: S_FALSE;
};

static inline HKEY keyCreate(HKEY parentKey, LPCTSTR keyName)
{
    HKEY childKey;
    if( ERROR_SUCCESS == RegCreateKeyEx(parentKey, keyName, 0, NULL,
             REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &childKey, NULL) )
    {
        return childKey;
    }
    return NULL;
};

static inline HKEY keySet(HKEY hKey, LPCTSTR valueName,
                          const void *s, size_t len, DWORD dwType = REG_SZ)
{
    if( NULL != hKey )
    {
        RegSetValueEx(hKey, valueName, 0, dwType, (const BYTE*)s, len);
    }
    return hKey;
};

static inline HKEY keySetDef(HKEY hKey,
                             const void *s, size_t len, DWORD dwType = REG_SZ)
{
    return keySet(hKey, NULL, s, len, dwType);
};

static inline HKEY keySetDef(HKEY hKey, LPCTSTR s)
{
    return keySetDef(hKey, s, sizeof(TCHAR)*(_tcslen(s)+1), REG_SZ);
};

static inline HKEY keyClose(HKEY hKey)
{
    if( NULL != hKey )
    {
        RegCloseKey(hKey);
    }
    return NULL;
};

static void UnregisterProgID(REFCLSID rclsid, unsigned int version)
{
    OLECHAR szCLSID[GUID_STRLEN];

    StringFromGUID2(rclsid, szCLSID, GUID_STRLEN);

    TCHAR progId[sizeof(PROGID_STR)+16];
    _stprintf(progId, TEXT("%s.%u"), TEXT(PROGID_STR), version);

    SHDeleteKey(HKEY_CLASSES_ROOT, progId);

    HKEY hClsIDKey;
    if( ERROR_SUCCESS == RegOpenKeyEx(HKEY_CLASSES_ROOT, TEXT("CLSID"), 0, KEY_WRITE, &hClsIDKey) )
    {
        SHDeleteKey(hClsIDKey, szCLSID);
        RegCloseKey(hClsIDKey);
    }
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
            _CATID_InternetAware,
            _CATID_SafeForInitializing,
            _CATID_SafeForScripting,
        };

        pcr->UnRegisterClassImplCategories(CLSID_VLCPlugin,
                sizeof(implCategories)/sizeof(CATID), implCategories);
        pcr->UnRegisterClassImplCategories(CLSID_VLCPlugin2,
                sizeof(implCategories)/sizeof(CATID), implCategories);
        pcr->Release();
    }

    SHDeleteKey(HKEY_CLASSES_ROOT, TEXT(PROGID_STR));

    UnregisterProgID(CLSID_VLCPlugin, 2);
    UnregisterProgID(CLSID_VLCPlugin2, 1);

    return S_OK;
};

static HRESULT RegisterClassID(HKEY hParent, REFCLSID rclsid, unsigned int version, BOOL isDefault, LPCTSTR path, size_t pathLen)
{
    TCHAR progId[sizeof(PROGID_STR)+16];
    _stprintf(progId, TEXT("%s.%u"), TEXT(PROGID_STR), version);

    TCHAR description[sizeof(DESCRIPTION)+16];
    _stprintf(description, TEXT("%s v%u"), TEXT(DESCRIPTION), version);

    HKEY hClassKey;
    {
        OLECHAR szCLSID[GUID_STRLEN];

        StringFromGUID2(rclsid, szCLSID, GUID_STRLEN);

        HKEY hProgKey = keyCreate(HKEY_CLASSES_ROOT, progId);
        if( NULL != hProgKey )
        {
            // default key value
            keySetDef(hProgKey, description);

            keyClose(keySetDef(keyCreate(hProgKey, TEXT("CLSID")),
                               szCLSID, sizeof(szCLSID)));

            //hSubKey = keyClose(keyCreate(hBaseKey, "Insertable"));
 
            RegCloseKey(hProgKey);
        }
        if( isDefault )
        {
            hProgKey = keyCreate(HKEY_CLASSES_ROOT, TEXT(PROGID_STR));
            if( NULL != hProgKey )
            {
                // default key value
                keySetDef(hProgKey, description);

                keyClose(keySetDef(keyCreate(hProgKey, TEXT("CLSID")),
                                   szCLSID, sizeof(szCLSID)));

                keyClose(keySetDef(keyCreate(hProgKey, TEXT("CurVer")),
                                   progId));
            }
        }
        hClassKey = keyCreate(hParent, szCLSID);
    }
    if( NULL != hClassKey )
    {
        // default key value
        keySetDef(hClassKey, description);

        // Control key value
        keyClose(keyCreate(hClassKey, TEXT("Control")));

        // Insertable key value
        //keyClose(keyCreate(hClassKey, TEXT("Insertable")));

        // ToolboxBitmap32 key value
        {
            TCHAR iconPath[pathLen+3];
            memcpy(iconPath, path, sizeof(TCHAR)*pathLen);
            _tcscpy(iconPath+pathLen, TEXT(",1"));
            keyClose(keySetDef(keyCreate(hClassKey,
                TEXT("ToolboxBitmap32")),
                iconPath, sizeof(iconPath)));
        }

#ifdef BUILD_LOCALSERVER
        // LocalServer32 key value
        keyClose(keySetDef(keyCreate(hClassKey,
            TEXT("LocalServer32"), path, sizeof(TCHAR)*(pathLen+1))));
#else
        // InprocServer32 key value
        {
            HKEY hSubKey = keySetDef(keyCreate(hClassKey,
                TEXT("InprocServer32")),
                path, sizeof(TCHAR)*(pathLen+1));
            keySet(hSubKey,
                TEXT("ThreadingModel"),
                TEXT(THREADING_MODEL), sizeof(TEXT(THREADING_MODEL)));
            keyClose(hSubKey);
        }
#endif

        // MiscStatus key value
        keyClose(keySetDef(keyCreate(hClassKey,TEXT("MiscStatus\\1")),
                           TEXT(MISC_STATUS), sizeof(TEXT(MISC_STATUS))));

        // Programmable key value
        keyClose(keyCreate(hClassKey, TEXT("Programmable")));

        // ProgID key value
        keyClose(keySetDef(keyCreate(hClassKey,TEXT("ProgID")),progId));

        // VersionIndependentProgID key value
        keyClose(keySetDef(keyCreate(hClassKey,
                                     TEXT("VersionIndependentProgID")),
                           TEXT(PROGID_STR), sizeof(TEXT(PROGID_STR))));

        // Version key value
        keyClose(keySetDef(keyCreate(hClassKey,TEXT("Version")),TEXT("1.0")));

        // TypeLib key value
        OLECHAR szLIBID[GUID_STRLEN];

        StringFromGUID2(LIBID_AXVLC, szLIBID, GUID_STRLEN);

        keyClose(keySetDef(keyCreate(hClassKey,TEXT("TypeLib")),
                           szLIBID, sizeof(szLIBID)));
 
        RegCloseKey(hClassKey);
    }
    return S_OK;
}

STDAPI DllRegisterServer(VOID)
{
    DllUnregisterServer();

    TCHAR DllPath[MAX_PATH];
    DWORD DllPathLen=GetModuleFileName(h_instance, DllPath, MAX_PATH) ;
    if( 0 == DllPathLen )
        return E_UNEXPECTED;
    DllPath[MAX_PATH-1] = '\0';

    HKEY hBaseKey;

    if( ERROR_SUCCESS != RegOpenKeyEx(HKEY_CLASSES_ROOT, TEXT("CLSID"),
                                      0, KEY_CREATE_SUB_KEY, &hBaseKey) )
        return SELFREG_E_CLASS;

    RegisterClassID(hBaseKey, CLSID_VLCPlugin, 1, FALSE, DllPath, DllPathLen);
    RegisterClassID(hBaseKey, CLSID_VLCPlugin2, 2, TRUE, DllPath, DllPathLen);

    RegCloseKey(hBaseKey);

    // indicate which component categories we support
    ICatRegister *pcr;
    if( SUCCEEDED(CoCreateInstance(CLSID_StdComponentCategoriesMgr,
            NULL, CLSCTX_INPROC_SERVER, IID_ICatRegister, (void**)&pcr)) ) {
        CATID implCategories[] = {
            CATID_Control,
            CATID_PersistsToPropertyBag,
            _CATID_InternetAware,
            _CATID_SafeForInitializing,
            _CATID_SafeForScripting,
        };

        pcr->RegisterClassImplCategories(CLSID_VLCPlugin,
                sizeof(implCategories)/sizeof(CATID), implCategories);
        pcr->RegisterClassImplCategories(CLSID_VLCPlugin2,
                sizeof(implCategories)/sizeof(CATID), implCategories);
        pcr->Release();
    }

#ifdef BUILD_LOCALSERVER
    // replace .exe by .tlb
    _tcscpy(DllPath+DllPathLen-4, TEXT(".tlb"));
#endif

    // register type lib into the registry
    ITypeLib *typeLib;

    HRESULT result = LoadTypeLibEx(DllPath, REGKIND_REGISTER, &typeLib);
    if( SUCCEEDED(result) )
        typeLib->Release();

    return result;
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

    if( FAILED(DllGetClassObject(CLSID_VLCPlugin, IID_IUnknown,
                                 (LPVOID *)&classProc)) )
        return 0;
 
    DWORD dwRegisterClassObject;
    DWORD dwRegisterClassObject2;

    if( FAILED(CoRegisterClassObject(CLSID_VLCPlugin, classProc,
        CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &dwRegisterClassObject)) )
        return 0;

    DWORD dwRegisterActiveObject;

    if( FAILED(RegisterActiveObject(classProc, CLSID_VLCPlugin,
                    ACTIVEOBJECT_WEAK, &dwRegisterActiveObject)) )
        return 0;

    if( FAILED(RegisterActiveObject(classProc, CLSID_VLCPlugin2,
                    ACTIVEOBJECT_WEAK, &dwRegisterActiveObject2)) )
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
                break;  // break out PeekMessage loop

            /*if(TranslateAccelerator(ghwndApp, ghAccel, &msg))
                continue;*/

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if(msg.message == WM_QUIT)
            break;  // break out main loop

        WaitMessage();
    }

    if( SUCCEEDED(RevokeActiveObject(dwRegisterActiveObject, NULL)) )
        CoRevokeClassObject(dwRegisterClassObject);

    if( SUCCEEDED(RevokeActiveObject(dwRegisterActiveObject2, NULL)) )
        CoRevokeClassObject(dwRegisterClassObject2);

    // Reached on WM_QUIT message
    OleUninitialize();
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

