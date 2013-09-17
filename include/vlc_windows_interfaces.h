/*****************************************************************************
 * vlc_windows_interfaces.h : Replacement for incomplete MinGW headers
 ****************************************************************************
 *
 * Copyright (C) 2009-2010 VideoLAN
 *
 * Authors: Geoffroy Couprie <geal@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef MINGW_WORKAROUNDS_H
#define MINGW_WORKAROUNDS_H

#ifdef __MINGW32__
# include <_mingw.h>
#endif

#ifdef __MINGW64_VERSION_MAJOR /* mingw.org lacks this header */
# include <shobjidl.h>
#endif

#include <commctrl.h>
#include <basetyps.h>
#include <objbase.h>

/* rpcndr.h defines small not only for idl */
#undef small

/* mingw.org fails to define this */
#ifndef __ITaskbarList3_INTERFACE_DEFINED__
#define __ITaskbarList3_INTERFACE_DEFINED__
const GUID CLSID_TaskbarList ={ 0x56FDF344,0xFD6D,0x11d0,{0x95,0x8A,0x00,0x60,0x97,0xC9,0xA0,0x90}};
const GUID IID_ITaskbarList3 = { 0xea1afb91,0x9e28,0x4b86,{0x90,0xe9,0x9e,0x9f,0x8a,0x5e,0xef,0xaf}};


typedef enum TBPFLAG
{
    TBPF_NOPROGRESS    = 0,
    TBPF_INDETERMINATE = 0x1,
    TBPF_NORMAL        = 0x2,
    TBPF_ERROR         = 0x4,
    TBPF_PAUSED        = 0x8
} TBPFLAG;

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

typedef enum THUMBBUTTONMASK {
    THB_BITMAP  = 0x1,
    THB_ICON    = 0x2,
    THB_TOOLTIP = 0x4,
    THB_FLAGS   = 0x8
} THUMBBUTTONMASK;

typedef enum THUMBBUTTONFLAGS {
    THBF_ENABLED        = 0x0,
    THBF_DISABLED       = 0x1,
    THBF_DISMISSONCLICK = 0x2,
    THBF_NOBACKGROUND   = 0x4,
    THBF_HIDDEN         = 0x8,
    THBF_NONINTERACTIVE = 0x10
} THUMBBUTTONFLAGS;

#ifdef __cplusplus
interface ITaskbarList : public IUnknown {
public:
    virtual HRESULT WINAPI HrInit(void) = 0;
    virtual HRESULT WINAPI AddTab(HWND hwnd) = 0;
    virtual HRESULT WINAPI DeleteTab(HWND hwnd) = 0;
    virtual HRESULT WINAPI ActivateTab(HWND hwnd) = 0;
    virtual HRESULT WINAPI SetActiveAlt(HWND hwnd) = 0;
};

interface ITaskbarList2 : public ITaskbarList {
public:
    virtual HRESULT WINAPI MarkFullscreenWindow(HWND hwnd,WINBOOL fFullscreen) = 0;
};

interface ITaskbarList3 : public ITaskbarList2
{
    virtual HRESULT STDMETHODCALLTYPE SetProgressValue(
        HWND hwnd,
        ULONGLONG ullCompleted,
        ULONGLONG ullTotal) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetProgressState(
        HWND hwnd,
        TBPFLAG tbpFlags) = 0;

    virtual HRESULT STDMETHODCALLTYPE RegisterTab(
        HWND hwndTab,
        HWND hwndMDI) = 0;

    virtual HRESULT STDMETHODCALLTYPE UnregisterTab(
        HWND hwndTab) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetTabOrder(
        HWND hwndTab,
        HWND hwndInsertBefore) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetTabActive(
        HWND hwndTab,
        HWND hwndMDI,
        DWORD dwReserved) = 0;

    virtual HRESULT STDMETHODCALLTYPE ThumbBarAddButtons(
        HWND hwnd,
        UINT cButtons,
        LPTHUMBBUTTON pButton) = 0;

