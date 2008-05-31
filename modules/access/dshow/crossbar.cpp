/*****************************************************************************
 * crossbar.c : DirectShow access module for vlc
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id$
 *
 * Author: Damien Fouilleul <damien dot fouilleul at laposte dot net>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_vout.h>

#ifndef _MSC_VER
    /* Work-around a bug in w32api-2.5 */
#   define QACONTAINERFLAGS QACONTAINERFLAGS_ANOTHERSOMETHINGELSE
#endif

#include "common.h"

/*****************************************************************************
 * DeleteCrossbarRoutes
 *****************************************************************************/
void DeleteCrossbarRoutes( access_sys_t *p_sys )
{
    /* Remove crossbar filters from graph */
    for( int i = 0; i < p_sys->i_crossbar_route_depth; i++ )
    {
        p_sys->crossbar_routes[i].pXbar->Release();
    }
    p_sys->i_crossbar_route_depth = 0;
}

/*****************************************************************************
 * RouteCrossbars (Does not AddRef the returned *Pin)
 *****************************************************************************/
static HRESULT GetCrossbarIPinAtIndex( IAMCrossbar *pXbar, LONG PinIndex,
                                       BOOL IsInputPin, IPin ** ppPin )
{
    LONG         cntInPins, cntOutPins;
    IPin        *pP = 0;
    IBaseFilter *pFilter = NULL;
    IEnumPins   *pins=0;
    ULONG        n;

    if( !pXbar || !ppPin ) return E_POINTER;

    *ppPin = 0;

    if( S_OK != pXbar->get_PinCounts(&cntOutPins, &cntInPins) ) return E_FAIL;

    LONG TrueIndex = IsInputPin ? PinIndex : PinIndex + cntInPins;

    if( pXbar->QueryInterface(IID_IBaseFilter, (void **)&pFilter) == S_OK )
    {
        if( SUCCEEDED(pFilter->EnumPins(&pins)) )
        {
            LONG i = 0;
            while( pins->Next(1, &pP, &n) == S_OK )
            {
                pP->Release();
                if( i == TrueIndex )
                {
                    *ppPin = pP;
                    break;
                }
                i++;
            }
            pins->Release();
        }
        pFilter->Release();
    }

    return *ppPin ? S_OK : E_FAIL;
}

/*****************************************************************************
 * GetCrossbarIndexFromIPin: Find corresponding index of an IPin on a crossbar
 *****************************************************************************/
static HRESULT GetCrossbarIndexFromIPin( IAMCrossbar * pXbar, LONG * PinIndex,
                                         BOOL IsInputPin, IPin * pPin )
{
    LONG         cntInPins, cntOutPins;
    IPin        *pP = 0;
    IBaseFilter *pFilter = NULL;
    IEnumPins   *pins = 0;
    ULONG        n;
    BOOL         fOK = FALSE;

    if(!pXbar || !PinIndex || !pPin )
        return E_POINTER;

    if( S_OK != pXbar->get_PinCounts(&cntOutPins, &cntInPins) )
        return E_FAIL;

    if( pXbar->QueryInterface(IID_IBaseFilter, (void **)&pFilter) == S_OK )
    {
        if( SUCCEEDED(pFilter->EnumPins(&pins)) )
        {
            LONG i=0;

            while( pins->Next(1, &pP, &n) == S_OK )
            {
                pP->Release();
                if( pPin == pP )
                {
                    *PinIndex = IsInputPin ? i : i - cntInPins;
                    fOK = TRUE;
                    break;
                }
                i++;
            }
            pins->Release();
        }
        pFilter->Release();
    }

    return fOK ? S_OK : E_FAIL;
}

/*****************************************************************************
 * FindCrossbarRoutes
 *****************************************************************************/
