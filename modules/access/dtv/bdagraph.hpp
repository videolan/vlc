/*****************************************************************************
 * bdagraph.hpp : DirectShow BDA graph builder header for vlc
 *****************************************************************************
 * Copyright ( C ) 2007 the VideoLAN team
 *
 * Author: Ken Self <kenself(at)optusnet(dot)com(dot)au>
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

#include <wtypes.h>
#include <unknwn.h>
#include <ole2.h>
#include <limits.h>

/* FIXME: mingw.org doesn't define secure versions of
 * http://msdn.microsoft.com/en-us/library/f30dzcf6.aspxu */
#define NO_DSHOW_STRSAFE
#include <dshow.h>

#include <comcat.h>
#include "dtv/bdadefs.h"
#include <stdexcept>


// TBS tuner extension headers

typedef struct _tbs_plp_info // PLP info for TBS tuners struct
{

    unsigned char plpId; //The Rel PLPID need to set

    unsigned char plpCount; //PLP count number

    unsigned char plpResered1;//

    unsigned char plpResered2;//  memory size set to 4

    unsigned char plpIdList[256];//store the Rel PLPID

} TBS_PLP_INFO,*p_TBS_PLP_INFO;

const GUID KSPROPSETID_BdaTunerExtensionProperties = {0xfaa8f3e5, 0x31d4, 0x4e41, {0x88, 0xef, 0xd9, 0xeb, 0x71, 0x6f, 0x6e, 0xc9}};

DWORD KSPROPERTY_BDA_PLPINFO = 22;

// End of TBS tuner extension headers


struct ComContext
{
    ComContext( int mode )
    {
        if( FAILED( CoInitializeEx( NULL, mode ) ) )
            throw std::runtime_error( "CoInitializeEx failed" );
    }
    ~ComContext()
    {
        CoUninitialize();
    }
};

class BDAOutput
{
public:
    BDAOutput( vlc_object_t * );
    ~BDAOutput();

    void    Push( block_t * );
    ssize_t Pop(void *, size_t, int);
    void    Empty();

private:
    vlc_object_t *p_access;
    vlc_mutex_t   lock;
    vlc_cond_t    wait;
    block_t      *p_first;
    block_t     **pp_next;
};

/* The main class for building the filter graph */
class BDAGraph : public ISampleGrabberCB
{
public:
    BDAGraph( vlc_object_t * );
    virtual ~BDAGraph();

    /* */
    int SubmitTuneRequest(void);
    unsigned EnumSystems(void);
    int SetInversion(int);
    float GetSignalStrength(void);
    float GetSignalNoiseRatio(void);

    int SetCQAM(long);
    int SetATSC(long);
    int SetDVBT(long, uint32_t, uint32_t, long, int, uint32_t, int);
    int SetDVBT2(long, uint32_t, long, int, uint32_t, int);
    int SetDVBC(long, const char *, long);
    int SetDVBS(long, long, uint32_t, int, char, long, long, long);

    /* */
    ssize_t Pop(void *, size_t, int);

private:
    /* ISampleGrabberCB methods */
    ULONG ul_cbrc;
    STDMETHODIMP_( ULONG ) AddRef( ) { return ++ul_cbrc; }
    STDMETHODIMP_( ULONG ) Release( ) { return --ul_cbrc; }
    STDMETHODIMP QueryInterface( REFIID /*riid*/, void** /*p_p_object*/ )
        { return E_NOTIMPL; }
    STDMETHODIMP SampleCB( double d_time, IMediaSample* p_sample );
    STDMETHODIMP BufferCB( double d_time, BYTE* p_buffer, long l_buffer_len );

    vlc_object_t *p_access;
    CLSID     guid_network_type;   /* network type in use */
    long      l_tuner_used;        /* Index of the Tuning Device in use */
    unsigned  systems;             /* bitmask of all tuners' network types */

    /* registration number for the RunningObjectTable */
    DWORD     d_graph_register;

    BDAOutput       output;

    IMediaControl*  p_media_control;
    IGraphBuilder*  p_filter_graph;
    ITuningSpace*               p_tuning_space;
    ITuneRequest*               p_tune_request;

#if 0
    IDigitalCableTuningSpace*   p_cqam_tuning_space;
    IATSCTuningSpace*           p_atsc_tuning_space;
#endif

    ICreateDevEnum* p_system_dev_enum;
    IBaseFilter*    p_network_provider;
    IBaseFilter*    p_tuner_device;
    IBaseFilter*    p_capture_device;
    IBaseFilter*    p_sample_grabber;
    IBaseFilter*    p_mpeg_demux;
    IBaseFilter*    p_transport_info;
    IScanningTuner* p_scanning_tuner;
    ISampleGrabber* p_grabber;

    HRESULT SetUpTuner( REFCLSID guid_this_network_type );
    HRESULT GetNextNetworkType( CLSID* guid_this_network_type );
    HRESULT Build( );
    HRESULT Check( REFCLSID guid_this_network_type );
    HRESULT GetFilterName( IBaseFilter* p_filter, char** psz_bstr_name );
    HRESULT GetPinName( IPin* p_pin, char** psz_bstr_name );
    IPin* FindPinOnFilter( IBaseFilter* pBaseFilter, const char* pPinName);
    unsigned GetSystem( REFCLSID clsid );
    HRESULT ListFilters( REFCLSID this_clsid );
    HRESULT FindFilter( REFCLSID clsid, long* i_moniker_used,
        IBaseFilter* p_upstream, IBaseFilter** p_p_downstream);
    HRESULT Connect( IBaseFilter* p_filter_upstream,
        IBaseFilter* p_filter_downstream);
    HRESULT Start( );
    HRESULT Destroy( );
    HRESULT Register( );
    void Deregister( );
};
