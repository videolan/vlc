/*****************************************************************************
 * vistaext.h : "Vista file associations support"
 ****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 * $Id$
 *
 * Authors: Geoffroy Couprie <geal@videolan.org>
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

#ifndef VISTAASSOC_H
#define VISTAASSOC_H

const GUID clsid_IApplication2 = { 0x1968106d,0xf3b5,0x44cf,{0x89,0x0e,0x11,0x6f,0xcb,0x9e,0xce,0xf1}};
const GUID IID_IApplicationAssociationRegistrationUI = {0x1f76a169,0xf994,0x40ac, {0x8f,0xc8,0x09,0x59,0xe8,0x87,0x47,0x10}};

#undef IUnknown
typedef struct _IUnknown IUnknown;
typedef struct _IApplicationAssociationRegistrationUI IApplicationAssociationRegistrationUI;

typedef struct IUnknown_vt
{
    long (STDCALL *QueryInterface)(IUnknown *This, const GUID *riid,
                                   void **ppvObject);
    long (STDCALL *AddRef)(IUnknown *This);
    long (STDCALL *Release)(IUnknown *This);

} IUnknown_vt;
struct _IUnknown { IUnknown_vt* vt; };
typedef IUnknown *LPUNKNOWN;

typedef struct IApplicationAssociationRegistrationUI_vt
{
    /* IUnknown methods */
    long (STDCALL *QueryInterface)(IUnknown *This, const GUID *riid,
                                   void **ppvObject);
    long (STDCALL *AddRef)(IUnknown *This);
    long (STDCALL *Release)(IUnknown *This);
    long (STDCALL *LaunchAdvancedAssociationUI)(IApplicationAssociationRegistrationUI *This, LPCWSTR app);
} IApplicationAssociationRegistrationUI_vt;
struct _IApplicationAssociationRegistrationUI { IApplicationAssociationRegistrationUI_vt* vt; };
typedef IApplicationAssociationRegistrationUI *LPAPPASSOCREGUI, *PAPPASSOCREGUI;

#define CLSCTX_INPROC_SERVER 1
typedef GUID IID;
#define REFIID const IID* const

extern "C" {
    HRESULT WINAPI CoCreateInstance(const GUID *,LPUNKNOWN,DWORD,REFIID,PVOID*);
    HRESULT WINAPI CoInitialize(PVOID);
    void WINAPI CoUninitialize(void);
};

#endif //VISTAASSOC_H
