/*****************************************************************************
 * bdagraph.cpp : DirectShow BDA graph for vlc
 *****************************************************************************
 * Copyright( C ) 2007 the VideoLAN team
 *
 * Author: Ken Self <kenself(at)optusnet(dot)com(dot)au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 *( at your option ) any later version.
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
#include "bdagraph.h"
#include <ctype.h>

/****************************************************************************
 * Interfaces for calls from C
 ****************************************************************************/
extern "C" {

    void dvb_newBDAGraph( access_t* p_access )
    {
        p_access->p_sys->p_bda_module = new BDAGraph( p_access );
    };

    void dvb_deleteBDAGraph( access_t* p_access )
    {
        delete p_access->p_sys->p_bda_module;
    };

    int dvb_SubmitATSCTuneRequest( access_t* p_access )
    {
        if( p_access->p_sys->p_bda_module )
            return p_access->p_sys->p_bda_module->SubmitATSCTuneRequest();
        return VLC_EGENERIC;
    };

    int dvb_SubmitDVBTTuneRequest( access_t* p_access )
    {
        if( p_access->p_sys->p_bda_module )
            return p_access->p_sys->p_bda_module->SubmitDVBTTuneRequest();
        return VLC_EGENERIC;
    };

    int dvb_SubmitDVBCTuneRequest( access_t* p_access )
    {
        if( p_access->p_sys->p_bda_module )
            return p_access->p_sys->p_bda_module->SubmitDVBCTuneRequest();
        return VLC_EGENERIC;
    };

    int dvb_SubmitDVBSTuneRequest( access_t* p_access )
    {
        if( p_access->p_sys->p_bda_module )
            return p_access->p_sys->p_bda_module->SubmitDVBSTuneRequest();
        return VLC_EGENERIC;
    };

    block_t *dvb_Pop( access_t* p_access )
    {
        if( p_access->p_sys->p_bda_module )
            return p_access->p_sys->p_bda_module->Pop();
        return NULL;
    };
};

/*****************************************************************************
* BDAOutput
*****************************************************************************/
BDAOutput::BDAOutput( access_t *p_access ) :
    p_access(p_access), p_first(NULL), pp_next(&p_first)
{
    vlc_mutex_init( &lock );
    vlc_cond_init( &wait );
}
BDAOutput::~BDAOutput()
{
    Empty();
    vlc_mutex_destroy( &lock );
    vlc_cond_destroy( &wait );
}
void BDAOutput::Push( block_t *p_block )
{
    vlc_mutex_locker l( &lock );

    block_ChainLastAppend( &pp_next, p_block );
    vlc_cond_signal( &wait );
}
block_t *BDAOutput::Pop()
{
    vlc_mutex_locker l( &lock );

    if( !p_first )
        vlc_cond_timedwait( &wait, &lock, mdate() + 250*1000 );

    block_t *p_ret = p_first;

    p_first = NULL;
    pp_next = &p_first;

    return p_ret;
}
void BDAOutput::Empty()
{
    vlc_mutex_locker l( &lock );

    if( p_first )
        block_ChainRelease( p_first );
    p_first = NULL;
    pp_next = &p_first;
}

/*****************************************************************************
* Constructor
*****************************************************************************/
BDAGraph::BDAGraph( access_t* p_this ):
    p_access( p_this ),
    guid_network_type(GUID_NULL),
    l_tuner_used(-1),
    d_graph_register( 0 ),
    output( p_this )
{
    p_tuning_space = NULL;
    p_tune_request = NULL;
    p_media_control = NULL;
    p_filter_graph = NULL;
    p_system_dev_enum = NULL;
    p_network_provider = p_tuner_device = p_capture_device = NULL;
    p_sample_grabber = p_mpeg_demux = p_transport_info = NULL;
    p_scanning_tuner = NULL;
    p_grabber = NULL;

    /* Initialize COM - MS says to use CoInitializeEx in preference to
     * CoInitialize */
    CoInitializeEx( 0, COINIT_APARTMENTTHREADED );
}

/*****************************************************************************
* Destructor
*****************************************************************************/
BDAGraph::~BDAGraph()
{
    Destroy();
    CoUninitialize();
}

/*****************************************************************************
* Submit an ATSC Tune Request
*****************************************************************************/
int BDAGraph::SubmitATSCTuneRequest()
{
    HRESULT hr = S_OK;
    class localComPtr
    {
        public:
        IATSCChannelTuneRequest* p_atsc_tune_request;
        IATSCLocator* p_atsc_locator;
        localComPtr(): p_atsc_tune_request(NULL), p_atsc_locator(NULL) {};
        ~localComPtr()
        {
            if( p_atsc_tune_request )
                p_atsc_tune_request->Release();
            if( p_atsc_locator )
                p_atsc_locator->Release();
        }
    } l;
    long l_major_channel, l_minor_channel, l_physical_channel;
    long l_frequency;

    l_major_channel     = var_GetInteger( p_access, "dvb-major-channel" );
    l_minor_channel     = var_GetInteger( p_access, "dvb-minor-channel" );
    l_physical_channel  = var_GetInteger( p_access, "dvb-physical-channel" );
    l_frequency         = var_GetInteger( p_access, "dvb-frequency" );

    guid_network_type = CLSID_ATSCNetworkProvider;
    hr = CreateTuneRequest();
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitATSCTuneRequest: "\
            "Cannot create Tuning Space: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = p_tune_request->QueryInterface( IID_IATSCChannelTuneRequest,
        (void**)&l.p_atsc_tune_request );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitATSCTuneRequest: "\
            "Cannot QI for IATSCChannelTuneRequest: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }
    hr = ::CoCreateInstance( CLSID_ATSCLocator, 0, CLSCTX_INPROC,
                             IID_IATSCLocator, (void**)&l.p_atsc_locator );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitATSCTuneRequest: "\
            "Cannot create the ATSC locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = S_OK;
    if( l_frequency > 0 )
        hr = l.p_atsc_locator->put_CarrierFrequency( l_frequency );
    if( l_major_channel > 0 )
        hr = l.p_atsc_tune_request->put_Channel( l_major_channel );
    if( SUCCEEDED( hr ) && l_minor_channel > 0 )
        hr = l.p_atsc_tune_request->put_MinorChannel( l_minor_channel );
    if( SUCCEEDED( hr ) && l_physical_channel > 0 )
        hr = l.p_atsc_locator->put_PhysicalChannel( l_physical_channel );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitATSCTuneRequest: "\
            "Cannot set tuning parameters: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = p_tune_request->put_Locator( l.p_atsc_locator );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitATSCTuneRequest: "\
            "Cannot put the locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    /* Build and Run the Graph. If a Tuner device is in use the graph will
     * fail to run. Repeated calls to build will check successive tuner
     * devices */
    do
    {
        hr = Build();
        if( FAILED( hr ) )
        {
            msg_Warn( p_access, "SubmitATSCTuneRequest: "\
                "Cannot Build the Graph: hr=0x%8lx", hr );
            return VLC_EGENERIC;
        }
        hr = Start();
    }
    while( hr != S_OK );

    return VLC_SUCCESS;
}