HRESULT FindCrossbarRoutes( vlc_object_t *p_this, access_sys_t *p_sys,
                            IPin *p_input_pin, LONG physicalType, int depth )
{
    HRESULT result = S_FALSE;

    IPin *p_output_pin;
    if( FAILED(p_input_pin->ConnectedTo(&p_output_pin)) ) return S_FALSE;

    // It is connected, so now find out if the filter supports IAMCrossbar
    PIN_INFO pinInfo;
    if( FAILED(p_output_pin->QueryPinInfo(&pinInfo)) ||
        PINDIR_OUTPUT != pinInfo.dir )
    {
        p_output_pin->Release ();
        return S_FALSE;
    }

    IAMCrossbar *pXbar=0;
    if( FAILED(pinInfo.pFilter->QueryInterface(IID_IAMCrossbar,
                                               (void **)&pXbar)) )
    {
        pinInfo.pFilter->Release();
        p_output_pin->Release ();
        return S_FALSE;
    }

    LONG inputPinCount, outputPinCount;
    if( FAILED(pXbar->get_PinCounts(&outputPinCount, &inputPinCount)) )
    {
        pXbar->Release();
        pinInfo.pFilter->Release();
        p_output_pin->Release ();
        return S_FALSE;
    }

    LONG inputPinIndexRelated, outputPinIndexRelated;
    LONG inputPinPhysicalType = 0, outputPinPhysicalType;
    LONG inputPinIndex = 0, outputPinIndex;
    if( FAILED(GetCrossbarIndexFromIPin( pXbar, &outputPinIndex,
                                         FALSE, p_output_pin )) ||
        FAILED(pXbar->get_CrossbarPinInfo( FALSE, outputPinIndex,
                                           &outputPinIndexRelated,
                                           &outputPinPhysicalType )) )
    {
        pXbar->Release();
        pinInfo.pFilter->Release();
        p_output_pin->Release ();
        return S_FALSE;
    }

    /*
    ** if physical type is 0, then use default/existing route to physical connector
    */
    if( physicalType == 0 )
    {
        /* use following as default connector type if we fail to find an existing route */
        physicalType = PhysConn_Video_Tuner;
        if( SUCCEEDED(pXbar->get_IsRoutedTo(outputPinIndex, &inputPinIndex)) )
        {

            if( SUCCEEDED( pXbar->get_CrossbarPinInfo( TRUE,  inputPinIndex,
                           &inputPinIndexRelated, &inputPinPhysicalType )) )
            {
                // remember connector type
                physicalType = inputPinPhysicalType;
 
                msg_Dbg( p_this, "found existing route for output %ld (type %ld) to input %ld (type %ld)",
                         outputPinIndex, outputPinPhysicalType, inputPinIndex,
                         inputPinPhysicalType );
 
                // fall through to for loop, note 'inputPinIndex' is set to the pin we are looking for
                // hence, loop iteration should not wind back

            }
        }
        else {
            // reset to first pin for complete loop iteration
            inputPinIndex = 0;
        }
    }
 
    //
    // for all input pins
    //
    for( /* inputPinIndex has been set */ ; (S_OK != result) && (inputPinIndex < inputPinCount); ++inputPinIndex )
    {
        if( FAILED(pXbar->get_CrossbarPinInfo( TRUE,  inputPinIndex,
            &inputPinIndexRelated, &inputPinPhysicalType )) ) continue;

        // Is this pin matching required connector physical type?
        if( inputPinPhysicalType != physicalType ) continue;

        // Can we route it?
        if( FAILED(pXbar->CanRoute(outputPinIndex, inputPinIndex)) ) continue;
 
 
        IPin *pPin;
        if( FAILED(GetCrossbarIPinAtIndex( pXbar, inputPinIndex,
                                           TRUE, &pPin)) ) continue;

        result = FindCrossbarRoutes( p_this, p_sys, pPin,
                                     physicalType, depth+1 );

        if( S_OK == result || (S_FALSE == result &&
            physicalType == inputPinPhysicalType &&
            (p_sys->i_crossbar_route_depth = depth+1) < MAX_CROSSBAR_DEPTH) )
        {
            // hold on crossbar, will be released when graph is destroyed
            pXbar->AddRef();

            // remember crossbar route
            p_sys->crossbar_routes[depth].pXbar = pXbar;
            p_sys->crossbar_routes[depth].VideoInputIndex = inputPinIndex;
            p_sys->crossbar_routes[depth].VideoOutputIndex = outputPinIndex;
            p_sys->crossbar_routes[depth].AudioInputIndex = inputPinIndexRelated;
            p_sys->crossbar_routes[depth].AudioOutputIndex = outputPinIndexRelated;

            msg_Dbg( p_this, "crossbar at depth %d, found route for "
                     "output %ld (type %ld) to input %ld (type %ld)", depth,
                     outputPinIndex, outputPinPhysicalType, inputPinIndex,
                     inputPinPhysicalType );

            result = S_OK;
        }
    }

    pXbar->Release();
    pinInfo.pFilter->Release();
    p_output_pin->Release ();

    return result;
}
