/*****************************************************************************
 * filter.c : DirectShow access module for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: filter.cpp,v 1.1 2003/08/24 11:17:39 gbazin Exp $
 *
 * Author: Gildas Bazin <gbazin@netcourrier.com>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/vout.h>

#ifndef _MSC_VER
#   include <wtypes.h>
#   include <unknwn.h>
#   include <ole2.h>
#   include <limits.h>
#   define _WINGDI_ 1
#   define AM_NOVTABLE
#   define _OBJBASE_H_
#   undef _X86_
#   define _I64_MAX LONG_LONG_MAX
#   define LONGLONG long long
#endif

#include <dshow.h>

#include "filter.h"

#define DEBUG_DSHOW 1

/*****************************************************************************
 * DirectShow GUIDs.
 * Easier to define them hear as mingw doesn't provide them all.
 *****************************************************************************/
const GUID CLSID_SystemDeviceEnum = {0x62be5d10, 0x60eb, 0x11d0, {0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86}};
const GUID CLSID_VideoInputDeviceCategory = {0x860BB310,0x5D01,0x11d0,{0xBD,0x3B,0x00,0xA0,0xC9,0x11,0xCE,0x86}};
const GUID IID_IPropertyBag = {0x55272A00, 0x42CB, 0x11CE, {0x81, 0x35, 0x00, 0xAA, 0x00, 0x4B, 0xB8, 0x51}};
const GUID IID_ICreateDevEnum = {0x29840822, 0x5b84, 0x11d0, {0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86}};
const GUID IID_IFilterGraph = {0x56a8689f, 0x0ad4, 0x11ce, {0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID IID_IMediaControl = {0x56a868b1, 0x0ad4, 0x11ce, {0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID CLSID_FilterGraph = {0xe436ebb3, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};

const GUID IID_IUnknown = {0x00000000, 0x0000, 0x0000, {0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46}};
const GUID IID_IPersist = {0x0000010c, 0x0000, 0x0000, {0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46}};
const GUID IID_IMediaFilter = {0x56a86899, 0x0ad4, 0x11ce, {0xb0,0x3a, 0x00,0x20,0xaf,0x0b,0xa7,0x70}};
const GUID IID_IBaseFilter = {0x56a86895, 0x0ad4, 0x11ce, {0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID IID_IPin = {0x56a86891, 0x0ad4, 0x11ce, {0xb0,0x3a, 0x00,0x20,0xaf,0x0b,0xa7,0x70}};
const GUID IID_IMemInputPin = {0x56a8689d, 0x0ad4, 0x11ce, {0xb0,0x3a, 0x00,0x20,0xaf,0x0b,0xa7,0x70}};

const GUID IID_IEnumPins = {0x56a86892, 0x0ad4, 0x11ce, {0xb0,0x3a, 0x00,0x20,0xaf,0x0b,0xa7,0x70}};
const GUID IID_IEnumMediaTypes = {0x89c31040, 0x846b, 0x11ce, {0x97,0xd3, 0x00,0xaa,0x00,0x55,0x59,0x5a}};

/*
 * MEDIATYPEs and MEDIASUBTYPEs
 */
const GUID MEDIATYPE_Video = {0x73646976, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

const GUID MEDIASUBTYPE_RGB8 = {0xe436eb7a, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID MEDIASUBTYPE_RGB565 = {0xe436eb7b, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID MEDIASUBTYPE_RGB555 = {0xe436eb7c, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID MEDIASUBTYPE_RGB24 = {0xe436eb7d, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID MEDIASUBTYPE_RGB32 = {0xe436eb7e, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID MEDIASUBTYPE_ARGB32 = {0x773c9ac0, 0x3274, 0x11d0, {0xb7, 0x24, 0x0, 0xaa, 0x0, 0x6c, 0x1a, 0x1}};

const GUID MEDIASUBTYPE_YUYV = {0x56595559, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_Y411 = {0x31313459, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_Y41P = {0x50313459, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_YUY2 = {0x32595559, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_YVYU = {0x55595659, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_UYVY = {0x59565955, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_Y211 = {0x31313259, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_YV12 = {0x32315659, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

void WINAPI FreeMediaType( AM_MEDIA_TYPE& mt )
{
    if( mt.cbFormat != 0 )
    {
        CoTaskMemFree( (PVOID)mt.pbFormat );
        mt.cbFormat = 0;
        mt.pbFormat = NULL;
    }
    if( mt.pUnk != NULL )
    {
        mt.pUnk->Release();
        mt.pUnk = NULL;
    }
}

HRESULT WINAPI CopyMediaType( AM_MEDIA_TYPE *pmtTarget,
                              const AM_MEDIA_TYPE *pmtSource )
{
    *pmtTarget = *pmtSource;
    if( pmtSource->cbFormat != 0 )
    {
        pmtTarget->pbFormat = (PBYTE)CoTaskMemAlloc( pmtSource->cbFormat );
        if( pmtTarget->pbFormat == NULL )
        {
            pmtTarget->cbFormat = 0;
            return E_OUTOFMEMORY;
        }
        else
        {
            CopyMemory( (PVOID)pmtTarget->pbFormat, (PVOID)pmtSource->pbFormat,
                        pmtTarget->cbFormat );
        }
    }
    if( pmtTarget->pUnk != NULL )
    {
        pmtTarget->pUnk->AddRef();
    }

    return S_OK;
}

/****************************************************************************
 * Implementation of our dummy directshow filter pin class
 ****************************************************************************/

CapturePin::CapturePin( input_thread_t * _p_input, CaptureFilter *_p_filter )
  : p_input( _p_input ), p_filter( _p_filter ), p_connected_pin( NULL ),
    i_ref( 1 )
{
}

CapturePin::~CapturePin()
{
}

HRESULT CapturePin::CustomGetSample( VLCMediaSample *vlc_sample )
{
    if( samples_queue.size() )
    {
        *vlc_sample = samples_queue.back();
        samples_queue.pop_back();
        return S_OK;
    }
    return S_FALSE;
}

AM_MEDIA_TYPE CapturePin::CustomGetMediaType()
{
    return media_type;
}

/* IUnknown methods */
STDMETHODIMP CapturePin::QueryInterface(REFIID riid, void **ppv)
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::QueryInterface" );
#endif

    if( riid == IID_IUnknown ||
        riid == IID_IPin )
    {
        *ppv = (IPin *)this;
        return NOERROR;
    }
    if( riid == IID_IMemInputPin )
    {
        *ppv = (IMemInputPin *)this;
        return NOERROR;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
}
STDMETHODIMP_(ULONG) CapturePin::AddRef()
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::AddRef" );
#endif

    i_ref++;
    return NOERROR;
};
STDMETHODIMP_(ULONG) CapturePin::Release()
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::Release" );
#endif

    i_ref--;
    if( !i_ref ) delete this;

    return NOERROR;
};

/* IPin methods */
STDMETHODIMP CapturePin::Connect( IPin * pReceivePin,
                                  const AM_MEDIA_TYPE *pmt )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::Connect" );
#endif
    return E_NOTIMPL;
}
STDMETHODIMP CapturePin::ReceiveConnection( IPin * pConnector,
                                            const AM_MEDIA_TYPE *pmt )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::ReceiveConnection" );
#endif

    p_connected_pin = pConnector;
    p_connected_pin->AddRef();
    return CopyMediaType( &media_type, pmt );
}
STDMETHODIMP CapturePin::Disconnect()
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::Disconnect" );
#endif

    p_connected_pin->Release();
    p_connected_pin = NULL;
    FreeMediaType( media_type );
    return S_OK;
}
STDMETHODIMP CapturePin::ConnectedTo( IPin **pPin )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::ConnectedTo" );
#endif

    p_connected_pin->AddRef();
    *pPin = p_connected_pin;

    return S_OK;
}
STDMETHODIMP CapturePin::ConnectionMediaType( AM_MEDIA_TYPE *pmt )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::ConnectionMediaType" );
#endif

    return CopyMediaType( pmt, &media_type );
}
STDMETHODIMP CapturePin::QueryPinInfo( PIN_INFO * pInfo )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::QueryPinInfo" );
#endif

    pInfo->pFilter = p_filter;
    if( p_filter ) p_filter->AddRef();

    pInfo->achName[0] = L'\0';

    pInfo->dir = PINDIR_INPUT;

    return NOERROR;
}
STDMETHODIMP CapturePin::QueryDirection( PIN_DIRECTION * pPinDir )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::QueryDirection" );
#endif

    *pPinDir = PINDIR_INPUT;
    return NOERROR;
}
STDMETHODIMP CapturePin::QueryId( LPWSTR * Id )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::QueryId" );
#endif
    return E_NOTIMPL;
}
STDMETHODIMP CapturePin::QueryAccept( const AM_MEDIA_TYPE *pmt )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::QueryAccept" );
#endif
    return E_NOTIMPL;
}
STDMETHODIMP CapturePin::EnumMediaTypes( IEnumMediaTypes **ppEnum )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::EnumMediaTypes" );
#endif

    *ppEnum = new CaptureEnumMediaTypes( p_input, this, NULL );

    if( *ppEnum == NULL ) return E_OUTOFMEMORY;

    return NOERROR;
}
STDMETHODIMP CapturePin::QueryInternalConnections( IPin* *apPin, ULONG *nPin )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::QueryInternalConnections" );
#endif
    return E_NOTIMPL;
}
STDMETHODIMP CapturePin::EndOfStream( void )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::EndOfStream" );
#endif
    return E_NOTIMPL;
}
STDMETHODIMP CapturePin::BeginFlush( void )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::BeginFlush" );
#endif
    return E_NOTIMPL;
}
STDMETHODIMP CapturePin::EndFlush( void )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::EndFlush" );
#endif
    return E_NOTIMPL;
}
STDMETHODIMP CapturePin::NewSegment( REFERENCE_TIME tStart,
                                     REFERENCE_TIME tStop,
                                     double dRate )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::NewSegment" );
#endif
    return E_NOTIMPL;
}

/* IMemInputPin methods */
STDMETHODIMP CapturePin::GetAllocator( IMemAllocator **ppAllocator )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::GetAllocator" );
#endif

    return VFW_E_NO_ALLOCATOR;
}
STDMETHODIMP CapturePin::NotifyAllocator( IMemAllocator *pAllocator,
                                          BOOL bReadOnly )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::NotifyAllocator" );
#endif

    return S_OK;
}
STDMETHODIMP CapturePin::GetAllocatorRequirements( ALLOCATOR_PROPERTIES *pProps )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::GetAllocatorRequirements" );
#endif
    return E_NOTIMPL;
}
STDMETHODIMP CapturePin::Receive( IMediaSample *pSample )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::Receive" );
#endif

    //pSample->AddRef();
    mtime_t i_timestamp = mdate() * 10;
    VLCMediaSample vlc_sample = {pSample, i_timestamp};
    samples_queue.push_front( vlc_sample );

    /* Make sure we don't cache too many samples */
    if( samples_queue.size() > 10 )
    {
        vlc_sample = samples_queue.back();
        samples_queue.pop_back();
        msg_Dbg( p_input, "CapturePin::Receive trashing late input sample" );
        // vlc_sample.p_sample->Release();
    }

    return S_OK;
}
STDMETHODIMP CapturePin::ReceiveMultiple( IMediaSample **pSamples,
                                          long nSamples,
                                          long *nSamplesProcessed )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::ReceiveMultiple" );
