/*****************************************************************************
 * bdagraph.h : DirectShow BDA graph builder header for vlc
 *****************************************************************************
 * Copyright ( C ) 2007 the VideoLAN team
 *
 * Author: Ken Self <kens@campoz.fslife.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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
#include <queue>

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
#   define _I64_MAX LONG_LONG_MAX
#   define LONGLONG long long
/* Work-around a bug in w32api-2.5 */
/* #   define QACONTAINERFLAGS QACONTAINERFLAGS_SOMETHINGELSE */
#endif
/* Needed to call CoInitializeEx */
#define _WIN32_DCOM

#include <dshow.h>
#include <comcat.h>
#include "bdadefs.h"
#include "bda.h"

/* The main class for building the filter graph */
class BDAGraph : public ISampleGrabberCB
{
public:
    BDAGraph( access_t* p_access );
    virtual ~BDAGraph();

    int SubmitATSCTuneRequest();
    int SubmitDVBTTuneRequest();
    int SubmitDVBCTuneRequest();
    int SubmitDVBSTuneRequest();
    long GetBufferSize();
    long ReadBuffer( long* l_buffer_len, BYTE* p_buff );

private:
    /* ISampleGrabberCB methods */
    STDMETHODIMP_( ULONG ) AddRef( ) { return 1; }
    STDMETHODIMP_( ULONG ) Release( ) { return 2; }
    STDMETHODIMP QueryInterface( REFIID riid, void** p_p_object )
        {return E_NOTIMPL;  }
    STDMETHODIMP SampleCB( double d_time, IMediaSample* p_sample );
    STDMETHODIMP BufferCB( double d_time, BYTE* p_buffer, long l_buffer_len );

    access_t* p_access;
    CLSID     guid_network_type;
    long      l_tuner_used;        /* Index of the Tuning Device */
    /* registration number for the RunningObjectTable */
    DWORD     d_graph_register;

    queue<IMediaSample*> queue_sample;
    queue<IMediaSample*> queue_buffer;
    BOOL b_ready;

    IMediaControl*  p_media_control;
    IGraphBuilder*  p_filter_graph;
    ITuningSpace*   p_tuning_space;
    ITuneRequest*   p_tune_request;

    ICreateDevEnum* p_system_dev_enum;
    IBaseFilter*    p_network_provider;
    IScanningTuner* p_scanning_tuner;
    IBaseFilter*    p_tuner_device;
    IBaseFilter*    p_capture_device;
    IBaseFilter*    p_sample_grabber;
    IBaseFilter*    p_mpeg_demux;
    IBaseFilter*    p_transport_info;
    ISampleGrabber* p_grabber;

    HRESULT CreateTuneRequest( );
    HRESULT Build( );
    HRESULT FindFilter( REFCLSID clsid, long* i_moniker_used,
        IBaseFilter* p_upstream, IBaseFilter** p_p_downstream );
    HRESULT Connect( IBaseFilter* p_filter_upstream,
        IBaseFilter* p_filter_downstream );
    HRESULT Start( );
    HRESULT Destroy( );
    HRESULT Register( );
    void Deregister( );
};
