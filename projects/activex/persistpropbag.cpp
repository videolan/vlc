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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
    HRESULT hr = _p_instance->onInit();
    if( FAILED(hr) )
        return hr;

    if( NULL == pPropBag )
        return E_INVALIDARG;

    VARIANT value;

    V_VT(&value) = VT_BSTR;
    if( S_OK == pPropBag->Read(OLESTR("mrl"), &value, pErrorLog) )
    {
        _p_instance->setMRL(V_BSTR(&value));
        VariantClear(&value);
    }
    else
    {
        /*
        ** try alternative syntax
        */
        V_VT(&value) = VT_BSTR;
        if( S_OK == pPropBag->Read(OLESTR("src"), &value, pErrorLog) )
        {
            _p_instance->setMRL(V_BSTR(&value));
            VariantClear(&value);
        }
        else
        {
            V_VT(&value) = VT_BSTR;
            if( S_OK == pPropBag->Read(OLESTR("filename"), &value, pErrorLog) )
            {
                _p_instance->setMRL(V_BSTR(&value));
                VariantClear(&value);
            }
        }
    }

    V_VT(&value) = VT_BOOL;
    if( S_OK == pPropBag->Read(OLESTR("autoplay"), &value, pErrorLog) )
    {
        _p_instance->setAutoPlay(V_BOOL(&value) != VARIANT_FALSE);
        VariantClear(&value);
    }
    else
    {
        /*
        ** try alternative syntax
        */
        V_VT(&value) = VT_BOOL;
        if( S_OK == pPropBag->Read(OLESTR("autostart"), &value, pErrorLog) )
        {
            _p_instance->setAutoPlay(V_BOOL(&value) != VARIANT_FALSE);
            VariantClear(&value);
        }
    }

    V_VT(&value) = VT_BOOL;
    if( S_OK == pPropBag->Read(OLESTR("toolbar"), &value, pErrorLog) )
    {
        _p_instance->setShowToolbar(V_BOOL(&value) != VARIANT_FALSE);
        VariantClear(&value);
    }

    SIZEL size = _p_instance->getExtent();
    V_VT(&value) = VT_I4;
    if( S_OK == pPropBag->Read(OLESTR("extentwidth"), &value, pErrorLog) )
    {
         size.cx = V_I4(&value);
        VariantClear(&value);
    }
    V_VT(&value) = VT_I4;
    if( S_OK == pPropBag->Read(OLESTR("extentheight"), &value, pErrorLog) )
    {
         size.cy = V_I4(&value);
        VariantClear(&value);
    }
    _p_instance->setExtent(size);

    V_VT(&value) = VT_BOOL;
    if( S_OK == pPropBag->Read(OLESTR("autoloop"), &value, pErrorLog) )
    {
        _p_instance->setAutoLoop(V_BOOL(&value) != VARIANT_FALSE);
        VariantClear(&value);
    }
    else
    {
        /*
        ** try alternative syntax
        */
        V_VT(&value) = VT_BOOL;
        if( S_OK == pPropBag->Read(OLESTR("loop"), &value, pErrorLog) )
        {
            _p_instance->setAutoLoop(V_BOOL(&value) != VARIANT_FALSE);
            VariantClear(&value);
        }
    }

    V_VT(&value) = VT_BOOL;
    if( S_OK == pPropBag->Read(OLESTR("mute"), &value, pErrorLog) )
    {
        _p_instance->setMute(V_BOOL(&value) != VARIANT_FALSE);
        VariantClear(&value);
    }

    V_VT(&value) = VT_BOOL;
    if( S_OK == pPropBag->Read(OLESTR("visible"), &value, pErrorLog) )
    {
        _p_instance->setVisible(V_BOOL(&value) != VARIANT_FALSE);
        VariantClear(&value);
    }
    else
    {
        /*
        ** try alternative syntax
        */
        V_VT(&value) = VT_BOOL;
        if( S_OK == pPropBag->Read(OLESTR("showdisplay"), &value, pErrorLog) )
        {
            _p_instance->setVisible(V_BOOL(&value) != VARIANT_FALSE);
            VariantClear(&value);
        }
    }

    V_VT(&value) = VT_I4;
    if( S_OK == pPropBag->Read(OLESTR("volume"), &value, pErrorLog) )
    {
        _p_instance->setVolume(V_I4(&value));
        VariantClear(&value);
    }

    V_VT(&value) = VT_I4;
    if( S_OK == pPropBag->Read(OLESTR("starttime"), &value, pErrorLog) )
    {
        _p_instance->setStartTime(V_I4(&value));
        VariantClear(&value);
    }

    V_VT(&value) = VT_BSTR;
    if( S_OK == pPropBag->Read(OLESTR("baseurl"), &value, pErrorLog) )
    {
        _p_instance->setBaseURL(V_BSTR(&value));
        VariantClear(&value);
    }

    V_VT(&value) = VT_I4;
    if( S_OK == pPropBag->Read(OLESTR("backcolor"), &value, pErrorLog) )
    {
        _p_instance->setBackColor(V_I4(&value));
        VariantClear(&value);
    }
    else
    {
        /*
        ** try alternative syntax
        */
        V_VT(&value) = VT_BSTR;
        if( S_OK == pPropBag->Read(OLESTR("bgcolor"), &value, pErrorLog) )
        {
            long backcolor;
            if( swscanf(V_BSTR(&value), L"#%lX", &backcolor) )
            {
                _p_instance->setBackColor(backcolor);
            }
            VariantClear(&value);
        }
    }

    return _p_instance->onLoad();
};