#endif

    HRESULT hr = S_OK;

    *nSamplesProcessed = 0;
    while( nSamples-- > 0 )
    {
         hr = Receive( pSamples[*nSamplesProcessed] );
         if( hr != S_OK ) break;
         (*nSamplesProcessed)++;
    }
    return hr;
}
STDMETHODIMP CapturePin::ReceiveCanBlock( void )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::ReceiveCanBlock" );
#endif

    return S_FALSE; /* Thou shalt not block */
}

/****************************************************************************
 * Implementation of our dummy directshow filter class
 ****************************************************************************/

CaptureFilter::CaptureFilter( input_thread_t * _p_input )
  : p_input( _p_input ), p_pin( new CapturePin( _p_input, this ) ),
    i_ref( 1 )
{
}

CaptureFilter::~CaptureFilter()
{
    p_pin->Release();
}

/* IUnknown methods */
STDMETHODIMP CaptureFilter::QueryInterface( REFIID riid, void **ppv )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::QueryInterface" );
#endif

    if( riid == IID_IUnknown )
    {
        *ppv = (IUnknown *)this;
        return NOERROR;
    }
    if( riid == IID_IPersist )
    {
        *ppv = (IPersist *)this;
        return NOERROR;
    }
    if( riid == IID_IMediaFilter )
    {
        *ppv = (IMediaFilter *)this;
        return NOERROR;
    }
    if( riid == IID_IBaseFilter )
    {
        *ppv = (IBaseFilter *)this;
        return NOERROR;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
};
STDMETHODIMP_(ULONG) CaptureFilter::AddRef()
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::AddRef" );
#endif

    i_ref++;
    return NOERROR;
};
STDMETHODIMP_(ULONG) CaptureFilter::Release()
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::Release" );
#endif

    i_ref--;
    if( !i_ref ) delete this;

    return NOERROR;
};

