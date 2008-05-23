/*****************************************************************************
 * filter.h : DirectShow access module for vlc
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id$
 *
 * Author: Gildas Bazin <gbazin@videolan.org>
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
#include <string>
#include <list>
#include <deque>
using namespace std;

#ifndef _MSC_VER
#   include <wtypes.h>
#   include <unknwn.h>
#   include <ole2.h>
#   include <limits.h>
#   ifdef _WINGDI_
#      undef _WINGDI_
#   endif
#   define _WINGDI_ 1
#   define AM_NOVTABLE
#   define _OBJBASE_H_
#   undef _X86_
#   ifndef _I64_MAX
#     define _I64_MAX LONG_LONG_MAX
#   endif
#   define LONGLONG long long
#endif

#include <dshow.h>

extern const GUID MEDIASUBTYPE_I420;
extern const GUID MEDIASUBTYPE_PREVIEW_VIDEO;

typedef struct VLCMediaSample
{
    IMediaSample *p_sample;
    mtime_t i_timestamp;

} VLCMediaSample;

class CaptureFilter;

void WINAPI FreeMediaType( AM_MEDIA_TYPE& mt );
HRESULT WINAPI CopyMediaType( AM_MEDIA_TYPE *pmtTarget,
                              const AM_MEDIA_TYPE *pmtSource );

int GetFourCCFromMediaType(const AM_MEDIA_TYPE &media_type);

/****************************************************************************
 * Declaration of our dummy directshow filter pin class
 ****************************************************************************/
class CapturePin: public IPin, public IMemInputPin
{
    friend class CaptureEnumMediaTypes;

    vlc_object_t *p_input;
    access_sys_t *p_sys;
    CaptureFilter  *p_filter;

    IPin *p_connected_pin;

    AM_MEDIA_TYPE *media_types;
    size_t media_type_count;

    AM_MEDIA_TYPE cx_media_type;

    deque<VLCMediaSample> samples_queue;

    long i_ref;

  public:
    CapturePin( vlc_object_t *_p_input, access_sys_t *p_sys,
                CaptureFilter *_p_filter,
                AM_MEDIA_TYPE *mt, size_t mt_count );
    virtual ~CapturePin();

    /* IUnknown methods */
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    /* IPin methods */
    STDMETHODIMP Connect( IPin * pReceivePin, const AM_MEDIA_TYPE *pmt );
    STDMETHODIMP ReceiveConnection( IPin * pConnector,
                                    const AM_MEDIA_TYPE *pmt );
    STDMETHODIMP Disconnect();
    STDMETHODIMP ConnectedTo( IPin **pPin );
    STDMETHODIMP ConnectionMediaType( AM_MEDIA_TYPE *pmt );
    STDMETHODIMP QueryPinInfo( PIN_INFO * pInfo );
    STDMETHODIMP QueryDirection( PIN_DIRECTION * pPinDir );
    STDMETHODIMP QueryId( LPWSTR * Id );
    STDMETHODIMP QueryAccept( const AM_MEDIA_TYPE *pmt );
    STDMETHODIMP EnumMediaTypes( IEnumMediaTypes **ppEnum );
    STDMETHODIMP QueryInternalConnections( IPin* *apPin, ULONG *nPin );
    STDMETHODIMP EndOfStream( void );

    STDMETHODIMP BeginFlush( void );
    STDMETHODIMP EndFlush( void );
    STDMETHODIMP NewSegment( REFERENCE_TIME tStart, REFERENCE_TIME tStop,
                             double dRate );

    /* IMemInputPin methods */
    STDMETHODIMP GetAllocator( IMemAllocator **ppAllocator );
    STDMETHODIMP NotifyAllocator(  IMemAllocator *pAllocator, BOOL bReadOnly );
    STDMETHODIMP GetAllocatorRequirements( ALLOCATOR_PROPERTIES *pProps );
    STDMETHODIMP Receive( IMediaSample *pSample );
    STDMETHODIMP ReceiveMultiple( IMediaSample **pSamples, long nSamples,
                                  long *nSamplesProcessed );
    STDMETHODIMP ReceiveCanBlock( void );

