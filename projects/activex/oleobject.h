/*****************************************************************************
 * oleobject.h: ActiveX control for VLC
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

#ifndef __OLEOBJECT_H__
#define __OLEOBJECT_H__

class VLCOleObject : public IOleObject
{

public:

    VLCOleObject(VLCPlugin *p_instance);
    virtual ~VLCOleObject();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( NULL == ppv )
            return E_POINTER;
        if( (IID_IUnknown == riid)
         || (IID_IOleObject == riid) )
        {
            AddRef();
            *ppv = reinterpret_cast<LPVOID>(this);
            return NOERROR;
        }
        return _p_instance->pUnkOuter->QueryInterface(riid, ppv);
    };

    STDMETHODIMP_(ULONG) AddRef(void) { return _p_instance->pUnkOuter->AddRef(); };
    STDMETHODIMP_(ULONG) Release(void) { return _p_instance->pUnkOuter->Release(); };

    // IOleObject methods
    STDMETHODIMP Advise(IAdviseSink *, LPDWORD);
    STDMETHODIMP Close(DWORD);
    STDMETHODIMP DoVerb(LONG, LPMSG, LPOLECLIENTSITE, LONG, HWND, LPCRECT);
    STDMETHODIMP EnumAdvise(IEnumSTATDATA **);
    STDMETHODIMP EnumVerbs(IEnumOleVerb **);
    STDMETHODIMP GetClientSite(LPOLECLIENTSITE *);
    STDMETHODIMP GetClipboardData(DWORD, LPDATAOBJECT *);
    STDMETHODIMP GetExtent(DWORD, SIZEL *);
    STDMETHODIMP GetMiscStatus(DWORD, DWORD *);
    STDMETHODIMP GetMoniker(DWORD, DWORD, LPMONIKER *);
    STDMETHODIMP GetUserClassID(CLSID *);
    STDMETHODIMP GetUserType(DWORD, LPOLESTR *);
    STDMETHODIMP InitFromData(IDataObject *, BOOL, DWORD);
    STDMETHODIMP IsUpToDate(void);
    STDMETHODIMP SetClientSite(LPOLECLIENTSITE);
    STDMETHODIMP SetColorScheme(LOGPALETTE *);
    STDMETHODIMP SetExtent(DWORD, SIZEL *);
    STDMETHODIMP SetHostNames(LPCOLESTR, LPCOLESTR) ;
    STDMETHODIMP SetMoniker(DWORD, LPMONIKER);
    STDMETHODIMP Unadvise(DWORD);
    STDMETHODIMP Update(void);

private:

    HRESULT doInPlaceActivate(LPMSG lpMsg, LPOLECLIENTSITE pActiveSite, HWND hwndParent, LPCRECT lprcPosRect, BOOL uiActivate);

    IOleAdviseHolder *_p_advise_holder;
    IOleClientSite *_p_clientsite;

    VLCPlugin *_p_instance;
};

#endif