    virtual HRESULT STDMETHODCALLTYPE ThumbBarUpdateButtons(
        HWND hwnd,
        UINT cButtons,
        LPTHUMBBUTTON pButton) = 0;

    virtual HRESULT STDMETHODCALLTYPE ThumbBarSetImageList(
        HWND hwnd,
        HIMAGELIST himl) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetOverlayIcon(
        HWND hwnd,
        HICON hIcon,
        LPCWSTR pszDescription) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetThumbnailTooltip(
        HWND hwnd,
        LPCWSTR pszTip) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetThumbnailClip(
        HWND hwnd,
        RECT *prcClip) = 0;

};

#else /* !__cplusplus */

struct ITaskbarList3Vtbl;
struct ITaskbarList3 { struct ITaskbarList3Vtbl* lpVtbl; };
typedef struct ITaskbarList3 ITaskbarList3;

struct ITaskbarList3Vtbl
{

    long ( WINAPI *QueryInterface )(ITaskbarList3 * This, REFIID riid, void **ppvObject);

    long ( WINAPI *AddRef )(ITaskbarList3 *This);

    long ( WINAPI *Release )(ITaskbarList3 *This);

    long ( WINAPI *HrInit )(ITaskbarList3 *This);

    long ( WINAPI *AddTab )(ITaskbarList3 *This, HWND hwnd);

    long ( WINAPI *DeleteTab )(ITaskbarList3 *This, HWND hwnd);

    long ( WINAPI *ActivateTab )(ITaskbarList3 *This, HWND hwnd);

    long ( WINAPI *SetActiveAlt )(ITaskbarList3 *This, HWND hwnd);

    long ( WINAPI *MarkFullscreenWindow )(ITaskbarList3 *This, HWND hwnd,
        BOOL fFullscreen);

    long ( WINAPI *SetProgressValue )(ITaskbarList3 *This, HWND hwnd,
        ULONGLONG ullCompleted, ULONGLONG ullTotal);

    long ( WINAPI *SetProgressState )(ITaskbarList3 *This, HWND hwnd,
        TBPFLAG tbpFlags);

    long ( WINAPI *RegisterTab )( ITaskbarList3 *This, HWND hwndTab, HWND hwndMDI);

    long ( WINAPI *UnregisterTab )(ITaskbarList3 *This, HWND hwndTab);

    long ( WINAPI *SetTabOrder )(ITaskbarList3 *This, HWND hwndTab,
        HWND hwndInsertBefore);

    long ( WINAPI *SetTabActive )(ITaskbarList3 *This, HWND hwndTab,
        HWND hwndMDI, DWORD dwReserved);

    long ( WINAPI *ThumbBarAddButtons )(ITaskbarList3 *This, HWND hwnd,
        UINT cButtons, LPTHUMBBUTTON pButton);

    long ( WINAPI *ThumbBarUpdateButtons )(ITaskbarList3 *This, HWND hwnd,
        UINT cButtons, LPTHUMBBUTTON pButton);

    long ( WINAPI *ThumbBarSetImageList )(ITaskbarList3 *This, HWND hwnd,
        HIMAGELIST himl);

    long ( WINAPI *SetOverlayIcon )(ITaskbarList3 *This, HWND hwnd,
        HICON hIcon, LPCWSTR pszDescription);

    long ( WINAPI *SetThumbnailTooltip )(ITaskbarList3 *This, HWND hwnd,
        LPCWSTR pszTip);

    long ( WINAPI *SetThumbnailClip )(ITaskbarList3 *This, HWND hwnd,
        RECT *prcClip);

};

#endif /* __cplusplus */
#endif /* __ITaskbarList3_INTERFACE_DEFINED__ */

/* mingw-w64 also fails to define these as of 2.0.1 */

#ifndef THBN_CLICKED
# define THBN_CLICKED        0x1800
#endif

#endif //MINGW_WORKAROUNDS_H