STDMETHODIMP VLCPersistPropertyBag::Save(LPPROPERTYBAG pPropBag, BOOL fClearDirty, BOOL fSaveAllProperties)
{
    if( NULL == pPropBag )
        return E_INVALIDARG;

    VARIANT value;

    VariantInit(&value);

    V_VT(&value) = VT_BOOL;
    V_BOOL(&value) = _p_instance->getAutoLoop()? VARIANT_TRUE : VARIANT_FALSE;
    pPropBag->Write(OLESTR("AutoLoop"), &value);
    VariantClear(&value);

    V_VT(&value) = VT_BOOL;
    V_BOOL(&value) = _p_instance->getAutoPlay()? VARIANT_TRUE : VARIANT_FALSE;
    pPropBag->Write(OLESTR("AutoPlay"), &value);
    VariantClear(&value);

    V_VT(&value) = VT_BOOL;
    V_BOOL(&value) = _p_instance->getShowToolbar()? VARIANT_TRUE : VARIANT_FALSE;
    pPropBag->Write(OLESTR("Toolbar"), &value);
    VariantClear(&value);

    SIZEL size = _p_instance->getExtent();
    V_VT(&value) = VT_I4;
    V_I4(&value) = size.cx;
    pPropBag->Write(OLESTR("ExtentWidth"), &value);
    V_I4(&value) = size.cy;
    pPropBag->Write(OLESTR("ExtentHeight"), &value);

    V_VT(&value) = VT_BSTR;
    V_BSTR(&value) = SysAllocStringLen(_p_instance->getMRL(),
                            SysStringLen(_p_instance->getMRL()));
    pPropBag->Write(OLESTR("MRL"), &value);
    VariantClear(&value);

    V_VT(&value) = VT_BOOL;
    V_BOOL(&value) = _p_instance->getVisible()? VARIANT_TRUE : VARIANT_FALSE;
    pPropBag->Write(OLESTR("Visible"), &value);
    VariantClear(&value);

    V_VT(&value) = VT_I4;
    V_I4(&value) = _p_instance->getVolume();
    pPropBag->Write(OLESTR("Volume"), &value);
    VariantClear(&value);

    V_VT(&value) = VT_I4;
    V_I4(&value) = _p_instance->getStartTime();
    pPropBag->Write(OLESTR("StartTime"), &value);
    VariantClear(&value);

    V_VT(&value) = VT_BSTR;
    V_BSTR(&value) = SysAllocStringLen(_p_instance->getBaseURL(),
                            SysStringLen(_p_instance->getBaseURL()));
    pPropBag->Write(OLESTR("BaseURL"), &value);
    VariantClear(&value);

    V_VT(&value) = VT_I4;
    V_I4(&value) = _p_instance->getBackColor();
    pPropBag->Write(OLESTR("BackColor"), &value);
    VariantClear(&value);

    if( fClearDirty )
        _p_instance->setDirty(FALSE);

    return S_OK;
};
