/*****************************************************************************
 * main.cpp: ActiveX control for VLC
 *****************************************************************************
 * Copyright (C) 2005 VideoLAN
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

using namespace std;

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

STDAPI DllRegisterServer(VOID)
{
    return S_OK;
};

STDAPI DllUnregisterServer(VOID)
{
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