/*****************************************************************************
* Submit a DVB-T Tune Request
******************************************************************************/
int BDAGraph::SubmitDVBTTuneRequest()
{
    HRESULT hr = S_OK;
    class localComPtr
    {
        public:
        IDVBTuneRequest* p_dvbt_tune_request;
        IDVBTLocator* p_dvbt_locator;
        IDVBTuningSpace2* p_dvb_tuning_space;
        localComPtr(): p_dvbt_tune_request(NULL), p_dvbt_locator(NULL),
           p_dvb_tuning_space(NULL) {};
        ~localComPtr()
        {
            if( p_dvbt_tune_request )
                p_dvbt_tune_request->Release();
            if( p_dvbt_locator )
                p_dvbt_locator->Release();
            if( p_dvb_tuning_space )
                p_dvb_tuning_space->Release();
        }
    } l;
    long l_frequency, l_bandwidth, l_hp_fec, l_lp_fec, l_guard;
    long l_transmission, l_hierarchy;
    BinaryConvolutionCodeRate i_hp_fec, i_lp_fec;
    GuardInterval             i_guard;
    TransmissionMode          i_transmission;
    HierarchyAlpha            i_hierarchy;

    l_frequency    = var_GetInteger( p_access, "dvb-frequency" ) / 1000;
    l_bandwidth    = var_GetInteger( p_access, "dvb-bandwidth" );
    l_hp_fec       = var_GetInteger( p_access, "dvb-code-rate-hp" );
    l_lp_fec       = var_GetInteger( p_access, "dvb-code-rate-lp" );
    l_guard        = var_GetInteger( p_access, "dvb-guard" );
    l_transmission = var_GetInteger( p_access, "dvb-transmission" );
    l_hierarchy    = var_GetInteger( p_access, "dvb-hierarchy" );

    switch( l_hp_fec )
    {
    case 1:
        i_hp_fec = BDA_BCC_RATE_1_2; break;
    case 2:
        i_hp_fec = BDA_BCC_RATE_2_3; break;
    case 3:
        i_hp_fec = BDA_BCC_RATE_3_4; break;
    case 4:
        i_hp_fec = BDA_BCC_RATE_5_6; break;
    case 5:
        i_hp_fec = BDA_BCC_RATE_7_8;break;
    default:
        i_hp_fec = BDA_BCC_RATE_NOT_SET;
    }

    switch( l_lp_fec )
    {
    case 1:
        i_lp_fec = BDA_BCC_RATE_1_2; break;
    case 2:
        i_lp_fec = BDA_BCC_RATE_2_3; break;
    case 3:
        i_lp_fec = BDA_BCC_RATE_3_4; break;
    case 4:
        i_lp_fec = BDA_BCC_RATE_5_6; break;
    case 5:
        i_lp_fec = BDA_BCC_RATE_7_8; break;
    default:
        i_lp_fec = BDA_BCC_RATE_NOT_SET;
    }

    switch( l_guard )
    {
    case 32:
        i_guard = BDA_GUARD_1_32; break;
    case 16:
        i_guard = BDA_GUARD_1_16; break;
    case 8:
        i_guard = BDA_GUARD_1_8; break;
    case 4:
        i_guard = BDA_GUARD_1_4; break;
    default:
        i_guard = BDA_GUARD_NOT_SET;
    }

    switch( l_transmission )
    {
    case 2:
        i_transmission = BDA_XMIT_MODE_2K; break;
    case 8:
        i_transmission = BDA_XMIT_MODE_8K; break;
    default:
        i_transmission = BDA_XMIT_MODE_NOT_SET;
    }

    switch( l_hierarchy )
    {
    case 1:
        i_hierarchy = BDA_HALPHA_1; break;
    case 2:
        i_hierarchy = BDA_HALPHA_2; break;
    case 4:
        i_hierarchy = BDA_HALPHA_4; break;
    default:
        i_hierarchy = BDA_HALPHA_NOT_SET;
    }

    guid_network_type = CLSID_DVBTNetworkProvider;
    hr = CreateTuneRequest();
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitDVBTTuneRequest: "\
            "Cannot create Tune Request: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = p_tune_request->QueryInterface( IID_IDVBTuneRequest,
        (void**)&l.p_dvbt_tune_request );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitDVBTTuneRequest: "\
            "Cannot QI for IDVBTuneRequest: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }
    l.p_dvbt_tune_request->put_ONID( -1 );
    l.p_dvbt_tune_request->put_SID( -1 );
    l.p_dvbt_tune_request->put_TSID( -1 );

    hr = ::CoCreateInstance( CLSID_DVBTLocator, 0, CLSCTX_INPROC,
        IID_IDVBTLocator, (void**)&l.p_dvbt_locator );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitDVBTTuneRequest: "\
            "Cannot create the DVBT Locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }
    hr = p_tuning_space->QueryInterface( IID_IDVBTuningSpace2,
        (void**)&l.p_dvb_tuning_space );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitDVBTTuneRequest: "\
            "Cannot QI for IDVBTuningSpace2: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = S_OK;
    hr = l.p_dvb_tuning_space->put_SystemType( DVB_Terrestrial );

    if( SUCCEEDED( hr ) && l_frequency > 0 )
        hr = l.p_dvbt_locator->put_CarrierFrequency( l_frequency );
    if( SUCCEEDED( hr ) && l_bandwidth > 0 )
        hr = l.p_dvbt_locator->put_Bandwidth( l_bandwidth );
    if( SUCCEEDED( hr ) && i_hp_fec != BDA_BCC_RATE_NOT_SET )
        hr = l.p_dvbt_locator->put_InnerFECRate( i_hp_fec );
    if( SUCCEEDED( hr ) && i_lp_fec != BDA_BCC_RATE_NOT_SET )
        hr = l.p_dvbt_locator->put_LPInnerFECRate( i_lp_fec );
    if( SUCCEEDED( hr ) && i_guard != BDA_GUARD_NOT_SET )
        hr = l.p_dvbt_locator->put_Guard( i_guard );
    if( SUCCEEDED( hr ) && i_transmission != BDA_XMIT_MODE_NOT_SET )
        hr = l.p_dvbt_locator->put_Mode( i_transmission );
    if( SUCCEEDED( hr ) && i_hierarchy != BDA_HALPHA_NOT_SET )
        hr = l.p_dvbt_locator->put_HAlpha( i_hierarchy );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitDVBTTuneRequest: "\
            "Cannot set tuning parameters on Locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = p_tune_request->put_Locator( l.p_dvbt_locator );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitDVBTTuneRequest: "\
            "Cannot put the locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    /* Build and Run the Graph. If a Tuner device is in use the graph will
     * fail to run. Repeated calls to build will check successive tuner
     * devices */
    do
    {
        hr = Build();
        if( FAILED( hr ) )
        {
            msg_Warn( p_access, "SubmitDVBTTuneRequest: "\
                "Cannot Build the Graph: hr=0x%8lx", hr );
            return VLC_EGENERIC;
        }
        hr = Start();
    }
    while( hr != S_OK );

    return VLC_SUCCESS;
}

/*****************************************************************************
* Submit a DVB-C Tune Request
******************************************************************************/
int BDAGraph::SubmitDVBCTuneRequest()
{
    HRESULT hr = S_OK;

    class localComPtr
    {
        public:
        IDVBTuneRequest* p_dvbc_tune_request;
        IDVBCLocator* p_dvbc_locator;
        IDVBTuningSpace2* p_dvb_tuning_space;

        localComPtr(): p_dvbc_tune_request(NULL), p_dvbc_locator(NULL),
                       p_dvb_tuning_space(NULL) {};
        ~localComPtr()
        {
            if( p_dvbc_tune_request )
                p_dvbc_tune_request->Release();
            if( p_dvbc_locator )
                p_dvbc_locator->Release();
            if( p_dvb_tuning_space )
                p_dvb_tuning_space->Release();
        }
    } l;

    long l_frequency, l_symbolrate;
    int  i_qam;
    ModulationType i_qam_mod;

    l_frequency  = var_GetInteger( p_access, "dvb-frequency" ) / 1000;
    l_symbolrate = var_GetInteger( p_access, "dvb-srate" );
    i_qam        = var_GetInteger( p_access, "dvb-modulation" );

    switch( i_qam )
    {
    case 16:
        i_qam_mod = BDA_MOD_16QAM; break;
    case 32:
        i_qam_mod = BDA_MOD_32QAM; break;
    case 64:
        i_qam_mod = BDA_MOD_64QAM; break;
    case 128:
        i_qam_mod = BDA_MOD_128QAM; break;
    case 256:
        i_qam_mod = BDA_MOD_256QAM; break;
    default:
        i_qam_mod = BDA_MOD_NOT_SET;
    }

    guid_network_type = CLSID_DVBCNetworkProvider;
    hr = CreateTuneRequest();
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitDVBCTuneRequest: "\
            "Cannot create Tune Request: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = p_tune_request->QueryInterface( IID_IDVBTuneRequest,
        (void**)&l.p_dvbc_tune_request );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitDVBCTuneRequest: "\
            "Cannot QI for IDVBTuneRequest: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }
    l.p_dvbc_tune_request->put_ONID( -1 );
    l.p_dvbc_tune_request->put_SID( -1 );
    l.p_dvbc_tune_request->put_TSID( -1 );

    hr = ::CoCreateInstance( CLSID_DVBCLocator, 0, CLSCTX_INPROC,
        IID_IDVBCLocator, (void**)&l.p_dvbc_locator );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitDVBCTuneRequest: "\
            "Cannot create the DVB-C Locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }
    hr = p_tuning_space->QueryInterface( IID_IDVBTuningSpace2,
        (void**)&l.p_dvb_tuning_space );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitDVBCTuneRequest: "\
            "Cannot QI for IDVBTuningSpace2: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = S_OK;
    hr = l.p_dvb_tuning_space->put_SystemType( DVB_Cable );

    if( SUCCEEDED( hr ) && l_frequency > 0 )
        hr = l.p_dvbc_locator->put_CarrierFrequency( l_frequency );
    if( SUCCEEDED( hr ) && l_symbolrate > 0 )
        hr = l.p_dvbc_locator->put_SymbolRate( l_symbolrate );
    if( SUCCEEDED( hr ) && i_qam_mod != BDA_MOD_NOT_SET )
        hr = l.p_dvbc_locator->put_Modulation( i_qam_mod );

    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitDVBCTuneRequest: "\
            "Cannot set tuning parameters on Locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = p_tune_request->put_Locator( l.p_dvbc_locator );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitDVBCTuneRequest: "\
            "Cannot put the locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    /* Build and Run the Graph. If a Tuner device is in use the graph will
     * fail to run. Repeated calls to build will check successive tuner
     * devices */
    do
    {
        hr = Build();
        if( FAILED( hr ) )
        {
            msg_Warn( p_access, "SubmitDVBCTuneRequest: "\
                "Cannot Build the Graph: hr=0x%8lx", hr );
            return VLC_EGENERIC;
        }
        hr = Start();
    }
    while( hr != S_OK );

    return VLC_SUCCESS;
}

