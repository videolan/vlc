/*****************************************************************************
 * persiststreaminit.cpp: ActiveX control for VLC
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

#include "plugin.h"
#include "persiststreaminit.h"

using namespace std;

STDMETHODIMP VLCPersistStreamInit::GetClassID(LPCLSID pClsID)
{
    if( NULL == pClsID )
        return E_POINTER;

    *pClsID = _p_instance->getClassID();

    return S_OK;
};

STDMETHODIMP VLCPersistStreamInit::InitNew(void)
{
    return _p_instance->onInit();
};

STDMETHODIMP VLCPersistStreamInit::Load(LPSTREAM pStm)
{
    if( NULL == pStm )
        return E_POINTER;

    return _p_instance->onInit();
};

STDMETHODIMP VLCPersistStreamInit::Save(LPSTREAM pStm, BOOL fClearDirty)
{
    if( NULL == pStm )
        return E_POINTER;

    return S_OK;
};

STDMETHODIMP VLCPersistStreamInit::IsDirty(void)
{
    return S_FALSE;
};

STDMETHODIMP VLCPersistStreamInit::GetSizeMax(ULARGE_INTEGER *pcbSize)
{
    pcbSize->QuadPart = 0ULL;

    return S_OK;
};

