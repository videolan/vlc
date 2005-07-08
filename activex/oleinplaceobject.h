/*****************************************************************************
 * oleinplaceobject.h: ActiveX control for VLC
 *****************************************************************************
 * Copyright (C) 2005 VideoLAN (Centrale Réseaux) and its contributors
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

#ifndef __OLEINPLACEOBJECT_H__
#define __OLEINPLACEOBJECT_H__

class VLCOleInPlaceObject : public IOleInPlaceObject
{

public:

    VLCOleInPlaceObject(VLCPlugin *p_instance) : _p_instance(p_instance) {};
    virtual ~VLCOleInPlaceObject() {};

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if( (NULL != ppv)
         && (IID_IUnknown == riid)
         && (IID_IOleWindow == riid)
         && (IID_IOleInPlaceObject == riid) ) 
        {
            AddRef();
            *ppv = reinterpret_cast<LPVOID>(this);
            return NOERROR;
        }
        return _p_instance->QueryInterface(riid, ppv);
    };

    STDMETHODIMP_(ULONG) AddRef(void) { return _p_instance->AddRef(); };
    STDMETHODIMP_(ULONG) Release(void) { return _p_instance->Release(); };

    // IOleWindow methods
    STDMETHODIMP GetWindow(HWND *);
    STDMETHODIMP ContextSensitiveHelp(BOOL);

    // IOleInPlaceObject methods
    STDMETHODIMP InPlaceDeactivate(void);
    STDMETHODIMP UIDeactivate(void);
    STDMETHODIMP SetObjectRects(LPCRECT, LPCRECT);
    STDMETHODIMP ReactivateAndUndo(void);

private:

    VLCPlugin *_p_instance;
};

#endif