/*****************************************************************************
* Submit a DVB-S Tune Request
******************************************************************************/
int BDAGraph::SubmitDVBSTuneRequest()
{
    HRESULT hr = S_OK;

    class localComPtr
    {
        public:
        IDVBTuneRequest* p_dvbs_tune_request;
        IDVBSLocator* p_dvbs_locator;
        IDVBSTuningSpace* p_dvbs_tuning_space;
        char* psz_polarisation;
        char* psz_input_range;
        BSTR bstr_input_range;
        WCHAR* pwsz_input_range;
        int i_range_len;
        localComPtr(): p_dvbs_tune_request(NULL), p_dvbs_locator(NULL),
            p_dvbs_tuning_space(NULL), bstr_input_range(NULL),
            pwsz_input_range(NULL), i_range_len(0), psz_polarisation(NULL),
            psz_input_range(NULL) {};
        ~localComPtr()
        {
            if( p_dvbs_tuning_space )
                p_dvbs_tuning_space->Release();
            if( p_dvbs_tune_request )
                p_dvbs_tune_request->Release();
            if( p_dvbs_locator )
                p_dvbs_locator->Release();
            SysFreeString( bstr_input_range );
            delete pwsz_input_range;
            free( psz_input_range );
            free( psz_polarisation );
        }
    } l;
    long l_frequency, l_symbolrate, l_azimuth, l_elevation, l_longitude;
    long l_lnb_lof1, l_lnb_lof2, l_lnb_slof, l_inversion, l_network_id;
    long l_hp_fec;
    int  i_mod;
    Polarisation i_polar;
    SpectralInversion i_inversion;
    VARIANT_BOOL b_west;
    BinaryConvolutionCodeRate i_hp_fec;
    ModulationType i_mod_typ;

    l_frequency        = var_GetInteger( p_access, "dvb-frequency" );
    l_symbolrate       = var_GetInteger( p_access, "dvb-srate" );
    l_azimuth          = var_GetInteger( p_access, "dvb-azimuth" );
    l_elevation        = var_GetInteger( p_access, "dvb-elevation" );
    l_longitude        = var_GetInteger( p_access, "dvb-longitude" );
    l_lnb_lof1         = var_GetInteger( p_access, "dvb-lnb-lof1" );
    l_lnb_lof2         = var_GetInteger( p_access, "dvb-lnb-lof2" );
    l_lnb_slof         = var_GetInteger( p_access, "dvb-lnb-slof" );
    i_mod              = var_GetInteger( p_access, "dvb-modulation" );
    l_hp_fec           = var_GetInteger( p_access, "dvb-code-rate-hp" );
    l_inversion        = var_GetInteger( p_access, "dvb-inversion" );
    l_network_id       = var_GetInteger( p_access, "dvb-network-id" );

    l.psz_input_range  = var_GetNonEmptyString( p_access, "dvb-range" );
    l.psz_polarisation = var_GetNonEmptyString( p_access, "dvb-polarisation" );

    b_west = ( l_longitude < 0 );

    i_polar = BDA_POLARISATION_NOT_SET;
    if( l.psz_polarisation != NULL )
    {
        switch( toupper( l.psz_polarisation[0] ) )
        {
        case 'H':
            i_polar = BDA_POLARISATION_LINEAR_H;
            break;
        case 'V':
            i_polar = BDA_POLARISATION_LINEAR_V;
            break;
        case 'L':
            i_polar = BDA_POLARISATION_CIRCULAR_L;
            break;
        case 'R':
            i_polar = BDA_POLARISATION_CIRCULAR_R;
            break;
        }
    }

    switch( l_inversion )
    {
    case 0:
        i_inversion = BDA_SPECTRAL_INVERSION_NORMAL; break;
    case 1:
        i_inversion = BDA_SPECTRAL_INVERSION_INVERTED; break;
    case 2:
        i_inversion = BDA_SPECTRAL_INVERSION_AUTOMATIC; break;
    default:
        i_inversion = BDA_SPECTRAL_INVERSION_NOT_SET;
    }

    switch( i_mod )
    {
    case 16:
        i_mod_typ = BDA_MOD_16QAM; break;
    case 128:
        i_mod_typ = BDA_MOD_128QAM; break;
    case 256:
        i_mod_typ = BDA_MOD_256QAM; break;
    case 10004:
        i_mod_typ = BDA_MOD_QPSK; break;
    default:
        i_mod_typ = BDA_MOD_NOT_SET;
    }

    switch( l_hp_fec )
    {
    case 1:
        i_hp_fec = BDA_BCC_RATE_1_2; break;
    case 2:
        i_hp_fec = BDA_BCC_RATE_2_3; break;
    case 3:
        i_hp_fec = BDA_BCC_RATE_3_4; break;
    case 4:
        i_hp_fec = BDA_BCC_RATE_5_6; break;
    case 5:
        i_hp_fec = BDA_BCC_RATE_7_8; break;
    default:
        i_hp_fec = BDA_BCC_RATE_NOT_SET;
    }

    l.i_range_len = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED,
        l.psz_input_range, -1, l.pwsz_input_range, 0 );
    if( l.i_range_len > 0 )
    {
        l.pwsz_input_range = new WCHAR[l.i_range_len];
        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED,
            l.psz_input_range, -1, l.pwsz_input_range, l.i_range_len );
        l.bstr_input_range=SysAllocString( l.pwsz_input_range );
    }

    guid_network_type = CLSID_DVBSNetworkProvider;
    hr = CreateTuneRequest();
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitDVBSTuneRequest: "\
            "Cannot create Tune Request: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = p_tune_request->QueryInterface( IID_IDVBTuneRequest,
        (void**)&l.p_dvbs_tune_request );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitDVBSTuneRequest: "\
            "Cannot QI for IDVBTuneRequest: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }
    l.p_dvbs_tune_request->put_ONID( -1 );
    l.p_dvbs_tune_request->put_SID( -1 );
    l.p_dvbs_tune_request->put_TSID( -1 );

    hr = ::CoCreateInstance( CLSID_DVBSLocator, 0, CLSCTX_INPROC,
        IID_IDVBSLocator, (void**)&l.p_dvbs_locator );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitDVBSTuneRequest: "\
            "Cannot create the DVBS Locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = p_tuning_space->QueryInterface( IID_IDVBSTuningSpace,
        (void**)&l.p_dvbs_tuning_space );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitDVBSTuneRequest: "\
            "Cannot QI for IDVBSTuningSpace: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = S_OK;
    hr = l.p_dvbs_tuning_space->put_SystemType( DVB_Satellite );
    if( SUCCEEDED( hr ) && l_lnb_lof1 > 0 )
        hr = l.p_dvbs_tuning_space->put_LowOscillator( l_lnb_lof1 );
    if( SUCCEEDED( hr ) && l_lnb_slof > 0 )
        hr = l.p_dvbs_tuning_space->put_LNBSwitch( l_lnb_slof );
    if( SUCCEEDED( hr ) && l_lnb_lof2 > 0 )
        hr = l.p_dvbs_tuning_space->put_HighOscillator( l_lnb_lof2 );
    if( SUCCEEDED( hr ) && i_inversion != BDA_SPECTRAL_INVERSION_NOT_SET )
        hr = l.p_dvbs_tuning_space->put_SpectralInversion( i_inversion );
    if( SUCCEEDED( hr ) && l_network_id > 0 )
        hr = l.p_dvbs_tuning_space->put_NetworkID( l_network_id );
    if( SUCCEEDED( hr ) && l.i_range_len > 0 )
        hr = l.p_dvbs_tuning_space->put_InputRange( l.bstr_input_range );

    if( SUCCEEDED( hr ) && l_frequency > 0 )
        hr = l.p_dvbs_locator->put_CarrierFrequency( l_frequency );
    if( SUCCEEDED( hr ) && l_symbolrate > 0 )
        hr = l.p_dvbs_locator->put_SymbolRate( l_symbolrate );
    if( SUCCEEDED( hr ) && i_polar != BDA_POLARISATION_NOT_SET )
        hr = l.p_dvbs_locator->put_SignalPolarisation( i_polar );
    if( SUCCEEDED( hr ) && i_mod_typ != BDA_MOD_NOT_SET )
        hr = l.p_dvbs_locator->put_Modulation( i_mod_typ );
    if( SUCCEEDED( hr ) && i_hp_fec != BDA_BCC_RATE_NOT_SET )
        hr = l.p_dvbs_locator->put_InnerFECRate( i_hp_fec );

    if( SUCCEEDED( hr ) && l_azimuth > 0 )
        hr = l.p_dvbs_locator->put_Azimuth( l_azimuth );
    if( SUCCEEDED( hr ) && l_elevation > 0 )
        hr = l.p_dvbs_locator->put_Elevation( l_elevation );
    if( SUCCEEDED( hr ) )
        hr = l.p_dvbs_locator->put_WestPosition( b_west );
    if( SUCCEEDED( hr ) )
        hr = l.p_dvbs_locator->put_OrbitalPosition( labs( l_longitude ) );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitDVBSTuneRequest: "\
            "Cannot set tuning parameters on Locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    hr = p_tune_request->put_Locator( l.p_dvbs_locator );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "SubmitDVBSTuneRequest: "\
            "Cannot put the locator: hr=0x%8lx", hr );
        return VLC_EGENERIC;
    }

    /* Build and Run the Graph. If a Tuner device is in use the graph will
     * fail to run. Repeated calls to build will check successive tuner
     * devices */
    do
    {
        hr = Build();
        if( FAILED( hr ) )
        {
            msg_Warn( p_access, "SubmitDVBSTuneRequest: "\
                "Cannot Build the Graph: hr=0x%8lx", hr );
            return VLC_EGENERIC;
        }
        hr = Start();
    }
    while( hr != S_OK );

    return VLC_SUCCESS;
}