    /* Custom methods */
    HRESULT CustomGetSample( VLCMediaSample * );
    AM_MEDIA_TYPE &CustomGetMediaType();
};

/****************************************************************************
 * Declaration of our dummy directshow filter class
 ****************************************************************************/
class CaptureFilter : public IBaseFilter
{
    friend class CapturePin;

    vlc_object_t   *p_input;
    CapturePin     *p_pin;
    IFilterGraph   *p_graph;
    //AM_MEDIA_TYPE  media_type;
    FILTER_STATE   state;

    long i_ref;

  public:
    CaptureFilter( vlc_object_t *_p_input, access_sys_t *p_sys,
                   AM_MEDIA_TYPE *mt, size_t mt_count );
    virtual ~CaptureFilter();

    /* IUnknown methods */
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    /* IPersist method */
    STDMETHODIMP GetClassID(CLSID *pClsID);

    /* IMediaFilter methods */
    STDMETHODIMP GetState(DWORD dwMSecs, FILTER_STATE *State);
    STDMETHODIMP SetSyncSource(IReferenceClock *pClock);
    STDMETHODIMP GetSyncSource(IReferenceClock **pClock);
    STDMETHODIMP Stop();
    STDMETHODIMP Pause();
    STDMETHODIMP Run(REFERENCE_TIME tStart);

    /* IBaseFilter methods */
    STDMETHODIMP EnumPins( IEnumPins ** ppEnum );
    STDMETHODIMP FindPin( LPCWSTR Id, IPin ** ppPin );
    STDMETHODIMP QueryFilterInfo( FILTER_INFO * pInfo );
    STDMETHODIMP JoinFilterGraph( IFilterGraph * pGraph, LPCWSTR pName );
    STDMETHODIMP QueryVendorInfo( LPWSTR* pVendorInfo );

    /* Custom methods */
    CapturePin *CustomGetPin();
};

/****************************************************************************
 * Declaration of our dummy directshow enumpins class
 ****************************************************************************/
class CaptureEnumPins : public IEnumPins
{
    vlc_object_t *p_input;
    CaptureFilter  *p_filter;

    int i_position;
    long i_ref;

public:
    CaptureEnumPins( vlc_object_t *_p_input, CaptureFilter *_p_filter,
                     CaptureEnumPins *pEnumPins );
    virtual ~CaptureEnumPins();

    // IUnknown
    STDMETHODIMP QueryInterface( REFIID riid, void **ppv );
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IEnumPins
    STDMETHODIMP Next( ULONG cPins, IPin ** ppPins, ULONG * pcFetched );
    STDMETHODIMP Skip( ULONG cPins );
    STDMETHODIMP Reset();
    STDMETHODIMP Clone( IEnumPins **ppEnum );
};

/****************************************************************************
 * Declaration of our dummy directshow enummediatypes class
 ****************************************************************************/
class CaptureEnumMediaTypes : public IEnumMediaTypes
{
    vlc_object_t *p_input;
    CapturePin     *p_pin;
    AM_MEDIA_TYPE cx_media_type;

    size_t i_position;
    long i_ref;

public:
    CaptureEnumMediaTypes( vlc_object_t *_p_input, CapturePin *_p_pin,
                           CaptureEnumMediaTypes *pEnumMediaTypes );

    virtual ~CaptureEnumMediaTypes();

    // IUnknown
    STDMETHODIMP QueryInterface( REFIID riid, void **ppv );
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IEnumMediaTypes
    STDMETHODIMP Next( ULONG cMediaTypes, AM_MEDIA_TYPE ** ppMediaTypes,
                       ULONG * pcFetched );
    STDMETHODIMP Skip( ULONG cMediaTypes );
    STDMETHODIMP Reset();
    STDMETHODIMP Clone( IEnumMediaTypes **ppEnum );
};