/* IPersist method */
STDMETHODIMP CaptureFilter::GetClassID(CLSID *pClsID)
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::GetClassID" );
#endif
    return E_NOTIMPL;
};

/* IMediaFilter methods */
STDMETHODIMP CaptureFilter::GetState(DWORD dwMSecs, FILTER_STATE *State)
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::GetStat" );
#endif
    return E_NOTIMPL;
};
STDMETHODIMP CaptureFilter::SetSyncSource(IReferenceClock *pClock)
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::SetSyncSource" );
#endif

    return NOERROR;
};
STDMETHODIMP CaptureFilter::GetSyncSource(IReferenceClock **pClock)
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::GetSyncSource" );
#endif
    return E_NOTIMPL;
};
STDMETHODIMP CaptureFilter::Stop()
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::Stop" );
#endif
    return E_NOTIMPL;
};
STDMETHODIMP CaptureFilter::Pause()
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::Pause" );
#endif
    return E_NOTIMPL;
};
STDMETHODIMP CaptureFilter::Run(REFERENCE_TIME tStart)
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::Run" );
#endif
    return E_NOTIMPL;
};

/* IBaseFilter methods */
STDMETHODIMP CaptureFilter::EnumPins( IEnumPins ** ppEnum )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::EnumPins" );
#endif

    /* Create a new ref counted enumerator */
    *ppEnum = new CaptureEnumPins( p_input, this, NULL );
    return *ppEnum == NULL ? E_OUTOFMEMORY : NOERROR;
};
STDMETHODIMP CaptureFilter::FindPin( LPCWSTR Id, IPin ** ppPin )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::FindPin" );
#endif
    return E_NOTIMPL;
};
STDMETHODIMP CaptureFilter::QueryFilterInfo( FILTER_INFO * pInfo )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::QueryFilterInfo" );
#endif

    pInfo->achName[0] = L'\0';

    pInfo->pGraph = p_graph;
    if( p_graph ) p_graph->AddRef();

    return NOERROR;
};
STDMETHODIMP CaptureFilter::JoinFilterGraph( IFilterGraph * pGraph,
                                             LPCWSTR pName )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::JoinFilterGraph" );