/*****************************************************************************
* Load the Tuning Space from System Tuning Spaces according to the
* Network Type requested
******************************************************************************/
HRESULT BDAGraph::CreateTuneRequest()
{
    HRESULT hr = S_OK;
    GUID guid_this_network_type;
    class localComPtr
    {
        public:
        ITuningSpaceContainer*  p_tuning_space_container;
        IEnumTuningSpaces*      p_tuning_space_enum;
        ITuningSpace*           p_this_tuning_space;
        IDVBTuningSpace2*       p_dvb_tuning_space;
        BSTR                    bstr_name;
        char * psz_network_name;
        char * psz_create_name;
        char * psz_bstr_name;
        WCHAR * wpsz_create_name;
        int i_name_len;
        localComPtr(): p_tuning_space_container(NULL),
            p_tuning_space_enum(NULL), p_this_tuning_space(NULL),
            p_dvb_tuning_space(NULL),
            i_name_len(0), psz_network_name(NULL), wpsz_create_name(NULL),
            psz_create_name(NULL), bstr_name(NULL), psz_bstr_name(NULL) {};
        ~localComPtr()
        {
            if( p_tuning_space_enum )
                p_tuning_space_enum->Release();
            if( p_tuning_space_container )
                p_tuning_space_container->Release();
            if( p_this_tuning_space )
                p_this_tuning_space->Release();
            if( p_dvb_tuning_space )
                p_dvb_tuning_space->Release();
            SysFreeString( bstr_name );
            delete[] psz_bstr_name;
            delete[] wpsz_create_name;
            free( psz_network_name );
            free( psz_create_name );
        }
    } l;

    /* We shall test for a specific Tuning space name supplied on the command
     * line as dvb-networkname=xxx.
     * For some users with multiple cards and/or multiple networks this could
     * be useful. This allows us to reasonably safely apply updates to the
     * System Tuning Space in the registry without disrupting other streams. */
    l.psz_network_name = var_GetNonEmptyString( p_access, "dvb-network-name" );
    if( l.psz_network_name )
    {
        msg_Dbg( p_access, "CreateTuneRequest: Find Tuning Space: %s",
            l.psz_network_name );
    }
    else
    {
        l.psz_network_name = new char[1];
        *l.psz_network_name = '\0';
    }

    /* A Tuning Space may already have been set up. If it is for the same
     * network type then all is well. Otherwise, reset the Tuning Space and get
     * a new one */
    if( p_tuning_space )
    {
        hr = p_tuning_space->get__NetworkType( &guid_this_network_type );
        if( FAILED( hr ) ) guid_this_network_type = GUID_NULL;
        if( guid_this_network_type == guid_network_type )
        {
            hr = p_tuning_space->get_UniqueName( &l.bstr_name );
            if( FAILED( hr ) )
            {
                msg_Warn( p_access, "CreateTuneRequest: "\
                    "Cannot get UniqueName for Tuning Space: hr=0x%8lx", hr );
                return hr;
            }
            l.i_name_len = WideCharToMultiByte( CP_ACP, 0, l.bstr_name, -1,
                l.psz_bstr_name, 0, NULL, NULL );
            l.psz_bstr_name = new char[ l.i_name_len ];
            l.i_name_len = WideCharToMultiByte( CP_ACP, 0, l.bstr_name, -1,
                l.psz_bstr_name, l.i_name_len, NULL, NULL );

            /* Test for a specific Tuning space name supplied on the command
             * line as dvb-networkname=xxx */
            if( strlen( l.psz_network_name ) == 0 ||
                strcmp( l.psz_network_name, l.psz_bstr_name ) == 0 )
            {
                msg_Dbg( p_access, "CreateTuneRequest: Using Tuning Space: %s",
                    l.psz_network_name );
                return S_OK;
            }
        }
        /* else different guid_network_type */
        if( p_tuning_space )
            p_tuning_space->Release();
        if( p_tune_request )
            p_tune_request->Release();
        p_tuning_space = NULL;
        p_tune_request = NULL;
    }

    /* Force use of the first available Tuner Device during Build */
    l_tuner_used = -1;

    /* Get the SystemTuningSpaces container to enumerate through all the
     * defined tuning spaces.
     * l.p_tuning_space_container->Refcount = 1  */
    hr = ::CoCreateInstance( CLSID_SystemTuningSpaces, 0, CLSCTX_INPROC,
        IID_ITuningSpaceContainer, (void**)&l.p_tuning_space_container );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "CreateTuneRequest: "\
            "Cannot CoCreate SystemTuningSpaces: hr=0x%8lx", hr );
        return hr;
    }

    /* Get the SystemTuningSpaces container to enumerate through all the
     * defined tuning spaces.
     * l.p_tuning_space_container->Refcount = 2
     * l.p_tuning_space_enum->Refcount = 1  */
    hr = l.p_tuning_space_container->get_EnumTuningSpaces(
         &l.p_tuning_space_enum );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "CreateTuneRequest: "\
            "Cannot create SystemTuningSpaces Enumerator: hr=0x%8lx", hr );
        return hr;
    }

    do
    {
        /* l.p_this_tuning_space->RefCount = 1 after the first pass
         * Release before overwriting with Next */
        if( l.p_this_tuning_space )
            l.p_this_tuning_space->Release();
        l.p_this_tuning_space = NULL;
        SysFreeString( l.bstr_name );

        hr = l.p_tuning_space_enum->Next( 1, &l.p_this_tuning_space, NULL );
        if( hr != S_OK ) break;

        hr = l.p_this_tuning_space->get__NetworkType( &guid_this_network_type );

        /* GUID_NULL means a non-BDA network was found e.g analog
         * Ignore failures and non-BDA networks and keep looking */
        if( FAILED( hr ) ) guid_this_network_type == GUID_NULL;

        if( guid_this_network_type == guid_network_type )
        {
            /* QueryInterface to clone l.p_this_tuning_space
             * l.p_this_tuning_space->RefCount = 2 */
            hr = l.p_this_tuning_space->Clone( &p_tuning_space );
            if( FAILED( hr ) )
            {
                msg_Warn( p_access, "CreateTuneRequest: "\
                    "Cannot QI Tuning Space: hr=0x%8lx", hr );
                return hr;
            }
            hr = p_tuning_space->get_UniqueName( &l.bstr_name );
            if( FAILED( hr ) )
            {
                msg_Warn( p_access, "CreateTuneRequest: "\
                    "Cannot get UniqueName for Tuning Space: hr=0x%8lx", hr );
                return hr;
            }

            /* Test for a specific Tuning space name supplied on the command
             * line as dvb-networkname=xxx */
            delete[] l.psz_bstr_name;
            l.i_name_len = WideCharToMultiByte( CP_ACP, 0, l.bstr_name, -1,
                l.psz_bstr_name, 0, NULL, NULL );
            l.psz_bstr_name = new char[ l.i_name_len ];
            l.i_name_len = WideCharToMultiByte( CP_ACP, 0, l.bstr_name, -1,
                l.psz_bstr_name, l.i_name_len, NULL, NULL );
            if( strlen( l.psz_network_name ) == 0 ||
                strcmp( l.psz_network_name, l.psz_bstr_name ) == 0 )
            {
                msg_Dbg( p_access, "CreateTuneRequest: Using Tuning Space: %s",
                    l.psz_bstr_name );

            /* CreateTuneRequest adds TuneRequest to p_tuning_space
             * p_tune_request->RefCount = 1 */
                hr = p_tuning_space->CreateTuneRequest( &p_tune_request );
                if( FAILED( hr ) )
                    msg_Warn( p_access, "CreateTuneRequest: "\
                        "Cannot Create Tune Request: hr=0x%8lx", hr );
                return hr;
            }
            if( p_tuning_space )
                p_tuning_space->Release();
            p_tuning_space = NULL;
        }
    }
    while( true );

    /* No tuning space was found. If the create-name parameter was set then
     * create a tuning space. By rights should use the same name used in
     * network-name
     * Also would be nice to copy a tuning space but we only come here if we do
     * not find any. */
    l.psz_create_name = var_GetNonEmptyString( p_access, "dvb-create-name" );
    if( !l.psz_create_name || strlen( l.psz_create_name ) <= 0 )
    {
        hr = E_FAIL;
        msg_Warn( p_access, "CreateTuneRequest: "\
            "Cannot find a suitable System Tuning Space: hr=0x%8lx", hr );
        return hr;
    }
    if( strcmp( l.psz_create_name, l.psz_network_name ) )
    {
        hr = E_FAIL;
        msg_Warn( p_access, "CreateTuneRequest: "\
            "dvb-create-name %s must match dvb-network-name %s",
            l.psz_create_name, l.psz_network_name );
        return hr;
    }

    /* Need to use DVBSTuningSpace for DVB-S and ATSCTuningSpace for ATSC */
    VARIANT var_id;
    CLSID cls_tuning_space;

    if( IsEqualCLSID( guid_network_type, CLSID_ATSCNetworkProvider ) )
        cls_tuning_space = CLSID_ATSCTuningSpace;
    if( IsEqualCLSID( guid_network_type, CLSID_DVBTNetworkProvider ) )
        cls_tuning_space = CLSID_DVBTuningSpace;
    if( IsEqualCLSID( guid_network_type, CLSID_DVBCNetworkProvider ) )
        cls_tuning_space = CLSID_DVBTuningSpace;
    if( IsEqualCLSID( guid_network_type, CLSID_DVBSNetworkProvider ) )
        cls_tuning_space = CLSID_DVBSTuningSpace;

    l.i_name_len = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED,
        l.psz_create_name, -1, l.wpsz_create_name, 0 );
    if( l.i_name_len <= 0 )
    {
        hr = E_FAIL;
        msg_Warn( p_access, "CreateTuneRequest: "\
            "Cannot convert zero length dvb-create-name %s",
            l.psz_create_name );
        return hr;
    }
    l.wpsz_create_name = new WCHAR[l.i_name_len];
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, l.psz_create_name, -1,
            l.wpsz_create_name, l.i_name_len );
    if( l.bstr_name )
        SysFreeString( l.bstr_name );
    l.bstr_name = SysAllocString( l.wpsz_create_name );

    msg_Dbg( p_access, "CreateTuneRequest: Create Tuning Space: %s",
        l.psz_create_name );

    hr = ::CoCreateInstance( cls_tuning_space, 0, CLSCTX_INPROC,
         IID_ITuningSpace, (void**)&p_tuning_space );

    if( FAILED( hr ) )
        msg_Warn( p_access, "CreateTuneRequest: "\
            "Cannot CoCreate new TuningSpace: hr=0x%8lx", hr );
    if( SUCCEEDED( hr ) )
        hr = p_tuning_space->put__NetworkType( guid_network_type );
    if( FAILED( hr ) )
        msg_Warn( p_access, "CreateTuneRequest: "\
            "Cannot Put Network Type: hr=0x%8lx", hr );
    if( SUCCEEDED( hr ) )
        hr = p_tuning_space->put_UniqueName( l.bstr_name );
    if( FAILED( hr ) )
        msg_Warn( p_access, "CreateTuneRequest: "\
            "Cannot Put Unique Name: hr=0x%8lx", hr );
    if( SUCCEEDED( hr ) )
        hr = p_tuning_space->put_FriendlyName( l.bstr_name );
    if( FAILED( hr ) )
        msg_Warn( p_access, "CreateTuneRequest: "\
            "Cannot Put Friendly Name: hr=0x%8lx", hr );
    if( guid_network_type == CLSID_DVBTNetworkProvider ||
        guid_network_type == CLSID_DVBCNetworkProvider ||
        guid_network_type == CLSID_DVBSNetworkProvider )
    {
        hr = p_tuning_space->QueryInterface( IID_IDVBTuningSpace2,
            (void**)&l.p_dvb_tuning_space );
        if( FAILED( hr ) )
        {
            msg_Warn( p_access, "CreateTuneRequest: "\
                "Cannot QI for IDVBTuningSpace2: hr=0x%8lx", hr );
            return hr;
        }
        if( guid_network_type == CLSID_DVBTNetworkProvider )
            hr = l.p_dvb_tuning_space->put_SystemType( DVB_Terrestrial );
        if( guid_network_type == CLSID_DVBCNetworkProvider )
            hr = l.p_dvb_tuning_space->put_SystemType( DVB_Cable );
        if( guid_network_type == CLSID_DVBSNetworkProvider )
            hr = l.p_dvb_tuning_space->put_SystemType( DVB_Satellite );
    }

    if( SUCCEEDED( hr ) )
        hr = l.p_tuning_space_container->Add( p_tuning_space, &var_id );

    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "CreateTuneRequest: "\
            "Cannot Create new TuningSpace: hr=0x%8lx", hr );
        return hr;
    }

    msg_Dbg( p_access, "CreateTuneRequest: Tuning Space: %s created",
         l.psz_create_name );

    hr = p_tuning_space->CreateTuneRequest( &p_tune_request );
    if( FAILED( hr ) )
        msg_Warn( p_access, "CreateTuneRequest: "\
            "Cannot Create Tune Request: hr=0x%8lx", hr );

    return hr;
}

