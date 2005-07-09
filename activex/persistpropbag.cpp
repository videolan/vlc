/*****************************************************************************
 * persistpropbag.cpp: ActiveX control for VLC
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
#include "persistpropbag.h"

#include "utils.h"
#include "oleobject.h"

using namespace std;

STDMETHODIMP VLCPersistPropertyBag::GetClassID(LPCLSID pClsID)
{
    if( NULL == pClsID )
        return E_POINTER;

    *pClsID = _p_instance->getClassID();

    return S_OK;
};

STDMETHODIMP VLCPersistPropertyBag::InitNew(void)
{
    return _p_instance->onInit();
};

STDMETHODIMP VLCPersistPropertyBag::Load(LPPROPERTYBAG pPropBag, LPERRORLOG pErrorLog)
{
    if( NULL == pPropBag )
        return E_POINTER;

    HRESULT hr = _p_instance->onInit();
    if( FAILED(hr) )
        return hr;

    VARIANT value;

    V_VT(&value) = VT_BSTR;
    if( S_OK == pPropBag->Read(OLESTR("filename"), &value, pErrorLog) )
    {
        char *src = CStrFromBSTR(_p_instance->getCodePage(), V_BSTR(&value));
        if( NULL != src )
        {
            _p_instance->setSourceURL(src);
            free(src);
        }
        VariantClear(&value);
    }

    V_VT(&value) = VT_BSTR;
    if( S_OK == pPropBag->Read(OLESTR("src"), &value, pErrorLog) )
    {
        char *src = CStrFromBSTR(_p_instance->getCodePage(), V_BSTR(&value));
        if( NULL != src )
        {
            _p_instance->setSourceURL(src);
            free(src);
        }
        VariantClear(&value);
    }

    V_VT(&value) = VT_BOOL;
    if( S_OK == pPropBag->Read(OLESTR("autoplay"), &value, pErrorLog) )
    {
        _p_instance->setAutoStart(V_BOOL(&value) != VARIANT_FALSE);
        VariantClear(&value);
    }

    V_VT(&value) = VT_BOOL;
    if( S_OK == pPropBag->Read(OLESTR("autostart"), &value, pErrorLog) )
    {
        _p_instance->setAutoStart(V_BOOL(&value) != VARIANT_FALSE);
        VariantClear(&value);
    }

    V_VT(&value) = VT_BOOL;
    if( S_OK == pPropBag->Read(OLESTR("loop"), &value, pErrorLog) )
    {
        _p_instance->setLoopMode(V_BOOL(&value) != VARIANT_FALSE);
        VariantClear(&value);
    }

    V_VT(&value) = VT_BOOL;
    if( S_OK == pPropBag->Read(OLESTR("mute"), &value, pErrorLog) )
    {
        _p_instance->setMute(V_BOOL(&value) != VARIANT_FALSE);
        VariantClear(&value);
    }

    V_VT(&value) = VT_BOOL;
    if( S_OK == pPropBag->Read(OLESTR("showdisplay"), &value, pErrorLog) )
    {
        _p_instance->setVisible(V_BOOL(&value) != VARIANT_FALSE);
        VariantClear(&value);
    }

    return _p_instance->onLoad();
};

STDMETHODIMP VLCPersistPropertyBag::Save(LPPROPERTYBAG pPropBag, BOOL fClearDirty, BOOL fSaveAllProperties)
{
    if( NULL == pPropBag )
        return E_POINTER;

    return S_OK;
};