#endif

    p_graph = pGraph;

    return NOERROR;
};
STDMETHODIMP CaptureFilter::QueryVendorInfo( LPWSTR* pVendorInfo )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::QueryVendorInfo" );
#endif
    return E_NOTIMPL;
};

/* Custom methods */
CapturePin *CaptureFilter::CustomGetPin()
{
    return p_pin;
}

/****************************************************************************
 * Implementation of our dummy directshow enumpins class
 ****************************************************************************/

CaptureEnumPins::CaptureEnumPins( input_thread_t * _p_input,
                                  CaptureFilter *_p_filter,
                                  CaptureEnumPins *pEnumPins )
  : p_input( _p_input ), p_filter( _p_filter ), i_ref( 1 )
{
    /* Hold a reference count on our filter */
    p_filter->AddRef();

    /* Are we creating a new enumerator */

    if( pEnumPins == NULL )
    {
        i_position = 0;
    }
    else
    {
        i_position = pEnumPins->i_position;
    }
}

CaptureEnumPins::~CaptureEnumPins()
{
    p_filter->Release();
}

/* IUnknown methods */
STDMETHODIMP CaptureEnumPins::QueryInterface( REFIID riid, void **ppv )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureEnumPins::QueryInterface" );
#endif

    if( riid == IID_IUnknown ||
        riid == IID_IEnumPins )
    {
        *ppv = (IEnumPins *)this;
        return NOERROR;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
};
STDMETHODIMP_(ULONG) CaptureEnumPins::AddRef()
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureEnumPins::AddRef" );
#endif

    i_ref++;
    return NOERROR;
};
STDMETHODIMP_(ULONG) CaptureEnumPins::Release()
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureEnumPins::Release" );
#endif

    i_ref--;
    if( !i_ref ) delete this;

    return NOERROR;
};