/******************************************************************************
* Build
* Step 4: Build the Filter Graph
* Build sets up devices, adds and connects filters
******************************************************************************/
HRESULT BDAGraph::Build()
{
    HRESULT hr = S_OK;
    long l_capture_used, l_tif_used;
    VARIANT l_tuning_space_id;
    AM_MEDIA_TYPE grabber_media_type;
    class localComPtr
    {
        public:
        ITuningSpaceContainer*  p_tuning_space_container;
        localComPtr(): p_tuning_space_container(NULL) {};
        ~localComPtr()
        {
            if( p_tuning_space_container )
                p_tuning_space_container->Release();
        }
    } l;

    /* Get the SystemTuningSpaces container to save the Tuning space */
    l_tuning_space_id.vt = VT_I4;
    l_tuning_space_id.lVal = 0L;
    hr = ::CoCreateInstance( CLSID_SystemTuningSpaces, 0, CLSCTX_INPROC,
        IID_ITuningSpaceContainer, (void**)&l.p_tuning_space_container );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot CoCreate SystemTuningSpaces: hr=0x%8lx", hr );
        return hr;
    }
    hr = l.p_tuning_space_container->FindID( p_tuning_space,
        &l_tuning_space_id.lVal );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot Find Tuning Space ID: hr=0x%8lx", hr );
        return hr;
    }
    msg_Dbg( p_access, "Build: Using Tuning Space ID %d",
        l_tuning_space_id.lVal );
    hr = l.p_tuning_space_container->put_Item( l_tuning_space_id,
        p_tuning_space );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot save Tuning Space: hr=0x%8lx (ignored)", hr );
    }

    /* If we have already have a filter graph, rebuild it*/
    Destroy();

    hr = ::CoCreateInstance( CLSID_FilterGraph, NULL, CLSCTX_INPROC,
        IID_IGraphBuilder, (void**)&p_filter_graph );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot CoCreate IFilterGraph: hr=0x%8lx", hr );
        return hr;
    }

    /* First filter in the graph is the Network Provider and
     * its Scanning Tuner which takes the Tune Request*/
    hr = ::CoCreateInstance( guid_network_type, NULL, CLSCTX_INPROC_SERVER,
        IID_IBaseFilter, (void**)&p_network_provider);
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot CoCreate Network Provider: hr=0x%8lx", hr );
        return hr;
    }
    hr = p_filter_graph->AddFilter( p_network_provider, L"Network Provider" );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot load network provider: hr=0x%8lx", hr );
        return hr;
    }

    hr = p_network_provider->QueryInterface( IID_IScanningTuner,
        (void**)&p_scanning_tuner );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot QI Network Provider for Scanning Tuner: hr=0x%8lx", hr );
        return hr;
    }

    hr = p_scanning_tuner->Validate( p_tune_request );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Tune Request is invalid: hr=0x%8lx", hr );
        return hr;
    }
    hr = p_scanning_tuner->put_TuneRequest( p_tune_request );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot submit the tune request: hr=0x%8lx", hr );
        return hr;
    }

    /* Add the Network Tuner to the Network Provider. On subsequent calls,
     * l_tuner_used will cause a different tuner to be selected
     * To select a specific device first get the parameter that nominates the
     * device (dvb-adapter) and use the value to initialise l_tuner_used.
     * When FindFilter returns check the contents of l_tuner_used.
     * If it is not what was selected then the requested device was not
     * available so return with an error. */

    long l_adapter = -1;
    l_adapter = var_GetInteger( p_access, "dvb-adapter" );
    if( l_tuner_used < 0 && l_adapter >= 0 )
        l_tuner_used = l_adapter - 1;

    hr = FindFilter( KSCATEGORY_BDA_NETWORK_TUNER, &l_tuner_used,
        p_network_provider, &p_tuner_device );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot load tuner device and connect network provider: "\
            "hr=0x%8lx", hr );
        return hr;
    }
    if( l_adapter > 0 && l_tuner_used != l_adapter )
    {
         msg_Warn( p_access, "Build: "\
             "Tuner device %d is not available", l_adapter );
        return E_FAIL;
    }
    msg_Dbg( p_access, "BDAGraph: Using adapter %d", l_tuner_used );

