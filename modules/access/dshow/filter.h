/*****************************************************************************
 * filter.h : DirectShow access module for vlc:
 * CapturePin, CaptureFilter, CaptureEnumPins implementations
 *****************************************************************************
 * Copyright (C) 2002-2004, 2008 VLC authors and VideoLAN
 *
 * Author: Gildas Bazin <gbazin@videolan.org>
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

#include <deque>

namespace dshow {

struct VLCMediaSample
{
    ComPtr<IMediaSample> p_sample;
    vlc_tick_t i_timestamp;
};

/* */
void WINAPI FreeMediaType( AM_MEDIA_TYPE& mt );
HRESULT WINAPI CopyMediaType( AM_MEDIA_TYPE *pmtTarget,
                              const AM_MEDIA_TYPE *pmtSource );

int GetFourCCFromMediaType(const AM_MEDIA_TYPE &media_type);

/****************************************************************************
 * Declaration of our dummy directshow filter pin class
 ****************************************************************************/
class CaptureFilter;
class CapturePin: public IPin, public IMemInputPin
{
    friend class CaptureEnumMediaTypes;

    vlc_object_t *p_input;
    access_sys_t *p_sys;
    // Don't store this filter as a ComPtr to avoid a circular reference.
    // p_filter is the parent filter, and already has a refcounter pointer to this CapturePin
    // instance
    CaptureFilter* p_filter;

    ComPtr<IPin> p_connected_pin;

    AM_MEDIA_TYPE *media_types;
    size_t media_type_count;

    AM_MEDIA_TYPE cx_media_type;

    std::deque<VLCMediaSample> samples_queue;

    long i_ref;

  public:
    CapturePin( vlc_object_t *_p_input, access_sys_t *p_sys,
                CaptureFilter* _p_filter,
                AM_MEDIA_TYPE *mt, size_t mt_count );

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
    HRESULT CustomGetSamples( std::deque<VLCMediaSample> &external_queue );

    AM_MEDIA_TYPE &CustomGetMediaType();

private:
    virtual ~CapturePin();
};

/****************************************************************************
 * Declaration of our dummy directshow filter class
 ****************************************************************************/
class CaptureFilter : public IBaseFilter
{
    friend class CapturePin;

    vlc_object_t   *p_input;
    ComPtr<CapturePin>   p_pin;
    ComPtr<IFilterGraph> p_graph;
    //AM_MEDIA_TYPE  media_type;
    FILTER_STATE   state;

    long i_ref;

  public:
    CaptureFilter( vlc_object_t *_p_input, access_sys_t *p_sys,
                   AM_MEDIA_TYPE *mt, size_t mt_count );

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
    ComPtr<CapturePin>& CustomGetPin();

private:
    virtual ~CaptureFilter();
};

/****************************************************************************
 * Declaration of our dummy directshow enumpins class
 ****************************************************************************/
class CaptureEnumPins : public IEnumPins
{
    vlc_object_t *p_input;
    ComPtr<CaptureFilter> p_filter;

    int i_position;
    long i_ref;

public:
    CaptureEnumPins( vlc_object_t *_p_input, ComPtr<CaptureFilter> _p_filter,
                     ComPtr<CaptureEnumPins> pEnumPins );

    // IUnknown
    STDMETHODIMP QueryInterface( REFIID riid, void **ppv );
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IEnumPins
    STDMETHODIMP Next( ULONG cPins, IPin ** ppPins, ULONG * pcFetched );
    STDMETHODIMP Skip( ULONG cPins );
    STDMETHODIMP Reset();
    STDMETHODIMP Clone( IEnumPins **ppEnum );

private:
    virtual ~CaptureEnumPins();
};

/****************************************************************************
 * Declaration of our dummy directshow enummediatypes class
 ****************************************************************************/
class CaptureEnumMediaTypes : public IEnumMediaTypes
{
    vlc_object_t *p_input;
    ComPtr<CapturePin> p_pin;
    AM_MEDIA_TYPE cx_media_type;

    size_t i_position;
    long i_ref;

public:
    CaptureEnumMediaTypes( vlc_object_t *_p_input, ComPtr<CapturePin> _p_pin,
                           CaptureEnumMediaTypes *pEnumMediaTypes );

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

private:
    virtual ~CaptureEnumMediaTypes();
};

} // namespace