/* IEnumPins */
STDMETHODIMP CaptureEnumPins::Next( ULONG cPins, IPin ** ppPins,
                                    ULONG * pcFetched )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureEnumPins::Next" );
#endif

    *pcFetched = 0;

    if( i_position < 1 && cPins > 0 )
    {
        IPin *pPin = p_filter->CustomGetPin();
        *ppPins = pPin;
        pPin->AddRef();
        *pcFetched = 1;
        i_position++;
        return NOERROR;
    }

    return S_FALSE;
};
STDMETHODIMP CaptureEnumPins::Skip( ULONG cPins )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureEnumPins::Skip" );
#endif

    if( cPins > 1 )
    {
        return S_FALSE;
    }

    i_position += cPins;
    return NOERROR;
};
STDMETHODIMP CaptureEnumPins::Reset()
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureEnumPins::Reset" );
#endif

    i_position = 0;
    return S_OK;
};
STDMETHODIMP CaptureEnumPins::Clone( IEnumPins **ppEnum )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureEnumPins::Clone" );
#endif

    *ppEnum = new CaptureEnumPins( p_input, p_filter, this );
    if( *ppEnum == NULL ) return E_OUTOFMEMORY;

    return NOERROR;
};

/****************************************************************************
 * Implementation of our dummy directshow enummediatypes class
 ****************************************************************************/

CaptureEnumMediaTypes::CaptureEnumMediaTypes( input_thread_t * _p_input,
                                  CapturePin *_p_pin,
                                  CaptureEnumMediaTypes *pEnumMediaTypes )
  : p_input( _p_input ), p_pin( _p_pin ), i_ref( 1 )
{
    /* Hold a reference count on our filter */
    p_pin->AddRef();

    /* Are we creating a new enumerator */

    if( pEnumMediaTypes == NULL )
    {
        i_position = 0;
    }
    else
    {
        i_position = pEnumMediaTypes->i_position;
    }
}

CaptureEnumMediaTypes::~CaptureEnumMediaTypes()
{
    p_pin->Release();
}

/* IUnknown methods */
STDMETHODIMP CaptureEnumMediaTypes::QueryInterface( REFIID riid, void **ppv )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureEnumMediaTypes::QueryInterface" );
#endif

    if( riid == IID_IUnknown ||
        riid == IID_IEnumMediaTypes )
    {
        *ppv = (IEnumMediaTypes *)this;
        return NOERROR;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
};
STDMETHODIMP_(ULONG) CaptureEnumMediaTypes::AddRef()
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureEnumMediaTypes::AddRef" );
#endif

    i_ref++;
    return NOERROR;
};
STDMETHODIMP_(ULONG) CaptureEnumMediaTypes::Release()
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureEnumMediaTypes::Release" );
#endif

    i_ref--;
    if( !i_ref ) delete this;

    return NOERROR;
};

/* IEnumMediaTypes */
STDMETHODIMP CaptureEnumMediaTypes::Next( ULONG cMediaTypes,
                                          AM_MEDIA_TYPE ** ppMediaTypes,
                                          ULONG * pcFetched )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureEnumMediaTypes::Next" );
#endif

    *pcFetched = 0;

#if 0
    if( i_position < 1 && cMediaTypes > 0 )
    {
        IPin *pPin = p_pin->CustomGetPin();
        *ppMediaTypes = pPin;
        pPin->AddRef();
        *pcFetched = 1;
        i_position++;
        return NOERROR;
    }
#endif

    return S_FALSE;
};
STDMETHODIMP CaptureEnumMediaTypes::Skip( ULONG cMediaTypes )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureEnumMediaTypes::Skip" );
#endif

    if( cMediaTypes > 1 )
    {
        return S_FALSE;
    }

    i_position += cMediaTypes;
    return NOERROR;
};
STDMETHODIMP CaptureEnumMediaTypes::Reset()
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureEnumMediaTypes::Reset" );
#endif

    i_position = 0;
    return S_OK;
};
STDMETHODIMP CaptureEnumMediaTypes::Clone( IEnumMediaTypes **ppEnum )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureEnumMediaTypes::Clone" );
#endif

    *ppEnum = new CaptureEnumMediaTypes( p_input, p_pin, this );
    if( *ppEnum == NULL ) return E_OUTOFMEMORY;

    return NOERROR;
};