/* VLC 1.0 works reliably up this point then crashes
 * Obvious candidate is FindFilter */
    /* Always look for all capture devices to match the Network Tuner */
    l_capture_used = -1;
    hr = FindFilter( KSCATEGORY_BDA_RECEIVER_COMPONENT, &l_capture_used,
        p_tuner_device, &p_capture_device );
    if( FAILED( hr ) )
    {
        /* Some BDA drivers do not provide a Capture Device Filter so force
         * the Sample Grabber to connect directly to the Tuner Device */
        p_capture_device = p_tuner_device;
        p_tuner_device = NULL;
        msg_Warn( p_access, "Build: "\
            "Cannot find Capture device. Connecting to tuner: hr=0x%8lx", hr );
    }
    if( p_sample_grabber )
         p_sample_grabber->Release();
    p_sample_grabber = NULL;
    /* Insert the Sample Grabber to tap into the Transport Stream. */
    hr = ::CoCreateInstance( CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER,
        IID_IBaseFilter, (void**)&p_sample_grabber );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot load Sample Grabber Filter: hr=0x%8lx", hr );
        return hr;
    }
    hr = p_filter_graph->AddFilter( p_sample_grabber, L"Sample Grabber" );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot add Sample Grabber Filter to graph: hr=0x%8lx", hr );
        return hr;
    }

    if( p_grabber )
        p_grabber->Release();
    p_grabber = NULL;
    hr = p_sample_grabber->QueryInterface( IID_ISampleGrabber,
        (void**)&p_grabber );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot QI Sample Grabber Filter: hr=0x%8lx", hr );
        return hr;
    }

    /* Try the possible stream type */
    hr = E_FAIL;
    for( int i = 0; i < 2; i++ )
    {
        ZeroMemory( &grabber_media_type, sizeof( AM_MEDIA_TYPE ) );
        grabber_media_type.majortype = MEDIATYPE_Stream;
        grabber_media_type.subtype   =  i == 0 ? MEDIASUBTYPE_MPEG2_TRANSPORT : KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT;
        msg_Dbg( p_access, "Build: "
                           "Trying connecting with subtype %s",
                           i == 0 ? "MEDIASUBTYPE_MPEG2_TRANSPORT" : "KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT" );
        hr = p_grabber->SetMediaType( &grabber_media_type );
        if( SUCCEEDED( hr ) )
        {
            hr = Connect( p_capture_device, p_sample_grabber );
            if( SUCCEEDED( hr ) )
                break;
            msg_Warn( p_access, "Build: "\
                "Cannot connect Sample Grabber to Capture device: hr=0x%8lx (try %d/2)", hr, 1+i );
        }
        else
        {
            msg_Warn( p_access, "Build: "\
                "Cannot set media type on grabber filter: hr=0x%8lx (try %d/2", hr, 1+i );
        }
    }
    if( hr )
        return hr;

    /* We need the MPEG2 Demultiplexer even though we are going to use the VLC
     * TS demuxer. The TIF filter connects to the MPEG2 demux and works with
     * the Network Provider filter to set up the stream */
    hr = ::CoCreateInstance( CLSID_MPEG2Demultiplexer, NULL,
        CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&p_mpeg_demux );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot CoCreateInstance MPEG2 Demultiplexer: hr=0x%8lx", hr );
        return hr;
    }
    hr = p_filter_graph->AddFilter( p_mpeg_demux, L"Demux" );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot add demux filter to graph: hr=0x%8lx", hr );
        return hr;
    }
    hr = Connect( p_sample_grabber, p_mpeg_demux );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot connect demux to grabber: hr=0x%8lx", hr );
        return hr;
    }

    /* Always look for the Transform Information Filter from the start
     * of the collection*/
    l_tif_used = -1;
    hr = FindFilter( KSCATEGORY_BDA_TRANSPORT_INFORMATION, &l_tif_used,
        p_mpeg_demux, &p_transport_info );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot load TIF onto demux: hr=0x%8lx", hr );
        return hr;
    }
    /* Configure the Sample Grabber to buffer the samples continuously */
    hr = p_grabber->SetBufferSamples( true );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot set Sample Grabber to buffering: hr=0x%8lx", hr );
        return hr;
    }
    hr = p_grabber->SetOneShot( false );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot set Sample Grabber to multi shot: hr=0x%8lx", hr );
        return hr;
    }
    hr = p_grabber->SetCallback( this, 0 );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot set SampleGrabber Callback: hr=0x%8lx", hr );
        return hr;
    }

    hr = Register();
    if( FAILED( hr ) )
    {
        d_graph_register = 0;
    }

    /* The Media Control is used to Run and Stop the Graph */
    if( p_media_control )
        p_media_control->Release();
    p_media_control = NULL;
    hr = p_filter_graph->QueryInterface( IID_IMediaControl,
        (void**)&p_media_control );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Build: "\
            "Cannot QI Media Control: hr=0x%8lx", hr );
        return hr;
    }

    return hr;

}

