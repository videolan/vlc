/*****************************************************************************
 * crossbar.cpp : DirectShow access module for vlc
 *****************************************************************************
 * Copyright (C) 2002, 2004, 2009 VLC authors and VideoLAN
 *
 * Author: Damien Fouilleul <damien dot fouilleul at laposte dot net>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#ifndef _MSC_VER
    /* Work-around a bug in w32api-2.5 */
#   define QACONTAINERFLAGS QACONTAINERFLAGS_ANOTHERSOMETHINGELSE
#endif

#include "access.h"
#include "vlc_dshow.h"

namespace dshow {

// Helper function to associate a crossbar pin name with the type.
static const char * GetPhysicalPinName(long lType)
{
    switch (lType)
    {
    case PhysConn_Video_Tuner:            return "Video Tuner";
    case PhysConn_Video_Composite:        return "Video Composite";
    case PhysConn_Video_SVideo:           return "S-Video";
    case PhysConn_Video_RGB:              return "Video RGB";
    case PhysConn_Video_YRYBY:            return "Video YRYBY";
    case PhysConn_Video_SerialDigital:    return "Video Serial Digital";
    case PhysConn_Video_ParallelDigital:  return "Video Parallel Digital";
    case PhysConn_Video_SCSI:             return "Video SCSI";
    case PhysConn_Video_AUX:              return "Video AUX";
    case PhysConn_Video_1394:             return "Video 1394";
    case PhysConn_Video_USB:              return "Video USB";
    case PhysConn_Video_VideoDecoder:     return "Video Decoder";
    case PhysConn_Video_VideoEncoder:     return "Video Encoder";

    case PhysConn_Audio_Tuner:            return "Audio Tuner";
    case PhysConn_Audio_Line:             return "Audio Line";
    case PhysConn_Audio_Mic:              return "Audio Microphone";
    case PhysConn_Audio_AESDigital:       return "Audio AES/EBU Digital";
    case PhysConn_Audio_SPDIFDigital:     return "Audio S/PDIF";
    case PhysConn_Audio_SCSI:             return "Audio SCSI";
    case PhysConn_Audio_AUX:              return "Audio AUX";
    case PhysConn_Audio_1394:             return "Audio 1394";
    case PhysConn_Audio_USB:              return "Audio USB";
    case PhysConn_Audio_AudioDecoder:     return "Audio Decoder";

    default:                              return "Unknown Type";
    }
}
/*****************************************************************************
 * DeleteCrossbarRoutes
 *****************************************************************************/
void DeleteCrossbarRoutes( access_sys_t *p_sys )
{
    /* Remove crossbar filters from graph */
    for( int i = 0; i < p_sys->i_crossbar_route_depth; i++ )
    {
        p_sys->crossbar_routes[i].pXbar.Reset();
    }
    p_sys->i_crossbar_route_depth = 0;
}

/*****************************************************************************
 * RouteCrossbars (Does not AddRef the returned *Pin)
 *****************************************************************************/
static HRESULT GetCrossbarIPinAtIndex( IAMCrossbar *pXbar, LONG PinIndex,
                                       BOOL IsInputPin, ComPtr<IPin>* ppPin )
{
    LONG         cntInPins, cntOutPins;
    ComPtr<IPin> pP;
    ComPtr<IBaseFilter> pFilter;
    ComPtr<IEnumPins> pins;
    ULONG        n;

    if( !pXbar || !ppPin ) return E_POINTER;

    if( S_OK != pXbar->get_PinCounts(&cntOutPins, &cntInPins) ) return E_FAIL;

    LONG TrueIndex = IsInputPin ? PinIndex : PinIndex + cntInPins;

    if( pXbar->QueryInterface(IID_IBaseFilter, (void **)pFilter.GetAddressOf()) == S_OK )
    {
        if( SUCCEEDED(pFilter->EnumPins(pins.GetAddressOf())) )
        {
            LONG i = 0;
            while( pins->Next(1, pP.ReleaseAndGetAddressOf(), &n) == S_OK )
            {
                if( i == TrueIndex )
                {
                    *ppPin = pP;
                    break;
                }
                i++;
            }
        }
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
    ComPtr<IPin> pP;
    ComPtr<IBaseFilter> pFilter;
    ComPtr<IEnumPins>   pins;
    ULONG        n;
    BOOL         fOK = FALSE;

    if(!pXbar || !PinIndex || !pPin )
        return E_POINTER;

    if( S_OK != pXbar->get_PinCounts(&cntOutPins, &cntInPins) )
        return E_FAIL;

    if( pXbar->QueryInterface(IID_IBaseFilter, (void **)pFilter.GetAddressOf()) == S_OK )
    {
        if( SUCCEEDED(pFilter->EnumPins(&pins)) )
        {
            LONG i=0;

            while( pins->Next(1, pP.ReleaseAndGetAddressOf(), &n) == S_OK )
            {
                if( pPin == pP.Get() )
                {
                    *PinIndex = IsInputPin ? i : i - cntInPins;
                    fOK = TRUE;
                    break;
                }
                i++;
            }
        }
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

    ComPtr<IPin> p_output_pin;
    if( FAILED(p_input_pin->ConnectedTo(p_output_pin.GetAddressOf())) ) return S_FALSE;

    // It is connected, so now find out if the filter supports IAMCrossbar
    PIN_INFO pinInfo;
    if( FAILED(p_output_pin->QueryPinInfo(&pinInfo)) ||
        PINDIR_OUTPUT != pinInfo.dir )
    {
        return S_FALSE;
    }

    ComPtr<IAMCrossbar> pXbar;
    if( FAILED(pinInfo.pFilter->QueryInterface(IID_IAMCrossbar,
                                               (void **)pXbar.GetAddressOf())) )
    {
        pinInfo.pFilter->Release();
        return S_FALSE;
    }

    LONG inputPinCount, outputPinCount;
    if( FAILED(pXbar->get_PinCounts(&outputPinCount, &inputPinCount)) )
    {
        pinInfo.pFilter->Release();
        return S_FALSE;
    }

    LONG inputPinIndexRelated, outputPinIndexRelated;
    LONG inputPinPhysicalType = 0, outputPinPhysicalType;
    LONG inputPinIndex = 0, outputPinIndex;
    if( FAILED(GetCrossbarIndexFromIPin( pXbar.Get(), &outputPinIndex,
                                         FALSE, p_output_pin.Get() )) ||
        FAILED(pXbar->get_CrossbarPinInfo( FALSE, outputPinIndex,
                                           &outputPinIndexRelated,
                                           &outputPinPhysicalType )) )
    {
        pinInfo.pFilter->Release();
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

                msg_Dbg( p_this, "found existing route for output %ld (type %s) to input %ld (type %s)",
                         outputPinIndex, GetPhysicalPinName( outputPinPhysicalType ),
                         inputPinIndex, GetPhysicalPinName( inputPinPhysicalType ) );

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


        ComPtr<IPin> pPin;
        if( FAILED(GetCrossbarIPinAtIndex( pXbar.Get(), inputPinIndex,
                                           TRUE, &pPin ) ) ) continue;

        result = FindCrossbarRoutes( p_this, p_sys, pPin.Get(),
                                     physicalType, depth+1 );

        if( S_OK == result || (S_FALSE == result &&
            physicalType == inputPinPhysicalType &&
            (p_sys->i_crossbar_route_depth = depth+1) < MAX_CROSSBAR_DEPTH) )
        {
            // remember crossbar route
            p_sys->crossbar_routes[depth].pXbar = pXbar;
            p_sys->crossbar_routes[depth].VideoInputIndex = inputPinIndex;
            p_sys->crossbar_routes[depth].VideoOutputIndex = outputPinIndex;
            p_sys->crossbar_routes[depth].AudioInputIndex = inputPinIndexRelated;
            p_sys->crossbar_routes[depth].AudioOutputIndex = outputPinIndexRelated;

            msg_Dbg( p_this, "crossbar at depth %d, found route for "
                     "output %ld (type %s) to input %ld (type %s)", depth,
                     outputPinIndex, GetPhysicalPinName( outputPinPhysicalType ),
                     inputPinIndex, GetPhysicalPinName( inputPinPhysicalType ) );

            result = S_OK;
        }
    }
    pinInfo.pFilter->Release();

    return result;
}

} // namespace
