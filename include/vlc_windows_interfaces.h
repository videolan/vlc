/*****************************************************************************
 * vlc_windows_interfaces.h : Vista/7 helpers
 ****************************************************************************
 *
 * Copyright (C) 2009-2010 VideoLAN
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

#include <commctrl.h>

#ifndef STDCALL
#define STDCALL
#endif

#define CLSCTX_INPROC_SERVER 1
typedef GUID IID;
#ifndef _REFIID_DEFINED
# define REFIID const IID* const
#endif

const GUID clsid_IApplication2 = { 0x1968106d,0xf3b5,0x44cf,{0x89,0x0e,0x11,0x6f,0xcb,0x9e,0xce,0xf1}};
const GUID IID_IApplicationAssociationRegistrationUI = {0x1f76a169,0xf994,0x40ac, {0x8f,0xc8,0x09,0x59,0xe8,0x87,0x47,0x10}};

const GUID clsid_ITaskbarList ={ 0x56FDF344,0xFD6D,0x11d0,{0x95,0x8A,0x00,0x60,0x97,0xC9,0xA0,0x90}};
const GUID IID_ITaskbarList3 = { 0xea1afb91,0x9e28,0x4b86,{0x90,0xe9,0x9e,0x9f,0x8a,0x5e,0xef,0xaf}};
#ifndef __IUnknown_INTERFACE_DEFINED__
#undef IUnknown
typedef struct _IUnknown IUnknown;
#endif
typedef struct _IApplicationAssociationRegistrationUI IApplicationAssociationRegistrationUI;
typedef struct _ITaskbarList3 ITaskbarList3;

typedef struct IUnknown_vt
{
    long (STDCALL *QueryInterface)(IUnknown *This, const GUID *riid,
                                   void **ppvObject);
    long (STDCALL *AddRef)(IUnknown *This);
    long (STDCALL *Release)(IUnknown *This);

} IUnknown_vt;
struct _IUnknown { IUnknown_vt* vt; };
#ifndef __IUnknown_INTERFACE_DEFINED__
typedef IUnknown *LPUNKNOWN;
#endif
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

typedef enum TBPFLAG
{
    TBPF_NOPROGRESS    = 0,
    TBPF_INDETERMINATE = 0x1,
    TBPF_NORMAL        = 0x2,
    TBPF_ERROR         = 0x4,
    TBPF_PAUSED        = 0x8
} TBPFLAG;

typedef enum TBATFLAG
{
    TBATF_USEMDITHUMBNAIL   = 0x1,
    TBATF_USEMDILIVEPREVIEW = 0x2
} TBATFLAG;

typedef struct tagTHUMBBUTTON
{
    DWORD dwMask;
    UINT iId;
    UINT iBitmap;
    HICON hIcon;
    //    WCHAR pszTip[ 260 ];
    wchar_t pszTip[ 260 ];
    DWORD dwFlags;
} THUMBBUTTON;

typedef struct tagTHUMBBUTTON *LPTHUMBBUTTON;

// THUMBBUTTON flags
#define THBF_ENABLED             0x0000
#define THBF_DISABLED            0x0001
#define THBF_DISMISSONCLICK      0x0002
#define THBF_NOBACKGROUND        0x0004
#define THBF_HIDDEN              0x0008

// THUMBBUTTON mask
#define THB_BITMAP          0x0001
#define THB_ICON            0x0002
#define THB_TOOLTIP         0x0004
#define THB_FLAGS           0x0008
#define THBN_CLICKED        0x1800

typedef struct ITaskbarList3Vtbl
{

    long ( STDCALL *QueryInterface )(ITaskbarList3 * This, REFIID riid, void **ppvObject);

    long ( STDCALL *AddRef )( ITaskbarList3 * This);

    long ( STDCALL *Release )( ITaskbarList3 * This);

    long ( STDCALL *HrInit )( ITaskbarList3 * This);

    long ( STDCALL *AddTab )( ITaskbarList3 * This, HWND hwnd);

    long ( STDCALL *DeleteTab )( ITaskbarList3 * This, HWND hwnd);

    long ( STDCALL *ActivateTab )( ITaskbarList3 * This, HWND hwnd);

    long ( STDCALL *SetActiveAlt )( ITaskbarList3 * This, HWND hwnd);

    long ( STDCALL *MarkFullscreenWindow )( ITaskbarList3 * This, HWND hwnd,
        BOOL fFullscreen);

    long ( STDCALL *SetProgressValue )( ITaskbarList3 * This, HWND hwnd,
        ULONGLONG ullCompleted, ULONGLONG ullTotal);

    long ( STDCALL *SetProgressState )( ITaskbarList3 * This, HWND hwnd,
        TBPFLAG tbpFlags);

    long ( STDCALL *RegisterTab )(  ITaskbarList3 * This, HWND hwndTab, HWND hwndMDI);

    long ( STDCALL *UnregisterTab )( ITaskbarList3 * This, HWND hwndTab);

    long ( STDCALL *SetTabOrder )( ITaskbarList3 * This, HWND hwndTab,
        HWND hwndInsertBefore);

    long ( STDCALL *SetTabActive )( ITaskbarList3 * This, HWND hwndTab,
        HWND hwndMDI, TBATFLAG tbatFlags);

    long ( STDCALL *ThumbBarAddButtons )( ITaskbarList3 * This, HWND hwnd,
        UINT cButtons, LPTHUMBBUTTON pButton);

    long ( STDCALL *ThumbBarUpdateButtons )( ITaskbarList3 * This, HWND hwnd,
        UINT cButtons, LPTHUMBBUTTON pButton);

    long ( STDCALL *ThumbBarSetImageList )( ITaskbarList3 * This, HWND hwnd,
        HIMAGELIST himl);

    long ( STDCALL *SetOverlayIcon )( ITaskbarList3 * This, HWND hwnd,
        HICON hIcon, LPCWSTR pszDescription);

    long ( STDCALL *SetThumbnailTooltip )( ITaskbarList3 * This, HWND hwnd,
        LPCWSTR pszTip);

    long ( STDCALL *SetThumbnailClip )( ITaskbarList3 * This, HWND hwnd,
        RECT *prcClip);

} ITaskbarList3Vtbl;

struct _ITaskbarList3 { ITaskbarList3Vtbl* vt; };
typedef ITaskbarList3 *LPTASKBARLIST3, *PTASKBARLIST3;


#ifdef __cplusplus
extern "C" {
#endif
    HRESULT WINAPI CoCreateInstance(const GUID *,LPUNKNOWN,DWORD,REFIID,PVOID*);
    HRESULT WINAPI CoInitialize(PVOID);
    void WINAPI CoUninitialize(void);
#ifdef __cplusplus
};
#endif

#endif //VISTAASSOC_H