/******************************************************************************
* FindFilter
* Looks up all filters in a category and connects to the upstream filter until
* a successful match is found. The index of the connected filter is returned.
* On subsequent calls, this can be used to start from that point to find
* another match.
* This is used when the graph does not run because a tuner device is in use so
* another one needs to be selected.
******************************************************************************/
HRESULT BDAGraph::FindFilter( REFCLSID clsid, long* i_moniker_used,
    IBaseFilter* p_upstream, IBaseFilter** p_p_downstream )
{
    HRESULT                 hr = S_OK;
    int                     i_moniker_index = -1;
    class localComPtr
    {
        public:
        IMoniker*      p_moniker;
        IEnumMoniker*  p_moniker_enum;
        IBaseFilter*   p_filter;
        IPropertyBag*  p_property_bag;
        VARIANT        var_bstr;
        char *         psz_bstr;
        int            i_bstr_len;
        localComPtr():
            p_moniker(NULL),
            p_moniker_enum(NULL),
            p_filter(NULL),
            p_property_bag(NULL),
            psz_bstr( NULL )
            { ::VariantInit(&var_bstr); };
        ~localComPtr()
        {
            if( p_property_bag )
                p_property_bag->Release();
            if( p_filter )
                p_filter->Release();
            if( p_moniker )
                p_moniker->Release();
            if( p_moniker_enum )
                p_moniker_enum->Release();
            ::VariantClear(&var_bstr);
            delete[] psz_bstr;
        }
    } l;

    if( !p_system_dev_enum )
    {
        hr = ::CoCreateInstance( CLSID_SystemDeviceEnum, 0, CLSCTX_INPROC,
            IID_ICreateDevEnum, (void**)&p_system_dev_enum );
        if( FAILED( hr ) )
        {
            msg_Warn( p_access, "FindFilter: "\
                "Cannot CoCreate SystemDeviceEnum: hr=0x%8lx", hr );
            return hr;
        }
    }

    hr = p_system_dev_enum->CreateClassEnumerator( clsid,
        &l.p_moniker_enum, 0 );
    if( hr != S_OK )
    {
        msg_Warn( p_access, "FindFilter: "\
            "Cannot CreateClassEnumerator: hr=0x%8lx", hr );
        return E_FAIL;
    }

    do
    {
        /* We are overwriting l.p_moniker so we should Release and nullify
         * It is important that p_moniker and p_property_bag are fully released
         * l.p_filter may not be dereferenced so we could force to NULL */
        if( l.p_property_bag )
            l.p_property_bag->Release();
        l.p_property_bag = NULL;
        if( l.p_filter )
            l.p_filter->Release();
        l.p_filter = NULL;
        if( l.p_moniker )
            l.p_moniker->Release();
         l.p_moniker = NULL;

        hr = l.p_moniker_enum->Next( 1, &l.p_moniker, 0 );
        if( hr != S_OK ) break;
        i_moniker_index++;

        /* Skip over devices already found on previous calls */
        if( i_moniker_index <= *i_moniker_used ) continue;
        *i_moniker_used = i_moniker_index;

        /* l.p_filter is Released at the top of the loop */
        hr = l.p_moniker->BindToObject( NULL, NULL, IID_IBaseFilter,
            (void**)&l.p_filter );
        if( FAILED( hr ) )
        {
            continue;
        }
        /* l.p_property_bag is released at the top of the loop */
        hr = l.p_moniker->BindToStorage( NULL, NULL, IID_IPropertyBag,
            (void**)&l.p_property_bag );
        if( FAILED( hr ) )
        {
            msg_Warn( p_access, "FindFilter: "\
                "Cannot Bind to Property Bag: hr=0x%8lx", hr );
            return hr;
        }
        hr = l.p_property_bag->Read( L"FriendlyName", &l.var_bstr, NULL );
        if( FAILED( hr ) )
        {
            msg_Warn( p_access, "FindFilter: "\
                "Cannot read filter friendly name: hr=0x%8lx", hr );
            return hr;
        }

        hr = p_filter_graph->AddFilter( l.p_filter, l.var_bstr.bstrVal );
        if( FAILED( hr ) )
        {
            msg_Warn( p_access, "FindFilter: "\
                "Cannot add filter: hr=0x%8lx", hr );
            return hr;
        }
        hr = Connect( p_upstream, l.p_filter );
        if( SUCCEEDED( hr ) )
        {
            /* p_p_downstream has not been touched yet so no release needed */
            delete[] l.psz_bstr;
            l.i_bstr_len = WideCharToMultiByte( CP_ACP, 0,
                l.var_bstr.bstrVal, -1, l.psz_bstr, 0, NULL, NULL );
            l.psz_bstr = new char[l.i_bstr_len];
            l.i_bstr_len = WideCharToMultiByte( CP_ACP, 0,
                l.var_bstr.bstrVal, -1, l.psz_bstr, l.i_bstr_len, NULL, NULL );
            msg_Dbg( p_access, "FindFilter: Connected %s", l.psz_bstr );
            l.p_filter->QueryInterface( IID_IBaseFilter,
                (void**)p_p_downstream );
            return S_OK;
        }
        /* Not the filter we want so unload and try the next one */
        hr = p_filter_graph->RemoveFilter( l.p_filter );
        if( FAILED( hr ) )
        {
            msg_Warn( p_access, "FindFilter: "\
                "Failed unloading Filter: hr=0x%8lx", hr );
            return hr;
        }

    }
    while( true );

    hr = E_FAIL;
    msg_Warn( p_access, "FindFilter: No filter connected: hr=0x%8lx", hr );
    return hr;
}

/*****************************************************************************
* Connect is called from Build to enumerate and connect pins
*****************************************************************************/
HRESULT BDAGraph::Connect( IBaseFilter* p_upstream, IBaseFilter* p_downstream )
{
    HRESULT             hr = E_FAIL;
    class localComPtr
    {
        public:
        IPin*      p_pin_upstream;
        IPin*      p_pin_downstream;
        IEnumPins* p_pin_upstream_enum;
        IEnumPins* p_pin_downstream_enum;
        IPin*      p_pin_temp;
        localComPtr(): p_pin_upstream(NULL), p_pin_downstream(NULL),
            p_pin_upstream_enum(NULL), p_pin_downstream_enum(NULL),
            p_pin_temp(NULL) { };
        ~localComPtr()
        {
            if( p_pin_temp )
                p_pin_temp->Release();
            if( p_pin_downstream )
                p_pin_downstream->Release();
            if( p_pin_upstream )
                p_pin_upstream->Release();
            if( p_pin_downstream_enum )
                p_pin_downstream_enum->Release();
            if( p_pin_upstream_enum )
                p_pin_upstream_enum->Release();
        }
    } l;

    PIN_DIRECTION pin_dir;

    hr = p_upstream->EnumPins( &l.p_pin_upstream_enum );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Connect: "\
            "Cannot get upstream filter enumerator: hr=0x%8lx", hr );
        return hr;
    }

    do
    {
        /* Release l.p_pin_upstream before next iteration */
        if( l.p_pin_upstream  )
            l.p_pin_upstream ->Release();
        l.p_pin_upstream = NULL;
        hr = l.p_pin_upstream_enum->Next( 1, &l.p_pin_upstream, 0 );
        if( hr != S_OK ) break;

        hr = l.p_pin_upstream->QueryDirection( &pin_dir );
        if( FAILED( hr ) )
        {
            msg_Warn( p_access, "Connect: "\
                "Cannot get upstream filter pin direction: hr=0x%8lx", hr );
            return hr;
        }
        hr = l.p_pin_upstream->ConnectedTo( &l.p_pin_downstream );
        if( SUCCEEDED( hr ) )
        {
            l.p_pin_downstream->Release();
            l.p_pin_downstream = NULL;
        }
        if( FAILED( hr ) && hr != VFW_E_NOT_CONNECTED )
        {
            msg_Warn( p_access, "Connect: "\
                "Cannot check upstream filter connection: hr=0x%8lx", hr );
            return hr;
        }
        if( ( pin_dir == PINDIR_OUTPUT ) && ( hr == VFW_E_NOT_CONNECTED ) )
        {
            /* The upstream pin is not yet connected so check each pin on the
             * downstream filter */
            hr = p_downstream->EnumPins( &l.p_pin_downstream_enum );
            if( FAILED( hr ) )
            {
                msg_Warn( p_access, "Connect: Cannot get "\
                    "downstream filter enumerator: hr=0x%8lx", hr );
                return hr;
            }
            do
            {
                /* Release l.p_pin_downstream before next iteration */
                if( l.p_pin_downstream  )
                    l.p_pin_downstream ->Release();
                l.p_pin_downstream = NULL;

                hr = l.p_pin_downstream_enum->Next( 1, &l.p_pin_downstream, 0 );
                if( hr != S_OK ) break;

                hr = l.p_pin_downstream->QueryDirection( &pin_dir );
                if( FAILED( hr ) )
                {
                    msg_Warn( p_access, "Connect: Cannot get "\
                        "downstream filter pin direction: hr=0x%8lx", hr );
                    return hr;
                }

                /* Looking for a free Pin to connect to
                 * A connected Pin may have an reference count > 1
                 * so Release and nullify the pointer */
                hr = l.p_pin_downstream->ConnectedTo( &l.p_pin_temp );
                if( SUCCEEDED( hr ) )
                {
                    l.p_pin_temp->Release();
                    l.p_pin_temp = NULL;
                }
                if( hr != VFW_E_NOT_CONNECTED )
                {
                    if( FAILED( hr ) )
                    {
                        msg_Warn( p_access, "Connect: Cannot check "\
                            "downstream filter connection: hr=0x%8lx", hr );
                        return hr;
                    }
                }
                if( ( pin_dir == PINDIR_INPUT ) &&
                    ( hr == VFW_E_NOT_CONNECTED ) )
                {
                    hr = p_filter_graph->ConnectDirect( l.p_pin_upstream,
                        l.p_pin_downstream, NULL );
                    if( SUCCEEDED( hr ) )
                    {
                        /* If we arrive here then we have a matching pair of
                         * pins. */
                        return S_OK;
                    }
                }
                /* If we arrive here it means this downstream pin is not
                 * suitable so try the next downstream pin.
                 * l.p_pin_downstream is released at the top of the loop */
            }
            while( true );
            /* If we arrive here then we ran out of pins before we found a
             * suitable one. Release outstanding refcounts */
            if( l.p_pin_downstream_enum )
                l.p_pin_downstream_enum->Release();
            l.p_pin_downstream_enum = NULL;
            if( l.p_pin_downstream )
                l.p_pin_downstream->Release();
            l.p_pin_downstream = NULL;
        }
        /* If we arrive here it means this upstream pin is not suitable
         * so try the next upstream pin
         * l.p_pin_upstream is released at the top of the loop */
    }
    while( true );
    /* If we arrive here it means we did not find any pair of suitable pins
     * Outstanding refcounts are released in the destructor */
    return E_FAIL;
}

/*****************************************************************************
* Start uses MediaControl to start the graph
*****************************************************************************/
HRESULT BDAGraph::Start()
{
    HRESULT hr = S_OK;
    OAFilterState i_state; /* State_Stopped, State_Paused, State_Running */

    if( !p_media_control )
    {
        msg_Dbg( p_access, "Start: Media Control has not been created" );
        return E_FAIL;
    }
    hr = p_media_control->Run();
    msg_Dbg( p_access, "Graph started hr=0x%lx", hr );
    if( hr == S_OK )
        return hr;

    /* Query the state of the graph - timeout after 100 milliseconds */
    while( hr = p_media_control->GetState( 100, &i_state ) != S_OK )
    {
        if( FAILED( hr ) )
        {
            msg_Warn( p_access,
                "Start: Cannot get Graph state: hr=0x%8lx", hr );
            return hr;
        }
    }
    if( i_state == State_Running )
        return hr;

    /* The Graph is not running so stop it and return an error */
    msg_Warn( p_access, "Start: Graph not started: %d", i_state );
    hr = p_media_control->StopWhenReady(); /* Instead of Stop() */
    if( FAILED( hr ) )
    {
        msg_Warn( p_access,
            "Start: Cannot stop Graph after Run failed: hr=0x%8lx", hr );
        return hr;
    }
    return E_FAIL;
}

/*****************************************************************************
* Pop the stream of data
*****************************************************************************/
block_t *BDAGraph::Pop()
{
    return output.Pop();
}

/******************************************************************************
* SampleCB - Callback when the Sample Grabber has a sample
******************************************************************************/
STDMETHODIMP BDAGraph::SampleCB( double d_time, IMediaSample *p_sample )
{
    if( p_sample->IsDiscontinuity() == S_OK )
        msg_Warn( p_access, "BDA SampleCB: Sample Discontinuity.");

    const size_t i_sample_size = p_sample->GetActualDataLength();
    BYTE *p_sample_data;
    p_sample->GetPointer( &p_sample_data );

    if( i_sample_size > 0 && p_sample_data )
    {
        block_t *p_block = block_New( p_access, i_sample_size );

        if( p_block )
        {
            memcpy( p_block->p_buffer, p_sample_data, i_sample_size );
            output.Push( p_block );
        }
     }
     return S_OK;
}

STDMETHODIMP BDAGraph::BufferCB( double d_time, BYTE* p_buffer,
    long l_buffer_len )
{
    return E_FAIL;
}

/******************************************************************************
* removes each filter from the graph
******************************************************************************/
HRESULT BDAGraph::Destroy()
{
    HRESULT hr = S_OK;
    ULONG ul_refcount = 0;

    if( p_media_control )
        hr = p_media_control->StopWhenReady(); /* Instead of Stop() */

    if( d_graph_register )
    {
        Deregister();
    }

    output.Empty();

    if( p_grabber )
    {
        p_grabber->Release();
        p_grabber = NULL;
    }

    if( p_transport_info )
    {
        p_filter_graph->RemoveFilter( p_transport_info );
        p_transport_info->Release();
        p_transport_info = NULL;
    }
    if( p_mpeg_demux )
    {
        p_filter_graph->RemoveFilter( p_mpeg_demux );
        p_mpeg_demux->Release();
        p_mpeg_demux = NULL;
    }
    if( p_sample_grabber )
    {
        p_filter_graph->RemoveFilter( p_sample_grabber );
        p_sample_grabber->Release();
        p_sample_grabber = NULL;
    }
    if( p_capture_device )
    {
        p_filter_graph->RemoveFilter( p_capture_device );
        p_capture_device->Release();
        p_capture_device = NULL;
    }
    if( p_tuner_device )
    {
        p_filter_graph->RemoveFilter( p_tuner_device );
        p_tuner_device->Release();
        p_tuner_device = NULL;
    }
    if( p_scanning_tuner )
    {
        p_scanning_tuner->Release();
        p_scanning_tuner = NULL;
    }
    if( p_network_provider )
    {
        p_filter_graph->RemoveFilter( p_network_provider );
        p_network_provider->Release();
        p_network_provider = NULL;
    }

    if( p_media_control )
    {
        p_media_control->Release();
        p_media_control = NULL;
    }
    if( p_filter_graph )
    {
        p_filter_graph->Release();
        p_filter_graph = NULL;
    }
    if( p_system_dev_enum )
    {
        p_system_dev_enum->Release();
        p_system_dev_enum = NULL;
    }

    return S_OK;
}

/*****************************************************************************
* Add/Remove a DirectShow filter graph to/from the Running Object Table.
* Allows GraphEdit to "spy" on a remote filter graph.
******************************************************************************/
HRESULT BDAGraph::Register()
{
    class localComPtr
    {
        public:
        IMoniker*             p_moniker;
        IRunningObjectTable*  p_ro_table;
        localComPtr(): p_moniker(NULL), p_ro_table(NULL) {};
        ~localComPtr()
        {
            if( p_moniker )
                p_moniker->Release();
            if( p_ro_table )
                p_ro_table->Release();
        }
    } l;
    WCHAR     psz_w_graph_name[128];
    HRESULT   hr;

    hr = ::GetRunningObjectTable( 0, &l.p_ro_table );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Register: Cannot get ROT: hr=0x%8lx", hr );
        return hr;
    }

    wsprintfW( psz_w_graph_name, L"VLC BDA Graph %08x Pid %08x",
        (DWORD_PTR) p_filter_graph, ::GetCurrentProcessId() );
    hr = CreateItemMoniker( L"!", psz_w_graph_name, &l.p_moniker );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Register: Cannot Create Moniker: hr=0x%8lx", hr );
        return hr;
    }
    hr = l.p_ro_table->Register( ROTFLAGS_REGISTRATIONKEEPSALIVE,
        p_filter_graph, l.p_moniker, &d_graph_register );
    if( FAILED( hr ) )
    {
        msg_Warn( p_access, "Register: Cannot Register Graph: hr=0x%8lx", hr );
        return hr;
    }
//    msg_Dbg( p_access, "Register: registered Graph: %S", psz_w_graph_name );
    return hr;
}

void BDAGraph::Deregister()
{
    HRESULT   hr;
    IRunningObjectTable* p_ro_table;
    hr = ::GetRunningObjectTable( 0, &p_ro_table );
    if( SUCCEEDED( hr ) )
        p_ro_table->Revoke( d_graph_register );
    d_graph_register = 0;
    p_ro_table->Release();
}
